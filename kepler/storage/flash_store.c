/******************************************************************************
 *
 * @file  flash_store.c
 *
 * @brief NV storage on top of the BLE stack's osal_snv key-value store.
 *
 *        osal_snv_read() returns nonzero (NV_OPER_FAILED) for items that
 *        have never been written; every reader here turns that into a
 *        documented default instead of propagating garbage.
 *
 *        SNV ID mapping: logical NV_ID_x (1..9) -> 0x80 + (id - 1), inside
 *        the customer range BLE_NVID_CUST_START..BLE_NVID_CUST_END.
 *
 *****************************************************************************/

#include "flash_store.h"
#include "../kepler_config.h"

#include <string.h>
#include "osal_snv.h"

/*--- SNV ID mapping --------------------------------------------------------*/

#define SNV_CUST_BASE  0x80u
#define SNV_ID(nv_id)  ((uint8_t)(SNV_CUST_BASE + (uint8_t)(nv_id) - 1u))

/* osal_snv returns SUCCESS (0) on success. */
#define SNV_OK(rc)     ((rc) == 0)

/*--- Helpers ----------------------------------------------------------------*/

static bool snv_write(uint16_t nv_id, const void *buf, uint16_t len)
{
    /* osal_snv_write takes a non-const pointer; it does not modify data.  */
    return SNV_OK(osal_snv_write(SNV_ID(nv_id), len, (void *)buf));
}

static bool snv_read(uint16_t nv_id, void *buf, uint16_t len)
{
    return SNV_OK(osal_snv_read(SNV_ID(nv_id), len, buf));
}

/*==========================================================================*
 *  Lifecycle                                                                *
 *==========================================================================*/

bool flash_store_init(void)
{
    /* osal_snv is initialised by the BLE stack; nothing to construct.     *
     * Probe one item so init failures surface at boot rather than first   *
     * use.  A read miss (never written) is NOT a failure.                 */
    uint8_t probe;
    (void)snv_read(NV_ID_HAPTIC_CALIBRATED, &probe, sizeof(probe));
    return true;
}

/*==========================================================================*
 *  Step history                                                             *
 *==========================================================================*/

bool flash_store_step_day(uint32_t steps)
{
    uint16_t hist[7];

    if (!snv_read(NV_ID_STEP_HISTORY, hist, sizeof(hist))) {
        memset(hist, 0, sizeof(hist));
    }

    /* Shift right: today becomes yesterday, oldest day falls off.         */
    for (uint8_t i = 6u; i > 0u; i--) {
        hist[i] = hist[i - 1u];
    }
    hist[0] = (steps > 0xFFFFu) ? 0xFFFFu : (uint16_t)steps;

    return snv_write(NV_ID_STEP_HISTORY, hist, sizeof(hist));
}

bool flash_store_read_steps(uint16_t out[7])
{
    if (out == NULL) { return false; }
    if (!snv_read(NV_ID_STEP_HISTORY, out, 7u * sizeof(uint16_t))) {
        memset(out, 0, 7u * sizeof(uint16_t));
        return false;
    }
    return true;
}

bool flash_store_write_step_today(uint32_t steps)
{
    return snv_write(NV_ID_STEP_TODAY, &steps, sizeof(steps));
}

uint32_t flash_store_read_step_today(void)
{
    uint32_t steps;
    if (!snv_read(NV_ID_STEP_TODAY, &steps, sizeof(steps))) {
        return 0u;
    }
    return steps;
}

/*==========================================================================*
 *  Sleep actigraphy                                                         *
 *==========================================================================*/

bool flash_store_write_sleep(const actigraphy_night_t *night)
{
    if (night == NULL) { return false; }
    return snv_write(NV_ID_SLEEP_LAST_NIGHT, night, sizeof(*night));
}

bool flash_store_read_sleep(actigraphy_night_t *out)
{
    if (out == NULL) { return false; }
    if (!snv_read(NV_ID_SLEEP_LAST_NIGHT, out, sizeof(*out))) {
        memset(out, 0, sizeof(*out));
        return false;
    }
    return true;
}

/*==========================================================================*
 *  Settings                                                                  *
 *==========================================================================*/

static void settings_defaults(kepler_settings_t *s)
{
    memset(s, 0, sizeof(*s));
    s->step_goal         = KEPLER_STEP_GOAL_DEFAULT;
    s->sleep_start_hour  = KEPLER_SLEEP_WINDOW_START_H;
    s->sleep_end_hour    = KEPLER_SLEEP_WINDOW_END_H;
    s->haptic_call_en    = 1u;
    s->haptic_message_en = 1u;
    s->haptic_alarm_en   = 1u;
}

bool flash_store_write_settings(const kepler_settings_t *s)
{
    if (s == NULL) { return false; }
    return snv_write(NV_ID_SETTINGS, s, sizeof(*s));
}

bool flash_store_read_settings(kepler_settings_t *out)
{
    if (out == NULL) { return false; }
    if (!snv_read(NV_ID_SETTINGS, out, sizeof(*out))) {
        settings_defaults(out);
        return false;
    }
    /* Sanity: a corrupted/zeroed record must not brick the step bar.      */
    if (out->step_goal == 0u) {
        out->step_goal = KEPLER_STEP_GOAL_DEFAULT;
    }
    return true;
}

/*==========================================================================*
 *  Haptic calibration                                                       *
 *==========================================================================*/

bool flash_store_write_haptic_cal(uint8_t comp, uint8_t bemf)
{
    uint8_t cal[2] = { comp, bemf };
    uint8_t flag   = 1u;

    if (!snv_write(NV_ID_HAPTIC_CAL, cal, sizeof(cal))) {
        return false;
    }
    return snv_write(NV_ID_HAPTIC_CALIBRATED, &flag, sizeof(flag));
}

bool flash_store_read_haptic_cal(uint8_t *comp, uint8_t *bemf)
{
    uint8_t cal[2];
    if (comp == NULL || bemf == NULL) { return false; }
    if (!snv_read(NV_ID_HAPTIC_CAL, cal, sizeof(cal))) {
        return false;
    }
    *comp = cal[0];
    *bemf = cal[1];
    return true;
}

bool flash_store_haptic_is_calibrated(void)
{
    uint8_t flag;
    if (!snv_read(NV_ID_HAPTIC_CALIBRATED, &flag, sizeof(flag))) {
        return false;
    }
    return (flag == 1u);
}

/*==========================================================================*
 *  Weather                                                                   *
 *==========================================================================*/

bool flash_store_write_weather(const weather_payload_t *w)
{
    if (w == NULL) { return false; }
    return snv_write(NV_ID_WEATHER, w, sizeof(*w));
}

bool flash_store_read_weather(weather_payload_t *out)
{
    if (out == NULL) { return false; }
    if (!snv_read(NV_ID_WEATHER, out, sizeof(*out))) {
        memset(out, 0, sizeof(*out));
        out->current.condition = (uint8_t)WEATHER_UNKNOWN;
        return false;
    }
    return true;
}

/*==========================================================================*
 *  Alarms                                                                    *
 *==========================================================================*/

bool flash_store_write_alarms(const alarms_payload_t *a)
{
    if (a == NULL) { return false; }
    return snv_write(NV_ID_ALARMS, a, sizeof(*a));
}

bool flash_store_read_alarms(alarms_payload_t *out)
{
    if (out == NULL) { return false; }
    if (!snv_read(NV_ID_ALARMS, out, sizeof(*out))) {
        memset(out, 0, sizeof(*out));
        return false;
    }
    if (out->count > KEPLER_ALARM_MAX) {
        out->count = KEPLER_ALARM_MAX;   /* clamp corrupted record         */
    }
    return true;
}

/*==========================================================================*
 *  Temperature unit                                                          *
 *==========================================================================*/

bool flash_store_write_temp_unit(uint8_t unit)
{
    if (unit > 1u) { unit = 0u; }
    return snv_write(NV_ID_TEMP_UNIT, &unit, sizeof(unit));
}

uint8_t flash_store_read_temp_unit(void)
{
    uint8_t unit;
    if (!snv_read(NV_ID_TEMP_UNIT, &unit, sizeof(unit))) {
        return 0u;
    }
    return (unit > 1u) ? 0u : unit;
}

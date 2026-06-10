/******************************************************************************
 *
 * @file  drv2605l.c
 *
 * @brief DRV2605L haptic controller driver implementation.
 *
 *        Init sequence (spec 03):
 *          1. Exit standby (MODE bit 6 = 0)
 *          2. Select ERM library 1
 *          3. MODE = internal trigger
 *          4. Auto-calibrate on first boot only (flag in flash);
 *             otherwise reload COMP/BEMF from flash
 *          5. Enter standby until first haptic event
 *
 *        RATED_VOLTAGE / OD_CLAMP below assume a 3 V coin ERM; tune
 *        against the chosen motor's datasheet during Phase 2 bring-up.
 *
 *****************************************************************************/

#include "drv2605l.h"
#include "../kepler_config.h"

#if KEPLER_HAS_DRV2605L

#include <stddef.h>

#include "../kepler_i2c.h"
#include "../storage/flash_store.h"

#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Clock.h>

/*--- ERM drive levels (3 V coin motor starting point) ----------------------*/
#define DRV_RATED_VOLTAGE_3V    0x90
#define DRV_OD_CLAMP_3V         0xA4

/*--- FEEDBACK register: ERM mode (bit7=0), defaults otherwise --------------*/
#define DRV_FEEDBACK_ERM        0x36

/*--- Auto-cal polling -------------------------------------------------------*/
#define AUTOCAL_POLL_MS         50u
#define AUTOCAL_POLL_MAX        40u     /* 40 x 50 ms = 2 s timeout         */

#define MS_TO_TICKS(ms)  ((uint32_t)(ms) * 1000u / Clock_tickPeriod)

/*--- Shorthand ----------------------------------------------------------------*/
#define DRV_WR(reg, val)  kepler_i2c_write_reg(I2C_ADDR_DRV2605L, (reg), (val))
#define DRV_RD(reg, p)    kepler_i2c_read_reg(I2C_ADDR_DRV2605L, (reg), (p))

static bool s_present;

/*==========================================================================*
 *  Internal helpers                                                         *
 *==========================================================================*/

static bool drv_exit_standby(void)
{
    return DRV_WR(DRV2605L_REG_MODE, DRV2605L_MODE_INT_TRIGGER);
}

static bool drv_enter_standby(void)
{
    return DRV_WR(DRV2605L_REG_MODE,
                  DRV2605L_MODE_STANDBY_BIT | DRV2605L_MODE_INT_TRIGGER);
}

/* Run auto-calibration; on success persist COMP/BEMF to flash.            */
static bool drv_autocalibrate(void)
{
    uint8_t status;
    uint8_t go;
    uint8_t comp;
    uint8_t bemf;
    uint8_t i;

    if (!DRV_WR(DRV2605L_REG_RATED_VOLTAGE, DRV_RATED_VOLTAGE_3V)) return false;
    if (!DRV_WR(DRV2605L_REG_OD_CLAMP,      DRV_OD_CLAMP_3V))      return false;
    if (!DRV_WR(DRV2605L_REG_FEEDBACK,      DRV_FEEDBACK_ERM))     return false;

    if (!DRV_WR(DRV2605L_REG_MODE, DRV2605L_MODE_AUTOCAL))         return false;
    if (!DRV_WR(DRV2605L_REG_GO, 0x01))                            return false;

    /* Auto-cal takes ~1.2 s; poll GO until it clears.                     */
    for (i = 0u; i < AUTOCAL_POLL_MAX; i++) {
        Task_sleep(MS_TO_TICKS(AUTOCAL_POLL_MS));
        if (!DRV_RD(DRV2605L_REG_GO, &go))                         return false;
        if ((go & 0x01u) == 0u) { break; }
    }
    if (i >= AUTOCAL_POLL_MAX)                                     return false;

    if (!DRV_RD(DRV2605L_REG_STATUS, &status))                     return false;
    if ((status & DRV2605L_STATUS_DIAG_FAIL) != 0u)                return false;

    if (!DRV_RD(DRV2605L_REG_AUTOCAL_COMP, &comp))                 return false;
    if (!DRV_RD(DRV2605L_REG_AUTOCAL_BEMF, &bemf))                 return false;

    return flash_store_write_haptic_cal(comp, bemf);
}

/* Reload a previous calibration from flash into the part.                 */
static bool drv_load_calibration(void)
{
    uint8_t comp;
    uint8_t bemf;

    if (!flash_store_read_haptic_cal(&comp, &bemf))                return false;

    if (!DRV_WR(DRV2605L_REG_FEEDBACK,     DRV_FEEDBACK_ERM))      return false;
    if (!DRV_WR(DRV2605L_REG_AUTOCAL_COMP, comp))                  return false;
    if (!DRV_WR(DRV2605L_REG_AUTOCAL_BEMF, bemf))                  return false;
    return true;
}

/*==========================================================================*
 *  Public API                                                               *
 *==========================================================================*/

bool drv2605l_init(void)
{
    uint8_t status;

    s_present = false;

    if (!kepler_i2c_open())                                        return false;

    /* Probe: STATUS device-ID field must read 0xE0 (DRV2605L).            */
    if (!DRV_RD(DRV2605L_REG_STATUS, &status))                     return false;
    if ((status & DRV2605L_STATUS_DEVID_MASK) != 0xE0u)            return false;

    if (!drv_exit_standby())                                       return false;
    if (!DRV_WR(DRV2605L_REG_LIBRARY, 0x01))                       return false;

    if (flash_store_haptic_is_calibrated()) {
        if (!drv_load_calibration())                               return false;
    } else {
        if (!drv_autocalibrate())                                  return false;
        if (!DRV_WR(DRV2605L_REG_MODE, DRV2605L_MODE_INT_TRIGGER)) return false;
    }

    if (!drv_enter_standby())                                      return false;

    s_present = true;
    return true;
}

bool drv2605l_play_sequence(const uint8_t *effects, uint8_t count)
{
    uint8_t i;

    if (!s_present || effects == NULL || count == 0u)              return false;
    if (count > DRV2605L_SEQ_MAX) { count = DRV2605L_SEQ_MAX; }

    if (!drv_exit_standby())                                       return false;

    for (i = 0u; i < count; i++) {
        if (!DRV_WR((uint8_t)(DRV2605L_REG_WAVESEQ1 + i), effects[i]))
            return false;
        if (effects[i] == 0u) { break; }   /* explicit end-of-sequence     */
    }
    /* Terminate the sequence if all slots were used.                      */
    if (i == count && count < DRV2605L_SEQ_MAX) {
        if (!DRV_WR((uint8_t)(DRV2605L_REG_WAVESEQ1 + count), 0x00))
            return false;
    }

    return DRV_WR(DRV2605L_REG_GO, 0x01);
}

bool drv2605l_retrigger(void)
{
    if (!s_present)                                                return false;
    if (!drv_exit_standby())                                       return false;
    return DRV_WR(DRV2605L_REG_GO, 0x01);
}

bool drv2605l_stop(void)
{
    if (!s_present)                                                return false;
    if (!DRV_WR(DRV2605L_REG_GO, 0x00))                            return false;
    return drv_enter_standby();
}

bool drv2605l_is_playing(void)
{
    uint8_t go;

    if (!s_present)                                                return false;
    if (!DRV_RD(DRV2605L_REG_GO, &go))                             return false;
    return ((go & 0x01u) != 0u);
}

#else /* !KEPLER_HAS_DRV2605L */

bool drv2605l_init(void)                  { return true;  }
bool drv2605l_play_sequence(const uint8_t *e, uint8_t c)
                                          { (void)e; (void)c; return false; }
bool drv2605l_retrigger(void)             { return false; }
bool drv2605l_stop(void)                  { return false; }
bool drv2605l_is_playing(void)            { return false; }

#endif /* KEPLER_HAS_DRV2605L */

/******************************************************************************
 *
 * @file  actigraphy.c
 *
 * @brief 5-minute-epoch sleep actigraphy implementation.
 *
 *        The epoch clock runs periodically all day (a Swi every 5 minutes
 *        is negligible power); epochs are only recorded while inside the
 *        sleep window, so daytime ticks are no-ops.
 *
 *****************************************************************************/

#include "actigraphy.h"
#include "../kepler_config.h"

#include <stddef.h>
#include <string.h>

#include <ti/drivers/Seconds.h>

/*--- Sleep window state (kept live even without the accelerometer) --------*/

static uint8_t s_window_start_h = KEPLER_SLEEP_WINDOW_START_H;
static uint8_t s_window_end_h   = KEPLER_SLEEP_WINDOW_END_H;

void actigraphy_set_window(uint8_t start_hour, uint8_t end_hour)
{
    if (start_hour < 24u) { s_window_start_h = start_hour; }
    if (end_hour   < 24u) { s_window_end_h   = end_hour;   }
}

bool actigraphy_in_sleep_window(void)
{
    /* Hour-of-day from the RTC.  Seconds_get() holds local time on this   *
     * watch (the phone syncs wall-clock time, not UTC).                   */
    uint8_t hour = (uint8_t)((Seconds_get() / 3600u) % 24u);

    if (s_window_start_h == s_window_end_h) { return false; }

    if (s_window_start_h < s_window_end_h) {
        return (hour >= s_window_start_h) && (hour < s_window_end_h);
    }
    /* Window wraps midnight (e.g. 22 -> 8). */
    return (hour >= s_window_start_h) || (hour < s_window_end_h);
}

#if KEPLER_ACTIGRAPHY

#include "../storage/flash_store.h"

#include <ti/sysbios/knl/Clock.h>

#define MS_TO_TICKS(ms)  ((uint32_t)(ms) * 1000u / Clock_tickPeriod)

/*--- Night record state -----------------------------------------------------*/

static actigraphy_night_t s_night;
static uint8_t            s_epoch_idx;
static volatile uint16_t  s_movement_count;
static bool               s_night_open;     /* an epoch has been recorded  */

static Clock_Struct       s_epoch_clk;

/*--- Epoch clock callback (Swi context — RAM only, no I2C, no flash) ------*/

static void epoch_swi(UArg arg)
{
    (void)arg;
    actigraphy_epoch_close();
}

/*==========================================================================*
 *  Public API                                                               *
 *==========================================================================*/

void actigraphy_init(void)
{
    Clock_Params p;

    memset(&s_night, 0, sizeof(s_night));
    s_epoch_idx      = 0u;
    s_movement_count = 0u;
    s_night_open     = false;

    Clock_Params_init(&p);
    p.period    = MS_TO_TICKS(ACTIGRAPHY_EPOCH_DURATION_SEC * 1000u);
    p.startFlag = TRUE;
    Clock_construct(&s_epoch_clk, epoch_swi,
                    MS_TO_TICKS(ACTIGRAPHY_EPOCH_DURATION_SEC * 1000u), &p);
}

void actigraphy_on_movement(void)
{
    if (actigraphy_in_sleep_window()) {
        s_movement_count++;
    }
}

void actigraphy_epoch_close(void)
{
    if (!actigraphy_in_sleep_window()) {
        s_movement_count = 0u;
        return;
    }

    if (s_epoch_idx >= ACTIGRAPHY_EPOCH_MAX) {     /* safety cap           */
        s_movement_count = 0u;
        return;
    }

    if (!s_night_open) {
        s_night.date = Seconds_get();
        s_night_open = true;
    }

    if (s_movement_count >= ACTIGRAPHY_EPOCH_THRESHOLD) {
        s_night.epochs[s_epoch_idx / 8u] |= (uint8_t)(1u << (s_epoch_idx % 8u));
    }
    s_epoch_idx++;
    s_movement_count = 0u;
}

void actigraphy_night_close(void)
{
    if (!s_night_open) { return; }   /* nothing recorded — nothing to save */

    s_night.epoch_count = s_epoch_idx;
    (void)flash_store_write_sleep(&s_night);

    memset(&s_night, 0, sizeof(s_night));
    s_epoch_idx      = 0u;
    s_movement_count = 0u;
    s_night_open     = false;
}

const actigraphy_night_t *actigraphy_current_night(void)
{
    return &s_night;
}

#else /* !KEPLER_ACTIGRAPHY */

static const actigraphy_night_t s_empty_night;

void actigraphy_init(void)          {}
void actigraphy_on_movement(void)   {}
void actigraphy_epoch_close(void)   {}
void actigraphy_night_close(void)   {}
const actigraphy_night_t *actigraphy_current_night(void)
{
    return &s_empty_night;
}

#endif /* KEPLER_ACTIGRAPHY */

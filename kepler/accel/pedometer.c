/******************************************************************************
 *
 * @file  pedometer.c
 *
 * @brief Daily step tracking over the LIS2DW12 hardware counter.
 *
 *        Crash recovery model: NV_ID_STEP_TODAY holds the day's base —
 *        steps accumulated before the last hardware-counter reset (a
 *        power cycle zeroes the LIS2DW12 counter).  On boot the base is
 *        reloaded and the hardware counter restarts from zero, so the
 *        displayed total survives a crash minus only the steps taken
 *        since the last lazy persist (every PEDO_PERSIST_DELTA steps).
 *
 *****************************************************************************/

#include "pedometer.h"
#include "../kepler_config.h"

#if KEPLER_STEP_COUNTER

#include <stddef.h>

#include "lis2dw12.h"
#include "../storage/flash_store.h"
#include "../power/event_queue.h"

/* Persist the recovery copy at most every this many new steps.            */
#define PEDO_PERSIST_DELTA  128u

static uint32_t s_base;        /* steps before last HW-counter reset       */
static uint32_t s_total;       /* cached daily total                       */
static uint32_t s_persisted;   /* total at last NV write                   */

void pedometer_init(void)
{
    s_base      = flash_store_read_step_today();
    s_total     = s_base;
    s_persisted = s_base;

    /* The HW counter is zero after power-on; if this is a warm restart    *
     * with a running counter, fold it into the base so it isn't counted   *
     * twice after the reset below.                                        */
    s_base += lis2dw12_read_steps();
    lis2dw12_reset_steps();
}

void pedometer_poll(void)
{
    uint32_t total = s_base + lis2dw12_read_steps();

    if (total == s_total) { return; }

    s_total = total;
    event_queue_post(EVT_STEP_UPDATE, s_total, NULL);

    if ((s_total - s_persisted) >= PEDO_PERSIST_DELTA) {
        if (flash_store_write_step_today(s_total)) {
            s_persisted = s_total;
        }
    }
}

uint32_t pedometer_get_steps(void)
{
    return s_total;
}

void pedometer_midnight_reset(void)
{
    /* Capture the final figure (including steps since last poll).         */
    uint32_t final_count = s_base + lis2dw12_read_steps();

    (void)flash_store_step_day(final_count);

    lis2dw12_reset_steps();
    s_base      = 0u;
    s_total     = 0u;
    s_persisted = 0u;
    (void)flash_store_write_step_today(0u);

    event_queue_post(EVT_STEP_UPDATE, 0u, NULL);
}

#else /* !KEPLER_STEP_COUNTER */

void     pedometer_init(void)            {}
void     pedometer_poll(void)            {}
uint32_t pedometer_get_steps(void)       { return 0u; }
void     pedometer_midnight_reset(void)  {}

#endif /* KEPLER_STEP_COUNTER */

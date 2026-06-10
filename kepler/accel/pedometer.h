/******************************************************************************
 *
 * @file  pedometer.h
 *
 * @brief Daily step counting on the LIS2DW12 hardware pedometer (Task 4).
 *
 *        The hardware counter accumulates autonomously; this module reads
 *        it on demand (display refresh cadence, no periodic interrupt),
 *        posts EVT_STEP_UPDATE when the total changes, persists a crash-
 *        recovery copy, and rolls the day over at midnight.
 *
 *        today_total = NV-recovered base + hardware counter.
 *
 *        Feature guard: KEPLER_STEP_COUNTER (== KEPLER_HAS_LIS2DW12).
 *
 *****************************************************************************/

#ifndef PEDOMETER_H
#define PEDOMETER_H

#include <stdint.h>
#include <stdbool.h>

/* Restore today's base count from flash (crash recovery).  Call after     *
 * lis2dw12_init() and flash_store_init().                                 */
void pedometer_init(void);

/* Read the hardware counter; if the daily total changed, post             *
 * EVT_STEP_UPDATE (param = total) and lazily persist the crash-recovery   *
 * copy.  Call at display-refresh cadence from task context.               */
void pedometer_poll(void);

/* Current daily total (cached — does not touch I2C).                      */
uint32_t pedometer_get_steps(void);

/* Midnight rollover: push today's total into the 7-day NV history,        *
 * reset the hardware counter and the recovery copy, post                  *
 * EVT_STEP_UPDATE(0).  Called from the main task on EVT_MIDNIGHT_RESET.  */
void pedometer_midnight_reset(void);

#endif /* PEDOMETER_H */

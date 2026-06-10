/******************************************************************************
 *
 * @file  actigraphy.h
 *
 * @brief Sleep tracking by movement actigraphy (Task 4).
 *
 *        During the configured sleep window (default 22:00-08:00) each
 *        LIS2DW12 wake-up interrupt counts as one movement event.  A
 *        5-minute epoch clock classifies each epoch restless (>= 3 events)
 *        or still, packing one bit per epoch into actigraphy_night_t.
 *        The night record is written to flash when the window closes.
 *
 *        Context rules: the epoch clock callback (Swi) touches RAM only.
 *        actigraphy_night_close() writes flash — task context only.
 *
 *        Feature guard: KEPLER_ACTIGRAPHY (== KEPLER_HAS_LIS2DW12).
 *        actigraphy_in_sleep_window() stays functional with the flag off
 *        (wrist-raise suppression must work without an accelerometer
 *        record store).
 *
 *****************************************************************************/

#ifndef ACTIGRAPHY_H
#define ACTIGRAPHY_H

#include <stdint.h>
#include <stdbool.h>
#include "../kepler_types.h"

#define ACTIGRAPHY_EPOCH_DURATION_SEC  300u  /* 5 minutes                  */
#define ACTIGRAPHY_EPOCH_THRESHOLD     3u    /* events => restless         */

/* Construct the epoch clock and reset the night record.                   */
void actigraphy_init(void);

/* Update the sleep window bounds (from settings).                         */
void actigraphy_set_window(uint8_t start_hour, uint8_t end_hour);

/* True while the current time (Seconds_get) is inside the sleep window.   */
bool actigraphy_in_sleep_window(void);

/* Count one movement event — called from the main task on                 *
 * EVT_WRIST_RAISE; increments only inside the sleep window.               */
void actigraphy_on_movement(void);

/* Close the current 5-minute epoch (RAM only).  Invoked by the periodic   *
 * epoch clock; exposed for tests.                                         */
void actigraphy_epoch_close(void);

/* Finalise the night: persist the record to flash and reset for the next  *
 * night.  Call from the main task when the sleep window ends.             */
void actigraphy_night_close(void);

/* Read-only view of the in-progress night (for BLE 0xFF04 / tests).       */
const actigraphy_night_t *actigraphy_current_night(void);

#endif /* ACTIGRAPHY_H */

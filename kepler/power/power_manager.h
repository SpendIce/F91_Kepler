/******************************************************************************
 *
 * @file  power_manager.h
 *
 * @brief Power state tracking + screen inactivity timer (Task 5).
 *
 *        TI-RTOS already drops the CC2640R2F into STANDBY whenever every
 *        task is blocked (the main task pends on the event queue), so this
 *        module does not call Power_* APIs directly.  It owns:
 *
 *          - the logical power state (IDLE / ACTIVE / BLE_CONNECTED) for
 *            diagnostics and for the BLE manager,
 *          - the screen inactivity clock: any button press resets it; on
 *            expiry it posts EVT_SCREEN_TIMEOUT (Swi posts only),
 *          - battery level reading via the AON battery monitor.
 *
 *****************************************************************************/

#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    POWER_IDLE          = 0,   /* pending on event queue, CPU in STANDBY   */
    POWER_ACTIVE        = 1,   /* handling an event                        */
    POWER_BLE_CONNECTED = 2,   /* connection maintained between events     */
} power_state_t;

/* Construct the inactivity clock.  Call once at startup.                  */
void power_manager_init(void);

/* Restart the KEPLER_SCREEN_TIMEOUT_MS inactivity window.  Call on every  *
 * user interaction (any button event, wrist-raise).                       */
void activity_timer_reset(void);

/* Stop the inactivity clock (used while in time-set mode, which has its   *
 * own 30 s timeout).                                                       */
void activity_timer_stop(void);

/* State bookkeeping — called from the main loop / BLE events.             */
void          power_manager_set_state(power_state_t st);
power_state_t power_manager_get_state(void);

/* Battery percent (0-100) from the AON battery monitor, mapped over the   *
 * LiPo 3.0-4.2 V discharge window.  Returns 100 on dev hardware where the *
 * monitor reads VDDS from the debugger supply.                            */
uint8_t power_manager_battery_pct(void);

#endif /* POWER_MANAGER_H */

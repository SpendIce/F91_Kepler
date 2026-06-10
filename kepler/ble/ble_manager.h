/******************************************************************************
 *
 * @file  ble_manager.h
 *
 * @brief BLE connection and advertising-interval management (Task 5).
 *
 *        This module is platform-glue-agnostic: the actual GAP/GATT calls
 *        live in the legacy CCS application (Firmware/f91_kepler_app),
 *        which registers a ble_port_t function table at init.  Phase 0
 *        sessions run with a NULL port (every hook is NULL-safe), so all
 *        policy logic is host-testable.
 *
 *        Advertising policy (spec 05):
 *          boot        -> fast (100 ms) for 30 s, then slow (2 s)
 *          disconnect  -> fast for 30 s, then slow
 *          connected   -> advertising off
 *
 *        Interval values are converted to 0.625 ms GAP units.
 *
 *****************************************************************************/

#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

/*--- Platform glue (implemented by the CCS application) -------------------*/
typedef struct {
    /* Set advertising interval (0.625 ms units) and (re)start advertising.*/
    void (*start_advertising)(uint16_t interval_units);
    /* Stop advertising entirely.                                          */
    void (*stop_advertising)(void);
    /* Send a GATT notification on a characteristic (0xFFxx shorthand).    */
    void (*notify_char)(uint16_t char_uuid, const uint8_t *value, uint16_t len);
} ble_port_t;

/*--- Connection state -------------------------------------------------------*/
typedef enum {
    BLE_STATE_ADVERTISING_FAST = 0,
    BLE_STATE_ADVERTISING_SLOW = 1,
    BLE_STATE_CONNECTED        = 2,
} ble_state_t;

/*==========================================================================*
 *  Public API                                                               *
 *==========================================================================*/

/* Register the platform port (NULL allowed for Phase 0) and start the     *
 * fast-advertising window.                                                 */
void ble_manager_init(const ble_port_t *port);

/* Called by the main task on EVT_BLE_CONNECTED / EVT_BLE_DISCONNECTED.   */
void ble_manager_on_connected(void);
void ble_manager_on_disconnected(void);

/* Called by the fast->slow window clock via the main task (the clock      *
 * posts no event of its own; kepler_main wires it — see kepler_main.c).  */
void ble_manager_adv_window_expired(void);

ble_state_t ble_manager_get_state(void);
bool        ble_manager_is_connected(void);

/* GATT notify helpers used by the services (NULL-safe without a port).    */
void ble_manager_notify(uint16_t char_uuid,
                        const uint8_t *value, uint16_t len);

/* ms -> 0.625 ms GAP advertising units (exposed for tests).               */
uint16_t ble_manager_ms_to_units(uint16_t ms);

#ifdef KEPLER_TEST_ONLY
/* Simulate expiry of the fast-advertising window clock.                   */
void ble_manager_test_fire_window(void);
#endif

#endif /* BLE_MANAGER_H */

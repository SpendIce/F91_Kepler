/******************************************************************************
 *
 * @file  kepler_main.h
 *
 * @brief Entry points for the Kepler application task and the BLE glue
 *        the legacy CCS application calls into (Task 5).
 *
 *****************************************************************************/

#ifndef KEPLER_MAIN_H
#define KEPLER_MAIN_H

#include <stdint.h>
#include <stdbool.h>
#include "ble/ble_manager.h"

/* Register the GAP/GATT platform port BEFORE the task starts.  NULL is    *
 * valid for Phase 0 launchpad bring-up (BLE notifies become no-ops).      */
void kepler_main_set_ble_port(const ble_port_t *port);

/* Task function: full module init, then the central event loop.           *
 * Never returns.  Wrap in the TI-RTOS task creation of the CCS app.       */
void kepler_main_task(void);

/* GATT write router — call from the BLE stack's write callbacks.          */
bool kepler_ble_on_char_write(uint16_t char_uuid,
                              const uint8_t *data, uint16_t len);

/* GAP connection callbacks — call from the peripheral role handlers.      */
void kepler_ble_on_connected(void);
void kepler_ble_on_disconnected(void);

#endif /* KEPLER_MAIN_H */

/******************************************************************************
 *
 * @file  kepler_gatt_service.h
 *
 * @brief Public interface for the Kepler custom GATT service (0xFFFF).
 *
 *        This is the spec 0xFFFF service implemented fresh on BLE5-Stack.
 *        The legacy CC2640R2 FA35xxxx profiles are reference-only and are
 *        NOT migrated here.
 *
 *        STATUS: PORT SKELETON — written against BLE5-Stack documentation;
 *        compile against SDK 7.x at board bring-up.
 *
 *****************************************************************************/

#ifndef KEPLER_GATT_SERVICE_H
#define KEPLER_GATT_SERVICE_H

#include <stdint.h>
#include <bcomdef.h>   /* bStatus_t */

/*==========================================================================*
 *  Public API                                                               *
 *==========================================================================*/

/* Register the Kepler service attributes with the BLE5-Stack GATT server.  *
 * Must be called once, before the BLE stack begins accepting connections.  */
bStatus_t KeplerGattService_addService(void);

/* Update a watch->app characteristic's stored value and, if a client has
 * enabled notifications via the CCCD, send a GATT notification.
 * Valid UUIDs: KEPLER_CHAR_STEPS (14 B), KEPLER_CHAR_LOCATOR (1 B),
 * KEPLER_CHAR_WX_REFRESH (1 B). KEPLER_CHAR_SLEEP (20 B) is read-only
 * (no CCCD) — the call just updates the stored value.                      */
bStatus_t KeplerGattService_setValue(uint16_t char_uuid,
                                     const uint8_t *value, uint16_t len);

#endif /* KEPLER_GATT_SERVICE_H */

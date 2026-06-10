/******************************************************************************
 *
 * @file  notif_service.h
 *
 * @brief Notification relay service (0xFF01) + step/battery notify (Task 5).
 *
 *        Flow: the GATT write callback (BLE stack context) hands the raw
 *        64-byte notif_payload_t to notif_service_on_write(), which copies
 *        it into a static slot pool and posts EVT_BLE_NOTIFICATION with a
 *        pointer to the parsed ui_notification_t.  The main task then
 *        plays haptics, pushes it into the UI ring and shows the banner.
 *
 *        The UI renderer (Task 1) owns the canonical on-watch ring buffer;
 *        the dismiss/scroll entry points here delegate to it so there is
 *        exactly one notification store.  (Spec 06 sketches a second ring
 *        inside notif_service — collapsed deliberately, flagged in the
 *        session report.)
 *
 *****************************************************************************/

#ifndef NOTIF_SERVICE_H
#define NOTIF_SERVICE_H

#include <stdint.h>
#include <stdbool.h>
#include "../kepler_types.h"
#include "../display/ui_renderer.h"

/*==========================================================================*
 *  GATT-side entry points (called from BLE stack write callbacks)          *
 *==========================================================================*/

/* 0xFF01 write: validate length, parse, post EVT_BLE_NOTIFICATION.        *
 * Safe to call from the BLE stack task; the payload is copied before      *
 * return.  Returns false on malformed input (too short).                  */
bool notif_service_on_write(const uint8_t *data, uint16_t len);

/* 0xFF02 write: 4-byte little-endian Unix timestamp -> EVT_TIME_SYNC.    */
bool notif_service_on_time_sync(const uint8_t *data, uint16_t len);

/* 0xFF05 write: 16-byte kepler_settings_t -> applied + persisted by the  *
 * main task (posts EVT via param? No — applied inline, RAM+flash only).   *
 * Returns false on malformed input.                                       */
bool notif_service_on_settings(const uint8_t *data, uint16_t len);

/*==========================================================================*
 *  Watch-side actions (called by the main task)                            *
 *==========================================================================*/

/* Dismiss the currently selected notification (BTN_1 on NOTIFICATIONS).   */
void notif_dismiss_selected(void);

/* Move selection to the next (older) notification (BTN_2).                */
void notif_scroll_next(void);

/* Active (stored) notification count.                                     */
uint8_t notif_count_active(void);

/*==========================================================================*
 *  Notify characteristics (watch -> app)                                   *
 *==========================================================================*/

/* 0xFF03: push the 7-day step history (uint16_t[7], LE) to the app.       */
void notif_service_notify_steps(void);

/* 0x2A19: push battery percent if it moved more than 5 % since last push. */
void notif_service_notify_battery(uint8_t percent);

#endif /* NOTIF_SERVICE_H */

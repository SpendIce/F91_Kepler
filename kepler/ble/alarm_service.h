/******************************************************************************
 *
 * @file  alarm_service.h
 *
 * @brief Alarm sync GATT handling (0xFF08 / 0xFF09, Task 6).
 *
 *        Alarms are created in the companion app and synced read-mostly:
 *        the watch can only toggle entries on/off.  Toggles are applied to
 *        g_alarms, persisted, and (when a port is registered) pushed back
 *        to the app by notifying the full payload on 0xFF08.
 *        [Spec 06 marks the write-back characteristic TBD — reusing 0xFF08
 *        as Notify is the minimal-surface answer; revisit with the app.]
 *
 *****************************************************************************/

#ifndef ALARM_SERVICE_H
#define ALARM_SERVICE_H

#include <stdint.h>
#include <stdbool.h>
#include "../kepler_types.h"

#define ALARM_TRIGGER_DISMISS_ALL  0xFFu
#define ALARM_TRIGGER_NONE         0xFEu

/*--- Global alarm state (spec 06) -------------------------------------------*/
extern alarms_payload_t g_alarms;

/*==========================================================================*
 *  Public API                                                               *
 *==========================================================================*/

/* Load persisted alarms from NV.  Call after flash_store_init().          */
void alarm_service_init(void);

/* 0xFF08 write: validate, store, persist, post EVT_ALARMS_UPDATE.         */
bool alarm_service_on_write(const uint8_t *data, uint16_t len);

/* 0xFF09 write: 1 byte alarm index (0-4) or 0xFF = dismiss all.           *
 * Posts EVT_ALARM_TRIGGER with the index as param.                        */
bool alarm_service_on_trigger(const uint8_t *data, uint16_t len);

/*--- Watch-side actions (main task) ------------------------------------------*/

/* BTN_1 short on ALARMS: toggle selected alarm, persist, notify app.      */
void alarms_toggle_selected(void);

/* BTN_2 short on ALARMS: move selection down (wraps).                     */
void alarms_scroll_next(void);

/* EVT_ALARM_TRIGGER handler: remember which alarm fired (for the UI).     */
void alarms_show_triggered(uint8_t index);

/* BTN_1 while an alarm is showing as triggered: clear it.                 */
void alarms_clear_triggered(void);

/*--- UI queries -----------------------------------------------------------------*/

uint8_t alarms_get_selected(void);
uint8_t alarms_get_triggered(void);   /* ALARM_TRIGGER_NONE if none        */

#endif /* ALARM_SERVICE_H */

/******************************************************************************
 *
 * @file  notif_service.c
 *
 * @brief Notification relay parsing + step/battery notify.
 *
 *        Slot pool: 4 static ui_notification_t slots used round-robin so
 *        bursts of writes don't clobber an event still in the queue.
 *        (The queue holds 16 events; 4 in-flight notifications is already
 *        beyond what a 500 ms connection interval can deliver.)
 *
 *        app_id -> display-name mapping must match the companion app's
 *        enum (Software/, Phase 1).  Unknown IDs render as "App".
 *
 *****************************************************************************/

#include "notif_service.h"
#include "../kepler_config.h"

#include <stddef.h>
#include <string.h>

#include "gatt_uuids.h"
#include "ble_manager.h"
#include "../power/event_queue.h"
#include "../storage/flash_store.h"
#include "../accel/actigraphy.h"

#include <ti/sysbios/hal/Seconds.h>

/*--- Slot pool ----------------------------------------------------------------*/

/* 2 in-flight slots: a 500 ms connection interval cannot deliver writes   *
 * faster than the main task drains them.  (Was 4; halved for R1.)         */
#define NOTIF_SLOTS  2u

static ui_notification_t s_slots[NOTIF_SLOTS];
static uint8_t           s_slot_next;

/*--- app_id -> name (must match companion app enum) -------------------------*/

static const char *const k_app_names[] = {
    "Phone",      /* 0 */
    "WhatsApp",   /* 1 */
    "Telegram",   /* 2 */
    "SMS",        /* 3 */
    "Gmail",      /* 4 */
    "Instagram",  /* 5 */
};
#define APP_NAME_COUNT  (sizeof(k_app_names) / sizeof(k_app_names[0]))

/*--- Battery notify hysteresis ----------------------------------------------*/

#define BATTERY_NOTIFY_DELTA  5u

static uint8_t s_battery_last = 0xFFu;   /* force first notify             */

/*==========================================================================*
 *  GATT-side entry points                                                   *
 *==========================================================================*/

bool notif_service_on_write(const uint8_t *data, uint16_t len)
{
    const notif_payload_t *p;
    ui_notification_t     *slot;
    const char            *app;

    if (data == NULL || len < sizeof(notif_payload_t)) { return false; }

    p    = (const notif_payload_t *)data;
    slot = &s_slots[s_slot_next];
    s_slot_next = (uint8_t)((s_slot_next + 1u) % NOTIF_SLOTS);

    memset(slot, 0, sizeof(*slot));
    slot->type = p->type;

    app = (p->app_id < APP_NAME_COUNT) ? k_app_names[p->app_id] : "App";
    strncpy(slot->app_name, app, sizeof(slot->app_name) - 1u);
    strncpy(slot->sender, p->sender, sizeof(slot->sender) - 1u);
    /* Wire text is max 40 chars; the UI field holds 60 — direct copy.     */
    strncpy(slot->text, p->text, sizeof(slot->text) - 1u);
    slot->timestamp = Seconds_get();

    event_queue_post(EVT_BLE_NOTIFICATION, slot->type, slot);
    return true;
}

bool notif_service_on_time_sync(const uint8_t *data, uint16_t len)
{
    uint32_t ts;

    if (data == NULL || len < 4u) { return false; }

    ts = (uint32_t)data[0]
       | ((uint32_t)data[1] << 8)
       | ((uint32_t)data[2] << 16)
       | ((uint32_t)data[3] << 24);

    event_queue_post(EVT_TIME_SYNC, ts, NULL);
    return true;
}

bool notif_service_on_settings(const uint8_t *data, uint16_t len)
{
    kepler_settings_t s;

    if (data == NULL || len < sizeof(kepler_settings_t)) { return false; }

    memcpy(&s, data, sizeof(s));
    if (s.step_goal == 0u) { s.step_goal = KEPLER_STEP_GOAL_DEFAULT; }

    (void)flash_store_write_settings(&s);
    actigraphy_set_window(s.sleep_start_hour, s.sleep_end_hour);
    return true;
}

/*==========================================================================*
 *  Watch-side actions                                                       *
 *==========================================================================*/

void notif_dismiss_selected(void)
{
    ui_dismiss_selected_notification();
}

void notif_scroll_next(void)
{
    ui_scroll_notifications(1);
}

uint8_t notif_count_active(void)
{
    return ui_notif_count();
}

/*==========================================================================*
 *  Notify characteristics                                                   *
 *==========================================================================*/

void notif_service_notify_steps(void)
{
    uint16_t hist[7];
    uint8_t  buf[14];

    (void)flash_store_read_steps(hist);

    for (uint8_t i = 0u; i < 7u; i++) {
        buf[2u * i]      = (uint8_t)(hist[i] & 0xFFu);
        buf[2u * i + 1u] = (uint8_t)(hist[i] >> 8);
    }
    ble_manager_notify(KEPLER_CHAR_STEPS, buf, sizeof(buf));
}

void notif_service_notify_battery(uint8_t percent)
{
    uint8_t delta;

    if (percent > 100u) { percent = 100u; }

    delta = (percent > s_battery_last) ? (uint8_t)(percent - s_battery_last)
                                       : (uint8_t)(s_battery_last - percent);
    if (s_battery_last != 0xFFu && delta <= BATTERY_NOTIFY_DELTA) { return; }

    s_battery_last = percent;
    ble_manager_notify(KEPLER_CHAR_BATTERY, &percent, 1u);
}

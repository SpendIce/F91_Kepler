/******************************************************************************
 *
 * @file  alarm_service.c
 *
 * @brief Alarm list sync, selection, toggling and trigger handling.
 *
 *****************************************************************************/

#include "alarm_service.h"
#include "../kepler_config.h"

#include <stddef.h>
#include <string.h>

#include "gatt_uuids.h"
#include "ble_manager.h"
#include "../power/event_queue.h"
#include "../storage/flash_store.h"

/*--- Global alarm state ---------------------------------------------------*/

alarms_payload_t g_alarms;

static uint8_t s_selected;
static uint8_t s_triggered = ALARM_TRIGGER_NONE;

/*==========================================================================*
 *  Public API                                                               *
 *==========================================================================*/

void alarm_service_init(void)
{
    (void)flash_store_read_alarms(&g_alarms);
    s_selected  = 0u;
    s_triggered = ALARM_TRIGGER_NONE;
}

bool alarm_service_on_write(const uint8_t *data, uint16_t len)
{
    if (data == NULL || len < 1u) { return false; }

    /* Partial payloads are valid: count + count*13 bytes.                 */
    uint8_t count = data[0];
    if (count > KEPLER_ALARM_MAX) { return false; }
    if (len < (uint16_t)(1u + (uint16_t)count * sizeof(alarm_entry_t))) {
        return false;
    }

    memset(&g_alarms, 0, sizeof(g_alarms));
    g_alarms.count = count;
    memcpy(g_alarms.alarms, &data[1],
           (size_t)count * sizeof(alarm_entry_t));

    /* Ensure labels are terminated regardless of what the app sent.       */
    for (uint8_t i = 0u; i < count; i++) {
        g_alarms.alarms[i].label[sizeof(g_alarms.alarms[i].label) - 1u] = '\0';
    }

    if (s_selected >= count) { s_selected = 0u; }

    (void)flash_store_write_alarms(&g_alarms);
    event_queue_post(EVT_ALARMS_UPDATE, 0u, NULL);
    return true;
}

bool alarm_service_on_trigger(const uint8_t *data, uint16_t len)
{
    if (data == NULL || len < 1u) { return false; }

    if (data[0] == ALARM_TRIGGER_DISMISS_ALL) {
        event_queue_post(EVT_ALARM_DISMISS, 0u, NULL);
        return true;
    }
    if (data[0] >= KEPLER_ALARM_MAX) { return false; }

    event_queue_post(EVT_ALARM_TRIGGER, data[0], NULL);
    return true;
}

/*--- Watch-side actions ------------------------------------------------------*/

void alarms_toggle_selected(void)
{
    if (g_alarms.count == 0u || s_selected >= g_alarms.count) { return; }

    g_alarms.alarms[s_selected].enabled ^= 1u;
    (void)flash_store_write_alarms(&g_alarms);

    /* Write-back so the app reflects the watch-side toggle.               */
    ble_manager_notify(KEPLER_CHAR_ALARMS,
                       (const uint8_t *)&g_alarms,
                       (uint16_t)(1u + (uint16_t)g_alarms.count
                                       * sizeof(alarm_entry_t)));
}

void alarms_scroll_next(void)
{
    if (g_alarms.count == 0u) { return; }
    s_selected = (uint8_t)((s_selected + 1u) % g_alarms.count);
}

void alarms_show_triggered(uint8_t index)
{
    if (index < g_alarms.count) {
        s_triggered = index;
    }
}

void alarms_clear_triggered(void)
{
    s_triggered = ALARM_TRIGGER_NONE;
}

/*--- UI queries -------------------------------------------------------------------*/

uint8_t alarms_get_selected(void)
{
    return s_selected;
}

uint8_t alarms_get_triggered(void)
{
    return s_triggered;
}

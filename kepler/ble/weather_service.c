/******************************************************************************
 *
 * @file  weather_service.c
 *
 * @brief Weather payload handling, unit conversion and refresh requests.
 *
 *****************************************************************************/

#include "weather_service.h"
#include "../kepler_config.h"

#include <stddef.h>
#include <string.h>

#include "gatt_uuids.h"
#include "ble_manager.h"
#include "../power/event_queue.h"
#include "../storage/flash_store.h"
#include "../display/ui_renderer.h"

#include <ti/sysbios/hal/Seconds.h>

/*--- Global wire-format state (spec 06) ------------------------------------*/

weather_payload_t g_weather;
bool              g_weather_valid = false;

/*--- UI view ------------------------------------------------------------------*/

static weather_data_t s_ui;
static uint8_t        s_unit;    /* 0 = Celsius, 1 = Fahrenheit            */

/*--- Helpers ----------------------------------------------------------------*/

static int8_t to_unit(int8_t temp_c)
{
    int16_t t;

    if (s_unit == 0u) { return temp_c; }
    t = (int16_t)((temp_c * 9) / 5 + 32);
    if (t > 127)  { t = 127;  }
    if (t < -128) { t = -128; }
    return (int8_t)t;
}

/* Rebuild the UI view from g_weather and push to the renderer.            */
static void rebuild_ui_view(void)
{
    if (!g_weather_valid) {
        ui_update_weather(NULL);
        return;
    }

    memset(&s_ui, 0, sizeof(s_ui));
    s_ui.condition    = g_weather.current.condition;
    s_ui.temp         = to_unit(g_weather.current.temp_c);
    s_ui.feels_like   = to_unit(g_weather.current.feels_like_c);
    s_ui.humidity_pct = g_weather.current.humidity_pct;
    s_ui.updated_at   = g_weather.current.updated_at;
    s_ui.unit         = (s_unit == 0u) ? 'C' : 'F';

    /* Renderer column 0 is "NOW" (current conditions); columns 1-5 take   *
     * the first five forecast slots.                                       */
    for (uint8_t i = 1u; i < 6u; i++) {
        s_ui.hourly[i].condition = g_weather.hourly[i - 1u].condition;
        s_ui.hourly[i].temp      = to_unit(g_weather.hourly[i - 1u].temp_c);
        s_ui.hourly[i].hour      = g_weather.hourly[i - 1u].hour;
    }

    ui_update_weather(&s_ui);
}

/*==========================================================================*
 *  Public API                                                               *
 *==========================================================================*/

void weather_service_init(void)
{
    s_unit = flash_store_read_temp_unit();

    if (flash_store_read_weather(&g_weather)) {
        g_weather_valid = true;
    }
    rebuild_ui_view();
}

bool weather_service_on_write(const uint8_t *data, uint16_t len)
{
    if (data == NULL || len < sizeof(weather_payload_t)) { return false; }

    memcpy(&g_weather, data, sizeof(g_weather));
    if (g_weather.current.condition >= WEATHER_COND_COUNT &&
        g_weather.current.condition != (uint8_t)WEATHER_UNKNOWN) {
        g_weather.current.condition = (uint8_t)WEATHER_UNKNOWN;
    }
    g_weather_valid = true;

    (void)flash_store_write_weather(&g_weather);
    event_queue_post(EVT_WEATHER_UPDATE, 0u, NULL);
    return true;
}

void weather_service_apply(void)
{
    rebuild_ui_view();
}

void weather_service_toggle_units(void)
{
    s_unit ^= 1u;
    (void)flash_store_write_temp_unit(s_unit);
    rebuild_ui_view();
}

void weather_service_request_refresh(void)
{
    static const uint8_t req = 0x01u;
    ble_manager_notify(KEPLER_CHAR_WX_REFRESH, &req, 1u);
}

uint16_t weather_service_age_min(void)
{
    uint32_t now;

    if (!g_weather_valid) { return 0xFFFFu; }

    now = Seconds_get();
    if (now <= g_weather.current.updated_at) { return 0u; }

    uint32_t min = (now - g_weather.current.updated_at) / 60u;
    return (min > 0xFFFEu) ? 0xFFFEu : (uint16_t)min;
}

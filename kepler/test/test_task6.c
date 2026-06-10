/******************************************************************************
 *
 * @file  test_task6.c
 *
 * @brief Host tests for Task 6 — weather, alarms, phone locator, stopwatch.
 *
 *          gcc -Wall -Wextra -std=c99 -DKEPLER_TEST_ONLY \
 *              -I kepler/test/stubs \
 *              kepler/test/test_task6.c kepler/test/stubs/mocks.c \
 *              kepler/power/event_queue.c kepler/storage/flash_store.c \
 *              kepler/ble/ble_manager.c kepler/ble/weather_service.c \
 *              kepler/ble/alarm_service.c kepler/ble/locator_service.c \
 *              kepler/screens/stopwatch.c kepler/accel/actigraphy.c \
 *              kepler/display/sharp_lcd.c kepler/display/fonts.c \
 *              kepler/display/ui_renderer.c kepler/display/weather_icons.c \
 *              -o kepler/test/test_task6
 *
 *****************************************************************************/

#include <stdio.h>
#include <string.h>

#include "stubs/mock_state.h"
#include "osal_snv.h"

#include "../power/event_queue.h"
#include "../storage/flash_store.h"
#include "../ble/ble_manager.h"
#include "../ble/gatt_uuids.h"
#include "../ble/weather_service.h"
#include "../ble/alarm_service.h"
#include "../ble/locator_service.h"
#include "../screens/stopwatch.h"
#include "../display/ui_renderer.h"
#include "../kepler_types.h"
#include "../kepler_config.h"

/*--- Task 1/2-convention mock globals ------------------------------------*/
uint8_t          mock_spi_buf[MOCK_SPI_BUFSIZE];
uint16_t         mock_spi_len;
int              mock_spi_call_count;
mock_pin_event_t mock_pin_log[MOCK_PIN_LOG_SIZE];
int              mock_pin_log_count;
mock_pin_int_cb_t mock_pin_int_cb;
void            *mock_pin_int_handle;
uint8_t          mock_pin_input[32];
uint32_t         mock_seconds_value;

void mock_spi_reset(void)
{
    memset(mock_spi_buf, 0, sizeof(mock_spi_buf));
    mock_spi_len        = 0u;
    mock_spi_call_count = 0;
}

static int s_pass;
static int s_fail;

#define CHECK(cond, name)                                            \
    do {                                                             \
        if (cond) { s_pass++; }                                      \
        else      { s_fail++; printf("FAIL: %s\n", name); }          \
    } while (0)

/*--- Port capture ----------------------------------------------------------*/

static uint16_t s_notify_uuid;
static uint8_t  s_notify_val[80];
static uint16_t s_notify_len;

static void port_start_adv(uint16_t u)  { (void)u; }
static void port_stop_adv(void)         {}
static void port_notify(uint16_t uuid, const uint8_t *v, uint16_t len)
{
    s_notify_uuid = uuid;
    s_notify_len  = (len <= sizeof(s_notify_val)) ? len : 0u;
    if (s_notify_len) { memcpy(s_notify_val, v, s_notify_len); }
}

static const ble_port_t k_port = { port_start_adv, port_stop_adv, port_notify };

static uint32_t drain_for(kepler_event_t type)
{
    kepler_event_msg_t msg;
    uint32_t found = 0xFFFFFFFFu;
    while (event_queue_pend(&msg, 0u)) {
        if (msg.type == type) { found = msg.param; }
    }
    return found;
}

/*==========================================================================*
 *  Weather service                                                          *
 *==========================================================================*/

static void test_weather_service(void)
{
    weather_payload_t w;

    event_queue_init();
    mock_snv_reset();
    ui_init();
    weather_service_init();
    CHECK(!g_weather_valid, "no weather before first push");

    memset(&w, 0, sizeof(w));
    w.current.temp_c       = 20;
    w.current.feels_like_c = 18;
    w.current.condition    = WEATHER_RAIN;
    w.current.humidity_pct = 65u;
    w.current.updated_at   = 1000u;
    w.hourly[0].hour       = 14u;
    w.hourly[0].temp_c     = 21;
    w.hourly[0].condition  = WEATHER_CLOUDY;

    CHECK(!weather_service_on_write((const uint8_t *)&w, 10u),
          "short weather write rejected");
    CHECK(weather_service_on_write((const uint8_t *)&w, sizeof(w)),
          "valid weather write accepted");
    CHECK(g_weather_valid, "weather valid after write");
    CHECK(drain_for(EVT_WEATHER_UPDATE) != 0xFFFFFFFFu,
          "EVT_WEATHER_UPDATE posted");
    {
        weather_payload_t r;
        CHECK(flash_store_read_weather(&r) && r.current.temp_c == 20,
              "weather persisted to flash");
    }

    /* Unit toggle persists and survives re-init                           */
    weather_service_toggle_units();
    CHECK(flash_store_read_temp_unit() == 1u, "Fahrenheit persisted");
    weather_service_toggle_units();
    CHECK(flash_store_read_temp_unit() == 0u, "back to Celsius");

    /* Refresh request notifies 0xFF0A with 0x01                           */
    ble_manager_init(&k_port);
    s_notify_uuid = 0u;
    weather_service_request_refresh();
    CHECK(s_notify_uuid == KEPLER_CHAR_WX_REFRESH && s_notify_val[0] == 1u,
          "refresh request notified on 0xFF0A");

    /* Age calculation                                                     */
    mock_seconds_value = 1000u + 180u;
    CHECK(weather_service_age_min() == 3u, "age = 3 min");
}

/*==========================================================================*
 *  Alarm service                                                            *
 *==========================================================================*/

static void test_alarm_service(void)
{
    uint8_t buf[1 + 2 * 13];

    event_queue_init();
    mock_snv_reset();
    alarm_service_init();
    CHECK(g_alarms.count == 0u, "no alarms initially");

    /* Build a 2-alarm payload                                             */
    memset(buf, 0, sizeof(buf));
    buf[0] = 2u;
    {
        alarm_entry_t a = { 7u, 30u, 1u, 0x1Fu, "Wake" };
        alarm_entry_t b = { 22u, 0u, 0u, 0x7Fu, "Sleep" };
        memcpy(&buf[1], &a, sizeof(a));
        memcpy(&buf[1 + 13], &b, sizeof(b));
    }

    CHECK(!alarm_service_on_write(buf, 5u), "truncated alarms rejected");
    CHECK(alarm_service_on_write(buf, sizeof(buf)), "valid alarms accepted");
    CHECK(g_alarms.count == 2u, "two alarms loaded");
    CHECK(drain_for(EVT_ALARMS_UPDATE) != 0xFFFFFFFFu,
          "EVT_ALARMS_UPDATE posted");

    /* Toggle writes back via notify on 0xFF08                             */
    s_notify_uuid = 0u;
    CHECK(g_alarms.alarms[0].enabled == 1u, "alarm 0 starts enabled");
    alarms_toggle_selected();
    CHECK(g_alarms.alarms[0].enabled == 0u, "toggle flips enabled");
    CHECK(s_notify_uuid == KEPLER_CHAR_ALARMS, "toggle notified to app");
    {
        alarms_payload_t r;
        CHECK(flash_store_read_alarms(&r) && r.alarms[0].enabled == 0u,
              "toggle persisted");
    }

    /* Scroll wraps                                                        */
    CHECK(alarms_get_selected() == 0u, "selection starts at 0");
    alarms_scroll_next();
    CHECK(alarms_get_selected() == 1u, "scroll to 1");
    alarms_scroll_next();
    CHECK(alarms_get_selected() == 0u, "scroll wraps to 0");

    /* Trigger validation                                                  */
    {
        uint8_t idx = 1u;
        CHECK(alarm_service_on_trigger(&idx, 1u), "valid trigger");
        CHECK(drain_for(EVT_ALARM_TRIGGER) == 1u, "trigger index in param");
        idx = 9u;
        CHECK(!alarm_service_on_trigger(&idx, 1u), "out-of-range rejected");
        idx = 0xFFu;
        CHECK(alarm_service_on_trigger(&idx, 1u), "dismiss-all accepted");
        CHECK(drain_for(EVT_ALARM_DISMISS) != 0xFFFFFFFFu,
              "dismiss-all posts EVT_ALARM_DISMISS");
    }

    alarms_show_triggered(1u);
    CHECK(alarms_get_triggered() == 1u, "triggered alarm recorded");
    alarms_clear_triggered();
    CHECK(alarms_get_triggered() == ALARM_TRIGGER_NONE, "trigger cleared");
}

/*==========================================================================*
 *  Phone locator                                                            *
 *==========================================================================*/

static void test_locator(void)
{
    event_queue_init();
    ui_init();
    ble_manager_init(&k_port);
    locator_service_init();

    /* Not connected: start is refused                                     */
    CHECK(!locator_service_is_ringing(), "idle initially");
    locator_service_start();
    CHECK(!locator_service_is_ringing(), "no ring while disconnected");

    ble_manager_on_connected();
    s_notify_uuid = 0u;
    locator_service_start();
    CHECK(locator_service_is_ringing(), "ringing after start");
    CHECK(s_notify_uuid == KEPLER_CHAR_LOCATOR && s_notify_val[0] == 0x01u,
          "start command 0x01 on 0xFF07");

    /* App ACK posts event                                                 */
    {
        uint8_t ack = 0x01u;
        CHECK(locator_service_on_write(&ack, 1u), "ACK accepted");
        CHECK(drain_for(EVT_PHONE_LOCATOR_ACK) != 0xFFFFFFFFu,
              "EVT_PHONE_LOCATOR_ACK posted");
    }

    s_notify_uuid = 0u;
    locator_service_stop();
    CHECK(!locator_service_is_ringing(), "idle after stop");
    CHECK(s_notify_uuid == KEPLER_CHAR_LOCATOR && s_notify_val[0] == 0x00u,
          "stop command 0x00 on 0xFF07");
}

/*==========================================================================*
 *  Stopwatch                                                                *
 *==========================================================================*/

static void test_stopwatch(void)
{
    char buf[16];

    event_queue_init();
    stopwatch_init();
    CHECK(stopwatch_get_state() == STOPWATCH_IDLE, "idle after init");

    stopwatch_start();
    CHECK(stopwatch_is_running(), "running after start");

    /* 25 ticks = 250 ms; events every 10 ticks                            */
    for (int i = 0; i < 25; i++) { stopwatch_test_fire_tick(); }
    CHECK(stopwatch_elapsed_cs() == 25u, "25 cs elapsed");
    CHECK(drain_for(EVT_STOPWATCH_TICK) == 20u,
          "tick events every 100 ms (last at 20 cs)");

    stopwatch_lap();
    for (int i = 0; i < 10; i++) { stopwatch_test_fire_tick(); }
    stopwatch_lap();
    {
        uint32_t laps[3];
        uint8_t  n = stopwatch_get_laps(laps, 3u);
        CHECK(n == 2u, "two laps stored");
        CHECK(laps[0] == 10u, "newest lap first (10 cs)");
        CHECK(laps[1] == 25u, "first lap second (25 cs)");
    }

    stopwatch_stop();
    CHECK(stopwatch_get_state() == STOPWATCH_PAUSED, "paused after stop");
    stopwatch_reset();
    CHECK(stopwatch_get_state() == STOPWATCH_IDLE
          && stopwatch_elapsed_cs() == 0u
          && stopwatch_get_laps(NULL, 0u) == 0u, "reset clears state");

    /* Reset is refused while running                                      */
    stopwatch_start();
    stopwatch_test_fire_tick();
    stopwatch_reset();
    CHECK(stopwatch_elapsed_cs() == 1u, "reset ignored while running");
    stopwatch_stop();

    /* Formatting                                                          */
    stopwatch_format(32341u, buf, sizeof(buf));        /* 5m 23.41s        */
    CHECK(strcmp(buf, "05:23.41") == 0, "format MM:SS.cc");
    stopwatch_format(99u * 6000u + 60000u, buf, sizeof(buf));
    CHECK(strncmp(buf, "99:", 3u) == 0, "display caps at 99 min");
}

int main(void)
{
    test_weather_service();
    test_alarm_service();
    test_locator();
    test_stopwatch();

    printf("test_task6: %d passed, %d failed\n", s_pass, s_fail);
    return (s_fail == 0) ? 0 : 1;
}

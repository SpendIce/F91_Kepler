/******************************************************************************
 *
 * @file  test_task5.c
 *
 * @brief Host tests for Task 5 — event queue, flash store, BLE manager,
 *        notification service, buzzer.
 *
 *          gcc -Wall -Wextra -std=c99 -DKEPLER_TEST_ONLY \
 *              -DKEPLER_HAS_BUZZER=1 \
 *              -I kepler/test/stubs \
 *              kepler/test/test_task5.c kepler/test/stubs/mocks.c \
 *              kepler/power/event_queue.c kepler/storage/flash_store.c \
 *              kepler/ble/ble_manager.c kepler/ble/notif_service.c \
 *              kepler/ble/weather_service.c kepler/ble/alarm_service.c \
 *              kepler/audio/buzzer.c kepler/accel/actigraphy.c \
 *              kepler/display/sharp_lcd.c kepler/display/fonts.c \
 *              kepler/display/ui_renderer.c kepler/display/weather_icons.c \
 *              -o kepler/test/test_task5
 *
 *****************************************************************************/

#include <stdio.h>
#include <string.h>

#include "stubs/mock_state.h"
#include <ti/drivers/PWM.h>
#include "osal_snv.h"

#include "../power/event_queue.h"
#include "../storage/flash_store.h"
#include "../ble/ble_manager.h"
#include "../ble/gatt_uuids.h"
#include "../ble/notif_service.h"
#include "../audio/buzzer.h"
#include "../kepler_types.h"
#include "../kepler_config.h"

/*--- Task 1/2-convention mock globals (display stack links them) --------*/
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

/*==========================================================================*
 *  Event queue                                                              *
 *==========================================================================*/

static void test_event_queue(void)
{
    kepler_event_msg_t msg;

    event_queue_init();
    CHECK(!event_queue_pend(&msg, 0u), "empty queue pend fails");

    event_queue_post(EVT_BUTTON_1_SHORT, 11u, NULL);
    event_queue_post(EVT_BUTTON_2_SHORT, 22u, NULL);
    CHECK(event_queue_depth() == 2u, "depth 2 after two posts");

    CHECK(event_queue_pend(&msg, 0u) && msg.type == EVT_BUTTON_1_SHORT
          && msg.param == 11u, "FIFO order: first out");
    CHECK(event_queue_pend(&msg, 0u) && msg.type == EVT_BUTTON_2_SHORT
          && msg.param == 22u, "FIFO order: second out");

    /* Overflow: 15 fit (head==tail-1 means full), 16th drops              */
    for (uint32_t i = 0u; i < 20u; i++) {
        event_queue_post(EVT_STEP_UPDATE, i, NULL);
    }
    CHECK(event_queue_depth() == KEPLER_EVENT_QUEUE_SIZE - 1u,
          "ring holds size-1 events");
    CHECK(event_queue_dropped() == 5u, "drops counted");
    CHECK(event_queue_pend(&msg, 0u) && msg.param == 0u,
          "oldest preserved on overflow");
}

/*==========================================================================*
 *  Flash store                                                              *
 *==========================================================================*/

static void test_flash_store(void)
{
    mock_snv_reset();
    CHECK(flash_store_init(), "init succeeds");

    /* Settings: defaults when unset                                       */
    {
        kepler_settings_t s;
        CHECK(!flash_store_read_settings(&s), "unset settings -> false");
        CHECK(s.step_goal == KEPLER_STEP_GOAL_DEFAULT, "default step goal");
        CHECK(s.sleep_start_hour == KEPLER_SLEEP_WINDOW_START_H,
              "default sleep start");

        s.step_goal = 12000u;
        CHECK(flash_store_write_settings(&s), "settings write");
        kepler_settings_t r;
        CHECK(flash_store_read_settings(&r) && r.step_goal == 12000u,
              "settings roundtrip");
    }

    /* Step history rotation                                               */
    {
        uint16_t hist[7];
        (void)flash_store_step_day(1000u);
        (void)flash_store_step_day(2000u);
        (void)flash_store_read_steps(hist);
        CHECK(hist[0] == 2000u && hist[1] == 1000u, "history rotates");
        (void)flash_store_step_day(70000u);   /* > uint16 max              */
        (void)flash_store_read_steps(hist);
        CHECK(hist[0] == 0xFFFFu, "oversized day clamped");
    }

    /* Temp unit                                                            */
    CHECK(flash_store_read_temp_unit() == 0u, "temp unit defaults Celsius");
    (void)flash_store_write_temp_unit(1u);
    CHECK(flash_store_read_temp_unit() == 1u, "temp unit persists");

    /* Weather default                                                       */
    {
        weather_payload_t w;
        CHECK(!flash_store_read_weather(&w), "unset weather -> false");
        CHECK(w.current.condition == (uint8_t)WEATHER_UNKNOWN,
              "unset weather condition UNKNOWN");
    }
}

/*==========================================================================*
 *  BLE manager                                                              *
 *==========================================================================*/

static uint16_t s_adv_units;
static int      s_adv_starts;
static int      s_adv_stops;
static uint16_t s_notify_uuid;
static uint8_t  s_notify_val[80];
static uint16_t s_notify_len;

static void port_start_adv(uint16_t units) { s_adv_units = units; s_adv_starts++; }
static void port_stop_adv(void)            { s_adv_stops++; }
static void port_notify(uint16_t uuid, const uint8_t *v, uint16_t len)
{
    s_notify_uuid = uuid;
    s_notify_len  = (len < sizeof(s_notify_val)) ? len : 0u;
    if (s_notify_len) { memcpy(s_notify_val, v, s_notify_len); }
}

static const ble_port_t k_port = {
    port_start_adv, port_stop_adv, port_notify
};

static void test_ble_manager(void)
{
    event_queue_init();
    s_adv_starts = 0; s_adv_stops = 0;

    CHECK(ble_manager_ms_to_units(100u) == 160u, "100 ms -> 160 units");
    CHECK(ble_manager_ms_to_units(2000u) == 3200u, "2 s -> 3200 units");

    ble_manager_init(&k_port);
    CHECK(ble_manager_get_state() == BLE_STATE_ADVERTISING_FAST,
          "boot: fast advertising");
    CHECK(s_adv_units == 160u, "boot interval = fast");

    /* Window expiry -> slow                                               */
    ble_manager_test_fire_window();
    ble_manager_adv_window_expired();
    CHECK(ble_manager_get_state() == BLE_STATE_ADVERTISING_SLOW,
          "window expiry demotes to slow");
    CHECK(s_adv_units == 3200u, "slow interval applied");

    /* Connect / disconnect                                                */
    ble_manager_on_connected();
    CHECK(ble_manager_is_connected(), "connected state");
    CHECK(s_adv_stops == 1, "advertising stopped on connect");

    ble_manager_on_disconnected();
    CHECK(ble_manager_get_state() == BLE_STATE_ADVERTISING_FAST,
          "disconnect: fast advertising again");
}

/*==========================================================================*
 *  Notification service                                                     *
 *==========================================================================*/

static void test_notif_service(void)
{
    kepler_event_msg_t msg;
    notif_payload_t    wire;

    event_queue_init();
    mock_seconds_value = 1000u;

    memset(&wire, 0, sizeof(wire));
    wire.type   = 1u;                  /* call                             */
    wire.app_id = 1u;                  /* WhatsApp                         */
    strncpy(wire.sender, "Maria Garcia", sizeof(wire.sender) - 1u);
    strncpy(wire.text, "Hey are you coming tonight?",
            sizeof(wire.text) - 1u);

    CHECK(!notif_service_on_write((const uint8_t *)&wire, 10u),
          "short write rejected");
    CHECK(notif_service_on_write((const uint8_t *)&wire, sizeof(wire)),
          "valid write accepted");

    CHECK(event_queue_pend(&msg, 0u) && msg.type == EVT_BLE_NOTIFICATION,
          "EVT_BLE_NOTIFICATION posted");
    {
        const ui_notification_t *n = (const ui_notification_t *)msg.data;
        CHECK(n != NULL && n->type == 1u, "type mapped");
        CHECK(n != NULL && strcmp(n->app_name, "WhatsApp") == 0,
              "app_id mapped to name");
        CHECK(n != NULL && strcmp(n->sender, "Maria Garcia") == 0,
              "sender copied");
        CHECK(n != NULL && n->timestamp == 1000u, "timestamp stamped");
    }

    /* Time sync: little-endian u32                                        */
    {
        const uint8_t ts[4] = { 0x78, 0x56, 0x34, 0x12 };
        CHECK(notif_service_on_time_sync(ts, 4u), "time sync accepted");
        CHECK(event_queue_pend(&msg, 0u) && msg.type == EVT_TIME_SYNC
              && msg.param == 0x12345678u, "LE timestamp decoded");
    }

    /* Battery hysteresis                                                  */
    s_notify_uuid = 0u;
    notif_service_notify_battery(80u);
    CHECK(s_notify_uuid == KEPLER_CHAR_BATTERY, "first battery notified");
    s_notify_uuid = 0u;
    notif_service_notify_battery(78u);
    CHECK(s_notify_uuid == 0u, "small delta suppressed");
    notif_service_notify_battery(70u);
    CHECK(s_notify_uuid == KEPLER_CHAR_BATTERY, ">5%% delta notified");
}

/*==========================================================================*
 *  Buzzer                                                                   *
 *==========================================================================*/

static void test_buzzer(void)
{
    kepler_event_msg_t msg;

    event_queue_init();
    memset(&mock_pwm, 0, sizeof(mock_pwm));

    buzzer_init();
    CHECK(!buzzer_is_active(), "idle after init");

    buzzer_tone(3200u, 100u);
    CHECK(buzzer_is_active(), "tone active");
    CHECK(mock_pwm.started == 1 && mock_pwm.period_hz == 3200u,
          "PWM running at requested frequency");

    /* Duration expiry: clock fires -> EVT_BUZZER_TICK -> tick stops       */
    buzzer_test_fire_step();
    CHECK(event_queue_pend(&msg, 0u) && msg.type == EVT_BUZZER_TICK,
          "step clock posts EVT_BUZZER_TICK");
    buzzer_tick();
    CHECK(!buzzer_is_active(), "tone ended after duration");
    CHECK(mock_pwm.started == 0, "PWM stopped — no bleed-through");

    /* Alarm loops until stopped                                           */
    buzzer_alarm();
    CHECK(buzzer_is_active() && mock_pwm.started == 1, "alarm sounding");
    for (int i = 0; i < 10; i++) {     /* run through > one full cycle     */
        buzzer_test_fire_step();
        (void)event_queue_pend(&msg, 0u);
        buzzer_tick();
    }
    CHECK(buzzer_is_active(), "alarm still looping after 10 steps");
    buzzer_stop();
    CHECK(!buzzer_is_active() && mock_pwm.started == 0,
          "alarm silenced by stop");
}

int main(void)
{
    test_event_queue();
    test_flash_store();
    test_ble_manager();
    test_notif_service();
    test_buzzer();

    printf("test_task5: %d passed, %d failed\n", s_pass, s_fail);
    return (s_fail == 0) ? 0 : 1;
}

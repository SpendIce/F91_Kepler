/******************************************************************************
 *
 * @file  rig_main.c
 *
 * @brief ESP32 peripheral test rig for the F91 Kepler firmware.
 *
 *        Runs the actual kepler/ drivers (compiled unchanged) against
 *        real hardware: Sharp LS013B7DH03, DRV2605L + ERM motor,
 *        LIS2DW12.  Serial console menu on the default UART (115200):
 *
 *          1  LCD: render all six screens with demo data (3 s each)
 *          2  LCD: invert torture (BTN_1-long equivalent, 10 cycles)
 *          3  Haptic: init + auto-cal, then play every pattern
 *          4  Haptic: HAPTIC_CALL repeat for 5 s, then stop
 *          5  Accel: WHO_AM_I + stream steps/raw XYZ until keypress
 *          6  Accel: wrist-raise INT1 watch (lift the breadboard!)
 *          7  Watch demo: live clock + carousel auto-advance
 *
 *        VCOM note: sharp_lcd_init starts the 1 Hz EXTCOMIN toggle on an
 *        esp_timer.  Scope GPIO16 to tick the Plan Maestro checklist item.
 *
 *****************************************************************************/

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"

#include "kepler_config.h"
#include "kepler_types.h"
#include "kepler_time.h"
#include "kepler_i2c.h"
#include "display/sharp_lcd.h"
#include "display/ui_renderer.h"
#include "haptic/drv2605l.h"
#include "haptic/haptic_patterns.h"
#include "accel/lis2dw12.h"
#include "accel/pedometer.h"
#include "accel/actigraphy.h"
#include "power/event_queue.h"
#include "ble/weather_service.h"
#include "ble/alarm_service.h"
#include "ble/ble_manager.h"
#include "storage/flash_store.h"

#include "rig_config.h"
#include <ti/sysbios/hal/Seconds.h>

/*==========================================================================*
 *  Console helpers                                                          *
 *==========================================================================*/

static int rig_getchar_timeout(uint32_t ms)
{
    uint8_t c;
    int n = uart_read_bytes(UART_NUM_0, &c, 1, pdMS_TO_TICKS(ms));
    return (n == 1) ? (int)c : -1;
}

static void rig_wait_key(void)
{
    while (rig_getchar_timeout(100) < 0) {}
}

/*==========================================================================*
 *  Demo data                                                                *
 *==========================================================================*/

static void load_demo_weather(void)
{
    weather_payload_t w;
    memset(&w, 0, sizeof(w));
    w.current.temp_c       = 22;
    w.current.feels_like_c = 20;
    w.current.temp_high_c  = 26;
    w.current.temp_low_c   = 14;
    w.current.condition    = WEATHER_PARTLY_CLOUD;
    w.current.humidity_pct = 65u;
    w.current.updated_at   = Seconds_get();
    for (uint8_t i = 0; i < 8u; i++) {
        w.hourly[i].hour      = (uint8_t)((12u + i) % 24u);
        w.hourly[i].temp_c    = (int8_t)(22 - i);
        w.hourly[i].condition = (uint8_t)(i % 4u);   /* clear..rain      */
    }
    weather_service_on_write((const uint8_t *)&w, sizeof(w));
    weather_service_apply();
}

static void load_demo_alarms(void)
{
    uint8_t buf[1 + 2 * 13];
    alarm_entry_t a = { 7u, 30u, 1u, 0x1Fu, "Wake" };
    alarm_entry_t b = { 22u, 30u, 0u, 0x7Fu, "Sleep" };
    buf[0] = 2u;
    memcpy(&buf[1], &a, sizeof(a));
    memcpy(&buf[1 + 13], &b, sizeof(b));
    alarm_service_on_write(buf, sizeof(buf));
}

static void load_demo_notifications(void)
{
    ui_notification_t n;

    memset(&n, 0, sizeof(n));
    n.type = 0u;
    strcpy(n.app_name, "WhatsApp");
    strcpy(n.sender, "Maria Garcia");
    strcpy(n.text, "Hey are you coming tonight?");
    n.timestamp = Seconds_get() - 120u;
    ui_push_notification(&n);

    memset(&n, 0, sizeof(n));
    n.type = 1u;
    strcpy(n.app_name, "Phone");
    strcpy(n.sender, "+54 11 5555 1234");
    strcpy(n.text, "Missed call");
    n.timestamp = Seconds_get() - 480u;
    ui_push_notification(&n);
}

/*==========================================================================*
 *  Tests                                                                    *
 *==========================================================================*/

static void test_lcd_screens(void)
{
    struct tm t;

    printf("LCD: cycling six screens, 3 s each...\n");
    kepler_epoch_to_tm(Seconds_get(), &t);
    ui_update_time(&t);
    ui_update_steps(6284u, 8000u);
    ui_update_battery(83u);
    ui_update_ble_status(true);
    load_demo_weather();
    load_demo_alarms();
    load_demo_notifications();

    for (int s = 0; s < UI_SCREEN_COUNT; s++) {
        ui_set_screen((ui_screen_t)s);
        ui_flush();
        printf("  screen %d\n", s);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
    ui_set_screen(UI_SCREEN_MAIN);
    ui_flush();
}

static void test_lcd_invert(void)
{
    printf("LCD: invert x10 (watch for artifacts)...\n");
    for (int i = 0; i < 10; i++) {
        ui_apply_invert(true);
        vTaskDelay(pdMS_TO_TICKS(400));
        ui_apply_invert(false);
        vTaskDelay(pdMS_TO_TICKS(400));
    }
    ui_render_full();
}

static void test_haptic_patterns_all(void)
{
    static const char *const names[HAPTIC_COUNT] = {
        "CALL", "MESSAGE", "ALARM", "CALENDAR",
        "CONFIRM", "REJECT", "STEP_GOAL"
    };

    printf("Haptic: drv2605l_init (auto-cal ~1.2 s on first boot)...\n");
    if (!drv2605l_init()) {
        printf("  FAIL: device not found / cal failed (check wiring, 0x5A)\n");
        return;
    }
    haptic_init();
    printf("  init OK (cal %s)\n",
           flash_store_haptic_is_calibrated() ? "stored" : "fresh");

    for (int p = 0; p < HAPTIC_COUNT; p++) {
        if (p == HAPTIC_CALL) { continue; }   /* repeating — test 4       */
        printf("  %s\n", names[p]);
        haptic_play((haptic_pattern_t)p);
        vTaskDelay(pdMS_TO_TICKS(1200));
    }
    haptic_stop();
}

static void test_haptic_call_repeat(void)
{
    kepler_event_msg_t msg;

    printf("Haptic: CALL repeating 5 s...\n");
    haptic_play(HAPTIC_CALL);
    for (int i = 0; i < 50; i++) {
        /* Drain EVT_HAPTIC_TICK like kepler_main would.                  */
        while (event_queue_pend(&msg, 0u)) {
            if (msg.type == EVT_HAPTIC_TICK) { haptic_tick(); }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    haptic_stop();
    printf("  stopped — motor must be silent NOW\n");
}

static void test_accel_stream(void)
{
    printf("Accel: lis2dw12_init...\n");
    if (!lis2dw12_init()) {
        printf("  FAIL: WHO_AM_I mismatch (check wiring, addr 0x18)\n");
        return;
    }
    pedometer_init();
    printf("  OK. Walk around with it! Any key stops.\n");

    while (rig_getchar_timeout(0) < 0) {
        int16_t x, y, z;
        kepler_event_msg_t msg;

        pedometer_poll();
        while (event_queue_pend(&msg, 0u)) {}   /* drain step events      */
        lis2dw12_read_accel(&x, &y, &z);
        printf("  steps=%5lu  xyz=%6d %6d %6d\n",
               (unsigned long)pedometer_get_steps(), x, y, z);
        ui_update_steps(pedometer_get_steps(), 8000u);
        ui_flush();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/*--- Wrist-raise: native ESP32 ISR posts the kepler event ----------------*/

static void IRAM_ATTR int1_isr(void *arg)
{
    (void)arg;
    event_queue_post(EVT_WRIST_RAISE, 0u, NULL);
}

static void test_wrist_raise(void)
{
    kepler_event_msg_t msg;

    if (!lis2dw12_init()) { printf("  accel missing\n"); return; }

    gpio_config_t io = {
        .pin_bit_mask = 1ULL << RIG_GPIO_ACCEL_INT1,
        .mode         = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type    = GPIO_INTR_POSEDGE,
    };
    gpio_config(&io);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(RIG_GPIO_ACCEL_INT1, int1_isr, NULL);

    printf("Wrist-raise: lift/flick the board. Any key stops.\n");
    while (rig_getchar_timeout(100) < 0) {
        while (event_queue_pend(&msg, 0u)) {
            if (msg.type == EVT_WRIST_RAISE) {
                uint8_t src = lis2dw12_clear_wakeup_src();
                printf("  WRIST RAISE (WAKE_UP_SRC=0x%02x)\n", src);
                ui_set_screen(UI_SCREEN_NOTIFICATIONS);
                ui_flush();
            }
        }
    }
    gpio_isr_handler_remove(RIG_GPIO_ACCEL_INT1);
    ui_set_screen(UI_SCREEN_MAIN);
    ui_flush();
}

static void test_watch_demo(void)
{
    struct tm t;
    uint32_t last_min = 0xFFFFFFFFu;
    int      screen   = 0;

    printf("Watch demo: live clock, carousel every 5 s. Any key stops.\n");
    Seconds_set(1765465800u);   /* a pleasant 11:30 local                  */
    load_demo_weather();
    load_demo_alarms();
    load_demo_notifications();
    ui_update_battery(83u);
    ui_update_ble_status(true);

    while (rig_getchar_timeout(0) < 0) {
        uint32_t now = Seconds_get();
        if (now / 60u != last_min) {
            last_min = now / 60u;
            kepler_epoch_to_tm(now, &t);
            ui_update_time(&t);
        }
        ui_set_screen((ui_screen_t)(screen % UI_SCREEN_COUNT));
        ui_flush();
        screen++;
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

/*==========================================================================*
 *  Entry                                                                    *
 *==========================================================================*/

void app_main(void)
{
    uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0);

    printf("\n=== F91 Kepler ESP32 peripheral rig ===\n");

    event_queue_init();
    (void)flash_store_init();
    (void)kepler_i2c_open();

    printf("sharp_lcd_init: %s (VCOM 1 Hz on GPIO%d — scope it)\n",
           sharp_lcd_init() ? "OK" : "FAIL", RIG_GPIO_LCD_VCOM);
    ui_init();
    ble_manager_init(NULL);
    weather_service_init();
    alarm_service_init();
    Seconds_set(1765465800u);

    for (;;) {
        printf("\n[1]screens [2]invert [3]haptic [4]call-repeat "
               "[5]accel [6]wrist [7]demo > ");
        fflush(stdout);
        int c = -1;
        while ((c = rig_getchar_timeout(100)) < 0) {}
        printf("%c\n", c);

        switch (c) {
            case '1': test_lcd_screens();        break;
            case '2': test_lcd_invert();         break;
            case '3': test_haptic_patterns_all();break;
            case '4': test_haptic_call_repeat(); break;
            case '5': test_accel_stream();       break;
            case '6': test_wrist_raise();        break;
            case '7': test_watch_demo();         break;
            default:  printf("?\n");             break;
        }
        (void)rig_wait_key;
    }
}

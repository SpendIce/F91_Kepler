/******************************************************************************
 *
 * @file  kepler_main.c
 *
 * @brief Top-level init and central event loop (Task 5/6).
 *
 *        Architecture (Plan Maestro §7, spec 05):
 *          - every interrupt source posts to the central event queue;
 *          - this task pends on the queue (CPU in STANDBY while blocked)
 *            and dispatches in kepler_handle_event();
 *          - all peripheral I/O (I2C, SPI, PWM, flash) happens here, in
 *            task context, never in Swi/Hwi callbacks.
 *
 *        BLE glue: the legacy CCS application (Firmware/f91_kepler_app)
 *        calls kepler_main_set_ble_port() before starting this task and
 *        routes GATT writes into kepler_ble_on_char_write().  With a NULL
 *        port (Phase 0 launchpad), all BLE notify calls are no-ops.
 *
 *****************************************************************************/

#include "kepler_config.h"
#include "kepler_types.h"
#include "kepler_i2c.h"

#include "display/sharp_lcd.h"
#include "display/ui_renderer.h"
#include "input/buttons.h"
#include "input/time_set.h"
#include "haptic/drv2605l.h"
#include "haptic/haptic_patterns.h"
#include "accel/lis2dw12.h"
#include "accel/pedometer.h"
#include "accel/actigraphy.h"
#include "accel/wrist_raise.h"
#include "power/event_queue.h"
#include "power/power_manager.h"
#include "ble/ble_manager.h"
#include "ble/gatt_uuids.h"
#include "ble/notif_service.h"
#include "ble/weather_service.h"
#include "ble/alarm_service.h"
#include "ble/locator_service.h"
#include "screens/stopwatch.h"
#include "storage/flash_store.h"
#include "audio/buzzer.h"

#include <stddef.h>
#include <string.h>
#include <time.h>

#include <ti/sysbios/knl/Clock.h>
#include <ti/sysbios/hal/Seconds.h>

#define MS_TO_TICKS(ms)  ((uint32_t)(ms) * 1000u / Clock_tickPeriod)

/*==========================================================================*
 *  Module state                                                            *
 *==========================================================================*/

static const ble_port_t *s_ble_port;

static kepler_settings_t s_settings;

static Clock_Struct s_minute_clk;          /* aligned 60 s scheduler        */
static Clock_Struct s_invert_clk;          /* 3 s display invert restore    */
static Clock_Struct s_banner_clk;          /* 2 s notification banner       */

static uint8_t s_last_hour = 0xFFu;        /* midnight / chime edge detect  */

/*==========================================================================*
 *  Clock callbacks (Swi context — post only)                               *
 *==========================================================================*/

static void minute_swi(UArg arg)
{
    (void)arg;
    event_queue_post(EVT_MINUTE_TICK, 0u, NULL);
}

static void invert_swi(UArg arg)
{
    (void)arg;
    event_queue_post(EVT_DISPLAY_INVERT_RESTORE, 0u, NULL);
}

static void banner_swi(UArg arg)
{
    (void)arg;
    event_queue_post(EVT_BANNER_EXPIRE, 0u, NULL);
}

/* Wake hook shared by buttons.c and time_set.c Swi's.                     */
static void input_wake_hook(void)
{
    event_queue_post(EVT_INPUT_PUMP, 0u, NULL);
}

/*==========================================================================*
 *  Time helpers (no libc localtime — no TZ database on target)             *
 *==========================================================================*/

/* Civil-date algorithm (Howard Hinnant's civil_from_days).                *
 * Seconds_get() holds local wall-clock time synced from the phone.        */
static void epoch_to_tm(uint32_t epoch, struct tm *t)
{
    uint32_t days = epoch / 86400u;
    uint32_t rem  = epoch % 86400u;

    memset(t, 0, sizeof(*t));
    t->tm_hour = (int)(rem / 3600u);
    t->tm_min  = (int)((rem % 3600u) / 60u);
    t->tm_sec  = (int)(rem % 60u);
    t->tm_wday = (int)((days + 4u) % 7u);      /* 1970-01-01 was Thursday  */

    {
        int64_t  z   = (int64_t)days + 719468;
        int64_t  era = (z >= 0 ? z : z - 146096) / 146097;
        uint32_t doe = (uint32_t)(z - era * 146097);
        uint32_t yoe = (doe - doe / 1460u + doe / 36524u - doe / 146096u)
                       / 365u;
        int64_t  y   = (int64_t)yoe + era * 400;
        uint32_t doy = doe - (365u * yoe + yoe / 4u - yoe / 100u);
        uint32_t mp  = (5u * doy + 2u) / 153u;
        uint32_t d   = doy - (153u * mp + 2u) / 5u + 1u;
        uint32_t m   = mp < 10u ? mp + 3u : mp - 9u;

        if (m <= 2u) { y += 1; }
        t->tm_year = (int)(y - 1900);
        t->tm_mon  = (int)(m - 1u);
        t->tm_mday = (int)d;
    }
}

static void refresh_time_display(void)
{
    struct tm t;
    epoch_to_tm(Seconds_get(), &t);
    if (!time_set_is_active()) {       /* time-set owns the digits         */
        ui_update_time(&t);
    }
}

/*==========================================================================*
 *  Button event translation                                                *
 *==========================================================================*/

/* Called from buttons_process() in task context.  Time-set gets first     *
 * claim on every button event (spec 02).                                  */
static void button_event_handler(button_id_t btn, button_event_t evt)
{
    static const kepler_event_t map[BTN_COUNT][2] = {
        /*          BTN_EVT_SHORT       BTN_EVT_LONG                       */
        [BTN_1] = { EVT_BUTTON_1_SHORT, EVT_BUTTON_1_LONG },
        [BTN_2] = { EVT_BUTTON_2_SHORT, EVT_BUTTON_2_LONG },
        [BTN_3] = { EVT_BUTTON_3_SHORT, EVT_BUTTON_3_LONG },
    };

    if (time_set_handle_button(btn, evt)) {
        activity_timer_reset();
        return;                        /* consumed by time-set             */
    }
    if (btn >= BTN_COUNT) { return; }

    event_queue_post(map[btn][(evt == BTN_EVT_LONG) ? 1 : 0], 0u, NULL);
}

/*==========================================================================*
 *  BLE glue (called by the legacy CCS application)                         *
 *==========================================================================*/

void kepler_main_set_ble_port(const ble_port_t *port)
{
    s_ble_port = port;
}

/* Route a GATT characteristic write into the owning service.  Returns     *
 * false for unknown characteristics or malformed payloads.                */
bool kepler_ble_on_char_write(uint16_t char_uuid,
                              const uint8_t *data, uint16_t len)
{
    switch (char_uuid) {
        case KEPLER_CHAR_NOTIF:         return notif_service_on_write(data, len);
        case KEPLER_CHAR_TIME_SYNC:     return notif_service_on_time_sync(data, len);
        case KEPLER_CHAR_SETTINGS:      return notif_service_on_settings(data, len);
        case KEPLER_CHAR_WEATHER:       return weather_service_on_write(data, len);
        case KEPLER_CHAR_LOCATOR:       return locator_service_on_write(data, len);
        case KEPLER_CHAR_ALARMS:        return alarm_service_on_write(data, len);
        case KEPLER_CHAR_ALARM_TRIGGER: return alarm_service_on_trigger(data, len);
        default:                        return false;
    }
}

/* Connection callbacks for the legacy application's GAP role handler.     */
void kepler_ble_on_connected(void)
{
    event_queue_post(EVT_BLE_CONNECTED, 0u, NULL);
}

void kepler_ble_on_disconnected(void)
{
    event_queue_post(EVT_BLE_DISCONNECTED, 0u, NULL);
}

/*==========================================================================*
 *  Minute scheduler                                                        *
 *==========================================================================*/

static void handle_minute_tick(void)
{
    struct tm t;
    epoch_to_tm(Seconds_get(), &t);

    refresh_time_display();
    pedometer_poll();

    /* Battery: cheap read, push UI + BLE (>5 % hysteresis inside).        */
    {
        uint8_t pct = power_manager_battery_pct();
        ui_update_battery(pct);
        notif_service_notify_battery(pct);
        if (pct <= 10u) {
            event_queue_post(EVT_BATTERY_LOW, pct, NULL);
        }
    }

    /* Settings may have been rewritten via 0xFF05 — refresh the cache.    */
    (void)flash_store_read_settings(&s_settings);

    if ((uint8_t)t.tm_hour != s_last_hour) {
        uint8_t hour = (uint8_t)t.tm_hour;

        if (hour == 0u && s_last_hour != 0xFFu) {
            event_queue_post(EVT_MIDNIGHT_RESET, 0u, NULL);
        }
        if (hour == s_settings.sleep_end_hour) {
            actigraphy_night_close();
        }
#if KEPLER_HAS_BUZZER
        buzzer_hour_chime();
#endif
        s_last_hour = hour;
    }
}

/*==========================================================================*
 *  Central event dispatch (spec 05 main event loop + spec 06 additions)   *
 *==========================================================================*/

static void kepler_handle_event(const kepler_event_msg_t *msg)
{
    switch (msg->type) {

    /* ── Input pump ─────────────────────────────────────────────────── */
    case EVT_INPUT_PUMP:
        (void)buttons_process();       /* dispatches button_event_handler  */
        time_set_process();
        break;

    /* ── Screen navigation (universal) ──────────────────────────────── */
    case EVT_BUTTON_3_SHORT:
        ui_next_screen();
        activity_timer_reset();
        break;

    case EVT_BUTTON_3_LONG:
        ui_goto_main();
        activity_timer_reset();
        break;

    case EVT_SCREEN_TIMEOUT:
        if (!stopwatch_is_running()) {
            ui_set_screen(UI_SCREEN_MAIN);
        }
        break;

    /* ── BTN_1: per-screen action ───────────────────────────────────── */
    case EVT_BUTTON_1_SHORT:
        switch (ui_get_screen()) {
            case UI_SCREEN_NOTIFICATIONS:
                notif_dismiss_selected();
                haptic_play(HAPTIC_CONFIRM);
                break;
            case UI_SCREEN_PHONE_LOCATOR:
                event_queue_post(locator_service_is_ringing()
                                     ? EVT_PHONE_LOCATOR_STOP
                                     : EVT_PHONE_LOCATOR_START, 0u, NULL);
                break;
            case UI_SCREEN_STOPWATCH:
                if (stopwatch_is_running()) { stopwatch_stop();  }
                else                        { stopwatch_start(); }
                ui_update_stopwatch(stopwatch_elapsed_cs(),
                                    stopwatch_is_running(), NULL, 0u);
                break;
            case UI_SCREEN_ALARMS:
                if (alarms_get_triggered() != ALARM_TRIGGER_NONE) {
                    event_queue_post(EVT_ALARM_DISMISS, 0u, NULL);
                } else {
                    alarms_toggle_selected();
                    ui_update_alarms();
                }
                break;
            case UI_SCREEN_WEATHER:
                event_queue_post(EVT_WEATHER_REFRESH_REQ, 0u, NULL);
                break;
            default:
                break;
        }
        activity_timer_reset();
        break;

    /* ── BTN_1 long: display invert ─────────────────────────────────── */
    case EVT_BUTTON_1_LONG:
        ui_apply_invert(true);
        Clock_setTimeout(Clock_handle(&s_invert_clk),
                         MS_TO_TICKS(KEPLER_INVERT_DISPLAY_DURATION_MS));
        Clock_start(Clock_handle(&s_invert_clk));
        activity_timer_reset();
        break;

    case EVT_DISPLAY_INVERT_RESTORE:
        ui_apply_invert(false);
        ui_render_full();
        break;

    /* ── BTN_2: per-screen secondary action ─────────────────────────── */
    case EVT_BUTTON_2_SHORT:
        switch (ui_get_screen()) {
            case UI_SCREEN_NOTIFICATIONS:
                notif_scroll_next();
                break;
            case UI_SCREEN_STOPWATCH: {
                uint32_t laps[3];
                if (stopwatch_is_running()) { stopwatch_lap();   }
                else                        { stopwatch_reset(); }
                ui_update_stopwatch(stopwatch_elapsed_cs(),
                                    stopwatch_is_running(), laps,
                                    stopwatch_get_laps(laps, 3u));
                break;
            }
            case UI_SCREEN_ALARMS:
                alarms_scroll_next();
                ui_update_alarms();
                break;
            case UI_SCREEN_WEATHER:
                weather_service_toggle_units();
                break;
            default:
                break;
        }
        activity_timer_reset();
        break;

    /* BTN_2 long is consumed by time_set_handle_button() before it       *
     * reaches the queue; nothing to do here.                              */
    case EVT_BUTTON_2_LONG:
        break;

    /* ── Wrist raise ────────────────────────────────────────────────── */
    case EVT_WRIST_RAISE:
        (void)lis2dw12_clear_wakeup_src();
        actigraphy_on_movement();
        if (!actigraphy_in_sleep_window()) {
            ui_set_screen(UI_SCREEN_NOTIFICATIONS);
            activity_timer_reset();
        }
        break;

    /* ── BLE / data ─────────────────────────────────────────────────── */
    case EVT_BLE_NOTIFICATION: {
        const ui_notification_t *n = (const ui_notification_t *)msg->data;
        if (n == NULL) { break; }

        haptic_play((n->type == 1u) ? HAPTIC_CALL : HAPTIC_MESSAGE);
#if KEPLER_HAS_BUZZER
        buzzer_notify();
#endif
        ui_push_notification(n);
        if (ui_get_screen() != UI_SCREEN_NOTIFICATIONS) {
            ui_show_notif_banner(n);
            Clock_setTimeout(Clock_handle(&s_banner_clk),
                             MS_TO_TICKS(2000u));
            Clock_start(Clock_handle(&s_banner_clk));
        }
        break;
    }

    case EVT_BANNER_EXPIRE:
        ui_banner_expire();
        break;

    case EVT_WEATHER_UPDATE:
        weather_service_apply();
        break;

    case EVT_WEATHER_REFRESH_REQ:
        weather_service_request_refresh();
        break;

    case EVT_PHONE_LOCATOR_START:
        locator_service_start();
        break;

    case EVT_PHONE_LOCATOR_STOP:
        locator_service_stop();
        break;

    case EVT_PHONE_LOCATOR_ACK:
        /* App confirmed it is ringing — display already shows RINGING.   */
        break;

    case EVT_STOPWATCH_TICK:
        if (ui_get_screen() == UI_SCREEN_STOPWATCH) {
            uint32_t laps[3];
            ui_update_stopwatch(msg->param, stopwatch_is_running(), laps,
                                stopwatch_get_laps(laps, 3u));
        }
        break;

    case EVT_ALARMS_UPDATE:
        ui_update_alarms();
        break;

    case EVT_ALARM_TRIGGER:
        haptic_play(HAPTIC_ALARM);
#if KEPLER_HAS_BUZZER
        buzzer_alarm();
#endif
        ui_set_screen(UI_SCREEN_ALARMS);
        alarms_show_triggered((uint8_t)msg->param);
        ui_update_alarms();
        break;

    case EVT_ALARM_DISMISS:
        alarms_clear_triggered();
        haptic_stop();
        buzzer_stop();
        ui_update_alarms();
        break;

    case EVT_STEP_UPDATE:
        ui_update_steps(msg->param, s_settings.step_goal);
        notif_service_notify_steps();
        if (msg->param == s_settings.step_goal) {
            haptic_play(HAPTIC_STEP_GOAL);
        }
        break;

    case EVT_MIDNIGHT_RESET:
        pedometer_midnight_reset();
        break;

    case EVT_TIME_SYNC:
        Seconds_set(msg->param);
        s_last_hour = 0xFFu;           /* re-arm hour edge detection       */
        refresh_time_display();
        break;

    case EVT_BLE_CONNECTED:
        ble_manager_on_connected();
        power_manager_set_state(POWER_BLE_CONNECTED);
        ui_update_ble_status(true);
        break;

    case EVT_BLE_DISCONNECTED:
        ble_manager_on_disconnected();
        power_manager_set_state(POWER_IDLE);
        ui_update_ble_status(false);
        if (locator_service_is_ringing()) {
            locator_service_stop();    /* link gone — ring state is stale  */
        }
        break;

    case EVT_BLE_ADV_WINDOW:
        ble_manager_adv_window_expired();
        break;

    /* ── Periodic / housekeeping ────────────────────────────────────── */
    case EVT_MINUTE_TICK:
        handle_minute_tick();
        break;

    case EVT_HAPTIC_TICK:
        haptic_tick();
        break;

    case EVT_BUZZER_TICK:
        buzzer_tick();
        break;

    case EVT_BATTERY_LOW:
        /* Reserved: future low-battery glyph on the status row.          */
        break;

    default:
        break;
    }

    /* After any event, push the dirty rows out (spec 05).                 */
    ui_flush();
}

/*==========================================================================*
 *  Init + event loop                                                       *
 *==========================================================================*/

static void kepler_init_clocks(void)
{
    Clock_Params p;
    uint32_t     sec_into_min = Seconds_get() % 60u;

    /* Minute scheduler: first fire at the next minute boundary, then      *
     * every 60 s.                                                          */
    Clock_Params_init(&p);
    p.period    = MS_TO_TICKS(60000u);
    p.startFlag = TRUE;
    Clock_construct(&s_minute_clk, minute_swi,
                    MS_TO_TICKS((60u - sec_into_min) * 1000u), &p);

    Clock_Params_init(&p);
    Clock_construct(&s_invert_clk, invert_swi,
                    MS_TO_TICKS(KEPLER_INVERT_DISPLAY_DURATION_MS), &p);

    Clock_Params_init(&p);
    Clock_construct(&s_banner_clk, banner_swi, MS_TO_TICKS(2000u), &p);
}

void kepler_main_task(void)
{
    /* --- Core infrastructure first ------------------------------------ */
    event_queue_init();
    (void)flash_store_init();
    (void)flash_store_read_settings(&s_settings);
    power_manager_init();

    /* --- Display ------------------------------------------------------- */
    (void)sharp_lcd_init();
    ui_init();

    /* --- Input ----------------------------------------------------------- */
    buttons_init(button_event_handler);
    buttons_set_wake_cb(input_wake_hook);
    time_set_init();
    time_set_set_wake_cb(input_wake_hook);

    /* --- I2C peripherals (stubs compile away when hardware absent) ----- */
    (void)kepler_i2c_open();
    (void)drv2605l_init();
    haptic_init();
    (void)lis2dw12_init();
    pedometer_init();
    actigraphy_init();
    actigraphy_set_window(s_settings.sleep_start_hour,
                          s_settings.sleep_end_hour);
    (void)wrist_raise_init();

    /* --- Services -------------------------------------------------------- */
    ble_manager_init(s_ble_port);
    weather_service_init();
    alarm_service_init();
    locator_service_init();
    stopwatch_init();
    buzzer_init();

    kepler_init_clocks();
    activity_timer_reset();

    /* --- First frame ------------------------------------------------------ */
    refresh_time_display();
    ui_update_steps(pedometer_get_steps(), s_settings.step_goal);
    ui_update_battery(power_manager_battery_pct());
    ui_render_full();

    /* --- Event loop (never returns) --------------------------------------- */
    for (;;) {
        kepler_event_msg_t msg;

        power_manager_set_state(ble_manager_is_connected()
                                    ? POWER_BLE_CONNECTED : POWER_IDLE);

        if (event_queue_pend(&msg, EVENT_QUEUE_WAIT_FOREVER)) {
            power_manager_set_state(POWER_ACTIVE);
            kepler_handle_event(&msg);
        }
    }
}

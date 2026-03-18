# Tasks 5–8 — Power Manager, BLE Service, Flash Storage & Buzzer

---

# Task 5 — Power Manager & Event Loop

## Objective

Refactor the top-level application loop into a clean event-driven architecture
with a well-defined power state machine. All peripheral activity is triggered
by events; the CPU sleeps between events.

---

## Power state machine

```
         ┌──────────────────────────────────────┐
         │              IDLE                    │
         │  CPU: STANDBY (~1µA)                 │
         │  LCD: always-on (Sharp, ~5µA)        │
         │  BLE: advertising at slow interval   │
         │  Accel: low-power mode (6µA)         │
         └──────┬───────────────────────────────┘
                │ Any event from queue
                ▼
         ┌──────────────────────────────────────┐
         │           ACTIVE                     │
         │  CPU: running (~4mA)                 │
         │  Handles event, updates display,     │
         │  triggers haptic/BLE as needed       │
         └──────┬───────────────────────────────┘
                │ Event queue empty
                ▼
         ┌──────────────────────────────────────┐
         │         BLE_CONNECTED                │
         │  CPU: STANDBY between conn events    │
         │  BLE: connected, interval 500ms      │
         │  Entered on EVT_BLE_CONNECTED        │
         │  Exited on EVT_BLE_DISCONNECTED      │
         └──────────────────────────────────────┘
```

---

## Main event loop

```c
// kepler_main.c

void kepler_main_task(void) {
    // Initialise all modules
    sharp_lcd_init();
    ui_init();
    buttons_init(button_event_handler);
    // I2C-dependent modules initialised after I2C bus opens:
#if KEPLER_HAS_DRV2605L
    drv2605l_init();
#endif
#if KEPLER_HAS_LIS2DW12
    lis2dw12_init();
    pedometer_init();
    actigraphy_init();
    wrist_raise_init();
#endif
    flash_store_init();
    ble_manager_init();
    buzzer_init();

    // Initial display render
    ui_render_full();
    ui_flush();

    // Event loop — never exits
    while (1) {
        kepler_event_msg_t msg;

        // Block until event arrives (CPU sleeps in Power_STANDBY during pend)
        if (event_queue_pend(&msg, BIOS_WAIT_FOREVER)) {
            kepler_handle_event(&msg);
        }
    }
}

void kepler_handle_event(const kepler_event_msg_t *msg) {
    switch (msg->type) {

        // ── Screen navigation (universal) ──────────────────────────────
        case EVT_BUTTON_3_SHORT:
            ui_screen_advance();           // cycles MAIN→WEATHER→...→ALARMS→MAIN
            activity_timer_reset();
            break;

        case EVT_BUTTON_3_LONG:
            ui_set_screen(UI_SCREEN_MAIN);
            activity_timer_reset();
            break;

        case EVT_SCREEN_TIMEOUT:
            if (!stopwatch_is_running()) {
                ui_set_screen(UI_SCREEN_MAIN);
            }
            break;

        // ── BTN_1: per-screen action + long = invert ───────────────────
        case EVT_BUTTON_1_SHORT:
            switch (ui_get_screen()) {
                case UI_SCREEN_NOTIFICATIONS:
                    notif_dismiss_selected();
                    break;
                case UI_SCREEN_PHONE_LOCATOR:
                    if (s_phone_locating) {
                        event_queue_post(EVT_PHONE_LOCATOR_STOP, 0, NULL);
                    } else {
                        event_queue_post(EVT_PHONE_LOCATOR_START, 0, NULL);
                    }
                    break;
                case UI_SCREEN_STOPWATCH:
                    if (stopwatch_is_running()) stopwatch_stop();
                    else                        stopwatch_start();
                    break;
                case UI_SCREEN_ALARMS:
                    alarms_toggle_selected();
                    break;
                case UI_SCREEN_WEATHER:
                    event_queue_post(EVT_WEATHER_REFRESH_REQ, 0, NULL);
                    break;
                default:
                    break;
            }
            activity_timer_reset();
            break;

        case EVT_BUTTON_1_LONG:
            ui_apply_invert(true);
            Clock_setTimeout(s_invert_restore_clock,
                             KEPLER_INVERT_DISPLAY_DURATION_MS * 1000 / Clock_tickPeriod);
            Clock_start(s_invert_restore_clock);
            break;

        case EVT_DISPLAY_INVERT_RESTORE:
            ui_apply_invert(false);
            ui_render_full();
            break;

        // ── BTN_2: per-screen secondary action + long = time-set ───────
        case EVT_BUTTON_2_SHORT:
            switch (ui_get_screen()) {
                case UI_SCREEN_NOTIFICATIONS:
                    notif_scroll_next();
                    break;
                case UI_SCREEN_STOPWATCH:
                    if (stopwatch_is_running()) stopwatch_lap();
                    else                        stopwatch_reset();
                    break;
                case UI_SCREEN_ALARMS:
                    alarms_scroll_next();
                    break;
                case UI_SCREEN_WEATHER:
                    ui_weather_toggle_units();
                    break;
                default:
                    break;
            }
            activity_timer_reset();
            break;

        case EVT_BUTTON_2_LONG:
            time_set_enter();
            break;

        // ── Wrist raise ────────────────────────────────────────────────
        case EVT_WRIST_RAISE:
            lis2dw12_clear_wakeup_src();
            actigraphy_on_movement();
            if (!actigraphy_in_sleep_window()) {
                ui_set_screen(UI_SCREEN_NOTIFICATIONS);
                activity_timer_reset();
            }
            break;

        // ── BLE / data ─────────────────────────────────────────────────
        case EVT_BLE_NOTIFICATION:
            haptic_play(((notif_payload_t *)msg->data)->type == 1
                        ? HAPTIC_CALL : HAPTIC_MESSAGE);
            notif_ring_push((notif_payload_t *)msg->data);
            ui_show_notification_banner((notif_payload_t *)msg->data);  // 2s overlay
            break;

        case EVT_WEATHER_UPDATE:
            memcpy(&g_weather, msg->data, sizeof(weather_payload_t));
            g_weather_valid = true;
            flash_store_write_weather(&g_weather);
            break;

        case EVT_WEATHER_REFRESH_REQ:
            ble_notify_weather_refresh();
            break;

        case EVT_PHONE_LOCATOR_START:
            s_phone_locating = true;
            ble_write_phone_locator(0x01);
            Clock_start(s_locator_auto_stop_clock);
            break;

        case EVT_PHONE_LOCATOR_STOP:
            s_phone_locating = false;
            ble_write_phone_locator(0x00);
            Clock_stop(s_locator_auto_stop_clock);
            break;

        case EVT_STOPWATCH_TICK:
            if (ui_get_screen() == UI_SCREEN_STOPWATCH) {
                ui_update_stopwatch(msg->param);
            }
            break;

        case EVT_ALARMS_UPDATE:
            memcpy(&g_alarms, msg->data, sizeof(alarms_payload_t));
            flash_store_write_alarms(&g_alarms);
            break;

        case EVT_ALARM_TRIGGER:
            haptic_play(HAPTIC_ALARM);
            ui_set_screen(UI_SCREEN_ALARMS);
            alarms_show_triggered(msg->param);
            break;

        case EVT_ALARM_DISMISS:
            alarms_clear_triggered();
            break;

        case EVT_STEP_UPDATE:
            ui_update_steps(msg->param, s_step_goal);
            break;

        case EVT_MIDNIGHT_RESET:
            pedometer_midnight_reset();
            actigraphy_night_close();
            break;

        case EVT_TIME_SYNC:
            Seconds_set(msg->param);
            break;

        case EVT_BLE_CONNECTED:
            ble_manager_on_connected();
            break;

        case EVT_BLE_DISCONNECTED:
            ble_manager_on_disconnected();
            break;

        default:
            break;
    }

    // After any event, flush changed display regions
    ui_flush();
}
```

---

## BLE advertising interval management

```c
// On boot: fast advertising for KEPLER_BLE_ADV_FAST_DURATION_MS, then slow
// On disconnect: fast for 30s, then slow
// On button long-press: toggle advertising off/on

void ble_manager_start_advertising(bool fast) {
    uint16_t interval_min = fast
        ? (KEPLER_BLE_ADV_INTERVAL_FAST_MS * 8 / 5)   // convert ms to 0.625ms units
        : (KEPLER_BLE_ADV_INTERVAL_SLOW_MS * 8 / 5);
    // Call GAPRole_SetParameter to update advertising interval
    // Then GAP_UpdateAdvertisingData or restart advertising
}
```

---

# Task 6 — BLE Notification Service Update

## Objective

Update the existing Kepler BLE GATT service to:
- Accept structured notification payloads from the companion app
- Expose step count and battery level as readable/notifiable characteristics
- Expose settings write characteristic
- Maintain backward compatibility with existing nRF Connect usage for testing

---

## GATT service definition

Custom service UUID: `0x0000FFFF-0000-1000-8000-00805F9B34FB` (shorthand 0xFFFF)

```c
// 0xFF01 — Notification relay (Write Without Response, app → watch)
// Format: notif_payload_t (64 bytes)
// On write: push to ring buffer, post EVT_BLE_NOTIFICATION

// 0xFF02 — Time sync (Write, app → watch)
// Format: uint32_t Unix timestamp (4 bytes, little-endian)
// On write: post EVT_TIME_SYNC

// 0xFF03 — Step data (Read + Notify, watch → app)
// Format: uint16_t[7] — today through 6 days ago (14 bytes)
// Notify on EVT_STEP_UPDATE when CCCD enabled

// 0xFF04 — Sleep data (Read, watch → app)
// Format: actigraphy_night_t (19 bytes)
// Updated at end of sleep window

// 0xFF05 — Settings (Write, app → watch)
// Format: kepler_settings_t (16 bytes)

// 0xFF06 — Weather data (Write Without Response, app → watch)
// Format: weather_payload_t (36 bytes)
// On write: update g_weather, write flash, post EVT_WEATHER_UPDATE

// 0xFF07 — Phone locator command (Write + Notify)
// Watch writes 0x01 (start) or 0x00 (stop) → app receives and plays/stops sound
// App writes back 0x01 (acknowledged) → watch receives EVT_PHONE_LOCATOR_ACK

// 0xFF08 — Alarms data (Write Without Response, app → watch)
// Format: alarms_payload_t (66 bytes)
// On write: update g_alarms, write flash, post EVT_ALARMS_UPDATE

// 0xFF09 — Alarm trigger (Write Without Response, app → watch)
// Format: uint8_t alarm index (0–4), or 0xFF = dismiss all
// On write: post EVT_ALARM_TRIGGER

// 0xFF0A — Weather refresh request (Notify, watch → app)
// Watch writes 0x01 notification → app fetches new weather → writes 0xFF06
// Triggered by BTN_1 short on WEATHER screen

// 0x2A19 — Battery level (standard BLE, Read + Notify)
// Format: uint8_t percent 0–100
// Notify when level changes by >5%
```

---

## Notification payload parsing

```c
void notif_service_on_write(const uint8_t *data, uint16_t len) {
    if (len < sizeof(notif_payload_t)) return;

    notif_payload_t *payload = (notif_payload_t *)data;

    // Copy to static storage (data pointer may be invalidated after return)
    static notif_payload_t s_last_notif;
    memcpy(&s_last_notif, payload, sizeof(notif_payload_t));

    // Map to UI notification
    ui_notification_t ui_notif;
    ui_notif.type = payload->type;
    strncpy(ui_notif.sender, payload->sender, sizeof(ui_notif.sender) - 1);
    strncpy(ui_notif.text,   payload->text,   sizeof(ui_notif.text)   - 1);
    ui_notif.timestamp = Seconds_get();

    event_queue_post(EVT_BLE_NOTIFICATION, payload->type, &ui_notif);
}
```

---

# Task 7 — Flash Storage

## Objective

Implement persistent storage using the CC2640R2F's NV (Non-Volatile) flash
area via the TI NVOCMP driver or SimpleNV driver from the SDK.

---

## Flash layout (NV items)

TI's NVOCMP uses a key-value store model. Define unique item IDs:

```c
#define NV_ID_STEP_HISTORY      0x0001  // uint16_t[7] — last 7 days steps
#define NV_ID_SLEEP_LAST_NIGHT  0x0002  // actigraphy_night_t
#define NV_ID_SETTINGS          0x0003  // kepler_settings_t
#define NV_ID_HAPTIC_CAL        0x0004  // DRV2605L calibration (COMP + BEMF bytes)
#define NV_ID_HAPTIC_CALIBRATED 0x0005  // uint8_t flag: 1 if calibrated
#define NV_ID_STEP_TODAY        0x0006  // uint32_t current day steps (crash recovery)
#define NV_ID_WEATHER           0x0007  // weather_payload_t (36 bytes) — last known
#define NV_ID_ALARMS            0x0008  // alarms_payload_t (66 bytes)
#define NV_ID_TEMP_UNIT         0x0009  // uint8_t: 0=Celsius, 1=Fahrenheit
```

---

## Settings struct

```c
typedef struct {
    uint16_t step_goal;             // default 8000
    uint8_t  sleep_start_hour;      // default 22
    uint8_t  sleep_end_hour;        // default 8
    uint8_t  haptic_call_en;        // default 1
    uint8_t  haptic_message_en;     // default 1
    uint8_t  haptic_alarm_en;       // default 1
    uint8_t  display_mode_default;  // 0=ambient, unused for now
    uint8_t  reserved[8];           // pad to 16 bytes for future use
} kepler_settings_t;                // 16 bytes
```

---

## flash_store.h — public interface

```c
bool flash_store_init(void);

// Step history (7 days, index 0 = today, 6 = 6 days ago)
bool flash_store_step_day(uint32_t steps);
bool flash_store_read_steps(uint16_t out[7]);

// Sleep actigraphy
bool flash_store_write_sleep(const actigraphy_night_t *night);
bool flash_store_read_sleep(actigraphy_night_t *out);

// Settings
bool flash_store_write_settings(const kepler_settings_t *s);
bool flash_store_read_settings(kepler_settings_t *out);   // returns defaults if not set

// Haptic calibration
bool flash_store_write_haptic_cal(uint8_t comp, uint8_t bemf);
bool flash_store_read_haptic_cal(uint8_t *comp, uint8_t *bemf);
bool flash_store_haptic_is_calibrated(void);

// Weather (last known — shown on boot before phone connects)
bool flash_store_write_weather(const weather_payload_t *w);
bool flash_store_read_weather(weather_payload_t *out);

// Alarms
bool flash_store_write_alarms(const alarms_payload_t *a);
bool flash_store_read_alarms(alarms_payload_t *out);

// Temperature unit preference
bool flash_store_write_temp_unit(uint8_t unit);  // 0=Celsius, 1=Fahrenheit
uint8_t flash_store_read_temp_unit(void);        // returns 0 if not set
```

---

# Task 8 — Buzzer / Sound

## Objective

Drive the piezo buzzer already connected on the Kepler PCB using CC2640R2F
PWM output for simple tones and alarm sequences.

---

## Hardware context

The Kepler PCB has traces routed to a piezo buzzer element. The buzzer is
driven directly from a GPIO or via a small NPN transistor — verify from schematic.

If driven via transistor: PWM GPIO drives transistor base; buzzer across collector.
If driven directly from GPIO: use high-drive mode GPIO (CC2640R2F supports 4/8mA drive).

Piezo resonant frequency: typically 2.7–4.0kHz for watch-sized buzzers.
Experiment to find the loudest frequency for the specific component.

---

## buzzer.h

```c
// Play a tone at given frequency (Hz) for duration_ms milliseconds
// Non-blocking: uses timer interrupt internally
void buzzer_tone(uint16_t freq_hz, uint16_t duration_ms);

// Stop immediately
void buzzer_stop(void);

// Named sequences
void buzzer_alarm(void);       // repeating alarm pattern
void buzzer_hour_chime(void);  // single short beep on the hour
void buzzer_notify(void);      // short two-tone notification beep
```

---

## PWM implementation

Use TI PWM driver (wraps Timer peripheral):

```c
PWM_Params pwm_params;
PWM_Params_init(&pwm_params);
pwm_params.dutyUnits       = PWM_DUTY_FRACTION;
pwm_params.dutyValue       = PWM_DUTY_FRACTION_MAX / 2;  // 50% duty = loudest
pwm_params.periodUnits     = PWM_PERIOD_HZ;
pwm_params.periodValue     = freq_hz;
s_pwm_handle = PWM_open(KEPLER_BUZZER_PWM_INDEX, &pwm_params);
PWM_start(s_pwm_handle);

// Stop after duration_ms using a Clock callback:
Clock_setTimeout(s_buzz_timer, duration_ms * 1000 / Clock_tickPeriod);
Clock_start(s_buzz_timer);
// Timer callback calls PWM_stop() and PWM_close()
```

---

## Acceptance criteria for Tasks 5–8

### Task 5 (Power)
- [ ] CPU enters STANDBY between events (verify via current measurement)
- [ ] All events dispatched correctly to handlers with no missed events
- [ ] Detail mode timeout fires reliably at configured interval
- [ ] BLE advertising interval switches from fast to slow after 30s

### Task 6 (BLE)
- [ ] Notification payload written from nRF Connect appears on display
- [ ] Time sync characteristic updates RTC correctly
- [ ] Step count characteristic readable and reflects current count
- [ ] Settings write updates step goal and persists across power cycle

### Task 7 (Flash)
- [ ] Step history persists across resets
- [ ] Settings persist across resets
- [ ] Haptic calibration loaded on boot (skips auto-cal if flag set)
- [ ] No flash corruption after 100 write cycles (NV wear levelling handles this)

### Task 8 (Buzzer)
- [ ] Audible tone produced at configured frequency
- [ ] Hour chime fires at 00 minutes of each hour
- [ ] Alarm pattern plays and stops correctly
- [ ] No PWM signal bleeds through after buzzer_stop()

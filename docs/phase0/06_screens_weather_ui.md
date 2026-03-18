# Task 6 — Screen Carousel, Weather, Stopwatch, Phone Locator & Alarms

## Objective

Expand the display from two modes (AMBIENT / DETAIL) into a full six-screen
carousel navigated by BTN_3. Add weather data (pulled from the companion app),
a phone locator, a classic stopwatch, and alarm/timer sync. Finalise the
complete button behaviour model for all screens.

---

## Screen carousel model

BTN_3 short press advances to the next screen. BTN_3 long press returns
directly to MAIN from any screen. Wrist-raise always switches to
NOTIFICATIONS (most likely reason someone raises their wrist mid-day).

```
  ┌──────────────────────────────────────────────────────────┐
  │  BTN_3 short → cycles forward                           │
  │  BTN_3 long  → jump to MAIN from anywhere               │
  └──────────────────────────────────────────────────────────┘

  MAIN ──► WEATHER ──► NOTIFICATIONS ──► PHONE LOCATOR
   ▲                                              │
   └──── ALARMS ◄──── STOPWATCH ◄────────────────┘
```

Wrist-raise → NOTIFICATIONS (not MAIN, as before).
After KEPLER_DETAIL_MODE_TIMEOUT_MS with no button activity → return to MAIN.
Timeout is reset by any button press on any screen.

---

## Revised screen enum

```c
// ui_renderer.h — replace ui_mode_t with:
typedef enum {
    UI_SCREEN_MAIN          = 0,   // time, date, steps, weather summary
    UI_SCREEN_WEATHER       = 1,   // current + hourly forecast
    UI_SCREEN_NOTIFICATIONS = 2,   // scrollable notification list
    UI_SCREEN_PHONE_LOCATOR = 3,   // find phone button
    UI_SCREEN_STOPWATCH     = 4,   // classic stopwatch with laps
    UI_SCREEN_ALARMS        = 5,   // alarm/timer list synced from phone
    UI_SCREEN_COUNT         = 6,
} ui_screen_t;

// Old UI_MODE_AMBIENT → UI_SCREEN_MAIN
// Old UI_MODE_DETAIL  → UI_SCREEN_NOTIFICATIONS
// Replace all ui_set_mode() / ui_get_mode() with ui_set_screen() / ui_get_screen()
```

---

## Complete button behaviour model

### Physical button mapping (matches original F91W positions)

```
  ┌─────────────────────────┐
  │         Watch           │
  │                         │
  │  [BTN_1] ◄── TOP        │  LIGHT position → ACTION / INVERT
  │                         │
  │  [BTN_2] ◄── BOTTOM-L   │  SET position   → CONTEXT-SET / TIME-SET
  │  [BTN_3] ◄── BOTTOM-R   │  MODE position  → SCREEN CYCLE / HOME
  └─────────────────────────┘
```

### Universal (all screens, always)

| Button | Event | Action |
|--------|-------|--------|
| BTN_3 | Short | Advance to next screen (wraps ALARMS → MAIN) |
| BTN_3 | Long | Jump to MAIN from any screen |
| BTN_2 | Long | Enter time-set mode (any screen; exits back to MAIN on confirm) |
| BTN_1 | Long | Toggle display invert for 3s (dark-mode equivalent for Sharp LCD) |

**Why display invert?** The Sharp Memory LCD is reflective — excellent outdoors
but occasionally better inverted (white-on-black) in dim indoor light at certain
angles. This replaces the classic F91W backlight function in a meaningful way.
The invert is temporary (3s) and cosmetic only — framebuffer content is unchanged.

### Per-screen BTN_1 short and BTN_2 short

| Screen | BTN_1 short | BTN_2 short |
|--------|-------------|-------------|
| MAIN | — (no action) | — (no action) |
| WEATHER | Request BLE weather refresh | Toggle temperature unit (°C ↔ °F) |
| NOTIFICATIONS | Dismiss top notification | Scroll to next notification |
| PHONE LOCATOR | **Start / stop phone ringing** | — (no action) |
| STOPWATCH | Start / Stop | Lap (while running) · Reset (while stopped) |
| ALARMS | Toggle selected alarm on/off | Scroll to next alarm |

### During time-set mode (entered from any screen via BTN_2 long)

| Button | Short | Notes |
|--------|-------|-------|
| BTN_1 | Increment selected field | Wraps at max |
| BTN_2 | Confirm → advance to next field | Final confirm writes RTC |
| BTN_3 | Decrement selected field | Wraps at 0 |

Time-set sequence unchanged from Task 2: SET_HOURS → SET_MINUTES → CONFIRM.
30-second inactivity timeout discards changes and returns to MAIN.

### BTN_1 long — display invert implementation

```c
static bool s_display_inverted = false;

void ui_apply_invert(bool invert) {
    s_display_inverted = invert;
    // XOR all bytes in framebuffer with 0xFF (flips every pixel)
    for (int y = 0; y < SHARP_LCD_HEIGHT; y++) {
        for (int b = 0; b < SHARP_LCD_STRIDE; b++) {
            s_framebuffer[y][b] ^= 0xFF;
            fb_mark_dirty(y);
        }
    }
    sharp_lcd_flush_dirty(s_framebuffer);
}

// On EVT_BUTTON_1_LONG:
ui_apply_invert(true);
Clock_setTimeout(s_invert_timer, 3000 * 1000 / Clock_tickPeriod);
Clock_start(s_invert_timer);
// Timer callback: ui_apply_invert(false); ui_render_full();
```

---

## Screen layouts — pixel coordinates (144 × 168 Sharp LCD)

### MAIN screen (updated to include weather summary)

```
y=0  ┌──────────────────────────────────────────┐
     │                                          │
     │      11:47          (large font)         │  y=8..52
     │                                          │
y=54 ├──────────────────────────────────────────┤ (1px divider)
     │   Mon 17 Mar        (small font)         │  y=60..70
y=72 ├──────────────────────────────────────────┤
     │   [☀] 18°C  Sunny   (icon + text)        │  y=78..98  ← NEW
y=100├──────────────────────────────────────────┤
     │   6,284 ████████░░  (steps + bar)        │  y=106..116
y=118├──────────────────────────────────────────┤
     │   ●BLE  🔋83%  [1]  (status)             │  y=124..134
y=136└──────────────────────────────────────────┘
```

Weather icon: 16×16px 1-bit bitmap, left-aligned at x=2, y=80
Temperature: "18°C" in small font at x=22, y=90
Condition text: "Sunny" in small font at x=22, y=102
If no weather data received yet: show "-- °C  No data"

### WEATHER screen

```
y=0  ┌──────────────────────────────────────────┐
     │  NOW             11:47 Mon   (header)    │  y=6..16
y=18 ├──────────────────────────────────────────┤
     │                                          │
     │        [☀]   (large 32×32 icon)          │  y=24..56
     │                                          │
     │     18°C                                 │  y=60..76  (large font)
     │     Sunny                                │  y=80..90
     │     Feels like 16°C                      │  y=94..104
     │     H:24° L:12°  Hum:62%                 │  y=108..118
y=120├──────────────────────────────────────────┤
     │  HOURLY FORECAST                         │  y=124..132
     │  12▸☀18° 14▸⛅16° 16▸🌧14° 18▸🌧13°    │  y=136..152  (4 slots)
y=154├──────────────────────────────────────────┤
     │  Updated 3 min ago   ●BLE                │  y=158..168
     └──────────────────────────────────────────┘
```

Hourly forecast row: 4 slots, each ~34px wide.
Each slot: hour label (2 chars) + tiny 12×12px icon + temp.
If BLE disconnected and no cached data: show "Connect phone for weather".

Large weather icon (32×32px) for current condition.
Small weather icons (12×12px) for hourly forecast slots.

### NOTIFICATIONS screen

```
y=0  ┌──────────────────────────────────────────┐
     │  NOTIFICATIONS  [3]         11:47        │  y=6..16
y=18 ├──────────────────────────────────────────┤
     │  [MSG] WhatsApp                          │  y=24..32
     │  Maria Garcia                            │  y=34..42  ← selected (inverted row)
     │  "Hey are you coming tonight?"           │  y=44..58  (word-wrapped, max 2 lines)
y=60 ├──────────────────────────────────────────┤
     │  [CALL] Phone  2 min ago                 │  y=66..74
     │  Missed · +54 11 1234 5678               │  y=76..84
y=86 ├──────────────────────────────────────────┤
     │  [MSG] Gmail   8 min ago                 │  y=92..100
     │  "Meeting rescheduled to..."             │  y=102..110
y=112├──────────────────────────────────────────┤
     │  BTN1:dismiss  BTN2:scroll  [2 more]     │  y=118..128
     └──────────────────────────────────────────┘
```

Stores up to 10 notifications in a ring buffer (RAM only — not persisted).
Selected notification highlighted with inverted row.
BTN_2 short scrolls selection down. BTN_1 short dismisses selected notification.
Dismissed notifications removed from ring buffer.

### PHONE LOCATOR screen

```
y=0  ┌──────────────────────────────────────────┐
     │  FIND PHONE                  11:47       │  y=6..16
y=60 ├──────────────────────────────────────────┤
     │       [  PHONE ICON 32x32  ]             │  y=40..72
     │                                          │
     │        Press BTN1                        │  y=84..94
     │       to make it ring                    │  y=98..108
y=120└──────────────────────────────────────────┘
     │  Status: idle / RINGING...               │  y=130..140
     │  ●BLE required                           │  y=148..158
     └──────────────────────────────────────────┘
```

State machine:
- IDLE: shows "Press BTN1 to make it ring"
- RINGING: shows "RINGING..." with animated dots (toggle every 500ms)
  BTN_1 short again → send stop command → back to IDLE
  Auto-stop after 30 seconds if not manually stopped
  Wrist-raise or BTN_3 navigates away but does NOT stop ringing (continues in background)

### STOPWATCH screen

```
y=0  ┌──────────────────────────────────────────┐
     │  STOPWATCH       [STOPPED/RUNNING]       │  y=6..16
y=18 ├──────────────────────────────────────────┤
     │                                          │
     │         05:23.41                         │  y=30..64  (large font MM:SS.cs)
     │                                          │
y=66 ├──────────────────────────────────────────┤
     │  LAP 3  01:45.22                         │  y=72..82  ← most recent lap
     │  LAP 2  01:52.18                         │  y=86..96
     │  LAP 1  01:46.01                         │  y=100..110
y=112├──────────────────────────────────────────┤
     │  Total laps: 3  [BTN2:lap/reset]         │  y=118..128
     │  BTN1: start/stop    BTN2:lap/rst        │  y=132..142
     └──────────────────────────────────────────┘
```

Display format: MM:SS.cs (centiseconds, updated every 10ms when running).
Maximum 10 laps stored in RAM (not persisted — stopwatch resets on power cycle).
Lap list scrolls: BTN_2 long while stopped scrolls up through laps.
Navigating away (BTN_3) does NOT stop a running stopwatch — it keeps running.
A running indicator dot blinks in the header when timing.

Centisecond display implementation:
- Use a CC2640R2F hardware timer peripheral for ~10ms periodic interrupt
- Increment a uint32_t counter in 10ms units
- On EVT_STOPWATCH_TICK, update display only when STOPWATCH screen is active
- When not on STOPWATCH screen, timer continues but display not updated

### ALARMS screen

```
y=0  ┌──────────────────────────────────────────┐
     │  ALARMS                      11:47       │  y=6..16
y=18 ├──────────────────────────────────────────┤
     │  ● 07:30  Weekdays           [ON ]       │  y=24..36  ← selected
     │  ○ 09:00  Weekend            [OFF]       │  y=40..52
     │  ● 22:30  Sleep reminder     [ON ]       │  y=56..68
     │  ─────────────────────────────────       │  y=70 divider
     │  TIMER                                   │  y=76..84
     │  ○ No active timers                      │  y=88..98
y=100├──────────────────────────────────────────┤
     │  BTN1:toggle  BTN2:scroll                │  y=106..116
     │  Set alarms in app           ●BLE        │  y=120..130
     └──────────────────────────────────────────┘
```

Alarms are read-only on the watch — created and configured in the Android app.
The watch can only toggle them on/off.
Active timers (countdown) show remaining time and count down.
When an alarm fires: EVT_ALARM_TRIGGER → HAPTIC_ALARM + display switches to ALARMS
screen showing which alarm fired, with BTN_1 to dismiss.

---

## Weather data structures and BLE characteristic

### Data types

```c
// kepler/ble/weather_service.h

typedef enum {
    WEATHER_CLEAR       = 0,
    WEATHER_PARTLY_CLOUD = 1,
    WEATHER_CLOUDY      = 2,
    WEATHER_RAIN        = 3,
    WEATHER_STORM       = 4,
    WEATHER_SNOW        = 5,
    WEATHER_FOG         = 6,
    WEATHER_WINDY       = 7,
    WEATHER_UNKNOWN     = 0xFF,
} weather_condition_t;

typedef struct {
    int8_t   temp_c;           // current temp °C (signed, range -128..127)
    int8_t   feels_like_c;     // feels like °C
    int8_t   temp_high_c;      // day high °C
    int8_t   temp_low_c;       // day low °C
    uint8_t  condition;        // weather_condition_t
    uint8_t  humidity_pct;     // relative humidity 0–100
    uint8_t  wind_kmh;         // wind speed km/h (0–255)
    uint8_t  uv_index;         // 0–11
    uint32_t updated_at;       // Unix timestamp of weather data
} weather_current_t;           // 12 bytes

typedef struct {
    uint8_t hour;              // hour of forecast (0–23)
    int8_t  temp_c;            // forecast temp °C
    uint8_t condition;         // weather_condition_t
} weather_hourly_t;            // 3 bytes per slot

typedef struct {
    weather_current_t current;
    weather_hourly_t  hourly[8];  // next 8 hours (24 bytes)
} weather_payload_t;           // 36 bytes total

// Global weather state (updated on BLE write, read by UI renderer)
extern weather_payload_t g_weather;
extern bool              g_weather_valid;  // false until first update received
```

### New BLE GATT characteristics

Add to the existing custom service (0xFFFF):

```
0xFF06 — Weather data
  Properties : Write Without Response (app → watch)
  Length     : 36 bytes (weather_payload_t)
  On write   : parse, store to g_weather, set g_weather_valid=true,
               write to flash (NV_ID_WEATHER), post EVT_WEATHER_UPDATE

0xFF07 — Phone locator command
  Properties : Write (watch → app), also notify (app → watch for status)
  Length     : 1 byte
  Values     : 0x01 = start ringing, 0x00 = stop ringing
  On app ACK : post EVT_PHONE_LOCATOR_ACK

0xFF08 — Alarms data
  Properties : Write Without Response (app → watch)
  Length     : up to 61 bytes (alarms_payload_t)
  On write   : parse, store to g_alarms, write to flash (NV_ID_ALARMS),
               post EVT_ALARMS_UPDATE

0xFF09 — Alarm trigger (app → watch)
  Properties : Write Without Response
  Length     : 1 byte (alarm index 0–4, or 0xFF = dismiss all)
  On write   : post EVT_ALARM_TRIGGER with alarm index as param

0xFF0A — Weather refresh request (watch → app)
  Properties : Notify
  Length     : 1 byte (0x01 = request new weather data)
  Triggered by BTN_1 short on WEATHER screen
  App receives notification → fetches new weather → writes 0xFF06
```

### Android app additions required

The companion app must:
1. Periodically fetch weather from a source (OpenWeatherMap free API, or
   Android's built-in weather API if available) using the device's last
   known location
2. Format as `weather_payload_t` and write to 0xFF06 on connect and every
   30 minutes while connected
3. Subscribe to 0xFF0A notifications for on-demand refresh requests
4. Implement phone-ringing logic (play a loud tone via MediaPlayer or
   AudioManager when 0xFF07 = 0x01 is received, stop on 0x00 or 30s timeout)
5. Read alarm list from Android AlarmManager or Calendar and write to 0xFF08
6. Write to 0xFF09 when an alarm fires (so watch can vibrate and display it)

**Weather API note for Argentina:** OpenWeatherMap free tier supports up to
60 calls/min and covers Buenos Aires. The companion app should cache the
last response and only re-fetch when the watch requests or on a 30-minute
timer, to avoid draining phone battery with frequent API calls.

---

## Weather icon bitmaps (1-bit, for Sharp LCD)

Two sizes needed: 32×32px for WEATHER screen, 16×16px for MAIN screen,
12×12px for hourly forecast slots.

Each icon is a `const uint8_t` array in `weather_icons.c / weather_icons.h`.

```c
// weather_icons.h
#define WEATHER_ICON_SIZE_LG  32  // 32×32px = 128 bytes per icon
#define WEATHER_ICON_SIZE_MD  16  // 16×16px = 32 bytes per icon
#define WEATHER_ICON_SIZE_SM  12  // 12×12px = 18 bytes per icon (padded to byte boundary)

// Access: weather_icon_lg[WEATHER_CLEAR] = pointer to 128-byte bitmap
extern const uint8_t * const weather_icon_lg[8];  // large (weather screen)
extern const uint8_t * const weather_icon_md[8];  // medium (main screen)
extern const uint8_t * const weather_icon_sm[8];  // small (hourly forecast)
```

Icons to design as 1-bit pixel art (0=black, 1=white background):
- CLEAR: circle (sun) with 8 radiating lines
- PARTLY_CLOUD: sun half-obscured by cloud outline
- CLOUDY: cloud outline, no sun
- RAIN: cloud with 3–4 diagonal downward lines
- STORM: cloud with lightning bolt zigzag
- SNOW: cloud with asterisk/snowflake dots
- FOG: 3 horizontal wavy lines
- WINDY: 3 curved horizontal lines

These are simple enough to hand-design as hex byte arrays. The font for large
digits in Task 1 already established a pattern for embedding bitmaps.

---

## Stopwatch implementation

### State and data

```c
// kepler/stopwatch/stopwatch.h

typedef enum {
    STOPWATCH_IDLE    = 0,
    STOPWATCH_RUNNING = 1,
    STOPWATCH_PAUSED  = 2,
} stopwatch_state_t;

typedef struct {
    uint32_t elapsed_cs;   // centiseconds (10ms units from timer)
    uint32_t start_cs;     // centiseconds at last start
    stopwatch_state_t state;
    uint32_t laps[10];     // lap times in centiseconds
    uint8_t  lap_count;
} stopwatch_t;

void     stopwatch_start(void);
void     stopwatch_stop(void);
void     stopwatch_lap(void);
void     stopwatch_reset(void);
uint32_t stopwatch_elapsed_cs(void);  // returns elapsed centiseconds
```

### Timer implementation

Use TI Timer driver for 10ms periodic interrupt. Only enable timer when
stopwatch is RUNNING. Disable between uses to save power.

```c
// Timer ISR (posts event to queue, does not call display directly):
void stopwatch_timer_isr(void) {
    s_stopwatch.elapsed_cs++;
    // Only post EVT_STOPWATCH_TICK every 10 ticks (100ms) to limit display updates
    if (s_stopwatch.elapsed_cs % 10 == 0) {
        event_queue_post(EVT_STOPWATCH_TICK, s_stopwatch.elapsed_cs, NULL);
    }
}
```

Display is only updated on `EVT_STOPWATCH_TICK` when `ui_get_screen() == UI_SCREEN_STOPWATCH`.

### Formatting elapsed time for display

```c
void stopwatch_format(uint32_t cs, char *buf, size_t len) {
    uint32_t centis  = cs % 100;
    uint32_t seconds = (cs / 100) % 60;
    uint32_t minutes = cs / 6000;
    if (minutes > 99) minutes = 99;  // cap display at 99:59.99
    snprintf(buf, len, "%02lu:%02lu.%02lu", minutes, seconds, centis);
}
```

---

## New events required in event_queue.h

```c
// Add to kepler_event_t enum:
EVT_WEATHER_UPDATE,        // BLE: new weather_payload_t received
EVT_WEATHER_REFRESH_REQ,   // BTN_1 on WEATHER screen: request new data
EVT_PHONE_LOCATOR_START,   // BTN_1 on PHONE_LOCATOR screen: start ringing
EVT_PHONE_LOCATOR_STOP,    // BTN_1 again / timeout: stop ringing
EVT_PHONE_LOCATOR_ACK,     // BLE: app acknowledged ring command
EVT_STOPWATCH_TICK,        // 100ms timer: update stopwatch display
EVT_ALARMS_UPDATE,         // BLE: new alarms_payload_t received
EVT_ALARM_TRIGGER,         // BLE: alarm fired on phone
EVT_ALARM_DISMISS,         // BTN_1 on ALARMS screen: dismiss active alarm
EVT_SCREEN_TIMEOUT,        // replaces EVT_DETAIL_MODE_TIMEOUT
```

---

## Updated kepler_handle_event() additions

```c
case EVT_BUTTON_3_SHORT:
    ui_screen_next();   // advance carousel
    activity_timer_reset();
    break;

case EVT_BUTTON_3_LONG:
    ui_set_screen(UI_SCREEN_MAIN);
    activity_timer_reset();
    break;

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
            if (s_stopwatch.state == STOPWATCH_RUNNING) {
                stopwatch_stop();
            } else {
                stopwatch_start();
            }
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
    break;

case EVT_BUTTON_2_SHORT:
    switch (ui_get_screen()) {
        case UI_SCREEN_NOTIFICATIONS:
            notif_scroll_next();
            break;
        case UI_SCREEN_STOPWATCH:
            if (s_stopwatch.state == STOPWATCH_RUNNING) {
                stopwatch_lap();
            } else if (s_stopwatch.state == STOPWATCH_PAUSED) {
                stopwatch_reset();
            }
            break;
        case UI_SCREEN_ALARMS:
            alarms_scroll_next();
            break;
        case UI_SCREEN_WEATHER:
            ui_weather_toggle_units();  // °C ↔ °F
            break;
        default:
            break;
    }
    break;

case EVT_BUTTON_1_LONG:
    ui_apply_invert(true);
    Clock_start(s_invert_timer);
    break;

case EVT_WEATHER_UPDATE:
    ui_update_weather((weather_payload_t *)msg->data);
    break;

case EVT_WEATHER_REFRESH_REQ:
    ble_notify_weather_refresh();  // write 0x01 to 0xFF0A characteristic
    break;

case EVT_PHONE_LOCATOR_START:
    s_phone_locating = true;
    ble_write_phone_locator(0x01);
    Clock_setTimeout(s_locator_auto_stop_clock, 30000 * 1000 / Clock_tickPeriod);
    Clock_start(s_locator_auto_stop_clock);
    ui_phone_locator_set_state(true);
    break;

case EVT_PHONE_LOCATOR_STOP:
    s_phone_locating = false;
    ble_write_phone_locator(0x00);
    Clock_stop(s_locator_auto_stop_clock);
    ui_phone_locator_set_state(false);
    break;

case EVT_STOPWATCH_TICK:
    if (ui_get_screen() == UI_SCREEN_STOPWATCH) {
        ui_update_stopwatch(msg->param);
    }
    break;

case EVT_ALARMS_UPDATE:
    ui_update_alarms((alarms_payload_t *)msg->data);
    break;

case EVT_ALARM_TRIGGER:
    haptic_play(HAPTIC_ALARM);
    ui_set_screen(UI_SCREEN_ALARMS);
    ui_alarms_show_triggered(msg->param);
    break;

case EVT_SCREEN_TIMEOUT:
    // Only return to MAIN if not stopwatch running
    if (s_stopwatch.state != STOPWATCH_RUNNING) {
        ui_set_screen(UI_SCREEN_MAIN);
    }
    break;

case EVT_WRIST_RAISE:
    lis2dw12_clear_wakeup_src();
    actigraphy_on_movement();
    if (!actigraphy_in_sleep_window()) {
        ui_set_screen(UI_SCREEN_NOTIFICATIONS);  // wrist raise → notifications
        activity_timer_reset();
    }
    break;
```

---

## Flash storage additions (NV IDs)

Add to `flash_store.h`:

```c
#define NV_ID_WEATHER       0x0007  // weather_payload_t (36 bytes) — last known
#define NV_ID_ALARMS        0x0008  // alarms_payload_t  (61 bytes)
#define NV_ID_TEMP_UNIT     0x0009  // uint8_t: 0=Celsius, 1=Fahrenheit

// New public functions:
bool flash_store_write_weather(const weather_payload_t *w);
bool flash_store_read_weather(weather_payload_t *out);

bool flash_store_write_alarms(const alarms_payload_t *a);
bool flash_store_read_alarms(alarms_payload_t *out);

bool flash_store_write_temp_unit(uint8_t unit);
uint8_t flash_store_read_temp_unit(void);  // returns 0 (Celsius) if not set
```

On boot, load last known weather from flash so the display shows something
even before the phone connects.

---

## Notification ring buffer (RAM)

Store up to 10 notifications in a RAM ring buffer. Not persisted.

```c
// kepler/ble/notif_service.h

#define NOTIF_RING_SIZE  10

typedef struct {
    uint8_t  type;          // 0=msg, 1=call, 2=calendar, 3=other
    uint8_t  app_id;        // app enum
    char     sender[21];
    char     text[41];
    uint32_t timestamp;
    bool     dismissed;
} notif_entry_t;

// Ring buffer operations
void notif_ring_push(const notif_payload_t *p);     // add new, evicts oldest if full
void notif_dismiss_selected(void);                   // mark selected as dismissed
void notif_scroll_next(void);                        // move selection to next
uint8_t notif_count_active(void);                    // undismissed count
const notif_entry_t *notif_get_selected(void);       // current selection for UI
```

---

## Alarms data structure

```c
// kepler/alarms/alarm_service.h

typedef struct {
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  enabled;        // 0 = off, 1 = on
    uint8_t  days_mask;      // bit per day: bit0=Mon, bit6=Sun, 0xFF=every day
    char     label[9];       // null-terminated short label (max 8 chars)
} alarm_entry_t;             // 13 bytes

typedef struct {
    uint8_t      count;           // number of valid entries (0–5)
    alarm_entry_t alarms[5];      // up to 5 alarms
} alarms_payload_t;               // 1 + 65 = 66 bytes

extern alarms_payload_t g_alarms;

void alarms_toggle_selected(void);
void alarms_scroll_next(void);
void alarms_show_triggered(uint8_t index);
```

---

## Acceptance criteria for Task 6

### Screen navigation
- [ ] BTN_3 short advances through all 6 screens in order, wrapping correctly
- [ ] BTN_3 long returns to MAIN from any screen
- [ ] Wrist-raise switches to NOTIFICATIONS (not MAIN)
- [ ] Screen activity timeout returns to MAIN after KEPLER_DETAIL_MODE_TIMEOUT_MS
- [ ] Running stopwatch does NOT trigger screen timeout return

### MAIN screen weather summary
- [ ] Weather icon and temperature display correctly on MAIN screen
- [ ] Shows "-- °C" when no weather data has been received
- [ ] Weather updates on EVT_WEATHER_UPDATE without display artefacts

### WEATHER screen
- [ ] Large icon and current conditions render correctly
- [ ] All 8 hourly forecast slots render (icon + temp) in the forecast row
- [ ] BTN_1 short posts EVT_WEATHER_REFRESH_REQ and BLE notify fires on 0xFF0A
- [ ] BTN_2 short toggles °C/°F on display
- [ ] "Updated X min ago" calculates correctly from updated_at timestamp

### NOTIFICATIONS screen
- [ ] Up to 10 notifications stored in ring buffer
- [ ] Oldest evicted when buffer full
- [ ] BTN_2 short scrolls selection correctly
- [ ] BTN_1 short dismisses selected, shrinks active count
- [ ] Notification count badge on MAIN status row reflects active count

### PHONE LOCATOR screen
- [ ] BTN_1 short sends 0x01 to 0xFF07 characteristic
- [ ] Status line shows "RINGING..." while active
- [ ] BTN_1 short again sends 0x00 and returns to idle
- [ ] Auto-stops after 30 seconds if not manually stopped
- [ ] Navigating away does NOT stop ringing

### STOPWATCH screen
- [ ] Start/stop with BTN_1 short
- [ ] Centisecond display updates every 100ms while running
- [ ] Lap stored on BTN_2 short while running
- [ ] Reset on BTN_2 short while stopped
- [ ] Up to 10 laps displayed (most recent at top)
- [ ] Navigating away does not stop a running stopwatch
- [ ] Returning to screen shows current running time

### ALARMS screen
- [ ] Alarm list received via 0xFF08 and displayed correctly
- [ ] BTN_2 short scrolls through alarms
- [ ] BTN_1 short toggles selected alarm on/off
- [ ] Toggle state written back to app via [TBD — add write-back characteristic if needed]
- [ ] EVT_ALARM_TRIGGER switches to ALARMS screen and triggers HAPTIC_ALARM
- [ ] BTN_1 on triggered alarm dismisses it

### Display invert
- [ ] BTN_1 long inverts framebuffer immediately
- [ ] Returns to normal after 3 seconds automatically
- [ ] Does not corrupt framebuffer content (invert is reversible)

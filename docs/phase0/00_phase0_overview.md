# F91 Kepler — Phase 0 Firmware: Master Spec

## What Phase 0 covers

Phase 0 is pure firmware work on the existing Kepler v1 hardware — the CC2640R2F launchpad
or assembled Kepler PCB. No new components are required to start. Drivers for hardware not
yet present (DRV2605L, LIS2DW12, ST25DV04K) are written against stub interfaces and enabled
via compile-time flags once Phase 2 hardware arrives.

## Development environment

- **Toolchain:** TI Code Composer Studio (CCS) 12.x or IAR Embedded Workbench
- **SDK:** TI SimpleLink CC2640R2 SDK v4.x (or latest compatible with existing Kepler firmware)
- **Flash programmer:** XDS110 USB Debug Probe (LAUNCHXL-CC2640R2F) or Segger J-Link
- **Starting point:** Existing Kepler firmware at https://github.com/PegorK/F91_Kepler/tree/master/Firmware

The existing firmware boots, advertises BLE, connects via nRF Connect, and drives the
SSD1306 OLED. Phase 0 replaces the display driver, adds the missing peripheral drivers,
and restructures the firmware for maintainability.

---

## Firmware module map

All new code lives in dedicated source/header pairs under a `kepler/` subdirectory.
The existing Kepler application files are kept but progressively refactored.

```
kepler/
  display/
    sharp_lcd.c / sharp_lcd.h       Task 1 — Sharp Memory LCD driver
    ui_renderer.c / ui_renderer.h   Task 1 — layout engine (6-screen carousel)
    weather_icons.c / .h            Task 6 — 1-bit weather icon bitmaps (3 sizes)
  input/
    buttons.c / buttons.h           Task 2 — GPIO interrupt handlers, debounce
    time_set.c / time_set.h         Task 2 — time-setting state machine
  haptic/
    drv2605l.c / drv2605l.h         Task 3 — DRV2605L I2C driver
    haptic_patterns.c / .h          Task 3 — named pattern sequences
  accel/
    lis2dw12.c / lis2dw12.h         Task 4 — LIS2DW12 I2C driver
    pedometer.c / pedometer.h       Task 4 — step counter, daily reset
    actigraphy.c / actigraphy.h     Task 4 — sleep epoch logging
    wrist_raise.c / wrist_raise.h   Task 4 — wrist-raise interrupt handler
  power/
    power_manager.c / .h            Task 5 — sleep/wake state machine
    event_queue.c / event_queue.h   Task 5 — central interrupt dispatch
  ble/
    notif_service.c / .h            Task 5 — GATT + notification ring buffer
    ble_manager.c / .h              Task 5 — connection, advertising interval mgmt
    weather_service.c / .h          Task 6 — weather GATT characteristic handling
    alarm_service.c / .h            Task 6 — alarm GATT characteristic handling
    locator_service.c / .h          Task 6 — phone locator GATT command
  screens/
    screen_main.c / .h              Task 6 — MAIN screen renderer
    screen_weather.c / .h           Task 6 — WEATHER screen renderer
    screen_notifications.c / .h     Task 6 — NOTIFICATIONS screen renderer
    screen_locator.c / .h           Task 6 — PHONE LOCATOR screen renderer
    screen_stopwatch.c / .h         Task 6 — STOPWATCH screen + timer logic
    screen_alarms.c / .h            Task 6 — ALARMS screen renderer
  storage/
    flash_store.c / flash_store.h   Task 5/6 — NV: steps, sleep, weather, alarms
  audio/
    buzzer.c / buzzer.h             Task 5 — PWM piezo driver
  kepler_main.c                     top-level init and event loop
  kepler_config.h                   compile-time feature flags
```

---

## Compile-time feature flags (kepler_config.h)

```c
// Enable when hardware is present on PCB
#define KEPLER_HAS_SHARP_LCD      1   // always 1 from Phase 0 onward
#define KEPLER_HAS_DRV2605L       0   // enable in Phase 2
#define KEPLER_HAS_LIS2DW12       0   // enable in Phase 2
#define KEPLER_HAS_ST25DV         0   // enable in Phase 2
#define KEPLER_HAS_INDUCTIVE_CHG  0   // enable in Phase 2

// Feature flags (independent of hardware)
#define KEPLER_STEP_COUNTER       (KEPLER_HAS_LIS2DW12)
#define KEPLER_ACTIGRAPHY         (KEPLER_HAS_LIS2DW12)
#define KEPLER_WRIST_RAISE        (KEPLER_HAS_LIS2DW12)
#define KEPLER_HAPTIC             (KEPLER_HAS_DRV2605L)
#define KEPLER_NFC_TAG            (KEPLER_HAS_ST25DV)

// Screen carousel
#define KEPLER_SCREEN_COUNT              6     // MAIN, WEATHER, NOTIF, LOCATOR, STOPWATCH, ALARMS
#define KEPLER_SCREEN_TIMEOUT_MS         8000  // auto-return to MAIN after inactivity
#define KEPLER_INVERT_DISPLAY_DURATION_MS 3000 // BTN_1 long: invert for 3s

// Notification ring buffer
#define KEPLER_NOTIF_RING_SIZE           10    // max notifications stored in RAM

// Weather
#define KEPLER_WEATHER_STALE_SEC         3600  // show stale indicator if data > 1h old
#define KEPLER_WEATHER_AUTO_REFRESH_SEC  1800  // request refresh every 30 min on BLE connect

// Phone locator
#define KEPLER_LOCATOR_AUTO_STOP_SEC     30    // auto-stop phone ringing after 30s

// Pedometer
#define KEPLER_STEP_GOAL_DEFAULT         8000

// Sleep tracking
#define KEPLER_SLEEP_WINDOW_START_H      22
#define KEPLER_SLEEP_WINDOW_END_H        8

// BLE advertising
#define KEPLER_BLE_ADV_INTERVAL_FAST_MS  100
#define KEPLER_BLE_ADV_INTERVAL_SLOW_MS  2000
#define KEPLER_BLE_ADV_FAST_DURATION_MS  30000
```

---

## Central event system

All interrupt sources post events to a single queue consumed by the main task.
No peripheral logic runs inside ISRs — ISRs only post to the queue.

```c
// kepler/power/event_queue.h

typedef enum {
    // Button events
    EVT_BUTTON_1_SHORT,
    EVT_BUTTON_1_LONG,
    EVT_BUTTON_2_SHORT,
    EVT_BUTTON_2_LONG,
    EVT_BUTTON_3_SHORT,
    EVT_BUTTON_3_LONG,

    // Accelerometer
    EVT_WRIST_RAISE,

    // Screen management
    EVT_SCREEN_TIMEOUT,          // inactivity → return to MAIN

    // BLE / connectivity
    EVT_BLE_NOTIFICATION,        // new notification from phone
    EVT_BLE_CONNECTED,
    EVT_BLE_DISCONNECTED,
    EVT_TIME_SYNC,               // phone sent current Unix timestamp

    // Pedometer & health
    EVT_STEP_UPDATE,
    EVT_MIDNIGHT_RESET,
    EVT_BATTERY_LOW,

    // Weather
    EVT_WEATHER_UPDATE,          // phone pushed new weather_payload_t
    EVT_WEATHER_REFRESH_REQ,     // BTN_1 on WEATHER screen — request new data

    // Phone locator
    EVT_PHONE_LOCATOR_START,     // BTN_1 on LOCATOR screen — start ringing
    EVT_PHONE_LOCATOR_STOP,      // BTN_1 again, or 30s auto-stop
    EVT_PHONE_LOCATOR_ACK,       // app acknowledged ring command

    // Stopwatch
    EVT_STOPWATCH_TICK,          // 100ms timer — update stopwatch display

    // Alarms
    EVT_ALARMS_UPDATE,           // phone pushed new alarms_payload_t
    EVT_ALARM_TRIGGER,           // phone sent alarm-fired notification
    EVT_ALARM_DISMISS,           // user dismissed triggered alarm

    // Display
    EVT_DISPLAY_INVERT_RESTORE,  // 3s timer: restore normal display polarity
} kepler_event_t;

typedef struct {
    kepler_event_t type;
    uint32_t       param;   // event-specific payload
    void          *data;    // optional pointer (e.g. notification payload)
} kepler_event_msg_t;

void     event_queue_post(kepler_event_t type, uint32_t param, void *data);
bool     event_queue_pend(kepler_event_msg_t *out, uint32_t timeout_ms);
```

---

## Screen carousel model

The Sharp LCD always shows one of six screens. BTN_3 short cycles forward.
BTN_3 long jumps directly to MAIN. Wrist-raise always jumps to NOTIFICATIONS.
Any button press resets the KEPLER_SCREEN_TIMEOUT_MS inactivity clock.
Timeout returns to MAIN (but not if stopwatch is running).

```
  BTN_3 short → cycles forward (wraps)
  BTN_3 long  → jump to MAIN from anywhere
  Wrist-raise → NOTIFICATIONS (outside sleep window)

  MAIN ──► WEATHER ──► NOTIFICATIONS ──► PHONE LOCATOR
   ▲                                              │
   └──── ALARMS ◄────── STOPWATCH ◄──────────────┘
```

**Screen ASCII layouts:**

```
MAIN (default)
  ┌──────────────────────────┐
  │   11:47                  │  ← large time
  │   Mon 17 Mar             │  ← date
  │   ───────────────────    │
  │   [☀] 22°C  Sunny        │  ← icon + temp + condition  ← NEW
  │   ───────────────────    │
  │   6,284  ████████░  79%  │  ← steps + progress bar
  │   ●BLE  🔋83%  [2 notif] │  ← status row
  └──────────────────────────┘

WEATHER
  ┌──────────────────────────┐
  │  11:47 Mon  ●BLE  🔋83%  │  ← compact header
  │  ───────────────────     │
  │      [☀☀]  22°C          │  ← large icon + temperature
  │  Feels 20°C  Hum 65%     │
  │  ───────────────────     │
  │  12h  14h  16h  18h      │  ← hourly forecast
  │  [☁] [🌧] [🌧] [☁]      │
  │  21°  18°  17°  19°      │
  └──────────────────────────┘

NOTIFICATIONS
  ┌──────────────────────────┐
  │  11:47 Mon  ●BLE  🔋83%  │
  │  ───────────────────     │
  │  [MSG] Maria Garcia      │  ← type + sender (selected, inverted)
  │  "Hey are you coming     │  ← message text (word-wrapped 2 lines)
  │   tonight? We're..."     │
  │  WhatsApp · 2 min ago    │
  │  ◄ 2/5 ►  BTN1=dismiss   │  ← position + hint
  └──────────────────────────┘

PHONE LOCATOR
  ┌──────────────────────────┐
  │  11:47 Mon  ●BLE  🔋83%  │
  │  ───────────────────     │
  │                          │
  │      FIND MY PHONE       │
  │                          │
  │   Press BTN1 to ring     │  ← idle state
  │   [RINGING...]           │  ← active state (blinks)
  │   Press BTN1 to stop     │
  └──────────────────────────┘

STOPWATCH
  ┌──────────────────────────┐
  │  STOPWATCH  [● RUNNING]  │  ← header with state indicator
  │  ───────────────────     │
  │      05:23.41            │  ← large MM:SS.cs
  │  ───────────────────     │
  │  LAP 3  01:45.22         │  ← most recent lap at top
  │  LAP 2  01:52.18         │
  │  LAP 1  01:46.01         │
  │  BTN1:start/stop  BTN2:lap│
  └──────────────────────────┘

ALARMS
  ┌──────────────────────────┐
  │  ALARMS             11:47│
  │  ───────────────────     │
  │  ●07:30  Weekdays  [ON]  │  ← selected (inverted)
  │  ○09:00  Weekend  [OFF]  │
  │  ●22:30  Sleep    [ON]   │
  │  ───────────────────     │
  │  TIMERS: none active     │
  │  BTN1:toggle  BTN2:scroll│
  └──────────────────────────┘
```

**Key behavioural rules:**
- BTN_3 always cycles screens — no auto-return except screen timeout
- Screen timeout (8s default) returns to MAIN only if stopwatch is NOT running
- Incoming notification triggers HAPTIC + 2-second banner overlay on any screen
- Wrist-raise during sleep window does NOT change screen (actigraphy mode)
- Display invert (BTN_1 long) works on any screen, restores after 3s

---

## Phase 0 task order and dependencies

| Task | Module | Depends on | Hardware required |
|------|---------|-----------|-------------------|
| 1 | Sharp LCD driver + UI renderer + screen layouts | Nothing | Sharp LCD or launchpad GPIO for testing |
| 2 | Button handlers + time setting | Event queue | Existing buttons on Kepler PCB |
| 3 | DRV2605L haptic driver | I2C bus, event queue | DRV2605L + motor (stub if absent) |
| 4 | LIS2DW12 (step, wrist-raise, actigraphy) | I2C bus, event queue, flash | LIS2DW12 (stub if absent) |
| 5 | Power manager, BLE service, flash storage, buzzer | All above | None additional |
| 6 | Screens: weather, stopwatch, phone locator, alarms | Display, BLE, flash | None additional |

**Start with Task 1.** The display driver is the foundation everything else renders to.
Build Task 1 completely before moving to Task 2.

---

## I2C bus configuration

```c
// All I2C peripherals share one bus
#define KEPLER_I2C_INSTANCE    0          // CC2640R2F I2C0
#define KEPLER_I2C_SDA_PIN     IOID_5     // verify against Kepler v1 schematic
#define KEPLER_I2C_SCL_PIN     IOID_6     // verify against Kepler v1 schematic
#define KEPLER_I2C_BITRATE     I2C_400kHz

// Device addresses (7-bit)
#define I2C_ADDR_DRV2605L      0x5A
#define I2C_ADDR_LIS2DW12      0x18       // SDO/SA0 pin low
#define I2C_ADDR_ST25DV_USER   0x53
#define I2C_ADDR_ST25DV_SYS    0x57
```

## SPI bus configuration (Sharp LCD)

```c
#define KEPLER_SPI_INSTANCE    0          // CC2640R2F SSI0
#define KEPLER_SPI_CLK_PIN     IOID_10    // verify against schematic
#define KEPLER_SPI_MOSI_PIN    IOID_9     // verify against schematic
#define SHARP_LCD_CS_PIN       IOID_11    // ACTIVE HIGH — not the usual convention
#define SHARP_LCD_DISP_PIN     IOID_12    // display on/off
#define SHARP_LCD_VCOM_PIN     IOID_13    // VCOM toggle — driven by timer
#define KEPLER_SPI_BITRATE     1000000    // 1MHz max for LS013B7DH03
```

**CRITICAL:** Sharp LCD CS is active HIGH. Standard SPI drivers assume active LOW.
The CS must be manually asserted/deasserted around transactions — do not rely on
hardware CS management from the SSI peripheral.

---

## Pin assignment checklist for Claude Code to complete

In Session 1, Claude Code will run:
```bash
grep -r "IOID_\|DIO[0-9]" Firmware/Application/ --include="*.h" | grep -v ".o:"
```
and fill in the table below with real values from `CC2640R2_KEPLER.h`.

The TI CC2640R2F Launchpad reference board (from which Kepler is derived) uses
these standard pins — Kepler likely keeps most of them but **verify from the file**:

| Signal | Launchpad default | Kepler actual (fill in) |
|--------|------------------|------------------------|
| SPI CLK | IOID_10 | __________ |
| SPI MOSI | IOID_9 | __________ |
| SPI MISO | IOID_8 | __________ (not used by Sharp LCD — CS-only read) |
| Sharp LCD CS (active HIGH) | — | __________ (new in v2 PCB) |
| Sharp LCD DISP | — | __________ (new in v2 PCB) |
| Sharp LCD VCOM | — | __________ (new in v2 PCB) |
| I2C SDA | IOID_5 | __________ |
| I2C SCL | IOID_6 | __________ |
| Button 1 (existing, active LOW) | — | __________ |
| Button 2 (existing, active LOW) | — | __________ |
| Button 3 (existing, active LOW) | — | __________ |
| Buzzer PWM | — | __________ |

**Note on Sharp LCD pins:** The original Kepler used an SSD1306 OLED over I2C.
The Sharp LCD uses SPI with three additional control pins (CS, DISP, VCOM).
These pins will be new additions to the v2 PCB — for Phase 0 development on the
launchpad, assign them to any free GPIO. Claude Code will do this during Session 1
and mark them clearly as `KEPLER_PHASE0_LAUNCHPAD_OVERRIDE` in `kepler_config.h`.

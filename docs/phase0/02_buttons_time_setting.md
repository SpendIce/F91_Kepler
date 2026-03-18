# Task 2 — Button Handlers & Time-Setting State Machine

## Objective

Enable all three Kepler buttons with proper debounce and define their
behaviours. Implement a time-setting UI state machine that lets the user
set hours and minutes without a phone connection.

---

## Button GPIO configuration

The Kepler PCB has three buttons wired to CC2640R2F GPIO pins. Button 1 is
already wired in the existing firmware (screen wake). Buttons 2 and 3 have
hardware connections but no interrupt handlers.

Verify actual pin assignments from the v1 KiCad schematic before implementing.

All three buttons are assumed active LOW with internal pull-up (standard for
TI CC2640R2F GPIO). Confirm from schematic.

---

## Debounce strategy

Use software debounce with a 20ms confirmation window. Hardware debounce via
RC filter is preferred if the PCB has it — check schematic.

```c
// buttons.h
typedef enum {
    BTN_1 = 0,
    BTN_2 = 1,
    BTN_3 = 2,
    BTN_COUNT
} button_id_t;

typedef enum {
    BTN_EVT_SHORT,   // press + release < 600ms
    BTN_EVT_LONG,    // press held >= 600ms
} button_event_t;

// Register callback — called from main task context (not ISR)
typedef void (*button_cb_t)(button_id_t btn, button_event_t evt);
void buttons_init(button_cb_t callback);
```

**Debounce ISR flow:**
1. GPIO falling edge ISR fires → disable GPIO interrupt → start 20ms Clock_setTimeout
2. Timer callback: re-read GPIO. If still low: press confirmed. Record timestamp.
   Re-enable interrupt for rising edge.
3. GPIO rising edge ISR fires → compute duration → post SHORT or LONG event to queue.
4. Re-enable falling edge interrupt.

Do NOT post BLE notifications or call display functions from inside the ISR.
Always post to the event queue and handle in main task.

---

## Button behaviour model

### Physical positions (match original F91W layout)

```
  [BTN_1]  TOP button     — LIGHT position → ACTION / DISPLAY INVERT
  [BTN_2]  BOTTOM-LEFT    — SET position   → CONTEXT-SET / TIME-SET
  [BTN_3]  BOTTOM-RIGHT   — MODE position  → SCREEN CYCLE / HOME
```

### Universal rules — active on every screen, always

| Button | Event | Action |
|--------|-------|--------|
| BTN_3 | Short | Advance to next screen (MAIN→WEATHER→NOTIFICATIONS→LOCATOR→STOPWATCH→ALARMS→MAIN) |
| BTN_3 | Long | Jump directly to MAIN from any screen |
| BTN_2 | Long | Enter time-set mode from any screen (exits back to MAIN on confirm or timeout) |
| BTN_1 | Long | Invert display for 3 seconds (Sharp LCD "dark mode" — replaces backlight function) |

### Per-screen contextual actions — BTN_1 short and BTN_2 short

| Screen | BTN_1 short | BTN_2 short |
|--------|-------------|-------------|
| MAIN | — (no action) | — (no action) |
| WEATHER | Request BLE weather refresh | Toggle temperature unit (°C ↔ °F) |
| NOTIFICATIONS | Dismiss selected notification | Scroll to next notification |
| PHONE LOCATOR | Start ringing (or stop if already ringing) | — (no action) |
| STOPWATCH | Start / Stop timer | Lap (while running) · Reset (while stopped) |
| ALARMS | Toggle selected alarm on/off | Scroll to next alarm |

### During time-set mode (entered via BTN_2 long from any screen)

| Button | Short | Notes |
|--------|-------|-------|
| BTN_1 | Increment selected field (hours or minutes) | Wraps at max value |
| BTN_2 | Confirm current field → advance to next | Final confirm writes RTC |
| BTN_3 | Decrement selected field | Wraps at 0 |

Sequence: SET_HOURS → SET_MINUTES → CONFIRM → return to MAIN.
30-second inactivity timeout discards changes and returns to MAIN.
Edited field blinks at 2Hz (250ms on / 250ms off).

### Rationale for BTN_1 long = display invert

The Sharp Memory LCD is reflective and has no backlight. In most conditions
it is perfectly readable. However in dim indoor lighting at certain angles,
inverting the display (black background, white pixels) can improve contrast.
This is a direct functional replacement for the F91W's backlight button — same
physical position, analogous purpose. The invert is cosmetic and temporary (3s),
after which the display returns to normal polarity automatically.

---

## Time-setting state machine

The CC2640R2F has an onboard RTC (Real-Time Clock) peripheral accessible via
the TI `AONRTCSubSecInc` and time keeping utilities in the SDK. Time is also
synced automatically by the companion app via BLE when connected. Manual
time setting is a fallback for when the phone is unavailable.

### State diagram

```
NORMAL
  │
  │  BTN_2 long-press
  ▼
SET_HOURS
  │  BTN_1 short: hours + 1 (wraps at 24)
  │  BTN_3 short: hours - 1 (wraps)
  │  display: large blinking hours, normal minutes
  │
  │  BTN_2 short-press
  ▼
SET_MINUTES
  │  BTN_1 short: minutes + 1 (wraps at 60)
  │  BTN_3 short: minutes - 1 (wraps)
  │  display: normal hours, large blinking minutes
  │
  │  BTN_2 short-press
  ▼
CONFIRM (display shows full time for 1.5s, no blinking)
  │
  │  automatic after 1500ms
  ▼
NORMAL  (RTC updated, BLE time sync re-enabled)
```

### Timeout: if no button pressed for 30 seconds during SET_HOURS or SET_MINUTES,
discard changes and return to NORMAL.

### Blink implementation

During time-set mode, the element being edited blinks at 2Hz (250ms on / 250ms off).
Implement with a 250ms Clock callback that toggles a `s_blink_state` flag and calls
`ui_update_time()` with either the real value or blank/white pixels.

---

## RTC write on confirm

```c
#include <ti/drivers/dpl/ClockP.h>

void time_set_apply(uint8_t hours, uint8_t minutes) {
    // Get current time, overwrite hours and minutes, write back
    uint32_t current = Seconds_get();  // TI SDK: seconds since epoch
    struct tm t;
    gmtime_r((const time_t *)&current, &t);
    t.tm_hour = hours;
    t.tm_min  = minutes;
    t.tm_sec  = 0;
    time_t new_time = mktime(&t);
    Seconds_set((uint32_t)new_time);
}
```

---

## Acceptance criteria for Task 2

- [ ] All three buttons fire SHORT events reliably with no spurious triggers
- [ ] LONG event fires after 600ms hold, not before
- [ ] BTN_3 short advances screen carousel through all 6 screens in order
- [ ] BTN_3 short wraps from ALARMS back to MAIN correctly
- [ ] BTN_3 long returns to MAIN from any screen
- [ ] BTN_2 long enters time-set mode from any screen
- [ ] BTN_1 long inverts display; restores after 3 seconds
- [ ] In time-set: BTN_1 short increments, BTN_3 short decrements, BTN_2 short confirms
- [ ] Edited field blinks at 2Hz during editing
- [ ] Final confirm writes RTC; display shows new time
- [ ] 30-second inactivity timeout discards changes and returns to MAIN
- [ ] No button events are lost during BLE connection events

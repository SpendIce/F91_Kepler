# Task 1 — Sharp Memory LCD Driver & UI Renderer

## Objective

Replace the existing SSD1306 OLED driver with a complete driver for the Sharp
LS013B7DH03 Memory LCD. Implement a UI renderer with ambient and detail display
modes. This task is the foundation for all other firmware tasks.

## Sharp LS013B7DH03 — key facts

- 1.28 inch diagonal, 144 × 168 pixels, monochrome (1 bit per pixel)
- Reflective — no backlight required, readable in all lighting conditions
- Memory display — retains last written frame with zero power
- Draw at rest: ~5 µA (frame held, no SPI activity)
- SPI interface, max clock 1MHz
- Logic levels: 3.3V (compatible with CC2640R2F directly)
- Frame buffer size: 144 × 168 / 8 = 3,024 bytes
- **CS is ACTIVE HIGH** — this is unusual and easy to get wrong
- **VCOM must toggle continuously** while powered — if VCOM stays static, the
  display accumulates DC bias and can be damaged within hours/days

## Datasheet write command format

Every write transaction to the Sharp LCD follows this exact byte sequence:

```
[CS HIGH]
  Mode byte   : 0x80  (write command = 0b10000000)
  Address byte: line number (1–168, NOT 0-indexed)
  Data bytes  : 18 bytes of pixel data (144 pixels / 8 = 18 bytes)
    ... repeat address + data for additional lines ...
  Dummy byte  : 0x00
  Dummy byte  : 0x00
[CS LOW]
```

Bit order within each data byte: **LSB first** (leftmost pixel = bit 0).
This is opposite to most displays — get this wrong and the image mirrors horizontally.

Clear all command (all pixels white):
```
[CS HIGH]
  0x20  (clear command = 0b00100000)
  0x00  (dummy)
[CS LOW]
```

VCOM toggle command (used if driving VCOM via SPI instead of GPIO):
```
[CS HIGH]
  0x40 or 0x00  (bit 6 = VCOM value)
  0x00  (dummy)
[CS LOW]
```
**Preferred approach:** Drive VCOM via dedicated GPIO toggled by a hardware timer.
This is simpler and decouples VCOM from SPI bus contention.

---

## sharp_lcd.h — public interface

```c
#ifndef SHARP_LCD_H
#define SHARP_LCD_H

#include <stdint.h>
#include <stdbool.h>

#define SHARP_LCD_WIDTH   144
#define SHARP_LCD_HEIGHT  168
#define SHARP_LCD_STRIDE  18    // bytes per row (144 / 8)

// Framebuffer: 1 bit per pixel, row-major, LSB = leftmost pixel
// Total: 3024 bytes
typedef uint8_t sharp_fb_t[SHARP_LCD_HEIGHT][SHARP_LCD_STRIDE];

// Initialise SPI, GPIO, VCOM timer. Must be called before any other function.
// Returns true on success.
bool sharp_lcd_init(void);

// Write the full framebuffer to the display (168 lines, ~3ms at 1MHz SPI)
void sharp_lcd_flush(const sharp_fb_t fb);

// Write a range of lines [first_line, last_line] inclusive (1-indexed, matches hardware)
// Use for partial updates — much faster than full flush when only a few rows changed
void sharp_lcd_flush_lines(const sharp_fb_t fb, uint8_t first_line, uint8_t last_line);

// Clear display to all-white (hardware clear command, faster than writing blank frame)
void sharp_lcd_clear(void);

// Turn display on or off (DISP pin). Does not affect framebuffer or VCOM.
void sharp_lcd_set_display(bool on);

// Called by VCOM timer ISR — do not call directly from application
void sharp_lcd_vcom_toggle(void);

#endif // SHARP_LCD_H
```

---

## sharp_lcd.c — implementation notes

### VCOM timer setup

Use a TI `Timer` driver instance configured for periodic callback at 1Hz.
The callback calls `sharp_lcd_vcom_toggle()` which XORs a static VCOM state variable
and sets the GPIO accordingly.

```c
static volatile bool s_vcom_state = false;

void sharp_lcd_vcom_toggle(void) {
    s_vcom_state = !s_vcom_state;
    GPIO_write(SHARP_LCD_VCOM_PIN, s_vcom_state ? 1 : 0);
}
```

The timer must be started before the display is first written and must never be stopped
while the display is powered. Even in deep sleep, the CC2640R2F's RTC can wake the device
to toggle VCOM if needed — or keep the device in a light sleep mode while VCOM is active.

**Practical simplification:** During the 90+ day battery life target, the ~0.5µA
cost of running a 1Hz timer is negligible. Keep the timer always running.

### SPI transaction wrapper

The TI SPI driver is synchronous (blocking). Wrap it:

```c
static void spi_write(const uint8_t *buf, size_t len) {
    SPI_Transaction txn = {
        .count   = len,
        .txBuf   = (void *)buf,
        .rxBuf   = NULL,
    };
    GPIO_write(SHARP_LCD_CS_PIN, 1);   // CS HIGH before transaction
    SPI_transfer(s_spi_handle, &txn);
    GPIO_write(SHARP_LCD_CS_PIN, 0);   // CS LOW after transaction
}
```

### Partial line update implementation

For `sharp_lcd_flush_lines`, build the SPI payload in a local buffer:

```c
void sharp_lcd_flush_lines(const sharp_fb_t fb,
                            uint8_t first_line, uint8_t last_line) {
    // Max payload: 2 (header+addr) * lines + 18 (data) * lines + 2 (dummy)
    // For 168 lines: 2*168 + 18*168 + 2 = 3,362 bytes — allocate on stack or static
    static uint8_t s_txbuf[2 + (1 + 18) * SHARP_LCD_HEIGHT + 2];
    uint16_t idx = 0;

    s_txbuf[idx++] = 0x80;  // write command

    for (uint8_t line = first_line; line <= last_line; line++) {
        s_txbuf[idx++] = line;  // 1-indexed line address
        // Copy 18 bytes of row data — line is 0-indexed in fb, 1-indexed in hardware
        memcpy(&s_txbuf[idx], fb[line - 1], SHARP_LCD_STRIDE);
        idx += SHARP_LCD_STRIDE;
    }

    s_txbuf[idx++] = 0x00;  // trailing dummy
    s_txbuf[idx++] = 0x00;

    spi_write(s_txbuf, idx);
}
```

### Dirty line tracking

To minimise SPI traffic, track which rows have changed since last flush:

```c
static uint32_t s_dirty_mask[6];  // 6 * 32 = 192 bits, covers 168 lines

void fb_mark_dirty(uint8_t line) {  // line is 0-indexed internally
    s_dirty_mask[line / 32] |= (1u << (line % 32));
}

void sharp_lcd_flush_dirty(const sharp_fb_t fb) {
    // Find first and last dirty line, flush that range, clear mask
    uint8_t first = 255, last = 0;
    for (uint8_t i = 0; i < SHARP_LCD_HEIGHT; i++) {
        if (s_dirty_mask[i / 32] & (1u << (i % 32))) {
            if (i < first) first = i;
            if (i > last)  last  = i;
        }
    }
    if (first <= last) {
        sharp_lcd_flush_lines(fb, first + 1, last + 1);  // convert to 1-indexed
    }
    memset(s_dirty_mask, 0, sizeof(s_dirty_mask));
}
```

---

## ui_renderer.h — public interface

```c
#ifndef UI_RENDERER_H
#define UI_RENDERER_H

#include "sharp_lcd.h"
#include "weather.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// The six screens navigated by BTN_3
// Matches the definition in 06_screens_weather_ui.md
typedef enum {
    UI_SCREEN_MAIN          = 0,  // time + date + weather summary + steps + status
    UI_SCREEN_WEATHER       = 1,  // weather detail + 8-slot hourly forecast
    UI_SCREEN_NOTIFICATIONS = 2,  // scrollable notification ring buffer (up to 10)
    UI_SCREEN_PHONE_LOCATOR = 3,  // phone locator — ring/stop
    UI_SCREEN_STOPWATCH     = 4,  // classic stopwatch with laps
    UI_SCREEN_ALARMS        = 5,  // alarm list synced from phone
    UI_SCREEN_COUNT         = 6,
} ui_screen_t;

typedef struct {
    uint8_t  type;            // 0=message, 1=call, 2=calendar, 3=other
    char     app_name[12];    // null-terminated, e.g. "WhatsApp"
    char     sender[21];      // null-terminated, max 20 chars
    char     text[61];        // null-terminated, max 60 chars
    uint32_t timestamp;       // Unix timestamp of notification
} ui_notification_t;

// Initialise renderer — clears framebuffer, sets screen to AMBIENT
void ui_init(void);

// Navigate screens
void       ui_set_screen(ui_screen_t screen);
ui_screen_t ui_get_screen(void);
void       ui_next_screen(void);        // BTN_3 handler calls this

// Update data sources — mark affected rows dirty (does NOT flush to display)
void ui_update_time(const struct tm *t);
void ui_update_steps(uint32_t steps, uint32_t goal);
void ui_update_ble_status(bool connected);
void ui_update_battery(uint8_t percent);
void ui_update_weather(const weather_data_t *w);   // NULL = no data / stale

// Notification history ring buffer (KEPLER_NOTIF_HISTORY_SIZE slots)
void ui_push_notification(const ui_notification_t *notif);   // adds to front
void ui_scroll_notifications(int8_t delta);                  // BTN_1 on SCREEN_NOTIFICATIONS

// Phone finder state
typedef enum { FINDER_IDLE, FINDER_RINGING } finder_state_t;
void ui_set_finder_state(finder_state_t state);

// Flush all dirty rows to display
void ui_flush(void);

// Force full redraw of current screen
void ui_render_full(void);

// Show a 2-second notification banner overlay on current screen
// (used for incoming notification while on non-NOTIFICATIONS screen)
void ui_show_notif_banner(const ui_notification_t *notif);

#endif // UI_RENDERER_H
```

---

## Font strategy

Use pre-baked 1-bit bitmap fonts. Do not import a full font rendering library.

Two fonts needed:
1. **Large time font** (ambient mode, HH:MM): Custom 7-segment style, ~40px tall.
   Each digit is a 24×40 1-bit bitmap = 120 bytes. 10 digits + colon = 11 glyphs = 1320 bytes.
2. **Small text font** (all other text): Use a standard 6×10 monospaced bitmap font.
   Full printable ASCII (96 chars) = 96 × 8 bytes = 768 bytes.

Both font tables fit comfortably in the CC2640R2F's 128KB flash.

**Bitmap drawing primitive** (place a glyph at pixel x, y in the framebuffer):

```c
void fb_draw_glyph(sharp_fb_t fb,
                   const uint8_t *glyph_data,
                   uint8_t glyph_w, uint8_t glyph_h,
                   uint8_t px, uint8_t py,
                   bool invert) {
    for (uint8_t row = 0; row < glyph_h; row++) {
        for (uint8_t col = 0; col < glyph_w; col++) {
            uint8_t byte_idx = (row * ((glyph_w + 7) / 8)) + (col / 8);
            uint8_t bit_mask = 0x80 >> (col % 8);  // MSB first in glyph data
            bool pixel_on = (glyph_data[byte_idx] & bit_mask) != 0;
            if (invert) pixel_on = !pixel_on;
            fb_set_pixel(fb, px + col, py + row, pixel_on);
            fb_mark_dirty(py + row);
        }
    }
}

void fb_set_pixel(sharp_fb_t fb, uint8_t x, uint8_t y, bool black) {
    if (x >= SHARP_LCD_WIDTH || y >= SHARP_LCD_HEIGHT) return;
    // Sharp LCD: LSB = leftmost pixel in each byte
    if (black) {
        fb[y][x / 8] &= ~(1u << (x % 8));   // 0 = black pixel
    } else {
        fb[y][x / 8] |=  (1u << (x % 8));   // 1 = white pixel
    }
}
```

**NOTE on Sharp LCD pixel polarity:** In the Sharp LS013B7DH03, a **0 bit = black pixel**
and a **1 bit = white pixel**. This is inverse of many displays. Initialise the framebuffer
to all 0xFF (all white) not all 0x00.

---

## Ambient mode layout — pixel coordinates

Display: 144px wide × 168px tall

```
y=0  ┌─────────────────────────────────────────┐
     │                                         │
     │       TIME (large font, centred)        │  y=10..52  (large digits ~40px tall)
     │            11:47                        │
     │                                         │
y=56 ├─────────────────────────────────────────┤
     │   Mon 17 Mar  (small font, centred)     │  y=62..72
y=76 ├─────────────────────────────────────────┤
     │ [WX_ICON_16] 22°C  Sunny                │  y=84..100  (16×16 icon + text)
y=104├─────────────────────────────────────────┤
     │   6,284 steps  [████████████░░░] 79%    │  y=110..118  (steps + progress bar)
y=122├─────────────────────────────────────────┤
     │   ● BLE   🔋83%   [2]                   │  y=128..138  (status + notif badge)
y=142└─────────────────────────────────────────┘
```

`[WX_ICON_16]` is a 16×16 pixel 1-bit weather icon (see weather icons section).
The notification badge `[2]` shows the count of unread notifications as a small
filled rectangle with a white digit inside.

---

## Weather screen layout — pixel coordinates

```
y=0  ┌─────────────────────────────────────────┐
     │  11:47  Mon        ● 🔋83%              │  y=4..14  (compact header)
y=16 ├─────────────────────────────────────────┤
     │                                         │
     │        [WX_ICON_32]   22°C              │  y=24..56  (32×32 icon, large temp)
     │                                         │
y=58 ├─────────────────────────────────────────┤
     │   Sunny · Feels 20°C · Hum 65%          │  y=64..74  (condition + details)
y=76 ├─────────────────────────────────────────┤  (horizontal rule)
     │  NOW   13h   14h   15h   16h   17h      │  y=84..94  (hourly headers)
     │  [☀]  [☁]  [☁]  [🌧]  [🌧]  [🌧]     │  y=100..116 (hourly icons 12×12)
     │  22°   21°   20°   18°   17°   16°      │  y=120..130 (hourly temps)
y=134└─────────────────────────────────────────┘
```

Six hourly columns at (144 / 6) = 24px each. Each column: 4px left pad +
12px icon + text centred. The first column labelled "NOW" shows current
conditions. Remaining five show the next 5 hours from the weather data.

If `weather_data_t.updated_at` is more than `KEPLER_WEATHER_STALE_SEC` seconds
ago, show a `?` icon and "No data" in place of temperature.

---

## Notifications screen layout — pixel coordinates

```
y=0  ┌─────────────────────────────────────────┐
     │  11:47  Mon        ● 🔋83%              │  y=4..14  (compact header)
y=16 ├─────────────────────────────────────────┤
     │  [MSG] WhatsApp                         │  y=22..32  (type badge + app, bold sender)
     │  Maria Garcia                           │  y=36..46  (sender name, weight 500)
y=50 ├─────────────────────────────────────────┤
     │  "Hey are you coming                    │  y=56..66  (message line 1)
     │   tonight? We're starting               │  y=70..80  (message line 2)
     │   at 8pm"                               │  y=84..94  (message line 3)
y=98 ├─────────────────────────────────────────┤
     │  2 min ago                              │  y=104..114 (relative time)
y=116├─────────────────────────────────────────┤
     │  ◄ 3 / 5 ►   BTN1 scrolls              │  y=122..132 (position + hint)
y=134└─────────────────────────────────────────┘
```

The position indicator shows current index / total stored notifications.
BTN_1 short press scrolls backward (older). The `◄ ►` arrows indicate scroll
direction availability. When there is only 1 notification, both arrows are absent.
When the notification history is empty, show centred text "No notifications".

---

## Finder screen layout — pixel coordinates

```
y=0  ┌─────────────────────────────────────────┐
     │  11:47  Mon        ● 🔋83%              │  y=4..14  (compact header)
y=16 ├─────────────────────────────────────────┤
     │                                         │
     │       Find my phone                     │  y=52..62  (title, centred)
     │                                         │
     │   [PHONE_ICON_32]                       │  y=68..100 (32×32 phone icon, centred)
     │                                         │
     │   Press BTN_1 to ring    (IDLE state)   │  y=112..122
     │   ── OR ──                              │
     │   [● RINGING...]         (ACTIVE state) │  y=112..122 (blinks 1Hz)
     │   Press BTN_1 to stop                   │  y=126..136
y=140└─────────────────────────────────────────┘
```

The phone icon uses a simple 1-bit bitmap showing a handset silhouette.
In ACTIVE (ringing) state, the "● RINGING..." text blinks at 1Hz via a Clock
callback toggling a `s_finder_blink` flag. BTN_1 sends `EVT_FINDER_TRIGGER`
which the BLE service converts to a BLE notification to the phone app.
If BLE is not connected, show "Not connected" instead of the BTN_1 prompt.

---

## Weather icon bitmaps (weather_icons.c / weather_icons.h)

Weather icons are pre-baked 1-bit bitmaps in two sizes: 16×16 (ambient screen,
notifications banner) and 32×32 (weather detail screen). Define them as
`static const uint8_t` arrays — LSB = leftmost pixel per row, matching the
Sharp LCD framebuffer convention.

```c
// weather_icons.h
typedef enum {
    WX_SUNNY = 0,         // circle with radiating lines (sun)
    WX_PARTLY_CLOUDY,     // sun half-obscured by cloud
    WX_CLOUDY,            // solid cloud silhouette
    WX_RAINY,             // cloud with vertical drop lines below
    WX_HEAVY_RAIN,        // cloud with denser drop lines
    WX_THUNDERSTORM,      // cloud with lightning bolt
    WX_SNOWY,             // cloud with dot/asterisk pattern below
    WX_FOGGY,             // horizontal parallel lines (fog layers)
    WX_NIGHT_CLEAR,       // crescent moon
    WX_NIGHT_CLOUDY,      // crescent moon partially behind cloud
    WX_UNKNOWN,           // question mark
    WX_ICON_COUNT
} wx_icon_id_t;

// 16×16 icons: 16 rows × 2 bytes = 32 bytes each
extern const uint8_t WX_ICONS_16[WX_ICON_COUNT][32];

// 32×32 icons: 32 rows × 4 bytes = 128 bytes each
extern const uint8_t WX_ICONS_32[WX_ICON_COUNT][128];

// Utility: draw a weather icon at pixel position (px, py) in the framebuffer
void draw_wx_icon_16(sharp_fb_t fb, wx_icon_id_t icon, uint8_t px, uint8_t py);
void draw_wx_icon_32(sharp_fb_t fb, wx_icon_id_t icon, uint8_t px, uint8_t py);
```

**Icon design guide for 1-bit bitmaps at 16×16:**
Icons must be legible at small size with no anti-aliasing. Design principles:
- Minimum 2px stroke width for all outlines
- Leave a 1px transparent border around the icon edge
- Use solid fills for cloud bodies; hollow circles with radiating lines for sun
- Rain drops: 3–4 single-pixel-wide vertical lines, 2–3px tall, spaced 3px apart
- Snow: dots arranged in a diamond pattern below the cloud
- Lightning bolt: diagonal zigzag 3px wide
- Night moon: C-shape crescent, 12px tall, 8px wide

**Icon-to-weather-code mapping** (app sends a condition enum, not text):
```c
wx_icon_id_t weather_condition_to_icon(uint8_t condition_code,
                                        bool is_night) {
    // condition_code matches weather_condition_t in weather.h
    // is_night: derived from time-of-day vs sunset/sunrise in weather_data_t
    switch (condition_code) {
        case WEATHER_SUNNY:   return is_night ? WX_NIGHT_CLEAR   : WX_SUNNY;
        case WEATHER_PARTLY:  return is_night ? WX_NIGHT_CLOUDY  : WX_PARTLY_CLOUDY;
        case WEATHER_CLOUDY:  return WX_CLOUDY;
        case WEATHER_RAINY:   return WX_RAINY;
        case WEATHER_HEAVY_RAIN: return WX_HEAVY_RAIN;
        case WEATHER_THUNDER: return WX_THUNDERSTORM;
        case WEATHER_SNOWY:   return WX_SNOWY;
        case WEATHER_FOGGY:   return WX_FOGGY;
        default:              return WX_UNKNOWN;
    }
}
```

---

## Testing without Sharp LCD hardware

If the Sharp LCD is not yet connected, a test harness can write the framebuffer
to the CC2640R2F UART as ASCII art for verification:

```c
#ifdef KEPLER_TEST_DISPLAY_UART
void sharp_lcd_flush(const sharp_fb_t fb) {
    for (int y = 0; y < SHARP_LCD_HEIGHT; y += 2) {
        for (int x = 0; x < SHARP_LCD_WIDTH; x++) {
            uint8_t byte = fb[y][x / 8];
            bool px = (byte >> (x % 8)) & 1;  // 1=white, 0=black
            UART_write(s_uart, px ? " " : "#", 1);
        }
        UART_write(s_uart, "\r\n", 2);
    }
}
#endif
```

---

## Acceptance criteria for Task 1

- [ ] Sharp LCD initialises without error (DISP high, VCOM toggling, clear command sent)
- [ ] Framebuffer initialises to all-white (0xFF)
- [ ] Full flush sends correct SPI waveform (verify with logic analyser or oscilloscope)
- [ ] Pixel polarity correct: set pixel black → appears black on display
- [ ] Partial line flush only sends changed rows
- [ ] VCOM toggles at ~1Hz continuously, including during BLE events
- [ ] UI_SCREEN_MAIN renders: large time, date, 16×16 weather icon + temp + condition, steps + bar, status row
- [ ] UI_SCREEN_WEATHER renders: 32×32 icon, current temp, feels-like, humidity, 8-slot hourly forecast
- [ ] UI_SCREEN_NOTIFICATIONS renders: sender, message text word-wrapped, position indicator
- [ ] UI_SCREEN_PHONE_LOCATOR renders: idle state and ringing state, blink toggles correctly
- [ ] UI_SCREEN_STOPWATCH renders: MM:SS.cs large font, lap list, button hint row
- [ ] UI_SCREEN_ALARMS renders: alarm list with on/off state, timer section
- [ ] BTN_3 cycles all six screens in order: MAIN→WEATHER→NOTIFICATIONS→PHONE_LOCATOR→STOPWATCH→ALARMS→MAIN
- [ ] BTN_3 long returns to MAIN from any screen
- [ ] Notification banner overlay appears for 2s on any active screen when notification arrives
- [ ] Stale weather (>1h) shows stale indicator
- [ ] `ui_flush()` after `ui_update_time()` takes <5ms total
- [ ] All 8 weather condition icons render recognizably in 32×32, 16×16, and 12×12 sizes

/******************************************************************************
 *
 * @file  ui_renderer.c
 *
 * @brief Layout engine for the F91 Kepler 6-screen carousel.
 *
 * Weather data
 * ------------
 * struct weather_data_s is provisionally defined here for Task 1.
 * It will be replaced by the authoritative definition in
 * kepler/ble/weather_service.h when Task 5/6 is implemented.
 * At that point remove the provisional definition below and add the
 * correct #include — no other changes needed in this file.
 *
 * Weather icons
 * -------------
 * weather_icons.h/c is a Task 6 deliverable.  Until then, icon draw calls
 * are routed to draw_wx_stub() which renders a 16x16 or 32x32 dashed box
 * so the layout is visible and testable without the real bitmaps.
 *
 * Stopwatch / Alarms
 * ------------------
 * Dynamic data (elapsed time, lap list, alarm list) is wired in Task 5/6.
 * The renderers produce correct static layouts with placeholder values now.
 *
 *****************************************************************************/

#include "ui_renderer.h"
#include "fonts.h"
#include "../kepler_config.h"

#if KEPLER_HAS_SHARP_LCD

#include <string.h>
#include <stdio.h>

/*==========================================================================*
 *  Provisional weather_data_t — REPLACE with Task 6 include               *
 *==========================================================================*/

typedef enum {
    WEATHER_SUNNY       = 0,
    WEATHER_PARTLY      = 1,
    WEATHER_CLOUDY      = 2,
    WEATHER_RAINY       = 3,
    WEATHER_HEAVY_RAIN  = 4,
    WEATHER_THUNDER     = 5,
    WEATHER_SNOWY       = 6,
    WEATHER_FOGGY       = 7,
    WEATHER_UNKNOWN     = 8,
} weather_condition_t;

struct weather_data_s {
    weather_condition_t condition;
    int8_t   temp_c;
    int8_t   feels_like_c;
    uint8_t  humidity_pct;
    uint32_t updated_at;      /* Unix timestamp of last BLE push            */
    struct {
        weather_condition_t condition;
        int8_t  temp_c;
        uint8_t hour;         /* 0-23                                       */
    } hourly[6];
};

/*==========================================================================*
 *  Layout constants — pixel coordinates from spec                          *
 *==========================================================================*/

/* MAIN screen */
#define MAIN_TIME_Y         10u   /* large time top edge                   */
#define MAIN_SEP1_Y         56u
#define MAIN_DATE_Y         62u
#define MAIN_SEP2_Y         76u
#define MAIN_WX_Y           84u   /* weather icon + text top edge          */
#define MAIN_SEP3_Y         104u
#define MAIN_STEPS_Y        110u
#define MAIN_SEP4_Y         122u
#define MAIN_STATUS_Y       128u

/* Compact header (all secondary screens) */
#define HDR_Y               4u
#define HDR_SEP_Y           16u

/* WEATHER screen */
#define WX_ICON_Y           24u
#define WX_DETAIL_Y         64u
#define WX_SEP_Y            76u
#define WX_HOURLY_HDR_Y     84u
#define WX_HOURLY_ICON_Y    100u
#define WX_HOURLY_TEMP_Y    120u

/* NOTIFICATIONS screen */
#define NOTIF_APP_Y         22u
#define NOTIF_SENDER_Y      36u
#define NOTIF_SEP1_Y        50u
#define NOTIF_TEXT1_Y       56u
#define NOTIF_TEXT2_Y       70u
#define NOTIF_TEXT3_Y       84u
#define NOTIF_SEP2_Y        98u
#define NOTIF_AGE_Y         104u
#define NOTIF_SEP3_Y        116u
#define NOTIF_NAV_Y         122u

/* PHONE LOCATOR screen */
#define LOC_TITLE_Y         52u
#define LOC_ICON_Y          68u
#define LOC_PROMPT_Y        112u
#define LOC_STOP_Y          126u

/* STOPWATCH screen */
#define SW_TITLE_Y          4u
#define SW_SEP1_Y           16u
#define SW_TIME_Y           24u   /* large MM:SS.cs */
#define SW_SEP2_Y           68u
#define SW_LAP_Y            76u
#define SW_HINT_Y           152u

/* ALARMS screen */
#define AL_TITLE_Y          4u
#define AL_SEP_Y            16u
#define AL_ITEM_Y           22u
#define AL_ITEM_H           14u
#define AL_TIMER_SEP_Y      82u
#define AL_TIMER_Y          88u
#define AL_HINT_Y           152u

/* Progress bar geometry */
#define BAR_X               58u
#define BAR_Y               MAIN_STEPS_Y
#define BAR_W               50u
#define BAR_H               6u

/* Hourly column width */
#define HOURLY_COL_W        24u
#define HOURLY_ICON_SZ      12u

/*==========================================================================*
 *  Module state                                                            *
 *==========================================================================*/

static sharp_fb_t         s_fb;

static ui_screen_t        s_screen;

/* Cached display data */
static struct tm          s_time;
static uint32_t           s_steps;
static uint32_t           s_step_goal;
static bool               s_ble;
static uint8_t            s_battery;
static const weather_data_t *s_weather;

/* Notification ring: newest at index 0, oldest at [count-1] */
static ui_notification_t  s_notif[KEPLER_NOTIF_RING_SIZE];
static uint8_t            s_notif_count;
static uint8_t            s_notif_idx;   /* currently visible index        */

/* Phone locator */
static finder_state_t     s_finder;
static bool               s_finder_blink_on;

/* Banner overlay */
static bool               s_banner_active;
static ui_notification_t  s_banner_notif;

/* Stopwatch placeholder state (wired in Task 5) */
static bool               s_sw_running;
static uint32_t           s_sw_cs;       /* total centiseconds             */
static uint8_t            s_sw_laps;     /* lap count                      */
static uint32_t           s_sw_lap_cs[3];/* last 3 lap times               */

static const char *const  DAY_NAMES[7] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};
static const char *const  MON_NAMES[12] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};
static const char *const  WX_COND_STR[] = {
    "Sunny", "Pt Cloudy", "Cloudy", "Rainy",
    "Heavy Rain", "Thunder", "Snowy", "Foggy", "---"
};

/*==========================================================================*
 *  Internal forward declarations                                           *
 *==========================================================================*/

static void draw_separator(uint8_t y);
static void draw_compact_header(void);
static void draw_wx_stub_16(uint8_t px, uint8_t py);
static void draw_wx_stub_32(uint8_t px, uint8_t py);
static void draw_progress_bar(uint8_t pct);
static void draw_notif_badge(uint8_t x, uint8_t y, uint8_t count);
static void render_screen_main(void);
static void render_screen_weather(void);
static void render_screen_notifications(void);
static void render_screen_phone_locator(void);
static void render_screen_stopwatch(void);
static void render_screen_alarms(void);
static void render_current_screen(void);

/*==========================================================================*
 *  Shared drawing helpers                                                  *
 *==========================================================================*/

/* 1-pixel horizontal rule across the full display width. */
static void draw_separator(uint8_t y)
{
    fb_hline(s_fb, 0, y, SHARP_LCD_WIDTH, true);
}

/* Compact header: "HH:MM  DDD    [•] [BBB%]"                             *
 * Shown at HDR_Y on all screens except MAIN.                              */
static void draw_compact_header(void)
{
    char buf[32];
    uint8_t x = 2;

    fb_fill_rect(s_fb, 0, 0, SHARP_LCD_WIDTH, HDR_SEP_Y, false);

    /* Time */
    snprintf(buf, sizeof(buf), "%02d:%02d", s_time.tm_hour, s_time.tm_min);
    font_draw_string(s_fb, x, HDR_Y, buf, false);
    x += 6u * (uint8_t)FONT_SMALL_W;   /* 5 chars + 1 space gap           */

    /* Day abbreviation */
    font_draw_string(s_fb, x, HDR_Y, DAY_NAMES[s_time.tm_wday], false);
    x += 4u * (uint8_t)FONT_SMALL_W;

    /* BLE dot: filled 4x4 square if connected */
    if (s_ble) {
        fb_fill_rect(s_fb,
                     SHARP_LCD_WIDTH - 40u, HDR_Y + 2u,
                     4u, 4u, true);
        font_draw_string(s_fb,
                         SHARP_LCD_WIDTH - 34u, HDR_Y,
                         "BLE", false);
    }

    /* Battery */
    snprintf(buf, sizeof(buf), "%u%%", s_battery);
    font_draw_string(s_fb,
                     SHARP_LCD_WIDTH - (uint8_t)(strlen(buf) * FONT_SMALL_W) - 2u,
                     HDR_Y, buf, false);

    draw_separator(HDR_SEP_Y);
}

/* Placeholder 16x16 weather icon box (Task 6 replaces with real bitmaps). */
static void draw_wx_stub_16(uint8_t px, uint8_t py)
{
    fb_hline(s_fb, px,        py,        16u, true);
    fb_hline(s_fb, px,        py + 15u,  16u, true);
    for (uint8_t r = 1; r < 15u; r++) {
        fb_set_pixel(s_fb, px,        py + r, true);
        fb_set_pixel(s_fb, px + 15u,  py + r, true);
    }
    /* '?' mark at centre */
    font_draw_string(s_fb, px + 5u, py + 4u, "?", false);
}

/* Placeholder 32x32 weather icon box. */
static void draw_wx_stub_32(uint8_t px, uint8_t py)
{
    fb_hline(s_fb, px,        py,        32u, true);
    fb_hline(s_fb, px,        py + 31u,  32u, true);
    for (uint8_t r = 1; r < 31u; r++) {
        fb_set_pixel(s_fb, px,        py + r, true);
        fb_set_pixel(s_fb, px + 31u,  py + r, true);
    }
    font_draw_string(s_fb, px + 12u, py + 12u, "?", false);
}

/* Step progress bar at the fixed MAIN screen position. */
static void draw_progress_bar(uint8_t pct)
{
    if (pct > 100u) pct = 100u;

    /* Outer border */
    fb_hline(s_fb, BAR_X, BAR_Y,          BAR_W, true);
    fb_hline(s_fb, BAR_X, BAR_Y + BAR_H - 1u, BAR_W, true);
    for (uint8_t r = 1; r < BAR_H - 1u; r++) {
        fb_set_pixel(s_fb, BAR_X,          BAR_Y + r, true);
        fb_set_pixel(s_fb, BAR_X + BAR_W - 1u, BAR_Y + r, true);
    }

    /* Fill interior: white first, then black for filled portion */
    uint8_t inner_w = BAR_W - 2u;
    fb_fill_rect(s_fb, BAR_X + 1u, BAR_Y + 1u, inner_w, BAR_H - 2u, false);
    uint8_t filled = (uint8_t)((uint16_t)pct * inner_w / 100u);
    if (filled > 0u) {
        fb_fill_rect(s_fb, BAR_X + 1u, BAR_Y + 1u, filled, BAR_H - 2u, true);
    }
}

/* Small filled-rect badge with a number inside. */
static void draw_notif_badge(uint8_t x, uint8_t y, uint8_t count)
{
    char buf[4];
    snprintf(buf, sizeof(buf), "%u", count);
    uint8_t tw = (uint8_t)(strlen(buf) * FONT_SMALL_W);
    uint8_t bw = tw + 4u;
    uint8_t bh = FONT_SMALL_H + 2u;

    fb_fill_rect(s_fb, x, y, bw, bh, true);
    font_draw_string(s_fb, x + 2u, y + 1u, buf, true);  /* inverted text  */
}

/*==========================================================================*
 *  MAIN screen renderer                                                    *
 *==========================================================================*/

static void render_screen_main(void)
{
    char buf[32];

    fb_clear(s_fb);

    /* --- Large time (centred) ----------------------------------------- *
     * Layout: HH:MM using 24px-wide digits and 12px colon               *
     * Total width = 4*FONT_LARGE_W + FONT_LARGE_COLON_W                 */
    uint8_t time_total_w = 4u * FONT_LARGE_W + FONT_LARGE_COLON_W;
    uint8_t tx = (SHARP_LCD_WIDTH - time_total_w) / 2u;

    font_draw_digit_large(s_fb, tx,                       MAIN_TIME_Y,
                          (uint8_t)(s_time.tm_hour / 10));
    font_draw_digit_large(s_fb, tx + FONT_LARGE_W,        MAIN_TIME_Y,
                          (uint8_t)(s_time.tm_hour % 10));
    font_draw_colon_large(s_fb, tx + 2u * FONT_LARGE_W,  MAIN_TIME_Y);
    font_draw_digit_large(s_fb, tx + 2u * FONT_LARGE_W + FONT_LARGE_COLON_W,
                          MAIN_TIME_Y, (uint8_t)(s_time.tm_min / 10));
    font_draw_digit_large(s_fb, tx + 3u * FONT_LARGE_W + FONT_LARGE_COLON_W,
                          MAIN_TIME_Y, (uint8_t)(s_time.tm_min % 10));

    draw_separator(MAIN_SEP1_Y);

    /* --- Date (centred) ----------------------------------------------- */
    snprintf(buf, sizeof(buf), "%s %02d %s",
             DAY_NAMES[s_time.tm_wday],
             s_time.tm_mday,
             MON_NAMES[s_time.tm_mon]);
    uint8_t dw = font_string_width(buf);
    font_draw_string(s_fb, (SHARP_LCD_WIDTH - dw) / 2u, MAIN_DATE_Y,
                     buf, false);

    draw_separator(MAIN_SEP2_Y);

    /* --- Weather summary (16x16 icon + temp + condition) -------------- */
    uint8_t wx_x = 4u;
    draw_wx_stub_16(wx_x, MAIN_WX_Y);   /* TODO Task 6: real icon        */
    wx_x += 20u;

    if (s_weather != NULL) {
        snprintf(buf, sizeof(buf), "%dC  %s",
                 s_weather->temp_c,
                 WX_COND_STR[s_weather->condition]);
    } else {
        snprintf(buf, sizeof(buf), "--C  No data");
    }
    font_draw_string(s_fb, wx_x, MAIN_WX_Y + 4u, buf, false);

    draw_separator(MAIN_SEP3_Y);

    /* --- Steps + progress bar ----------------------------------------- */
    if (s_step_goal > 0u) {
        uint32_t pct32 = (s_steps * 100u) / s_step_goal;
        uint8_t  pct   = (pct32 > 100u) ? 100u : (uint8_t)pct32;

        snprintf(buf, sizeof(buf), "%lu", (unsigned long)s_steps);
        font_draw_string(s_fb, 2u, MAIN_STEPS_Y, buf, false);

        draw_progress_bar(pct);

        snprintf(buf, sizeof(buf), "%u%%", pct);
        font_draw_string(s_fb, BAR_X + BAR_W + 4u, MAIN_STEPS_Y,
                         buf, false);
    }

    draw_separator(MAIN_SEP4_Y);

    /* --- Status row (BLE dot, battery, notification badge) ------------ */
    uint8_t sx = 2u;

    if (s_ble) {
        fb_fill_rect(s_fb, sx, MAIN_STATUS_Y + 2u, 4u, 4u, true);
        sx += 6u;
        font_draw_string(s_fb, sx, MAIN_STATUS_Y, "BLE", false);
        sx += 4u * FONT_SMALL_W;
    }

    snprintf(buf, sizeof(buf), "%u%%", s_battery);
    font_draw_string(s_fb, sx, MAIN_STATUS_Y, buf, false);

    if (s_notif_count > 0u) {
        draw_notif_badge(SHARP_LCD_WIDTH - 20u, MAIN_STATUS_Y,
                         s_notif_count);
    }
}

/*==========================================================================*
 *  WEATHER screen renderer                                                 *
 *==========================================================================*/

static void render_screen_weather(void)
{
    char buf[32];

    fb_clear(s_fb);
    draw_compact_header();

    if (s_weather == NULL) {
        font_draw_string(s_fb, 20u, 60u, "No weather data", false);
        return;
    }

    /* --- Large icon + current temperature ----------------------------- */
    uint8_t icon_x = (SHARP_LCD_WIDTH / 2u) - 40u;
    draw_wx_stub_32(icon_x, WX_ICON_Y);   /* TODO Task 6: real icon       */

    snprintf(buf, sizeof(buf), "%dC", s_weather->temp_c);
    font_draw_string(s_fb, icon_x + 36u, WX_ICON_Y + 12u, buf, false);

    /* --- Condition + feels-like + humidity ---------------------------- */
    draw_separator(WX_SEP_Y - 2u);
    snprintf(buf, sizeof(buf), "%s  Fl:%dC  H:%u%%",
             WX_COND_STR[s_weather->condition],
             s_weather->feels_like_c,
             s_weather->humidity_pct);
    font_draw_string(s_fb, 2u, WX_DETAIL_Y, buf, false);
    draw_separator(WX_SEP_Y);

    /* --- Hourly forecast (6 columns at 24px each) --------------------- */
    for (uint8_t col = 0; col < 6u; col++) {
        uint8_t cx = (uint8_t)(col * HOURLY_COL_W) + 2u;

        if (col == 0u) {
            font_draw_string(s_fb, cx + 2u, WX_HOURLY_HDR_Y, "NOW", false);
        } else {
            snprintf(buf, sizeof(buf), "%02u", s_weather->hourly[col].hour);
            font_draw_string(s_fb, cx + 4u, WX_HOURLY_HDR_Y, buf, false);
        }

        draw_wx_stub_16(cx + 4u, WX_HOURLY_ICON_Y);   /* TODO Task 6     */

        int8_t t = (col == 0u) ? s_weather->temp_c
                               : s_weather->hourly[col].temp_c;
        snprintf(buf, sizeof(buf), "%d", t);
        font_draw_string(s_fb, cx + 4u, WX_HOURLY_TEMP_Y, buf, false);
    }
}

/*==========================================================================*
 *  NOTIFICATIONS screen renderer                                           *
 *==========================================================================*/

static void render_screen_notifications(void)
{
    char buf[64];

    fb_clear(s_fb);
    draw_compact_header();

    if (s_notif_count == 0u) {
        font_draw_string(s_fb, 28u, 60u, "No notifications", false);
        return;
    }

    const ui_notification_t *n = &s_notif[s_notif_idx];

    /* --- Type badge + app name --------------------------------------- */
    static const char *const TYPE_BADGE[4] = { "MSG", "CALL", "CAL", "???" };
    uint8_t type = (n->type < 4u) ? n->type : 3u;
    snprintf(buf, sizeof(buf), "[%s] %s", TYPE_BADGE[type], n->app_name);
    font_draw_string(s_fb, 2u, NOTIF_APP_Y, buf, false);

    /* --- Sender (inverted row) --------------------------------------- */
    fb_fill_rect(s_fb, 0u, NOTIF_SENDER_Y - 1u,
                 SHARP_LCD_WIDTH, FONT_SMALL_H + 2u, true);
    font_draw_string(s_fb, 2u, NOTIF_SENDER_Y, n->sender, true);

    draw_separator(NOTIF_SEP1_Y);

    /* --- Message text: word-wrap across 3 lines (20 chars per line) -- */
    /* Simple fixed-width character split: 20 chars / 6px = 120px < 144. */
    uint8_t tlen = (uint8_t)strlen(n->text);
    char line[21];
    uint8_t lines[3];
    lines[0] = 0u;
    lines[1] = (tlen > 20u) ? 20u : tlen;
    lines[2] = (tlen > 40u) ? 40u : tlen;

    static const uint8_t TEXT_Y[3] = {
        NOTIF_TEXT1_Y, NOTIF_TEXT2_Y, NOTIF_TEXT3_Y
    };
    for (uint8_t l = 0; l < 3u; l++) {
        if (lines[l] >= tlen) break;
        uint8_t seg = lines[l];
        uint8_t end = (l < 2u) ? lines[l + 1u] : tlen;
        if (end > tlen) end = tlen;
        uint8_t len = end - seg;
        if (len > 20u) len = 20u;
        memcpy(line, &n->text[seg], len);
        line[len] = '\0';
        font_draw_string(s_fb, 2u, TEXT_Y[l], line, false);
    }

    draw_separator(NOTIF_SEP2_Y);

    /* --- Relative time placeholder (full timestamp formatting Task 5) - */
    font_draw_string(s_fb, 2u, NOTIF_AGE_Y, "-- min ago", false);

    draw_separator(NOTIF_SEP3_Y);

    /* --- Navigation indicator ---------------------------------------- */
    buf[0] = '\0';
    if (s_notif_count > 1u) {
        if (s_notif_idx > 0u)
            strncat(buf, "< ", sizeof(buf) - 1u);
        char pos[12];
        snprintf(pos, sizeof(pos), "%u/%u", s_notif_idx + 1u, s_notif_count);
        strncat(buf, pos, sizeof(buf) - 1u);
        if (s_notif_idx < s_notif_count - 1u)
            strncat(buf, " >", sizeof(buf) - 1u);
    }
    uint8_t nw = font_string_width(buf);
    font_draw_string(s_fb, (SHARP_LCD_WIDTH - nw) / 2u,
                     NOTIF_NAV_Y, buf, false);
}

/*==========================================================================*
 *  PHONE LOCATOR screen renderer                                           *
 *==========================================================================*/

static void render_screen_phone_locator(void)
{
    fb_clear(s_fb);
    draw_compact_header();

    /* --- Title -------------------------------------------------------- */
    const char *title = "Find my phone";
    uint8_t tw = font_string_width(title);
    font_draw_string(s_fb, (SHARP_LCD_WIDTH - tw) / 2u, LOC_TITLE_Y,
                     title, false);

    /* --- Phone icon stub (32x32 centred) ------------------------------ */
    uint8_t icon_x = (SHARP_LCD_WIDTH - 32u) / 2u;
    draw_wx_stub_32(icon_x, LOC_ICON_Y);   /* TODO Task 6: handset icon   */

    /* --- State-dependent prompt --------------------------------------- */
    if (!s_ble) {
        const char *msg = "Not connected";
        font_draw_string(s_fb, (SHARP_LCD_WIDTH - font_string_width(msg)) / 2u,
                         LOC_PROMPT_Y, msg, false);
        return;
    }

    if (s_finder == FINDER_IDLE) {
        const char *msg = "Press BTN1 to ring";
        font_draw_string(s_fb, (SHARP_LCD_WIDTH - font_string_width(msg)) / 2u,
                         LOC_PROMPT_Y, msg, false);
    } else {
        /* RINGING: blink "* RINGING..." at 1 Hz via s_finder_blink_on   */
        if (s_finder_blink_on) {
            const char *msg = "* RINGING...";
            font_draw_string(s_fb,
                             (SHARP_LCD_WIDTH - font_string_width(msg)) / 2u,
                             LOC_PROMPT_Y, msg, false);
        }
        const char *stop = "Press BTN1 to stop";
        font_draw_string(s_fb,
                         (SHARP_LCD_WIDTH - font_string_width(stop)) / 2u,
                         LOC_STOP_Y, stop, false);
    }
}

/*==========================================================================*
 *  STOPWATCH screen renderer                                               *
 *==========================================================================*/

static void render_screen_stopwatch(void)
{
    char buf[32];

    fb_clear(s_fb);

    /* --- Header row --------------------------------------------------- */
    font_draw_string(s_fb, 2u, SW_TITLE_Y, "STOPWATCH", false);
    if (s_sw_running) {
        font_draw_string(s_fb, 80u, SW_TITLE_Y, "[RUNNING]", false);
    } else {
        font_draw_string(s_fb, 80u, SW_TITLE_Y, "[STOPPED]", false);
    }
    draw_separator(SW_SEP1_Y);

    /* --- Large elapsed time MM:SS.cs ---------------------------------- */
    uint32_t cs   = s_sw_cs;
    uint8_t  csec = (uint8_t)(cs % 100u);
    uint32_t sec  = cs / 100u;
    uint8_t  min  = (uint8_t)((sec / 60u) % 100u);
    uint8_t  s8   = (uint8_t)(sec % 60u);

    /* MM:SS in large font, .cs in small font below */
    uint8_t lw = 2u * FONT_LARGE_W + FONT_LARGE_COLON_W + 2u * FONT_LARGE_W;
    uint8_t lx = (SHARP_LCD_WIDTH - lw) / 2u;

    font_draw_digit_large(s_fb, lx,                            SW_TIME_Y,
                          min / 10u);
    font_draw_digit_large(s_fb, lx + FONT_LARGE_W,             SW_TIME_Y,
                          min % 10u);
    font_draw_colon_large(s_fb, lx + 2u * FONT_LARGE_W,        SW_TIME_Y);
    font_draw_digit_large(s_fb, lx + 2u * FONT_LARGE_W + FONT_LARGE_COLON_W,
                          SW_TIME_Y, s8 / 10u);
    font_draw_digit_large(s_fb, lx + 3u * FONT_LARGE_W + FONT_LARGE_COLON_W,
                          SW_TIME_Y, s8 % 10u);

    /* centiseconds in small font, right-aligned under large display */
    snprintf(buf, sizeof(buf), ".%02u", csec);
    font_draw_string(s_fb, lx + lw, SW_TIME_Y + FONT_LARGE_H - FONT_SMALL_H,
                     buf, false);

    draw_separator(SW_SEP2_Y);

    /* --- Lap list (last 3, newest first) ------------------------------ */
    uint8_t show = (s_sw_laps < 3u) ? s_sw_laps : 3u;
    for (uint8_t i = 0u; i < show; i++) {
        uint32_t lcs  = s_sw_lap_cs[i];
        uint8_t  lcsec = (uint8_t)(lcs % 100u);
        uint32_t lsec  = lcs / 100u;
        uint8_t  lmin  = (uint8_t)((lsec / 60u) % 100u);
        uint8_t  ls8   = (uint8_t)(lsec % 60u);
        snprintf(buf, sizeof(buf), "LAP %u  %02u:%02u.%02u",
                 s_sw_laps - i,
                 lmin, ls8, lcsec);
        font_draw_string(s_fb, 2u,
                         SW_LAP_Y + i * (FONT_SMALL_H + 2u),
                         buf, false);
    }

    /* --- Button hints ------------------------------------------------- */
    font_draw_string(s_fb, 2u, SW_HINT_Y,
                     "BTN1:start/stop  BTN2:lap", false);
}

/*==========================================================================*
 *  ALARMS screen renderer                                                  *
 *==========================================================================*/

static void render_screen_alarms(void)
{
    fb_clear(s_fb);

    /* --- Header row --------------------------------------------------- */
    font_draw_string(s_fb, 2u, AL_TITLE_Y, "ALARMS", false);
    {
        char tbuf[6];
        snprintf(tbuf, sizeof(tbuf), "%02d:%02d",
                 s_time.tm_hour, s_time.tm_min);
        uint8_t tw = font_string_width(tbuf);
        font_draw_string(s_fb, SHARP_LCD_WIDTH - tw - 2u, AL_TITLE_Y,
                         tbuf, false);
    }
    draw_separator(AL_SEP_Y);

    /* --- Alarm list placeholder (Task 5/6 provides alarm_list_t) ------ */
    font_draw_string(s_fb, 6u, AL_ITEM_Y, "No alarms synced", false);
    font_draw_string(s_fb, 6u, AL_ITEM_Y + AL_ITEM_H, "Connect phone app", false);

    draw_separator(AL_TIMER_SEP_Y);

    /* --- Timer section ------------------------------------------------ */
    font_draw_string(s_fb, 2u, AL_TIMER_Y, "TIMERS: none active", false);

    /* --- Button hints ------------------------------------------------- */
    font_draw_string(s_fb, 2u, AL_HINT_Y,
                     "BTN1:toggle  BTN2:scroll", false);
}

/*==========================================================================*
 *  Screen dispatch                                                         *
 *==========================================================================*/

static void render_current_screen(void)
{
    switch (s_screen) {
        case UI_SCREEN_MAIN:          render_screen_main();          break;
        case UI_SCREEN_WEATHER:       render_screen_weather();        break;
        case UI_SCREEN_NOTIFICATIONS: render_screen_notifications();  break;
        case UI_SCREEN_PHONE_LOCATOR: render_screen_phone_locator();  break;
        case UI_SCREEN_STOPWATCH:     render_screen_stopwatch();      break;
        case UI_SCREEN_ALARMS:        render_screen_alarms();         break;
        default:                      render_screen_main();           break;
    }
}

/*==========================================================================*
 *  Public API — lifecycle                                                  *
 *==========================================================================*/

void ui_init(void)
{
    s_screen        = UI_SCREEN_MAIN;
    s_steps         = 0u;
    s_step_goal     = KEPLER_STEP_GOAL_DEFAULT;
    s_ble           = false;
    s_battery       = 100u;
    s_weather       = NULL;
    s_notif_count   = 0u;
    s_notif_idx     = 0u;
    s_finder        = FINDER_IDLE;
    s_finder_blink_on = false;
    s_banner_active = false;
    s_sw_running    = false;
    s_sw_cs         = 0u;
    s_sw_laps       = 0u;

    memset(&s_time, 0, sizeof(s_time));
    memset(s_notif, 0, sizeof(s_notif));
    memset(s_sw_lap_cs, 0, sizeof(s_sw_lap_cs));

    fb_clear(s_fb);
    render_screen_main();
    sharp_lcd_flush(s_fb);
}

/*==========================================================================*
 *  Public API — screen navigation                                          *
 *==========================================================================*/

void ui_set_screen(ui_screen_t screen)
{
    if (screen >= UI_SCREEN_COUNT) return;
    s_screen = screen;
    render_current_screen();
}

ui_screen_t ui_get_screen(void)
{
    return s_screen;
}

void ui_next_screen(void)
{
    s_screen = (ui_screen_t)((s_screen + 1u) % UI_SCREEN_COUNT);
    render_current_screen();
}

void ui_goto_main(void)
{
    s_screen = UI_SCREEN_MAIN;
    render_screen_main();
}

/*==========================================================================*
 *  Public API — data updates                                               *
 *==========================================================================*/

void ui_update_time(const struct tm *t)
{
    if (t == NULL) return;
    s_time = *t;

    /* Re-render affected rows for the current screen. */
    if (s_screen == UI_SCREEN_MAIN) {
        /* Clear and redraw the time band + date band only. */
        fb_fill_rect(s_fb, 0u, MAIN_TIME_Y,
                     SHARP_LCD_WIDTH, MAIN_SEP1_Y - MAIN_TIME_Y, false);
        fb_fill_rect(s_fb, 0u, MAIN_DATE_Y,
                     SHARP_LCD_WIDTH, MAIN_SEP2_Y - MAIN_DATE_Y, false);
        render_screen_main();   /* full redraw is simplest for now         */
    } else {
        /* Compact header time changes on every screen. */
        draw_compact_header();
    }
}

void ui_update_time_blink(const struct tm *t, bool blank_hours, bool blank_minutes)
{
    ui_update_time(t);

    if (s_screen != UI_SCREEN_MAIN) { return; }
    if (!blank_hours && !blank_minutes) { return; }

    /* Compute the same tx as render_screen_main() uses. */
    uint8_t time_total_w = 4u * FONT_LARGE_W + FONT_LARGE_COLON_W;
    uint8_t tx           = (SHARP_LCD_WIDTH - time_total_w) / 2u;

    if (blank_hours) {
        fb_fill_rect(s_fb, tx, MAIN_TIME_Y,
                     2u * FONT_LARGE_W, FONT_LARGE_H, false);
    }
    if (blank_minutes) {
        fb_fill_rect(s_fb,
                     tx + 2u * FONT_LARGE_W + FONT_LARGE_COLON_W, MAIN_TIME_Y,
                     2u * FONT_LARGE_W, FONT_LARGE_H, false);
    }
}

void ui_update_steps(uint32_t steps, uint32_t goal)
{
    s_steps     = steps;
    s_step_goal = goal;

    if (s_screen == UI_SCREEN_MAIN) {
        /* Clear and redraw the steps band only. */
        fb_fill_rect(s_fb, 0u, MAIN_STEPS_Y,
                     SHARP_LCD_WIDTH, MAIN_SEP4_Y - MAIN_STEPS_Y, false);
        if (goal > 0u) {
            char buf[16];
            snprintf(buf, sizeof(buf), "%lu", (unsigned long)steps);
            font_draw_string(s_fb, 2u, MAIN_STEPS_Y, buf, false);

            uint8_t pct = (uint8_t)((steps * 100u) / goal);
            draw_progress_bar(pct);

            snprintf(buf, sizeof(buf), "%u%%", pct);
            font_draw_string(s_fb, BAR_X + BAR_W + 4u, MAIN_STEPS_Y,
                             buf, false);
        }
    }
}

void ui_update_ble_status(bool connected)
{
    s_ble = connected;

    if (s_screen == UI_SCREEN_MAIN) {
        fb_fill_rect(s_fb, 0u, MAIN_STATUS_Y,
                     SHARP_LCD_WIDTH,
                     SHARP_LCD_HEIGHT - MAIN_STATUS_Y, false);
        /* Re-render status row */
        char buf[8];
        uint8_t sx = 2u;
        if (s_ble) {
            fb_fill_rect(s_fb, sx, MAIN_STATUS_Y + 2u, 4u, 4u, true);
            sx += 6u;
            font_draw_string(s_fb, sx, MAIN_STATUS_Y, "BLE", false);
            sx += 4u * FONT_SMALL_W;
        }
        snprintf(buf, sizeof(buf), "%u%%", s_battery);
        font_draw_string(s_fb, sx, MAIN_STATUS_Y, buf, false);
        if (s_notif_count > 0u) {
            draw_notif_badge(SHARP_LCD_WIDTH - 20u, MAIN_STATUS_Y,
                             s_notif_count);
        }
    } else {
        draw_compact_header();
    }
}

void ui_update_battery(uint8_t percent)
{
    if (percent > 100u) percent = 100u;
    s_battery = percent;

    if (s_screen == UI_SCREEN_MAIN) {
        /* Redraw status row only */
        ui_update_ble_status(s_ble);   /* reuses status row draw logic    */
    } else {
        draw_compact_header();
    }
}

void ui_update_weather(const weather_data_t *w)
{
    s_weather = w;

    if (s_screen == UI_SCREEN_MAIN) {
        fb_fill_rect(s_fb, 0u, MAIN_WX_Y,
                     SHARP_LCD_WIDTH, MAIN_SEP3_Y - MAIN_WX_Y, false);
        uint8_t wx_x = 4u;
        draw_wx_stub_16(wx_x, MAIN_WX_Y);
        wx_x += 20u;
        char buf[32];
        if (s_weather != NULL) {
            snprintf(buf, sizeof(buf), "%dC  %s",
                     s_weather->temp_c,
                     WX_COND_STR[s_weather->condition]);
        } else {
            snprintf(buf, sizeof(buf), "--C  No data");
        }
        font_draw_string(s_fb, wx_x, MAIN_WX_Y + 4u, buf, false);
    } else if (s_screen == UI_SCREEN_WEATHER) {
        render_screen_weather();
    }
}

/*==========================================================================*
 *  Public API — notification ring buffer                                   *
 *==========================================================================*/

void ui_push_notification(const ui_notification_t *notif)
{
    if (notif == NULL) return;

    /* Shift ring up (index 0 = newest). */
    if (s_notif_count < KEPLER_NOTIF_RING_SIZE) {
        s_notif_count++;
    }
    for (uint8_t i = s_notif_count - 1u; i > 0u; i--) {
        s_notif[i] = s_notif[i - 1u];
    }
    s_notif[0] = *notif;
    s_notif_idx = 0u;

    if (s_screen == UI_SCREEN_NOTIFICATIONS) {
        render_screen_notifications();
    } else {
        /* Update notification badge on MAIN status row. */
        if (s_screen == UI_SCREEN_MAIN) {
            if (s_notif_count > 0u) {
                draw_notif_badge(SHARP_LCD_WIDTH - 20u, MAIN_STATUS_Y,
                                 s_notif_count);
            }
        }
    }
}

void ui_scroll_notifications(int8_t delta)
{
    if (s_notif_count == 0u) return;

    if (delta > 0) {
        if (s_notif_idx + 1u < s_notif_count) s_notif_idx++;
    } else {
        if (s_notif_idx > 0u) s_notif_idx--;
    }

    if (s_screen == UI_SCREEN_NOTIFICATIONS) {
        render_screen_notifications();
    }
}

uint8_t ui_notif_count(void)
{
    return s_notif_count;
}

/*==========================================================================*
 *  Public API — phone locator                                              *
 *==========================================================================*/

void ui_set_finder_state(finder_state_t state)
{
    s_finder = state;
    s_finder_blink_on = (state == FINDER_RINGING);

    if (s_screen == UI_SCREEN_PHONE_LOCATOR) {
        render_screen_phone_locator();
    }
}

void ui_finder_blink_tick(void)
{
    if (s_finder != FINDER_RINGING) return;

    s_finder_blink_on = !s_finder_blink_on;

    if (s_screen == UI_SCREEN_PHONE_LOCATOR) {
        /* Redraw only the prompt line rather than the full screen. */
        fb_fill_rect(s_fb, 0u, LOC_PROMPT_Y,
                     SHARP_LCD_WIDTH, FONT_SMALL_H + 2u, false);
        if (s_finder_blink_on) {
            const char *msg = "* RINGING...";
            font_draw_string(s_fb,
                             (SHARP_LCD_WIDTH - font_string_width(msg)) / 2u,
                             LOC_PROMPT_Y, msg, false);
        }
    }
}

/*==========================================================================*
 *  Public API — display flush                                              *
 *==========================================================================*/

void ui_flush(void)
{
    sharp_lcd_flush_dirty(s_fb);
}

void ui_render_full(void)
{
    render_current_screen();
    sharp_lcd_flush(s_fb);
}

/*==========================================================================*
 *  Public API — notification banner overlay                               *
 *==========================================================================*/

void ui_show_notif_banner(const ui_notification_t *notif)
{
    if (notif == NULL) return;

    s_banner_active = true;
    s_banner_notif  = *notif;

    /* Overwrite the top 20 rows with the banner. */
    fb_fill_rect(s_fb, 0u, 0u, SHARP_LCD_WIDTH, 20u, false);
    fb_hline(s_fb, 0u, 0u, SHARP_LCD_WIDTH, true);
    fb_hline(s_fb, 0u, 19u, SHARP_LCD_WIDTH, true);

    char buf[33];   /* "%.11s: %.19s" → max 32 chars + null */
    snprintf(buf, sizeof(buf), "%.11s: %.19s",
             notif->app_name, notif->sender);
    font_draw_string(s_fb, 2u, 2u, buf, false);
    font_draw_string(s_fb, 2u, 2u + FONT_SMALL_H, notif->text, false);
}

void ui_banner_expire(void)
{
    if (!s_banner_active) return;

    s_banner_active = false;

    /* Restore the top 20 rows by re-rendering the current screen's        *
     * header region.                                                       */
    if (s_screen == UI_SCREEN_MAIN) {
        /* MAIN has no compact header; redraw the time band. */
        fb_fill_rect(s_fb, 0u, 0u, SHARP_LCD_WIDTH, 20u, false);
        /* Trigger a full MAIN redraw to be safe. */
        render_screen_main();
    } else {
        draw_compact_header();
    }
}

#endif /* KEPLER_HAS_SHARP_LCD */

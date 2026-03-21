/******************************************************************************
 *
 * @file  ui_renderer.h
 *
 * @brief Layout engine for the F91 Kepler 6-screen carousel.
 *
 *        Owns one global framebuffer.  All screen renderers write into it
 *        via the fb_* primitives from sharp_lcd.h.  ui_flush() pushes only
 *        dirty rows to the display.
 *
 *        weather_data_t is forward-declared here; the full definition will
 *        live in kepler/ble/weather_service.h (Task 6).  ui_update_weather()
 *        accepts a pointer so only the pointer size matters at this stage.
 *
 *****************************************************************************/

#ifndef UI_RENDERER_H
#define UI_RENDERER_H

#include "sharp_lcd.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/*--- Forward declaration -- full type defined in Task 6 -----------------*/
typedef struct weather_data_s weather_data_t;

/*--- Screen identifiers -------------------------------------------------*/
typedef enum {
    UI_SCREEN_MAIN          = 0,
    UI_SCREEN_WEATHER       = 1,
    UI_SCREEN_NOTIFICATIONS = 2,
    UI_SCREEN_PHONE_LOCATOR = 3,
    UI_SCREEN_STOPWATCH     = 4,
    UI_SCREEN_ALARMS        = 5,
    UI_SCREEN_COUNT         = 6,
} ui_screen_t;

/*--- Notification record ------------------------------------------------*/
typedef struct {
    uint8_t  type;         /* 0=message, 1=call, 2=calendar, 3=other      */
    char     app_name[12]; /* null-terminated, e.g. "WhatsApp"             */
    char     sender[21];   /* null-terminated, max 20 chars                */
    char     text[61];     /* null-terminated, max 60 chars                */
    uint32_t timestamp;    /* Unix timestamp of notification               */
} ui_notification_t;

/*--- Phone finder state -------------------------------------------------*/
typedef enum {
    FINDER_IDLE    = 0,
    FINDER_RINGING = 1,
} finder_state_t;

/*==========================================================================*
 *  Lifecycle                                                               *
 *==========================================================================*/

/* Initialise renderer: clears framebuffer, renders UI_SCREEN_MAIN.        *
 * Must be called after sharp_lcd_init().                                  */
void ui_init(void);

/*==========================================================================*
 *  Screen navigation                                                       *
 *==========================================================================*/

void        ui_set_screen(ui_screen_t screen);
ui_screen_t ui_get_screen(void);
void        ui_next_screen(void);   /* BTN_3 short: advance carousel       */
void        ui_goto_main(void);     /* BTN_3 long:  jump to MAIN           */

/*==========================================================================*
 *  Data-source updates                                                     *
 *  Each call marks affected rows dirty but does NOT flush to display.     *
 *  Call ui_flush() after all updates in a frame are complete.             *
 *==========================================================================*/

void ui_update_time(const struct tm *t);

/* Like ui_update_time() but blanks the hours or minutes digits for blink. *
 * blank_hours=true whites out the HH region; blank_minutes whites out MM. *
 * Only affects MAIN screen (compact header is never blanked).             */
void ui_update_time_blink(const struct tm *t,
                          bool blank_hours, bool blank_minutes);
void ui_update_steps(uint32_t steps, uint32_t goal);
void ui_update_ble_status(bool connected);
void ui_update_battery(uint8_t percent);

/* w == NULL means no data available (shows stale / "No data" indicator). */
void ui_update_weather(const weather_data_t *w);

/*==========================================================================*
 *  Notification ring buffer                                                *
 *==========================================================================*/

/* Add a notification to the front of the ring (oldest slot overwritten).  */
void ui_push_notification(const ui_notification_t *notif);

/* Scroll the visible notification.  delta = +1 (older) / -1 (newer).     */
void ui_scroll_notifications(int8_t delta);

/* Return count of stored notifications (0 .. KEPLER_NOTIF_RING_SIZE).    */
uint8_t ui_notif_count(void);

/*==========================================================================*
 *  Phone locator                                                           *
 *==========================================================================*/

void ui_set_finder_state(finder_state_t state);

/* Called by a 1 Hz clock to blink the "RINGING" text in FINDER_RINGING.  */
void ui_finder_blink_tick(void);

/*==========================================================================*
 *  Display flush                                                           *
 *==========================================================================*/

/* Push all dirty rows to the Sharp LCD.                                   */
void ui_flush(void);

/* Force a complete redraw of the current screen, then flush.              */
void ui_render_full(void);

/*==========================================================================*
 *  Notification banner overlay                                             *
 *==========================================================================*/

/* Show a 2-second banner over the current screen when a notification       *
 * arrives while not on UI_SCREEN_NOTIFICATIONS.  Rendered in the top      *
 * 20 rows; a timer callback restores those rows afterward.                */
void ui_show_notif_banner(const ui_notification_t *notif);

/* Called by the 2-second banner timer to restore the underlying screen.   */
void ui_banner_expire(void);

#endif /* UI_RENDERER_H */

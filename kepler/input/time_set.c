/******************************************************************************
 *
 * @file  time_set.c
 *
 * @brief Manual time-setting state machine for the F91 Kepler firmware.
 *
 *        Three TI-RTOS Clock objects drive blink, inactivity timeout, and
 *        the post-confirm display hold.  Their Swi callbacks set volatile
 *        flags only; all display work happens in time_set_process() which
 *        must be called from the main task.
 *
 *        RTC access uses TI SDK Seconds_get() / Seconds_set().
 *
 *****************************************************************************/

#include "time_set.h"
#include "../kepler_config.h"
#include "../display/ui_renderer.h"

#include <ti/sysbios/knl/Clock.h>
#include <ti/drivers/Seconds.h>

#include <time.h>
#include <string.h>

/*--- Timing helpers (ms → RTOS ticks) -----------------------------------*/
#define MS_TO_TICKS(ms)  ((uint32_t)(ms) * 1000u / Clock_tickPeriod)

/*--- State ---------------------------------------------------------------*/
static time_set_state_t  s_state;
static uint8_t           s_hours;
static uint8_t           s_minutes;
static bool              s_blink_on;

/* Volatile flags set by Swi callbacks, cleared by time_set_process().    */
static volatile bool     s_blink_dirty;
static volatile bool     s_timeout_fired;
static volatile bool     s_confirm_done;

/*--- Clocks --------------------------------------------------------------*/
static Clock_Struct s_blink_clk;    /* periodic, KEPLER_TIME_SET_BLINK_MS   */
static Clock_Struct s_timeout_clk;  /* one-shot, KEPLER_TIME_SET_TIMEOUT_MS */
static Clock_Struct s_confirm_clk;  /* one-shot, KEPLER_TIME_SET_CONFIRM_MS */

/*==========================================================================*
 *  Swi callbacks (Swi context — set flags only)                            *
 *==========================================================================*/

static void blink_swi(UArg arg)
{
    (void)arg;
    s_blink_on    = !s_blink_on;
    s_blink_dirty = true;
}

static void timeout_swi(UArg arg)
{
    (void)arg;
    s_timeout_fired = true;
}

static void confirm_swi(UArg arg)
{
    (void)arg;
    s_confirm_done = true;
}

/*==========================================================================*
 *  Internal helpers                                                         *
 *==========================================================================*/

/* Build a struct tm from the working hours/minutes for display calls.     */
static void make_display_tm(struct tm *t)
{
    memset(t, 0, sizeof(*t));
    t->tm_hour = s_hours;
    t->tm_min  = s_minutes;
}

/* Redraw the time with the appropriate field blanked for blink-off phase. */
static void render_blink(void)
{
    struct tm t;
    make_display_tm(&t);
    bool blank_h = (s_state == TS_SET_HOURS)   && !s_blink_on;
    bool blank_m = (s_state == TS_SET_MINUTES)  && !s_blink_on;
    ui_update_time_blink(&t, blank_h, blank_m);
}

/* Write the working hours/minutes to the RTC (preserves date/seconds=0). */
static void time_set_apply(void)
{
    time_t   current = (time_t)Seconds_get();
    struct tm t;
    gmtime_r(&current, &t);
    t.tm_hour = (int)s_hours;
    t.tm_min  = (int)s_minutes;
    t.tm_sec  = 0;
    time_t new_time = mktime(&t);
    Seconds_set((uint32_t)new_time);
}

/* Transition into SET_HOURS: read RTC, reset timers, start editing.      */
static void enter_time_set(void)
{
    time_t   now = (time_t)Seconds_get();
    struct tm t;
    gmtime_r(&now, &t);
    s_hours   = (uint8_t)t.tm_hour;
    s_minutes = (uint8_t)t.tm_min;

    s_blink_on      = true;
    s_blink_dirty   = false;
    s_timeout_fired = false;
    s_confirm_done  = false;
    s_state         = TS_SET_HOURS;

    ui_set_screen(UI_SCREEN_MAIN);

    Clock_setTimeout(Clock_handle(&s_timeout_clk),
                     MS_TO_TICKS(KEPLER_TIME_SET_TIMEOUT_MS));
    Clock_start(Clock_handle(&s_timeout_clk));
    Clock_start(Clock_handle(&s_blink_clk));

    render_blink();
    ui_flush();
}

/* Restart the inactivity timeout (any button activity resets it).        */
static void reset_timeout(void)
{
    Clock_stop(Clock_handle(&s_timeout_clk));
    Clock_setTimeout(Clock_handle(&s_timeout_clk),
                     MS_TO_TICKS(KEPLER_TIME_SET_TIMEOUT_MS));
    Clock_start(Clock_handle(&s_timeout_clk));
}

/* Return to NORMAL without writing the RTC.                              */
static void cancel_time_set(void)
{
    Clock_stop(Clock_handle(&s_blink_clk));
    Clock_stop(Clock_handle(&s_timeout_clk));
    Clock_stop(Clock_handle(&s_confirm_clk));
    s_state = TS_NORMAL;

    /* Restore display from actual RTC. */
    time_t   now = (time_t)Seconds_get();
    struct tm t;
    gmtime_r(&now, &t);
    ui_update_time(&t);
    ui_flush();
}

/*==========================================================================*
 *  Public API                                                               *
 *==========================================================================*/

void time_set_init(void)
{
    Clock_Params p;

    s_state         = TS_NORMAL;
    s_hours         = 0u;
    s_minutes       = 0u;
    s_blink_on      = false;
    s_blink_dirty   = false;
    s_timeout_fired = false;
    s_confirm_done  = false;

    /* Blink: periodic at KEPLER_TIME_SET_BLINK_MS. */
    Clock_Params_init(&p);
    p.period    = MS_TO_TICKS(KEPLER_TIME_SET_BLINK_MS);
    p.startFlag = FALSE;
    p.arg       = 0u;
    Clock_construct(&s_blink_clk, blink_swi,
                    MS_TO_TICKS(KEPLER_TIME_SET_BLINK_MS), &p);

    /* Timeout: one-shot, started when time-set is entered. */
    Clock_Params_init(&p);
    Clock_construct(&s_timeout_clk, timeout_swi,
                    MS_TO_TICKS(KEPLER_TIME_SET_TIMEOUT_MS), &p);

    /* Confirm: one-shot, started when CONFIRM state is entered. */
    Clock_Params_init(&p);
    Clock_construct(&s_confirm_clk, confirm_swi,
                    MS_TO_TICKS(KEPLER_TIME_SET_CONFIRM_MS), &p);
}

bool time_set_handle_button(button_id_t btn, button_event_t evt)
{
    /* When idle, only BTN_2 LONG enters time-set mode. */
    if (s_state == TS_NORMAL) {
        if (btn == BTN_2 && evt == BTN_EVT_LONG) {
            enter_time_set();
            return true;
        }
        return false;
    }

    /* In CONFIRM, absorb all buttons (display hold is timer-driven). */
    if (s_state == TS_CONFIRM) {
        return true;
    }

    /* SET_HOURS / SET_MINUTES — only short presses act; longs are absorbed. */
    if (evt == BTN_EVT_LONG) {
        return true;
    }

    /* Reset inactivity timeout on every button action. */
    reset_timeout();
    s_blink_on = true;  /* snap blink-on so field is visible after press */

    if (s_state == TS_SET_HOURS) {
        if (btn == BTN_1) {
            s_hours = (uint8_t)((s_hours + 1u) % 24u);
        } else if (btn == BTN_3) {
            s_hours = (s_hours == 0u) ? 23u : (uint8_t)(s_hours - 1u);
        } else if (btn == BTN_2) {
            s_state = TS_SET_MINUTES;
        }
    } else {   /* TS_SET_MINUTES */
        if (btn == BTN_1) {
            s_minutes = (uint8_t)((s_minutes + 1u) % 60u);
        } else if (btn == BTN_3) {
            s_minutes = (s_minutes == 0u) ? 59u : (uint8_t)(s_minutes - 1u);
        } else if (btn == BTN_2) {
            /* Transition to CONFIRM: write RTC, stop edit timers. */
            Clock_stop(Clock_handle(&s_blink_clk));
            Clock_stop(Clock_handle(&s_timeout_clk));
            time_set_apply();
            s_state = TS_CONFIRM;
            Clock_setTimeout(Clock_handle(&s_confirm_clk),
                             MS_TO_TICKS(KEPLER_TIME_SET_CONFIRM_MS));
            Clock_start(Clock_handle(&s_confirm_clk));

            /* Show full time (no blanking) during confirm hold. */
            struct tm t;
            make_display_tm(&t);
            ui_update_time_blink(&t, false, false);
            ui_flush();
            return true;
        }
    }

    render_blink();
    ui_flush();
    return true;
}

void time_set_process(void)
{
    if (s_timeout_fired) {
        s_timeout_fired = false;
        cancel_time_set();
        return;
    }

    if (s_confirm_done) {
        s_confirm_done = false;
        s_state        = TS_NORMAL;

        /* Show the newly written RTC time. */
        time_t   now = (time_t)Seconds_get();
        struct tm t;
        gmtime_r(&now, &t);
        ui_update_time(&t);
        ui_flush();
        return;
    }

    if (s_blink_dirty) {
        s_blink_dirty = false;
        if (s_state == TS_SET_HOURS || s_state == TS_SET_MINUTES) {
            render_blink();
            ui_flush();
        }
    }
}

time_set_state_t time_set_get_state(void)  { return s_state; }
bool             time_set_is_active(void)  { return s_state != TS_NORMAL; }

/*--- Test-only clock accessors ------------------------------------------*/

#ifdef KEPLER_TEST_ONLY
Clock_Struct *time_set_test_confirm_clk(void) { return &s_confirm_clk; }
Clock_Struct *time_set_test_timeout_clk(void) { return &s_timeout_clk; }
#endif

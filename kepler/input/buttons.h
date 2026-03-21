/******************************************************************************
 *
 * @file  buttons.h
 *
 * @brief Three-button GPIO driver for the F91 Kepler firmware.
 *
 *        ISRs write press/release edges to a static 8-entry ring buffer.
 *        buttons_process() drains the ring in task context and fires the
 *        registered callback — no event_queue dependency, no malloc.
 *
 *        Feature guard: all hardware code wrapped in #if KEPLER_HAS_BUTTONS.
 *        When 0, all functions compile to empty stubs so callers need no
 *        #if guards of their own.
 *
 *****************************************************************************/

#ifndef BUTTONS_H
#define BUTTONS_H

#include <stdint.h>
#include <stdbool.h>

/*--- Button identifiers --------------------------------------------------*/
typedef enum {
    BTN_1     = 0,   /* TOP       — LIGHT / ACTION              */
    BTN_2     = 1,   /* BTM-LEFT  — SET / TIME-SET (BTN_2 LONG) */
    BTN_3     = 2,   /* BTM-RIGHT — MODE / SCREEN CYCLE         */
    BTN_COUNT = 3,
} button_id_t;

/*--- Event types ---------------------------------------------------------*/
typedef enum {
    BTN_EVT_SHORT,   /* press + release < KEPLER_LONG_PRESS_MS              */
    BTN_EVT_LONG,    /* press held >= KEPLER_LONG_PRESS_MS                  */
} button_event_t;

/*--- Callback ------------------------------------------------------------*/
/* Called from task context (buttons_process), never from ISR/Swi.         */
typedef void (*button_cb_t)(button_id_t btn, button_event_t evt);

/*==========================================================================*
 *  Public API                                                               *
 *==========================================================================*/

/* Open GPIO pins, construct debounce/long-press Clock objects, register    *
 * callback.  Must be called once at startup.                               */
void buttons_init(button_cb_t callback);

/* Drain the ring buffer and invoke the callback for each queued event.     *
 * Call from the main application task (not from an ISR or Swi).           *
 * Returns the number of events dispatched (0 if ring was empty).          */
uint8_t buttons_process(void);

/* Snapshot the current debounced state of a button.                        *
 * Returns true if btn is currently held down.                              */
bool buttons_is_pressed(button_id_t btn);

/*==========================================================================*
 *  Test-only API  (compiled only when KEPLER_TEST_ONLY is defined)         *
 *==========================================================================*/

#ifdef KEPLER_TEST_ONLY
/* Fire the debounce clock for the given button (simulate timer expiry).   */
void buttons_test_fire_debounce(button_id_t btn);
/* Fire the long-press clock for the given button.                         */
void buttons_test_fire_long(button_id_t btn);
#endif /* KEPLER_TEST_ONLY */

#endif /* BUTTONS_H */

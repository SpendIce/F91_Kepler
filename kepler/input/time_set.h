/******************************************************************************
 *
 * @file  time_set.h
 *
 * @brief Manual time-setting state machine for the F91 Kepler firmware.
 *
 *        Entered via BTN_2 LONG from any screen.
 *        State sequence: NORMAL → SET_HOURS → SET_MINUTES → CONFIRM → NORMAL
 *
 *        All timer callbacks (blink, timeout, confirm) only set volatile flags.
 *        time_set_process() acts on those flags in task context.
 *
 *****************************************************************************/

#ifndef TIME_SET_H
#define TIME_SET_H

#include "buttons.h"
#include <stdbool.h>

/*--- State enum ---------------------------------------------------------*/
typedef enum {
    TS_NORMAL,       /* Inactive — button events pass through               */
    TS_SET_HOURS,    /* Editing hours; hours digit blinks                   */
    TS_SET_MINUTES,  /* Editing minutes; minutes digit blinks               */
    TS_CONFIRM,      /* Both digits show for 1.5 s, then write RTC          */
} time_set_state_t;

/*==========================================================================*
 *  Public API                                                               *
 *==========================================================================*/

/* Construct the three Clock objects.  Must be called once before use.     */
void time_set_init(void);

/* Handle a button event.  Returns true if the event was consumed by the   *
 * time-set state machine and should NOT be processed further.             *
 * Returns false when state is TS_NORMAL and the event is not BTN_2 LONG. */
bool time_set_handle_button(button_id_t btn, button_event_t evt);

/* Drive timer-triggered transitions (blink redraw, timeout, confirm done).*
 * Call from the main application task — never from an ISR or Swi.        */
void time_set_process(void);

time_set_state_t time_set_get_state(void);
bool             time_set_is_active(void);   /* true when != TS_NORMAL     */

#endif /* TIME_SET_H */

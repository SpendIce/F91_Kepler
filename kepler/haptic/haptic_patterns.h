/******************************************************************************
 *
 * @file  haptic_patterns.h
 *
 * @brief Named vibration patterns for the F91 Kepler firmware (Task 3).
 *
 *        Patterns map to DRV2605L ERM library-1 effect sequences.
 *        HAPTIC_CALL repeats until haptic_stop() — the 500 ms repeat clock
 *        (Swi context) posts EVT_HAPTIC_TICK; the main task then calls
 *        haptic_tick(), which re-fires the GO bit over I2C in task context.
 *        (Spec 03 says "called by timer ISR" — moved to task context to
 *        honour the no-I2C-in-ISR rule.)
 *
 *        When KEPLER_HAS_DRV2605L == 0 everything is a no-op.
 *
 *****************************************************************************/

#ifndef HAPTIC_PATTERNS_H
#define HAPTIC_PATTERNS_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    HAPTIC_CALL,        /* Incoming call: long buzz, repeating              */
    HAPTIC_MESSAGE,     /* New message: two short pulses                    */
    HAPTIC_ALARM,       /* Alarm: three medium pulses                       */
    HAPTIC_CALENDAR,    /* Calendar reminder: single medium pulse           */
    HAPTIC_CONFIRM,     /* User action confirmed: single short tap          */
    HAPTIC_REJECT,      /* Error / reject: two short pulses, stronger       */
    HAPTIC_STEP_GOAL,   /* Step goal reached: celebratory pattern           */
    HAPTIC_COUNT
} haptic_pattern_t;

/*==========================================================================*
 *  Public API                                                               *
 *==========================================================================*/

/* Construct the repeat clock.  Call once at startup, after drv2605l_init.*/
void haptic_init(void);

/* Play a pattern.  Returns immediately; vibration runs asynchronously.    *
 * If hardware absent (KEPLER_HAS_DRV2605L == 0), this is a no-op.        */
void haptic_play(haptic_pattern_t pattern);

/* Stop any ongoing vibration immediately (also cancels repeating).        */
void haptic_stop(void);

/* Returns true if vibration is currently active.                          */
bool haptic_is_active(void);

/* Advance repeating patterns (e.g. HAPTIC_CALL).  Called by the main     *
 * task on EVT_HAPTIC_TICK — not from application code, not from ISRs.    */
void haptic_tick(void);

#endif /* HAPTIC_PATTERNS_H */

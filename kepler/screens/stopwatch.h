/******************************************************************************
 *
 * @file  stopwatch.h
 *
 * @brief Classic stopwatch with laps (Task 6).
 *
 *        A 10 ms periodic Clock runs ONLY while the stopwatch is RUNNING
 *        (constructed stopped, started/stopped with the state machine) so
 *        idle power is unaffected.  The Swi increments the centisecond
 *        counter (RAM only) and posts EVT_STOPWATCH_TICK every 100 ms;
 *        the main task updates the display only while the STOPWATCH
 *        screen is visible.  Navigating away never stops the timing.
 *
 *        Up to 10 laps in RAM; nothing is persisted (spec: resets on
 *        power cycle).
 *
 *        Note (Plan Maestro R6): the GPT this Clock derives from must be
 *        driven from XOSC, not RCOSC, or the count drifts — verify in the
 *        CCS power policy configuration.
 *
 *****************************************************************************/

#ifndef STOPWATCH_H
#define STOPWATCH_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    STOPWATCH_IDLE    = 0,   /* zeroed, not running                        */
    STOPWATCH_RUNNING = 1,
    STOPWATCH_PAUSED  = 2,   /* stopped with elapsed time on display       */
} stopwatch_state_t;

#define STOPWATCH_LAP_MAX  10u

/* Construct the 10 ms clock (stopped).  Call once at startup.             */
void stopwatch_init(void);

void stopwatch_start(void);
void stopwatch_stop(void);
void stopwatch_lap(void);     /* while running; ignored otherwise          */
void stopwatch_reset(void);   /* while stopped; ignored while running      */

stopwatch_state_t stopwatch_get_state(void);
bool              stopwatch_is_running(void);
uint32_t          stopwatch_elapsed_cs(void);

/* Lap access for the renderer: index 0 = most recent lap.                 *
 * Returns the lap count; fills up to max entries of out[].                */
uint8_t stopwatch_get_laps(uint32_t *out, uint8_t max);

/* Format cs as "MM:SS.cc" (caps at 99:59.99).  len >= 9.                  */
void stopwatch_format(uint32_t cs, char *buf, size_t len);

#ifdef KEPLER_TEST_ONLY
/* Simulate one 10 ms tick of the internal clock.                          */
void stopwatch_test_fire_tick(void);
#endif

#endif /* STOPWATCH_H */

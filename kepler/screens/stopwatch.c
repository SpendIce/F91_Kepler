/******************************************************************************
 *
 * @file  stopwatch.c
 *
 * @brief Stopwatch state machine and 10 ms tick source.
 *
 *****************************************************************************/

#include "stopwatch.h"
#include "../kepler_config.h"

#include <stdio.h>

#include "../power/event_queue.h"

#include <ti/sysbios/knl/Clock.h>

#define MS_TO_TICKS(ms)  ((uint32_t)(ms) * 1000u / Clock_tickPeriod)

/*--- Module state -------------------------------------------------------------*/

static stopwatch_state_t s_state;
static volatile uint32_t s_elapsed_cs;
static uint32_t          s_laps[STOPWATCH_LAP_MAX];  /* [0] = newest        */
static uint8_t           s_lap_count;
static uint32_t          s_last_lap_cs;              /* total at last lap   */

static Clock_Struct      s_tick_clk;

/*--- 10 ms tick (Swi: RAM increment + event post only) ----------------------*/

static void tick_swi(UArg arg)
{
    (void)arg;
    s_elapsed_cs++;
    /* Limit display traffic: one event per 100 ms (spec 06).              */
    if ((s_elapsed_cs % 10u) == 0u) {
        event_queue_post(EVT_STOPWATCH_TICK, s_elapsed_cs, NULL);
    }
}

/*==========================================================================*
 *  Public API                                                               *
 *==========================================================================*/

void stopwatch_init(void)
{
    Clock_Params p;

    s_state       = STOPWATCH_IDLE;
    s_elapsed_cs  = 0u;
    s_lap_count   = 0u;
    s_last_lap_cs = 0u;

    Clock_Params_init(&p);
    p.period = MS_TO_TICKS(10u);   /* periodic 10 ms, constructed stopped  */
    Clock_construct(&s_tick_clk, tick_swi, MS_TO_TICKS(10u), &p);
}

void stopwatch_start(void)
{
    if (s_state == STOPWATCH_RUNNING) { return; }
    s_state = STOPWATCH_RUNNING;
    Clock_start(Clock_handle(&s_tick_clk));
}

void stopwatch_stop(void)
{
    if (s_state != STOPWATCH_RUNNING) { return; }
    Clock_stop(Clock_handle(&s_tick_clk));
    s_state = STOPWATCH_PAUSED;
}

void stopwatch_lap(void)
{
    uint32_t now;
    uint8_t  i;

    if (s_state != STOPWATCH_RUNNING)        { return; }
    if (s_lap_count >= STOPWATCH_LAP_MAX)    { return; }

    now = s_elapsed_cs;

    /* Newest lap at index 0 (spec: most recent on top).                   */
    for (i = s_lap_count; i > 0u; i--) {
        s_laps[i] = s_laps[i - 1u];
    }
    s_laps[0]     = now - s_last_lap_cs;
    s_last_lap_cs = now;
    s_lap_count++;
}

void stopwatch_reset(void)
{
    if (s_state == STOPWATCH_RUNNING) { return; }
    s_state       = STOPWATCH_IDLE;
    s_elapsed_cs  = 0u;
    s_lap_count   = 0u;
    s_last_lap_cs = 0u;
}

stopwatch_state_t stopwatch_get_state(void)
{
    return s_state;
}

bool stopwatch_is_running(void)
{
    return (s_state == STOPWATCH_RUNNING);
}

uint32_t stopwatch_elapsed_cs(void)
{
    return s_elapsed_cs;
}

uint8_t stopwatch_get_laps(uint32_t *out, uint8_t max)
{
    uint8_t n = (s_lap_count < max) ? s_lap_count : max;

    if (out != NULL) {
        for (uint8_t i = 0u; i < n; i++) {
            out[i] = s_laps[i];
        }
    }
    return s_lap_count;
}

#ifdef KEPLER_TEST_ONLY
void stopwatch_test_fire_tick(void)
{
    tick_swi(0u);
}
#endif /* KEPLER_TEST_ONLY */

void stopwatch_format(uint32_t cs, char *buf, size_t len)
{
    uint32_t centis  = cs % 100u;
    uint32_t seconds = (cs / 100u) % 60u;
    uint32_t minutes = cs / 6000u;

    if (minutes > 99u) { minutes = 99u; }   /* display cap 99:59.99        */

    (void)snprintf(buf, len, "%02lu:%02lu.%02lu",
                   (unsigned long)minutes,
                   (unsigned long)seconds,
                   (unsigned long)centis);
}

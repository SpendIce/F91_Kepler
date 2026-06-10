/******************************************************************************
 *
 * @file  haptic_patterns.c
 *
 * @brief Pattern tables and repeat logic for the DRV2605L driver.
 *
 *        Effect IDs from DRV2605L datasheet Table 2 (ERM library 1):
 *          1  Strong Click 100%        47 Long buzz (programmatic stop)
 *          4  Sharp Click 100%         48 Smooth Hum 1 (50%)
 *          10 Double Click 60%         52 Short Double Click Strong (80%)
 *          12 Triple Click 60%         58 Transition Ramp Down Long Smooth
 *
 *****************************************************************************/

#include "haptic_patterns.h"
#include "../kepler_config.h"

#if KEPLER_HAS_DRV2605L

#include <stddef.h>

#include "drv2605l.h"
#include "../power/event_queue.h"

#include <ti/sysbios/knl/Clock.h>

/*--- Repeat interval for HAPTIC_CALL --------------------------------------*/
#define HAPTIC_REPEAT_MS  500u

#define MS_TO_TICKS(ms)   ((uint32_t)(ms) * 1000u / Clock_tickPeriod)

/*--- Pattern table ----------------------------------------------------------*
 *  Sequences are zero-terminated lists of library effect IDs (max 8).     *
 *--------------------------------------------------------------------------*/

typedef struct {
    const uint8_t *seq;     /* zero-terminated effect ID list               */
    uint8_t        len;     /* entries incl. terminator                     */
    bool           repeat;  /* re-trigger every HAPTIC_REPEAT_MS            */
} haptic_def_t;

static const uint8_t SEQ_CALL[]      = { 47, 0 };          /* long buzz    */
static const uint8_t SEQ_MESSAGE[]   = { 52, 52, 0 };      /* double click */
static const uint8_t SEQ_ALARM[]     = { 12, 12, 12, 0 };  /* triple click */
static const uint8_t SEQ_CALENDAR[]  = { 10, 0 };          /* double 60%   */
static const uint8_t SEQ_CONFIRM[]   = { 1, 0 };           /* strong click */
static const uint8_t SEQ_REJECT[]    = { 4, 4, 0 };        /* two sharp    */
static const uint8_t SEQ_STEP_GOAL[] = { 58, 48, 0 };      /* ramp + hum   */

static const haptic_def_t k_patterns[HAPTIC_COUNT] = {
    [HAPTIC_CALL]      = { SEQ_CALL,      sizeof(SEQ_CALL),      true  },
    [HAPTIC_MESSAGE]   = { SEQ_MESSAGE,   sizeof(SEQ_MESSAGE),   false },
    [HAPTIC_ALARM]     = { SEQ_ALARM,     sizeof(SEQ_ALARM),     false },
    [HAPTIC_CALENDAR]  = { SEQ_CALENDAR,  sizeof(SEQ_CALENDAR),  false },
    [HAPTIC_CONFIRM]   = { SEQ_CONFIRM,   sizeof(SEQ_CONFIRM),   false },
    [HAPTIC_REJECT]    = { SEQ_REJECT,    sizeof(SEQ_REJECT),    false },
    [HAPTIC_STEP_GOAL] = { SEQ_STEP_GOAL, sizeof(SEQ_STEP_GOAL), false },
};

/*--- Module state -------------------------------------------------------------*/

static Clock_Struct  s_repeat_clk;
static volatile bool s_repeating;   /* HAPTIC_CALL active                   */
static bool          s_active;      /* any pattern fired, not yet stopped   */

/*--- Repeat clock (Swi context: post event only, no I2C) -------------------*/

static void repeat_swi(UArg arg)
{
    (void)arg;
    if (s_repeating) {
        event_queue_post(EVT_HAPTIC_TICK, 0u, NULL);
    }
}

/*==========================================================================*
 *  Public API                                                               *
 *==========================================================================*/

void haptic_init(void)
{
    Clock_Params p;

    Clock_Params_init(&p);
    p.period = MS_TO_TICKS(HAPTIC_REPEAT_MS);   /* periodic                */
    Clock_construct(&s_repeat_clk, repeat_swi,
                    MS_TO_TICKS(HAPTIC_REPEAT_MS), &p);

    s_repeating = false;
    s_active    = false;
}

void haptic_play(haptic_pattern_t pattern)
{
    const haptic_def_t *def;

    if (pattern >= HAPTIC_COUNT) { return; }
    def = &k_patterns[pattern];

    if (!drv2605l_play_sequence(def->seq, def->len)) { return; }

    s_active = true;

    if (def->repeat) {
        s_repeating = true;
        Clock_start(Clock_handle(&s_repeat_clk));
    } else if (s_repeating) {
        /* A non-repeating pattern interrupts a repeating one.             */
        s_repeating = false;
        Clock_stop(Clock_handle(&s_repeat_clk));
    }
}

void haptic_stop(void)
{
    s_repeating = false;
    s_active    = false;
    Clock_stop(Clock_handle(&s_repeat_clk));
    (void)drv2605l_stop();
}

bool haptic_is_active(void)
{
    if (s_repeating) { return true; }
    if (!s_active)   { return false; }
    return drv2605l_is_playing();
}

void haptic_tick(void)
{
    if (s_repeating) {
        (void)drv2605l_retrigger();
    }
}

#else /* !KEPLER_HAS_DRV2605L */

void haptic_init(void)                    {}
void haptic_play(haptic_pattern_t p)      { (void)p; }
void haptic_stop(void)                    {}
bool haptic_is_active(void)               { return false; }
void haptic_tick(void)                    {}

#endif /* KEPLER_HAS_DRV2605L */

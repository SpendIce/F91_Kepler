/******************************************************************************
 *
 * @file  buzzer.c
 *
 * @brief PWM buzzer sequencer implementation.
 *
 *        Sequences are arrays of buzzer_step_t; freq 0 = silent gap;
 *        duration 0 terminates.  The 3.2 kHz default sits inside the
 *        2.7-4.0 kHz resonance band of watch-sized piezos — sweep during
 *        bring-up for the loudest spot (spec 05, Task 8).
 *
 *****************************************************************************/

#include "buzzer.h"
#include "../kepler_config.h"

#if KEPLER_HAS_BUZZER

#include <stddef.h>

#include "../power/event_queue.h"

#include <ti/drivers/PWM.h>
#include <ti/sysbios/knl/Clock.h>

#define MS_TO_TICKS(ms)  ((uint32_t)(ms) * 1000u / Clock_tickPeriod)

/*--- Sequence step ----------------------------------------------------------*/

typedef struct {
    uint16_t freq_hz;       /* 0 = silence                                 */
    uint16_t duration_ms;   /* 0 = end of sequence                         */
} buzzer_step_t;

/*--- Named sequences ---------------------------------------------------------*/

static const buzzer_step_t SEQ_ALARM[] = {     /* beep-beep ... (loops)    */
    { 3200, 120 }, { 0, 80 }, { 3200, 120 }, { 0, 680 }, { 0, 0 }
};
static const buzzer_step_t SEQ_CHIME[] = {     /* single short beep        */
    { 3200, 60 }, { 0, 0 }
};
static const buzzer_step_t SEQ_NOTIFY[] = {    /* two-tone                 */
    { 2800, 70 }, { 3600, 70 }, { 0, 0 }
};

/*--- Module state -------------------------------------------------------------*/

static PWM_Handle    s_pwm;
static Clock_Struct  s_step_clk;
static bool          s_active;
static bool          s_loop;            /* restart sequence at end          */
static const buzzer_step_t *s_seq;
static uint8_t       s_step;
static buzzer_step_t s_single[2];       /* storage for buzzer_tone()        */
static volatile bool s_tick_pending;

/*--- Step clock callback (Swi: post only) ----------------------------------*/

static void step_swi(UArg arg)
{
    (void)arg;
    if (!s_tick_pending) {
        s_tick_pending = true;
        event_queue_post(EVT_BUZZER_TICK, 0u, NULL);
    }
}

/*--- PWM helpers (task context) ---------------------------------------------*/

static void pwm_output(uint16_t freq_hz)
{
    if (s_pwm == NULL) { return; }

    if (freq_hz == 0u) {
        PWM_stop(s_pwm);
        return;
    }
    PWM_stop(s_pwm);
    PWM_setPeriod(s_pwm, PWM_PERIOD_HZ, freq_hz);
    PWM_setDuty(s_pwm, PWM_DUTY_FRACTION, PWM_DUTY_FRACTION_MAX / 2u);
    PWM_start(s_pwm);
}

/* Output the current step and arm the clock for its duration.             */
static void run_step(void)
{
    const buzzer_step_t *st = &s_seq[s_step];

    if (st->duration_ms == 0u) {
        if (s_loop) {
            s_step = 0u;
            st     = &s_seq[0];
        } else {
            buzzer_stop();
            return;
        }
    }

    pwm_output(st->freq_hz);
    Clock_setTimeout(Clock_handle(&s_step_clk),
                     MS_TO_TICKS(st->duration_ms));
    Clock_start(Clock_handle(&s_step_clk));
}

static void start_sequence(const buzzer_step_t *seq, bool loop)
{
    Clock_stop(Clock_handle(&s_step_clk));
    s_seq          = seq;
    s_step         = 0u;
    s_loop         = loop;
    s_active       = true;
    s_tick_pending = false;
    run_step();
}

/*==========================================================================*
 *  Public API                                                               *
 *==========================================================================*/

void buzzer_init(void)
{
    PWM_Params   params;
    Clock_Params cp;

    PWM_init();
    PWM_Params_init(&params);
    params.dutyUnits   = PWM_DUTY_FRACTION;
    params.dutyValue   = 0u;
    params.periodUnits = PWM_PERIOD_HZ;
    params.periodValue = 3200u;
    s_pwm = PWM_open(KEPLER_BUZZER_PWM_INDEX, &params);

    Clock_Params_init(&cp);
    Clock_construct(&s_step_clk, step_swi, MS_TO_TICKS(100u), &cp);

    s_active = false;
    s_seq    = NULL;
}

void buzzer_tone(uint16_t freq_hz, uint16_t duration_ms)
{
    if (freq_hz == 0u || duration_ms == 0u) { return; }

    s_single[0].freq_hz     = freq_hz;
    s_single[0].duration_ms = duration_ms;
    s_single[1].freq_hz     = 0u;
    s_single[1].duration_ms = 0u;

    start_sequence(s_single, false);
}

void buzzer_stop(void)
{
    Clock_stop(Clock_handle(&s_step_clk));
    pwm_output(0u);
    s_active       = false;
    s_loop         = false;
    s_seq          = NULL;
    s_tick_pending = false;
}

void buzzer_alarm(void)      { start_sequence(SEQ_ALARM,  true);  }
void buzzer_hour_chime(void) { start_sequence(SEQ_CHIME,  false); }
void buzzer_notify(void)     { start_sequence(SEQ_NOTIFY, false); }

void buzzer_tick(void)
{
    s_tick_pending = false;
    if (!s_active || s_seq == NULL) { return; }
    s_step++;
    run_step();
}

bool buzzer_is_active(void)
{
    return s_active;
}

/*--- Test-only helpers ---------------------------------------------------*/

#ifdef KEPLER_TEST_ONLY
void buzzer_test_fire_step(void)
{
    step_swi(0u);
}
#endif /* KEPLER_TEST_ONLY */

#else /* !KEPLER_HAS_BUZZER */

void buzzer_init(void)                              {}
void buzzer_tone(uint16_t f, uint16_t d)            { (void)f; (void)d; }
void buzzer_stop(void)                              {}
void buzzer_alarm(void)                             {}
void buzzer_hour_chime(void)                        {}
void buzzer_notify(void)                            {}
void buzzer_tick(void)                              {}
bool buzzer_is_active(void)                         { return false; }

#endif /* KEPLER_HAS_BUZZER */

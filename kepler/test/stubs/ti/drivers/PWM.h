#ifndef STUB_PWM_H
#define STUB_PWM_H

#include <stdint.h>
#include <stddef.h>

/* PWM stub recording start/stop/period for host tests.                   */

typedef struct {
    int      started;
    uint32_t period_hz;
    uint32_t duty;
    int      start_count;
    int      stop_count;
} mock_pwm_t;

extern mock_pwm_t mock_pwm;

typedef mock_pwm_t *PWM_Handle;

typedef enum { PWM_PERIOD_HZ = 0, PWM_PERIOD_US = 1 } PWM_Period_Units;
typedef enum { PWM_DUTY_FRACTION = 0, PWM_DUTY_US = 1 } PWM_Duty_Units;

#define PWM_DUTY_FRACTION_MAX  0xFFFFFFFFu

typedef struct {
    PWM_Duty_Units   dutyUnits;
    uint32_t         dutyValue;
    PWM_Period_Units periodUnits;
    uint32_t         periodValue;
} PWM_Params;

static inline void PWM_init(void) {}

static inline void PWM_Params_init(PWM_Params *p) {
    p->dutyUnits   = PWM_DUTY_FRACTION;
    p->dutyValue   = 0u;
    p->periodUnits = PWM_PERIOD_HZ;
    p->periodValue = 0u;
}

static inline PWM_Handle PWM_open(unsigned idx, PWM_Params *p) {
    (void)idx;
    mock_pwm.period_hz = p ? p->periodValue : 0u;
    return &mock_pwm;
}

static inline void PWM_start(PWM_Handle h) {
    if (h) { h->started = 1; h->start_count++; }
}

static inline void PWM_stop(PWM_Handle h) {
    if (h) { h->started = 0; h->stop_count++; }
}

static inline void PWM_setPeriod(PWM_Handle h, PWM_Period_Units u, uint32_t v) {
    (void)u;
    if (h) { h->period_hz = v; }
}

static inline void PWM_setDuty(PWM_Handle h, PWM_Duty_Units u, uint32_t v) {
    (void)u;
    if (h) { h->duty = v; }
}

#endif /* STUB_PWM_H */

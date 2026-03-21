#ifndef STUB_UTIL_H
#define STUB_UTIL_H

/*
 * Thin wrappers matching the TI SDK util.h / Util_* pattern used by the
 * original Kepler firmware.  Delegates to Clock stub functions.
 */

#include <ti/sysbios/knl/Clock.h>

static inline void Util_constructClock(Clock_Struct *s, Clock_FuncPtr fn,
                                       uint32_t period_ms, uint32_t timeout_ms,
                                       int start, UArg arg)
{
    Clock_Params p;
    Clock_Params_init(&p);
    p.period    = (period_ms > 0u) ? period_ms * 1000u / Clock_tickPeriod : 0u;
    p.startFlag = start;
    p.arg       = arg;
    Clock_construct(s, fn, timeout_ms * 1000u / Clock_tickPeriod, &p);
}

static inline void Util_restartClock(Clock_Struct *s, uint32_t timeout_ms)
{
    Clock_stop(Clock_handle(s));
    Clock_setTimeout(Clock_handle(s), timeout_ms * 1000u / Clock_tickPeriod);
    Clock_start(Clock_handle(s));
}

static inline void Util_stopClock(Clock_Struct *s)
{
    Clock_stop(Clock_handle(s));
}

#endif /* STUB_UTIL_H */

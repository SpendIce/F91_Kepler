#ifndef STUB_CLOCK_H
#define STUB_CLOCK_H

#include <stdint.h>
#include <stddef.h>

typedef uint32_t UArg;
typedef void (*Clock_FuncPtr)(UArg);

#define TRUE  1
#define FALSE 0

/* Clock_Struct stores fn+arg so mock_clock_fire can invoke the callback.  */
typedef struct {
    Clock_FuncPtr fn;
    UArg          arg;
    uint32_t      timeout_ticks;
    int           started;
    int           periodic;
} Clock_Struct;

typedef Clock_Struct *Clock_Handle;

typedef struct {
    uint32_t period;
    int      startFlag;
    UArg     arg;
} Clock_Params;

/* Ticks per microsecond — standard TI-RTOS 10 us tick */
static const uint32_t Clock_tickPeriod = 10u;

static inline void Clock_Params_init(Clock_Params *p) {
    p->period    = 0;
    p->startFlag = FALSE;
    p->arg       = 0u;
}

static inline Clock_Handle Clock_handle(Clock_Struct *s) { return s; }

static inline void Clock_construct(Clock_Struct *s, Clock_FuncPtr fn,
                                   uint32_t timeout, Clock_Params *p) {
    s->fn            = fn;
    s->arg           = (p != NULL) ? p->arg : 0u;
    s->timeout_ticks = timeout;
    s->started       = (p != NULL) ? p->startFlag : FALSE;
    s->periodic      = (p != NULL && p->period > 0u) ? TRUE : FALSE;
}

static inline void Clock_start(Clock_Handle h) {
    if (h) { h->started = TRUE; }
}

static inline void Clock_stop(Clock_Handle h) {
    if (h) { h->started = FALSE; }
}

static inline void Clock_setTimeout(Clock_Handle h, uint32_t ticks) {
    if (h) { h->timeout_ticks = ticks; }
}

/* Test helper: fire callback only if clock is started.                    */
static inline void mock_clock_fire(Clock_Handle h) {
    if (h && h->started && h->fn) { h->fn(h->arg); }
}

/* Test helper: fire callback unconditionally (ignores started flag).      */
static inline void mock_clock_fire_force(Clock_Handle h) {
    if (h && h->fn) { h->fn(h->arg); }
}

#endif /* STUB_CLOCK_H */

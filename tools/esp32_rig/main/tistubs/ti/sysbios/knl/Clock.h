/******************************************************************************
 * TI-RTOS Clock shim -> esp_timer (rig).
 *
 * Semantics preserved: tick = 10 us, one-shot vs periodic from
 * Clock_Params.period, callbacks fire in the esp_timer task ("Swi"
 * context for the kepler code — all kepler Swi callbacks are post-only
 * or RAM-only, so a task context is strictly safer).
 *****************************************************************************/

#ifndef RIGSHIM_CLOCK_H
#define RIGSHIM_CLOCK_H

#include <stdint.h>
#include <stddef.h>

typedef uint32_t UArg;
typedef void (*Clock_FuncPtr)(UArg);

#ifndef TRUE
#define TRUE  1
#define FALSE 0
#endif

/* 10 us per tick — matches the TI-RTOS default the kepler code assumes.  */
static const uint32_t Clock_tickPeriod = 10u;

typedef struct {
    Clock_FuncPtr fn;
    UArg          arg;
    uint32_t      timeout_ticks;
    uint32_t      period_ticks;     /* 0 = one-shot                        */
    void         *esp_handle;       /* esp_timer_handle_t                  */
    int           constructed;
} Clock_Struct;

typedef Clock_Struct *Clock_Handle;

typedef struct {
    uint32_t period;
    int      startFlag;
    UArg     arg;
} Clock_Params;

void         Clock_Params_init(Clock_Params *p);
void         Clock_construct(Clock_Struct *s, Clock_FuncPtr fn,
                             uint32_t timeout, Clock_Params *p);
Clock_Handle Clock_handle(Clock_Struct *s);
void         Clock_start(Clock_Handle h);
void         Clock_stop(Clock_Handle h);
void         Clock_setTimeout(Clock_Handle h, uint32_t ticks);

#endif /* RIGSHIM_CLOCK_H */

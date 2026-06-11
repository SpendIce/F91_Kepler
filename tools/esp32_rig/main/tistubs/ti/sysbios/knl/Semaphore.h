/******************************************************************************
 * TI-RTOS Semaphore shim -> FreeRTOS counting semaphore (rig).
 * Pend timeout arrives in 10 us ticks; 0xFFFFFFFF = wait forever.
 *****************************************************************************/

#ifndef RIGSHIM_SEMAPHORE_H
#define RIGSHIM_SEMAPHORE_H

#include <stdint.h>

typedef struct {
    void *freertos_sem;    /* SemaphoreHandle_t                            */
} Semaphore_Struct;

typedef Semaphore_Struct *Semaphore_Handle;

typedef struct { int mode; } Semaphore_Params;

#define Semaphore_Mode_COUNTING  1

void             Semaphore_Params_init(Semaphore_Params *p);
void             Semaphore_construct(Semaphore_Struct *s, int count,
                                     Semaphore_Params *p);
Semaphore_Handle Semaphore_handle(Semaphore_Struct *s);
void             Semaphore_post(Semaphore_Handle h);
int              Semaphore_pend(Semaphore_Handle h, uint32_t timeout_ticks);

#endif /* RIGSHIM_SEMAPHORE_H */

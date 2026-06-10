/******************************************************************************
 *
 * @file  event_queue.c
 *
 * @brief Static ring buffer + counting semaphore event queue.
 *
 *        Producers may run in Hwi, Swi or Task context, so the ring index
 *        update is wrapped in Hwi_disable()/Hwi_restore().  The consumer
 *        (main task) blocks on the semaphore; TI-RTOS drops the CPU into
 *        STANDBY while every task is pended, which is the Phase 0 power
 *        model — no explicit sleep calls needed here.
 *
 *****************************************************************************/

#include "event_queue.h"

#include <stddef.h>

#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/hal/Hwi.h>
#include <ti/sysbios/knl/Clock.h>

/*--- Ring storage ---------------------------------------------------------*/

static kepler_event_msg_t s_ring[KEPLER_EVENT_QUEUE_SIZE];
static volatile uint8_t   s_head;     /* next write slot                    */
static volatile uint8_t   s_tail;     /* next read slot                     */
static volatile uint32_t  s_dropped;

#define RING_MASK  (KEPLER_EVENT_QUEUE_SIZE - 1u)

/*--- Counting semaphore ---------------------------------------------------*/

static Semaphore_Struct s_sem_struct;
static Semaphore_Handle s_sem;

/*--- Timing helper ---------------------------------------------------------*/

#define MS_TO_TICKS(ms)  ((uint32_t)(ms) * 1000u / Clock_tickPeriod)

/*==========================================================================*
 *  Public API                                                               *
 *==========================================================================*/

void event_queue_init(void)
{
    Semaphore_Params sp;

    s_head    = 0u;
    s_tail    = 0u;
    s_dropped = 0u;

    Semaphore_Params_init(&sp);
    sp.mode = Semaphore_Mode_COUNTING;
    Semaphore_construct(&s_sem_struct, 0, &sp);
    s_sem = Semaphore_handle(&s_sem_struct);
}

void event_queue_post(kepler_event_t type, uint32_t param, void *data)
{
    uint32_t key = Hwi_disable();

    uint8_t next = (uint8_t)((s_head + 1u) & RING_MASK);
    if (next == s_tail) {
        /* Full: drop newest, keep oldest (matches buttons.c ring policy). */
        s_dropped++;
        Hwi_restore(key);
        return;
    }

    s_ring[s_head].type  = type;
    s_ring[s_head].param = param;
    s_ring[s_head].data  = data;
    s_head               = next;

    Hwi_restore(key);

    Semaphore_post(s_sem);
}

bool event_queue_pend(kepler_event_msg_t *out, uint32_t timeout_ms)
{
    uint32_t ticks;
    bool     got;

    if (out == NULL) { return false; }

    ticks = (timeout_ms == EVENT_QUEUE_WAIT_FOREVER)
          ? BIOS_WAIT_FOREVER
          : MS_TO_TICKS(timeout_ms);

    got = (bool)Semaphore_pend(s_sem, ticks);
    if (!got) { return false; }

    {
        uint32_t key = Hwi_disable();
        if (s_tail == s_head) {
            /* Semaphore/ring mismatch should not happen; fail safe.        */
            Hwi_restore(key);
            return false;
        }
        *out   = s_ring[s_tail];
        s_tail = (uint8_t)((s_tail + 1u) & RING_MASK);
        Hwi_restore(key);
    }
    return true;
}

uint32_t event_queue_dropped(void)
{
    return s_dropped;
}

uint8_t event_queue_depth(void)
{
    uint32_t key   = Hwi_disable();
    uint8_t  depth = (uint8_t)((s_head - s_tail) & RING_MASK);
    Hwi_restore(key);
    return depth;
}

/******************************************************************************
 *
 * @file  wrist_raise.c
 *
 * @brief INT1 GPIO interrupt -> EVT_WRIST_RAISE.
 *
 *        The LIS2DW12 latches INT1 (LIR set) until WAKE_UP_SRC is read,
 *        so a rising-edge interrupt cannot retrigger before the main task
 *        services the event — natural debouncing, no Clock needed here.
 *
 *****************************************************************************/

#include "wrist_raise.h"
#include "../kepler_config.h"

#if KEPLER_WRIST_RAISE

#include <stddef.h>

#include "../power/event_queue.h"

#include <ti/drivers/PIN.h>

static PIN_Handle s_pin_handle;
static PIN_State  s_pin_state;

static PIN_Config s_pin_table[2];

/*--- GPIO ISR (Hwi context — post event only) -----------------------------*/

static void int1_isr(PIN_Handle handle, PIN_Id pinId)
{
    (void)handle;
    (void)pinId;
    event_queue_post(EVT_WRIST_RAISE, 0u, NULL);
}

/*==========================================================================*
 *  Public API                                                               *
 *==========================================================================*/

bool wrist_raise_init(void)
{
    if (KEPLER_LIS2DW12_INT1_PIN == IOID_UNUSED) {
        return false;   /* Tier 3: pin not routed until v2 PCB             */
    }

    /* INT1 is push-pull active-high out of the LIS2DW12: pull-down,       *
     * interrupt on rising edge.                                           */
    s_pin_table[0] = (PIN_Config)KEPLER_LIS2DW12_INT1_PIN
                   | PIN_INPUT_EN
                   | PIN_PULLDOWN
                   | PIN_IRQ_POSEDGE;
    s_pin_table[1] = PIN_TERMINATE;

    s_pin_handle = PIN_open(&s_pin_state, s_pin_table);
    if (s_pin_handle == NULL) { return false; }

    PIN_registerIntCb(s_pin_handle, int1_isr);
    return true;
}

#else /* !KEPLER_WRIST_RAISE */

bool wrist_raise_init(void) { return false; }

#endif /* KEPLER_WRIST_RAISE */

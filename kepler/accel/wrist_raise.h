/******************************************************************************
 *
 * @file  wrist_raise.h
 *
 * @brief Wrist-raise interrupt handling (Task 4).
 *
 *        The LIS2DW12 wake-up function (configured by lis2dw12_init)
 *        asserts INT1 on a >0.5 g threshold crossing.  The GPIO ISR here
 *        does exactly one thing: post EVT_WRIST_RAISE.  The main task
 *        clears the latch (lis2dw12_clear_wakeup_src) and decides between
 *        screen switch (daytime) and actigraphy logging (sleep window).
 *
 *        Feature guard: KEPLER_WRIST_RAISE.  Also a no-op when
 *        KEPLER_LIS2DW12_INT1_PIN is IOID_UNUSED (pin not routed yet —
 *        Tier 3 until the v2 PCB assigns it).
 *
 *****************************************************************************/

#ifndef WRIST_RAISE_H
#define WRIST_RAISE_H

#include <stdbool.h>

/* Open the INT1 GPIO with a rising-edge interrupt.  Returns true if the   *
 * pin was configured (false when feature off or pin unrouted).            */
bool wrist_raise_init(void);

#endif /* WRIST_RAISE_H */

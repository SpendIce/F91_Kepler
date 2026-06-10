/******************************************************************************
 *
 * @file  power_manager.c
 *
 * @brief Inactivity timer + battery monitor implementation.
 *
 *****************************************************************************/

#include "power_manager.h"
#include "../kepler_config.h"

#include <stddef.h>

#include "event_queue.h"

#include <ti/sysbios/knl/Clock.h>

#define MS_TO_TICKS(ms)  ((uint32_t)(ms) * 1000u / Clock_tickPeriod)

/*--- Module state -------------------------------------------------------------*/

static power_state_t s_state;
static Clock_Struct  s_inactivity_clk;

/*--- Inactivity clock (Swi: post only) --------------------------------------*/

static void inactivity_swi(UArg arg)
{
    (void)arg;
    event_queue_post(EVT_SCREEN_TIMEOUT, 0u, NULL);
}

/*==========================================================================*
 *  Public API                                                               *
 *==========================================================================*/

void power_manager_init(void)
{
    Clock_Params p;

    s_state = POWER_IDLE;

    Clock_Params_init(&p);
    Clock_construct(&s_inactivity_clk, inactivity_swi,
                    MS_TO_TICKS(KEPLER_SCREEN_TIMEOUT_MS), &p);
}

void activity_timer_reset(void)
{
    Clock_stop(Clock_handle(&s_inactivity_clk));
    Clock_setTimeout(Clock_handle(&s_inactivity_clk),
                     MS_TO_TICKS(KEPLER_SCREEN_TIMEOUT_MS));
    Clock_start(Clock_handle(&s_inactivity_clk));
}

void activity_timer_stop(void)
{
    Clock_stop(Clock_handle(&s_inactivity_clk));
}

void power_manager_set_state(power_state_t st)
{
    s_state = st;
}

power_state_t power_manager_get_state(void)
{
    return s_state;
}

/*--- Battery ------------------------------------------------------------------*
 *  AON BatMon reports VDDS in unsigned 8.8 fixed-point volts.  LiPo        *
 *  discharge window mapped linearly 3.0 V -> 0 %, 4.2 V -> 100 %.  Linear  *
 *  is a coarse fit for LiPo but good enough for a status row; refine with  *
 *  a lookup table during Phase 2 battery bring-up.                          *
 *----------------------------------------------------------------------------*/

#ifndef KEPLER_TEST_ONLY
  #if KEPLER_HAS_INDUCTIVE_CHG
    #include <driverlib/aon_batmon.h>
    #define HAVE_BATMON 1
  #else
    #define HAVE_BATMON 0
  #endif
#else
  #define HAVE_BATMON 0
#endif

uint8_t power_manager_battery_pct(void)
{
#if HAVE_BATMON
    /* 8.8 fixed point -> millivolts */
    uint32_t raw = AONBatMonBatteryVoltageGet();
    uint32_t mv  = (raw * 1000u) >> 8;

    if (mv <= 3000u) { return 0u;   }
    if (mv >= 4200u) { return 100u; }
    return (uint8_t)((mv - 3000u) * 100u / 1200u);
#else
    /* Dev board / coin cell v1: no usable fuel gauge — report full.        */
    return 100u;
#endif
}

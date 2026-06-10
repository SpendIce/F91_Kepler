/******************************************************************************
 *
 * @file  locator_service.c
 *
 * @brief Phone locator implementation.
 *
 *        The auto-stop clock and the 1 Hz blink clock run in Swi context
 *        and only post events / flip a UI flag (ui_finder_blink_tick is
 *        RAM-only framebuffer work, same class as the button Swi's).
 *
 *****************************************************************************/

#include "locator_service.h"
#include "../kepler_config.h"

#include <stddef.h>

#include "gatt_uuids.h"
#include "ble_manager.h"
#include "../power/event_queue.h"
#include "../display/ui_renderer.h"

#include <ti/sysbios/knl/Clock.h>

#define MS_TO_TICKS(ms)  ((uint32_t)(ms) * 1000u / Clock_tickPeriod)

/*--- Module state -------------------------------------------------------------*/

static bool         s_ringing;
static Clock_Struct s_auto_stop_clk;
static Clock_Struct s_blink_clk;

/*--- Clock callbacks (Swi) ----------------------------------------------------*/

static void auto_stop_swi(UArg arg)
{
    (void)arg;
    event_queue_post(EVT_PHONE_LOCATOR_STOP, 0u, NULL);
}

static void blink_swi(UArg arg)
{
    (void)arg;
    ui_finder_blink_tick();   /* framebuffer only; flushed by main loop    */
}

/*==========================================================================*
 *  Public API                                                               *
 *==========================================================================*/

void locator_service_init(void)
{
    Clock_Params p;

    s_ringing = false;

    Clock_Params_init(&p);
    Clock_construct(&s_auto_stop_clk, auto_stop_swi,
                    MS_TO_TICKS(KEPLER_LOCATOR_AUTO_STOP_SEC * 1000u), &p);

    Clock_Params_init(&p);
    p.period = MS_TO_TICKS(500u);      /* spec 06: toggle every 500 ms     */
    Clock_construct(&s_blink_clk, blink_swi, MS_TO_TICKS(500u), &p);
}

void locator_service_start(void)
{
    static const uint8_t cmd_start = 0x01u;

    if (!ble_manager_is_connected()) { return; }

    s_ringing = true;
    ble_manager_notify(KEPLER_CHAR_LOCATOR, &cmd_start, 1u);

    Clock_setTimeout(Clock_handle(&s_auto_stop_clk),
                     MS_TO_TICKS(KEPLER_LOCATOR_AUTO_STOP_SEC * 1000u));
    Clock_start(Clock_handle(&s_auto_stop_clk));
    Clock_start(Clock_handle(&s_blink_clk));

    ui_set_finder_state(FINDER_RINGING);
}

void locator_service_stop(void)
{
    static const uint8_t cmd_stop = 0x00u;

    if (!s_ringing) { return; }

    s_ringing = false;
    ble_manager_notify(KEPLER_CHAR_LOCATOR, &cmd_stop, 1u);

    Clock_stop(Clock_handle(&s_auto_stop_clk));
    Clock_stop(Clock_handle(&s_blink_clk));

    ui_set_finder_state(FINDER_IDLE);
}

bool locator_service_on_write(const uint8_t *data, uint16_t len)
{
    if (data == NULL || len < 1u) { return false; }

    if (data[0] == 0x01u) {
        event_queue_post(EVT_PHONE_LOCATOR_ACK, 0u, NULL);
        return true;
    }
    return false;
}

bool locator_service_is_ringing(void)
{
    return s_ringing;
}

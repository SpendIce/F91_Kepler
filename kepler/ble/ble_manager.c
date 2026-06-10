/******************************************************************************
 *
 * @file  ble_manager.c
 *
 * @brief Advertising-interval state machine.
 *
 *        The fast->slow demotion clock runs in Swi context and only sets
 *        a pending flag; kepler_main calls ble_manager_adv_window_expired()
 *        from task context (GAP parameter updates go through ICall and
 *        must not run in a Swi).
 *
 *****************************************************************************/

#include "ble_manager.h"
#include "../kepler_config.h"

#include <stddef.h>

#include "../power/event_queue.h"

#include <ti/sysbios/knl/Clock.h>

#define MS_TO_TICKS(ms)  ((uint32_t)(ms) * 1000u / Clock_tickPeriod)

/*--- Module state -------------------------------------------------------------*/

static const ble_port_t *s_port;
static ble_state_t       s_state;
static Clock_Struct      s_window_clk;
static volatile bool     s_window_expired;

/*--- Window clock (Swi: flag only) ------------------------------------------*/

static void window_swi(UArg arg)
{
    (void)arg;
    s_window_expired = true;
    event_queue_post(EVT_BLE_ADV_WINDOW, 0u, NULL);
}

/*--- Helpers ----------------------------------------------------------------*/

uint16_t ble_manager_ms_to_units(uint16_t ms)
{
    /* 0.625 ms units: ms * 8 / 5 */
    return (uint16_t)(((uint32_t)ms * 8u) / 5u);
}

static void start_adv(bool fast)
{
    uint16_t units = ble_manager_ms_to_units(
        fast ? KEPLER_BLE_ADV_INTERVAL_FAST_MS
             : KEPLER_BLE_ADV_INTERVAL_SLOW_MS);

    s_state = fast ? BLE_STATE_ADVERTISING_FAST
                   : BLE_STATE_ADVERTISING_SLOW;

    if (s_port != NULL && s_port->start_advertising != NULL) {
        s_port->start_advertising(units);
    }

    if (fast) {
        Clock_setTimeout(Clock_handle(&s_window_clk),
                         MS_TO_TICKS(KEPLER_BLE_ADV_FAST_DURATION_MS));
        Clock_start(Clock_handle(&s_window_clk));
    } else {
        Clock_stop(Clock_handle(&s_window_clk));
    }
}

/*==========================================================================*
 *  Public API                                                               *
 *==========================================================================*/

void ble_manager_init(const ble_port_t *port)
{
    Clock_Params p;

    s_port           = port;
    s_window_expired = false;

    Clock_Params_init(&p);
    Clock_construct(&s_window_clk, window_swi,
                    MS_TO_TICKS(KEPLER_BLE_ADV_FAST_DURATION_MS), &p);

    start_adv(true);   /* boot: fast advertising window                    */
}

void ble_manager_on_connected(void)
{
    s_state          = BLE_STATE_CONNECTED;
    s_window_expired = false;
    Clock_stop(Clock_handle(&s_window_clk));

    if (s_port != NULL && s_port->stop_advertising != NULL) {
        s_port->stop_advertising();
    }
}

void ble_manager_on_disconnected(void)
{
    start_adv(true);   /* disconnect: fast for 30 s, then slow             */
}

void ble_manager_adv_window_expired(void)
{
    if (!s_window_expired) { return; }
    s_window_expired = false;

    if (s_state == BLE_STATE_ADVERTISING_FAST) {
        start_adv(false);
    }
}

ble_state_t ble_manager_get_state(void)
{
    return s_state;
}

bool ble_manager_is_connected(void)
{
    return (s_state == BLE_STATE_CONNECTED);
}

void ble_manager_notify(uint16_t char_uuid,
                        const uint8_t *value, uint16_t len)
{
    if (s_port != NULL && s_port->notify_char != NULL) {
        s_port->notify_char(char_uuid, value, len);
    }
}

/*--- Test-only helpers ---------------------------------------------------*/

#ifdef KEPLER_TEST_ONLY
void ble_manager_test_fire_window(void)
{
    window_swi(0u);
}
#endif /* KEPLER_TEST_ONLY */

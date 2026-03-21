/******************************************************************************
 *
 * @file  buttons.c
 *
 * @brief GPIO interrupt + debounce driver for F91 Kepler's three buttons.
 *
 *        ISR flow (per button):
 *          1. Edge ISR       → disable IRQ, start 20 ms debounce clock.
 *          2. Debounce fires → re-read pin.
 *               Press confirmed:   set pressed, start 600 ms long clock,
 *                                  re-enable release-edge IRQ.
 *               Release confirmed: stop long clock,
 *                                  if !long_fired → enqueue BTN_EVT_SHORT,
 *                                  re-enable press-edge IRQ.
 *          3. Long clock fires → if still pressed, enqueue BTN_EVT_LONG.
 *
 *        Ring buffer: 8-entry circular; newest event dropped on overflow.
 *        buttons_process() drains the ring in task context.
 *
 *****************************************************************************/

#include "buttons.h"
#include "../kepler_config.h"

#if KEPLER_HAS_BUTTONS

#include <ti/drivers/PIN.h>
#include <ti/sysbios/knl/Clock.h>

/*--- Board pin mapping ---------------------------------------------------*/

#ifdef KEPLER_DEV_BOARD
  #include "Board.h"
  static const uint32_t k_ioid[BTN_COUNT] = {
      Board_BUTTON0, Board_BUTTON1, Board_BUTTON2
  };
#else
  static const uint32_t k_ioid[BTN_COUNT] = {
      KEPLER_BTN1_PIN, KEPLER_BTN2_PIN, KEPLER_BTN3_PIN
  };
#endif

/*--- Ring buffer ---------------------------------------------------------*/

#define RING_SIZE  8u
#define RING_MASK  (RING_SIZE - 1u)

typedef struct {
    button_id_t    id;
    button_event_t evt;
} ring_entry_t;

static ring_entry_t     s_ring[RING_SIZE];
static volatile uint8_t s_ring_head;   /* written by Swi  */
static volatile uint8_t s_ring_tail;   /* read by task    */

static void ring_push(button_id_t id, button_event_t evt)
{
    uint8_t next = (uint8_t)((s_ring_head + 1u) & RING_MASK);
    if (next != s_ring_tail) {
        s_ring[s_ring_head].id  = id;
        s_ring[s_ring_head].evt = evt;
        s_ring_head             = next;
    }
    /* else: overflow — silently drop newest, oldest preserved */
}

/*--- Per-button state ----------------------------------------------------*/

typedef struct {
    Clock_Struct  debounce_clk;
    Clock_Struct  long_clk;
    volatile bool pressed;
    volatile bool long_fired;
} btn_state_t;

static btn_state_t s_btn[BTN_COUNT];

/*--- PIN driver ----------------------------------------------------------*/

static PIN_Handle s_pin_handle;
static PIN_State  s_pin_state;

/*--- Application callback -----------------------------------------------*/

static button_cb_t s_callback;

/*--- Timing helpers (ms → RTOS ticks) -----------------------------------*/

#define MS_TO_TICKS(ms)  ((uint32_t)(ms) * 1000u / Clock_tickPeriod)

/*==========================================================================*
 *  Swi callbacks (Swi context — no blocking calls, no peripheral I/O)     *
 *==========================================================================*/

static void debounce_swi(UArg arg)
{
    uint8_t     idx     = (uint8_t)arg;
    btn_state_t *b      = &s_btn[idx];
    uint32_t    ioid    = k_ioid[idx];
    uint8_t     pinval  = PIN_getInputValue(ioid);
    bool        pressed = (bool)(pinval ^ (uint8_t)KEPLER_BTN_ACTIVE_LOW);

    if (pressed && !b->pressed) {
        /* Press confirmed */
        b->pressed    = true;
        b->long_fired = false;
        Clock_setTimeout(Clock_handle(&b->long_clk),
                         MS_TO_TICKS(KEPLER_LONG_PRESS_MS));
        Clock_start(Clock_handle(&b->long_clk));
        PIN_setConfig(s_pin_handle, PIN_BM_IRQ,
                      ioid | KEPLER_BTN_RELEASE_EDGE);
    } else if (!pressed && b->pressed) {
        /* Release confirmed */
        Clock_stop(Clock_handle(&b->long_clk));
        if (!b->long_fired) {
            ring_push((button_id_t)idx, BTN_EVT_SHORT);
        }
        b->pressed    = false;
        b->long_fired = false;
        PIN_setConfig(s_pin_handle, PIN_BM_IRQ,
                      ioid | KEPLER_BTN_PRESS_EDGE);
    }
    /* else: spurious bounce — edge already re-disabled, await next ISR */
}

static void long_press_swi(UArg arg)
{
    uint8_t     idx = (uint8_t)arg;
    btn_state_t *b  = &s_btn[idx];
    if (b->pressed) {
        b->long_fired = true;
        ring_push((button_id_t)idx, BTN_EVT_LONG);
    }
}

/*--- GPIO ISR (Hwi context) ---------------------------------------------*/

static void gpio_isr(PIN_Handle handle, PIN_Id pinId)
{
    uint8_t idx;
    for (idx = 0u; idx < (uint8_t)BTN_COUNT; idx++) {
        if (k_ioid[idx] == pinId) { break; }
    }
    if (idx >= (uint8_t)BTN_COUNT) { return; }

    PIN_setConfig(handle, PIN_BM_IRQ, pinId | PIN_IRQ_DIS);
    Clock_setTimeout(Clock_handle(&s_btn[idx].debounce_clk),
                     MS_TO_TICKS(KEPLER_DEBOUNCE_MS));
    Clock_start(Clock_handle(&s_btn[idx].debounce_clk));
}

/*==========================================================================*
 *  PIN config table (built at runtime — k_ioid[] is not a compile-time    *
 *  constant when KEPLER_DEV_BOARD maps to Board_BUTTONx)                  *
 *==========================================================================*/

static PIN_Config s_pin_table[BTN_COUNT + 1u];

static void build_pin_table(void)
{
    uint8_t i;
    for (i = 0u; i < (uint8_t)BTN_COUNT; i++) {
        s_pin_table[i] = (PIN_Config)(k_ioid[i])
                       | PIN_INPUT_EN
                       | KEPLER_BTN_PULL
                       | KEPLER_BTN_PRESS_EDGE;
    }
    s_pin_table[BTN_COUNT] = PIN_TERMINATE;
}

/*==========================================================================*
 *  Public API                                                               *
 *==========================================================================*/

void buttons_init(button_cb_t callback)
{
    uint8_t i;

    s_callback  = callback;
    s_ring_head = 0u;
    s_ring_tail = 0u;

    build_pin_table();
    s_pin_handle = PIN_open(&s_pin_state, s_pin_table);
    PIN_registerIntCb(s_pin_handle, gpio_isr);

    for (i = 0u; i < (uint8_t)BTN_COUNT; i++) {
        Clock_Params p;
        Clock_Params_init(&p);
        p.arg = (UArg)i;
        Clock_construct(&s_btn[i].debounce_clk, debounce_swi,
                        MS_TO_TICKS(KEPLER_DEBOUNCE_MS), &p);
        Clock_construct(&s_btn[i].long_clk, long_press_swi,
                        MS_TO_TICKS(KEPLER_LONG_PRESS_MS), &p);
        s_btn[i].pressed    = false;
        s_btn[i].long_fired = false;
    }
}

uint8_t buttons_process(void)
{
    uint8_t count = 0u;
    while (s_ring_tail != s_ring_head) {
        ring_entry_t e = s_ring[s_ring_tail];
        s_ring_tail    = (uint8_t)((s_ring_tail + 1u) & RING_MASK);
        count++;
        if (s_callback != NULL) {
            s_callback(e.id, e.evt);
        }
    }
    return count;
}

bool buttons_is_pressed(button_id_t btn)
{
    if (btn >= BTN_COUNT) { return false; }
    return s_btn[btn].pressed;
}

/*--- Test-only helpers ---------------------------------------------------*/

#ifdef KEPLER_TEST_ONLY
void buttons_test_fire_debounce(button_id_t btn)
{
    if (btn < BTN_COUNT) {
        mock_clock_fire_force(Clock_handle(&s_btn[btn].debounce_clk));
    }
}

void buttons_test_fire_long(button_id_t btn)
{
    if (btn < BTN_COUNT) {
        mock_clock_fire_force(Clock_handle(&s_btn[btn].long_clk));
    }
}
#endif /* KEPLER_TEST_ONLY */

#else /* !KEPLER_HAS_BUTTONS */

void    buttons_init(button_cb_t cb)    { (void)cb; }
uint8_t buttons_process(void)           { return 0u; }
bool    buttons_is_pressed(button_id_t b) { (void)b; return false; }

#endif /* KEPLER_HAS_BUTTONS */

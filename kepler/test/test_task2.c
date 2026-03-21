/******************************************************************************
 *
 * @file  test_task2.c
 *
 * @brief Host-side test harness for Task 2: Button handlers + time-set FSM.
 *
 *        Build (from repo root):
 *          gcc -Wall -Wextra -std=c99 \
 *              -I kepler/test/stubs \
 *              -DKEPLER_HAS_SHARP_LCD=1 \
 *              -DKEPLER_HAS_BUTTONS=1 \
 *              -DKEPLER_TEST_ONLY \
 *              kepler/test/test_task2.c \
 *              kepler/input/buttons.c \
 *              kepler/input/time_set.c \
 *              kepler/display/sharp_lcd.c \
 *              kepler/display/ui_renderer.c \
 *              kepler/display/fonts.c \
 *              -o kepler/test/test_task2 && ./kepler/test/test_task2
 *
 *****************************************************************************/

/* POSIX extensions for gmtime_r */
#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "stubs/mock_state.h"
#include <ti/sysbios/knl/Clock.h>   /* Clock_Struct, mock_clock_fire */
#include "../input/buttons.h"
#include "../input/time_set.h"
#include "../display/ui_renderer.h"
#include "../kepler_config.h"

/*==========================================================================*
 *  Mock state definitions (owned by this translation unit)                 *
 *==========================================================================*/

uint8_t          mock_spi_buf[MOCK_SPI_BUFSIZE];
uint16_t         mock_spi_len;
int              mock_spi_call_count;
mock_pin_event_t mock_pin_log[MOCK_PIN_LOG_SIZE];
int              mock_pin_log_count;

mock_pin_int_cb_t mock_pin_int_cb     = NULL;
void             *mock_pin_int_handle = NULL;
uint8_t           mock_pin_input[32];

uint32_t          mock_seconds_value  = 0u;

void mock_spi_reset(void) {
    mock_spi_len       = 0;
    mock_spi_call_count = 0;
    mock_pin_log_count  = 0;
    mock_pin_int_cb     = NULL;
    mock_pin_int_handle = NULL;
    memset(mock_pin_input, 0, sizeof(mock_pin_input));
    mock_seconds_value  = 0u;
}

/*==========================================================================*
 *  Test infrastructure                                                      *
 *==========================================================================*/

static int tests_run    = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    do { \
        tests_run++; \
        printf("  %-56s ", name); \
    } while (0)

#define PASS() \
    do { tests_passed++; printf("PASS\n"); } while (0)

#define FAIL(msg) \
    do { tests_failed++; printf("FAIL  (%s)\n", msg); } while (0)

#define ASSERT(cond, msg) \
    do { if (!(cond)) { FAIL(msg); return; } } while (0)

/*==========================================================================*
 *  time_set clock accessors (implemented in time_set.c under KEPLER_TEST_ONLY)
 *==========================================================================*/

extern Clock_Struct *time_set_test_confirm_clk(void);
extern Clock_Struct *time_set_test_timeout_clk(void);

#define s_confirm_clk_test (*time_set_test_confirm_clk())
#define s_timeout_clk_test (*time_set_test_timeout_clk())

/*==========================================================================*
 *  Button callback helper                                                   *
 *==========================================================================*/

static button_id_t    last_btn_id  = (button_id_t)0xFF;
static button_event_t last_btn_evt = (button_event_t)0xFF;
static int            cb_call_count = 0;

static void btn_callback(button_id_t btn, button_event_t evt)
{
    last_btn_id  = btn;
    last_btn_evt = evt;
    cb_call_count++;
}

static void reset_cb(void)
{
    last_btn_id   = (button_id_t)0xFF;
    last_btn_evt  = (button_event_t)0xFF;
    cb_call_count = 0;
}

/*==========================================================================*
 *  Simulated press/release helpers                                          *
 *                                                                           *
 *  Target polarity (KEPLER_BTN_ACTIVE_LOW = 0):                            *
 *    pressed  = pin HIGH (1)                                                *
 *    released = pin LOW  (0)                                                *
 *==========================================================================*/

/* IOID values for the three buttons on the target (non-dev) build. */
#define TEST_BTN1_IOID  KEPLER_BTN1_PIN   /* IOID_15 */
#define TEST_BTN2_IOID  KEPLER_BTN2_PIN   /* IOID_25 */
#define TEST_BTN3_IOID  KEPLER_BTN3_PIN   /* IOID_26 */

static const uint32_t k_test_ioid[3] = {
    TEST_BTN1_IOID, TEST_BTN2_IOID, TEST_BTN3_IOID
};

/* Simulate a full short press: ISR → debounce(press) → ISR → debounce(release). */
static void sim_short_press(button_id_t btn)
{
    uint32_t ioid = k_test_ioid[btn];
    mock_pin_set_input(ioid, 1u);           /* pin goes HIGH (pressed)  */
    mock_pin_trigger_interrupt(ioid);       /* press-edge ISR           */
    buttons_test_fire_debounce(btn);        /* debounce confirms press  */
    mock_pin_set_input(ioid, 0u);           /* pin goes LOW (released)  */
    mock_pin_trigger_interrupt(ioid);       /* release-edge ISR         */
    buttons_test_fire_debounce(btn);        /* debounce confirms release */
}

/* Simulate a long press: ISR → debounce(press) → long fires → ISR → debounce(release). */
static void sim_long_press(button_id_t btn)
{
    uint32_t ioid = k_test_ioid[btn];
    mock_pin_set_input(ioid, 1u);
    mock_pin_trigger_interrupt(ioid);
    buttons_test_fire_debounce(btn);        /* confirms press, starts long clock */
    buttons_test_fire_long(btn);            /* long fires → BTN_EVT_LONG         */
    mock_pin_set_input(ioid, 0u);
    mock_pin_trigger_interrupt(ioid);
    buttons_test_fire_debounce(btn);        /* confirms release, no SHORT         */
}

/*==========================================================================*
 *  Tests — button debounce                                                  *
 *==========================================================================*/

static void test_debounce_filters_noise(void)
{
    TEST("debounce: filters noise (pin reads inactive after ISR)");
    mock_spi_reset();
    reset_cb();
    buttons_init(btn_callback);

    uint32_t ioid = TEST_BTN1_IOID;
    mock_pin_set_input(ioid, 0u);   /* pin still LOW when debounce fires   */
    mock_pin_trigger_interrupt(ioid);
    buttons_test_fire_debounce(BTN_1);  /* confirms "not pressed" — spurious */

    ASSERT(buttons_process() == 0, "spurious event delivered");
    ASSERT(cb_call_count == 0, "callback invoked for noise");
    PASS();
}

static void test_short_press_delivers_event(void)
{
    TEST("debounce: short press delivers BTN_EVT_SHORT");
    mock_spi_reset();
    reset_cb();
    buttons_init(btn_callback);

    sim_short_press(BTN_1);
    uint8_t n = buttons_process();

    ASSERT(n == 1,                        "expected 1 event");
    ASSERT(last_btn_id  == BTN_1,         "wrong button id");
    ASSERT(last_btn_evt == BTN_EVT_SHORT, "expected SHORT event");
    PASS();
}

static void test_long_press_no_short(void)
{
    TEST("debounce: long press gives LONG, not SHORT on release");
    mock_spi_reset();
    reset_cb();
    buttons_init(btn_callback);

    sim_long_press(BTN_1);
    uint8_t n = buttons_process();

    ASSERT(n == 1,                       "expected 1 event");
    ASSERT(last_btn_id  == BTN_1,        "wrong button id");
    ASSERT(last_btn_evt == BTN_EVT_LONG, "expected LONG event");
    PASS();
}

static void test_ring_overflow_graceful(void)
{
    TEST("ring buffer: overflow is graceful (no crash, no deadlock)");
    mock_spi_reset();
    reset_cb();
    buttons_init(btn_callback);

    /* Fill ring beyond capacity (RING_SIZE = 8, head+1 == tail is full). */
    uint8_t i;
    for (i = 0u; i < 10u; i++) {
        sim_short_press(BTN_2);
    }

    /* Must not crash, and process() must drain without error. */
    uint8_t n = buttons_process();
    ASSERT(n <= 8u,       "ring overflowed capacity");
    ASSERT(n > 0u,        "no events delivered");
    PASS();
}

/*==========================================================================*
 *  Tests — time-set state machine                                           *
 *==========================================================================*/

static void test_enter_time_set_via_btn2_long(void)
{
    TEST("time_set: BTN_2 LONG enters SET_HOURS");
    mock_spi_reset();
    reset_cb();

    struct tm ref = {0};
    ref.tm_year = 124; ref.tm_mon = 0; ref.tm_mday = 1;
    ref.tm_hour = 10; ref.tm_min = 30;
    mock_seconds_value = (uint32_t)mktime(&ref);

    ui_init();
    time_set_init();

    ASSERT(!time_set_is_active(), "should start in NORMAL");

    bool consumed = time_set_handle_button(BTN_2, BTN_EVT_LONG);

    ASSERT(consumed,                              "BTN_2 LONG should be consumed");
    ASSERT(time_set_is_active(),                  "should be active after BTN_2 LONG");
    ASSERT(time_set_get_state() == TS_SET_HOURS,  "should be in SET_HOURS");
    PASS();
}

static void test_increment_hours_wraps(void)
{
    TEST("time_set: BTN_1 increments hours, wraps at 24");
    mock_spi_reset();

    struct tm ref = {0};
    ref.tm_year = 124; ref.tm_mon = 0; ref.tm_mday = 1;
    ref.tm_hour = 23; ref.tm_min = 0;
    mock_seconds_value = (uint32_t)mktime(&ref);

    ui_init();
    time_set_init();
    time_set_handle_button(BTN_2, BTN_EVT_LONG);   /* enter SET_HOURS at h=23 */

    time_set_handle_button(BTN_1, BTN_EVT_SHORT);  /* 23 → 0 (wrap)           */

    /* Read back via confirm: advance to SET_MINUTES then check RTC write. */
    time_set_handle_button(BTN_2, BTN_EVT_SHORT);  /* SET_HOURS → SET_MINUTES */
    time_set_handle_button(BTN_2, BTN_EVT_SHORT);  /* SET_MINUTES → CONFIRM   */

    time_t   ts = (time_t)mock_seconds_value;
    struct tm t;
    gmtime_r(&ts, &t);

    ASSERT(t.tm_hour == 0, "hours should wrap to 0");
    PASS();
}

static void test_decrement_hours_wraps(void)
{
    TEST("time_set: BTN_3 decrements hours, wraps at 0");
    mock_spi_reset();

    struct tm ref = {0};
    ref.tm_year = 124; ref.tm_mon = 0; ref.tm_mday = 1;
    ref.tm_hour = 0; ref.tm_min = 0;
    mock_seconds_value = (uint32_t)mktime(&ref);

    ui_init();
    time_set_init();
    time_set_handle_button(BTN_2, BTN_EVT_LONG);  /* enter at h=0 */

    time_set_handle_button(BTN_3, BTN_EVT_SHORT); /* 0 → 23 (wrap) */

    time_set_handle_button(BTN_2, BTN_EVT_SHORT); /* → SET_MINUTES */
    time_set_handle_button(BTN_2, BTN_EVT_SHORT); /* → CONFIRM     */

    time_t   ts = (time_t)mock_seconds_value;
    struct tm t;
    gmtime_r(&ts, &t);

    ASSERT(t.tm_hour == 23, "hours should wrap to 23");
    PASS();
}

static void test_increment_minutes_wraps(void)
{
    TEST("time_set: BTN_1 increments minutes, wraps at 60");
    mock_spi_reset();

    struct tm ref = {0};
    ref.tm_year = 124; ref.tm_mon = 0; ref.tm_mday = 1;
    ref.tm_hour = 0; ref.tm_min = 59;
    mock_seconds_value = (uint32_t)mktime(&ref);

    ui_init();
    time_set_init();
    time_set_handle_button(BTN_2, BTN_EVT_LONG);  /* enter SET_HOURS at m=59 */
    time_set_handle_button(BTN_2, BTN_EVT_SHORT); /* → SET_MINUTES           */

    time_set_handle_button(BTN_1, BTN_EVT_SHORT); /* 59 → 0 (wrap)           */
    time_set_handle_button(BTN_2, BTN_EVT_SHORT); /* → CONFIRM               */

    time_t   ts = (time_t)mock_seconds_value;
    struct tm t;
    gmtime_r(&ts, &t);

    ASSERT(t.tm_min == 0, "minutes should wrap to 0");
    PASS();
}

static void test_decrement_minutes_wraps(void)
{
    TEST("time_set: BTN_3 decrements minutes, wraps at 0");
    mock_spi_reset();

    struct tm ref = {0};
    ref.tm_year = 124; ref.tm_mon = 0; ref.tm_mday = 1;
    ref.tm_hour = 0; ref.tm_min = 0;
    mock_seconds_value = (uint32_t)mktime(&ref);

    ui_init();
    time_set_init();
    time_set_handle_button(BTN_2, BTN_EVT_LONG);  /* enter at m=0  */
    time_set_handle_button(BTN_2, BTN_EVT_SHORT); /* → SET_MINUTES */

    time_set_handle_button(BTN_3, BTN_EVT_SHORT); /* 0 → 59 (wrap) */
    time_set_handle_button(BTN_2, BTN_EVT_SHORT); /* → CONFIRM     */

    time_t   ts = (time_t)mock_seconds_value;
    struct tm t;
    gmtime_r(&ts, &t);

    ASSERT(t.tm_min == 59, "minutes should wrap to 59");
    PASS();
}

static void test_confirm_writes_rtc(void)
{
    TEST("time_set: confirm writes correct hours and minutes to RTC");
    mock_spi_reset();

    struct tm ref = {0};
    ref.tm_year = 124; ref.tm_mon = 0; ref.tm_mday = 1;
    ref.tm_hour = 6; ref.tm_min = 15;
    mock_seconds_value = (uint32_t)mktime(&ref);

    ui_init();
    time_set_init();
    time_set_handle_button(BTN_2, BTN_EVT_LONG);  /* enter h=6, m=15         */

    /* Set to 14:30 */
    uint8_t i;
    for (i = 0u; i < 8u; i++) {
        time_set_handle_button(BTN_1, BTN_EVT_SHORT);   /* 6+8 = 14            */
    }
    time_set_handle_button(BTN_2, BTN_EVT_SHORT);  /* → SET_MINUTES, h=14    */

    for (i = 0u; i < 15u; i++) {
        time_set_handle_button(BTN_1, BTN_EVT_SHORT);   /* 15+15 = 30          */
    }
    time_set_handle_button(BTN_2, BTN_EVT_SHORT);  /* → CONFIRM, writes RTC  */

    time_t   ts = (time_t)mock_seconds_value;
    struct tm t;
    gmtime_r(&ts, &t);

    ASSERT(t.tm_hour == 14, "wrong hours in RTC");
    ASSERT(t.tm_min  == 30, "wrong minutes in RTC");
    ASSERT(t.tm_sec  == 0,  "seconds not zeroed");
    PASS();
}

static void test_confirm_auto_returns_to_normal(void)
{
    TEST("time_set: confirm auto-returns to NORMAL after confirm clock fires");
    mock_spi_reset();

    struct tm ref = {0};
    ref.tm_year = 124; ref.tm_mon = 0; ref.tm_mday = 1;
    ref.tm_hour = 8; ref.tm_min = 0;
    mock_seconds_value = (uint32_t)mktime(&ref);

    ui_init();
    time_set_init();
    time_set_handle_button(BTN_2, BTN_EVT_LONG);  /* → SET_HOURS  */
    time_set_handle_button(BTN_2, BTN_EVT_SHORT); /* → SET_MINUTES */
    time_set_handle_button(BTN_2, BTN_EVT_SHORT); /* → CONFIRM    */

    ASSERT(time_set_get_state() == TS_CONFIRM, "should be CONFIRM");

    /* Simulate confirm timer expiry. */
    mock_clock_fire(Clock_handle(&s_confirm_clk_test));
    time_set_process();

    ASSERT(time_set_get_state() == TS_NORMAL, "should return to NORMAL");
    ASSERT(!time_set_is_active(),             "should not be active");
    PASS();
}

static void test_timeout_discards_changes(void)
{
    TEST("time_set: 30s inactivity timeout discards changes");
    mock_spi_reset();

    struct tm ref = {0};
    ref.tm_year = 124; ref.tm_mon = 0; ref.tm_mday = 1;
    ref.tm_hour = 12; ref.tm_min = 0;
    mock_seconds_value = (uint32_t)mktime(&ref);

    ui_init();
    time_set_init();
    time_set_handle_button(BTN_2, BTN_EVT_LONG);  /* enter at h=12  */

    /* Attempt to change hours (will be discarded). */
    uint8_t i;
    for (i = 0u; i < 7u; i++) {
        time_set_handle_button(BTN_3, BTN_EVT_SHORT); /* 12→11→...→5 */
    }
    ASSERT(time_set_get_state() == TS_SET_HOURS, "should be SET_HOURS");

    /* Simulate timeout without confirming. */
    mock_clock_fire(Clock_handle(&s_timeout_clk_test));
    time_set_process();

    ASSERT(time_set_get_state() == TS_NORMAL, "should return to NORMAL");
    /* mock_seconds_value must be unchanged (no RTC write on timeout). */
    time_t   ts = (time_t)mock_seconds_value;
    struct tm t;
    gmtime_r(&ts, &t);
    ASSERT(t.tm_hour == 12, "RTC was changed on timeout (should discard)");
    PASS();
}

static void test_normal_passthrough(void)
{
    TEST("time_set: returns false for non-BTN_2-LONG when NORMAL");
    mock_spi_reset();

    ui_init();
    time_set_init();

    ASSERT(!time_set_is_active(), "should be NORMAL");

    /* All of these should pass through. */
    ASSERT(!time_set_handle_button(BTN_1, BTN_EVT_SHORT), "BTN_1 SHORT should pass");
    ASSERT(!time_set_handle_button(BTN_1, BTN_EVT_LONG),  "BTN_1 LONG should pass");
    ASSERT(!time_set_handle_button(BTN_3, BTN_EVT_SHORT), "BTN_3 SHORT should pass");
    ASSERT(!time_set_handle_button(BTN_3, BTN_EVT_LONG),  "BTN_3 LONG should pass");
    ASSERT(!time_set_handle_button(BTN_2, BTN_EVT_SHORT), "BTN_2 SHORT should pass");

    /* BTN_2 LONG must be consumed and enter time-set. */
    ASSERT( time_set_handle_button(BTN_2, BTN_EVT_LONG),  "BTN_2 LONG should be consumed");
    ASSERT(time_set_is_active(), "should now be active");
    PASS();
}

/*==========================================================================*
 *  main                                                                     *
 *==========================================================================*/

int main(void)
{
    /* Force UTC so mktime() and gmtime_r() are consistent in all tests.   */
    setenv("TZ", "UTC", 1);
    tzset();

    printf("\n=== Task 2: Button Handlers + Time-Setting State Machine ===\n\n");

    printf("-- Button debounce --\n");
    test_debounce_filters_noise();
    test_short_press_delivers_event();
    test_long_press_no_short();
    test_ring_overflow_graceful();

    printf("\n-- Time-set state machine --\n");
    test_enter_time_set_via_btn2_long();
    test_increment_hours_wraps();
    test_decrement_hours_wraps();
    test_increment_minutes_wraps();
    test_decrement_minutes_wraps();
    test_confirm_writes_rtc();
    test_confirm_auto_returns_to_normal();
    test_timeout_discards_changes();
    test_normal_passthrough();

    printf("\n=== %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0) {
        printf(", %d FAILED", tests_failed);
    }
    printf(" ===\n\n");

    return (tests_failed > 0) ? 1 : 0;
}

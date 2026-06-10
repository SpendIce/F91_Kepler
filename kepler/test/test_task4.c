/******************************************************************************
 *
 * @file  test_task4.c
 *
 * @brief Host tests for Task 4 — LIS2DW12, pedometer, actigraphy.
 *
 *          gcc -Wall -Wextra -std=c99 -DKEPLER_TEST_ONLY \
 *              -DKEPLER_HAS_LIS2DW12=1 \
 *              -I kepler/test/stubs \
 *              kepler/test/test_task4.c kepler/test/stubs/mocks.c \
 *              kepler/accel/lis2dw12.c kepler/accel/pedometer.c \
 *              kepler/accel/actigraphy.c \
 *              kepler/kepler_i2c.c kepler/storage/flash_store.c \
 *              kepler/power/event_queue.c \
 *              -o kepler/test/test_task4
 *
 *****************************************************************************/

#include <stdio.h>
#include <string.h>

#include "stubs/mock_state.h"
#include <ti/drivers/I2C.h>
#include "osal_snv.h"

#include "../accel/lis2dw12.h"
#include "../accel/pedometer.h"
#include "../accel/actigraphy.h"
#include "../power/event_queue.h"
#include "../storage/flash_store.h"
#include "../kepler_config.h"

uint32_t mock_seconds_value = 0u;

static int s_pass;
static int s_fail;

#define CHECK(cond, name)                                            \
    do {                                                             \
        if (cond) { s_pass++; }                                      \
        else      { s_fail++; printf("FAIL: %s\n", name); }          \
    } while (0)

static uint8_t *lis_regs(void) { return mock_i2c_reg(0x18); }

static void reset_all(void)
{
    mock_i2c_reset();
    mock_snv_reset();
    event_queue_init();
    lis_regs()[LIS2DW12_REG_WHO_AM_I] = LIS2DW12_WHO_AM_I_VALUE;
}

/* Drain the queue looking for an event type; returns its param or        *
 * 0xFFFFFFFF if not found.                                                */
static uint32_t find_event(kepler_event_t type)
{
    kepler_event_msg_t msg;
    uint32_t found = 0xFFFFFFFFu;
    while (event_queue_pend(&msg, 0u)) {
        if (msg.type == type) { found = msg.param; }
    }
    return found;
}

/*==========================================================================*
 *  LIS2DW12 driver                                                          *
 *==========================================================================*/

static void test_lis_init(void)
{
    reset_all();
    lis_regs()[LIS2DW12_REG_WHO_AM_I] = 0x00;
    CHECK(!lis2dw12_init(), "init fails on wrong WHO_AM_I");

    reset_all();
    CHECK(lis2dw12_init(), "init succeeds with WHO_AM_I=0x44");
    CHECK(lis_regs()[LIS2DW12_REG_CTRL1] == 0x10, "CTRL1: LP1, ODR 12.5Hz");
    CHECK(lis_regs()[LIS2DW12_REG_CTRL6] == 0x04, "CTRL6: +/-2g low-noise");
    CHECK(lis_regs()[LIS2DW12_REG_CTRL2] == 0x0C, "CTRL2: BDU + ADD_INC");
    CHECK(lis_regs()[LIS2DW12_REG_WAKE_UP_THS] == 0x10,
          "wake threshold ~0.5g");
    CHECK(lis_regs()[LIS2DW12_REG_WAKE_UP_DUR] == 0x02, "wake duration 2");
    CHECK((lis_regs()[LIS2DW12_REG_CTRL4_INT1] & 0x20) != 0u,
          "wake-up routed to INT1");
    CHECK((lis_regs()[LIS2DW12_REG_CTRL3] & 0x10) != 0u, "LIR latched");
    CHECK(lis_regs()[LIS2DW12_REG_CTRL7] == 0x20, "interrupts enabled");
    CHECK(lis_regs()[LIS2DW12_REG_FUNC_CFG_ACCESS] == 0x00,
          "bank A restored after pedometer enable");
}

static void test_lis_steps(void)
{
    reset_all();
    (void)lis2dw12_init();

    /* The mock register file is flat — bank B regs share addresses with  *
     * bank A.  STEP_COUNTER_L/H (0x3A/0x3B) don't collide with anything  *
     * the driver writes in bank A, so poking them directly works.        */
    lis_regs()[LIS2DW12_REGB_STEP_COUNTER_L] = 0x34;
    lis_regs()[LIS2DW12_REGB_STEP_COUNTER_H] = 0x12;
    CHECK(lis2dw12_read_steps() == 0x1234, "step counter hi/lo merge");

    lis2dw12_reset_steps();
    CHECK(lis_regs()[LIS2DW12_REGB_FUNC_CK_GATE] == 0x10,
          "reset leaves STEP_D_EN set, reset bit cleared");
    CHECK(lis_regs()[LIS2DW12_REG_FUNC_CFG_ACCESS] == 0x00,
          "bank A restored after reset");
}

static void test_lis_wakeup_clear(void)
{
    reset_all();
    (void)lis2dw12_init();
    lis_regs()[LIS2DW12_REG_WAKE_UP_SRC] = 0x08;
    CHECK(lis2dw12_clear_wakeup_src() == 0x08, "WAKE_UP_SRC read back");
}

/*==========================================================================*
 *  Pedometer                                                                *
 *==========================================================================*/

static void test_pedometer_poll_and_reset(void)
{
    reset_all();
    (void)lis2dw12_init();
    pedometer_init();
    CHECK(pedometer_get_steps() == 0u, "starts at zero");

    lis_regs()[LIS2DW12_REGB_STEP_COUNTER_L] = 100;
    pedometer_poll();
    CHECK(pedometer_get_steps() == 100u, "poll picks up hardware count");
    CHECK(find_event(EVT_STEP_UPDATE) == 100u,
          "EVT_STEP_UPDATE posted with total");

    pedometer_poll();
    CHECK(find_event(EVT_STEP_UPDATE) == 0xFFFFFFFFu,
          "no event when count unchanged");

    /* Midnight: history rotates, counter resets                           */
    pedometer_midnight_reset();
    CHECK(lis_regs()[LIS2DW12_REGB_STEP_COUNTER_L] == 100,
          "mock HW counter untouched by reset cmd (flat regfile)");
    {
        uint16_t hist[7];
        (void)flash_store_read_steps(hist);
        CHECK(hist[0] == 100u, "yesterday's count in history[0]");
    }
    CHECK(find_event(EVT_STEP_UPDATE) == 0u, "post-midnight step event 0");
}

static void test_pedometer_crash_recovery(void)
{
    reset_all();
    (void)lis2dw12_init();
    (void)flash_store_write_step_today(5000u);
    pedometer_init();
    CHECK(pedometer_get_steps() == 5000u, "base restored from flash");
}

/*==========================================================================*
 *  Actigraphy                                                               *
 *==========================================================================*/

/* 2026-01-01 23:00 local — inside the default 22-08 window.               */
#define TS_IN_WINDOW    ((uint32_t)(86400u * 20454u + 23u * 3600u))
/* 12:00 — outside.                                                        */
#define TS_OUT_WINDOW   ((uint32_t)(86400u * 20454u + 12u * 3600u))

static void test_actigraphy_window(void)
{
    actigraphy_set_window(22u, 8u);

    mock_seconds_value = TS_IN_WINDOW;
    CHECK(actigraphy_in_sleep_window(), "23:00 inside 22-08 window");

    mock_seconds_value = TS_OUT_WINDOW;
    CHECK(!actigraphy_in_sleep_window(), "12:00 outside 22-08 window");

    mock_seconds_value = 86400u * 20454u + 3u * 3600u;   /* 03:00 */
    CHECK(actigraphy_in_sleep_window(), "03:00 inside wrapped window");

    actigraphy_set_window(9u, 17u);
    mock_seconds_value = 86400u * 20454u + 12u * 3600u;
    CHECK(actigraphy_in_sleep_window(), "non-wrapped window works");
    actigraphy_set_window(22u, 8u);
}

static void test_actigraphy_epochs(void)
{
    reset_all();
    actigraphy_init();
    actigraphy_set_window(22u, 8u);
    mock_seconds_value = TS_IN_WINDOW;

    /* Epoch 0: 3 movements -> restless                                    */
    actigraphy_on_movement();
    actigraphy_on_movement();
    actigraphy_on_movement();
    actigraphy_epoch_close();

    /* Epoch 1: 1 movement -> still                                        */
    actigraphy_on_movement();
    actigraphy_epoch_close();

    {
        const actigraphy_night_t *n = actigraphy_current_night();
        CHECK((n->epochs[0] & 0x01u) != 0u, "epoch 0 restless bit set");
        CHECK((n->epochs[0] & 0x02u) == 0u, "epoch 1 still bit clear");
        CHECK(n->date == TS_IN_WINDOW, "night dated at first epoch");
    }

    actigraphy_night_close();
    {
        actigraphy_night_t saved;
        CHECK(flash_store_read_sleep(&saved), "night persisted");
        CHECK(saved.epoch_count == 2u, "two epochs recorded");
        CHECK((saved.epochs[0] & 0x03u) == 0x01u, "bit pattern persisted");
    }
    {
        const actigraphy_night_t *n = actigraphy_current_night();
        CHECK(n->epoch_count == 0u && n->date == 0u,
              "night state reset after close");
    }
}

static void test_actigraphy_outside_window(void)
{
    reset_all();
    actigraphy_init();
    mock_seconds_value = TS_OUT_WINDOW;

    actigraphy_on_movement();
    actigraphy_on_movement();
    actigraphy_on_movement();
    actigraphy_epoch_close();

    actigraphy_night_close();
    {
        actigraphy_night_t saved;
        CHECK(!flash_store_read_sleep(&saved),
              "nothing persisted for daytime movement");
    }
}

int main(void)
{
    test_lis_init();
    test_lis_steps();
    test_lis_wakeup_clear();
    test_pedometer_poll_and_reset();
    test_pedometer_crash_recovery();
    test_actigraphy_window();
    test_actigraphy_epochs();
    test_actigraphy_outside_window();

    printf("test_task4: %d passed, %d failed\n", s_pass, s_fail);
    return (s_fail == 0) ? 0 : 1;
}

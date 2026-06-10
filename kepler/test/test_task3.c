/******************************************************************************
 *
 * @file  test_task3.c
 *
 * @brief Host tests for Task 3 — DRV2605L driver + haptic patterns.
 *
 *        Compiled with gcc on Linux (not CCS), hardware flag forced on:
 *
 *          gcc -Wall -Wextra -std=c99 -DKEPLER_TEST_ONLY \
 *              -DKEPLER_HAS_DRV2605L=1 \
 *              -I kepler/test/stubs \
 *              kepler/test/test_task3.c kepler/test/stubs/mocks.c \
 *              kepler/haptic/drv2605l.c kepler/haptic/haptic_patterns.c \
 *              kepler/kepler_i2c.c kepler/storage/flash_store.c \
 *              kepler/power/event_queue.c \
 *              -o kepler/test/test_task3
 *
 *****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "stubs/mock_state.h"
#include <ti/drivers/I2C.h>
#include <ti/sysbios/knl/Task.h>
#include "osal_snv.h"

#include "../haptic/drv2605l.h"
#include "../haptic/haptic_patterns.h"
#include "../power/event_queue.h"
#include "../storage/flash_store.h"
#include "../kepler_config.h"

/*--- Task 1/2-convention mock globals (unused here but linked stubs) ----*/
uint32_t mock_seconds_value = 0u;

static int s_pass;
static int s_fail;

#define CHECK(cond, name)                                            \
    do {                                                             \
        if (cond) { s_pass++; }                                      \
        else      { s_fail++; printf("FAIL: %s\n", name); }          \
    } while (0)

/*--- DRV2605L mock helpers ------------------------------------------------*/

static uint8_t *drv_regs(void) { return mock_i2c_reg(0x5A); }

static void reset_all(void)
{
    mock_i2c_reset();
    mock_snv_reset();
    mock_task_sleep_ticks = 0u;
    event_queue_init();
    /* Device present: STATUS device ID = 0xE0 (DRV2605L), no DIAG fail.  */
    drv_regs()[DRV2605L_REG_STATUS] = 0xE0;
}

/*==========================================================================*
 *  Tests                                                                    *
 *==========================================================================*/

static void test_init_device_absent(void)
{
    reset_all();
    drv_regs()[DRV2605L_REG_STATUS] = 0x00;   /* wrong device ID           */
    CHECK(!drv2605l_init(), "init fails when device ID wrong");
}

/* Emulate the DRV2605L clearing GO when auto-calibration completes.      */
static void autocal_go_hook(uint8_t addr, uint8_t reg, uint8_t val)
{
    if (addr == 0x5A && reg == DRV2605L_REG_GO && val == 0x01 &&
        drv_regs()[DRV2605L_REG_MODE] == DRV2605L_MODE_AUTOCAL) {
        drv_regs()[DRV2605L_REG_GO]           = 0x00;   /* cal "done"     */
        drv_regs()[DRV2605L_REG_AUTOCAL_COMP] = 0x42;
        drv_regs()[DRV2605L_REG_AUTOCAL_BEMF] = 0x77;
    }
}

static void test_init_autocal_first_boot(void)
{
    reset_all();
    mock_i2c_write_hook = autocal_go_hook;

    CHECK(drv2605l_init(), "first-boot init runs auto-cal successfully");
    CHECK(flash_store_haptic_is_calibrated(), "calibration flag persisted");
    {
        uint8_t comp = 0u, bemf = 0u;
        CHECK(flash_store_read_haptic_cal(&comp, &bemf) &&
              comp == 0x42 && bemf == 0x77,
              "COMP/BEMF persisted to flash");
    }
    CHECK(mock_task_sleep_ticks > 0u, "auto-cal polling waited");

    mock_i2c_write_hook = 0;
}

static void test_init_autocal_timeout(void)
{
    reset_all();   /* no hook: GO never clears -> 2 s poll timeout         */
    CHECK(!drv2605l_init(), "init fails when auto-cal GO never clears");
    CHECK(!flash_store_haptic_is_calibrated(),
          "no calibration stored after failed auto-cal");
}

static void test_init_with_stored_calibration(void)
{
    reset_all();
    /* Pre-store calibration: driver must skip auto-cal entirely.          */
    CHECK(flash_store_write_haptic_cal(0x42, 0x77), "store haptic cal");

    CHECK(drv2605l_init(), "init succeeds with stored calibration");
    CHECK(drv_regs()[DRV2605L_REG_AUTOCAL_COMP] == 0x42,
          "COMP reloaded from flash");
    CHECK(drv_regs()[DRV2605L_REG_AUTOCAL_BEMF] == 0x77,
          "BEMF reloaded from flash");
    CHECK(drv_regs()[DRV2605L_REG_LIBRARY] == 0x01, "ERM library selected");
    CHECK((drv_regs()[DRV2605L_REG_MODE] & DRV2605L_MODE_STANDBY_BIT) != 0u,
          "device left in standby after init");
    CHECK(mock_task_sleep_ticks == 0u, "no auto-cal delay when calibrated");
}

static void test_play_sequence(void)
{
    reset_all();
    (void)flash_store_write_haptic_cal(0x42, 0x77);
    (void)drv2605l_init();

    uint8_t seq[] = { 52, 52, 0 };
    CHECK(drv2605l_play_sequence(seq, 3u), "play_sequence accepted");
    CHECK(drv_regs()[DRV2605L_REG_WAVESEQ1] == 52, "WAVESEQ1 loaded");
    CHECK(drv_regs()[DRV2605L_REG_WAVESEQ1 + 1] == 52, "WAVESEQ2 loaded");
    CHECK(drv_regs()[DRV2605L_REG_WAVESEQ1 + 2] == 0, "sequence terminated");
    CHECK(drv_regs()[DRV2605L_REG_GO] == 1, "GO fired");
    CHECK((drv_regs()[DRV2605L_REG_MODE] & DRV2605L_MODE_STANDBY_BIT) == 0u,
          "standby exited for playback");

    CHECK(drv2605l_is_playing(), "is_playing true while GO set");

    CHECK(drv2605l_stop(), "stop accepted");
    CHECK(drv_regs()[DRV2605L_REG_GO] == 0, "GO cleared on stop");
    CHECK((drv_regs()[DRV2605L_REG_MODE] & DRV2605L_MODE_STANDBY_BIT) != 0u,
          "standby re-entered on stop");
}

static void test_haptic_patterns(void)
{
    reset_all();
    (void)flash_store_write_haptic_cal(0x42, 0x77);
    (void)drv2605l_init();
    haptic_init();

    /* CONFIRM: single effect 1, non-repeating                             */
    haptic_play(HAPTIC_CONFIRM);
    CHECK(drv_regs()[DRV2605L_REG_WAVESEQ1] == 1, "CONFIRM uses effect 1");
    CHECK(drv_regs()[DRV2605L_REG_GO] == 1, "CONFIRM fires GO");

    /* CALL: repeating — tick must retrigger GO after a manual clear       */
    haptic_play(HAPTIC_CALL);
    CHECK(drv_regs()[DRV2605L_REG_WAVESEQ1] == 47, "CALL uses effect 47");
    CHECK(haptic_is_active(), "CALL active");

    drv_regs()[DRV2605L_REG_GO] = 0;   /* hardware finished the buzz       */
    haptic_tick();                     /* main task on EVT_HAPTIC_TICK     */
    CHECK(drv_regs()[DRV2605L_REG_GO] == 1, "tick retriggers CALL");

    haptic_stop();
    CHECK(drv_regs()[DRV2605L_REG_GO] == 0, "stop silences motor");
    CHECK(!haptic_is_active(), "inactive after stop");

    /* Out-of-range pattern is a no-op                                     */
    drv_regs()[DRV2605L_REG_GO] = 0;
    haptic_play(HAPTIC_COUNT);
    CHECK(drv_regs()[DRV2605L_REG_GO] == 0, "invalid pattern ignored");
}

int main(void)
{
    test_init_device_absent();
    test_init_autocal_first_boot();
    test_init_autocal_timeout();
    test_init_with_stored_calibration();
    test_play_sequence();
    test_haptic_patterns();

    printf("test_task3: %d passed, %d failed\n", s_pass, s_fail);
    return (s_fail == 0) ? 0 : 1;
}

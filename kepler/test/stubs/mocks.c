/******************************************************************************
 *
 * @file  mocks.c
 *
 * @brief Definitions for the mock globals introduced with Tasks 3-6
 *        (I2C register file, PWM recorder, Task_sleep accumulator,
 *        osal_snv RAM store).
 *
 *        The Task 1/2 mock globals (SPI, PIN, Seconds) remain defined in
 *        each test file per the existing convention; link this file in
 *        addition when a test pulls in I2C / PWM / flash-backed modules.
 *
 *****************************************************************************/

#include "ti/drivers/I2C.h"
#include "ti/drivers/PWM.h"
#include "osal_snv.h"

#include <stdint.h>

mock_i2c_dev_t  mock_i2c_dev[MOCK_I2C_DEVICES];
int             mock_i2c_fail_next;
int             mock_i2c_xfer_count;
I2C_Config      mock_i2c_config;
mock_i2c_write_hook_t mock_i2c_write_hook;

mock_pwm_t      mock_pwm;

uint32_t        mock_task_sleep_ticks;

mock_snv_item_t mock_snv[MOCK_SNV_ITEMS];

/******************************************************************************
 *
 * @file  lis2dw12.h
 *
 * @brief I2C driver for the ST LIS2DW12 accelerometer (Task 4).
 *
 *        Configuration: low-power mode 1, ODR 12.5 Hz, +/-2 g, BDU,
 *        hardware pedometer enabled (register bank B), wake-up interrupt
 *        routed to INT1 (wrist-raise / actigraphy movement proxy).
 *
 *        Feature guard: KEPLER_HAS_LIS2DW12.  When 0, init returns true
 *        and every read returns zeros so callers need no guards.
 *
 *        Task context only — blocking I2C transfers.
 *
 *****************************************************************************/

#ifndef LIS2DW12_H
#define LIS2DW12_H

#include <stdint.h>
#include <stdbool.h>

/*--- Bank A registers ------------------------------------------------------*/
#define LIS2DW12_REG_FUNC_CFG_ACCESS  0x1E   /* bit1: bank B enable         */
#define LIS2DW12_REG_WHO_AM_I         0x0F   /* reads 0x44                  */
#define LIS2DW12_REG_CTRL1            0x20
#define LIS2DW12_REG_CTRL2            0x21
#define LIS2DW12_REG_CTRL3            0x22
#define LIS2DW12_REG_CTRL4_INT1       0x23
#define LIS2DW12_REG_CTRL5_INT2       0x24
#define LIS2DW12_REG_CTRL6            0x25
#define LIS2DW12_REG_STATUS           0x27
#define LIS2DW12_REG_OUT_X_L          0x28   /* X/Y/Z burst from here       */
#define LIS2DW12_REG_WAKE_UP_THS      0x34
#define LIS2DW12_REG_WAKE_UP_DUR      0x35
#define LIS2DW12_REG_WAKE_UP_SRC      0x38   /* read to clear INT1 latch    */
#define LIS2DW12_REG_CTRL7            0x3F

/*--- Bank B (embedded function) registers ----------------------------------*/
#define LIS2DW12_REGB_PEDO_THS        0x2F
#define LIS2DW12_REGB_STEP_COUNTER_L  0x3A
#define LIS2DW12_REGB_STEP_COUNTER_H  0x3B
#define LIS2DW12_REGB_FUNC_CK_GATE    0x3C   /* STEP_D_EN, PEDO_RST_STEP    */

/*--- Identity ----------------------------------------------------------------*/
#define LIS2DW12_WHO_AM_I_VALUE       0x44

/*--- Wrist-raise tuning (spec 04: start 16 ~= 0.5 g; 12..24 range) ---------*/
#define LIS2DW12_WAKE_THS_DEFAULT     0x10
#define LIS2DW12_WAKE_DUR_DEFAULT     0x02   /* 2 samples @ 12.5 Hz = 160ms */

/*==========================================================================*
 *  Public API                                                               *
 *==========================================================================*/

/* Verify WHO_AM_I, reset, configure ODR/power/pedometer/wrist-raise.       *
 * Returns false if the device is not found on the bus.                     */
bool lis2dw12_init(void);

/* Read the 16-bit hardware pedometer counter (bank B).                     */
uint16_t lis2dw12_read_steps(void);

/* Reset the hardware step counter to zero (PEDO_RST_STEP).                 */
void lis2dw12_reset_steps(void);

/* Read and clear WAKE_UP_SRC (releases the latched INT1 line).             *
 * Returns the register value for diagnostics.                              */
uint8_t lis2dw12_clear_wakeup_src(void);

/* Raw acceleration, 14-bit left-justified, +/-2 g FS.                      */
void lis2dw12_read_accel(int16_t *x, int16_t *y, int16_t *z);

#endif /* LIS2DW12_H */

/******************************************************************************
 *
 * @file  lis2dw12.c
 *
 * @brief LIS2DW12 driver implementation (spec 04 init sequence).
 *
 *        Bank switching: pedometer registers live in bank B; access is
 *        bracketed by FUNC_CFG_ACCESS = 0x02 / 0x00.  Never leave bank B
 *        selected — every helper restores bank A before returning.
 *
 *****************************************************************************/

#include "lis2dw12.h"
#include "../kepler_config.h"

#if KEPLER_HAS_LIS2DW12

#include <stddef.h>

#include "../kepler_i2c.h"

#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Clock.h>

#define MS_TO_TICKS(ms)  ((uint32_t)(ms) * 1000u / Clock_tickPeriod)

#define LIS_WR(reg, val)  kepler_i2c_write_reg(I2C_ADDR_LIS2DW12, (reg), (val))
#define LIS_RD(reg, p)    kepler_i2c_read_reg(I2C_ADDR_LIS2DW12, (reg), (p))

/*--- Config values (spec 04) ------------------------------------------------*/
#define CTRL1_LP1_ODR12HZ5   0x10   /* LP mode 1, ODR = 12.5 Hz             */
#define CTRL2_SOFT_RESET     0x40
#define CTRL2_BDU_ADDR_INC   0x0C   /* BDU=1, IF_ADD_INC=1                  */
#define CTRL6_FS2G_LOWNOISE  0x04   /* FS=+/-2g, LOW_NOISE=1                */
#define CTRL3_LIR            0x10   /* latch interrupts                     */
#define CTRL4_INT1_WU        0x20   /* route wake-up to INT1                */
#define CTRL7_INT_ENABLE     0x20   /* INTERRUPTS_ENABLE                    */

#define BANK_B               0x02
#define BANK_A               0x00

#define CK_GATE_STEP_EN      0x10   /* STEP_D_EN                            */
#define CK_GATE_STEP_RST     0x02   /* PEDO_RST_STEP                        */

static bool s_present;

/*==========================================================================*
 *  Public API                                                               *
 *==========================================================================*/

bool lis2dw12_init(void)
{
    uint8_t who;
    uint8_t ctrl;

    s_present = false;

    if (!kepler_i2c_open())                                  { return false; }

    /* 1. Identity check */
    if (!LIS_RD(LIS2DW12_REG_WHO_AM_I, &who))                { return false; }
    if (who != LIS2DW12_WHO_AM_I_VALUE)                      { return false; }

    /* 2. Software reset, wait 5 ms */
    if (!LIS_WR(LIS2DW12_REG_CTRL2, CTRL2_SOFT_RESET))       { return false; }
    Task_sleep(MS_TO_TICKS(5u));

    /* 3-4. Power mode, scale, BDU + address auto-increment */
    if (!LIS_WR(LIS2DW12_REG_CTRL1, CTRL1_LP1_ODR12HZ5))     { return false; }
    if (!LIS_WR(LIS2DW12_REG_CTRL6, CTRL6_FS2G_LOWNOISE))    { return false; }
    if (!LIS_WR(LIS2DW12_REG_CTRL2, CTRL2_BDU_ADDR_INC))     { return false; }

    /* 5. Enable hardware pedometer (bank B) */
    if (!LIS_WR(LIS2DW12_REG_FUNC_CFG_ACCESS, BANK_B))       { return false; }
    if (!LIS_WR(LIS2DW12_REGB_FUNC_CK_GATE, CK_GATE_STEP_EN)){ return false; }
    if (!LIS_WR(LIS2DW12_REG_FUNC_CFG_ACCESS, BANK_A))       { return false; }

    /* 6. Wrist-raise: wake-up interrupt on INT1 */
    if (!LIS_WR(LIS2DW12_REG_WAKE_UP_THS, LIS2DW12_WAKE_THS_DEFAULT))
                                                             { return false; }
    if (!LIS_WR(LIS2DW12_REG_WAKE_UP_DUR, LIS2DW12_WAKE_DUR_DEFAULT))
                                                             { return false; }
    if (!LIS_RD(LIS2DW12_REG_CTRL4_INT1, &ctrl))             { return false; }
    if (!LIS_WR(LIS2DW12_REG_CTRL4_INT1, ctrl | CTRL4_INT1_WU))
                                                             { return false; }
    if (!LIS_RD(LIS2DW12_REG_CTRL3, &ctrl))                  { return false; }
    if (!LIS_WR(LIS2DW12_REG_CTRL3, ctrl | CTRL3_LIR))       { return false; }

    /* 7. Global interrupt enable */
    if (!LIS_WR(LIS2DW12_REG_CTRL7, CTRL7_INT_ENABLE))       { return false; }

    s_present = true;
    return true;
}

uint16_t lis2dw12_read_steps(void)
{
    uint8_t lo = 0u;
    uint8_t hi = 0u;

    if (!s_present)                                          { return 0u; }

    if (!LIS_WR(LIS2DW12_REG_FUNC_CFG_ACCESS, BANK_B))       { return 0u; }
    (void)LIS_RD(LIS2DW12_REGB_STEP_COUNTER_L, &lo);
    (void)LIS_RD(LIS2DW12_REGB_STEP_COUNTER_H, &hi);
    (void)LIS_WR(LIS2DW12_REG_FUNC_CFG_ACCESS, BANK_A);

    return (uint16_t)(((uint16_t)hi << 8) | lo);
}

void lis2dw12_reset_steps(void)
{
    if (!s_present)                                          { return; }

    if (!LIS_WR(LIS2DW12_REG_FUNC_CFG_ACCESS, BANK_B))       { return; }
    (void)LIS_WR(LIS2DW12_REGB_FUNC_CK_GATE,
                 CK_GATE_STEP_EN | CK_GATE_STEP_RST);
    (void)LIS_WR(LIS2DW12_REGB_FUNC_CK_GATE, CK_GATE_STEP_EN);
    (void)LIS_WR(LIS2DW12_REG_FUNC_CFG_ACCESS, BANK_A);
}

uint8_t lis2dw12_clear_wakeup_src(void)
{
    uint8_t src = 0u;

    if (!s_present)                                          { return 0u; }
    (void)LIS_RD(LIS2DW12_REG_WAKE_UP_SRC, &src);
    return src;
}

void lis2dw12_read_accel(int16_t *x, int16_t *y, int16_t *z)
{
    uint8_t raw[6] = {0};

    if (x != NULL) { *x = 0; }
    if (y != NULL) { *y = 0; }
    if (z != NULL) { *z = 0; }

    if (!s_present)                                          { return; }

    if (!kepler_i2c_read_regs(I2C_ADDR_LIS2DW12,
                              LIS2DW12_REG_OUT_X_L, raw, 6u)) { return; }

    if (x != NULL) { *x = (int16_t)(((uint16_t)raw[1] << 8) | raw[0]); }
    if (y != NULL) { *y = (int16_t)(((uint16_t)raw[3] << 8) | raw[2]); }
    if (z != NULL) { *z = (int16_t)(((uint16_t)raw[5] << 8) | raw[4]); }
}

#else /* !KEPLER_HAS_LIS2DW12 */

#include <stddef.h>

bool     lis2dw12_init(void)             { return true; }
uint16_t lis2dw12_read_steps(void)       { return 0u;  }
void     lis2dw12_reset_steps(void)      {}
uint8_t  lis2dw12_clear_wakeup_src(void) { return 0u;  }
void     lis2dw12_read_accel(int16_t *x, int16_t *y, int16_t *z)
{
    if (x != NULL) { *x = 0; }
    if (y != NULL) { *y = 0; }
    if (z != NULL) { *z = 0; }
}

#endif /* KEPLER_HAS_LIS2DW12 */

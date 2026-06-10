/******************************************************************************
 *
 * @file  kepler_i2c.h
 *
 * @brief Shared I2C bus access for the F91 Kepler firmware.
 *
 *        One physical bus (I2C0, pins from kepler_config.h) is shared by
 *        the DRV2605L (0x5A), LIS2DW12 (0x18) and ST25DV04K (0x53/0x57).
 *        This module owns the single TI I2C driver handle; peripheral
 *        drivers go through the register helpers below and never open the
 *        bus themselves.
 *
 *        Compiled away entirely when no I2C peripheral flag is enabled —
 *        all functions become stubs returning false.
 *
 *        Task context only: I2C_transfer blocks.  Never call from ISR/Swi.
 *
 *****************************************************************************/

#ifndef KEPLER_I2C_H
#define KEPLER_I2C_H

#include <stdint.h>
#include <stdbool.h>

/*--- 7-bit device addresses (Plan Maestro §5.3) ---------------------------*/
#define I2C_ADDR_DRV2605L      0x5A
#define I2C_ADDR_LIS2DW12      0x18    /* SDO/SA0 low                      */
#define I2C_ADDR_ST25DV_USER   0x53
#define I2C_ADDR_ST25DV_SYS    0x57

/*==========================================================================*
 *  Public API                                                               *
 *==========================================================================*/

/* Open I2C0 at 400 kHz.  Idempotent — safe to call from each driver init. *
 * Returns true if the bus is (already) open.                              */
bool kepler_i2c_open(void);

/* Write a single 8-bit register.                                          */
bool kepler_i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t val);

/* Read a single 8-bit register.                                           */
bool kepler_i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t *val);

/* Burst-read `len` consecutive registers starting at `reg`.               *
 * (LIS2DW12 needs IF_ADD_INC set in CTRL2 for auto-increment.)            */
bool kepler_i2c_read_regs(uint8_t addr, uint8_t reg,
                          uint8_t *buf, uint8_t len);

#endif /* KEPLER_I2C_H */

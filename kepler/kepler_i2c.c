/******************************************************************************
 *
 * @file  kepler_i2c.c
 *
 * @brief Shared I2C bus implementation on the TI I2C driver.
 *
 *****************************************************************************/

#include "kepler_i2c.h"
#include "kepler_config.h"

#define KEPLER_USES_I2C \
    (KEPLER_HAS_DRV2605L || KEPLER_HAS_LIS2DW12 || KEPLER_HAS_ST25DV)

#if KEPLER_USES_I2C

#include <ti/drivers/I2C.h>

static I2C_Handle s_i2c;

bool kepler_i2c_open(void)
{
    I2C_Params params;

    if (s_i2c != NULL) {
        return true;    /* already open */
    }

    I2C_init();
    I2C_Params_init(&params);
    params.bitRate        = I2C_400kHz;
    params.transferMode   = I2C_MODE_BLOCKING;

    s_i2c = I2C_open(KEPLER_I2C_INSTANCE, &params);
    return (s_i2c != NULL);
}

bool kepler_i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t val)
{
    I2C_Transaction t;
    uint8_t         tx[2];

    if (s_i2c == NULL) { return false; }

    tx[0] = reg;
    tx[1] = val;

    t.slaveAddress = addr;
    t.writeBuf     = tx;
    t.writeCount   = 2u;
    t.readBuf      = NULL;
    t.readCount    = 0u;

    return (bool)I2C_transfer(s_i2c, &t);
}

bool kepler_i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t *val)
{
    return kepler_i2c_read_regs(addr, reg, val, 1u);
}

bool kepler_i2c_read_regs(uint8_t addr, uint8_t reg,
                          uint8_t *buf, uint8_t len)
{
    I2C_Transaction t;

    if (s_i2c == NULL || buf == NULL || len == 0u) { return false; }

    t.slaveAddress = addr;
    t.writeBuf     = &reg;
    t.writeCount   = 1u;
    t.readBuf      = buf;
    t.readCount    = len;

    return (bool)I2C_transfer(s_i2c, &t);
}

#else /* !KEPLER_USES_I2C */

bool kepler_i2c_open(void)                                   { return false; }
bool kepler_i2c_write_reg(uint8_t a, uint8_t r, uint8_t v)
                                  { (void)a; (void)r; (void)v; return false; }
bool kepler_i2c_read_reg(uint8_t a, uint8_t r, uint8_t *v)
                                  { (void)a; (void)r; (void)v; return false; }
bool kepler_i2c_read_regs(uint8_t a, uint8_t r, uint8_t *b, uint8_t l)
                        { (void)a; (void)r; (void)b; (void)l; return false; }

#endif /* KEPLER_USES_I2C */

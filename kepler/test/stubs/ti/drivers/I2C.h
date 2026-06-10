#ifndef STUB_I2C_H
#define STUB_I2C_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Register-map-emulating I2C stub for host tests.
 *
 * Maintains a 256-byte register file per known device address.
 * Write transfers of {reg, val} store val; combined write/read transfers
 * return consecutive registers starting at reg (auto-increment).
 *
 * Tests poke/peek registers directly via mock_i2c_reg(addr) and can force
 * the next N transfers to fail via mock_i2c_fail_next.                   */

#define MOCK_I2C_DEVICES 4

typedef struct {
    uint8_t addr;
    uint8_t regs[256];
} mock_i2c_dev_t;

extern mock_i2c_dev_t mock_i2c_dev[MOCK_I2C_DEVICES];
extern int            mock_i2c_fail_next;     /* fail this many transfers  */
extern int            mock_i2c_xfer_count;

/* Optional hook called after every register write — lets tests emulate   *
 * device-side behaviour (e.g. DRV2605L GO self-clearing).  NULL = off.   */
typedef void (*mock_i2c_write_hook_t)(uint8_t addr, uint8_t reg, uint8_t val);
extern mock_i2c_write_hook_t mock_i2c_write_hook;

typedef struct { int dummy; } I2C_Config;
typedef I2C_Config *I2C_Handle;

typedef enum { I2C_100kHz = 0, I2C_400kHz = 1 } I2C_BitRate;
typedef enum { I2C_MODE_BLOCKING = 0, I2C_MODE_CALLBACK = 1 } I2C_TransferMode;

typedef struct {
    I2C_BitRate      bitRate;
    I2C_TransferMode transferMode;
} I2C_Params;

typedef struct {
    uint8_t  slaveAddress;
    void    *writeBuf;
    size_t   writeCount;
    void    *readBuf;
    size_t   readCount;
} I2C_Transaction;

extern I2C_Config mock_i2c_config;

static inline void I2C_init(void) {}
static inline void I2C_Params_init(I2C_Params *p) {
    p->bitRate      = I2C_100kHz;
    p->transferMode = I2C_MODE_BLOCKING;
}

static inline I2C_Handle I2C_open(unsigned idx, I2C_Params *p) {
    (void)idx; (void)p;
    return &mock_i2c_config;
}

static inline mock_i2c_dev_t *mock_i2c_find(uint8_t addr) {
    for (int i = 0; i < MOCK_I2C_DEVICES; i++) {
        if (mock_i2c_dev[i].addr == addr) { return &mock_i2c_dev[i]; }
    }
    return NULL;
}

/* Direct register access for test setup/assertions. */
static inline uint8_t *mock_i2c_reg(uint8_t addr) {
    mock_i2c_dev_t *d = mock_i2c_find(addr);
    return d ? d->regs : NULL;
}

static inline void mock_i2c_reset(void) {
    static const uint8_t addrs[MOCK_I2C_DEVICES] = {0x5A, 0x18, 0x53, 0x57};
    for (int i = 0; i < MOCK_I2C_DEVICES; i++) {
        mock_i2c_dev[i].addr = addrs[i];
        memset(mock_i2c_dev[i].regs, 0, 256);
    }
    mock_i2c_fail_next  = 0;
    mock_i2c_xfer_count = 0;
    mock_i2c_write_hook = 0;
}

static inline int I2C_transfer(I2C_Handle h, I2C_Transaction *t) {
    (void)h;
    mock_i2c_xfer_count++;
    if (mock_i2c_fail_next > 0) { mock_i2c_fail_next--; return 0; }

    mock_i2c_dev_t *d = mock_i2c_find(t->slaveAddress);
    if (d == NULL) { return 0; }   /* NACK: unknown device                 */

    const uint8_t *w = (const uint8_t *)t->writeBuf;

    if (t->readCount > 0) {
        /* Combined write(reg) + read(len): auto-increment read.           */
        uint8_t reg = (t->writeCount > 0) ? w[0] : 0;
        uint8_t *r  = (uint8_t *)t->readBuf;
        for (size_t i = 0; i < t->readCount; i++) {
            r[i] = d->regs[(uint8_t)(reg + i)];
        }
    } else if (t->writeCount >= 2) {
        /* Write transfer: {reg, val, val, ...} auto-increment.            */
        uint8_t reg = w[0];
        for (size_t i = 1; i < t->writeCount; i++) {
            d->regs[(uint8_t)(reg + i - 1)] = w[i];
            if (mock_i2c_write_hook) {
                mock_i2c_write_hook(t->slaveAddress,
                                    (uint8_t)(reg + i - 1), w[i]);
            }
        }
    }
    return 1;
}

#endif /* STUB_I2C_H */

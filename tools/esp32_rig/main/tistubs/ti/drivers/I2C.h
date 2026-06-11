/******************************************************************************
 * TI I2C driver shim -> ESP-IDF i2c master (rig).
 *****************************************************************************/

#ifndef RIGSHIM_I2C_H
#define RIGSHIM_I2C_H

#include <stdint.h>
#include <stddef.h>

typedef struct rigi2c_s *I2C_Handle;

typedef enum { I2C_100kHz = 0, I2C_400kHz = 1 } I2C_BitRate;
typedef enum { I2C_MODE_BLOCKING = 0 } I2C_TransferMode;

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

void       I2C_init(void);
void       I2C_Params_init(I2C_Params *p);
I2C_Handle I2C_open(uint32_t idx, I2C_Params *p);
int        I2C_transfer(I2C_Handle h, I2C_Transaction *t);

#endif /* RIGSHIM_I2C_H */

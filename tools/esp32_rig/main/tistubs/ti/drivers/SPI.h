/******************************************************************************
 * TI SPI driver shim -> ESP-IDF spi_master (rig).
 * Blocking transfers only; CS is handled manually by the caller
 * (sharp_lcd drives its ACTIVE-HIGH CS through the PIN shim).
 *****************************************************************************/

#ifndef RIGSHIM_SPI_H
#define RIGSHIM_SPI_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct rigspi_s *SPI_Handle;

#define SPI_MODE_BLOCKING 0
#define SPI_POL0_PHA0     0

typedef struct {
    uint16_t count;
    void    *txBuf;
    void    *rxBuf;
} SPI_Transaction;

typedef struct {
    int      transferMode;
    uint32_t bitRate;
    uint8_t  dataSize;
    int      frameFormat;
} SPI_Params;

void       SPI_init(void);
void       SPI_Params_init(SPI_Params *p);
SPI_Handle SPI_open(uint32_t idx, SPI_Params *p);
bool       SPI_transfer(SPI_Handle h, SPI_Transaction *txn);

#endif /* RIGSHIM_SPI_H */

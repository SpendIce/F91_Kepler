#ifndef STUB_SPI_H
#define STUB_SPI_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../../mock_state.h"

/* Opaque handle */
typedef void *SPI_Handle;

/* Transfer mode */
#define SPI_MODE_BLOCKING 0

/* Frame format */
#define SPI_POL0_PHA0 0

/* Transaction */
typedef struct {
    uint16_t count;
    void    *txBuf;
    void    *rxBuf;
} SPI_Transaction;

/* Params */
typedef struct {
    int      transferMode;
    uint32_t bitRate;
    uint8_t  dataSize;
    int      frameFormat;
} SPI_Params;

/*--- Stub implementations ------------------------------------------------*/

static int s_spi_dummy;

static inline void SPI_init(void) { (void)0; }

static inline void SPI_Params_init(SPI_Params *p) {
    memset(p, 0, sizeof(*p));
}

static inline SPI_Handle SPI_open(uint32_t idx, SPI_Params *p) {
    (void)idx; (void)p;
    return (SPI_Handle)&s_spi_dummy;
}

static inline bool SPI_transfer(SPI_Handle h, SPI_Transaction *txn) {
    (void)h;
    if (txn->txBuf && txn->count <= MOCK_SPI_BUFSIZE) {
        memcpy(mock_spi_buf, txn->txBuf, txn->count);
        mock_spi_len = txn->count;
    }
    mock_spi_call_count++;
    return true;
}

#endif /* STUB_SPI_H */

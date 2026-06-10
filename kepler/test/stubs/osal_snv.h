#ifndef STUB_OSAL_SNV_H
#define STUB_OSAL_SNV_H

#include <stdint.h>
#include <string.h>

/* RAM-backed osal_snv stub for host tests.
 * IDs 0x80..0x8F (customer range), max 128 bytes per item.               */

#define MOCK_SNV_BASE   0x80u
#define MOCK_SNV_ITEMS  16u
#define MOCK_SNV_LEN    128u

typedef struct {
    uint8_t  data[MOCK_SNV_LEN];
    uint16_t len;       /* 0 = never written                               */
} mock_snv_item_t;

extern mock_snv_item_t mock_snv[MOCK_SNV_ITEMS];

static inline void mock_snv_reset(void) {
    memset(mock_snv, 0, sizeof(mock_snv_item_t) * MOCK_SNV_ITEMS);
}

static inline int osal_snv_write(uint8_t id, uint16_t len, void *buf) {
    if (id < MOCK_SNV_BASE || id >= MOCK_SNV_BASE + MOCK_SNV_ITEMS) return 1;
    if (len > MOCK_SNV_LEN) return 1;
    memcpy(mock_snv[id - MOCK_SNV_BASE].data, buf, len);
    mock_snv[id - MOCK_SNV_BASE].len = len;
    return 0;
}

static inline int osal_snv_read(uint8_t id, uint16_t len, void *buf) {
    if (id < MOCK_SNV_BASE || id >= MOCK_SNV_BASE + MOCK_SNV_ITEMS) return 1;
    if (mock_snv[id - MOCK_SNV_BASE].len == 0u) return 1;   /* never written */
    if (len > MOCK_SNV_LEN) return 1;
    memcpy(buf, mock_snv[id - MOCK_SNV_BASE].data, len);
    return 0;
}

#endif /* STUB_OSAL_SNV_H */

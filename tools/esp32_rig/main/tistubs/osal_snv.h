#ifndef RIGSHIM_OSAL_SNV_H
#define RIGSHIM_OSAL_SNV_H

#include <stdint.h>

/* osal_snv shim -> ESP-IDF NVS ("kepler" namespace, key = "snv<id>").     *
 * Returns 0 on success (matches the TI SUCCESS convention used by         *
 * flash_store.c).                                                          */
int osal_snv_read(uint8_t id, uint16_t len, void *buf);
int osal_snv_write(uint8_t id, uint16_t len, void *buf);

#endif /* RIGSHIM_OSAL_SNV_H */

#ifndef RIGSHIM_HWI_H
#define RIGSHIM_HWI_H

#include <stdint.h>

/* Critical section shim — FreeRTOS spinlock underneath.                   */
uint32_t Hwi_disable(void);
void     Hwi_restore(uint32_t key);

#endif /* RIGSHIM_HWI_H */

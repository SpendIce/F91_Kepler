#ifndef STUB_HWI_H
#define STUB_HWI_H

#include <stdint.h>

/* Interrupt lock stub — host tests are single-threaded.                  */

static inline uint32_t Hwi_disable(void)        { return 0u; }
static inline void     Hwi_restore(uint32_t k)  { (void)k;   }

#endif /* STUB_HWI_H */

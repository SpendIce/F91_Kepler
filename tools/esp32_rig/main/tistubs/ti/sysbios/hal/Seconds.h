#ifndef RIGSHIM_SECONDS_H
#define RIGSHIM_SECONDS_H

#include <stdint.h>

/* Settable wall-clock over esp_timer (epoch base + monotonic offset).     */
uint32_t Seconds_get(void);
void     Seconds_set(uint32_t t);

#endif /* RIGSHIM_SECONDS_H */

#ifndef STUB_SECONDS_H
#define STUB_SECONDS_H

#include <stdint.h>
#include "../../mock_state.h"

/* TI SDK Seconds module — backed by mock_seconds_value for host tests.   */

static inline uint32_t Seconds_get(void) {
    return mock_seconds_value;
}

static inline void Seconds_set(uint32_t t) {
    mock_seconds_value = t;
}

#endif /* STUB_SECONDS_H */

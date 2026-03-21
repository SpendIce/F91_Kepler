#ifndef MOCK_STATE_H
#define MOCK_STATE_H

#include <stdint.h>

/*==========================================================================*
 *  Mock SPI recording                                                      *
 *==========================================================================*/

#define MOCK_SPI_BUFSIZE 4096

extern uint8_t  mock_spi_buf[MOCK_SPI_BUFSIZE];
extern uint16_t mock_spi_len;
extern int      mock_spi_call_count;

/*==========================================================================*
 *  Mock PIN output recording                                               *
 *==========================================================================*/

#define MOCK_PIN_LOG_SIZE 64

typedef struct {
    uint32_t pin;
    uint8_t  val;
} mock_pin_event_t;

extern mock_pin_event_t mock_pin_log[MOCK_PIN_LOG_SIZE];
extern int              mock_pin_log_count;

/*==========================================================================*
 *  Mock PIN interrupt callback                                             *
 *==========================================================================*/

/* Generic function pointer type — cast to/from PIN_IntCb in PIN.h.       */
typedef void (*mock_pin_int_cb_t)(void *handle, uint32_t pinId);

extern mock_pin_int_cb_t mock_pin_int_cb;
extern void             *mock_pin_int_handle;

/* Input state for PIN_getInputValue() — indexed by IOID (0-31).          */
extern uint8_t mock_pin_input[32];

static inline void mock_pin_set_input(uint32_t ioid, uint8_t val) {
    if (ioid < 32u) { mock_pin_input[ioid] = val; }
}

/* Simulate a GPIO edge on pinId — calls the registered ISR.              */
static inline void mock_pin_trigger_interrupt(uint32_t pinId) {
    if (mock_pin_int_cb) {
        mock_pin_int_cb(mock_pin_int_handle, pinId);
    }
}

/*==========================================================================*
 *  Mock Seconds (RTC)                                                      *
 *==========================================================================*/

extern uint32_t mock_seconds_value;

/*==========================================================================*
 *  Reset                                                                   *
 *==========================================================================*/

void mock_spi_reset(void);

#endif /* MOCK_STATE_H */

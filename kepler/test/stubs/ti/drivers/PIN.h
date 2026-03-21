#ifndef STUB_PIN_H
#define STUB_PIN_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../../mock_state.h"

/* Opaque types */
typedef void    *PIN_Handle;
typedef uint32_t PIN_Id;
typedef struct { int dummy; } PIN_State;
typedef uint32_t PIN_Config;

/* Output config flags */
#define PIN_GPIO_OUTPUT_EN  0x00010000u
#define PIN_GPIO_LOW        0x00020000u
#define PIN_PUSHPULL        0x00040000u
#define PIN_TERMINATE       0xFFFFFFFFu

/* Input / interrupt config flags */
#define PIN_INPUT_EN        0x01000000u
#define PIN_PULLUP          0x00002000u
#define PIN_PULLDOWN        0x00001000u
#define PIN_BM_IRQ          0x0F000000u
#define PIN_IRQ_DIS         0x00000000u
#define PIN_IRQ_NEGEDGE     0x00100000u
#define PIN_IRQ_POSEDGE     0x00200000u

/* Stub state */
static int s_pin_dummy;

static inline PIN_Handle PIN_open(PIN_State *state, PIN_Config *config) {
    (void)state; (void)config;
    return (PIN_Handle)&s_pin_dummy;
}

static inline void PIN_close(PIN_Handle h) { (void)h; }

static inline void PIN_setOutputValue(PIN_Handle h, uint32_t pin, uint8_t val) {
    (void)h;
    if (mock_pin_log_count < MOCK_PIN_LOG_SIZE) {
        mock_pin_log[mock_pin_log_count].pin = pin;
        mock_pin_log[mock_pin_log_count].val = val;
        mock_pin_log_count++;
    }
}

/* Interrupt callback type */
typedef void (*PIN_IntCb)(PIN_Handle handle, PIN_Id pinId);

static inline int PIN_registerIntCb(PIN_Handle handle, PIN_IntCb cb) {
    mock_pin_int_handle = handle;
    mock_pin_int_cb     = (mock_pin_int_cb_t)cb;
    return 0;
}

/* Stub: records config changes but takes no action (IRQ en/dis is a no-op) */
static inline void PIN_setConfig(PIN_Handle h, uint32_t bmask,
                                 PIN_Config config) {
    (void)h; (void)bmask; (void)config;
}

/* Returns the mock input value set via mock_pin_set_input(). */
static inline uint8_t PIN_getInputValue(PIN_Id pinId) {
    if (pinId < 32u) { return mock_pin_input[pinId]; }
    return 0u;
}

#endif /* STUB_PIN_H */

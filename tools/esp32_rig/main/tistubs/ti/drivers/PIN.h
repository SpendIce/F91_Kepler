/******************************************************************************
 * TI PIN driver shim -> ESP32 GPIO (rig).
 *
 * Only the subset the kepler display driver uses is implemented:
 * output pins (CS/DISP/VCOM) and PIN_setOutputValue.  Interrupt-input
 * support (buttons/wrist-raise) is wired separately in rig_main.c via
 * native ESP-IDF GPIO ISRs — the rig does not compile buttons.c.
 *****************************************************************************/

#ifndef RIGSHIM_PIN_H
#define RIGSHIM_PIN_H

#include <stdint.h>
#include <stddef.h>

typedef uint32_t PIN_Config;
typedef uint32_t PIN_Id;
typedef struct { int opened; } PIN_State;
typedef PIN_State *PIN_Handle;

/* Config flags — values irrelevant, must be distinct bits above IOID.     */
#define PIN_TERMINATE          0xFFFFFFFFu
#define PIN_GPIO_OUTPUT_EN     (1u << 8)
#define PIN_GPIO_LOW           (0u << 9)
#define PIN_GPIO_HIGH          (1u << 9)
#define PIN_PUSHPULL           (1u << 10)
#define PIN_INPUT_EN           (1u << 11)
#define PIN_PULLUP             (1u << 12)
#define PIN_PULLDOWN           (1u << 13)
#define PIN_IRQ_DIS            (0u << 14)
#define PIN_IRQ_NEGEDGE        (1u << 14)
#define PIN_IRQ_POSEDGE        (2u << 14)
#define PIN_BM_IRQ             (3u << 14)

#define PIN_ID_MASK            0xFFu

typedef void (*PIN_IntCb)(PIN_Handle handle, PIN_Id pinId);

/* Implemented in rigshim.c */
PIN_Handle PIN_open(PIN_State *state, const PIN_Config *config);
void       PIN_close(PIN_Handle h);
void       PIN_setOutputValue(PIN_Handle h, uint32_t ioid, uint8_t val);
uint8_t    PIN_getInputValue(uint32_t ioid);
void       PIN_setConfig(PIN_Handle h, uint32_t bmask, PIN_Config cfg);
void       PIN_registerIntCb(PIN_Handle h, PIN_IntCb cb);

#endif /* RIGSHIM_PIN_H */

/******************************************************************************
 *
 * @file  PIN.h  (CC26x2 port shim)
 *
 * @brief TI PIN driver API over the SimpleLink SDK 7.x GPIO driver.
 *
 *        The PIN driver was removed in SDK 7.x; the kepler modules that
 *        touch pins (input/buttons.c, display/sharp_lcd.c,
 *        accel/wrist_raise.c) keep their PIN-based code and this shim
 *        (PIN.h + pin_shim.c) translates to GPIO_* calls — same pattern
 *        as the host-test stubs and the ESP32 rig, proven twice.
 *
 *        Assumption (document in SysConfig): the GPIO driver is configured
 *        so that GPIO index == DIO number for every pin this project
 *        uses.  On CC26x2 the GPIO driver indexes rawDIOs when all pins
 *        are exposed in SysConfig.
 *
 *****************************************************************************/

#ifndef CC26X2_SHIM_PIN_H
#define CC26X2_SHIM_PIN_H

#include <stdint.h>
#include <stddef.h>

typedef uint32_t PIN_Config;
typedef uint32_t PIN_Id;
typedef struct { uint8_t opened; } PIN_State;
typedef PIN_State *PIN_Handle;

#define PIN_TERMINATE          0xFFFFFFFFu
#define PIN_ID_MASK            0xFFu

/* Flag bits (values local to the shim, decoded in pin_shim.c)             */
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

typedef void (*PIN_IntCb)(PIN_Handle handle, PIN_Id pinId);

PIN_Handle PIN_open(PIN_State *state, const PIN_Config *config);
void       PIN_close(PIN_Handle h);
void       PIN_setOutputValue(PIN_Handle h, uint32_t ioid, uint8_t val);
uint8_t    PIN_getInputValue(uint32_t ioid);
void       PIN_setConfig(PIN_Handle h, uint32_t bmask, PIN_Config cfg);
void       PIN_registerIntCb(PIN_Handle h, PIN_IntCb cb);

#endif /* CC26X2_SHIM_PIN_H */

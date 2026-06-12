/******************************************************************************
 *
 * @file  pin_shim.c
 *
 * @brief PIN-over-GPIO translation for the SimpleLink SDK 7.x port.
 *
 *        STATUS: PORT SKELETON — written against the SDK 7.x GPIO driver
 *        API from documentation; compile against the real SDK at board
 *        bring-up.  The GPIO callback runs in Hwi context, same as the
 *        original PIN ISR — the kepler ISRs only post events, so the
 *        contract holds.
 *
 *****************************************************************************/

#include "ti/drivers/PIN.h"

#include <ti/drivers/GPIO.h>

static PIN_State *s_handle;
static PIN_IntCb  s_cb;

/*--- GPIO callback -> PIN callback ------------------------------------------*/

static void gpio_cb(uint_least8_t index)
{
    if (s_cb != NULL) {
        s_cb(s_handle, (PIN_Id)index);
    }
}

/*--- Flag decode --------------------------------------------------------------*/

static GPIO_PinConfig decode(PIN_Config c)
{
    GPIO_PinConfig g = 0;

    if (c & PIN_GPIO_OUTPUT_EN) {
        g  = GPIO_CFG_OUTPUT;
        g |= (c & PIN_GPIO_HIGH) ? GPIO_CFG_OUT_HIGH : GPIO_CFG_OUT_LOW;
    } else {
        g = GPIO_CFG_INPUT;
        if (c & PIN_PULLUP)   { g |= GPIO_CFG_PULL_UP_INTERNAL; }
        if (c & PIN_PULLDOWN) { g |= GPIO_CFG_PULL_DOWN_INTERNAL; }
    }

    switch (c & PIN_BM_IRQ) {
        case PIN_IRQ_NEGEDGE: g |= GPIO_CFG_IN_INT_FALLING; break;
        case PIN_IRQ_POSEDGE: g |= GPIO_CFG_IN_INT_RISING;  break;
        default:                                            break;
    }
    return g;
}

/*==========================================================================*
 *  PIN API                                                                  *
 *==========================================================================*/

PIN_Handle PIN_open(PIN_State *state, const PIN_Config *config)
{
    GPIO_init();   /* idempotent */

    for (const PIN_Config *c = config; *c != PIN_TERMINATE; c++) {
        uint32_t ioid = *c & PIN_ID_MASK;
        GPIO_setConfig((uint_least8_t)ioid, decode(*c));
        if ((*c & PIN_BM_IRQ) != PIN_IRQ_DIS) {
            GPIO_setCallback((uint_least8_t)ioid, gpio_cb);
            GPIO_enableInt((uint_least8_t)ioid);
        }
    }
    state->opened = 1u;
    s_handle = state;
    return state;
}

void PIN_close(PIN_Handle h)
{
    (void)h;
}

void PIN_setOutputValue(PIN_Handle h, uint32_t ioid, uint8_t val)
{
    (void)h;
    GPIO_write((uint_least8_t)ioid, val);
}

uint8_t PIN_getInputValue(uint32_t ioid)
{
    return (uint8_t)GPIO_read((uint_least8_t)ioid);
}

void PIN_setConfig(PIN_Handle h, uint32_t bmask, PIN_Config cfg)
{
    (void)h;
    uint32_t ioid = cfg & PIN_ID_MASK;

    if (bmask == PIN_BM_IRQ) {
        switch (cfg & PIN_BM_IRQ) {
            case PIN_IRQ_NEGEDGE:
                GPIO_setInterruptConfig((uint_least8_t)ioid,
                                        GPIO_CFG_IN_INT_FALLING
                                        | GPIO_CFG_INT_ENABLE);
                break;
            case PIN_IRQ_POSEDGE:
                GPIO_setInterruptConfig((uint_least8_t)ioid,
                                        GPIO_CFG_IN_INT_RISING
                                        | GPIO_CFG_INT_ENABLE);
                break;
            default:
                GPIO_disableInt((uint_least8_t)ioid);
                break;
        }
    }
}

void PIN_registerIntCb(PIN_Handle h, PIN_IntCb cb)
{
    (void)h;
    s_cb = cb;
}

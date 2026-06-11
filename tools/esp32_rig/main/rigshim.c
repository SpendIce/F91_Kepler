/******************************************************************************
 *
 * @file  rigshim.c
 *
 * @brief TI driver/RTOS API shims over ESP-IDF for the peripheral test
 *        rig.  Lets the kepler/ firmware sources compile and run on an
 *        ESP32 devkit unchanged.
 *
 *        Context note: TI "Swi" callbacks land in the esp_timer task
 *        here.  Every kepler Swi callback is post-only or RAM-only, so a
 *        task context is strictly safer than the original Swi context.
 *
 *****************************************************************************/

#include <string.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "driver/spi_master.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "ti/drivers/PIN.h"
#include "ti/drivers/SPI.h"
#include "ti/drivers/I2C.h"
#include "ti/sysbios/knl/Clock.h"
#include "ti/sysbios/knl/Task.h"
#include "ti/sysbios/knl/Semaphore.h"
#include "ti/sysbios/hal/Hwi.h"
#include "ti/sysbios/hal/Seconds.h"
#include "osal_snv.h"

#include "rig_config.h"

/*==========================================================================*
 *  PIN -> GPIO                                                              *
 *==========================================================================*/

static PIN_State s_pin_singleton;

PIN_Handle PIN_open(PIN_State *state, const PIN_Config *config)
{
    (void)state;
    for (const PIN_Config *c = config; *c != PIN_TERMINATE; c++) {
        int gpio = rig_ioid_to_gpio(*c & PIN_ID_MASK);
        if (gpio < 0) { continue; }

        gpio_config_t io = {
            .pin_bit_mask = 1ULL << gpio,
            .mode         = (*c & PIN_GPIO_OUTPUT_EN) ? GPIO_MODE_OUTPUT
                                                      : GPIO_MODE_INPUT,
            .pull_up_en   = (*c & PIN_PULLUP)   ? GPIO_PULLUP_ENABLE
                                                : GPIO_PULLUP_DISABLE,
            .pull_down_en = (*c & PIN_PULLDOWN) ? GPIO_PULLDOWN_ENABLE
                                                : GPIO_PULLDOWN_DISABLE,
            .intr_type    = GPIO_INTR_DISABLE,
        };
        gpio_config(&io);
        if (*c & PIN_GPIO_OUTPUT_EN) {
            gpio_set_level(gpio, (*c & PIN_GPIO_HIGH) ? 1 : 0);
        }
    }
    s_pin_singleton.opened = 1;
    return &s_pin_singleton;
}

void PIN_close(PIN_Handle h)                       { (void)h; }

void PIN_setOutputValue(PIN_Handle h, uint32_t ioid, uint8_t val)
{
    (void)h;
    int gpio = rig_ioid_to_gpio(ioid);
    if (gpio >= 0) { gpio_set_level(gpio, val ? 1 : 0); }
}

uint8_t PIN_getInputValue(uint32_t ioid)
{
    int gpio = rig_ioid_to_gpio(ioid);
    return (gpio >= 0) ? (uint8_t)gpio_get_level(gpio) : 0u;
}

void PIN_setConfig(PIN_Handle h, uint32_t bmask, PIN_Config cfg)
{
    (void)h; (void)bmask; (void)cfg;   /* IRQ reconfig unused on the rig  */
}

void PIN_registerIntCb(PIN_Handle h, PIN_IntCb cb)
{
    (void)h; (void)cb;                 /* rig wires ISRs natively         */
}

/*==========================================================================*
 *  SPI -> spi_master (VSPI)                                                 *
 *==========================================================================*/

struct rigspi_s { spi_device_handle_t dev; };
static struct rigspi_s s_spi;

void SPI_init(void) {}

void SPI_Params_init(SPI_Params *p)
{
    memset(p, 0, sizeof(*p));
    p->bitRate  = 1000000u;
    p->dataSize = 8u;
}

SPI_Handle SPI_open(uint32_t idx, SPI_Params *p)
{
    (void)idx;

    spi_bus_config_t bus = {
        .mosi_io_num   = RIG_GPIO_LCD_MOSI,
        .miso_io_num   = -1,
        .sclk_io_num   = RIG_GPIO_LCD_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 512,
    };
    if (spi_bus_initialize(SPI3_HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK) {
        return NULL;
    }

    spi_device_interface_config_t dev = {
        .clock_speed_hz = (int)p->bitRate,   /* 1 MHz — Sharp max          */
        .mode           = 0,                 /* POL0 PHA0                  */
        .spics_io_num   = -1,                /* CS manual (ACTIVE HIGH)    */
        .queue_size     = 2,
    };
    if (spi_bus_add_device(SPI3_HOST, &dev, &s_spi.dev) != ESP_OK) {
        return NULL;
    }
    return &s_spi;
}

bool SPI_transfer(SPI_Handle h, SPI_Transaction *txn)
{
    spi_transaction_t t = {
        .length    = (size_t)txn->count * 8u,
        .tx_buffer = txn->txBuf,
        .rx_buffer = NULL,
    };
    return (spi_device_transmit(h->dev, &t) == ESP_OK);
}

/*==========================================================================*
 *  I2C -> i2c master (port 0)                                               *
 *==========================================================================*/

struct rigi2c_s { int port; };
static struct rigi2c_s s_i2c = { I2C_NUM_0 };

void I2C_init(void) {}

void I2C_Params_init(I2C_Params *p)
{
    p->bitRate      = I2C_400kHz;
    p->transferMode = I2C_MODE_BLOCKING;
}

I2C_Handle I2C_open(uint32_t idx, I2C_Params *p)
{
    (void)idx;

    i2c_config_t cfg = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = RIG_GPIO_I2C_SDA,
        .scl_io_num       = RIG_GPIO_I2C_SCL,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = (p->bitRate == I2C_400kHz) ? 400000 : 100000,
    };
    if (i2c_param_config(s_i2c.port, &cfg) != ESP_OK)            return NULL;
    if (i2c_driver_install(s_i2c.port, I2C_MODE_MASTER,
                           0, 0, 0) != ESP_OK)                   return NULL;
    return &s_i2c;
}

int I2C_transfer(I2C_Handle h, I2C_Transaction *t)
{
    esp_err_t rc;

    if (t->readCount > 0) {
        rc = i2c_master_write_read_device(
                 h->port, t->slaveAddress,
                 (const uint8_t *)t->writeBuf, t->writeCount,
                 (uint8_t *)t->readBuf, t->readCount,
                 pdMS_TO_TICKS(100));
    } else {
        rc = i2c_master_write_to_device(
                 h->port, t->slaveAddress,
                 (const uint8_t *)t->writeBuf, t->writeCount,
                 pdMS_TO_TICKS(100));
    }
    return (rc == ESP_OK) ? 1 : 0;
}

/*==========================================================================*
 *  Clock -> esp_timer                                                       *
 *==========================================================================*/

static void clock_dispatch(void *arg)
{
    Clock_Struct *s = (Clock_Struct *)arg;
    if (s->fn) { s->fn(s->arg); }
}

void Clock_Params_init(Clock_Params *p)
{
    p->period    = 0u;
    p->startFlag = FALSE;
    p->arg       = 0u;
}

void Clock_construct(Clock_Struct *s, Clock_FuncPtr fn,
                     uint32_t timeout, Clock_Params *p)
{
    s->fn            = fn;
    s->arg           = (p != NULL) ? p->arg : 0u;
    s->timeout_ticks = timeout;
    s->period_ticks  = (p != NULL) ? p->period : 0u;
    s->constructed   = 1;

    esp_timer_create_args_t args = {
        .callback = clock_dispatch,
        .arg      = s,
        .name     = "kclk",
    };
    esp_timer_handle_t th = NULL;
    esp_timer_create(&args, &th);
    s->esp_handle = th;

    if (p != NULL && p->startFlag) { Clock_start(s); }
}

Clock_Handle Clock_handle(Clock_Struct *s) { return s; }

void Clock_start(Clock_Handle h)
{
    esp_timer_handle_t th = (esp_timer_handle_t)h->esp_handle;
    if (th == NULL) { return; }
    esp_timer_stop(th);   /* restart semantics; ignore not-running error  */
    if (h->period_ticks > 0u) {
        esp_timer_start_periodic(th, (uint64_t)h->period_ticks * 10u);
    } else {
        esp_timer_start_once(th, (uint64_t)h->timeout_ticks * 10u);
    }
}

void Clock_stop(Clock_Handle h)
{
    if (h->esp_handle) { esp_timer_stop((esp_timer_handle_t)h->esp_handle); }
}

void Clock_setTimeout(Clock_Handle h, uint32_t ticks)
{
    h->timeout_ticks = ticks;
}

/*==========================================================================*
 *  Task / Semaphore / Hwi                                                   *
 *==========================================================================*/

void Task_sleep(uint32_t ticks)
{
    uint32_t ms = ticks / 100u;        /* 10 us ticks -> ms                */
    vTaskDelay(pdMS_TO_TICKS(ms ? ms : 1u));
}

void Semaphore_Params_init(Semaphore_Params *p)
{
    p->mode = Semaphore_Mode_COUNTING;
}

void Semaphore_construct(Semaphore_Struct *s, int count, Semaphore_Params *p)
{
    (void)p;
    s->freertos_sem = xSemaphoreCreateCounting(64, (UBaseType_t)count);
}

Semaphore_Handle Semaphore_handle(Semaphore_Struct *s) { return s; }

void Semaphore_post(Semaphore_Handle h)
{
    xSemaphoreGive((SemaphoreHandle_t)h->freertos_sem);
}

int Semaphore_pend(Semaphore_Handle h, uint32_t timeout_ticks)
{
    TickType_t to = (timeout_ticks == 0xFFFFFFFFu)
                  ? portMAX_DELAY
                  : pdMS_TO_TICKS(timeout_ticks / 100u);
    return (xSemaphoreTake((SemaphoreHandle_t)h->freertos_sem, to) == pdTRUE)
               ? 1 : 0;
}

static portMUX_TYPE s_hwi_mux = portMUX_INITIALIZER_UNLOCKED;

uint32_t Hwi_disable(void)
{
    taskENTER_CRITICAL(&s_hwi_mux);
    return 0u;
}

void Hwi_restore(uint32_t key)
{
    (void)key;
    taskEXIT_CRITICAL(&s_hwi_mux);
}

/*==========================================================================*
 *  Seconds — settable wall clock over the monotonic timer                   *
 *==========================================================================*/

static uint32_t s_epoch_base;          /* wall clock at boot/last set      */
static int64_t  s_mono_base_us;        /* esp_timer time at last set       */

uint32_t Seconds_get(void)
{
    int64_t elapsed = esp_timer_get_time() - s_mono_base_us;
    return s_epoch_base + (uint32_t)(elapsed / 1000000);
}

void Seconds_set(uint32_t t)
{
    s_epoch_base   = t;
    s_mono_base_us = esp_timer_get_time();
}

/*==========================================================================*
 *  osal_snv -> NVS                                                          *
 *==========================================================================*/

static nvs_handle_t s_nvs;
static int          s_nvs_open;

static int snv_ensure(void)
{
    if (s_nvs_open) { return 1; }
    if (nvs_flash_init() == ESP_ERR_NVS_NO_FREE_PAGES) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    if (nvs_open("kepler", NVS_READWRITE, &s_nvs) != ESP_OK) { return 0; }
    s_nvs_open = 1;
    return 1;
}

int osal_snv_read(uint8_t id, uint16_t len, void *buf)
{
    char   key[8];
    size_t sz = len;

    if (!snv_ensure()) { return 1; }
    snprintf(key, sizeof(key), "snv%02x", id);
    return (nvs_get_blob(s_nvs, key, buf, &sz) == ESP_OK && sz == len)
               ? 0 : 1;
}

int osal_snv_write(uint8_t id, uint16_t len, void *buf)
{
    char key[8];

    if (!snv_ensure()) { return 1; }
    snprintf(key, sizeof(key), "snv%02x", id);
    if (nvs_set_blob(s_nvs, key, buf, len) != ESP_OK) { return 1; }
    return (nvs_commit(s_nvs) == ESP_OK) ? 0 : 1;
}

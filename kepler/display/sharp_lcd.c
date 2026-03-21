/******************************************************************************
 *
 * @file  sharp_lcd.c
 *
 * @brief Driver implementation for the Sharp LS013B7DH03 Memory LCD.
 *
 * SPI bit-order note
 * ------------------
 * The CC2640R2F SSI peripheral transmits MSB-first.  The Sharp LCD expects
 * LSB-first on the wire for both address bytes and pixel data bytes.
 * Command bytes (0x80 write, 0x20 clear) are written in the spec already
 * bit-reversed for MSB-first SPI and are transmitted as-is.
 * Address and data bytes are reversed via s_reverse[] before transmission.
 *
 * Pixel polarity: framebuffer 0 = black, 1 = white (LCD native convention).
 * Framebuffer bit order: bit 0 = leftmost pixel in each byte (LSB = left).
 *
 * CS polarity: Sharp LCD CS is ACTIVE HIGH.  CS is toggled manually around
 * every SPI transfer; the SSI hardware CS pin (IOID_11) is left to the
 * board configuration and is harmless since nothing is connected to it in
 * the launchpad test setup.
 *
 * IOID_14 conflict note
 * ---------------------
 * SHARP_LCD_CS_PIN = IOID_14 = Board_BUTTON1 on the launchpad.
 * During Task 1 (display-only), the button driver must not be initialised,
 * or its PIN handle must be closed before calling sharp_lcd_init().
 * This conflict is resolved when the v2 PCB assigns a dedicated pin.
 *
 *****************************************************************************/

#include "sharp_lcd.h"
#include "../kepler_config.h"

#if KEPLER_HAS_SHARP_LCD

#include <string.h>

#include <ti/drivers/SPI.h>
#include <ti/drivers/PIN.h>
#include <ti/sysbios/knl/Clock.h>

/*==========================================================================*
 *  Sharp LCD write protocol constants                                      *
 *==========================================================================*/

/* Command bytes — already bit-reversed for MSB-first SPI transmission.   *
 * When the SSI sends these MSB-first, the LCD receives them LSB-first and *
 * interprets: 0x80 → M0=1 (write), 0x20 → M2=1 (clear).                 */
#define CMD_WRITE   0x80u
#define CMD_CLEAR   0x20u

/* SPI transmit buffer: 1 (cmd) + HEIGHT*(1 addr + STRIDE data) + 2 dummy */
#define TXBUF_SIZE  (2u + ((uint16_t)SHARP_LCD_HEIGHT * (1u + SHARP_LCD_STRIDE)) + 2u)

/*==========================================================================*
 *  Bit-reverse lookup table                                               *
 *                                                                          *
 *  s_reverse[n] returns n with all 8 bits mirrored.                      *
 *  Used to adapt address and data bytes for the MSB-first SSI peripheral. *
 *  256 bytes of flash — negligible on the 128 KB CC2640R2F.              *
 *==========================================================================*/
static const uint8_t s_reverse[256] = {
    /* 0x00 */ 0x00, 0x80, 0x40, 0xC0, 0x20, 0xA0, 0x60, 0xE0,
               0x10, 0x90, 0x50, 0xD0, 0x30, 0xB0, 0x70, 0xF0,
    /* 0x10 */ 0x08, 0x88, 0x48, 0xC8, 0x28, 0xA8, 0x68, 0xE8,
               0x18, 0x98, 0x58, 0xD8, 0x38, 0xB8, 0x78, 0xF8,
    /* 0x20 */ 0x04, 0x84, 0x44, 0xC4, 0x24, 0xA4, 0x64, 0xE4,
               0x14, 0x94, 0x54, 0xD4, 0x34, 0xB4, 0x74, 0xF4,
    /* 0x30 */ 0x0C, 0x8C, 0x4C, 0xCC, 0x2C, 0xAC, 0x6C, 0xEC,
               0x1C, 0x9C, 0x5C, 0xDC, 0x3C, 0xBC, 0x7C, 0xFC,
    /* 0x40 */ 0x02, 0x82, 0x42, 0xC2, 0x22, 0xA2, 0x62, 0xE2,
               0x12, 0x92, 0x52, 0xD2, 0x32, 0xB2, 0x72, 0xF2,
    /* 0x50 */ 0x0A, 0x8A, 0x4A, 0xCA, 0x2A, 0xAA, 0x6A, 0xEA,
               0x1A, 0x9A, 0x5A, 0xDA, 0x3A, 0xBA, 0x7A, 0xFA,
    /* 0x60 */ 0x06, 0x86, 0x46, 0xC6, 0x26, 0xA6, 0x66, 0xE6,
               0x16, 0x96, 0x56, 0xD6, 0x36, 0xB6, 0x76, 0xF6,
    /* 0x70 */ 0x0E, 0x8E, 0x4E, 0xCE, 0x2E, 0xAE, 0x6E, 0xEE,
               0x1E, 0x9E, 0x5E, 0xDE, 0x3E, 0xBE, 0x7E, 0xFE,
    /* 0x80 */ 0x01, 0x81, 0x41, 0xC1, 0x21, 0xA1, 0x61, 0xE1,
               0x11, 0x91, 0x51, 0xD1, 0x31, 0xB1, 0x71, 0xF1,
    /* 0x90 */ 0x09, 0x89, 0x49, 0xC9, 0x29, 0xA9, 0x69, 0xE9,
               0x19, 0x99, 0x59, 0xD9, 0x39, 0xB9, 0x79, 0xF9,
    /* 0xA0 */ 0x05, 0x85, 0x45, 0xC5, 0x25, 0xA5, 0x65, 0xE5,
               0x15, 0x95, 0x55, 0xD5, 0x35, 0xB5, 0x75, 0xF5,
    /* 0xB0 */ 0x0D, 0x8D, 0x4D, 0xCD, 0x2D, 0xAD, 0x6D, 0xED,
               0x1D, 0x9D, 0x5D, 0xDD, 0x3D, 0xBD, 0x7D, 0xFD,
    /* 0xC0 */ 0x03, 0x83, 0x43, 0xC3, 0x23, 0xA3, 0x63, 0xE3,
               0x13, 0x93, 0x53, 0xD3, 0x33, 0xB3, 0x73, 0xF3,
    /* 0xD0 */ 0x0B, 0x8B, 0x4B, 0xCB, 0x2B, 0xAB, 0x6B, 0xEB,
               0x1B, 0x9B, 0x5B, 0xDB, 0x3B, 0xBB, 0x7B, 0xFB,
    /* 0xE0 */ 0x07, 0x87, 0x47, 0xC7, 0x27, 0xA7, 0x67, 0xE7,
               0x17, 0x97, 0x57, 0xD7, 0x37, 0xB7, 0x77, 0xF7,
    /* 0xF0 */ 0x0F, 0x8F, 0x4F, 0xCF, 0x2F, 0xAF, 0x6F, 0xEF,
               0x1F, 0x9F, 0x5F, 0xDF, 0x3F, 0xBF, 0x7F, 0xFF,
};

/*==========================================================================*
 *  Module state                                                            *
 *==========================================================================*/

static SPI_Handle    s_spi;
static PIN_Handle    s_pins;
static PIN_State     s_pin_state;
static Clock_Struct  s_vcom_clock;

static volatile bool s_vcom_state;

/* Dirty-line bitmask: 6 × 32 = 192 bits, covers all 168 rows.            */
static uint32_t      s_dirty[6];

/* Static SPI transmit buffer — avoids large stack allocation on flush.    */
static uint8_t       s_txbuf[TXBUF_SIZE];

/*--- GPIO pin configuration table ----------------------------------------*/
static PIN_Config s_pin_table[] = {
    SHARP_LCD_CS_PIN   | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL,
    SHARP_LCD_DISP_PIN | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL,
    SHARP_LCD_VCOM_PIN | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL,
    PIN_TERMINATE
};

/*==========================================================================*
 *  Internal helpers                                                        *
 *==========================================================================*/

/* VCOM 1 Hz timer callback — safe to call from Swi/Hwi context.          */
static void vcom_clock_cb(UArg arg)
{
    (void)arg;
    sharp_lcd_vcom_toggle();
}

/* Perform one SPI write transaction with manual active-high CS.           */
static void spi_write(const uint8_t *buf, uint16_t len)
{
    SPI_Transaction txn;
    txn.count = len;
    txn.txBuf = (void *)buf;
    txn.rxBuf = NULL;

    PIN_setOutputValue(s_pins, SHARP_LCD_CS_PIN, 1);  /* CS HIGH            */
    SPI_transfer(s_spi, &txn);
    PIN_setOutputValue(s_pins, SHARP_LCD_CS_PIN, 0);  /* CS LOW             */
}

/*==========================================================================*
 *  Lifecycle                                                               *
 *==========================================================================*/

bool sharp_lcd_init(void)
{
    /* Open GPIO pins (CS, DISP, VCOM).                                    *
     * IOID_14 conflict: see file header.  Ensure the button driver is not *
     * holding a PIN handle for IOID_14 before calling this function.      */
    s_pins = PIN_open(&s_pin_state, s_pin_table);
    if (!s_pins) {
        return false;
    }

    /* Open SPI instance (blocking mode, SPI mode 0, 1 MHz).              */
    SPI_init();
    SPI_Params spi_params;
    SPI_Params_init(&spi_params);
    spi_params.transferMode = SPI_MODE_BLOCKING;
    spi_params.bitRate      = KEPLER_SPI_BITRATE;
    spi_params.dataSize     = 8;
    spi_params.frameFormat  = SPI_POL0_PHA0;

    s_spi = SPI_open(KEPLER_SPI_INSTANCE, &spi_params);
    if (!s_spi) {
        PIN_close(s_pins);
        s_pins = NULL;
        return false;
    }

    /* Bring display on.                                                   */
    PIN_setOutputValue(s_pins, SHARP_LCD_DISP_PIN, 1);

    /* Hardware clear — sets all pixels to white.                          */
    sharp_lcd_clear();

    /* Start VCOM 1 Hz periodic clock.  Must run continuously while        *
     * display is powered; stopping it risks DC bias damage.               */
    uint32_t one_sec = 1000u * (1000u / Clock_tickPeriod);
    Clock_Params clk_params;
    Clock_Params_init(&clk_params);
    clk_params.period    = one_sec;
    clk_params.startFlag = TRUE;
    Clock_construct(&s_vcom_clock, vcom_clock_cb, one_sec, &clk_params);

    memset(s_dirty, 0, sizeof(s_dirty));

    return true;
}

/*==========================================================================*
 *  Framebuffer -> display transfer                                         *
 *==========================================================================*/

void sharp_lcd_flush(const sharp_fb_t fb)
{
    sharp_lcd_flush_lines(fb, 1, SHARP_LCD_HEIGHT);
}

void sharp_lcd_flush_lines(const sharp_fb_t fb,
                           uint8_t first_line, uint8_t last_line)
{
    uint16_t idx = 0;

    s_txbuf[idx++] = CMD_WRITE;

    for (uint8_t line = first_line; line <= last_line; line++) {
        /* Address byte is 1-indexed; bit-reverse it for MSB-first SPI.   */
        s_txbuf[idx++] = s_reverse[line];

        /* Pixel data: bit-reverse each byte so the MSB-first SSI delivers *
         * the data to the LCD with LSB-first bit order on the wire.       */
        const uint8_t *row = fb[line - 1];   /* fb is 0-indexed           */
        for (uint8_t b = 0; b < SHARP_LCD_STRIDE; b++) {
            s_txbuf[idx++] = s_reverse[row[b]];
        }
    }

    s_txbuf[idx++] = 0x00;   /* trailing dummy bytes                      */
    s_txbuf[idx++] = 0x00;

    spi_write(s_txbuf, idx);
}

void sharp_lcd_flush_dirty(const sharp_fb_t fb)
{
    uint8_t first = SHARP_LCD_HEIGHT;
    uint8_t last  = 0;

    for (uint8_t i = 0; i < SHARP_LCD_HEIGHT; i++) {
        if (s_dirty[i / 32] & (1u << (i % 32))) {
            if (i < first) first = i;
            if (i > last)  last  = i;
        }
    }

    if (first <= last) {
        /* flush_lines expects 1-indexed; dirty mask is 0-indexed.         */
        sharp_lcd_flush_lines(fb, first + 1u, last + 1u);
    }

    memset(s_dirty, 0, sizeof(s_dirty));
}

/*==========================================================================*
 *  Display control                                                         *
 *==========================================================================*/

void sharp_lcd_clear(void)
{
    uint8_t cmd[2] = { CMD_CLEAR, 0x00 };
    spi_write(cmd, 2);
}

void sharp_lcd_set_display(bool on)
{
    PIN_setOutputValue(s_pins, SHARP_LCD_DISP_PIN, on ? 1 : 0);
}

/*==========================================================================*
 *  VCOM                                                                    *
 *==========================================================================*/

void sharp_lcd_vcom_toggle(void)
{
    s_vcom_state = !s_vcom_state;
    PIN_setOutputValue(s_pins, SHARP_LCD_VCOM_PIN, s_vcom_state ? 1 : 0);
}

/*==========================================================================*
 *  Dirty-line tracking                                                     *
 *==========================================================================*/

void sharp_lcd_mark_dirty(uint8_t row)
{
    if (row < SHARP_LCD_HEIGHT) {
        s_dirty[row / 32] |= (1u << (row % 32));
    }
}

/*==========================================================================*
 *  Framebuffer drawing primitives                                          *
 *==========================================================================*/

void fb_set_pixel(sharp_fb_t fb, uint8_t x, uint8_t y, bool black)
{
    if (x >= SHARP_LCD_WIDTH || y >= SHARP_LCD_HEIGHT) return;

    /* Sharp LCD: bit 0 (LSB) = leftmost pixel in each byte.              *
     *   0 = black pixel,  1 = white pixel.                               */
    if (black) {
        fb[y][x / 8] &= ~(1u << (x % 8));
    } else {
        fb[y][x / 8] |=  (1u << (x % 8));
    }

    sharp_lcd_mark_dirty(y);
}

void fb_draw_glyph(sharp_fb_t fb,
                   const uint8_t *glyph_data,
                   uint8_t glyph_w, uint8_t glyph_h,
                   uint8_t px, uint8_t py,
                   bool invert)
{
    for (uint8_t row = 0; row < glyph_h; row++) {
        for (uint8_t col = 0; col < glyph_w; col++) {
            /* Glyph bitmaps are stored MSB-first (standard bitmap conv.)  */
            uint8_t byte_idx = (row * ((glyph_w + 7u) / 8u)) + (col / 8u);
            uint8_t bit_mask = 0x80u >> (col % 8u);
            bool pixel_on = (glyph_data[byte_idx] & bit_mask) != 0;
            if (invert) pixel_on = !pixel_on;
            fb_set_pixel(fb, px + col, py + row, pixel_on);
        }
    }
}

void fb_fill_rect(sharp_fb_t fb,
                  uint8_t x, uint8_t y,
                  uint8_t w, uint8_t h,
                  bool fill)
{
    uint16_t x_end = (uint16_t)x + w;
    uint16_t y_end = (uint16_t)y + h;
    if (x_end > SHARP_LCD_WIDTH)  x_end = SHARP_LCD_WIDTH;
    if (y_end > SHARP_LCD_HEIGHT) y_end = SHARP_LCD_HEIGHT;

    for (uint16_t row = y; row < y_end; row++) {
        for (uint16_t col = x; col < x_end; col++) {
            if (fill) {
                fb[row][col / 8] &= ~(1u << (col % 8));  /* black         */
            } else {
                fb[row][col / 8] |=  (1u << (col % 8));  /* white         */
            }
        }
        sharp_lcd_mark_dirty((uint8_t)row);
    }
}

void fb_hline(sharp_fb_t fb,
              uint8_t x, uint8_t y, uint8_t w,
              bool black)
{
    if (y >= SHARP_LCD_HEIGHT) return;

    uint16_t x_end = (uint16_t)x + w;
    if (x_end > SHARP_LCD_WIDTH) x_end = SHARP_LCD_WIDTH;

    for (uint16_t col = x; col < x_end; col++) {
        if (black) {
            fb[y][col / 8] &= ~(1u << (col % 8));
        } else {
            fb[y][col / 8] |=  (1u << (col % 8));
        }
    }

    sharp_lcd_mark_dirty(y);
}

void fb_clear(sharp_fb_t fb)
{
    /* 0xFF = all bits 1 = all pixels white.                               */
    memset(fb, 0xFF, sizeof(sharp_fb_t));

    /* All rows now differ from what the display holds — mark all dirty.   */
    memset(s_dirty, 0xFF, sizeof(s_dirty));
}

#endif /* KEPLER_HAS_SHARP_LCD */

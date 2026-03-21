/******************************************************************************
 *
 * @file  sharp_lcd.h
 *
 * @brief Driver for the Sharp LS013B7DH03 Memory LCD (144x168, 1-bpp).
 *
 *        The display uses SPI with an ACTIVE-HIGH chip select, a DISP
 *        on/off pin, and a VCOM toggle pin driven by a 1 Hz timer.
 *        All pin assignments come from kepler_config.h (Tier 2).
 *
 *        Pixel polarity: 0 = black, 1 = white.
 *        Bit order per byte: LSB = leftmost pixel.
 *        Framebuffer must be initialised to 0xFF (all white).
 *
 *****************************************************************************/

#ifndef SHARP_LCD_H
#define SHARP_LCD_H

#include <stdint.h>
#include <stdbool.h>

/*--- Display geometry ----------------------------------------------------*/
#define SHARP_LCD_WIDTH   144
#define SHARP_LCD_HEIGHT  168
#define SHARP_LCD_STRIDE  18    /* bytes per row  (144 / 8) */

/*--- Framebuffer type ----------------------------------------------------*/
/* 1 bit per pixel, row-major, LSB = leftmost pixel.  Total 3 024 bytes.   */
typedef uint8_t sharp_fb_t[SHARP_LCD_HEIGHT][SHARP_LCD_STRIDE];

/*--- Lifecycle -----------------------------------------------------------*/

/* Initialise SPI, GPIO pins, VCOM 1 Hz timer.
 * Sends the hardware clear command; leaves display ON.
 * Returns true on success. */
bool sharp_lcd_init(void);

/*--- Framebuffer -> display transfer -------------------------------------*/

/* Write the full framebuffer (168 lines) to the display.                  */
void sharp_lcd_flush(const sharp_fb_t fb);

/* Write lines [first_line .. last_line] inclusive.
 * Line numbers are 1-indexed to match the hardware protocol.              */
void sharp_lcd_flush_lines(const sharp_fb_t fb,
                           uint8_t first_line, uint8_t last_line);

/* Flush only rows marked dirty since the last flush, then clear the mask. */
void sharp_lcd_flush_dirty(const sharp_fb_t fb);

/* Send the hardware "clear all" command (all pixels -> white).            */
void sharp_lcd_clear(void);

/*--- Display power -------------------------------------------------------*/

/* Set DISP pin HIGH (on) or LOW (off).
 * Does not affect framebuffer contents or VCOM toggling.                  */
void sharp_lcd_set_display(bool on);

/*--- Dirty-line tracking -------------------------------------------------*/

/* Mark a single row (0-indexed) as needing retransmission.                */
void sharp_lcd_mark_dirty(uint8_t row);

/*--- VCOM (internal -- called by timer callback, not by application) -----*/
void sharp_lcd_vcom_toggle(void);

/*--- Framebuffer drawing primitives --------------------------------------*/

/* Set a single pixel.  black=true -> 0 bit; black=false -> 1 bit.
 * Automatically marks the row dirty.                                      */
void fb_set_pixel(sharp_fb_t fb, uint8_t x, uint8_t y, bool black);

/* Blit a 1-bpp glyph (MSB-first in glyph data) into the framebuffer.
 * Marks affected rows dirty.  If invert is true, pixel sense is flipped.  */
void fb_draw_glyph(sharp_fb_t fb,
                   const uint8_t *glyph_data,
                   uint8_t glyph_w, uint8_t glyph_h,
                   uint8_t px, uint8_t py,
                   bool invert);

/* Fill a rectangle with black (fill=true) or white (fill=false).
 * Marks affected rows dirty.                                              */
void fb_fill_rect(sharp_fb_t fb,
                  uint8_t x, uint8_t y,
                  uint8_t w, uint8_t h,
                  bool fill);

/* Draw a 1-pixel-wide horizontal line at row y from x to x+w-1.          */
void fb_hline(sharp_fb_t fb,
              uint8_t x, uint8_t y, uint8_t w,
              bool black);

/* Clear the entire framebuffer to white (0xFF).  Marks all rows dirty.    */
void fb_clear(sharp_fb_t fb);

#endif /* SHARP_LCD_H */

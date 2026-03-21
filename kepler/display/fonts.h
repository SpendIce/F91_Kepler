/******************************************************************************
 *
 * @file  fonts.h
 *
 * @brief Font tables and text-rendering helpers for the F91 Kepler UI.
 *
 *        Two font sizes are provided:
 *
 *        LARGE  —  24×40 px, digits 0-9 plus a colon glyph.
 *                  Used for the main-screen HH:MM clock.
 *                  Stored row-major, 3 bytes per row, MSB = leftmost pixel.
 *
 *        SMALL  —  6×8 px, ASCII 0x20-0x7E (96 printable characters).
 *                  Stored row-major, 1 byte per row, MSB = leftmost pixel
 *                  (bit 7 is column 0, bit 2 is column 5; bits 1-0 unused).
 *                  Source: font_5x7.h column data transposed to row format
 *                  and widened to 6 columns.
 *
 *        SMALL_EXT  —  same 6×8 format, 16 Spanish / punctuation glyphs.
 *                  Indexed by font_ext_id_t.  Accessed automatically by
 *                  font_draw_string() when it encounters valid UTF-8
 *                  two-byte sequences for the supported code-points.
 *
 *****************************************************************************/

#ifndef FONTS_H
#define FONTS_H

#include "sharp_lcd.h"
#include <stdint.h>
#include <stdbool.h>

/*==========================================================================*
 *  Large font  (digits + colon)                                            *
 *==========================================================================*/

#define FONT_LARGE_W           24u   /* pixels wide                         */
#define FONT_LARGE_H           40u   /* pixels tall                         */
#define FONT_LARGE_STRIDE       3u   /* bytes per row  (24 / 8)             */
#define FONT_LARGE_GLYPH_BYTES 120u  /* FONT_LARGE_H * FONT_LARGE_STRIDE    */

#define FONT_LARGE_COLON_W     12u   /* colon glyph is narrower             */
/* colon storage: FONT_LARGE_H rows × 2 bytes each = 80 bytes               */

/*==========================================================================*
 *  Small font  (6×8 ASCII)                                                 *
 *==========================================================================*/

#define FONT_SMALL_W            6u
#define FONT_SMALL_H            8u
#define FONT_SMALL_GLYPH_BYTES  8u   /* one byte per row                    */
#define FONT_SMALL_FIRST_CHAR   0x20 /* space                               */
#define FONT_SMALL_LAST_CHAR    0x7E /* tilde                               */
#define FONT_SMALL_COUNT       96u

/*==========================================================================*
 *  Small extended font  (Spanish + ¡ ¿)                                    *
 *==========================================================================*/

typedef enum {
    FONT_EXT_IEXCL    =  0,  /* ¡  U+00A1                                  */
    FONT_EXT_IQUEST   =  1,  /* ¿  U+00BF                                  */
    FONT_EXT_A_UC     =  2,  /* Á  U+00C1                                  */
    FONT_EXT_E_UC     =  3,  /* É  U+00C9                                  */
    FONT_EXT_I_UC     =  4,  /* Í  U+00CD                                  */
    FONT_EXT_N_UC     =  5,  /* Ñ  U+00D1                                  */
    FONT_EXT_O_UC     =  6,  /* Ó  U+00D3                                  */
    FONT_EXT_U_UC     =  7,  /* Ú  U+00DA                                  */
    FONT_EXT_U_UML_UC =  8,  /* Ü  U+00DC                                  */
    FONT_EXT_A_LC     =  9,  /* á  U+00E1                                  */
    FONT_EXT_E_LC     = 10,  /* é  U+00E9                                  */
    FONT_EXT_I_LC     = 11,  /* í  U+00ED                                  */
    FONT_EXT_N_LC     = 12,  /* ñ  U+00F1                                  */
    FONT_EXT_O_LC     = 13,  /* ó  U+00F3                                  */
    FONT_EXT_U_LC     = 14,  /* ú  U+00FA                                  */
    FONT_EXT_U_UML_LC = 15,  /* ü  U+00FC                                  */
    FONT_EXT_COUNT    = 16,
} font_ext_id_t;

/*==========================================================================*
 *  Glyph tables                                                             *
 *==========================================================================*/

extern const uint8_t FONT_LARGE_DIGITS[10][FONT_LARGE_GLYPH_BYTES];
extern const uint8_t FONT_LARGE_COLON[FONT_LARGE_H * 2u];
extern const uint8_t FONT_SMALL[FONT_SMALL_COUNT][FONT_SMALL_GLYPH_BYTES];
extern const uint8_t FONT_SMALL_EXT[FONT_EXT_COUNT][FONT_SMALL_GLYPH_BYTES];

/*==========================================================================*
 *  Rendering helpers                                                        *
 *==========================================================================*/

/* Draw one large digit (0-9) with its top-left corner at (x, y).          */
void font_draw_digit_large(sharp_fb_t fb, uint8_t x, uint8_t y, uint8_t digit);

/* Draw the large colon glyph (FONT_LARGE_COLON_W × FONT_LARGE_H).        */
void font_draw_colon_large(sharp_fb_t fb, uint8_t x, uint8_t y);

/* Draw one small ASCII character.  Non-printable → space.                 */
/* If invert is true the pixel sense is flipped (white-on-black).          */
void font_draw_char_small(sharp_fb_t fb, uint8_t x, uint8_t y,
                          char c, bool invert);

/* Draw a null-terminated string using the small font.
 *
 * Handles UTF-8:  two-byte sequences whose second byte's codepoint maps
 * to a supported Spanish/punctuation glyph are rendered from FONT_SMALL_EXT.
 * All other multi-byte sequences are rendered as '?'.
 *
 * Stops before writing beyond SHARP_LCD_WIDTH.                            */
void font_draw_string(sharp_fb_t fb, uint8_t x, uint8_t y,
                      const char *str, bool invert);

/* Return the pixel width that font_draw_string() would occupy.
 * UTF-8 aware (each code-point counts as FONT_SMALL_W pixels).           */
uint8_t font_string_width(const char *str);

#endif /* FONTS_H */

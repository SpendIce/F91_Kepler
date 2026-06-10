/******************************************************************************
 *
 * @file  weather_icons.h
 *
 * @brief 1-bit weather condition icons in three sizes (Task 6).
 *
 *        Bitmap format matches fb_draw_glyph(): row-major, MSB-first,
 *        ceil(w/8) bytes per row, set bit = black pixel.
 *
 *        Row strides are byte-aligned, so the small 12x12 icons occupy
 *        24 bytes (2 bytes/row), not the bit-packed 18 the spec estimates.
 *
 *        Indexed by weather_condition_t (0-7).  For WEATHER_UNKNOWN use
 *        the renderer's dashed-box stub instead.
 *
 *****************************************************************************/

#ifndef WEATHER_ICONS_H
#define WEATHER_ICONS_H

#include <stdint.h>

#define WEATHER_ICON_SIZE_LG  32   /* WEATHER screen current condition     */
#define WEATHER_ICON_SIZE_MD  16   /* MAIN screen summary                  */
#define WEATHER_ICON_SIZE_SM  12   /* hourly forecast slots                */

#define WEATHER_ICON_BYTES_LG  (32 * 4)   /* 128 */
#define WEATHER_ICON_BYTES_MD  (16 * 2)   /*  32 */
#define WEATHER_ICON_BYTES_SM  (12 * 2)   /*  24 */

/* weather_icon_lg[WEATHER_CLEAR] -> 128-byte bitmap, etc.                 */
extern const uint8_t *const weather_icon_lg[8];
extern const uint8_t *const weather_icon_md[8];
extern const uint8_t *const weather_icon_sm[8];

#endif /* WEATHER_ICONS_H */

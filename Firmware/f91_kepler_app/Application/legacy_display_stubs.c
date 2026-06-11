/******************************************************************************
 *
 * @file  legacy_display_stubs.c
 *
 * @brief No-op replacements for the SSD1306 OLED driver and the legacy
 *        button driver (Phase 0 migration step).
 *
 *        ssd1306.c and f91_buttons.c are excluded from the build:
 *          - the SSD1306 is replaced by the Sharp Memory LCD
 *            (kepler/display/), freeing its display buffers (~540 B RAM)
 *            and ~4 KB flash;
 *          - the legacy button driver collides with kepler/input/buttons.c
 *            on the same PIN entries — only one PIN client can own a pin.
 *
 *        The legacy task code (f91_kepler.c / f91_clock.c /
 *        f91_notification.c) still calls these APIs while its BLE service
 *        handling is migrated to the kepler services; the stubs keep it
 *        linking and harmless until that code is deleted outright.
 *
 *****************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "ssd1306.h"
#include "f91_buttons.h"

/*--- ssd1306.c replacements -------------------------------------------------*/

void ssd1306_init(void)                                      {}
void ssd1306_update(void)                                    {}
void ssd1306_clear(void)                                     {}
void ssd1306_display_text(char *text, uint8_t x, uint8_t y, bool erase)
                       { (void)text; (void)x; (void)y; (void)erase; }
void ssd1306_display_number(uint8_t number, uint8_t x, uint8_t y, bool erase)
                       { (void)number; (void)x; (void)y; (void)erase; }
void ssd1306_display_semicolon(uint8_t x, uint8_t y, bool erase)
                       { (void)x; (void)y; (void)erase; }
void ssd1306_display_pm(uint8_t x, uint8_t y, bool erase)
                       { (void)x; (void)y; (void)erase; }
void ssd1306_display_small_number(uint8_t number, uint8_t x, uint8_t y,
                                  bool erase)
                       { (void)number; (void)x; (void)y; (void)erase; }
void ssd1306_display_notification(uint8_t icon, uint8_t x, uint8_t y,
                                  bool erase)
                       { (void)icon; (void)x; (void)y; (void)erase; }
void ssd1306_display_full_notification(uint8_t type, char *text)
                       { (void)type; (void)text; }
void ssd1306_display_ellipsis(uint8_t x, uint8_t y, bool erase)
                       { (void)x; (void)y; (void)erase; }
void ssd1306_toggle_display(bool state)                      { (void)state; }
bool ssd1306_isReady(void)                                   { return true; }
bool ssd1306_getState(void)                                  { return false; }

/*--- f91_buttons.c replacements ----------------------------------------------*
 *  Buttons are owned by kepler/input/buttons.c from this point on.        *
 *--------------------------------------------------------------------------*/

void F91Buttons_init(void)                                   {}
void F91Buttons_processButtonPress(button_state_t *buttonInfo)
                                                  { (void)buttonInfo; }
void F91Buttons_resetOneShot(void)                           {}

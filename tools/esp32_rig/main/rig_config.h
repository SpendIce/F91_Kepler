/******************************************************************************
 *
 * @file  rig_config.h
 *
 * @brief ESP32 peripheral test rig — pin mapping and bus selection.
 *
 *        The kepler firmware addresses pins as CC26xx IOID numbers (from
 *        kepler_config.h).  The shim layer translates IOID -> ESP32 GPIO
 *        through the table in this file.  Rewire here, nowhere else.
 *
 *        Default wiring (classic ESP32-WROOM devkit, all 3.3 V):
 *
 *          Sharp LS013B7DH03        ESP32
 *            SCLK    ------------   GPIO18  (VSPI CLK)
 *            SI      ------------   GPIO23  (VSPI MOSI)
 *            SCS     ------------   GPIO5   (plain GPIO — CS is ACTIVE
 *                                            HIGH, driven manually)
 *            DISP    ------------   GPIO17
 *            EXTCOMIN ('VCOM') ---   GPIO16
 *            VDD/VDDA ----------    3V3,  GND -> GND
 *            (EXTMODE strapped HIGH on most breakouts = EXTCOMIN drives
 *             VCOM, which matches this driver)
 *
 *          I2C bus (DRV2605L + LIS2DW12 breakouts, shared)
 *            SDA     ------------   GPIO21
 *            SCL     ------------   GPIO22
 *            (most breakouts carry pull-ups; if not, add 4.7 k to 3V3)
 *
 *          LIS2DW12 INT1 --------   GPIO34  (input-only pin is fine)
 *          DRV2605L OUTP/OUTN ---   ERM coin motor leads
 *
 *****************************************************************************/

#ifndef RIG_CONFIG_H
#define RIG_CONFIG_H

#include <stdint.h>

/*--- ESP32 GPIO assignments ------------------------------------------------*/
#define RIG_GPIO_LCD_SCLK   18
#define RIG_GPIO_LCD_MOSI   23
#define RIG_GPIO_LCD_CS      5
#define RIG_GPIO_LCD_DISP   17
#define RIG_GPIO_LCD_VCOM   16
#define RIG_GPIO_I2C_SDA    21
#define RIG_GPIO_I2C_SCL    22
#define RIG_GPIO_ACCEL_INT1 34

/*--- IOID -> ESP32 GPIO translation ----------------------------------------*
 *  IOIDs come from kepler_config.h (launchpad tier):                       *
 *    SPI CLK 10, MOSI 9, LCD CS 14, DISP 15, VCOM 16,                      *
 *    I2C SDA 5, SCL 6, LIS2DW12 INT1 = IOID_UNUSED (rig maps it anyway     *
 *    through RIG_GPIO_ACCEL_INT1 in the rig's own interrupt hookup).       *
 *  Unmapped IOIDs return -1 (shim ignores them).                           */
static inline int rig_ioid_to_gpio(uint32_t ioid)
{
    switch (ioid) {
        case 10: return RIG_GPIO_LCD_SCLK;
        case 9:  return RIG_GPIO_LCD_MOSI;
        case 14: return RIG_GPIO_LCD_CS;
        case 15: return RIG_GPIO_LCD_DISP;
        case 16: return RIG_GPIO_LCD_VCOM;
        case 5:  return RIG_GPIO_I2C_SDA;
        case 6:  return RIG_GPIO_I2C_SCL;
        default: return -1;
    }
}

#endif /* RIG_CONFIG_H */

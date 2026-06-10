/******************************************************************************
 *
 * @file  flash_store.h
 *
 * @brief Non-volatile storage for the F91 Kepler firmware.
 *
 *        Backed by the BLE stack's SNV (osal_snv) key-value store, which is
 *        what the original Kepler firmware links against and what the
 *        CC2640R2 SDK wear-levels across two flash pages.  The logical
 *        NV item IDs below are mapped onto the customer SNV ID range
 *        (BLE_NVID_CUST_START = 0x80) inside flash_store.c.
 *
 *        All reads return defaults (and false) when an item has never been
 *        written — callers never see garbage.
 *
 *****************************************************************************/

#ifndef FLASH_STORE_H
#define FLASH_STORE_H

#include <stdint.h>
#include <stdbool.h>
#include "../kepler_types.h"

/*--- Logical NV item IDs (spec 05, Task 7) --------------------------------*/
#define NV_ID_STEP_HISTORY      0x0001  /* uint16_t[7] — last 7 days steps  */
#define NV_ID_SLEEP_LAST_NIGHT  0x0002  /* actigraphy_night_t               */
#define NV_ID_SETTINGS          0x0003  /* kepler_settings_t                */
#define NV_ID_HAPTIC_CAL        0x0004  /* DRV2605L COMP + BEMF bytes       */
#define NV_ID_HAPTIC_CALIBRATED 0x0005  /* uint8_t flag: 1 if calibrated    */
#define NV_ID_STEP_TODAY        0x0006  /* uint32_t current-day steps       */
#define NV_ID_WEATHER           0x0007  /* weather_payload_t (36 bytes)     */
#define NV_ID_ALARMS            0x0008  /* alarms_payload_t (66 bytes)      */
#define NV_ID_TEMP_UNIT         0x0009  /* uint8_t: 0=Celsius, 1=Fahrenheit */

/*==========================================================================*
 *  Public API                                                               *
 *==========================================================================*/

/* Initialise the store.  Must be called after the BLE stack (osal_snv     *
 * is owned by it).  Returns true on success.                              */
bool flash_store_init(void);

/*--- Step history (7 days; index 0 = today, 6 = six days ago) ------------*/

/* Close out the current day: shift history right and store `steps` at     *
 * index 0's previous slot (i.e. yesterday after the midnight rollover).   */
bool flash_store_step_day(uint32_t steps);
bool flash_store_read_steps(uint16_t out[7]);

/*--- Current-day step count (crash recovery) ------------------------------*/
bool     flash_store_write_step_today(uint32_t steps);
uint32_t flash_store_read_step_today(void);   /* 0 if never written        */

/*--- Sleep actigraphy ------------------------------------------------------*/
bool flash_store_write_sleep(const actigraphy_night_t *night);
bool flash_store_read_sleep(actigraphy_night_t *out);

/*--- Settings ---------------------------------------------------------------*/
bool flash_store_write_settings(const kepler_settings_t *s);
/* Fills *out with stored settings, or spec defaults if never written.     *
 * Returns true if stored settings were found.                             */
bool flash_store_read_settings(kepler_settings_t *out);

/*--- Haptic calibration ------------------------------------------------------*/
bool flash_store_write_haptic_cal(uint8_t comp, uint8_t bemf);
bool flash_store_read_haptic_cal(uint8_t *comp, uint8_t *bemf);
bool flash_store_haptic_is_calibrated(void);

/*--- Weather (last known — shown on boot before phone connects) ----------*/
bool flash_store_write_weather(const weather_payload_t *w);
bool flash_store_read_weather(weather_payload_t *out);

/*--- Alarms -------------------------------------------------------------------*/
bool flash_store_write_alarms(const alarms_payload_t *a);
bool flash_store_read_alarms(alarms_payload_t *out);

/*--- Temperature unit preference ----------------------------------------------*/
bool    flash_store_write_temp_unit(uint8_t unit);  /* 0=C, 1=F            */
uint8_t flash_store_read_temp_unit(void);           /* 0 if not set        */

#endif /* FLASH_STORE_H */

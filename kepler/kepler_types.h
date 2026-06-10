/******************************************************************************
 *
 * @file  kepler_types.h
 *
 * @brief Shared data payload types for the F91 Kepler firmware.
 *
 *        These structs are the BLE wire formats and NV flash record formats
 *        shared between the ble/, storage/, accel/ and screens/ modules.
 *        Keeping them in one header avoids circular includes between
 *        flash_store.h (Task 5) and the GATT service headers (Task 6).
 *
 *        Wire sizes (packed, little-endian on both CC2640R2F and Android):
 *          notif_payload_t       64 bytes   (0xFF01)
 *          weather_payload_t     36 bytes   (0xFF06)
 *          alarms_payload_t      66 bytes   (0xFF08)
 *          actigraphy_night_t    20 bytes   (0xFF04 — spec quotes 19;
 *                                            epoch_count padding makes 20)
 *          kepler_settings_t     16 bytes   (0xFF05)
 *
 *****************************************************************************/

#ifndef KEPLER_TYPES_H
#define KEPLER_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/*==========================================================================*
 *  Notification relay payload (0xFF01, app -> watch, 64 bytes)            *
 *==========================================================================*/

typedef struct {
    uint8_t type;          /* 0=message, 1=call, 2=calendar, 3=other        */
    uint8_t app_id;        /* app enum assigned by companion app            */
    char    sender[21];    /* null-terminated, max 20 chars                 */
    char    text[41];      /* null-terminated, max 40 chars                 */
} notif_payload_t;         /* 64 bytes                                      */

/*==========================================================================*
 *  Weather (0xFF06, app -> watch, 36 bytes)                               *
 *==========================================================================*/

typedef enum {
    WEATHER_CLEAR        = 0,
    WEATHER_PARTLY_CLOUD = 1,
    WEATHER_CLOUDY       = 2,
    WEATHER_RAIN         = 3,
    WEATHER_STORM        = 4,
    WEATHER_SNOW         = 5,
    WEATHER_FOG          = 6,
    WEATHER_WINDY        = 7,
    WEATHER_COND_COUNT   = 8,
    WEATHER_UNKNOWN      = 0xFF,
} weather_condition_t;

typedef struct {
    int8_t   temp_c;        /* current temp degC (signed)                   */
    int8_t   feels_like_c;  /* feels like degC                              */
    int8_t   temp_high_c;   /* day high degC                                */
    int8_t   temp_low_c;    /* day low degC                                 */
    uint8_t  condition;     /* weather_condition_t                          */
    uint8_t  humidity_pct;  /* relative humidity 0-100                      */
    uint8_t  wind_kmh;      /* wind speed km/h (0-255)                      */
    uint8_t  uv_index;      /* 0-11                                         */
    uint32_t updated_at;    /* Unix timestamp of weather data               */
} weather_current_t;        /* 12 bytes                                     */

typedef struct {
    uint8_t hour;           /* hour of forecast (0-23)                      */
    int8_t  temp_c;         /* forecast temp degC                           */
    uint8_t condition;      /* weather_condition_t                          */
} weather_hourly_t;         /* 3 bytes per slot                             */

typedef struct {
    weather_current_t current;
    weather_hourly_t  hourly[8];   /* next 8 hours (24 bytes)               */
} weather_payload_t;        /* 36 bytes total                               */

/*==========================================================================*
 *  Alarms (0xFF08, app -> watch, 66 bytes)                                *
 *==========================================================================*/

typedef struct {
    uint8_t hour;
    uint8_t minute;
    uint8_t enabled;        /* 0 = off, 1 = on                              */
    uint8_t days_mask;      /* bit0=Mon .. bit6=Sun, 0xFF = every day       */
    char    label[9];       /* null-terminated short label (max 8 chars)    */
} alarm_entry_t;            /* 13 bytes                                     */

#define KEPLER_ALARM_MAX  5

typedef struct {
    uint8_t       count;                    /* valid entries (0-5)          */
    alarm_entry_t alarms[KEPLER_ALARM_MAX];
} alarms_payload_t;         /* 1 + 65 = 66 bytes                            */

/*==========================================================================*
 *  Sleep actigraphy night record (NV + 0xFF04, 20 bytes)                  *
 *==========================================================================*/

#define ACTIGRAPHY_EPOCH_MAX  120   /* 10 h at 5-min epochs                 */

typedef struct {
    uint32_t date;          /* Unix timestamp of sleep-window start         */
    uint8_t  epochs[15];    /* 120 bits, 1 bit per 5-min epoch, LSB first   */
    uint8_t  epoch_count;   /* epochs recorded this night                   */
} actigraphy_night_t;       /* 20 bytes                                     */

/*==========================================================================*
 *  Settings (0xFF05 + NV, 16 bytes)                                       *
 *==========================================================================*/

typedef struct {
    uint16_t step_goal;             /* default 8000                         */
    uint8_t  sleep_start_hour;      /* default 22                           */
    uint8_t  sleep_end_hour;        /* default 8                            */
    uint8_t  haptic_call_en;        /* default 1                            */
    uint8_t  haptic_message_en;     /* default 1                            */
    uint8_t  haptic_alarm_en;       /* default 1                            */
    uint8_t  display_mode_default;  /* 0=ambient, unused for now            */
    uint8_t  reserved[8];           /* pad to 16 bytes for future use       */
} kepler_settings_t;        /* 16 bytes                                     */

#endif /* KEPLER_TYPES_H */

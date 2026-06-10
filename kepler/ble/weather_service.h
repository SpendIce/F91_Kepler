/******************************************************************************
 *
 * @file  weather_service.h
 *
 * @brief Weather GATT characteristic handling (0xFF06 / 0xFF0A, Task 6).
 *
 *        Owns two representations:
 *          - weather_payload_t  g_weather : the 36-byte BLE wire format
 *            (kepler_types.h), persisted to NV so the watch shows the last
 *            known weather on boot before the phone connects;
 *          - struct weather_data_s        : the UI view consumed by
 *            ui_renderer (forward-declared there since Task 1).  Temps are
 *            pre-converted to the user's display unit.
 *
 *        Unit toggle (BTN_2 on WEATHER screen) converts on the UI copy
 *        only; the wire/NV copy stays Celsius.
 *
 *****************************************************************************/

#ifndef WEATHER_SERVICE_H
#define WEATHER_SERVICE_H

#include <stdint.h>
#include <stdbool.h>
#include "../kepler_types.h"

/*--- UI view (authoritative definition of ui_renderer's forward decl) ----*/
struct weather_data_s {
    uint8_t  condition;       /* weather_condition_t (0-7, 0xFF unknown)   */
    int8_t   temp;            /* current temp in display unit              */
    int8_t   feels_like;      /* feels-like in display unit                */
    uint8_t  humidity_pct;
    uint32_t updated_at;      /* Unix timestamp of last BLE push           */
    char     unit;            /* 'C' or 'F'                                */
    struct {
        uint8_t condition;
        int8_t  temp;         /* in display unit                           */
        uint8_t hour;         /* 0-23                                      */
    } hourly[6];              /* [0] unused (NOW column), [1..5] forecast  */
};
typedef struct weather_data_s weather_data_t;

/*--- Global weather state (spec 06) ----------------------------------------*/
extern weather_payload_t g_weather;
extern bool              g_weather_valid;

/*==========================================================================*
 *  Public API                                                               *
 *==========================================================================*/

/* Load last-known weather + unit preference from NV and push to the UI.   *
 * Call after flash_store_init() and ui_init().                            */
void weather_service_init(void);

/* 0xFF06 write callback: validate, store, persist, post                   *
 * EVT_WEATHER_UPDATE.  Returns false on malformed input.                  */
bool weather_service_on_write(const uint8_t *data, uint16_t len);

/* Called by the main task on EVT_WEATHER_UPDATE: rebuild the UI view and  *
 * hand it to ui_update_weather().                                         */
void weather_service_apply(void);

/* BTN_2 on WEATHER screen: toggle degC/degF, persist preference,          *
 * re-render.                                                               */
void weather_service_toggle_units(void);

/* BTN_1 on WEATHER screen path: notify 0x01 on 0xFF0A so the app fetches  *
 * fresh data.                                                              */
void weather_service_request_refresh(void);

/* Age of the current data in minutes (0xFFFF if no data).                 */
uint16_t weather_service_age_min(void);

#endif /* WEATHER_SERVICE_H */

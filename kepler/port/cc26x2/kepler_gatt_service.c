/******************************************************************************
 *
 * @file  kepler_gatt_service.c
 *
 * @brief TI BLE5-Stack attribute table for the Kepler custom GATT service
 *        (0xFFFF).
 *
 *        STATUS: PORT SKELETON — written against BLE5-Stack documentation;
 *        attribute-table macros/permissions verified at bring-up against the
 *        SDK 7.x simple_gatt_profile example.
 *
 *****************************************************************************/

#include <string.h>
#include <icall.h>
#include "icall_ble_api.h"
#include "kepler_gatt_service.h"
#include "../../ble/gatt_uuids.h"
#include "../../kepler_main.h"

/*--- Callback forward declarations (needed by keplerServiceCBs below) -------*/

static bStatus_t readAttrCB(uint16_t connHandle, gattAttribute_t *pAttr,
                             uint8_t *pValue, uint16_t *pLen, uint16_t offset,
                             uint16_t maxLen, uint8_t method);
static bStatus_t writeAttrCB(uint16_t connHandle, gattAttribute_t *pAttr,
                              uint8_t *pValue, uint16_t len, uint16_t offset,
                              uint8_t method);

/*--- UUID arrays (ATT_BT_UUID_SIZE = 2 bytes, little-endian) ----------------*/

static CONST uint8 keplerServiceUUID[ATT_BT_UUID_SIZE] = {
    LO_UINT16(KEPLER_SVC_UUID),           HI_UINT16(KEPLER_SVC_UUID)
};
static CONST uint8 notifUUID[ATT_BT_UUID_SIZE] = {
    LO_UINT16(KEPLER_CHAR_NOTIF),         HI_UINT16(KEPLER_CHAR_NOTIF)
};
static CONST uint8 timeSyncUUID[ATT_BT_UUID_SIZE] = {
    LO_UINT16(KEPLER_CHAR_TIME_SYNC),     HI_UINT16(KEPLER_CHAR_TIME_SYNC)
};
static CONST uint8 stepsUUID[ATT_BT_UUID_SIZE] = {
    LO_UINT16(KEPLER_CHAR_STEPS),         HI_UINT16(KEPLER_CHAR_STEPS)
};
static CONST uint8 sleepUUID[ATT_BT_UUID_SIZE] = {
    LO_UINT16(KEPLER_CHAR_SLEEP),         HI_UINT16(KEPLER_CHAR_SLEEP)
};
static CONST uint8 settingsUUID[ATT_BT_UUID_SIZE] = {
    LO_UINT16(KEPLER_CHAR_SETTINGS),      HI_UINT16(KEPLER_CHAR_SETTINGS)
};
static CONST uint8 weatherUUID[ATT_BT_UUID_SIZE] = {
    LO_UINT16(KEPLER_CHAR_WEATHER),       HI_UINT16(KEPLER_CHAR_WEATHER)
};
static CONST uint8 locatorUUID[ATT_BT_UUID_SIZE] = {
    LO_UINT16(KEPLER_CHAR_LOCATOR),       HI_UINT16(KEPLER_CHAR_LOCATOR)
};
static CONST uint8 alarmsUUID[ATT_BT_UUID_SIZE] = {
    LO_UINT16(KEPLER_CHAR_ALARMS),        HI_UINT16(KEPLER_CHAR_ALARMS)
};
static CONST uint8 alarmTriggerUUID[ATT_BT_UUID_SIZE] = {
    LO_UINT16(KEPLER_CHAR_ALARM_TRIGGER), HI_UINT16(KEPLER_CHAR_ALARM_TRIGGER)
};
static CONST uint8 wxRefreshUUID[ATT_BT_UUID_SIZE] = {
    LO_UINT16(KEPLER_CHAR_WX_REFRESH),    HI_UINT16(KEPLER_CHAR_WX_REFRESH)
};

/*--- Service type (pValue of the service declaration attribute) -------------*/
/*  gattAttrType_t is the TI type; verify against gatt.h at bring-up.       */

static CONST gattAttrType_t keplerService = { ATT_BT_UUID_SIZE,
                                              keplerServiceUUID };

/*--- Characteristic property bytes ------------------------------------------*/

static uint8 notifProps        = GATT_PROP_WRITE_NO_RSP;
static uint8 timeSyncProps     = GATT_PROP_WRITE;
static uint8 stepsProps        = GATT_PROP_READ | GATT_PROP_NOTIFY;
static uint8 sleepProps        = GATT_PROP_READ;
static uint8 settingsProps     = GATT_PROP_WRITE;
static uint8 weatherProps      = GATT_PROP_WRITE_NO_RSP;
static uint8 locatorProps      = GATT_PROP_WRITE | GATT_PROP_NOTIFY;
static uint8 alarmsProps       = GATT_PROP_WRITE_NO_RSP;
static uint8 alarmTriggerProps = GATT_PROP_WRITE_NO_RSP;
static uint8 wxRefreshProps    = GATT_PROP_NOTIFY;

/*--- Value buffers (static, zero-initialised) --------------------------------*/

static uint8 notifValue[64];
static uint8 timeSyncValue[4];
static uint8 stepsValue[14];
static uint8 sleepValue[20];
static uint8 settingsValue[16];
static uint8 weatherValue[36];
static uint8 locatorValue[1];
static uint8 alarmsValue[66];
static uint8 alarmTriggerValue[1];
static uint8 wxRefreshValue[1];

/*--- CCCD tables -------------------------------------------------------------*/
/*  Allocated once in addService via ICall_malloc + GATTServApp_InitCharCfg. *
 *  This is the one place TI's BLE5-Stack requires heap; it is the canonical *
 *  CCCD pattern for BLE5-Stack peripherals, done once at init and never     *
 *  freed.  See simple_gatt_profile.c in the SDK 7.x examples.              */

static gattCharCfg_t *stepsClientCfg;
static gattCharCfg_t *locatorClientCfg;
static gattCharCfg_t *wxRefreshClientCfg;

/*--- Attribute table (24 attributes total) -----------------------------------*/
/*                                                                            *
 *  Layout:                                                                   *
 *   [0]  service declaration                                                 *
 *   [1]  char decl FF01   [2]  char value FF01  (WriteNoRsp 64 B)           *
 *   [3]  char decl FF02   [4]  char value FF02  (Write 4 B)                 *
 *   [5]  char decl FF03   [6]  char value FF03  [7]  CCCD (steps)           *
 *   [8]  char decl FF04   [9]  char value FF04  (Read 20 B)                 *
 *   [10] char decl FF05   [11] char value FF05  (Write 16 B)                *
 *   [12] char decl FF06   [13] char value FF06  (WriteNoRsp 36 B)           *
 *   [14] char decl FF07   [15] char value FF07  [16] CCCD (locator)         *
 *   [17] char decl FF08   [18] char value FF08  (WriteNoRsp 66 B)           *
 *   [19] char decl FF09   [20] char value FF09  (WriteNoRsp 1 B)            *
 *   [21] char decl FF0A   [22] char value FF0A  [23] CCCD (wxRefresh)       */

static gattAttribute_t keplerAttrTbl[] = {

    /* ---- Service declaration -------------------------------------------- */
    { { ATT_BT_UUID_SIZE, primaryServiceUUID },
      GATT_PERMIT_READ, 0, (uint8 *)&keplerService },

    /* ---- FF01  WriteNoRsp  app->watch  64 B ----------------------------- */
    { { ATT_BT_UUID_SIZE, characterUUID },
      GATT_PERMIT_READ, 0, &notifProps },
    { { ATT_BT_UUID_SIZE, notifUUID },
      GATT_PERMIT_WRITE, 0, notifValue },

    /* ---- FF02  Write  app->watch  4 B ----------------------------------- */
    { { ATT_BT_UUID_SIZE, characterUUID },
      GATT_PERMIT_READ, 0, &timeSyncProps },
    { { ATT_BT_UUID_SIZE, timeSyncUUID },
      GATT_PERMIT_WRITE, 0, timeSyncValue },

    /* ---- FF03  Read|Notify  watch->app  14 B  (+ CCCD) ------------------ */
    { { ATT_BT_UUID_SIZE, characterUUID },
      GATT_PERMIT_READ, 0, &stepsProps },
    { { ATT_BT_UUID_SIZE, stepsUUID },
      GATT_PERMIT_READ, 0, stepsValue },
    { { ATT_BT_UUID_SIZE, clientCharCfgUUID },
      GATT_PERMIT_READ | GATT_PERMIT_WRITE, 0, (uint8 *)&stepsClientCfg },

    /* ---- FF04  Read  watch->app  20 B ----------------------------------- */
    { { ATT_BT_UUID_SIZE, characterUUID },
      GATT_PERMIT_READ, 0, &sleepProps },
    { { ATT_BT_UUID_SIZE, sleepUUID },
      GATT_PERMIT_READ, 0, sleepValue },

    /* ---- FF05  Write  app->watch  16 B ---------------------------------- */
    { { ATT_BT_UUID_SIZE, characterUUID },
      GATT_PERMIT_READ, 0, &settingsProps },
    { { ATT_BT_UUID_SIZE, settingsUUID },
      GATT_PERMIT_WRITE, 0, settingsValue },

    /* ---- FF06  WriteNoRsp  app->watch  36 B ----------------------------- */
    { { ATT_BT_UUID_SIZE, characterUUID },
      GATT_PERMIT_READ, 0, &weatherProps },
    { { ATT_BT_UUID_SIZE, weatherUUID },
      GATT_PERMIT_WRITE, 0, weatherValue },

    /* ---- FF07  Write|Notify  bidir  1 B  (+ CCCD) ----------------------- */
    { { ATT_BT_UUID_SIZE, characterUUID },
      GATT_PERMIT_READ, 0, &locatorProps },
    { { ATT_BT_UUID_SIZE, locatorUUID },
      GATT_PERMIT_WRITE, 0, locatorValue },
    { { ATT_BT_UUID_SIZE, clientCharCfgUUID },
      GATT_PERMIT_READ | GATT_PERMIT_WRITE, 0, (uint8 *)&locatorClientCfg },

    /* ---- FF08  WriteNoRsp  app->watch  66 B ----------------------------- */
    { { ATT_BT_UUID_SIZE, characterUUID },
      GATT_PERMIT_READ, 0, &alarmsProps },
    { { ATT_BT_UUID_SIZE, alarmsUUID },
      GATT_PERMIT_WRITE, 0, alarmsValue },

    /* ---- FF09  WriteNoRsp  app->watch  1 B ------------------------------ */
    { { ATT_BT_UUID_SIZE, characterUUID },
      GATT_PERMIT_READ, 0, &alarmTriggerProps },
    { { ATT_BT_UUID_SIZE, alarmTriggerUUID },
      GATT_PERMIT_WRITE, 0, alarmTriggerValue },

    /* ---- FF0A  Notify  watch->app  1 B  (+ CCCD; value attr perms = 0) -- */
    { { ATT_BT_UUID_SIZE, characterUUID },
      GATT_PERMIT_READ, 0, &wxRefreshProps },
    { { ATT_BT_UUID_SIZE, wxRefreshUUID },
      0, 0, wxRefreshValue },
    { { ATT_BT_UUID_SIZE, clientCharCfgUUID },
      GATT_PERMIT_READ | GATT_PERMIT_WRITE, 0, (uint8 *)&wxRefreshClientCfg },
};

/*--- Service callback table --------------------------------------------------*/

static CONST gattServiceCBs_t keplerServiceCBs = {
    readAttrCB,   /* Read  — handles FF03 (14 B) and FF04 (20 B)            */
    writeAttrCB,  /* Write — handles all app->watch chars and CCCDs         */
    NULL          /* Authorization callback — not used                      */
};

/*--- Read attribute callback -------------------------------------------------*/

static bStatus_t readAttrCB(uint16_t connHandle, gattAttribute_t *pAttr,
                             uint8_t *pValue, uint16_t *pLen, uint16_t offset,
                             uint16_t maxLen, uint8_t method)
{
    bStatus_t status = SUCCESS;
    uint16    uuid16;

    (void)connHandle;
    (void)method;

    if (pAttr->type.len != ATT_BT_UUID_SIZE) {
        *pLen = 0;
        return ATT_ERR_INVALID_HANDLE;
    }

    /* Reject long-read requests (no characteristic here is > ATT_MTU). */
    if (offset != 0) {
        return ATT_ERR_ATTR_NOT_LONG;
    }

    uuid16 = BUILD_UINT16(pAttr->type.uuid[0], pAttr->type.uuid[1]);

    switch (uuid16) {
        case KEPLER_CHAR_STEPS:
            *pLen = (maxLen < 14u) ? maxLen : 14u;
            memcpy(pValue, stepsValue, *pLen);
            break;

        case KEPLER_CHAR_SLEEP:
            *pLen = (maxLen < 20u) ? maxLen : 20u;
            memcpy(pValue, sleepValue, *pLen);
            break;

        default:
            *pLen  = 0;
            status = ATT_ERR_ATTR_NOT_FOUND;
            break;
    }
    return status;
}

/*--- Write attribute callback ------------------------------------------------*/

static bStatus_t writeAttrCB(uint16_t connHandle, gattAttribute_t *pAttr,
                              uint8_t *pValue, uint16_t len, uint16_t offset,
                              uint8_t method)
{
    /*  This callback runs in the BLE stack task context.
     *  kepler_ble_on_char_write only copies data and posts to the kepler
     *  event queue — it never performs peripheral I/O — so the contract
     *  holds.                                                               */
    bStatus_t status = SUCCESS;
    uint16    uuid16;

    (void)method;

    if (pAttr->type.len != ATT_BT_UUID_SIZE) {
        return ATT_ERR_INVALID_HANDLE;
    }

    uuid16 = BUILD_UINT16(pAttr->type.uuid[0], pAttr->type.uuid[1]);

    switch (uuid16) {

        case GATT_CLIENT_CHAR_CFG_UUID:
            /* ProcessCCCWriteReq resolves the right cfg array via
             * pAttr->pValue (pointer-to-pointer stored in the table).      */
            status = GATTServApp_ProcessCCCWriteReq(connHandle, pAttr,
                                                    pValue, len, offset,
                                                    GATT_CLIENT_CFG_NOTIFY);
            break;

        case KEPLER_CHAR_NOTIF:
            if (offset != 0)   { return ATT_ERR_ATTR_NOT_LONG; }
            if (len != 64u)    { return ATT_ERR_INVALID_VALUE_SIZE; }
            memcpy(notifValue, pValue, len);
            kepler_ble_on_char_write(uuid16, pValue, len);
            break;

        case KEPLER_CHAR_TIME_SYNC:
            if (offset != 0)   { return ATT_ERR_ATTR_NOT_LONG; }
            if (len != 4u)     { return ATT_ERR_INVALID_VALUE_SIZE; }
            memcpy(timeSyncValue, pValue, len);
            kepler_ble_on_char_write(uuid16, pValue, len);
            break;

        case KEPLER_CHAR_SETTINGS:
            if (offset != 0)   { return ATT_ERR_ATTR_NOT_LONG; }
            if (len != 16u)    { return ATT_ERR_INVALID_VALUE_SIZE; }
            memcpy(settingsValue, pValue, len);
            kepler_ble_on_char_write(uuid16, pValue, len);
            break;

        case KEPLER_CHAR_WEATHER:
            if (offset != 0)   { return ATT_ERR_ATTR_NOT_LONG; }
            if (len != 36u)    { return ATT_ERR_INVALID_VALUE_SIZE; }
            memcpy(weatherValue, pValue, len);
            kepler_ble_on_char_write(uuid16, pValue, len);
            break;

        case KEPLER_CHAR_LOCATOR:
            if (offset != 0)   { return ATT_ERR_ATTR_NOT_LONG; }
            if (len != 1u)     { return ATT_ERR_INVALID_VALUE_SIZE; }
            memcpy(locatorValue, pValue, len);
            kepler_ble_on_char_write(uuid16, pValue, len);
            break;

        case KEPLER_CHAR_ALARMS:
            if (offset != 0)   { return ATT_ERR_ATTR_NOT_LONG; }
            if (len != 66u)    { return ATT_ERR_INVALID_VALUE_SIZE; }
            memcpy(alarmsValue, pValue, len);
            kepler_ble_on_char_write(uuid16, pValue, len);
            break;

        case KEPLER_CHAR_ALARM_TRIGGER:
            if (offset != 0)   { return ATT_ERR_ATTR_NOT_LONG; }
            if (len != 1u)     { return ATT_ERR_INVALID_VALUE_SIZE; }
            memcpy(alarmTriggerValue, pValue, len);
            kepler_ble_on_char_write(uuid16, pValue, len);
            break;

        default:
            status = ATT_ERR_ATTR_NOT_FOUND;
            break;
    }
    return status;
}

/*==========================================================================*
 *  Public API                                                               *
 *==========================================================================*/

bStatus_t KeplerGattService_addService(void)
{
    /* Allocate CCCD arrays — the one place TI's BLE5-Stack requires heap.
     * ICall_malloc + GATTServApp_InitCharCfg is the canonical pattern;
     * allocated once at init and never freed.                              */
    stepsClientCfg = (gattCharCfg_t *)ICall_malloc(
        sizeof(gattCharCfg_t) * MAX_NUM_BLE_CONNS);
    if (stepsClientCfg == NULL) { return bleMemAllocError; }
    GATTServApp_InitCharCfg(LINKDB_CONNHANDLE_INVALID, stepsClientCfg);

    locatorClientCfg = (gattCharCfg_t *)ICall_malloc(
        sizeof(gattCharCfg_t) * MAX_NUM_BLE_CONNS);
    if (locatorClientCfg == NULL) { return bleMemAllocError; }
    GATTServApp_InitCharCfg(LINKDB_CONNHANDLE_INVALID, locatorClientCfg);

    wxRefreshClientCfg = (gattCharCfg_t *)ICall_malloc(
        sizeof(gattCharCfg_t) * MAX_NUM_BLE_CONNS);
    if (wxRefreshClientCfg == NULL) { return bleMemAllocError; }
    GATTServApp_InitCharCfg(LINKDB_CONNHANDLE_INVALID, wxRefreshClientCfg);

    return GATTServApp_RegisterService(keplerAttrTbl,
                                       GATT_NUM_ATTRS(keplerAttrTbl),
                                       GATT_MAX_ENCRYPT_KEY_SIZE,
                                       &keplerServiceCBs);
}

bStatus_t KeplerGattService_setValue(uint16_t char_uuid,
                                     const uint8_t *value, uint16_t len)
{
    switch (char_uuid) {

        case KEPLER_CHAR_STEPS:
            if (len != 14u) { return INVALIDPARAMETER; }
            memcpy(stepsValue, value, len);
            GATTServApp_ProcessCharCfg(stepsClientCfg, stepsValue, FALSE,
                                       keplerAttrTbl,
                                       GATT_NUM_ATTRS(keplerAttrTbl),
                                       INVALID_TASK_ID, readAttrCB);
            break;

        case KEPLER_CHAR_SLEEP:
            /* Read-only; no CCCD — call just updates the stored value.    */
            if (len != 20u) { return INVALIDPARAMETER; }
            memcpy(sleepValue, value, len);
            break;

        case KEPLER_CHAR_LOCATOR:
            /* FF07 has no readAttrCB path, but GATTServApp_ProcessCharCfg *
             * requires the callback pointer; pass readAttrCB per TI API.  */
            if (len != 1u) { return INVALIDPARAMETER; }
            memcpy(locatorValue, value, len);
            GATTServApp_ProcessCharCfg(locatorClientCfg, locatorValue, FALSE,
                                       keplerAttrTbl,
                                       GATT_NUM_ATTRS(keplerAttrTbl),
                                       INVALID_TASK_ID, readAttrCB);
            break;

        case KEPLER_CHAR_WX_REFRESH:
            /* FF0A: same note as FF07 above re: readAttrCB argument.      */
            if (len != 1u) { return INVALIDPARAMETER; }
            memcpy(wxRefreshValue, value, len);
            GATTServApp_ProcessCharCfg(wxRefreshClientCfg, wxRefreshValue,
                                       FALSE, keplerAttrTbl,
                                       GATT_NUM_ATTRS(keplerAttrTbl),
                                       INVALID_TASK_ID, readAttrCB);
            break;

        default:
            return bleInvalidRange;
    }
    return SUCCESS;
}

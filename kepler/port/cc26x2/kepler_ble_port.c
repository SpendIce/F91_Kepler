/******************************************************************************
 *
 * @file  kepler_ble_port.c
 *
 * @brief ble_port_t implementation and application glue for the CC2652R7
 *        BLE5-Stack port.
 *
 *        STATUS: PORT SKELETON — GapAdv handle/param names are from
 *        BLE5-Stack documentation; verify against the SDK 7.x basic_ble
 *        example at bring-up.  The app owns advertisement-set creation via
 *        GapAdv_create; this port only re-enables/disables that set.
 *
 *****************************************************************************/

#include <icall.h>
#include "icall_ble_api.h"
#include <ti/sysbios/knl/Task.h>
#include "kepler_gatt_service.h"
#include "../../kepler_main.h"    /* brings in ble_manager.h / ble_port_t  */

/*--- Task configuration constants -------------------------------------------*/

#define KEPLER_TASK_STACK_SIZE  1024u
#define KEPLER_TASK_PRIORITY    1

/*--- Advertising handle (set by KeplerPort_init) ----------------------------*/

static uint8 advHandle;

/*--- Task static storage (no dynamic allocation) ----------------------------*/

static Task_Struct s_taskStruct;
static uint8       s_taskStack[KEPLER_TASK_STACK_SIZE];

/*--- Port functions ----------------------------------------------------------*/

static void port_start_advertising(uint16_t interval_units)
{
    uint32_t interval = interval_units;

    /* Disable first — ignore failure; the set may not be running yet.     */
    (void)GapAdv_disable(advHandle);

    /* Apply the new interval symmetrically on min and max.                */
    GapAdv_setParam(advHandle, GAP_ADV_PARAM_PRIMARY_INTERVAL_MIN, &interval);
    GapAdv_setParam(advHandle, GAP_ADV_PARAM_PRIMARY_INTERVAL_MAX, &interval);

    /* Re-enable with no hard duration limit (USE_MAX = run indefinitely). */
    GapAdv_enable(advHandle, GAP_ADV_ENABLE_OPTIONS_USE_MAX, 0);
}

static void port_stop_advertising(void)
{
    GapAdv_disable(advHandle);
}

static void port_notify_char(uint16_t char_uuid,
                             const uint8_t *value, uint16_t len)
{
    KeplerGattService_setValue(char_uuid, value, len);
}

/*--- Port function table -----------------------------------------------------*/

static const ble_port_t s_port = {
    port_start_advertising,
    port_stop_advertising,
    port_notify_char
};

/*--- Task entry point --------------------------------------------------------*/

static void keplerTaskFxn(UArg a0, UArg a1)
{
    (void)a0;
    (void)a1;
    kepler_main_task();   /* never returns */
}

/*==========================================================================*
 *  Public API                                                               *
 *  Note: no companion header is provided for this file; the app .c units   *
 *  that call these functions should declare them via extern.  Bring-up may *
 *  promote these prototypes into an app-level header.                      *
 *==========================================================================*/

/* Store the advertisement handle created by the app (via GapAdv_create),
 * register the GATT service, and wire the ble_port_t into the kepler
 * application.  Call once from the app task, before BLE advertising starts.*/
void KeplerPort_init(uint8 advHandleFromApp)
{
    advHandle = advHandleFromApp;
    KeplerGattService_addService();
    kepler_main_set_ble_port(&s_port);
}

/* Forward the GAP connection event into the kepler state machine.
 * Call from the app's GAP_LINK_ESTABLISHED_EVENT handler.
 * Runs in BLE stack task context; kepler_ble_on_connected only posts an
 * event to the kepler queue — no peripheral I/O.                          */
void KeplerPort_onConnected(void)
{
    kepler_ble_on_connected();
}

/* Forward the GAP disconnection event into the kepler state machine.
 * Call from the app's GAP_LINK_TERMINATED_EVENT handler.
 * Same task-context contract as KeplerPort_onConnected above.             */
void KeplerPort_onDisconnected(void)
{
    kepler_ble_on_disconnected();
}

/* Construct the kepler application task on the static stack below.
 * Call once from the app's startup code, before BIOS_start().             */
void KeplerPort_createTask(void)
{
    Task_Params params;

    Task_Params_init(&params);
    params.stack     = s_taskStack;
    params.stackSize = KEPLER_TASK_STACK_SIZE;
    params.priority  = KEPLER_TASK_PRIORITY;

    Task_construct(&s_taskStruct, keplerTaskFxn, &params, NULL);
}

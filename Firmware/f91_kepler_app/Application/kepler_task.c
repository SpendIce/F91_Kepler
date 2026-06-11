/******************************************************************************
 *
 * @file  kepler_task.c
 *
 * @brief TI-RTOS task wrapper for the new kepler/ firmware (Phase 0
 *        integration).
 *
 *        Runs kepler_main_task() — the central event loop — alongside the
 *        legacy F91 tasks.  No ICall registration: the kepler code never
 *        calls BLE stack APIs directly (everything goes through the
 *        ble_port_t table, which the legacy f91_kepler.c task will
 *        populate as the GATT migration proceeds), so ICALL_MAX_NUM_TASKS
 *        does not need to grow.
 *
 *        NOTE (migration state): while both the legacy display/button
 *        path and the kepler task are present, the kepler button driver
 *        must not open the same PIN entries as f91_buttons.c.  For the
 *        size-gate build this is acceptable — PIN_open failure leaves the
 *        kepler button driver inert; the legacy path keeps working.  The
 *        legacy display/button modules are removed in the next migration
 *        step.
 *
 *****************************************************************************/

#include <ti/sysbios/knl/Task.h>

#include "kepler/kepler_main.h"

/*--- Task configuration -----------------------------------------------------*/

#ifndef KEPLER_TASK_PRIORITY
#define KEPLER_TASK_PRIORITY    1
#endif

/* Rendering + snprintf paths are the deepest users; 1024 B leaves margin   *
 * over the legacy display task's 644 B.  VERIFY with ROV stack peak after  *
 * LaunchPad bring-up before trusting long-term.                            */
#ifndef KEPLER_TASK_STACK_SIZE
#define KEPLER_TASK_STACK_SIZE  1024
#endif

static Task_Struct keplerTaskStruct;
static Char        keplerTaskStack[KEPLER_TASK_STACK_SIZE];

/*--- Task body ----------------------------------------------------------------*/

static void KeplerMain_taskFxn(UArg a0, UArg a1)
{
    (void)a0;
    (void)a1;
    kepler_main_task();   /* never returns */
}

/*--- Public API -----------------------------------------------------------------*/

void KeplerMain_createTask(void)
{
    Task_Params taskParams;

    Task_Params_init(&taskParams);
    taskParams.stack     = keplerTaskStack;
    taskParams.stackSize = KEPLER_TASK_STACK_SIZE;
    taskParams.priority  = KEPLER_TASK_PRIORITY;

    Task_construct(&keplerTaskStruct, KeplerMain_taskFxn, &taskParams, NULL);
}

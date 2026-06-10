/******************************************************************************
 *
 * @file  event_queue.h
 *
 * @brief Central event queue for the F91 Kepler firmware.
 *
 *        All interrupt sources (button Swi's, accel INT1 ISR, BLE stack
 *        callbacks, timer callbacks) post events here; the main task pends
 *        and dispatches.  No peripheral logic ever runs inside an ISR.
 *
 *        Implementation: static 16-entry ring buffer + counting semaphore.
 *        Posting from Hwi/Swi/Task context is safe (critical section via
 *        Hwi_disable around the ring indices).  No dynamic memory.
 *
 *        The `data` pointer is NOT copied — it must point to static or
 *        otherwise persistent storage owned by the poster.
 *
 *****************************************************************************/

#ifndef EVENT_QUEUE_H
#define EVENT_QUEUE_H

#include <stdint.h>
#include <stdbool.h>

/*--- Event identifiers (master spec, 00_phase0_overview.md) --------------*/
typedef enum {
    /* Button events */
    EVT_BUTTON_1_SHORT,
    EVT_BUTTON_1_LONG,
    EVT_BUTTON_2_SHORT,
    EVT_BUTTON_2_LONG,
    EVT_BUTTON_3_SHORT,
    EVT_BUTTON_3_LONG,

    /* Accelerometer */
    EVT_WRIST_RAISE,

    /* Screen management */
    EVT_SCREEN_TIMEOUT,          /* inactivity -> return to MAIN           */

    /* BLE / connectivity */
    EVT_BLE_NOTIFICATION,        /* new notification from phone            */
    EVT_BLE_CONNECTED,
    EVT_BLE_DISCONNECTED,
    EVT_TIME_SYNC,               /* phone sent current Unix timestamp      */

    /* Pedometer & health */
    EVT_STEP_UPDATE,
    EVT_MIDNIGHT_RESET,
    EVT_BATTERY_LOW,

    /* Weather */
    EVT_WEATHER_UPDATE,          /* phone pushed new weather_payload_t     */
    EVT_WEATHER_REFRESH_REQ,     /* BTN_1 on WEATHER screen                */

    /* Phone locator */
    EVT_PHONE_LOCATOR_START,     /* BTN_1 on LOCATOR screen                */
    EVT_PHONE_LOCATOR_STOP,      /* BTN_1 again, or 30 s auto-stop         */
    EVT_PHONE_LOCATOR_ACK,       /* app acknowledged ring command          */

    /* Stopwatch */
    EVT_STOPWATCH_TICK,          /* 100 ms timer — update stopwatch view   */

    /* Alarms */
    EVT_ALARMS_UPDATE,           /* phone pushed new alarms_payload_t      */
    EVT_ALARM_TRIGGER,           /* phone sent alarm-fired notification    */
    EVT_ALARM_DISMISS,           /* user dismissed triggered alarm         */

    /* Display */
    EVT_DISPLAY_INVERT_RESTORE,  /* 3 s timer: restore display polarity    */

    /* Haptic (not in master spec enum — added because the repeat clock    *
     * runs in Swi context and must not touch I2C; main task re-triggers   *
     * the DRV2605L GO bit on this event instead)                          */
    EVT_HAPTIC_TICK,

    /* Buzzer sequence step (same rationale: PWM start/stop happens in     *
     * task context, the tone clock only posts)                            */
    EVT_BUZZER_TICK,

    /* Input pump: a button/time-set Swi has pending work — the main task  *
     * must call buttons_process() / time_set_process().  (Tasks 1-2 were  *
     * built standalone; this is the Task 5 integration wake-up.)          */
    EVT_INPUT_PUMP,

    /* One-minute scheduler tick: time/date refresh, pedometer poll,       *
     * hour chime, midnight + sleep-window-end detection.                  */
    EVT_MINUTE_TICK,

    /* Fast-advertising window expired — demote to slow advertising.       */
    EVT_BLE_ADV_WINDOW,

    /* 2 s notification banner overlay expired — restore screen rows.      */
    EVT_BANNER_EXPIRE,

    EVT_COUNT
} kepler_event_t;

typedef struct {
    kepler_event_t type;
    uint32_t       param;   /* event-specific payload                      */
    void          *data;    /* optional pointer (poster-owned storage)     */
} kepler_event_msg_t;

/*--- Queue depth (power of two) ------------------------------------------*/
#define KEPLER_EVENT_QUEUE_SIZE  16u

/*==========================================================================*
 *  Public API                                                               *
 *==========================================================================*/

/* Construct the semaphore and reset the ring.  Call once at startup.      */
void event_queue_init(void);

/* Post an event.  Safe from Hwi, Swi or Task context.                     *
 * On overflow the newest event is dropped (oldest preserved) and the      *
 * dropped-event counter increments.                                        */
void event_queue_post(kepler_event_t type, uint32_t param, void *data);

/* Block until an event arrives or timeout_ms elapses.                     *
 * Pass EVENT_QUEUE_WAIT_FOREVER to block indefinitely (CPU enters         *
 * STANDBY while pending).  Returns true and fills *out on success.        */
bool event_queue_pend(kepler_event_msg_t *out, uint32_t timeout_ms);

#define EVENT_QUEUE_WAIT_FOREVER  0xFFFFFFFFu

/* Diagnostics: events dropped due to a full ring since boot.              */
uint32_t event_queue_dropped(void);

/* Number of events currently queued (snapshot, for tests/diagnostics).    */
uint8_t  event_queue_depth(void);

#endif /* EVENT_QUEUE_H */

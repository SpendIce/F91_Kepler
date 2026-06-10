/******************************************************************************
 *
 * @file  buzzer.h
 *
 * @brief PWM piezo buzzer driver (Task 5/8).
 *
 *        Tones are non-blocking: buzzer_tone() starts the PWM and arms a
 *        Clock; the Clock callback posts EVT_BUZZER_TICK and the main task
 *        calls buzzer_tick() to advance/stop the sequence in task context
 *        (no peripheral I/O in Swi).
 *
 *        Named sequences are step tables of {freq_hz, duration_ms} pairs;
 *        buzzer_alarm() loops until buzzer_stop().
 *
 *        Feature guard: KEPLER_HAS_BUZZER.  The v1 PCB has buzzer traces
 *        but the PWM pin is unverified — flag stays 0 until the schematic
 *        confirms KEPLER_BUZZER_PWM_PIN.
 *
 *****************************************************************************/

#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>
#include <stdbool.h>

/* Open the PWM instance and construct the step clock.                     */
void buzzer_init(void);

/* Play a tone at freq_hz for duration_ms.  Non-blocking; replaces any     *
 * sequence in progress.                                                    */
void buzzer_tone(uint16_t freq_hz, uint16_t duration_ms);

/* Stop immediately (kills PWM output — nothing bleeds through).           */
void buzzer_stop(void);

/* Named sequences */
void buzzer_alarm(void);       /* repeating alarm pattern until stop()     */
void buzzer_hour_chime(void);  /* single short beep on the hour            */
void buzzer_notify(void);      /* short two-tone notification beep         */

/* Advance the active sequence.  Called by the main task on               *
 * EVT_BUZZER_TICK only.                                                   */
void buzzer_tick(void);

/* True while a tone or sequence is sounding.                              */
bool buzzer_is_active(void);

#if defined(KEPLER_TEST_ONLY)
/* Simulate expiry of the step clock (posts EVT_BUZZER_TICK).              */
void buzzer_test_fire_step(void);
#endif

#endif /* BUZZER_H */

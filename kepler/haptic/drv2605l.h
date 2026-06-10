/******************************************************************************
 *
 * @file  drv2605l.h
 *
 * @brief I2C driver for the TI DRV2605L haptic controller (Task 3).
 *
 *        Target motor: generic 10 mm x 2.5 mm coin ERM on OUTP/OUTN.
 *        ERM waveform library 1, internal-trigger mode, standby between
 *        events (~0.6 uA).
 *
 *        Auto-calibration runs once (~1.2 s, blocking Task_sleep polls);
 *        results are persisted via flash_store and reloaded on later boots.
 *
 *        Feature guard: KEPLER_HAS_DRV2605L.  When 0 every function
 *        compiles to a stub (init returns true) so callers need no guards.
 *
 *        Task context only — all calls perform blocking I2C transfers.
 *
 *****************************************************************************/

#ifndef DRV2605L_H
#define DRV2605L_H

#include <stdint.h>
#include <stdbool.h>

/*--- Register map (datasheet 7.6) -----------------------------------------*/
#define DRV2605L_REG_STATUS         0x00
#define DRV2605L_REG_MODE           0x01
#define DRV2605L_REG_RTP            0x02
#define DRV2605L_REG_LIBRARY        0x03
#define DRV2605L_REG_WAVESEQ1       0x04   /* ..WAVESEQ8 = 0x0B            */
#define DRV2605L_REG_GO             0x0C
#define DRV2605L_REG_OVERDRIVE      0x0D
#define DRV2605L_REG_SUSTAIN_P      0x0E
#define DRV2605L_REG_SUSTAIN_N      0x0F
#define DRV2605L_REG_BRAKE          0x10
#define DRV2605L_REG_RATED_VOLTAGE  0x16
#define DRV2605L_REG_OD_CLAMP       0x17
#define DRV2605L_REG_AUTOCAL_COMP   0x18
#define DRV2605L_REG_AUTOCAL_BEMF   0x19
#define DRV2605L_REG_FEEDBACK       0x1A
#define DRV2605L_REG_CONTROL1       0x1B
#define DRV2605L_REG_CONTROL2       0x1C
#define DRV2605L_REG_CONTROL3       0x1D
#define DRV2605L_REG_CONTROL4       0x1E

/*--- MODE register values ---------------------------------------------------*/
#define DRV2605L_MODE_INT_TRIGGER   0x00
#define DRV2605L_MODE_AUTOCAL       0x07
#define DRV2605L_MODE_STANDBY_BIT   0x40

/*--- STATUS register bits -----------------------------------------------------*/
#define DRV2605L_STATUS_DIAG_FAIL   0x08   /* auto-cal / diagnostic failed */
#define DRV2605L_STATUS_DEVID_MASK  0xE0   /* 0xE0 = DRV2605L (ID 7)       */

/*--- Waveform sequencer ---------------------------------------------------------*/
#define DRV2605L_SEQ_MAX            8u

/*==========================================================================*
 *  Public API                                                               *
 *==========================================================================*/

/* Probe device, select ERM library, run or reload auto-calibration,       *
 * leave the part in standby.  Returns false if the device is absent or    *
 * calibration reports a failure.                                          */
bool drv2605l_init(void);

/* Load up to DRV2605L_SEQ_MAX library effect IDs into the sequencer and   *
 * fire GO.  `effects` is zero-terminated ({0} ends the sequence early).   *
 * Exits standby first.  Returns immediately; playback is asynchronous.    */
bool drv2605l_play_sequence(const uint8_t *effects, uint8_t count);

/* Re-fire GO with the currently loaded sequence (repeat support).         */
bool drv2605l_retrigger(void);

/* Clear GO and enter standby — motor brakes immediately.                  */
bool drv2605l_stop(void);

/* Read back the GO bit (true while the sequencer is playing).             */
bool drv2605l_is_playing(void);

#endif /* DRV2605L_H */

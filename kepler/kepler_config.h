/******************************************************************************
 *
 * @file  kepler_config.h
 *
 * @brief Compile-time configuration for the F91 Kepler firmware rewrite.
 *
 *        All GPIO pin assignments, hardware feature flags, and tuning
 *        constants live in this single file.  No other source file should
 *        hard-code IOID values — reference the defines below instead.
 *
 * Pin assignment tiers
 * --------------------
 *   TIER 1 — V1 PCB CONFIRMED
 *       Pins verified from the Kepler v1 firmware source
 *       (f91_buttons.c, ssd1306.c, Board.h / TI SDK).
 *       These are the real hardware connections on the existing PCB.
 *
 *   TIER 2 — LAUNCHPAD DEVELOPMENT  (KEPLER_PHASE0_LAUNCHPAD_OVERRIDE)
 *       Temporary assignments for Phase 0 development on the
 *       LAUNCHXL-CC2640R2F.  Every pin in this tier WILL be
 *       reassigned when the v2 PCB schematic is finalised.
 *
 *   TIER 3 — FUTURE HARDWARE  (not on v1 PCB or launchpad)
 *       Signals for components that do not yet exist in hardware.
 *       Set to IOID_UNUSED.  Assign real IOID values when the
 *       v2 schematic provides them.
 *
 *****************************************************************************/

#ifndef KEPLER_CONFIG_H
#define KEPLER_CONFIG_H

#include <driverlib/ioc.h>       /* IOID_x constants                      */

/* Sentinel for pins not yet routed to physical hardware.                   */
#define IOID_UNUSED                 0xFF

/*==========================================================================*
 *  TIER 1 — V1 PCB CONFIRMED                                              *
 *                                                                          *
 *  Verified from Kepler v1 firmware.  Change only if the schematic is      *
 *  re-read and a correction is needed.                                     *
 *==========================================================================*/

/* -- I2C bus (shared by SSD1306 on v1; DRV2605L, LIS2DW12, ST25DV on v2) */
#define KEPLER_I2C_INSTANCE         0
#define KEPLER_I2C_SDA_PIN          IOID_5
#define KEPLER_I2C_SCL_PIN          IOID_6

/* -- Buttons (active HIGH, internal pull-down on Kepler PCB)              *
 *    On the launchpad (IS_DEV_BOARD), f91_buttons.c maps these to         *
 *    Board_BUTTON0/1 instead (active LOW, pull-up).                       *
 *    NOTE: KEPLER_BTN1_PIN shares the same IOID as SHARP_LCD_DISP_PIN    *
 *    (Tier 2).  No runtime conflict: the launchpad never activates the    *
 *    Kepler-PCB button defines, and the v2 PCB will reassign the LCD      *
 *    pins.                                                                */
#define KEPLER_BTN1_PIN             IOID_15   /* BUTTON_0 — top-left       */
#define KEPLER_BTN2_PIN             IOID_25   /* BUTTON_1 — bottom-left    */
#define KEPLER_BTN3_PIN             IOID_26   /* BUTTON_2 — bottom-right   */

/* -- RF switch (managed by TI radio core, listed for completeness)        */
#define KEPLER_RFSW_PIN             IOID_1    /* Board_DIO1_RFSW           */
#define KEPLER_RFSW_PWR_PIN         IOID_30   /* Board_DIO30_SWPWR         */

/*==========================================================================*
 *  TIER 1.5 — V2 PCB (CC2652R7 RGZ48)  — AUTHORITATIVE PIN MAP            *
 *                                                                          *
 *  Source of truth: Hardware/v2/f91_kepler_v2.kicad_sch / .kicad_pcb and  *
 *  Hardware/v2/PINMAP.md.  LCD moved to top-edge DIOs (26-30) because the *
 *  QFN south pads are unroutable on the 2-layer board (see PINMAP.md).    *
 *  Build with -DKEPLER_BOARD_V2 to activate.                              *
 *==========================================================================*/
#ifdef KEPLER_BOARD_V2
  /* buttons (SW1 left, SW2/SW3 right edge) */
  #undef  KEPLER_BTN1_PIN
  #undef  KEPLER_BTN2_PIN
  #undef  KEPLER_BTN3_PIN
  #define KEPLER_BTN1_PIN           IOID_0    /* pad 5                     */
  #define KEPLER_BTN2_PIN           IOID_25   /* pad 38                    */
  #define KEPLER_BTN3_PIN           IOID_20   /* pad 30                    */
  /* Sharp Memory LCD (J1 FPC, top edge) */
  #define KEPLER_V2_LCD_MOSI_PIN    IOID_27   /* pad 40                    */
  #define KEPLER_V2_LCD_SCLK_PIN    IOID_28   /* pad 41                    */
  #define KEPLER_V2_LCD_CS_PIN      IOID_29   /* pad 42 — ACTIVE HIGH      */
  #define KEPLER_V2_LCD_DISP_PIN    IOID_30   /* pad 43                    */
  #define KEPLER_V2_LCD_VCOM_PIN    IOID_26   /* pad 39 — EXTCOMIN         */
  /* UART bootloader / debug console (pogo pads TP6/TP7) */
  #define KEPLER_V2_UART_RX_PIN     IOID_9    /* pad 15                    */
  #define KEPLER_V2_UART_TX_PIN     IOID_10   /* pad 16                    */
  /* peripherals */
  #define KEPLER_V2_ACC_INT1_PIN    IOID_7    /* pad 12 — LIS2DW12 INT1    */
  #define KEPLER_V2_ACC_INT2_PIN    IOID_11   /* pad 17 — LIS2DW12 INT2    */
  #define KEPLER_V2_NFC_GPO_PIN     IOID_14   /* pad 20 — ST25DV GPO       */
  #define KEPLER_V2_CHG_DET_PIN     IOID_21   /* pad 31 — Qi charge detect */
  #define KEPLER_V2_BUZZER_PIN      IOID_22   /* pad 32 — piezo PWM        */
  #define KEPLER_V2_HAPTIC_EN_PIN   IOID_24   /* pad 37 — DRV2605L EN      */
#endif /* KEPLER_BOARD_V2 */

/*==========================================================================*
 *  TIER 2 — LAUNCHPAD DEVELOPMENT  (KEPLER_PHASE0_LAUNCHPAD_OVERRIDE)     *
 *                                                                          *
 *  Temporary GPIO assignments for Phase 0 on the CC2640R2F launchpad.     *
 *  To swap a pin, change the IOID_xx value here — nothing else to touch.  *
 *==========================================================================*/

/* -- SPI bus (launchpad SPI0 peripheral mapping) --------------------------*/
#define KEPLER_SPI_INSTANCE         0
#define KEPLER_SPI_CLK_PIN          IOID_10   /* KEPLER_PHASE0_LAUNCHPAD_OVERRIDE */
#define KEPLER_SPI_MOSI_PIN         IOID_9    /* KEPLER_PHASE0_LAUNCHPAD_OVERRIDE */
#define KEPLER_SPI_MISO_PIN         IOID_8    /* KEPLER_PHASE0_LAUNCHPAD_OVERRIDE — unused by Sharp LCD */
#define KEPLER_SPI_BITRATE          1000000   /* 1 MHz — Sharp LS013B7DH03 max   */

/* -- Sharp Memory LCD control signals ------------------------------------*
 *    New for the v2 PCB; no traces on v1.  For launchpad dev these are    *
 *    assigned to free DIO pins on the BoosterPack header.                 *
 *                                                                         *
 *    IOID_14 note: coincides with Board_BUTTON1 on the launchpad.         *
 *    Acceptable for Task 1 (display-only); must be revisited before       *
 *    Task 2 (button handling) is brought up on the launchpad.             *
 *------------------------------------------------------------------------*/
#define SHARP_LCD_CS_PIN            IOID_14   /* KEPLER_PHASE0_LAUNCHPAD_OVERRIDE — ACTIVE HIGH */
#define SHARP_LCD_DISP_PIN          IOID_15   /* KEPLER_PHASE0_LAUNCHPAD_OVERRIDE */
#define SHARP_LCD_VCOM_PIN          IOID_16   /* KEPLER_PHASE0_LAUNCHPAD_OVERRIDE */

/*==========================================================================*
 *  TIER 3 — FUTURE HARDWARE  (not on v1 PCB or launchpad)                 *
 *                                                                          *
 *  Set to IOID_UNUSED.  Enable and assign real IOID values once the       *
 *  v2 PCB schematic routes these signals.                                 *
 *==========================================================================*/

#define KEPLER_DRV2605L_EN_PIN      IOID_UNUSED   /* Haptic motor driver enable   */
#define KEPLER_LIS2DW12_INT1_PIN    IOID_UNUSED   /* Accelerometer interrupt 1    */
#define KEPLER_LIS2DW12_INT2_PIN    IOID_UNUSED   /* Accelerometer interrupt 2    */
#define KEPLER_ST25DV_GPO_PIN       IOID_UNUSED   /* NFC tag GPO interrupt        */
#define KEPLER_INDUCTIVE_CHG_PIN    IOID_UNUSED   /* Inductive charge detection   */
#define KEPLER_BUZZER_PWM_PIN       IOID_UNUSED   /* Piezo buzzer PWM output      */
#define KEPLER_BUZZER_PWM_INDEX     0             /* TI PWM driver instance       */

/*==========================================================================*
 *  HARDWARE FEATURE FLAGS                                                  *
 *                                                                          *
 *  Guard all hardware-dependent code with #if KEPLER_HAS_xxx.             *
 *  Code must compile cleanly with any flag set to 0.                      *
 *==========================================================================*/

/* All flags are #ifndef-guarded so host test builds (and CCS build       *
 * configurations) can force a flag with -DKEPLER_HAS_xxx=1.              */
#ifndef KEPLER_HAS_SHARP_LCD
#define KEPLER_HAS_SHARP_LCD        1   /* Always 1 from Phase 0 onward     */
#endif
#ifndef KEPLER_HAS_BUTTONS
#define KEPLER_HAS_BUTTONS          1   /* Always 1 — buttons on all hardware */
#endif
#ifndef KEPLER_HAS_DRV2605L
#define KEPLER_HAS_DRV2605L         0   /* Enable when haptic motor present */
#endif
#ifndef KEPLER_HAS_LIS2DW12
#define KEPLER_HAS_LIS2DW12         0   /* Enable when accelerometer present*/
#endif
#ifndef KEPLER_HAS_ST25DV
#define KEPLER_HAS_ST25DV           0   /* Enable when NFC tag present      */
#endif
#ifndef KEPLER_HAS_INDUCTIVE_CHG
#define KEPLER_HAS_INDUCTIVE_CHG    0   /* Enable when charge coil present  */
#endif
#ifndef KEPLER_HAS_BUZZER
#define KEPLER_HAS_BUZZER           0   /* Enable once the v1 buzzer trace  *
                                         * is verified and the PWM pin set  */
#endif

/* Derived feature flags — auto-enable when parent hardware is present     */
#define KEPLER_STEP_COUNTER         (KEPLER_HAS_LIS2DW12)
#define KEPLER_ACTIGRAPHY           (KEPLER_HAS_LIS2DW12)
#define KEPLER_WRIST_RAISE          (KEPLER_HAS_LIS2DW12)
#define KEPLER_HAPTIC               (KEPLER_HAS_DRV2605L)
#define KEPLER_NFC_TAG              (KEPLER_HAS_ST25DV)

/*==========================================================================*
 *  BUTTON TIMING                                                           *
 *==========================================================================*/

#define KEPLER_DEBOUNCE_MS              20
#define KEPLER_LONG_PRESS_MS           600
#define KEPLER_TIME_SET_BLINK_MS       250
#define KEPLER_TIME_SET_TIMEOUT_MS   30000
#define KEPLER_TIME_SET_CONFIRM_MS    1500

/*--- Dev-board vs target polarity ----------------------------------------*
 *                                                                          *
 *  KEPLER_DEV_BOARD: Launchpad buttons are active LOW with pull-up.       *
 *  Target PCB:       Kepler buttons are active HIGH with pull-down.       *
 *                                                                          *
 *  SHARP_LCD_CS_PIN must be remapped to IOID_11 on the launchpad to       *
 *  avoid conflicting with Board_BUTTON1 at IOID_14.                       *
 *------------------------------------------------------------------------*/
#ifdef KEPLER_DEV_BOARD
  #define KEPLER_BTN_ACTIVE_LOW     1
  #define KEPLER_BTN_PULL           PIN_PULLUP
  #define KEPLER_BTN_PRESS_EDGE     PIN_IRQ_NEGEDGE
  #define KEPLER_BTN_RELEASE_EDGE   PIN_IRQ_POSEDGE
  /* Remap CS to IOID_11 — IOID_14 is Board_BUTTON1 on the launchpad */
  #undef  SHARP_LCD_CS_PIN
  #define SHARP_LCD_CS_PIN          IOID_11
#else
  #define KEPLER_BTN_ACTIVE_LOW     0
  #define KEPLER_BTN_PULL           PIN_PULLDOWN
  #define KEPLER_BTN_PRESS_EDGE     PIN_IRQ_POSEDGE
  #define KEPLER_BTN_RELEASE_EDGE   PIN_IRQ_NEGEDGE
#endif

/*==========================================================================*
 *  SCREEN & UI TUNING                                                     *
 *==========================================================================*/

#define KEPLER_SCREEN_COUNT                 6      /* MAIN, WEATHER, NOTIF, LOCATOR, STOPWATCH, ALARMS */
#define KEPLER_SCREEN_TIMEOUT_MS            8000   /* Auto-return to MAIN after inactivity   */
#define KEPLER_INVERT_DISPLAY_DURATION_MS   3000   /* BTN_1 long-press: invert for 3 s       */

/*==========================================================================*
 *  NOTIFICATION RING BUFFER                                               *
 *==========================================================================*/

/* 5 entries (~100 B each).  Plan Maestro R1 names this ring the first    *
 * RAM recorte; baseline .map showed only ~6 KB free, so the cut is       *
 * taken up front.  5 is still plenty for a watch.                        */
#define KEPLER_NOTIF_RING_SIZE              5      /* Max stored in RAM   */

/*==========================================================================*
 *  WEATHER                                                                *
 *==========================================================================*/

#define KEPLER_WEATHER_STALE_SEC            3600   /* Show stale indicator if data > 1 h old */
#define KEPLER_WEATHER_AUTO_REFRESH_SEC     1800   /* Request refresh every 30 min on BLE    */

/*==========================================================================*
 *  PHONE LOCATOR                                                          *
 *==========================================================================*/

#define KEPLER_LOCATOR_AUTO_STOP_SEC        30     /* Auto-stop ringing after 30 s           */

/*==========================================================================*
 *  PEDOMETER                                                              *
 *==========================================================================*/

#define KEPLER_STEP_GOAL_DEFAULT            8000

/*==========================================================================*
 *  SLEEP TRACKING                                                         *
 *==========================================================================*/

#define KEPLER_SLEEP_WINDOW_START_H         22
#define KEPLER_SLEEP_WINDOW_END_H           8

/*==========================================================================*
 *  BLE ADVERTISING                                                        *
 *==========================================================================*/

#define KEPLER_BLE_ADV_INTERVAL_FAST_MS     100
#define KEPLER_BLE_ADV_INTERVAL_SLOW_MS     2000
#define KEPLER_BLE_ADV_FAST_DURATION_MS     30000

#endif /* KEPLER_CONFIG_H */

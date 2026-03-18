# F91 Kepler — Project Context for Claude Code

## What this project is

A complete firmware rewrite for the Casio F91W smartwatch based on the open-source
F91 Kepler project. The original author burned out and open-sourced everything under MIT.
This fork completes and significantly extends the firmware.

**Original repo:** https://github.com/PegorK/F91_Kepler
**Hardware:** Texas Instruments CC2640R2F (ARM Cortex-M3, BLE 4.2)
**Build system:** TI Code Composer Studio (CCS) or IAR — NOT a Makefile/CMake project
**Language:** C (no C++)
**SDK:** TI SimpleLink CC2640R2 SDK v4.x

## Repository structure (original + new)

```
F91_Kepler/
  Firmware/           ← original Kepler firmware (CCS project)
    Application/      ← main application source (kepler_app.c, board files, etc.)
    Startup/          ← TI-RTOS startup code
    Profiles/         ← BLE GATT profile stubs
    .cproject         ← CCS project file
    .project
  Hardware/           ← KiCad schematic and PCB files
  Software/           ← original (empty) phone app placeholder
  docs/               ← NEW: all project documentation
    phase0/           ← NEW: per-task firmware specs
  kepler/             ← NEW: all new firmware modules (added alongside Firmware/)
    display/
    input/
    haptic/
    accel/
    power/
    ble/
    screens/
    storage/
    audio/
    kepler_main.c
    kepler_config.h
  CLAUDE.md           ← this file
```

## Documentation location

All specification files live in `docs/phase0/`. Read them before writing any code.
They are the authoritative source for every interface, data structure, pin assignment,
and design decision. When in doubt, read the spec — do not infer.

Key files:
- `docs/phase0/00_phase0_overview.md` — architecture, module map, config flags, all events
- `docs/phase0/01_sharp_lcd_driver.md` — Sharp LCD SPI driver + UI renderer
- `docs/phase0/02_buttons_time_setting.md` — button model + time-set state machine
- `docs/phase0/03_haptic_driver.md` — DRV2605L I2C driver
- `docs/phase0/04_accelerometer.md` — LIS2DW12 driver + pedometer + wrist-raise
- `docs/phase0/05_power_ble_storage_buzzer.md` — event queue, BLE GATT, flash, buzzer
- `docs/phase0/06_screens_weather_ui.md` — all 6 screens, weather, stopwatch, alarms

## GPIO pin assignments

**CRITICAL: Before writing any driver, read the actual board file:**
```
Firmware/Application/CC2640R2_KEPLER.h
```
That file contains the definitive IOID_x assignments for this custom PCB.
Update `kepler/kepler_config.h` with the real values found there.

Expected pins to verify (placeholders until board file is read):
- SPI CLK, MOSI, Sharp LCD CS (active HIGH), DISP, VCOM
- I2C SDA, SCL
- Button 1, 2, 3 (active LOW with pull-up)
- Buzzer PWM output

## Rules for all code in this project

1. **No dynamic memory** — no malloc, no calloc, no free. All buffers are static or stack.
2. **No C++** — pure C only. The CCS project is configured for C.
3. **ISRs post to event queue only** — no peripheral I/O inside interrupt handlers.
4. **Feature flags guard hardware** — all hardware-dependent code wrapped in
   `#if KEPLER_HAS_xxx` from `kepler_config.h`. Code must compile without hardware present.
5. **Confirm headers before .c files** — show the .h interface and wait for approval
   before writing the implementation.
6. **One task per session** — do not start a new task until the current one is accepted.
7. **Verify against spec** — if there is a conflict between what you would normally do
   and what the spec says, follow the spec and flag the conflict.

## Build instructions

This is a CCS (Code Composer Studio) project. To build from the command line:

```bash
# CCS headless build (requires CCS installed)
eclipse -noSplash \
  -application com.ti.ccstudio.apps.projectBuild \
  -ccs.workspace /path/to/workspace \
  -ccs.projects F91_Kepler_Firmware \
  -ccs.configuration Debug
```

For iterative development, open CCS GUI and build from there.
Flash with XDS110 debug probe connected to the programming jig pads.

## Current firmware state (original Kepler baseline)

What works in the original firmware before any changes:
- BLE advertising, pairing, and connection
- Notifications displayed via nRF Connect diagnostic app
- SSD1306 OLED driver (to be replaced with Sharp LCD driver)
- Button 1 fires a wake interrupt (buttons 2 and 3 have GPIO connections but no handlers)
- ~15 day battery life baseline

What does NOT work yet:
- Buttons 2 and 3
- Time setting via buttons
- Companion Android app (Software/ dir is empty)
- Sound (PCB has buzzer traces but no firmware)
- Any of the new modules in kepler/

## Session 1 task

Implement Task 1: Sharp Memory LCD driver + UI renderer.
Read `docs/phase0/01_sharp_lcd_driver.md` in full before writing any code.
Read `Firmware/Application/CC2640R2_KEPLER.h` to verify actual GPIO pin numbers.
Start by showing the verified pin assignments, then proceed with `sharp_lcd.h`.

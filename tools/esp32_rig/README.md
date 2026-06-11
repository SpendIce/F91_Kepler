# F91 Kepler — ESP32 Peripheral Test Rig

Validates the watch's real peripherals (Sharp Memory LCD, DRV2605L haptic, LIS2DW12 accelerometer) by compiling the repo's `kepler/` firmware sources unchanged on an ESP32 devkit, through a TI-API shim layer (`main/tistubs/` + `main/rigshim.c`) that maps TI drivers and TI-RTOS onto ESP-IDF (spi_master, i2c, esp_timer, FreeRTOS, NVS). No BLE — BLE validation needs the LP-CC2652R7 LaunchPad.

## Hardware

Classic ESP32-WROOM devkit, all 3.3 V. Wire breakouts as follows:

| Peripheral signal | ESP32 pin |
|---|---|
| Sharp LCD SCLK | GPIO18 |
| Sharp LCD SI (MOSI) | GPIO23 |
| Sharp LCD SCS (chip select, ACTIVE HIGH, manual) | GPIO5 |
| Sharp LCD DISP | GPIO17 |
| Sharp LCD EXTCOMIN (VCOM) | GPIO16 |
| I2C SDA (DRV2605L + LIS2DW12 shared bus) | GPIO21 |
| I2C SCL | GPIO22 |
| LIS2DW12 INT1 | GPIO34 |

**Important notes:**

- All parts are **3.3 V only** — never 5 V.
- Sharp LCD EXTMODE must be strapped HIGH; GPIO16 drives VCOM toggling at 1 Hz.
- Most I2C breakouts have pull-ups; add 4.7 kΩ resistors to 3V3 if needed.
- DRV2605L I2C address is 0x5A; LIS2DW12 is 0x18 (SA0 to GND).
- Connect an ERM coin motor across DRV2605L OUTP and OUTN.
- Pin remapping lives in `main/rig_config.h` only — edit there to adapt to your wiring.

## Build and Flash

Requires [ESP-IDF v5.x](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/). Install it first.

```bash
cd tools/esp32_rig
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

Serial console runs at 115200 baud. Exit monitor with **Ctrl+]**.

## Test Menu

The serial console offers:

- **1**: Render all six watch screens with demo data, 3 seconds each
- **2**: Display invert torture test, 10 cycles (verify pixel quality and contrast)
- **3**: Haptic init + auto-calibration, then play every pattern
- **4**: HAPTIC_CALL repeating pattern for 5 seconds, then hard stop
- **5**: Stream step count + raw accel XYZ (walk around with the board)
- **6**: Wrist-raise interrupt watch (lift the board to trigger detection)
- **7**: Watch demo — live clock + screen carousel

## What This Validates

- **Sharp LCD protocol**: ACTIVE-HIGH CS pulse, chunked flush timing, 1 Hz VCOM toggle on GPIO16. Verify with an oscilloscope per the Plan Maestro checklist.
- **Fonts and layout**: All six screens rendered with real fonts and weather icons on actual glass — confirms legibility and icon placement.
- **DRV2605L haptics**: Auto-calibration detection with the real motor; tactile feedback from every pattern preset.
- **LIS2DW12 accelerometer**: Hardware step counter and wrist-raise detection. Fine-tune thresholds by editing `LIS2DW12_WAKE_THS_DEFAULT` in `kepler/accel/lis2dw12.h` (range 12–24).

**What it cannot validate**: anything BLE — that needs the CC2652R7 LaunchPad.
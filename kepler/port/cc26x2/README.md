# kepler/port/cc26x2 — SimpleLink SDK 7.x port layer (CC2652R7)

**Status:** PORT SKELETON

This port layer bridges Kepler firmware modules to TI SimpleLink CC13xx/CC26xx SDK 7.x targeting the CC2652R7 MCU. Written against TI SimpleLink CC13xx/CC26xx SDK 7.x and BLE5-Stack documentation; nothing here has been compiled against the real SDK (not installed locally). First compile happens at board bring-up.

The legacy CC2640R2 application (`Firmware/f91_kepler_app`) is reference-only after the CC2652R7 decision — its FA35xxxx GATT layer is NOT migrated. The 0xFFFF custom service is implemented fresh in this port.

## What lives here

### `pin_shim/ti/drivers/PIN.h` + `pin_shim/pin_shim.c`

The TI PIN driver was removed in SDK 7.x (GPIO driver replaces it). Three kepler modules touch PIN:
- `input/buttons.c`
- `display/sharp_lcd.c`
- `accel/wrist_raise.c`

The shim translates PIN_* calls to SDK 7.x GPIO_* calls so those modules port unchanged. This shim pattern has been proven twice already: host-test stubs and the ESP32 peripheral rig.

**Integration:** Add `pin_shim/` to the include path ahead of the SDK so `#include <ti/drivers/PIN.h>` resolves to the shim.

### `kepler_gatt_service.h/.c`

The spec 0xFFFF custom service as a BLE5-Stack attribute table with ten characteristics:

| UUID | Name | Size | Type | Notes |
|------|------|------|------|-------|
| 0xFF01 | Notifications | 64 B | WriteNoRsp | Phone pushes notifications to watch |
| 0xFF02 | Time Sync | 4 B | Write | App sets epoch seconds |
| 0xFF03 | Steps | 14 B | Read+Notify+CCCD | Pedometer output with notification enable |
| 0xFF04 | Sleep | 20 B | Read | Sleep stage and duration |
| 0xFF05 | Settings | 16 B | Write | Device configuration |
| 0xFF06 | Weather | 36 B | WriteNoRsp | Weather data from companion |
| 0xFF07 | Locator | 1 B | Write+Notify+CCCD | Find-device trigger with notification enable |
| 0xFF08 | Alarms | 66 B | WriteNoRsp | Alarm list and metadata |
| 0xFF09 | Alarm Trigger | 1 B | WriteNoRsp | Immediate alarm fire command |
| 0xFF0A | Weather Refresh | 1 B | Notify+CCCD | Refresh signal with notification enable |

Write callbacks validate exact lengths and route into `kepler_ble_on_char_write()`, which only copies to a buffer and posts to the kepler event queue — safe in BLE stack task context.

Battery level uses the SDK's standard Battery Service (0x2A19), not reimplemented here.

### `kepler_ble_port.c`

Implements the `ble_port_t` function table from `kepler/ble/ble_manager.h` over GapAdv_* and SDK services.

**Core functionality:**
- Start/stop advertising at a given interval (in 0.625 ms units)
- Notify via the service's setValue
- `KeplerPort_init(advHandle)` — initialize the port with the advertisement set handle
- `KeplerPort_onConnected/onDisconnected` — call from GAP link event handlers
- `KeplerPort_createTask()` — creates a static TI-RTOS task (1024 B stack) that runs `kepler_main_task`

## What ports unchanged

| Area | Why |
|------|-----|
| `storage/flash_store.c` | BLE5-Stack still ships osal_snv — same API |
| Time base | TI-RTOS7 keeps `ti/sysbios/hal/Seconds` |
| I2C, SPI, PWM | Driver APIs carry over from SDK 4.x |
| Clock, Task, Semaphore | Kernel APIs carry over from SDK 4.x |
| All `kepler/` logic modules | SDK-agnostic by design; includes 214 assertions in host tests |
| Host tests | Same test suite runs unchanged |

## Bring-up checklist

1. **Install SDK 7.x.** Download SimpleLink CC13xx/CC26xx SDK 7.x; start from the `basic_ble` example for CC2652R7.

2. **SysConfig.** Expose all DIOs used by the project so GPIO index == DIO number (the pin shim assumes this). Configure I2C, SPI, and PWM instances.

3. **Add to build.** Include `kepler/` and this port directory in the project build; exclude `kepler/test/`.

4. **App init.** Create an advertisement set (`GapAdv_create`), then call `KeplerPort_init(advHandle)`, then `KeplerPort_createTask()`.

5. **Wire GAP handlers.** Connect GAP link established and terminated event handlers to `KeplerPort_onConnected` and `KeplerPort_onDisconnected`.

6. **Verify GATT table.** Check attribute table macros and permissions against the SDK's `simple_gatt_profile.c` reference (the skeleton was written from BLE5-Stack documentation).

7. **Flash path.** Use ROM serial bootloader (BSL) over UART via CP2102 with `cc2538-bsl`. CCFG keeps the BSL backdoor bound to a button. OAD (internal-flash dual image) integration comes after first flash.

# Task 4 — LIS2DW12 Accelerometer: Step Counter, Wrist-Raise & Actigraphy

## Objective

Implement an I2C driver for the ST LIS2DW12 accelerometer. Use the built-in
hardware pedometer for step counting with zero MCU involvement. Implement
wrist-raise interrupt for display mode switching. Implement actigraphy epoch
logging for sleep tracking.

**Hardware decision:** LIS2DW12 selected over LIS2DH12.
Reasons: built-in hardware pedometer (no MCU algorithm needed), lower standby
current (1µA vs 6µA), same LGA-12 2×2mm footprint and I2C interface.
Cost difference: ~$0.40 more per unit — negligible.

---

## LIS2DW12 overview

- I2C address: 0x18 (SDO/SA0 pin low) or 0x19 (SA0 high) — verify on PCB
- 14-bit, 12-bit, 10-bit resolution selectable
- Built-in hardware pedometer engine — counts steps autonomously, no MCU polling
- Built-in activity/inactivity detection, single/double tap, free-fall
- Operating modes: high-performance, low-power (4 sub-modes)
- **Low-power mode 1 with pedometer: ~1µA** — 6× better than LIS2DH12
- Interrupt outputs: INT1 and INT2
- FIFO: up to 32 samples, multiple modes

---

## Key registers

| Register | Address | Description |
|----------|---------|-------------|
| WHO_AM_I | 0x0F | Should return 0x44 — use to verify device present |
| CTRL1 | 0x20 | ODR, low-power mode selection |
| CTRL2 | 0x21 | CS_PU_DIS, I2C_DISABLE, IF_ADD_INC, BDU, reset |
| CTRL3 | 0x22 | ST, PP_OD, LIR, H_LACTIVE, SLP_MODE |
| CTRL4_INT1 | 0x23 | INT1 signal routing |
| CTRL5_INT2 | 0x24 | INT2 signal routing |
| CTRL6 | 0x25 | BW_FILT, FS, FDS, LOW_NOISE |
| STATUS | 0x27 | FIFO threshold, WU, SLP, DRDY |
| OUT_X_L/H | 0x28/29 | X-axis output |
| OUT_Y_L/H | 0x2A/2B | Y-axis output |
| OUT_Z_L/H | 0x2C/2D | Z-axis output |
| FIFO_CTRL | 0x2E | FIFO mode and threshold |
| FIFO_SAMPLES | 0x2F | FIFO sample count + overrun flag |
| TAP_THS_X | 0x30 | Tap X threshold |
| TAP_THS_Y | 0x31 | Tap Y threshold |
| TAP_THS_Z | 0x32 | Tap Z threshold + axes enable |
| INT_DUR | 0x33 | Tap duration, latency, quiet |
| WAKE_UP_THS | 0x34 | Wakeup threshold + sleep/double-tap enable |
| WAKE_UP_DUR | 0x35 | Wakeup/sleep duration |
| FREE_FALL | 0x36 | Free-fall threshold + duration |
| STATUS_DUP | 0x37 | Duplicate status for int clearing |
| WAKE_UP_SRC | 0x38 | Wakeup source (read to clear) |
| TAP_SRC | 0x39 | Tap source (read to clear) |
| SIXD_SRC | 0x3A | 6D/4D source (read to clear) |
| ALL_INT_SRC | 0x3B | All interrupt sources (read to clear all) |
| X_OFS_USR | 0x3C | User offset X |
| Y_OFS_USR | 0x3D | User offset Y |
| Z_OFS_USR | 0x3E | User offset Z |
| CTRL7 | 0x3F | **USR_OFF_ON_OUT, USR_OFF_ON_WU, INTERRUPTS_ENABLE, DRDY_PULSED, INT2_ON_INT1, SLEEP_ON** |
| PEDO_THS_REG | 0x2F (bank B) | Pedometer threshold — requires bank switch via FUNC_CFG_ACCESS |
| STEP_COUNTER_L | 0x3A (bank B) | Step count low byte |
| STEP_COUNTER_H | 0x3B (bank B) | Step count high byte |
| FUNC_CK_GATE | 0x3C (bank B) | Enable pedometer, tilt, step detect |
| FUNC_CFG_ACCESS | 0x1E | Bit 1: switch to embedded function register bank |

**Bank switching:** Pedometer and embedded function registers live in a separate
register bank. Access them by writing 0x02 to FUNC_CFG_ACCESS (0x1E) to enable
bank B, reading/writing, then writing 0x00 to return to bank A.

---

## Initialisation sequence

```c
// 1. Verify WHO_AM_I == 0x44
// 2. Software reset: CTRL2 bit 6 = 1, wait 5ms
// 3. Configure ODR and power mode:
//    CTRL1 = 0x10  (LP mode 1, ODR=12.5Hz — sufficient for pedometer and wrist-raise)
//    Low-power mode 1 = lowest power, 12-bit resolution
// 4. Set full-scale ±2g, BDU enabled, low-noise:
//    CTRL6 = 0x04  (FS=00 (±2g), LOW_NOISE=1)
//    CTRL2 = 0x08  (BDU=1, IF_ADD_INC=1 for burst reads)
// 5. Enable pedometer (bank B):
//    Write 0x02 to FUNC_CFG_ACCESS
//    Write 0x10 to FUNC_CK_GATE  (STEP_D_EN=1)
//    Write 0x00 to FUNC_CFG_ACCESS
// 6. Configure wrist-raise interrupt on INT1 (see below)
// 7. Enable interrupts: CTRL7 = 0x20  (INTERRUPTS_ENABLE=1)
```

---

## Hardware pedometer — reading step count

The LIS2DW12 pedometer increments a 16-bit counter in embedded function
register bank B. The MCU only needs to read it — no algorithm, no FIFO polling.

```c
uint16_t lis2dw12_read_steps(void) {
    uint8_t lo, hi;

    // Switch to embedded function bank B
    i2c_write_reg(I2C_ADDR_LIS2DW12, FUNC_CFG_ACCESS, 0x02);

    lo = i2c_read_reg(I2C_ADDR_LIS2DW12, STEP_COUNTER_L);
    hi = i2c_read_reg(I2C_ADDR_LIS2DW12, STEP_COUNTER_H);

    // Return to bank A
    i2c_write_reg(I2C_ADDR_LIS2DW12, FUNC_CFG_ACCESS, 0x00);

    return (uint16_t)(hi << 8) | lo;
}

// Reset the internal step counter (called at midnight)
void lis2dw12_reset_steps(void) {
    i2c_write_reg(I2C_ADDR_LIS2DW12, FUNC_CFG_ACCESS, 0x02);
    // Reset by writing 0x02 to FUNC_CK_GATE (PEDO_RST_STEP bit)
    i2c_write_reg(I2C_ADDR_LIS2DW12, FUNC_CK_GATE, 0x12);  // PEDO_RST_STEP + STEP_D_EN
    i2c_write_reg(I2C_ADDR_LIS2DW12, FUNC_CK_GATE, 0x10);  // clear reset bit, keep enabled
    i2c_write_reg(I2C_ADDR_LIS2DW12, FUNC_CFG_ACCESS, 0x00);
}
```

**Step read strategy:** Read the counter on every display refresh (once per minute
in ambient mode, or when any screen is active). Do NOT set up a periodic interrupt
just to read steps — the counter accumulates autonomously. Post `EVT_STEP_UPDATE`
after each read if the value has changed.

---

## Wrist-raise configuration (INT1)

Use the LIS2DW12's built-in wake-up detection on INT1. The wake-up function
detects a threshold crossing on any axis — equivalent to detecting the
characteristic Z-axis rise of a wrist raise.

```c
// Wrist-raise: wake-up interrupt on INT1
// WAKE_UP_THS: threshold in units of full-scale/64
// At ±2g FS: 1 LSB = 2g/64 = 31.25mg
// Target ~0.5g threshold: 0.5g / 31.25mg = 16

void lis2dw12_config_wrist_raise(void) {
    // Threshold = 16 (~0.5g), sleep disabled
    i2c_write_reg(I2C_ADDR_LIS2DW12, WAKE_UP_THS, 0x10);

    // Wake-up duration = 2 samples (2 × 1/12.5Hz = 160ms minimum hold)
    // Sleep duration bits = 0 (sleep not used)
    i2c_write_reg(I2C_ADDR_LIS2DW12, WAKE_UP_DUR, 0x02);

    // Route wake-up interrupt to INT1
    // CTRL4_INT1: bit 5 = INT1_WU (wake-up)
    uint8_t ctrl4 = i2c_read_reg(I2C_ADDR_LIS2DW12, CTRL4_INT1);
    ctrl4 |= 0x20;
    i2c_write_reg(I2C_ADDR_LIS2DW12, CTRL4_INT1, ctrl4);

    // Latch interrupt (cleared by reading WAKE_UP_SRC)
    uint8_t ctrl3 = i2c_read_reg(I2C_ADDR_LIS2DW12, CTRL3);
    ctrl3 |= 0x10;  // LIR = 1
    i2c_write_reg(I2C_ADDR_LIS2DW12, CTRL3, ctrl3);

    // Enable interrupts globally
    i2c_write_reg(I2C_ADDR_LIS2DW12, CTRL7, 0x20);
}
```

On INT1 GPIO rising edge (CC2640R2F GPIO ISR):
1. Post `EVT_WRIST_RAISE` to event queue — do nothing else in ISR
2. Event handler reads `WAKE_UP_SRC` register to clear the latch
3. Event handler calls `ui_set_screen(UI_SCREEN_NOTIFICATIONS)`
4. Event handler resets the screen activity timer (`KEPLER_SCREEN_TIMEOUT_MS`)

**Tuning:** Start with WAKE_UP_THS=16 (~0.5g). If false triggers occur while
typing or walking, increase to 20–24. If the gesture isn't detected reliably,
decrease to 12. Keep WAKE_UP_DUR=2 to suppress single-sample spikes.

---

## Actigraphy (sleep tracking)

During the sleep window (default 22:00–08:00), log movement activity into
5-minute epochs using the wake-up interrupt as a movement proxy.

The LIS2DW12's wake-up interrupt fires whenever acceleration exceeds the
wake-up threshold. During sleep, each INT1 event is a movement event.
Count events per 5-minute epoch and classify as restless or still.

```c
#define ACTIGRAPHY_EPOCH_DURATION_SEC   300   // 5 minutes
#define ACTIGRAPHY_EPOCH_THRESHOLD      3     // INT1 events to classify as restless

typedef struct {
    uint32_t date;          // Unix timestamp of sleep start (midnight-aligned)
    uint8_t  epochs[15];    // 120 bits, 1 bit per 5-min epoch, LSB = first epoch
    uint8_t  epoch_count;   // how many epochs recorded
} actigraphy_night_t;

// Called from ISR context indirectly — post to event queue first,
// then increment counter in main task
static volatile uint16_t s_epoch_movement_count = 0;
static uint8_t           s_current_epoch        = 0;
static actigraphy_night_t s_current_night;

// In main task, on EVT_WRIST_RAISE during sleep window:
void actigraphy_on_movement(void) {
    if (actigraphy_in_sleep_window()) {
        s_epoch_movement_count++;
    }
}

// Called by 5-minute timer during sleep window:
void actigraphy_epoch_close(void) {
    if (s_current_epoch >= 120) return;  // safety cap

    bool restless = (s_epoch_movement_count >= ACTIGRAPHY_EPOCH_THRESHOLD);
    if (restless) {
        s_current_night.epochs[s_current_epoch / 8] |=
            (1u << (s_current_epoch % 8));
    }
    s_current_epoch++;
    s_epoch_movement_count = 0;
}

// Called at sleep window end (08:00):
void actigraphy_night_close(void) {
    s_current_night.epoch_count = s_current_epoch;
    flash_store_write_sleep(&s_current_night);

    // Reset for next night
    memset(&s_current_night, 0, sizeof(s_current_night));
    s_current_night.date = Seconds_get();
    s_current_epoch      = 0;
}
```

**Note on dual use of wake-up interrupt:** The same INT1 wake-up interrupt serves
both wrist-raise (daytime) and actigraphy (sleep window). The event handler checks
the current time to decide whether to switch to detail mode or increment the
actigraphy counter — or both. During the sleep window, detail mode switching is
suppressed (wrist movement while sleeping would produce spurious mode switches).

```c
// In kepler_handle_event, EVT_WRIST_RAISE case:
case EVT_WRIST_RAISE:
    lis2dw12_clear_wakeup_src();  // read WAKE_UP_SRC to deassert INT1 latch
    actigraphy_on_movement();     // always log if in sleep window
    if (!actigraphy_in_sleep_window()) {
        // Switch to NOTIFICATIONS screen outside sleep window
        ui_set_screen(UI_SCREEN_NOTIFICATIONS);
        activity_timer_reset();
    }
    break;
```

---

## Daily step reset (midnight)

```c
void pedometer_midnight_reset(void) {
    // Save today's hardware counter total to flash
    uint16_t final_count = lis2dw12_read_steps();
    flash_store_step_day(final_count);

    // Reset hardware counter in LIS2DW12
    lis2dw12_reset_steps();

    // Reset software tracking
    s_step_count = 0;

    event_queue_post(EVT_STEP_UPDATE, 0, NULL);
}
```

---

## lis2dw12.h — public interface

```c
#ifndef LIS2DW12_H
#define LIS2DW12_H

#include <stdint.h>
#include <stdbool.h>

// Initialise device: verify WHO_AM_I, configure ODR/power mode,
// enable pedometer, configure wrist-raise interrupt on INT1.
// Returns false if device not found on I2C bus.
bool     lis2dw12_init(void);

// Read current step count from hardware pedometer register (bank B)
uint16_t lis2dw12_read_steps(void);

// Reset hardware step counter to zero
void     lis2dw12_reset_steps(void);

// Read and clear the wake-up source register (INT1 latch release)
// Returns the WAKE_UP_SRC register value for diagnostics
uint8_t  lis2dw12_clear_wakeup_src(void);

// Read raw acceleration (14-bit signed, ±2g FS, 1 LSB = 0.122mg)
// Used for diagnostic / future features — not needed for pedometer or wrist-raise
void     lis2dw12_read_accel(int16_t *x, int16_t *y, int16_t *z);

#endif // LIS2DW12_H
```

---

## Compile-time guard

All LIS2DW12 code is wrapped with `KEPLER_HAS_LIS2DW12`:

```c
void lis2dw12_init(void) {
#if KEPLER_HAS_LIS2DW12
    // real implementation
#else
    return true;  // stub: always succeeds
#endif
}
```

---

## Acceptance criteria for Task 4

- [ ] WHO_AM_I returns 0x44 on I2C scan at address 0x18 (or 0x19)
- [ ] Pedometer register bank switch (FUNC_CFG_ACCESS) works without I2C errors
- [ ] STEP_COUNTER increments while walking (~100 steps measured ±15%)
- [ ] STEP_COUNTER does NOT increment while sitting still or typing
- [ ] lis2dw12_reset_steps() sets counter to zero (verified by subsequent read)
- [ ] Daily step count resets at midnight, prior day saved to flash
- [ ] 7-day step history in flash survives power cycle
- [ ] INT1 fires on wrist raise (Z-axis threshold crossing)
- [ ] INT1 does NOT fire from gentle arm movement while seated
- [ ] EVT_WRIST_RAISE posted to event queue on INT1 (not in ISR)
- [ ] WAKE_UP_SRC cleared after each INT1 event (INT1 pin returns low)
- [ ] Display switches to NOTIFICATIONS screen on wrist-raise (outside sleep window)
- [ ] Display does NOT switch screen during sleep window
- [ ] Screen returns to MAIN after KEPLER_SCREEN_TIMEOUT_MS ms
- [ ] Actigraphy epochs recorded correctly during sleep window
- [ ] Sleep data written to flash at end of sleep window
- [ ] Stub compiles cleanly when KEPLER_HAS_LIS2DW12 = 0

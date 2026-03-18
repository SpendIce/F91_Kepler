# Task 3 — DRV2605L Haptic Driver & Vibration Patterns

## Objective

Implement an I2C driver for the TI DRV2605L haptic feedback controller.
Define named vibration patterns for all notification types. Build a
stub interface that compiles cleanly when hardware is absent.

---

## DRV2605L overview

- I2C address: 0x5A (fixed)
- Supports both ERM (eccentric rotating mass) and LRA (linear resonant actuator) motors
- Contains a library of 123 pre-defined waveform effects (stored in ROM)
- Waveform sequencer: up to 8 effects played in sequence with configurable delays
- Auto-calibration mode: measures motor parameters on first boot
- Standby current: ~0.6µA (MODE register bit 6 = 1)

**Target motor:** Generic 10mm × 2.5mm coin ERM motor
**Motor connection:** DRV2605L OUTP/OUTN pins (H-bridge output)

---

## Key registers

| Register | Address | Description |
|----------|---------|-------------|
| STATUS   | 0x00    | Read: auto-cal result, device ID |
| MODE     | 0x01    | Operating mode, standby bit |
| REAL_TIME_PLAY | 0x02 | RTP mode amplitude (0–255) |
| LIBRARY  | 0x03    | Waveform library selection (LRA=6, ERM=1–5) |
| WAVESEQ1 | 0x04    | First effect in sequence (0 = end of sequence) |
| WAVESEQ2 | 0x05    | Second effect |
| ... | 0x06–0x0B | Effects 3–8 |
| GO       | 0x0C    | Write 1 to start sequence playback |
| OVERDRIVE | 0x0D   | Overdrive time offset |
| SUSTAIN_P | 0x0E   | Positive sustain time |
| SUSTAIN_N | 0x0F   | Negative sustain time |
| BRAKE    | 0x10    | Brake time offset |
| A_TO_V_CTRL | 0x11 | Audio-to-vibe control |
| A_TO_V_MIN  | 0x12 | Audio-to-vibe min input |
| A_TO_V_MAX  | 0x13 | Audio-to-vibe max input |
| RATED_VOLTAGE | 0x16 | Rated voltage for ERM auto-cal |
| OD_CLAMP    | 0x17 | Overdrive clamp voltage |
| AUTO_CAL_COMP | 0x18 | Auto-cal compensation result |
| AUTO_CAL_BEMF | 0x19 | Auto-cal BEMF result |
| FEEDBACK    | 0x1A | Feedback control (ERM/LRA select, loop) |
| CONTROL1    | 0x1B | Drive time, bidir input |
| CONTROL2    | 0x1C | Sample time, blanking, idiss |
| CONTROL3    | 0x1D | Supply comp, ERM open loop, data format |
| CONTROL4    | 0x1E | Auto-cal time, OTP |
| CONTROL5    | 0x1F | LRA auto-resonance |
| LRA_OPEN_LOOP | 0x20 | LRA open-loop period |
| VBAT        | 0x21 | VBAT measurement (ADC) |
| LRA_RESONANCE | 0x22 | LRA resonance period (measured) |

---

## Initialisation sequence

```c
// 1. Exit standby (MODE register, bit 6 = 0)
// 2. Select ERM library (LIBRARY register = 0x01 for ERM)
// 3. Set MODE to Internal Trigger (0x00)
// 4. Run auto-calibration (MODE = 0x07, write GO=1, wait for GO to clear)
// 5. Store calibration results to NV flash (COMP and BEMF registers)
// 6. Return to Internal Trigger mode (MODE = 0x00)
// 7. Enter standby (MODE bit 6 = 1) until first haptic event
```

Auto-calibration takes ~1.2 seconds. Run it only on first boot or after
motor replacement (store a "calibrated" flag in flash, see Task 7).

---

## DRV2605L waveform library effects (ERM, Library 1)

Useful effects for notification patterns (effect IDs 1–123):

| Effect ID | Description |
|-----------|-------------|
| 1 | Strong Click - 100% |
| 2 | Strong Click - 60% |
| 4 | Sharp Click - 100% |
| 10 | Double Click - 60% |
| 12 | Triple Click - 60% |
| 14 | Soft Fuzz - 60% |
| 47 | Long buzz for programmatic stopping |
| 48 | Smooth Hum 1 (50%) |
| 52 | Short Double Click Strong 1 (80%) |
| 58 | Transition Ramp Down Long Smooth 1 (100% to 0%) |
| 86 | Buzz 1 (100%) |

See DRV2605L datasheet Table 2 for complete list.

---

## haptic_patterns.h — pattern definitions

```c
#ifndef HAPTIC_PATTERNS_H
#define HAPTIC_PATTERNS_H

typedef enum {
    HAPTIC_CALL,        // Incoming call: long continuous pulse, repeating
    HAPTIC_MESSAGE,     // New message: two short pulses
    HAPTIC_ALARM,       // Alarm: three medium pulses
    HAPTIC_CALENDAR,    // Calendar reminder: single medium pulse
    HAPTIC_CONFIRM,     // User action confirmed: single short tap
    HAPTIC_REJECT,      // Error / reject: two short pulses, stronger
    HAPTIC_STEP_GOAL,   // Step goal reached: celebratory pattern
    HAPTIC_COUNT
} haptic_pattern_t;

// Play a pattern. Returns immediately; vibration runs asynchronously.
// If hardware absent (KEPLER_HAS_DRV2605L == 0), this is a no-op.
void haptic_play(haptic_pattern_t pattern);

// Stop any ongoing vibration immediately
void haptic_stop(void);

// Returns true if vibration is currently active
bool haptic_is_active(void);

// Called by timer ISR to advance repeating patterns (e.g. HAPTIC_CALL)
// Do not call from application code
void haptic_tick(void);

#endif
```

---

## Pattern waveform sequences

```c
// Encoded as {effect_id, delay_ms} pairs, terminated by {0, 0}
// DRV2605L handles up to 8 effects per GO command; use timer for repeating

static const uint8_t PATTERN_CALL[]     = {47, 0, 0};         // Long buzz (repeats via timer)
static const uint8_t PATTERN_MESSAGE[]  = {52, 0, 14, 0, 0};  // Double click strong
static const uint8_t PATTERN_ALARM[]    = {12, 0, 12, 0, 12, 0, 0}; // Triple click
static const uint8_t PATTERN_CALENDAR[] = {10, 0, 0};          // Double click 60%
static const uint8_t PATTERN_CONFIRM[]  = {1,  0, 0};          // Strong click 100%
static const uint8_t PATTERN_REJECT[]   = {4,  0, 4,  0, 0};  // Two sharp clicks
static const uint8_t PATTERN_STEP_GOAL[]= {58, 0, 48, 0, 0};  // Ramp down + hum
```

HAPTIC_CALL must repeat until `haptic_stop()` is called (call answered/rejected).
Implement with a 500ms Clock timer that re-triggers the waveform until stopped.

---

## Stub implementation (no hardware)

```c
// In haptic_patterns.c, wrap all I2C calls:
void haptic_play(haptic_pattern_t pattern) {
#if KEPLER_HAS_DRV2605L
    // real implementation
#else
    (void)pattern;  // no-op
#endif
}
```

---

## Acceptance criteria for Task 3

- [ ] DRV2605L detected on I2C bus at 0x5A (read STATUS register returns 0xE0 or similar)
- [ ] Auto-calibration completes without error flag in STATUS register
- [ ] Calibration result stored in flash and reloaded on subsequent boots
- [ ] HAPTIC_CONFIRM plays a single short pulse on BTN press (subjectively perceptible)
- [ ] HAPTIC_MESSAGE plays a distinct double-pulse
- [ ] HAPTIC_CALL repeats continuously until haptic_stop() called
- [ ] haptic_stop() immediately silences motor (no residual spin)
- [ ] Standby mode entered between events (<1µA draw confirmed if measurable)
- [ ] Stub compiles and links cleanly when KEPLER_HAS_DRV2605L = 0

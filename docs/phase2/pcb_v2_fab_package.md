# PCB v2 — Fabrication Readiness Package

> Working document for Fase 2 (Plan Maestro §14). Collects everything needed
> to take the v2 board from "firmware ready" to "Gerbers at JLCPCB":
> decisions, BOM delta, pin map, layout constraints, order specs and the
> remaining human EDA work.
>
> Status: firmware gate **passed** (Fase 0 complete, host-tested, size-gated).
> Layout **does not exist yet** — see §8.

---

## 1. Blocking decision first: MCU package finding (Plan Maestro erratum)

**The Plan Maestro §3.1 package claim is wrong.** Evidence:

1. `Hardware/BOM_main-board.xlsx` lists the v1 MCU as **`CC2640R2FRGZR`** —
   **VQFN-48, 7×7 mm (RGZ)**, not "QFN-32 4×4 mm (RSM)".
2. The v1 firmware (`f91_buttons.c`) maps buttons 2/3 to **IOID_25 / IOID_26**.
   The 4×4 RSM package only exposes DIO_0–DIO_9; DIO_25/26 exist only on the
   7×7 RGZ (DIO_0–DIO_30).

Consequences:

- **The F91W case demonstrably fits a 7×7 VQFN-48** — the v1 board proves it.
- §3.1's conclusion "no pin/package-compatible upgrade exists" inverts:
  the **CC2652R7 (RGZ, 7×7 VQFN-48)** is the same footprint class, with
  **704 KB flash / 144 KB RAM / BLE 5.2** — it dissolves the 128 KB / 20 KB
  knife-edge this project has been engineering around. Future path CC2674R10
  (per the Plan's own note) hangs off the same footprint.

**Decision required before schematic capture:**

| Option | Pros | Cons |
|---|---|---|
| **A. Keep CC2640R2F** | Zero firmware re-port; SDK 4.40 + BLE stack proven in this repo; v1 routing reusable nearly 1:1 | 128 KB/20 KB stays tight (current margins: see §2); no OAD without external SPI flash |
| **B. CC2652R7** | 5.5× flash, 7× RAM; internal-flash OAD trivial; modern SDK (SysConfig, BLE5-Stack); same 7×7 RGZ class | Firmware re-port (TI-RTOS7/new SDK, ICall changes, all driver inits revalidated); pinout similar but NOT 1:1 — full schematic re-check; RF matching/cert re-verify |

Recommendation: decide on the **measured** v2 integrated `.map` margins (§2).
If the final image clears the 110 KB gate with the full feature set enabled,
Option A ships sooner. Option B is the right call if NFC/OAD/app growth keeps
pressing the ceiling.

## 2. Firmware size gate (R1/R2) — measured state

- Baseline (original app + BLE stack, FlashROM_StackLibrary): flash 82.0 KB /
  124 KB, SRAM 11.0 KB / 17 KB app-visible (6.05 KB free).
- RAM mitigations already landed: chunked LCD flush staging (−3.0 KB),
  notification ring 10→5 (−0.5 KB). kepler module static RAM now ≈ 6.4 KB.
- Integrated CCS build (kepler + legacy dual-resident, ssd1306/f91_buttons
  stubbed, measured 2026-06-11):
  - **Flash: 102.8 KB / 124 KB** — soft gate 110 KB **passed**, and ~5-7 KB of
    legacy code is still scheduled for deletion. Flash is no longer the risk.
  - **SRAM: link fails by ~0.6 KB** (system stack unplaced; heap window ~0.4 KB).
    Remaining ~3.2 KB to a runnable image lives in the legacy task layer
    (f91_kepler/f91_clock task stacks + statics ≈ 1.6 KB) plus heap floor.
    Post-migration estimate: BLE heap ≈ 2.5-3 KB — viable for 1 connection,
    but with no slack for feature growth. This is the quantified argument for
    §1 Option B (CC2652R7).
  - Migration prerequisite: replace the legacy FA35xxxx GATT profiles with the
    spec 0xFFFF service and delete the f91_clock/f91_kepler task layer —
    **do this with a LaunchPad attached**; it rewires live BLE paths.
  - [ ] Final post-migration numbers: flash ____ / RAM ____ / heap ____

## 3. v2 schematic change list (Plan Maestro §14, restated against v1)

1. Remove SSD1306 OLED connector + display adapter sub-PCB.
2. Add Sharp LS013B7DH03 FPC connector (10-pin, 0.5 mm pitch).
3. Add BQ51013B inductive charge receiver + coil (4–5 turns, ~20 mm ID, rear copper).
4. Add LiPo 150 mAh pads + DW01A + dual-FET protection (FS8205A or equiv.).
5. Add LIS2DW12 (LGA-12 2×2), I2C addr 0x18 (SA0→GND), INT1 (+INT2 optional) to MCU.
6. Add DRV2605L (WSON-6), I2C addr 0x5A; OUTP/OUTN to ERM pads (motor in bracket recess).
7. Add ST25DV04K (SO-8), I2C 0x53/0x57, GPO to MCU, NFC loop antenna on perimeter.
8. Keep buzzer drive (verify v1 net + transistor from DipTrace schematic — not recoverable from firmware; v1 had no sound code).
9. Programming: 5 pogo pads (VCC, GND, SWDIO/TMS, SWCLK/TCK, RESET) rear face.
10. Retain: CC2640R2F RGZ (pending §1), 24 MHz + 32.768 kHz crystals, RF matching + antenna, 3 button nets, I2C pull-ups.
11. Optional (enables OAD on Option A): SPI NOR flash (e.g. MX25R8035F, 1 MB, ultra-low standby) sharing LCD SPI bus, own CS.
12. Remove any USB/wired charge remnants.

## 4. v2 pin map proposal (resolves kepler_config.h Tier 2/3)

RGZ has 31 DIOs — no contention. Keep every v1 net unchanged; assign new
signals to v1-unused DIOs (verify against DipTrace before capture; v1 unused
set is inferred, not extracted from the binary schematic):

| Signal | DIO | Status |
|---|---|---|
| I2C SDA / SCL | IOID_5 / IOID_6 | v1, keep |
| BTN_1 / BTN_2 / BTN_3 | IOID_15 / IOID_25 / IOID_26 | v1, keep (active-high, pull-down) |
| SPI CLK / MOSI (LCD + opt. flash) | IOID_10 / IOID_9 | new |
| LCD CS (ACTIVE HIGH) / DISP / VCOM | IOID_11 / IOID_12 / IOID_13 | new |
| LIS2DW12 INT1 / INT2 | IOID_7 / IOID_8 | new |
| ST25DV GPO | IOID_14 | new |
| Charge detect (BQ51013B /CHG or coil sense) | IOID_21 | new |
| Buzzer PWM | IOID_22 (or v1 net if one exists) | verify schematic |
| SPI flash CS (optional) | IOID_23 | new |
| SWD TMS/TCK | dedicated pins | pogo pads |

On sign-off: copy into `kepler/kepler_config.h` Tier 1, set the
`KEPLER_HAS_*` flags, delete the launchpad overrides.

## 5. Layout constraints (case-driven, Plan Maestro §5.4/§14)

- Usable area ≈ 30 × 28 mm; board thickness **1.0 mm** (explicit at order);
  edges must not intrude on the gasket seat.
- Rear copper, three concentric zones: NFC loop (1 turn, perimeter ~30 mm) →
  charge coil (4–5 turns, ~20 mm ID) → ground pour + routing. **No ground pour
  under coil or NFC loop.**
- ERM motor and LiPo are off-board (bracket recess / behind PCB).
- RF: keep v1 antenna geometry + matching network and crystal placement
  exactly — do not re-engineer working RF.
- Pogo pads on rear face, clear of coil zones, matching the jig grid.

## 6. JLCPCB order specification

| Parameter | Value |
|---|---|
| Layers | 2 |
| Thickness | **1.0 mm** (not default 1.6) |
| Finish | **ENIG** (pogo pads + fine-pitch FPC) |
| Copper | 1 oz |
| Min track/space | ≥0.127 mm (design ≥0.15 mm) |
| Quantity | 10 |
| Outputs | Gerbers + drill, pick-and-place CSV, BOM CSV (LCSC part numbers where stocked) |

## 7. Pre-order checklist

- [ ] §1 MCU decision recorded
- [ ] §2 integrated .map margins recorded and green
- [ ] Schematic captured in KiCad (v1 DipTrace as visual reference; files are
      binary `.dip/.dch` — netlist must be re-entered, no clean import path)
- [ ] ERC clean; I2C addresses strapped per Plan §5.3
- [ ] Layout DRC clean, incl. coil/NFC keepouts
- [ ] LCD FPC pinout cross-checked against LS013B7DH03 datasheet (CS active HIGH)
- [ ] Charging coil coupling prototyped on bench before committing case fit
- [ ] 3D check: PCB + LiPo + motor + bracket inside case scan/measurements
- [ ] Gerbers reviewed in a separate viewer before upload

## 8. What remains human work

The layout itself. The v1 design exists only as DipTrace binaries
(`Hardware/PCB/*.dip/.dch`) — Plan Maestro specifies KiCad for v2, so the
schematic is a re-capture, not a conversion. Everything upstream of EDA
(firmware, drivers, pin plan, constraints, order spec) is in this repo and
done; everything downstream of this document is mouse-work in KiCad plus the
bench prototyping in §7.

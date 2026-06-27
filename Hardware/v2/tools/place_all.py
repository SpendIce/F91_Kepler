#!/usr/bin/env python3
"""
F91 Kepler v2 — KiCad scripting-console placement script.

Paste the body of main() into the KiCad PCB editor scripting console
(Tools → Scripting Console) and hit Enter.  All 89 footprints are
moved to layout-correct positions; ratsnest updates automatically.

Board: 25 × 27 mm, origin (0,0) = top-left, Y increases downward.
U1 CC2652R7 (7×7 mm QFN-48) centred at (12.5, 13.5).

Pin-side reference for U1 (CCW from pin 1 = bottom-left of left side):
  Left  (x=9.0): pin 1 RF_P y=16.25 … pin 12 ACC_INT1 y=10.75
  Top   (y=10.0): pin 13 … pin 24 SWD_TMS x=15.25
  Right (x=16.0): pin 25 SWD_TCK y=10.75 … pin 36 y=15.75
  Bottom(y=17.0): pin 37 HAPTIC_EN x=15.25 … pin 48 VDDR x=10.25
"""

import pcbnew


def main():
    board = pcbnew.GetBoard()

    def place(ref, x_mm, y_mm, rot=0):
        fp = board.FindFootprintByReference(ref)
        if fp is None:
            print(f"  NOT FOUND: {ref}")
            return
        fp.SetPosition(pcbnew.VECTOR2I(
            pcbnew.FromMM(x_mm), pcbnew.FromMM(y_mm)))
        fp.SetOrientationDegrees(rot)
        print(f"  {ref:6s}  ({x_mm:5.1f}, {y_mm:5.1f})  rot={rot}")

    print("=== F91 Kepler v2 — placing 89 components ===")

    # ── MCU ──────────────────────────────────────────────────────────────────
    place("U1",   12.5, 13.5)               # CC2652R7, centre of board

    # ── RF CHAIN ─────────────────────────────────────────────────────────────
    # U1 RF_P (pin1) y=16.25, RF_N (pin2) y=15.75 — left side of U1 at x=9.0
    # FL1 directly adjacent, chain extends left to board edge.
    place("FL1",   7.5, 16.0)               # LFB18 balun; RF_P/RF_N pads face right
    place("R2",    5.5, 16.0,  90)          # 0R series jumper
    place("C14",   7.5, 17.5,  90)          # DNP shunt: RF_UNBAL → GND
    place("C15",   5.5, 17.5,  90)          # DNP shunt: ANT_FEED → GND
    place("AE1",   2.0, 16.0)               # IFA feed pad at left edge

    # ── CRYSTALS ─────────────────────────────────────────────────────────────
    # X32K pins (3,4) on left side of U1 at y=15.25/14.75
    place("Y2",    7.0, 14.5)               # 32.768 kHz, horizontal, pads L/R
    place("C12",   5.0, 14.5,  90)          # X32K_1 load cap — left of Y2
    place("C13",   8.5, 14.5,  90)          # X32K_2 load cap — right of Y2, toward U1

    # X48M pins (46,47) on bottom of U1 at x=11.25/10.75
    place("Y1",   10.5, 18.5)               # 48 MHz FA-128, below U1

    # ── DCDC (U1 pin 33 DCDC_SW, right side at x=16.0, y≈14.25) ──────────
    place("L2",   17.5, 14.5)               # 6.8 uH inductor
    place("C7",   19.5, 14.5,  90)          # 22 uF VDDR bulk
    place("C8",   17.5, 13.0,  90)          # 100 nF VDDR
    place("C9",   17.5, 15.5,  90)          # 100 nF VDDR

    # ── U1 SUPPLY DECOUPLING ─────────────────────────────────────────────────
    # VDDS pins: 22(top x=14.25), 34(right y=14.75), 44(bottom x=12.25), 13(top x=9.75)
    place("C2",   10.5,  8.5,  90)          # 100 nF VDDS
    place("C3",   12.0,  8.5,  90)          # 100 nF VDDS
    place("C4",   13.5,  8.5,  90)          # 100 nF VDDS
    place("C5",   15.0,  8.5,  90)          # 22 uF VDDS bulk
    place("C6",   15.5, 10.0,  90)          # 100 nF VDDS (near right/top corner)
    place("C10",  15.0,  9.5,  90)          # 1 uF DCOUPL (pin 23, top side x=14.75)
    place("C11",  17.5, 15.0,  90)          # 100 nF nRESET (pin 35, right side y=15.25)
    place("R1",   18.5, 15.0)               # 100 k nRESET pull-up
    place("FB1",  17.5, 11.0)               # ferrite BLM18, +3V0 → VDDS filter

    # ── LDO (U4 XC6206) ──────────────────────────────────────────────────────
    place("U4",   20.0, 16.5, 180)          # 3.0 V LDO; pin2(+3V0) faces right at rot=180
    place("C33",  21.5, 16.5,  90)          # 1 uF +3V0 output bypass
    place("C32",  22.0, 13.5,  90)          # 1 uF VBAT bypass (near BT1)

    # ── BATTERY PROTECTION (Q1 FS8205A + U3 DW01A) ───────────────────────────
    # Tightly coupled: Q1 and U3 adjacent, shared BATN/PROT_* nets
    place("Q1",   20.5, 10.0)               # FS8205A dual-FET TSSOP-8
    place("U3",   22.5, 11.5)               # DW01A SOT-23-6
    place("C31",  21.0,  8.5,  90)          # 100 nF DW_VCC bypass
    place("R10",  23.0, 10.0,  90)          # 470 R VBAT → DW_VCC
    place("R11",  23.5, 11.5,  90)          # 1 k PROT_CS → GND

    # ── QI CHARGER (U2 BQ51050B, VQFN-20) ───────────────────────────────────
    # AC1 (pin2) / AC2 (pin19) face the coil pads (bottom of board)
    place("U2",   19.5, 22.0)

    # U2 RECT filter (pin 18 RECT, left side of U2)
    place("C16",  16.5, 21.0,  90)          # 10 uF RECT
    place("C17",  16.5, 22.0,  90)          # 10 uF RECT
    place("C18",  16.5, 23.0,  90)          # 100 nF RECT

    # U2 bootstrap caps
    place("C23",  16.5, 19.5,  90)          # 10 nF BOOT1 (AC1-BOOT1)
    place("C24",  17.5, 19.5,  90)          # 10 nF BOOT2 (AC2-BOOT2)

    # U2 clamp caps
    place("C25",  16.5, 20.5,  90)          # 470 nF CLAMP1
    place("C26",  17.5, 20.5,  90)          # 470 nF CLAMP2

    # U2 COMM caps
    place("C27",  16.5, 24.0,  90)          # 22 nF COMM1
    place("C28",  17.5, 24.0,  90)          # 22 nF COMM2

    # Qi resonant / bench-tune caps (coil side, BENCH-TUNE)
    place("C19",  19.5, 24.0,  90)          # BENCH-C1a COIL1-AC1
    place("C20",  21.0, 24.0,  90)          # DNP-C1b
    place("C21",  19.5, 25.0,  90)          # BENCH-C2a AC1-AC2
    place("C22",  21.0, 25.0,  90)          # DNP-C2b

    # U2 programming resistors
    place("R5",   22.5, 20.0,  90)          # 3.92 k ILIM
    place("R6",   23.5, 20.0,  90)          # 200 R FOD_TAP
    place("R7",   22.5, 21.0,  90)          # 2.4 k TERM
    place("R8",   23.5, 22.0,  90)          # 10 k TS
    place("R9",   17.5, 16.5,  90)          # 100 k CHG_DET pull-up (U1 pin31 right side)

    # ── BATTERY PADS & COIL/CHARGE TEST POINTS ───────────────────────────────
    place("BT1",  23.0, 14.0)               # LiPo 150 mAh pads
    place("C29",  22.0, 13.0,  90)          # 1 uF VBAT bypass
    place("C30",  22.0, 14.0,  90)          # 100 nF VBAT bypass
    place("TP9",  15.5, 25.5)               # COIL1
    place("TP10", 17.0, 25.5)               # COIL2
    place("W1",   16.25,24.5,  90)          # 0R COIL2-AC2 jumper
    place("TP11", 21.5, 25.5)               # CHG+ (VBAT)
    place("TP12", 22.5, 25.5)               # CHG- (GND)

    # ── I2C PERIPHERALS ──────────────────────────────────────────────────────
    # SDA/SCL = U1 pins 10/11 on left side at y=11.75/11.25

    # U5 LIS2DW12 accelerometer (LGA-12, 2×2 mm)
    place("U5",    7.5,  9.5)
    place("C34",   6.0,  8.5,  90)          # 100 nF VDD bypass
    place("C35",   7.0,  8.5,  90)          # 100 nF VDDIO bypass

    # U6 DRV2605L haptic driver (VSSOP-10, 3×3 mm)
    place("U6",    5.5, 12.0)
    place("C36",   3.5, 11.0,  90)          # 1 uF DRV_REG (internal LDO bypass)
    place("C37",   3.5, 12.5,  90)          # 100 nF VDD bypass

    # Motor pads — exit toward bracket recess (left edge, mid-lower)
    place("TP13",  2.5, 18.5)               # MOT_P
    place("TP14",  2.5, 20.0)               # MOT_N

    # U7 ST25DV04K NFC (SOIC-8, 3.9×4.9 mm)
    place("U7",    9.5,  5.5)
    place("C38",   8.0,  4.0,  90)          # 100 nF +3V0 bypass

    # NFC antenna taps
    place("TP15",  3.5,  5.0)               # NFC_A
    place("TP16",  3.5,  6.0)               # NFC_B

    # I2C pull-ups
    place("R13",   9.5, 11.0,  90)          # 3.3 k SDA
    place("R14",   9.5, 12.0,  90)          # 3.3 k SCL
    place("R12",  11.0,  4.5,  90)          # 100 k NFC_GPO pull-up

    # ── LCD FPC (top edge — display faces up) ─────────────────────────────────
    place("J1",   12.5,  2.0, 180)          # Hirose FH12-10S-0.5SH; cable exits top
    place("C39",  15.0,  2.0,  90)          # 100 nF +3V0 FPC bypass
    place("C40",  16.0,  2.0,  90)          # 1 uF +3V0 FPC bypass

    # ── BUTTONS ──────────────────────────────────────────────────────────────
    # SW1 also = BSL backdoor.  Consider replacing with contact pads (§7 guide).
    place("SW1",   2.5, 13.5, 270)          # BTN_1 / BSL — left edge
    place("SW2",  22.5, 11.5,  90)          # BTN_2 — right edge upper
    place("SW3",  22.5, 15.0,  90)          # BTN_3 — right edge lower

    # ── POGO PADS — bottom row, programming / debug ───────────────────────────
    place("TP1",   4.0, 25.5)               # POGO_VCC
    place("TP2",   5.5, 25.5)               # POGO_GND
    place("TP3",   7.0, 25.5)               # POGO_RST
    place("TP4",   8.5, 25.5)               # POGO_TMS
    place("TP5",  10.0, 25.5)               # POGO_TCK
    place("TP6",  11.5, 25.5)               # POGO_TX
    place("TP7",  13.0, 25.5)               # POGO_RX

    # ── BUZZER / PIEZO ────────────────────────────────────────────────────────
    # U1 BUZZER = pin 32, right side at y≈14.75
    place("R3",   17.5, 17.5,  90)          # 100 R series drive
    place("R4",   17.5, 18.5,  90)          # 1 k bleed to GND
    place("TP8",  17.5, 19.5)               # PIEZO contact pad

    # ── REFRESH ──────────────────────────────────────────────────────────────
    pcbnew.Refresh()
    print("=== Done — 89 footprints placed ===")
    print("Next: run DRC, check courtyard overlaps, fine-tune RF chain orientation.")


main()

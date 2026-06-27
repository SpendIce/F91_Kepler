"""
fix5.py  —  second-pass short elimination

ROOT CAUSES fixed here:
  1. Y2 (32kHz XTAL) right pad at x=10.75 inside U1 thermal-GND pad (x=9.7-15.3)
     → R13 also at x=9.0 inside U1 left-pad copper zone (pads at x=9.0625)
  2. C19/C20/C21/C22 (Qi AC caps) placed on Y1 crystal zone (10.5,18.5) by fix4.py
  3. Qi grid right column (x=17.5) conflicts with U2 AC1 pad at x=17.3
  4. BT1 connector: ±2mm pad pitch → pad1 VBAT at y=24.5 shorts TP3 at y=25.5
  5. Q1 right pads at x=22.65 overlap SW2 pads at y≈9.5
  6. U4 pad2 at (20.94,12.55) overlaps U3 pad2 at (21.36,12.5)
  7. U4 pad1 GND at (20.94,14.45) overlaps SW3 BTN_3 pad at (21.7,15.15)
  8. R11 at (23.5,11.5) inside U3 right-pad copper zone
  9. C9 pad at (19.0,12.98) overlaps U4 VBAT pad at (19.06,13.5)
 10. Pogo pad row at 1.5mm pitch with ~1.5mm pads → copper gap = 0mm

Run order: fix2.py → fix3.py → fix4.py → fix5.py
Console: exec(open('/home/spendice/Documents/F91_Kepler/Hardware/v2/tools/fix5.py').read())
"""

import pcbnew
board = pcbnew.GetBoard()

def place(ref, x, y, rot=0):
    fp = board.FindFootprintByReference(ref)
    if fp:
        fp.SetPosition(pcbnew.VECTOR2I(pcbnew.FromMM(x), pcbnew.FromMM(y)))
        fp.SetOrientationDegrees(rot)
        print(f"  {ref:6s}  ({x:.3f}, {y:.3f})  rot={rot}")
    else:
        print(f"  NOT FOUND: {ref}")

print("=== fix5.py — second-pass short elimination ===")

# ── 1. Y2 (32 kHz XTAL) ──────────────────────────────────────────────────────
# U1 thermal-GND pad: x=9.7–15.3, y=10.7–16.3.
# Y2 at (9.5,14.5): right pad at x=10.75 → INSIDE thermal pad → short.
# Move Y2 left so right pad at x=8.25, clear of thermal edge (9.7-0.5=9.2 ✓).
# Left pad at (5.75,14.5) — left edge 5.25, board left at x=0 ✓.
place("Y2",  7.0, 14.5, 0)

# C12 [X32K_1 load cap] — left of Y2, same row.
# Y2 left pad left edge = 5.25. C12 at x=4.5: right edge = 4.825. Gap=0.425mm ✓.
# col x=4.5: TP1(25.5) → C12(14.5) diff=11mm ✓.
place("C12",  4.5, 14.5, 90)

# C13 [X32K_2 load cap] — above and right of Y2 to avoid Y2 right pad at (8.25,14.5).
# C13 at (8.0,12.0): pad2 at y=13.02. Y2 right pad at y=14.5-0.5=14.0 (top edge).
# Gap in Y: 14.0-13.02-0.325=0.655mm ✓.  ΔX=0.25, ΔY=2.5 → diagonal ✓.
# col x=8.0: C13(12.0) alone ✓.
place("C13",  8.0, 12.0, 90)

# ── 2. R13 (I2C SDA pull-up) ──────────────────────────────────────────────────
# Was at (9.0,11.5): x=9.0 inside U1 left-pad copper (pads at x=9.0625, extend to 8.61).
# Move to x=6.5 — U1 pad left copper edge at 8.6125. Gap=8.6125-6.5-0.35=1.76mm ✓.
# col x=6.5: U7 body ends ~y=8.5. R13(11.5) pad top=10.99. Gap=2.49mm ✓.
place("R13",  6.5, 11.5, 90)

# ── 3. C19-C22 (Qi AC caps) — off Y1 crystal ─────────────────────────────────
# Y1 at (10.5,18.5) pads at y=18.05 and y=18.95.
# Caps need y ≥ 21.0 so pad1 at y=19.98; gap from Y1 lower pad = 19.98-18.95=1.03mm.
# ΔX check: C19 at x=10.0 vs Y1 pad at x=9.5 → ΔX=0.5, ΔY=1.03 → dist=1.14mm,
#   copper gap = 1.14-0.325-0.275=0.54mm ✓.
# ROW y=21.0: C19(10.0) C20(12.5)
# ROW y=24.1: C21(10.0) C22(12.5)
# col x=10.0: C19(21.0), C21(24.1) → 3.1mm ✓; C29 moves to x=9.0 → clear ✓.
# col x=12.5: J1(5.0), C20(21.0), C22(24.1) → 16mm, 3.1mm ✓.
place("C19", 10.0, 21.0, 90)
place("C20", 12.5, 21.0, 90)
place("C21", 10.0, 24.1, 90)
place("C22", 12.5, 24.1, 90)

# C29 shifted from x=10.0 to x=9.0 to free col x=10.0 for C19/C21.
# col x=9.0: C29(22.5) alone ✓.
place("C29",  9.0, 22.5, 90)

# ── 4. Qi bypass grid — shift left column from x=17.5 to x=13.0 ──────────────
# U2 AC1 pad at (17.3,21.5) — old C17 at (17.5,21.1) pad overlapping U2.
# New grid: x=13.0 / 14.5 / 16.0  (was 14.5 / 16.0 / 17.5).
# Row pitch 3.1mm as before; all same-row pairs → safe.
# col x=13.0: C23(18.0), C25(21.1), C27(24.2) → 3.1mm diffs ✓.
# col x=14.5: C24(18.0), C26(21.1), C28(24.2) → 3.1mm diffs ✓.
# col x=16.0: C16(18.0), C17(21.1), C18(24.2) → 3.1mm diffs ✓.
# C27 pad bottom = 24.2+1.45=25.65; edge clearance 27-25.65=1.35mm ✓.
place("C23", 13.0, 18.0, 90)   # BOOT1
place("C24", 14.5, 18.0, 90)   # BOOT2
place("C16", 16.0, 18.0, 90)   # RECT 10uF

place("C25", 13.0, 21.1, 90)   # CLAMP1
place("C26", 14.5, 21.1, 90)   # CLAMP2
place("C17", 16.0, 21.1, 90)   # RECT 10uF

place("C27", 13.0, 24.2, 90)   # COMM1
place("C28", 14.5, 24.2, 90)   # COMM2
place("C18", 16.0, 24.2, 90)   # RECT 100nF

# C32 [VBAT bypass] — was at (13.0,22.5); col x=13.0 now has Qi grid → conflict.
# Move to x=14.0: col x=14.0: C32(22.5) alone ✓.
# C32 vs C28(14.5,24.2): ΔX=0.5, ΔY=1.7 → diagonal ✓.
place("C32", 14.0, 22.5, 90)

# ── 5. BT1 (LiPo connector, ±2mm pad pitch) ──────────────────────────────────
# Was (7.5,22.5): pad1 VBAT at y=24.5, only 1mm from TP3 at y=25.5 → short.
# Move to (5.0,20.5): pad1 at y=22.5, pad2 at y=18.5. Both clear of pogo row ✓.
# col x=5.0: C37(12.0) → pad2(18.5) diff=6.5mm ✓.
place("BT1",  5.0, 20.5, 90)

# ── 6. C9 — clear of U4 VBAT pad ─────────────────────────────────────────────
# Was (19.0,12.5): pad2 at y=13.52, U4 VBAT at y=13.5 → 0mm gap → short.
# Move to (19.0,11.0): pad2 at y=12.02. U4 VBAT pad at y=13.5: gap=1.48mm ✓.
# col x=19.0: C9(11.0) alone (R11 moves to x=20.0) ✓.
place("C9",  19.0, 11.0, 90)

# ── 7. U4 (XC6206 LDO) — separate from U3 ────────────────────────────────────
# Was (20.0,13.5): pad2 +3V0 at (20.94,12.55) vs U3 pad2 at (21.36,12.5) → 0.425mm X gap → short.
# Move to (19.5,13.5): pad2 at (20.44,12.55). U3 pad2 at (21.36,12.5): X gap=0.92mm ✓.
# pad3 VBAT at (18.56,13.5) vs C8(17.5,12.5) pad2(y=13.52): ΔX=1.06 → gap=0.41mm ✓.
place("U4",  19.5, 13.5, 180)

# ── 8. SW3 — BTN_3 pad clears U4 GND ─────────────────────────────────────────
# Was (22.5,17.2): BTN_3 pad at (21.7,15.15). U4 GND pad at (20.44,14.45).
#   U4 GND pad right edge = 20.77. SW3 BTN_3 pad copper extends to x=20.8 → overlap.
# Move to (22.5,18.5): BTN_3 pad at (21.7,16.45). U4 GND at (20.44,14.45).
#   Distance=2.37mm, gap=2.37-0.77=1.6mm ✓.
# SW3 +3V0 pad at (23.3,20.55) — edge check: x=23.3 vs right arc (23,2) r=2;
#   at y=20.55 board right is straight (x=25). Gap=25-23.3-pad_half=1.35mm ✓.
# U3 bottom pads at y≈13.6. BTN_3 top at 16.45-pad_half=15.65. Gap=2.05mm ✓.
place("SW3", 22.5, 18.5, 90)

# ── 9. Q1 (FS8205A dual-FET TSSOP-8) ─────────────────────────────────────────
# Was (20.5,10.0): right pads at x=22.65, y=9.025-10.975. SW2 pads at y=9.55.
#   Q1 pad at (22.65,9.025) vs SW2 pad at (23.3,9.55) → ΔX=0.65, ΔY=0.525 → short.
# Move to (17.0,11.5): right pads at x=19.15.
#   vs SW2 pad at (23.3/21.7,9.55): ΔX≥2.55mm ✓.
#   vs C8(17.5,12.5) pad at y=11.48: Q1 bottom pin at y=12.475. gap=0.47mm ✓.
#   vs C31(19.5,7.5) pad at y=8.52: Q1 right top at (19.15,10.525). dist=2.04mm ✓.
place("Q1",  17.0, 11.5, 0)

# ── 10. R11 (PROT_CS 1k resistor) ────────────────────────────────────────────
# Was (23.5,11.5,90): pads at (23.5,10.99) and (23.5,12.01), inside U3 right-pad
#   copper zone. U3 right pads at x=23.6375, copper extends to x≈23.0 → overlap.
# Move to (20.0,10.5,90): pads at (20.0,10.0) and (20.0,11.0).
#   col x=20.0: R4(17.5), R11(10.5) → diff=7.0mm ✓.
#   vs Q1 right pin at (19.15,10.525): ΔX=0.85, ΔY=0.025 → dist=0.85mm,
#   gap=0.85-0.2-0.2=0.45mm ✓.
place("R11", 20.0, 10.5, 90)

# ── 11. C7 (VDDR bulk cap, 0805) ─────────────────────────────────────────────
# Was (18.5,14.0): pad2 VDDR at y=14.775. C11 at (18.0,15.5) pad at y=15.02.
#   Gap ΔX=0.5, ΔY=0.245 → overlap at same nets → short.
# Move to (17.0,14.0): ΔX from C11=1.0mm → diagonal → safe ✓.
# col x=17.0: C7(14.0) alone (R9 at 16.5) ✓.
place("C7",  17.0, 14.0, 90)

# ── 12. C14 (RF cap) — clear of R2 ──────────────────────────────────────────
# Was (6.0,17.0): pad at y≈16.52 overlaps R2 pad at (5.5,16.51) — ΔX=0.5 → short.
# Move to (6.0,20.0): pads at y=18.98 and y=21.02. R2 at y=16.0 — clear ✓.
# col x=6.0: C34(10.5), C14(20.0) → diff=9.5mm ✓.
place("C14",  6.0, 20.0, 90)

# ── 13. TP8 (PIEZO test point) — away from U2 ────────────────────────────────
# Was (19.5,19.5): U2 RECT pad at (19.5,20.3) → 0.8mm gap → short.
# Move to (15.5,22.5): clear of U2 ✓.
# vs C17(16.0,21.1): ΔX=0.5, ΔY=1.4 → dist=1.49mm; TP8 r=0.75, gap=0.41mm ✓.
# vs C32(14.0,22.5): ΔX=1.5, ΔY=0mm → TP8 left edge=14.75, C32 right=14.325 → gap=0.425mm ✓.
place("TP8", 15.5, 22.5)

# ── 14. Pogo pad row — 2.5mm pitch ───────────────────────────────────────────
# Was 1.5mm pitch with ~1.5mm diameter pads → gap=0mm → adjacent pads short.
# New pitch 2.5mm: gap=2.5-1.5=1.0mm ✓.
# TP1(4.5): arc (2,25) r=2 → dist=sqrt(6.5)-2=0.55mm ✓.
# TP7(19.5): arc (23,25) r=2 → dist=sqrt(12.5)-2=1.54mm ✓.
place("TP1",  4.5, 25.5)
place("TP2",  7.0, 25.5)
place("TP3",  9.5, 25.5)
place("TP4", 12.0, 25.5)
place("TP5", 14.5, 25.5)
place("TP6", 17.0, 25.5)
place("TP7", 19.5, 25.5)

# TP11/TP12 — off bottom-right arc zone, not in pogo row.
# TP11(6.5,23.0): BT1 pad at (5.0,22.5) → ΔX=1.5, ΔY=0.5 → dist=1.58mm,
#   gap=1.58-0.75-0.325=0.505mm ✓.
# TP12(8.0,23.5): vs C29(9.0,22.5) ΔX=1.0, ΔY=1.0 → dist=1.41mm, gap=0.34mm ✓.
place("TP11",  6.5, 23.0)
place("TP12",  8.0, 23.5)

pcbnew.Refresh()
print("=== fix5.py done ===")
print("Expected: 0 shorting_items.  Run DRC to confirm.")
print("Residual non-blocking: courtyards_overlap, solder_mask_bridge (set Mask Exp=0.05mm).")
print("RUN ORDER: fix2 → fix3 → fix4 → fix5")

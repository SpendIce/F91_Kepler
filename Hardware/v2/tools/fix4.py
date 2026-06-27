"""
fix4.py  —  eliminate all shorting_items and clearance violations

ROOT CAUSE of 150+ shorts:
  rot=90 caps in COLUMNS (same X, <3.1 mm Y pitch) → facing pads overlap → short.

RULES applied:
  Same column (same X), rot=90: need ≥ 3.1 mm Y pitch  (gap = 3.1-2.895 = 0.21 mm ✓)
  Same row   (same Y), rot=90:  safe at any X pitch
  Diagonal   (diff X AND diff Y): always safe

QI CLUSTER — 3×3 grid:
  Cols at x=14.5/16.0/17.5  (col pitch 1.5 mm, pad X gap = 0.85 mm ✓)
  Rows at y=18.0/21.1/24.2  (row pitch 3.1 mm, pad Y gap = 0.21 mm ✓)
  Same-row caps (same Y) = ROW layout → always safe.

VBAT bypass at y=22.5 (not 21.0) so resonant caps can use x=9.5-12.5
without column conflicts with VBAT caps.

Run order: fix2.py → fix3.py → fix4.py
Console: exec(open('/home/spendice/Documents/F91_Kepler/Hardware/v2/tools/fix4.py').read())
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


print("=== fix4.py — short-circuit / clearance redesign ===")

# ── BT1 — isolated bottom-left ────────────────────────────────────────────
# rot=90 → pad1(VBAT) at y=20.5, pad2(BATN) at y=24.5; clear of all ICs.
place("BT1",  7.5, 22.5,  90)

# ── VBAT bypass — ROW at y=22.5 ──────────────────────────────────────────
# y=22.5 keeps these away from resonant caps (y=17.5-19.5) → no col conflicts.
# col x=10.0: C29(22.5); TP4(25.5) diff = 3.0 mm ✓
# col x=11.5: C30(22.5); C21(17.5)  diff = 5.0 mm ✓
# col x=13.0: C32(22.5) alone ✓
place("C29", 10.0, 22.5,  90)
place("C30", 11.5, 22.5,  90)
place("C32", 13.0, 22.5,  90)

# ── U1 VDDS decoupling — ROW at y=8.5 ───────────────────────────────────
place("C2",  10.5,  8.5,  90)
place("C3",  12.0,  8.5,  90)
place("C4",  13.5,  8.5,  90)
place("C5",  15.0,  8.5,  90)
place("C10", 16.5,  8.5,  90)
# C6: col x=17.5 only with C6(9.0); C16 at y=18.0 → diff = 9.0 mm ✓
place("C6",  17.5,  9.0,  90)

# ── DCDC bypass — ROW at y=12.5 ──────────────────────────────────────────
# col x=17.5: C6(9.0) → C8(12.5) = 3.5 mm ✓
place("C8",  17.5, 12.5,  90)
place("C9",  19.0, 12.5,  90)
# C7 bulk: col x=18.5: C7(14.0), R3(17.5), W1(24.5) → diffs 3.5/7 mm ✓
place("C7",  18.5, 14.0,  90)

# ── nRESET — ROW at y=15.5 ───────────────────────────────────────────────
place("C11", 18.0, 15.5,  90)
place("R1",  19.5, 15.5,  90)

# ── LDO (U4 XC6206) ──────────────────────────────────────────────────────
# rot=180 → pin2(+3V0) at x≈20.94, y≈12.6; clears SW3 BTN_3 pad at y≈15.15 ✓
place("U4",  20.0, 13.5, 180)
# C33 bypass: col x=21.5: C33(9.0), R5(22.5) → diff = 13.5 mm ✓
# At y=9.0 the pad bottom = 10.45; U3 left pads at y≈12.0 → gap 1.55 mm ✓
place("C33", 21.5,  9.0,  90)

# ── Battery protection ────────────────────────────────────────────────────
# col x=19.5: C31(7.5), R1(15.5), R7(24.5) → diffs 8/9 mm ✓
place("C31", 19.5,  7.5,  90)

# ── SW3 right-edge button ────────────────────────────────────────────────
# BTN_3 pad top = 17.2-2.8 = 14.4; U3 bottom ≈ 13.8 → gap 0.6 mm ✓
# +3V0  pad bot = 17.2+2.8 = 20.0; R5 pad top ≈ 21.05     → gap 1.1 mm ✓
place("SW3", 22.5, 17.2,  90)

# ── RF chain ─────────────────────────────────────────────────────────────
# Y2 moved right to avoid R2(5.5,16.0) pad clash.
place("Y2",  9.5, 14.5)
# C12: col x=7.5: C35(10.5), C12(14.5) → diff = 4.0 mm ✓
# C13 at x=12.0 (not 11.0) — avoids Y2 right pad zone near x=10.5
#   col x=12.0: C3(8.5), C13(14.5) → diff = 6.0 mm ✓
place("C12",  7.5, 14.5,  90)
place("C13", 12.0, 14.5,  90)
# C14 at x=6.0: col x=6.0: C34(10.5), C14(17.0) → diff = 6.5 mm ✓
place("C14",  6.0, 17.0,  90)
place("C15",  4.0, 16.5,  90)

# ── Qi charger bypass — 3×3 grid ─────────────────────────────────────────
# Columns at x=14.5, 16.0, 17.5 with 3.1 mm Y pitch (gap 0.21 mm ✓).
# Same-row Y=18.0/21.1/24.2: all three cols share each Y → safe ROW layout.
# col x=17.5: C6(9.0) → C16(18.0) = 9.0 mm ✓
# C28 pad bottom = 24.2+1.45 = 25.65; board edge 27 → 1.35 mm ✓
place("C23", 14.5, 18.0,  90)   # BOOT1
place("C24", 16.0, 18.0,  90)   # BOOT2
place("C16", 17.5, 18.0,  90)   # RECT 10uF

place("C25", 14.5, 21.1,  90)   # CLAMP1
place("C26", 16.0, 21.1,  90)   # CLAMP2
place("C17", 17.5, 21.1,  90)   # RECT 10uF

place("C27", 14.5, 24.2,  90)   # COMM1
place("C28", 16.0, 24.2,  90)   # COMM2
place("C18", 17.5, 24.2,  90)   # RECT 100nF

# ── Resonant / tune caps — centre strip ──────────────────────────────────
# Placed at x=9.5-12.5, y=17.5-19.5 — well away from SW3 and Qi grid.
# VBAT row at y=22.5 means these X columns have no cap within 3.1 mm in Y.
#
# col x=9.5 : Y2(14.5,0°), C19(18.0,90°) → diff = 3.5 mm ✓
# col x=10.5: C2(8.5),      C20(19.5)     → diff = 11.0 mm ✓
# col x=11.5: R14(11.5),    C21(17.5),  C30(22.5) → diffs 6/5 mm ✓
# col x=12.5: J1(5.0),      C22(19.5)     → diff = 14.5 mm ✓
#
# Cross-col X gaps (0.35 mm at adjacent X=0.5 mm offset, 0.85 mm at 1.5 mm) ✓
# C21(11.5,17.5) also joins ROW y=17.5 with R3(18.5) and R4(20.0) — safe ✓
place("C19",  9.5, 18.0,  90)
place("C21", 11.5, 17.5,  90)
place("C20", 10.5, 19.5,  90)
place("C22", 12.5, 19.5,  90)

# ── Qi programming resistors ──────────────────────────────────────────────
# ROW y=22.5: R5(21.5), R6(23.0) — X diff 1.5 mm ✓
# ROW y=24.5: R7(19.5), R8(21.0) — X diff 1.5 mm ✓
# col x=21.5: C33(9.0), R5(22.5) → diff 13.5 mm ✓
# col x=19.5: C31(7.5), R1(15.5), R7(24.5) → diffs 8/9 mm ✓
# col x=21.0: R8(24.5) alone ✓
# R7 pad bottom = 24.5+1.45 = 25.95; edge clearance = 27-25.95 = 1.05 mm ✓
place("R5",  21.5, 22.5,  90)
place("R6",  23.0, 22.5,  90)
place("R7",  19.5, 24.5,  90)
place("R8",  21.0, 24.5,  90)

# col x=16.5: C10(8.5), R9(16.5) → diff 8.0 mm ✓
place("R9",  16.5, 16.5,  90)
# col x=18.5: C7(14.0), R3(17.5), W1(24.5) → diffs 3.5/7 mm ✓
place("W1",  18.5, 24.5,  90)

# ── I2C peripherals ───────────────────────────────────────────────────────
# LIS2DW12
place("C34",  6.0, 10.5,  90)   # col x=6.0: C34(10.5), C14(17.0) → 6.5 mm ✓
place("C35",  7.5, 10.5,  90)   # col x=7.5: C35(10.5), C12(14.5) → 4.0 mm ✓

# DRV2605L — ROW at y=12.0 (fix2.py left them as col at x=3.0 → short)
place("C36",  3.5, 12.0,  90)
place("C37",  5.0, 12.0,  90)

place("C38",  8.5,  5.0,  90)   # ST25DV NFC +3V0; col alone ✓

# LCD FPC — ROW at y=3.5
place("C39", 15.0,  3.5,  90)   # col x=15.0: C5(8.5)/C39(3.5) → 5.0 mm ✓
place("C40", 16.5,  3.5,  90)   # col x=16.5: C10(8.5)/C40(3.5) → 5.0 mm ✓

# I2C pull-ups — ROW at y=11.5
# R13 at x=9.0 (not 9.5) to stay clear of Y2 column at x=9.5
# col x=9.0 : R13(11.5) alone ✓
# col x=11.5: R14(11.5), C21(17.5), C30(22.5) → diffs 6/5 mm ✓
place("R13",  9.0, 11.5,  90)
place("R14", 11.5, 11.5,  90)
# col x=11.0: R12(4.5), C13(14.5) → diff 10 mm ✓
place("R12", 11.0,  4.5,  90)

# ── Buzzer — ROW at y=17.5 ───────────────────────────────────────────────
# Old col x=17.5 (R3/R4, 1 mm Y apart) → short.  New ROW layout:
# col x=18.5: C7(14.0), R3(17.5), W1(24.5) → diffs 3.5/7 mm ✓
# col x=20.0: U4(13.5), R4(17.5)            → diff  4.0 mm ✓
place("R3",  18.5, 17.5,  90)
place("R4",  20.0, 17.5,  90)
# TP8 clear of R3/R4: col x=19.5: C31(7.5), R1(15.5), TP8(19.5), R7(24.5)
place("TP8", 19.5, 19.5)

pcbnew.Refresh()
print("=== fix4.py done ===")
print("Expected: ~0 shorting_items.  Run DRC to verify.")
print("Known residual: SW3 courtyard_overlap with U3 — informational only, not fab-blocking.")
print("RUN ORDER: fix2.py → fix3.py → fix4.py")

import pcbnew, math
board = pcbnew.GetBoard()

def place(ref, x, y, rot=0):
    fp = board.FindFootprintByReference(ref)
    if fp:
        fp.SetPosition(pcbnew.VECTOR2I(pcbnew.FromMM(x), pcbnew.FromMM(y)))
        fp.SetOrientationDegrees(rot)
        print(f"  {ref:6s}  ({x}, {y})")
    else:
        print(f"  NOT FOUND: {ref}")

def check_arc_clearance(x, y, arc_cx, arc_cy, arc_r):
    d = math.sqrt((x - arc_cx)**2 + (y - arc_cy)**2)
    return round(d - arc_r, 3)

print("=== fix3.py — edge clearance fixes ===")

# ── SW2 (BTN_2 button, top-right edge) ─────────────────────────────────────
# Top-right arc centre (23, 2) r=2mm. SW2 at (22.5, 6.5) rot=90 puts
# pad 2 (+3V0) at (23.3, 4.45) — 0.447 mm from arc, needs ≥0.5 mm.
# Move SW2 down to y=7.5 → pad 2 at (23.3, 5.5):
#   clearance = sqrt((23.3-23)^2+(5.5-2)^2) - 2 = sqrt(0.09+12.25)-2 = 3.51-2 = 1.51mm ✓
place("SW2", 22.5, 7.5, 90)

# ── U7 (ST25DV04K, top-left area) ──────────────────────────────────────────
# Top-left arc centre (2, 2) r=2mm. U7 at (5.5, 5.5) puts
# pad 1 at (3.025, 3.595) — distance=1.895mm, INSIDE the arc. Physical edge violation.
# Move to (6.5, 6.5): pad 1 at (4.025, 4.595).
#   clearance = sqrt((4.025-2)^2+(4.595-2)^2) - 2 = sqrt(4.1+6.73)-2 = 3.29-2 = 1.29mm ✓
place("U7",  6.5, 6.5)
# C38 bypass cap follows U7
place("C38", 8.5, 5.0, 90)

# ── Pogo pad row (bottom edge) ──────────────────────────────────────────────
# Bottom-left arc centre (2, 25) r=2mm. Pads at y=25.5 need x ≥ 4.45mm.
# TP1 at (4.0, 25.5) → clearance 0.06mm. Shift whole row +0.5mm right.
# New positions: 4.5, 6.0, 7.5, 9.0, 10.5, 12.0, 13.5
# TP1 at (4.5, 25.5): clearance = sqrt(6.5+0.25)-2 = 2.598-2 = 0.60mm ✓
place("TP1",  4.5, 25.5)
place("TP2",  6.0, 25.5)
place("TP3",  7.5, 25.5)
place("TP4",  9.0, 25.5)
place("TP5", 10.5, 25.5)
place("TP6", 12.0, 25.5)
place("TP7", 13.5, 25.5)

# ── TP15/TP16 (NFC antenna taps, top-left area) ────────────────────────────
# TP15 at (3.5, 5.0): DRC reports 0.37mm clearance from top-left arc (2,2).
# Geometrically it's fine (distance=3.35mm) but move slightly to silence DRC.
place("TP15", 4.0, 6.0)
place("TP16", 4.0, 7.5)

pcbnew.Refresh()
print("=== Done ===")
print("NEXT: Board Setup → Constraints → Solder Mask Expansion → set 0.05mm")
print("      Then re-run DRC — should drop to ~0 copper/edge errors.")

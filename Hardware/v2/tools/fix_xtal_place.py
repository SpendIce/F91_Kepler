"""fix_xtal_place.py — relocate Y1 (48 MHz XTAL) next to U1 pins 46/47.

Review finding: Y1 was 13 mm from the CC2652R7 XTAL pins (parasitics, EMI,
startup margin). Swap: Y1 moves to the freed 0402 row above U1, the three
displaced pull resistors (R14/R13 I2C pulls, R1 nRESET pull) take Y1's old
slot at the bottom edge. Y2 (32 kHz) shifts 0.55 mm left so Y1 pad copper
clears Y2 pad copper.

Usage: python3 fix_xtal_place.py IN.kicad_pcb OUT.kicad_pcb
"""
import sys, pcbnew

IN, OUT = sys.argv[1], sys.argv[2]
b = pcbnew.LoadBoard(IN)

MOVES = {
    # ref: (x_mm, y_mm, rot_deg)
    "Y1":  (10.20,  8.55, 180),  # 48M XTAL: pads -> (11.20,8.10) X48M_N, (9.20,9.00) X48M_P
    "Y2":  ( 6.00,  8.80,   0),  # 32k XTAL: 0.55 left, keeps 3.5mm run to U1.3/4
    "R14": (10.55, 23.20,  90),  # SCL pull -> old Y1 slot
    "R13": (11.55, 23.20,  90),  # SDA pull
    "R1":  (12.55, 23.20,  90),  # nRESET pull
}

for ref, (x, y, rot) in MOVES.items():
    fp = b.FindFootprintByReference(ref)
    if fp is None:
        sys.exit(f"missing {ref}")
    fp.SetPosition(pcbnew.VECTOR2I(pcbnew.FromMM(x), pcbnew.FromMM(y)))
    fp.SetOrientationDegrees(rot)
    p = fp.GetPosition()
    print(f"{ref}: -> ({pcbnew.ToMM(p.x):.2f},{pcbnew.ToMM(p.y):.2f}) rot{rot}")

for ref in ("Y1", "Y2"):
    fp = b.FindFootprintByReference(ref)
    for pad in fp.Pads():
        pos = pad.GetPosition()
        print(f"  {ref}.{pad.GetNumber()} ({pcbnew.ToMM(pos.x):.2f},{pcbnew.ToMM(pos.y):.2f}) {pad.GetNetname()}")

b.Save(OUT)
print("saved", OUT)

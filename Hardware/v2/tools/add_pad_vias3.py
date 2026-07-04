"""add_pad_vias3.py — surgery round 3: ladders for power-tree debris.

Remaining F-only endpoints from the round-2 autopsy. Offsets pre-checked;
the script re-verifies against every existing via before placing.
"""
import sys
import pcbnew

BOARD = sys.argv[1] if len(sys.argv) > 1 else "f91_kepler_v2.kicad_pcb"
b = pcbnew.LoadBoard(BOARD)
mm, MM = pcbnew.ToMM, pcbnew.FromMM

VIAS = [
    ("VDDS",     9.75, 17.09),   # U1.13 +0.15y
    ("VDDS",    14.25, 16.79),   # U1.22 -0.15y (clears DCOUPL via at 0.59)
    ("VDDS",    12.55, 22.79),   # R1.2 centre
    ("SDA",      8.91, 15.25),   # U1.10 -0.15x
    ("SWD_TCK", 16.09, 16.25),   # U1.25 +0.15x
    ("SWD_TCK", 14.80, 25.50),   # TP5 centre
    ("VBAT",    20.82, 22.30),   # C30.1 centre
    ("BTN_1",    3.00, 11.05),   # SW1 pad centre (large button pad)
    ("CLAMP1",  13.75, 21.50),   # U2.5 +0.15x
    ("CLAMP1",   9.30, 23.78),   # C25.1 centre
    ("VDDR",    19.55,  4.58),   # C7.1 centre
    ("VDDR",    16.88,  9.90),   # C8.1 centre
]

existing = [(mm(v.GetStart().x), mm(v.GetStart().y), mm(v.GetWidth(pcbnew.F_Cu)))
            for v in b.GetTracks() if v.GetClass() == "PCB_VIA"]

added = 0
for net, x, y in VIAS:
    bad = None
    for ex, ey, ew in existing:
        need = (0.40 + ew) / 2 + 0.15
        if (x - ex) ** 2 + (y - ey) ** 2 < need ** 2:
            bad = (round(ex, 2), round(ey, 2), ew)
            break
    if bad:
        print(f"CONFLICT {net} ({x},{y}) vs via at {bad}")
        continue
    v = pcbnew.PCB_VIA(b)
    v.SetPosition(pcbnew.VECTOR2I(MM(x), MM(y)))
    v.SetWidth(MM(0.40)); v.SetDrill(MM(0.20))
    v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
    v.SetNetCode(b.FindNet(net).GetNetCode())
    v.SetLocked(True)
    b.Add(v)
    existing.append((x, y, 0.40))
    added += 1
    print(f"via {net:9s} at ({x},{y})")
print(f"added {added}")
pcbnew.ZONE_FILLER(b).Fill(b.Zones())
b.Save(BOARD)
print("saved")

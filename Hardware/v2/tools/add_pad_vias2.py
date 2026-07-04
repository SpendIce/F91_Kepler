"""add_pad_vias2.py — surgery round 2: ladders for the 27 survivors.

Every remaining unconnected pair has at least one F-only endpoint. Give
each a locked 0.4/0.2 via (in-pad or at the exact stub end) so the router
can reach it on In2/B. Offsets pre-checked against every existing locked
via (min centre distance 0.55 for 0.4+0.4, 0.58 for 0.46+0.4).

Also bridges the X32K_1 In2 fragment to its locked via and deletes the
zero-length F junk track.
"""
import sys
import pcbnew

BOARD = sys.argv[1] if len(sys.argv) > 1 else "f91_kepler_v2.kicad_pcb"
b = pcbnew.LoadBoard(BOARD)
mm, MM = pcbnew.ToMM, pcbnew.FromMM

# net -> list of exact via positions (already staggered/verified)
VIAS = [
    ("BTN_1",       9.21, 12.75),   # U1.5  +0.15x
    ("BTN_3",      16.09, 13.75),   # U1.30 +0.15x
    ("DCOUPL",     14.73, 17.14),   # U1.23 (nudged x, +0.20y)
    ("LCD_SCLK",   13.25,  9.91),   # U1.41 -0.15y
    ("NFC_GPO",    13.25, 17.09),   # U1.20 +0.15y
    ("AC1",        13.45, 20.00),   # U2.2  -0.15x
    ("COMM1",      14.80, 22.45),   # U2.6  +0.25y
    ("RECT",       15.80, 18.55),   # U2.18 -0.25y
    ("DCOUPL",     13.82, 24.28),   # C10.1 centre
    ("AC1",        19.30, 18.02),   # C20.2 centre
    ("AC1",        19.05, 21.53),   # C21.1 centre
    ("AC2",        19.05, 20.57),   # C21.2 centre
    ("AC2",        16.80, 23.07),   # C22.2 centre
    ("AC1",         9.30, 22.82),   # C25.2 centre
    ("AC2",        18.80, 23.07),   # C26.2 centre
    ("COMM1",       8.05, 17.28),   # C27.1 centre
    ("AC1",         8.05, 16.32),   # C27.2 centre
    ("NFC_GPO",     6.80,  7.31),   # R12.1 centre
    ("SCL",        10.55, 23.81),   # R14.1 centre
    ("CHG_DET",    22.80, 11.06),   # R9.1  centre
    ("VDDS",       20.80,  5.28),   # C2.1  centre
    ("BTN_3",      22.00, 16.95),   # SW3.1 centre (big button pad)
    ("SCL",         2.85, 13.55),   # U6.2  +0.15x
    ("VDDS",       18.25,  4.60),   # F stub end near C2
    ("CHG_DET",    16.65, 13.35),   # F stub end east of U1
    ("nRESET",     12.55, 23.85),   # F stub end at R1.1
]

existing = [(mm(v.GetStart().x), mm(v.GetStart().y),
             mm(v.GetWidth()))
            for v in b.GetTracks() if v.GetClass() == "PCB_VIA"]

added = 0
for net, x, y in VIAS:
    bad = None
    for ex, ey, ew in existing:
        need = (0.40 + ew) / 2 + 0.15
        if (x - ex) ** 2 + (y - ey) ** 2 < need ** 2:
            bad = (ex, ey, ew)
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
print(f"added {added} vias")

# X32K_1: bridge In2 fragment to the locked via, drop the 0-length F junk
xc = b.FindNet("X32K_1").GetNetCode()
for t in list(b.GetTracks()):
    if (t.GetClass() != "PCB_VIA" and t.GetNetCode() == xc
            and t.GetStart() == t.GetEnd()):
        b.Delete(t); print("deleted 0-length X32K_1 junk")
t = pcbnew.PCB_TRACK(b)
t.SetStart(pcbnew.VECTOR2I(MM(9.05), MM(11.75)))
t.SetEnd(pcbnew.VECTOR2I(MM(9.35), MM(11.75)))
t.SetWidth(MM(0.20)); t.SetLayer(pcbnew.In2_Cu)
t.SetNetCode(xc); t.SetLocked(True); b.Add(t)
print("X32K_1 In2 bridge added")

pcbnew.ZONE_FILLER(b).Fill(b.Zones())
b.Save(BOARD)
print("saved")

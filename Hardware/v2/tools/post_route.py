"""post_route.py — import freerouting session and finish the board.

1. ImportSpecctraSES into the board
2. Audit: vias per RF net (report), locked tracks survival
3. Add F.Cu GND pour (same footprint as GND_B), refill all zones
4. Ground stitching vias on a grid where both pours have copper

Usage: python3 post_route.py BOARD.kicad_pcb SESSION.ses
"""
import sys, pcbnew
from collections import Counter

BOARD, SES = sys.argv[1], sys.argv[2]
b = pcbnew.LoadBoard(BOARD)
MM = pcbnew.FromMM

ok = pcbnew.ImportSpecctraSES(b, SES)
print("SES import:", ok)

# --- audit ---
RF = ("RF_P", "RF_N", "RF_UNBAL", "ANT_FEED", "X48M_P", "X48M_N")
vias_per_net = Counter()
tracks_per_net = Counter()
locked = 0
for t in b.GetTracks():
    n = t.GetNetname()
    if t.GetClass() == "PCB_VIA":
        vias_per_net[n] += 1
    else:
        tracks_per_net[n] += 1
        if t.IsLocked():
            locked += 1
print(f"total tracks {sum(tracks_per_net.values())}, vias {sum(vias_per_net.values())}, locked {locked}")
for n in RF:
    print(f"  {n}: {tracks_per_net[n]} segs, {vias_per_net[n]} vias")

# --- F.Cu GND pour ---
for z in list(b.Zones()):
    if z.GetZoneName() == "GND_F":
        b.Delete(z)
z = pcbnew.ZONE(b)
z.SetLayer(pcbnew.F_Cu)
z.SetNetCode(b.FindNet("GND").GetNetCode())
z.SetZoneName("GND_F")
z.SetLocalClearance(MM(0.2))
z.SetMinThickness(MM(0.2))
z.SetThermalReliefGap(MM(0.3))
z.SetThermalReliefSpokeWidth(MM(0.4))
z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
z.Outline().NewOutline()
for x, y in ((0, 0), (25, 0), (25, 27), (0, 27)):
    z.Outline().Append(MM(x), MM(y))
b.Add(z)

filler = pcbnew.ZONE_FILLER(b)
filler.Fill(b.Zones())
print("GND_F added, zones refilled")

# --- stitching vias: grid, only where BOTH zone fills have copper ---
zf = zb = None
for z in b.Zones():
    if z.GetZoneName() == "GND_F": zf = z
    if z.GetZoneName() == "GND_B": zb = z
gnd = b.FindNet("GND").GetNetCode()

def clear_of_items(x, y, r_mm):
    probe = pcbnew.VECTOR2I(MM(x), MM(y))
    r = MM(r_mm)
    for t in b.GetTracks():
        if t.GetNetname() == "GND":
            continue
        if t.HitTest(probe, r):
            return False
    for fp in b.GetFootprints():
        for p in fp.Pads():
            if p.GetNetname() != "GND" and p.HitTest(probe, r):
                return False
    return True

added = 0
STEP = 2.5
y = 1.5
while y < 26.5:
    x = 1.5
    while x < 24.5:
        pt = pcbnew.VECTOR2I(MM(x), MM(y))
        if (zf.GetFilledPolysList(pcbnew.F_Cu).Contains(pt)
                and zb.GetFilledPolysList(pcbnew.B_Cu).Contains(pt)
                and clear_of_items(x, y, 0.55)):
            v = pcbnew.PCB_VIA(b)
            v.SetPosition(pt)
            v.SetDrill(MM(0.3))
            v.SetWidth(MM(0.6))
            v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
            v.SetNetCode(gnd)
            b.Add(v)
            added += 1
        x += STEP
    y += STEP
print(f"stitching vias: {added}")

filler = pcbnew.ZONE_FILLER(b)
filler.Fill(b.Zones())
b.Save(BOARD)
print("saved", BOARD)

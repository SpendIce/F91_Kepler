"""fix_rf_conflicts.py — resolve collisions between hand RF routes and
freerouting leftovers + board-constraint issues.

1. Board minimums: through-drill 0.2 / via dia 0.4 (JLC standard-PCB capable)
   so the U1.2 via-in-pad passes.
2. GND zones -> solid connection (kills starved_thermal on dense F side).
3. Rip freerouting GND copper colliding with the locked RF corridor, and any
   unlocked B.Cu track crossing the locked RF_UNBAL sweep.
4. RF_N B-leg y 11.95 -> 11.75 (0.245 to RF_UNBAL via, was 0.045).
5. Delete GND vias closer than 0.55 mm to the board edge (freerouting used
   0.2 class clearance; edge rule is 0.25).
Usage: python3 fix_rf_conflicts.py BOARD.kicad_pcb
"""
import sys, math, pcbnew

BOARD = sys.argv[1]
b = pcbnew.LoadBoard(BOARD)
MM = pcbnew.FromMM
def mm(v): return pcbnew.ToMM(v)

ds = b.GetDesignSettings()
ds.m_MinThroughDrill = MM(0.2)
ds.m_ViasMinSize = MM(0.4)
print("board min: drill 0.2, via 0.4")

for z in b.Zones():
    if z.GetZoneName() in ("GND_F", "GND_B"):
        z.SetPadConnection(pcbnew.ZONE_CONNECTION_FULL)
print("GND zones -> solid connect")

# --- rip conflicting unlocked copper ---
HOT = [(6.90, 12.30, 1.3), (4.00, 6.45, 1.3), (8.15, 12.42, 1.1), (9.06, 11.25, 0.9)]
# locked RF_UNBAL B sweep segments as (x1,y1,x2,y2)
SWEEP = [(8.15, 12.42, 5.20, 12.75), (5.20, 12.75, 4.20, 11.00),
         (4.20, 11.00, 4.20, 7.00), (4.20, 7.00, 4.00, 6.45)]

def seg_dist(px, py, x1, y1, x2, y2):
    dx, dy = x2 - x1, y2 - y1
    L2 = dx * dx + dy * dy
    t = 0 if L2 == 0 else max(0, min(1, ((px - x1) * dx + (py - y1) * dy) / L2))
    return math.hypot(px - (x1 + t * dx), py - (y1 + t * dy))

def segs_cross(a, c):
    (ax1, ay1, ax2, ay2), (cx1, cy1, cx2, cy2) = a, c
    d = lambda ox, oy, px, py, qx, qy: (px - ox) * (qy - oy) - (py - oy) * (qx - ox)
    d1 = d(ax1, ay1, ax2, ay2, cx1, cy1); d2 = d(ax1, ay1, ax2, ay2, cx2, cy2)
    d3 = d(cx1, cy1, cx2, cy2, ax1, ay1); d4 = d(cx1, cy1, cx2, cy2, ax2, ay2)
    return d1 * d2 < 0 and d3 * d4 < 0

ripped = 0
for t in list(b.GetTracks()):
    if t.IsLocked():
        continue
    x1, y1 = mm(t.GetStart().x), mm(t.GetStart().y)
    x2, y2 = mm(t.GetEnd().x), mm(t.GetEnd().y)
    kill = False
    if t.GetNetname() == "GND":
        for hx, hy, hr in HOT:
            if seg_dist(hx, hy, x1, y1, x2, y2) < hr:
                kill = True
        if t.GetClass() == "PCB_VIA":
            ex = min(x1, 25 - x1, y1, 27 - y1)
            if ex < 0.55:
                kill = True
    if not kill and t.GetClass() != "PCB_VIA" and t.GetLayer() == pcbnew.B_Cu:
        for s in SWEEP:
            if segs_cross((x1, y1, x2, y2), s):
                kill = True
    if kill:
        print(f"rip {t.GetNetname()} {t.GetClass()} ({x1:.2f},{y1:.2f})-({x2:.2f},{y2:.2f})")
        b.Delete(t)
        ripped += 1
print("ripped:", ripped)

# --- move RF_N B-leg up to y 11.75 ---
for t in list(b.GetTracks()):
    if t.GetNetname() == "RF_N" and t.GetClass() != "PCB_VIA" and t.GetLayer() == pcbnew.B_Cu:
        b.Delete(t)
def bseg(net, x1, y1, x2, y2):
    t = pcbnew.PCB_TRACK(b)
    t.SetStart(pcbnew.VECTOR2I(MM(x1), MM(y1)))
    t.SetEnd(pcbnew.VECTOR2I(MM(x2), MM(y2)))
    t.SetWidth(MM(0.25))
    t.SetLayer(pcbnew.B_Cu)
    t.SetNetCode(b.FindNet(net).GetNetCode())
    t.SetLocked(True)
    b.Add(t)
bseg("RF_N", 6.90, 12.30, 7.45, 11.75)
bseg("RF_N", 7.45, 11.75, 8.50, 11.75)
bseg("RF_N", 8.50, 11.75, 9.06, 11.25)
print("RF_N B-leg rerouted at y 11.75")

pcbnew.ZONE_FILLER(b).Fill(b.Zones())
b.Save(BOARD)
print("saved")

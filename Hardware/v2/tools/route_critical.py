"""route_critical.py — hand-route the RF-critical nets with locked tracks.

Routes (all F.Cu):
  RF_P   : U1.1  (9.06,10.75) -> FL1.4 (7.05,10.40)   0.25 mm
  RF_N   : U1.2  (9.06,11.25) -> FL1.3 (7.75,11.40)   0.25 mm
  X48M_P : Y1.3  (9.20,9.00)  -> U1.47 (10.25,10.06)  0.20 mm
  X48M_N : Y1.1  (11.20,8.10) -> U1.46 (10.75,10.06)  0.20 mm
  ANT_FEED final hop: R2.2 (6.00,3.49) -> AE1.1 (3.00,1.90) 0.30 mm

Tracks are locked so freerouting (DSN "protect") keeps them.
Usage: python3 route_critical.py IN.kicad_pcb OUT.kicad_pcb
"""
import sys, pcbnew

IN, OUT = sys.argv[1], sys.argv[2]
b = pcbnew.LoadBoard(IN)
MM = pcbnew.FromMM

def net(name):
    n = b.FindNet(name)
    if n is None:
        sys.exit(f"net {name} not found")
    return n.GetNetCode()

def route(netname, pts, w):
    code = net(netname)
    for (x1, y1), (x2, y2) in zip(pts, pts[1:]):
        t = pcbnew.PCB_TRACK(b)
        t.SetStart(pcbnew.VECTOR2I(MM(x1), MM(y1)))
        t.SetEnd(pcbnew.VECTOR2I(MM(x2), MM(y2)))
        t.SetWidth(MM(w))
        t.SetLayer(pcbnew.F_Cu)
        t.SetNetCode(code)
        t.SetLocked(True)
        b.Add(t)
    print(f"{netname}: {len(pts)-1} segs, {w} mm")

# RF diff pair U1 <-> balun, 45-degree bends
route("RF_P", [(9.06, 10.75), (7.40, 10.75), (7.05, 10.40)], 0.25)
route("RF_N", [(9.06, 11.25), (7.90, 11.25), (7.75, 11.40)], 0.25)

# 48 MHz crystal, direct diagonals (no crossover by construction)
route("X48M_P", [(9.20, 9.00), (10.25, 10.05), (10.25, 10.06)], 0.20)
route("X48M_N", [(11.20, 8.10), (10.75, 8.55), (10.75, 10.06)], 0.20)

# antenna feed: series element R2 -> AE1 feed pad, through the keepout corner
route("ANT_FEED", [(6.00, 3.49), (6.00, 3.20), (4.30, 1.90), (3.00, 1.90)], 0.30)

b.Save(OUT)
print("saved", OUT)

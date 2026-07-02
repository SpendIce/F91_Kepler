"""route_rf_bcu.py — hand-route RF_N and RF_UNBAL (the two nets the FL1
corner cannot fit on F.Cu; exhaustive corridor analysis in session notes).

RF_N: FL1.3 -> B.Cu sweep -> 0.4/0.2 via-in-pad at U1.2 (standard RF
practice; JLC min via 0.4/0.2 ok). RF_UNBAL: FL1.1 -> east stub -> F
descent x8.36 -> B.Cu sweep -> back to F near the match -> C14.1.
All locked. Usage: python3 route_rf_bcu.py BOARD.kicad_pcb
"""
import sys, pcbnew

BOARD = sys.argv[1]
b = pcbnew.LoadBoard(BOARD)
MM = pcbnew.FromMM

def seg(net, x1, y1, x2, y2, layer, w):
    t = pcbnew.PCB_TRACK(b)
    t.SetStart(pcbnew.VECTOR2I(MM(x1), MM(y1)))
    t.SetEnd(pcbnew.VECTOR2I(MM(x2), MM(y2)))
    t.SetWidth(MM(w))
    t.SetLayer(layer)
    t.SetNetCode(b.FindNet(net).GetNetCode())
    t.SetLocked(True)
    b.Add(t)

def via(net, x, y, dia, drill):
    v = pcbnew.PCB_VIA(b)
    v.SetPosition(pcbnew.VECTOR2I(MM(x), MM(y)))
    v.SetDrill(MM(drill))
    v.SetWidth(MM(dia))
    v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
    v.SetNetCode(b.FindNet(net).GetNetCode())
    v.SetLocked(True)
    b.Add(v)

F, B = pcbnew.F_Cu, pcbnew.B_Cu

# --- RF_N: FL1.3 [6.70..7.10,11.10..11.40] -> U1.2 (9.06,11.25) ---
seg("RF_N", 6.90, 11.40, 6.90, 12.30, F, 0.25)   # south exit
via("RF_N", 6.90, 12.30, 0.6, 0.3)
seg("RF_N", 6.90, 12.30, 7.20, 11.95, B, 0.25)
seg("RF_N", 7.20, 11.95, 8.50, 11.95, B, 0.25)
seg("RF_N", 8.50, 11.95, 9.06, 11.25, B, 0.25)
via("RF_N", 9.06, 11.25, 0.4, 0.2)               # via-in-pad U1.2

# --- RF_UNBAL: FL1.1 [7.70..8.10,11.10..11.40] -> C14.1 [4.24..4.86,5.00..5.56] ---
seg("RF_UNBAL", 8.10, 11.25, 8.36, 11.25, F, 0.20)  # east stub
seg("RF_UNBAL", 8.36, 11.25, 8.36, 12.25, F, 0.20)  # descent past U1.3/4
seg("RF_UNBAL", 8.36, 12.25, 8.15, 12.42, F, 0.20)
via("RF_UNBAL", 8.15, 12.42, 0.6, 0.3)
seg("RF_UNBAL", 8.15, 12.42, 5.20, 12.75, B, 0.25)  # B sweep west
seg("RF_UNBAL", 5.20, 12.75, 4.20, 11.00, B, 0.25)
seg("RF_UNBAL", 4.20, 11.00, 4.20, 7.00, B, 0.25)
seg("RF_UNBAL", 4.20, 7.00, 4.00, 6.45, B, 0.25)
via("RF_UNBAL", 4.00, 6.45, 0.6, 0.3)
seg("RF_UNBAL", 4.00, 6.45, 4.30, 5.70, F, 0.25)
seg("RF_UNBAL", 4.30, 5.70, 4.55, 5.56, F, 0.25)    # into C14.1

# refill zones (B pour must re-clear around new tracks)
pcbnew.ZONE_FILLER(b).Fill(b.Zones())
b.Save(BOARD)
print("RF_N + RF_UNBAL routed (locked), zones refilled, saved")

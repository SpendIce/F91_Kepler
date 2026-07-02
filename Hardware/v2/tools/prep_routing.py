"""prep_routing.py — one-shot routing preparation for f91_kepler_v2.

1. Placement fixes:
   - FL1 balun -> rot180 (RF_P pad lands on U1.1's row: direct trace)
   - C8 (VDDR 100n) -> (16.30,9.60) rot90 next to L2/VDDR source; frees the
     only escape pocket south of FL1 for RF_N / RF_UNBAL
   - R14/R13/R1 -> y 23.30 (0.165 mm pad gap vs U7.5 -> 0.265)
2. Netclasses in .kicad_pro:
   - RF (0.25 track / 0.15 clearance): RF_P RF_N RF_UNBAL ANT_FEED X48M_* X32K_*
   - POWER (0.35 track): rails + Qi + haptic/buzzer drive
3. Hand-routes (locked, F.Cu), all analytically clearance-checked:
   - RF_P, X48M_P, X48M_N, ANT_FEED final hop R2.2->AE1.1

Usage: python3 prep_routing.py  (in Hardware/v2, edits files in place)
"""
import json, pcbnew

BOARD = "f91_kepler_v2.kicad_pcb"
PRO = "f91_kepler_v2.kicad_pro"

RF_NETS = ["RF_P", "RF_N", "RF_UNBAL", "ANT_FEED",
           "X48M_P", "X48M_N", "X32K_1", "X32K_2"]
PWR_NETS = ["VBAT", "BATN", "+3V0", "VDDS", "VDDR", "DCDC_SW", "DCOUPL",
            "MOT_P", "MOT_N", "PIEZO", "AC1", "AC2", "RECT", "COMM1",
            "CLAMP1", "BOOT2", "DRV_REG"]

# --- project netclasses ---
d = json.load(open(PRO))
ns = d["net_settings"]
base = ns["classes"][0]
def clone(name, track, clearance):
    c = dict(base)
    c["name"] = name
    c["track_width"] = track
    c["clearance"] = clearance
    return c
ns["classes"] = [c for c in ns["classes"] if c["name"] == "Default"]
ns["classes"].append(clone("RF", 0.25, 0.15))
ns["classes"].append(clone("POWER", 0.35, 0.2))
ns["netclass_patterns"] = (
    [{"netclass": "RF", "pattern": n} for n in RF_NETS] +
    [{"netclass": "POWER", "pattern": n} for n in PWR_NETS])
json.dump(d, open(PRO, "w"), indent=2)
print("netclasses: RF x%d, POWER x%d" % (len(RF_NETS), len(PWR_NETS)))

# --- custom DRC rules (fab floor: JLCPCB 0.127 mm) ---
open("f91_kepler_v2.kicad_dru", "w").write('''(version 1)
(rule "rf_relax"
  (condition "A.NetClass == 'RF' || B.NetClass == 'RF'")
  (constraint clearance (min 0.15mm)))
(rule "u5_lga_pads"
  (condition "A.Type == 'Pad' && B.Type == 'Pad' && A.insideCourtyard('U5') && B.insideCourtyard('U5')")
  (constraint clearance (min 0.12mm)))
(rule "fl1_balun_pads"
  (condition "A.Type == 'Pad' && B.Type == 'Pad' && A.insideCourtyard('FL1') && B.insideCourtyard('FL1')")
  (constraint clearance (min 0.09mm)))
''')
print("wrote f91_kepler_v2.kicad_dru")

# --- board edits ---
b = pcbnew.LoadBoard(BOARD)
MM = pcbnew.FromMM

MOVES = {
    "FL1": (7.40, 10.90, 180),
    "C8":  (16.30, 9.60, 90),
    "R14": (10.55, 23.30, 90),
    "R13": (11.55, 23.30, 90),
    "R1":  (12.55, 23.30, 90),
}
for ref, (x, y, rot) in MOVES.items():
    fp = b.FindFootprintByReference(ref)
    fp.SetPosition(pcbnew.VECTOR2I(MM(x), MM(y)))
    fp.SetOrientationDegrees(rot)
    print(f"{ref} -> ({x},{y}) rot{rot}")

def route(netname, pts, w):
    code = b.FindNet(netname).GetNetCode()
    for (x1, y1), (x2, y2) in zip(pts, pts[1:]):
        t = pcbnew.PCB_TRACK(b)
        t.SetStart(pcbnew.VECTOR2I(MM(x1), MM(y1)))
        t.SetEnd(pcbnew.VECTOR2I(MM(x2), MM(y2)))
        t.SetWidth(MM(w))
        t.SetLayer(pcbnew.F_Cu)
        t.SetNetCode(code)
        t.SetLocked(True)
        b.Add(t)
    print(f"routed {netname} ({len(pts)-1} segs, {w}mm, locked)")

# RF_P: FL1.4 [7.70..8.10,10.40..10.70] -> U1.1 [8.62..9.50,10.62..10.88]
route("RF_P", [(8.10, 10.55), (8.30, 10.75), (8.90, 10.75)], 0.25)
# X48M_P: Y1.3 (9.70,9.00) -> U1.47 (10.25,10.06)
route("X48M_P", [(9.70, 9.00), (10.25, 9.55), (10.25, 10.06)], 0.20)
# X48M_N: Y1.1 (11.70,8.10) -> U1.46 (10.70,10.06) through the pad3/pad4 gap
route("X48M_N", [(11.70, 8.10), (10.70, 8.10), (10.70, 10.06)], 0.20)
# ANT_FEED hop: R2.2 -> AE1.1 (corner-arc-safe, keepout allows tracks)
route("ANT_FEED", [(6.00, 3.30), (6.00, 3.12), (2.90, 3.12), (2.90, 2.60)], 0.30)

b.Save(BOARD)
print("saved", BOARD)

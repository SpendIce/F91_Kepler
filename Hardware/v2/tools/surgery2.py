"""surgery2.py — placement surgery round 2 + 0.15 mm clearance rules.

Rationale (user-approved): remaining ~25 nets are placement-blocked, not
router-blocked. JLCPCB standard 2-layer does 0.127/0.127, so:
  - Default netclass clearance 0.2 -> 0.15 (track widths unchanged)
  - U2 Qi charger +0.7 south (U1-U2 pad gap 0.35 -> 1.05: routing channel)
  - FB1 +0.5, C22/C23/C26/R7 +0.5 south (cascade under U2)
  - R3/C11/R5/C18/R4 0402 row -0.6 north (opens LCD via lane y~9.2 between
    row and U1 top pads for the re-pinned LCD nets -> J1)
  - U6 -0.05 west (U6-U1 channel 0.44 -> 0.49: one 0.15-clearance track)
  - RF_UNBAL via (8.15,12.42) -> (7.75,12.42) (frees U6-U1 channel mouth)
Then rips all unlocked copper for a clean reroute.

Usage: python3 surgery2.py  (in Hardware/v2)
"""
import json, math, pcbnew

BOARD = "f91_kepler_v2.kicad_pcb"
PRO = "f91_kepler_v2.kicad_pro"
MMf = pcbnew.FromMM
def mm(v): return pcbnew.ToMM(v)

# --- project rules ---
d = json.load(open(PRO))
for c in d["net_settings"]["classes"]:
    if c["name"] in ("Default", "POWER"):
        c["clearance"] = 0.15
json.dump(d, open(PRO, "w"), indent=2)
print("netclass clearance -> 0.15 (Default, POWER)")

b = pcbnew.LoadBoard(BOARD)

MOVES = {
    "U2":  (15.80, 20.50),
    "FB1": (14.80, 23.30),
    "C22": (16.80, 23.55),
    "C23": (17.80, 23.55),
    "C26": (18.80, 23.55),
    "R7":  (19.80, 23.55),
    "R3":  (12.80, 7.95),
    "C11": (13.80, 7.95),
    "R5":  (14.80, 7.95),
    "C18": (15.80, 7.95),
    "R4":  (16.80, 7.95),
    "U6":  (5.25, 14.05),
}
for ref, (x, y) in MOVES.items():
    fp = b.FindFootprintByReference(ref)
    fp.SetPosition(pcbnew.VECTOR2I(MMf(x), MMf(y)))
    print(f"{ref} -> ({x},{y})")

# --- relocate RF_UNBAL via + attached locked segs ---
for t in list(b.GetTracks()):
    if t.GetNetname() != "RF_UNBAL":
        continue
    s, e = t.GetStart(), t.GetEnd()
    if t.GetClass() == "PCB_VIA" and abs(mm(s.x) - 8.15) < 0.01:
        b.Delete(t)
        print("deleted RF_UNBAL via @8.15")
    elif t.GetClass() != "PCB_VIA":
        pts = {(round(mm(s.x), 2), round(mm(s.y), 2)),
               (round(mm(e.x), 2), round(mm(e.y), 2))}
        if (8.15, 12.42) in pts:
            b.Delete(t)
            print(f"deleted RF_UNBAL seg touching 8.15,12.42")

def seg(net, x1, y1, x2, y2, layer, w):
    t = pcbnew.PCB_TRACK(b)
    t.SetStart(pcbnew.VECTOR2I(MMf(x1), MMf(y1)))
    t.SetEnd(pcbnew.VECTOR2I(MMf(x2), MMf(y2)))
    t.SetWidth(MMf(w))
    t.SetLayer(layer)
    t.SetNetCode(b.FindNet(net).GetNetCode())
    t.SetLocked(True)
    b.Add(t)

v = pcbnew.PCB_VIA(b)
v.SetPosition(pcbnew.VECTOR2I(MMf(7.75), MMf(12.42)))
v.SetDrill(MMf(0.3)); v.SetWidth(MMf(0.6))
v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
v.SetNetCode(b.FindNet("RF_UNBAL").GetNetCode())
v.SetLocked(True)
b.Add(v)
seg("RF_UNBAL", 8.36, 12.25, 7.75, 12.42, pcbnew.F_Cu, 0.20)
seg("RF_UNBAL", 7.75, 12.42, 7.60, 13.05, pcbnew.B_Cu, 0.25)
print("RF_UNBAL via -> (7.75,12.42), segs restitched")

# --- rip all unlocked copper for clean reroute ---
ripped = 0
for t in list(b.GetTracks()):
    if not t.IsLocked():
        b.Delete(t)
        ripped += 1
print("ripped unlocked:", ripped)

pcbnew.ZONE_FILLER(b).Fill(b.Zones())
b.Save(BOARD)
print("saved")

# J1 pad extents (for LCD B.Cu lane planning)
j1 = b.FindFootprintByReference("J1")
for p in j1.Pads():
    bb = p.GetBoundingBox()
    print(f"J1.{p.GetNumber():4s} [{mm(bb.GetLeft()):.2f},{mm(bb.GetTop()):.2f}.."
          f"{mm(bb.GetRight()):.2f},{mm(bb.GetBottom()):.2f}] {p.GetNetname()}")

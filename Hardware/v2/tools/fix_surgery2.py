"""fix_surgery2.py — second cleanup round.

U5 is a 4-side LGA: stub vias must sit NORTH of the package, not between
pad columns. X32K_1 via re-centred on pad 3's y (11.75). Rip the In2
fragments (VDDS, CHG_DET, PROT_CS, nRESET) shorting the new locked vias.
"""
import sys
import pcbnew

BOARD = sys.argv[1] if len(sys.argv) > 1 else "f91_kepler_v2.kicad_pcb"
b = pcbnew.LoadBoard(BOARD)
mm = pcbnew.ToMM
MM = pcbnew.FromMM

def near(t, x, y, tol=0.12):
    return abs(mm(t.GetStart().x) - x) < tol and abs(mm(t.GetStart().y) - y) < tol

removed = 0
for t in list(b.GetTracks()):
    if t.GetClass() == "PCB_VIA":
        if near(t, 5.15, 11.00) or near(t, 5.95, 11.00):
            b.Delete(t); removed += 1
        elif near(t, 9.35, 11.80):
            t.SetPosition(pcbnew.VECTOR2I(MM(9.35), MM(11.75)))
            print("X32K_1 via -> (9.35,11.75)")
    elif t.IsLocked() and t.GetLayer() == pcbnew.F_Cu:
        # the two U5 stub tracks placed by fix_surgery.py
        s, e = t.GetStart(), t.GetEnd()
        if (abs(mm(e.x) - 5.15) < 0.05 and abs(mm(e.y) - 11.0) < 0.05) or \
           (abs(mm(e.x) - 5.95) < 0.05 and abs(mm(e.y) - 11.0) < 0.05):
            b.Delete(t); removed += 1
print(f"removed {removed} bad U5 items")

def add_via(net, x, y):
    v = pcbnew.PCB_VIA(b)
    v.SetPosition(pcbnew.VECTOR2I(MM(x), MM(y)))
    v.SetWidth(MM(0.40)); v.SetDrill(MM(0.20))
    v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
    v.SetNetCode(b.FindNet(net).GetNetCode())
    v.SetLocked(True)
    b.Add(v)

def add_stub(net, x1, y1, x2, y2):
    t = pcbnew.PCB_TRACK(b)
    t.SetStart(pcbnew.VECTOR2I(MM(x1), MM(y1)))
    t.SetEnd(pcbnew.VECTOR2I(MM(x2), MM(y2)))
    t.SetWidth(MM(0.20))
    t.SetLayer(pcbnew.F_Cu)
    t.SetNetCode(b.FindNet(net).GetNetCode())
    t.SetLocked(True)
    b.Add(t)

# north of U5 (pads 12/11 on north row at y=10.3)
add_via("ACC_INT1", 5.15, 9.60); add_stub("ACC_INT1", 5.30, 10.30, 5.15, 9.60)
add_via("ACC_INT2", 5.95, 9.60); add_stub("ACC_INT2", 5.80, 10.30, 5.95, 9.60)
print("U5 stub vias re-placed north")

RIP = ["VDDS", "CHG_DET", "PROT_CS", "nRESET"]
codes = {b.FindNet(n).GetNetCode(): n for n in RIP if b.FindNet(n)}
n = 0
for t in list(b.GetTracks()):
    if not t.IsLocked() and t.GetNetCode() in codes:
        b.Delete(t); n += 1
print(f"ripped {n} unlocked items across {len(codes)} nets")

pcbnew.ZONE_FILLER(b).Fill(b.Zones())
b.Save(BOARD)
print("saved")

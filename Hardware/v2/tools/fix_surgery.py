"""fix_surgery.py — post via-in-pad cleanup.

1. U5 (LGA) via-in-pads violate hole clearance vs 0.5-pitch neighbours:
   delete, re-place 0.7 mm south with locked F.Cu stub tracks.
2. X32K_1 via at U1.3 sits 0.56 mm from the locked RF_N via (needs 0.58):
   nudge to (9.35, 11.80).
3. Rip every unlocked net now colliding with the new locked vias; the
   router re-routes them around the hard obstacles next cycle.

Usage: python3 tools/fix_surgery.py BOARD.kicad_pcb
"""
import sys
import pcbnew

BOARD = sys.argv[1] if len(sys.argv) > 1 else "f91_kepler_v2.kicad_pcb"
b = pcbnew.LoadBoard(BOARD)
mm = pcbnew.ToMM
MM = pcbnew.FromMM

def near(v, x, y, tol=0.1):
    return abs(mm(v.GetStart().x) - x) < tol and abs(mm(v.GetStart().y) - y) < tol

# --- 1+2: delete U5 vias, move X32K_1 via ---
moved = deleted = 0
for t in list(b.GetTracks()):
    if t.GetClass() != "PCB_VIA":
        continue
    if near(t, 5.30, 10.14) or near(t, 5.80, 10.44):
        b.Delete(t); deleted += 1
    elif near(t, 9.21, 11.75):
        t.SetPosition(pcbnew.VECTOR2I(MM(9.35), MM(11.80))); moved += 1
print(f"deleted {deleted} U5 vias, moved {moved} X32K_1 via")

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

# U5.12 = ACC_INT1 pad (5.3,10.3); U5.11 = ACC_INT2 pad (5.8,10.3)
add_via("ACC_INT1", 5.15, 11.00); add_stub("ACC_INT1", 5.30, 10.30, 5.15, 11.00)
add_via("ACC_INT2", 5.95, 11.00); add_stub("ACC_INT2", 5.80, 10.30, 5.95, 11.00)
print("U5 stub vias placed")

# --- 3: rip colliding unlocked nets ---
RIP = ["+3V0", "SWD_TCK", "BTN_1", "BTN_2", "NFC_GPO", "SDA", "DCOUPL",
       "DCDC_SW", "PIEZO", "CLAMP2", "BATN", "TERM", "RECT", "COMM1",
       "COMM2", "AC1", "VDDR", "LCD_MOSI"]
codes = {b.FindNet(n).GetNetCode(): n for n in RIP if b.FindNet(n)}
n = 0
for t in list(b.GetTracks()):
    if not t.IsLocked() and t.GetNetCode() in codes:
        b.Delete(t); n += 1
print(f"ripped {n} unlocked items across {len(codes)} nets")

pcbnew.ZONE_FILLER(b).Fill(b.Zones())
b.Save(BOARD)
print("saved")

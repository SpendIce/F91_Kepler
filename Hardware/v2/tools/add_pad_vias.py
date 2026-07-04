"""add_pad_vias.py — locked 0.4/0.2 via-in-pads on stuck route endpoints.

Gives each walled-in pad direct In2/B access so grid_router can escape on
the near-empty inner layer. Stagger offsets (along the pad long axis) keep
adjacent QFN via holes apart. Vias are locked: rip() never deletes them and
foreign nets treat them as hard obstacles.

Usage: python3 tools/add_pad_vias.py BOARD.kicad_pcb
"""
import sys
import pcbnew

BOARD = sys.argv[1] if len(sys.argv) > 1 else "f91_kepler_v2.kicad_pcb"
VIA_D, VIA_DRILL = 0.40, 0.20

# (ref, pad) -> stagger offset in mm along pad long axis (0 = pad centre)
TARGETS = {
    # U1 CC2652R7 QFN48 0.5-pitch
    ("U1", "3"): +0.15, ("U1", "4"): -0.15,      # X32K_1 / X32K_2
    ("U1", "11"): +0.15, ("U1", "12"): -0.15,    # SCL / ACC_INT1
    ("U1", "15"): +0.15, ("U1", "16"): -0.15, ("U1", "17"): +0.15,  # UART_RX/TX, ACC_INT2
    ("U1", "24"): 0.0,                            # SWD_TMS
    ("U1", "32"): 0.0, ("U1", "34"): +0.15, ("U1", "35"): -0.15,    # BUZZER, VDDS, nRESET
    ("U1", "37"): 0.0, ("U1", "39"): 0.0, ("U1", "44"): 0.0,        # HAPTIC_EN, LCD_EXTCOMIN, VDDS
    # U2 wireless charger
    ("U2", "3"): +0.15, ("U2", "4"): -0.15,       # BOOT1, VBAT
    ("U2", "7"): 0.0,                             # CHG_DET
    ("U2", "12"): +0.15, ("U2", "13"): -0.15, ("U2", "14"): +0.15,  # ILIM, TS, FOD_TAP
    ("U2", "19"): 0.0,                            # AC2
    ("U4", "3"): 0.0,                             # VBAT
    ("U5", "11"): +0.15, ("U5", "12"): -0.15,     # ACC_INT2 / ACC_INT1
    ("U6", "5"): 0.0, ("U6", "9"): 0.0,           # HAPTIC_EN, MOT_N
    ("U7", "6"): 0.0,                             # SCL
    # passives / connector / crystal / test pads (centre)
    ("C6", "1"): 0.0, ("C11", "1"): 0.0, ("C23", "1"): 0.0,
    ("C24", "2"): 0.0, ("C28", "2"): 0.0,
    ("R3", "1"): 0.0, ("R5", "1"): 0.0, ("R5", "2"): 0.0,
    ("R6", "1"): 0.0, ("R8", "1"): 0.0, ("R9", "2"): 0.0,
    ("R10", "1"): 0.0, ("R13", "1"): 0.0, ("FB1", "2"): 0.0,
    ("J1", "4"): 0.0, ("Y2", "1"): 0.0, ("Y2", "2"): 0.0,
    ("TP4", "1"): 0.0, ("TP6", "1"): 0.0, ("TP7", "1"): 0.0,
    ("TP14", "1"): 0.0,
}

b = pcbnew.LoadBoard(BOARD)
mm = pcbnew.ToMM
MM = pcbnew.FromMM

existing = [(mm(v.GetStart().x), mm(v.GetStart().y)) for v in b.GetTracks()
            if v.GetClass() == "PCB_VIA"]

added, skipped = 0, []
for fp in b.GetFootprints():
    ref = fp.GetReference()
    for p in fp.Pads():
        key = (ref, p.GetName())
        if key not in TARGETS:
            continue
        off = TARGETS.pop(key)
        c = p.GetPosition()
        cx, cy = mm(c.x), mm(c.y)
        bb = p.GetBoundingBox()
        if bb.GetWidth() >= bb.GetHeight():
            vx, vy = cx + off, cy
        else:
            vx, vy = cx, cy + off
        if any((vx - ex) ** 2 + (vy - ey) ** 2 < 0.3 ** 2 for ex, ey in existing):
            skipped.append((key, "via already nearby"))
            continue
        v = pcbnew.PCB_VIA(b)
        v.SetPosition(pcbnew.VECTOR2I(MM(vx), MM(vy)))
        v.SetWidth(MM(VIA_D)); v.SetDrill(MM(VIA_DRILL))
        v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
        v.SetNetCode(p.GetNetCode())
        v.SetLocked(True)
        b.Add(v)
        existing.append((vx, vy))
        added += 1
        print(f"via {v.GetNetname():14s} at ({vx:.2f},{vy:.2f}) in {ref}.{p.GetName()}")

for key in TARGETS:
    skipped.append((key, "pad not found"))
for k, why in skipped:
    print("SKIP", k, why)
print(f"added {added} via-in-pads")
pcbnew.ZONE_FILLER(b).Fill(b.Zones())
b.Save(BOARD)
print("saved")

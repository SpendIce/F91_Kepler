#!/usr/bin/env python3
"""Build Hardware/v2/f91_kepler_v2.kicad_pcb from netlist.py via the pcbnew API.

Produces a board with: every footprint loaded + placed (spread on the schematic
grid so nothing overlaps), the full netlist assigned (so the ratsnest is live),
2-layer / 1.0 mm stackup, JLCPCB-class design rules, and a 30x28 mm rounded
board outline on Edge.Cuts.

This is a LAYOUT STARTING POINT — placement is rough and nothing is routed.
Open in the KiCad PCB editor, run Tools > Update PCB from Schematic (it
reconciles by reference, so this board stays in sync), then place/route per
docs/phase2/v2_pcb_layout_guide.md.

Run:  python3 gen_pcb.py   then  kicad-cli pcb drc f91_kepler_v2.kicad_pcb
"""

import os
import pcbnew

from netlist import INSTANCES

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "f91_kepler_v2.kicad_pcb")

LIB_PATHS = {
    "f91_footprints": os.path.join(HERE, "..", "lib", "f91_footprints.pretty"),
    "Capacitor_SMD": "/usr/share/kicad/footprints/Capacitor_SMD.pretty",
    "Resistor_SMD": "/usr/share/kicad/footprints/Resistor_SMD.pretty",
    "Inductor_SMD": "/usr/share/kicad/footprints/Inductor_SMD.pretty",
    "Package_DFN_QFN": "/usr/share/kicad/footprints/Package_DFN_QFN.pretty",
    "Package_LGA": "/usr/share/kicad/footprints/Package_LGA.pretty",
    "Package_SO": "/usr/share/kicad/footprints/Package_SO.pretty",
    "Package_TO_SOT_SMD": "/usr/share/kicad/footprints/Package_TO_SOT_SMD.pretty",
    "Crystal": "/usr/share/kicad/footprints/Crystal.pretty",
    "Button_Switch_SMD": "/usr/share/kicad/footprints/Button_Switch_SMD.pretty",
    "TestPoint": "/usr/share/kicad/footprints/TestPoint.pretty",
    "Connector_FFC-FPC": "/usr/share/kicad/footprints/Connector_FFC-FPC.pretty",
}


def mm(v):
    return pcbnew.FromMM(v)


def add_outline(board, w=25.0, h=27.0, r=2.0, x0=0.0, y0=0.0):
    """Rounded-rect board outline on Edge.Cuts (segments + arcs)."""
    edge = pcbnew.Edge_Cuts
    def seg(x1, y1, x2, y2):
        s = pcbnew.PCB_SHAPE(board)
        s.SetShape(pcbnew.SHAPE_T_SEGMENT)
        s.SetStart(pcbnew.VECTOR2I(mm(x1), mm(y1)))
        s.SetEnd(pcbnew.VECTOR2I(mm(x2), mm(y2)))
        s.SetLayer(edge)
        s.SetWidth(mm(0.1))
        board.Add(s)
    def arc(cx, cy, sx, sy, ex, ey):
        s = pcbnew.PCB_SHAPE(board)
        s.SetShape(pcbnew.SHAPE_T_ARC)
        s.SetCenter(pcbnew.VECTOR2I(mm(cx), mm(cy)))
        s.SetStart(pcbnew.VECTOR2I(mm(sx), mm(sy)))
        s.SetEnd(pcbnew.VECTOR2I(mm(ex), mm(ey)))
        s.SetLayer(edge)
        s.SetWidth(mm(0.1))
        board.Add(s)
    L, T, R, B = x0, y0, x0 + w, y0 + h
    seg(L + r, T, R - r, T)        # top
    seg(R, T + r, R, B - r)        # right
    seg(R - r, B, L + r, B)        # bottom
    seg(L, B - r, L, T + r)        # left
    arc(L + r, T + r, L + r, T, L, T + r)   # TL
    arc(R - r, T + r, R, T + r, R - r, T)   # TR
    arc(R - r, B - r, R - r, B, R, B - r)   # BR
    arc(L + r, B - r, L, B - r, L + r, B)   # BL


def main():
    board = pcbnew.CreateEmptyBoard()

    ds = board.GetDesignSettings()
    ds.SetBoardThickness(mm(1.0))
    # JLCPCB economic 2-layer capability
    ds.m_TrackMinWidth = mm(0.127)
    ds.m_MinClearance = mm(0.127)
    try:
        ds.m_ViasMinSize = mm(0.45)
        ds.m_MinThroughDrill = mm(0.3)
    except AttributeError:
        pass

    # 2 copper layers
    board.SetCopperLayerCount(2)

    nets = {}
    def get_net(name):
        if name not in nets:
            n = pcbnew.NETINFO_ITEM(board, name)
            board.Add(n)
            nets[name] = n
        return nets[name]

    placed, missing = 0, []
    for ref, lib_id, value, footprint, (sx, sy), pin_nets in INSTANCES:
        lib, fpname = footprint.split(":")
        path = LIB_PATHS.get(lib)
        fp = pcbnew.FootprintLoad(path, fpname) if path else None
        if fp is None:
            missing.append((ref, footprint))
            continue
        fp.SetReference(ref)
        fp.SetValue(value)
        # spread on the schematic grid (scaled) so nothing overlaps
        fp.SetPosition(pcbnew.VECTOR2I(mm(sx + 60.0), mm(sy)))
        board.Add(fp)
        placed += 1
        for pad in fp.Pads():
            num = pad.GetNumber()
            net = pin_nets.get(num)
            if net and net != "NC":
                pad.SetNet(get_net(net))
    add_outline(board)

    pcbnew.SaveBoard(OUT, board)
    print(f"wrote {os.path.normpath(OUT)}  placed={placed}  nets={len(nets)}")
    if missing:
        print("MISSING FOOTPRINTS:")
        for r, f in missing:
            print(f"  {r}: {f}")


if __name__ == "__main__":
    main()

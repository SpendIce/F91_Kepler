"""Remove deterministic routing artifacts that block manufacturing checks.

Usage: python3 cleanup_route_artifacts.py BOARD.kicad_pcb
"""

import sys

import pcbnew


board = pcbnew.LoadBoard(sys.argv[1])


def point(x_mm, y_mm):
    return pcbnew.VECTOR2I(pcbnew.FromMM(x_mm), pcbnew.FromMM(y_mm))


def at(position, x_mm, y_mm):
    return position == point(x_mm, y_mm)


# The router emitted two VDDS vias 0.141 mm apart plus a joining segment.
# Collapse that cluster to the existing 17.3, 9.8 via and retarget copper.
vdds_target = point(17.3, 9.8)
for item in list(board.GetTracks()):
    if item.GetNetname() != "VDDS":
        continue
    if item.GetClass() == "PCB_VIA" and at(item.GetPosition(), 17.2, 9.7):
        board.Delete(item)
        continue
    if item.GetClass() != "PCB_TRACK":
        continue
    start_at_old = at(item.GetStart(), 17.2, 9.7)
    end_at_old = at(item.GetEnd(), 17.2, 9.7)
    if start_at_old and at(item.GetEnd(), 17.3, 9.8):
        board.Delete(item)
    elif end_at_old and at(item.GetStart(), 17.3, 9.8):
        board.Delete(item)
    else:
        if start_at_old:
            item.SetStart(vdds_target)
        if end_at_old:
            item.SetEnd(vdds_target)

# This one-cell In2.Cu BTN_1 segment terminates in free space and has no via.
for item in list(board.GetTracks()):
    if (
        item.GetClass() == "PCB_TRACK"
        and item.GetNetname() == "BTN_1"
        and item.GetLayer() == pcbnew.In2_Cu
        and (
            (at(item.GetStart(), 7.0, 11.8) and at(item.GetEnd(), 7.1, 11.9))
            or (at(item.GetEnd(), 7.0, 11.8) and at(item.GetStart(), 7.1, 11.9))
        )
    ):
        board.Delete(item)

pcbnew.ZONE_FILLER(board).Fill(board.Zones())
board.Save(sys.argv[1])

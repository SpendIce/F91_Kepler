"""Create a two-sided, courtyard-aware F91 Kepler v2 placement candidate.

This script deliberately removes existing routing from the output candidate.
It is a placement tool, not an autorouter. RF, crystal, LCD, button and MCU
geometry stays on the front; complete functional blocks move together to the
back so their local decoupling remains on the same side as the IC.

Usage:
    python3 tools/place_double_sided.py input.kicad_pcb output.kicad_pcb
"""

from __future__ import annotations

import math
import os
import sys

import pcbnew


SOURCE, OUTPUT = sys.argv[1:3]
board = pcbnew.LoadBoard(SOURCE)


def iu(value_mm: float) -> int:
    return pcbnew.FromMM(value_mm)


def mm(value_iu: int) -> float:
    return pcbnew.ToMM(value_iu)


footprints = {fp.GetReference(): fp for fp in board.GetFootprints()}

# Keep the PCB net assignment synchronized with the routing-oriented LCD pin
# order recorded in netlist.py/PINMAP.md. The source board may predate it.
lcd_pad_nets = {
    "39": "LCD_SCLK",
    "40": "LCD_MOSI",
    "41": "LCD_CS",
    "42": "LCD_EXTCOMIN",
    "43": "LCD_DISP",
}
for pad in footprints["U1"].Pads():
    if pad.GetNumber() in lcd_pad_nets:
        pad.SetNet(board.FindNet(lcd_pad_nets[pad.GetNumber()]))

# Bare contact pads have no placed component body and therefore need no
# assembly courtyard. Keeping the generic test-point courtyard creates false
# pick-and-place overlap errors in mechanically fixed pogo/contact rows. Strip
# it before flipping footprints; KiCad 10's SWIG drawing iterator is unstable
# after a footprint has changed sides.
contact_refs = {"BT1", *(f"TP{number}" for number in range(1, 17))}
contact_graphics = {
    ref: list(footprints[ref].GraphicalItems())
    for ref in contact_refs
    if ref in footprints
}
for ref, graphics in contact_graphics.items():
    fp = footprints[ref]
    for graphic in graphics:
        if graphic.GetLayer() in (pcbnew.F_CrtYd, pcbnew.B_CrtYd):
            fp.Remove(graphic)

# The watch PCB has no usable area for per-footprint silkscreen. Keep Fab
# layers/BOM/position data authoritative and suppress overlapping production
# references, values and generic package outlines.
all_graphics = {ref: list(fp.GraphicalItems()) for ref, fp in footprints.items()}
for ref, fp in footprints.items():
    fp.Reference().SetVisible(False)
    fp.Value().SetVisible(False)
    for graphic in all_graphics[ref]:
        if graphic.GetLayer() in (pcbnew.F_SilkS, pcbnew.B_SilkS):
            fp.Remove(graphic)

# Parts that must remain on the front because they face the display/case, form
# a sensitive local network, or require an uninterrupted ground reference.
FRONT_FIXED = {
    "U1": (12.5, 13.5, 0),
    "J1": (12.5, 4.5, 180),
    "AE1": (4.3, 1.9, 180),
    "FL1": (6.9, 10.9, 180),
    "R2": (6.0, 4.0, 90),
    "C14": (4.35, 5.0, 180),
    "C15": (4.35, 3.8, 180),
    "L2": (17.85, 12.4, 90),
    "Y1": (18.8, 19.0, 90),
    "Y2": (5.5, 15.5, 0),
    "SW1": (2.2, 9.0, 270),
    "SW2": (22.8, 7.0, 90),
    "SW3": (22.8, 19.0, 90),
}

# Rear-facing contacts are mechanically pinned. Component placement is kept
# inside the central pocket so the perimeter remains available for the NFC
# loop and an external Qi coil/ferrite assembly.
BACK_FIXED = {
    "BT1": (12.5, 3.5, 0),
    "TP1": (5.0, 25.5, 0),
    "TP2": (7.37, 25.5, 0),
    "TP3": (9.93, 25.5, 0),
    "TP4": (12.5, 25.5, 0),
    "TP5": (15.07, 25.5, 0),
    "TP6": (17.63, 25.5, 0),
    "TP7": (20.0, 25.5, 0),
    "TP9": (2.2, 10.0, 0),
    "TP10": (2.2, 12.6, 0),
    "TP13": (22.8, 10.0, 0),
    "TP14": (22.8, 12.6, 0),
    "TP15": (22.8, 20.0, 0),
    "TP16": (22.6, 22.0, 0),
}

BACK_BLOCKS = {
    "U2": {"C16", "C17", "C18", "C19", "C20", "C21", "C22", "C23",
           "C24", "C25", "C26", "C27", "C28", "R5", "R6", "R7", "R8",
           "W1", "TP11", "TP12"},
    "U3": {"Q1", "C31", "R10", "R11"},
    "U4": {"C29", "C30", "C32", "C33"},
    "U6": {"C36", "C37", "TP8"},
    "U7": {"C38", "R12", "R13", "R14"},
}

back_refs = set(BACK_FIXED)
for anchor, members in BACK_BLOCKS.items():
    back_refs.add(anchor)
    back_refs.update(members)

front_refs = set(footprints) - back_refs

HOME = {
    **{ref: "U2" for ref in BACK_BLOCKS["U2"]},
    **{ref: "U3" for ref in BACK_BLOCKS["U3"]},
    **{ref: "U4" for ref in BACK_BLOCKS["U4"]},
    **{ref: "U6" for ref in BACK_BLOCKS["U6"]},
    **{ref: "U7" for ref in BACK_BLOCKS["U7"]},
    "C2": "U1", "C3": "U1", "C4": "U1", "C5": "U1", "C6": "U1",
    "C7": "U1", "C8": "U1", "C9": "U1", "C10": "U1", "C11": "U1",
    "R1": "U1", "R3": "U1", "R4": "U1", "R9": "U1", "FB1": "U1",
    "C12": "Y2", "C13": "Y2", "C14": "AE1", "C15": "AE1",
    "C34": "U5", "C35": "U5", "C39": "J1", "C40": "J1",
}

ANCHOR_HINTS = {
    "U2": (12.5, 17.8),
    "U3": (17.5, 12.0),
    "Q1": (17.5, 8.5),
    "U4": (12.5, 8.0),
    "U6": (7.5, 17.0),
    "U7": (7.5, 10.0),
    "U5": (5.5, 12.0),
}

FRONT_REGION = (0.55, 0.55, 24.45, 26.45)
BACK_REGION = (4.7, 4.7, 20.3, 22.3)
GRID = 0.25
COURTYARD_GAP = 0.05


def set_side(fp: pcbnew.FOOTPRINT, back: bool) -> None:
    wanted = pcbnew.B_Cu if back else pcbnew.F_Cu
    if fp.GetLayer() != wanted:
        fp.Flip(fp.GetPosition(), False)


def place(fp: pcbnew.FOOTPRINT, x: float, y: float, angle: float) -> None:
    fp.SetPosition(pcbnew.VECTOR2I(iu(x), iu(y)))
    fp.SetOrientationDegrees(angle)


def layer_bbox(fp: pcbnew.FOOTPRINT) -> tuple[float, float, float, float]:
    layer = pcbnew.B_CrtYd if fp.GetLayer() == pcbnew.B_Cu else pcbnew.F_CrtYd
    layers = pcbnew.LSET()
    layers.AddLayer(layer)
    box = fp.GetLayerBoundingBox(layers)
    if box.GetWidth() == 0 or box.GetHeight() == 0:
        box = fp.GetBoundingBox(False, False)
    return mm(box.GetLeft()), mm(box.GetTop()), mm(box.GetRight()), mm(box.GetBottom())


def physical_bbox(fp: pcbnew.FOOTPRINT) -> tuple[float, float, float, float]:
    box = fp.GetBoundingBox(False, False)
    return mm(box.GetLeft()), mm(box.GetTop()), mm(box.GetRight()), mm(box.GetBottom())


def inside_outline(box: tuple[float, float, float, float]) -> bool:
    left, top, right, bottom = box
    if left < 0.55 or top < 0.55 or right > 24.45 or bottom > 26.45:
        return False
    for cx, cy in ((2, 2), (23, 2), (2, 25), (23, 25)):
        nearest_x = min(max(cx, left), right)
        nearest_y = min(max(cy, top), bottom)
        if math.hypot(nearest_x - cx, nearest_y - cy) < 1.45:
            return False
    return True


def overlaps(a: tuple[float, float, float, float],
             b: tuple[float, float, float, float]) -> bool:
    return (a[0] < b[2] + COURTYARD_GAP and a[2] + COURTYARD_GAP > b[0]
            and a[1] < b[3] + COURTYARD_GAP and a[3] + COURTYARD_GAP > b[1])


occupied = {False: [], True: []}
positions: dict[str, tuple[float, float]] = {}


def occupy(ref: str) -> None:
    fp = footprints[ref]
    occupied[fp.GetLayer() == pcbnew.B_Cu].append(layer_bbox(fp))
    pos = fp.GetPosition()
    positions[ref] = (mm(pos.x), mm(pos.y))


def try_near(ref: str, hint: tuple[float, float], back: bool) -> bool:
    fp = footprints[ref]
    region = BACK_REGION if back else FRONT_REGION
    candidates = []
    x = region[0]
    while x <= region[2]:
        y = region[1]
        while y <= region[3]:
            candidates.append((x, y))
            y += GRID
        x += GRID
    candidates.sort(key=lambda p: (p[0] - hint[0]) ** 2 + (p[1] - hint[1]) ** 2)
    for angle in (0, 90, 180, 270):
        for x, y in candidates:
            place(fp, x, y, angle)
            box = layer_bbox(fp)
            if not inside_outline(physical_bbox(fp)):
                continue
            if any(overlaps(box, other) for other in occupied[back]):
                continue
            occupy(ref)
            return True
    return False


for ref, fp in footprints.items():
    set_side(fp, ref in back_refs)

for fixed, back in ((FRONT_FIXED, False), (BACK_FIXED, True)):
    for ref, (x, y, angle) in fixed.items():
        place(footprints[ref], x, y, angle)
        occupy(ref)

# Preserve a direct fanout channel between the LCD FPC and the five adjacent
# MCU pads. No passives or unrelated vias may consume this narrow corridor.
occupied[False].append((11.7, 6.0, 15.3, 10.4))

movable = set(footprints) - set(FRONT_FIXED) - set(BACK_FIXED)

# Place ICs and other large footprints before their local passives.
ordered = sorted(
    movable,
    key=lambda ref: (
        0 if ref in ANCHOR_HINTS else 1,
        -footprints[ref].GetBoundingBox(False, False).GetArea(),
        ref,
    ),
)

failed = []
for ref in ordered:
    home = HOME.get(ref)
    hint = ANCHOR_HINTS.get(ref)
    if hint is None and home in positions:
        hint = positions[home]
    if hint is None and home in ANCHOR_HINTS:
        hint = ANCHOR_HINTS[home]
    if hint is None:
        hint = (12.5, 13.5)
    if not try_near(ref, hint, ref in back_refs):
        failed.append(ref)

# The U7 pull-up cluster needs an explicit manufacturing override.  At the
# generic nearest-free position R14's SCL pad blocks the only legal F/B +3V0
# transition.  This verified offset preserves courtyard clearance and opens a
# 0.40/0.20 mm via pocket between SCL and C34.
place(footprints["R14"], 5.0, 6.65, 0)

# Preserve the verified right-side +3V0 corridor and separate MOT_P/MOT_N.
place(footprints["U6"], 5.9, 16.95, 0)

# Placement candidates must not retain tracks tied to the old coordinates.
for item in list(board.GetTracks()):
    board.Remove(item)
pcbnew.ZONE_FILLER(board).Fill(board.Zones())

board.Save(OUTPUT)
print(f"front={len(front_refs)} back={len(back_refs)} failed={len(failed)}")
if failed:
    print("unplaced:", " ".join(failed))
    raise SystemExit(1)
sys.stdout.flush()
# KiCad 10's Arch SWIG binding can crash while destructing removed track
# wrappers. The board is already serialized; bypass interpreter teardown.
os._exit(0)

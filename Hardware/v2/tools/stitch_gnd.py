"""stitch_gnd.py — GND stitching via grid.

Places 0.46/0.20 GND vias on a ~2.2 mm grid wherever a via fits clear of
all copper (hard pads/locked + unlocked tracks of any net, all 4 layers'
worth of routing layers) and inside the board minus edge margin. Connects
every isolated F.Cu GND pour fragment to the In1 plane and stiffens RF
return paths.

Usage: python3 stitch_gnd.py BOARD.kicad_pcb
"""
import sys, math
import numpy as np
import pcbnew

BOARD = sys.argv[1] if len(sys.argv) > 1 else "f91_kepler_v2.kicad_pcb"
RES = 0.05
W, H = 25.0, 27.0
NX, NY = int(W / RES) + 1, int(H / RES) + 1
CLR = 0.15
VIA_D, VIA_DRILL = 0.46, 0.20
EDGE = 0.30
PITCH = 2.2

b = pcbnew.LoadBoard(BOARD)
mm = pcbnew.ToMM
MM = pcbnew.FromMM
KLAYER = {pcbnew.F_Cu: 0, pcbnew.In2_Cu: 1, pcbnew.B_Cu: 2}
GND = b.FindNet("GND").GetNetCode()

blk = np.zeros((NY, NX), bool)   # any-layer blockage for a GND via

def block_disc(cx, cy, r):
    x0, x1 = max(0, int((cx - r) / RES)), min(NX - 1, int((cx + r) / RES) + 1)
    y0, y1 = max(0, int((cy - r) / RES)), min(NY - 1, int((cy + r) / RES) + 1)
    xs, ys = np.meshgrid(np.arange(x0, x1 + 1), np.arange(y0, y1 + 1))
    d2 = (xs * RES - cx) ** 2 + (ys * RES - cy) ** 2
    blk[ys[d2 <= r * r], xs[d2 <= r * r]] = True

def block_seg(x1, y1, x2, y2, r):
    n = max(2, int(math.hypot(x2 - x1, y2 - y1) / RES) + 1)
    for i in range(n + 1):
        t = i / n
        block_disc(x1 + (x2 - x1) * t, y1 + (y2 - y1) * t, r)

R_VIA = VIA_D / 2 + CLR          # via copper + clearance to foreign copper
for fp in b.GetFootprints():
    for p in fp.Pads():
        bb = p.GetBoundingBox()
        gnd_pad = p.GetNetCode() == GND
        margin = R_VIA if not gnd_pad else VIA_D / 2   # may touch GND pads? keep off anyway
        x0, y0 = mm(bb.GetLeft()) - margin, mm(bb.GetTop()) - margin
        x1, y1 = mm(bb.GetRight()) + margin, mm(bb.GetBottom()) + margin
        ix0, ix1 = max(0, int(x0 / RES)), min(NX - 1, int(x1 / RES) + 1)
        iy0, iy1 = max(0, int(y0 / RES)), min(NY - 1, int(y1 / RES) + 1)
        blk[iy0:iy1 + 1, ix0:ix1 + 1] = True

for t in b.GetTracks():
    r = R_VIA + mm(t.GetWidth()) / 2
    if t.GetNetCode() == GND:
        r = VIA_D / 2 + mm(t.GetWidth()) / 2 + 0.02   # same net: only avoid overlap noise
    if t.GetClass() == "PCB_VIA":
        block_disc(mm(t.GetStart().x), mm(t.GetStart().y),
                   R_VIA + mm(t.GetWidth()) / 2)
    else:
        if t.GetLayer() not in KLAYER:
            continue
        block_seg(mm(t.GetStart().x), mm(t.GetStart().y),
                  mm(t.GetEnd().x), mm(t.GetEnd().y), r)

# edge + corner arcs + antenna keepout (7x5 top-left)
m = int((EDGE + VIA_D / 2) / RES)
blk[:m, :] = True; blk[-m:, :] = True; blk[:, :m] = True; blk[:, -m:] = True
for cx, cy in ((2, 2), (23, 2), (2, 25), (23, 25)):
    for iy in range(NY):
        for ix in range(NX):
            pass
for cx, cy in ((2, 2), (23, 2), (2, 25), (23, 25)):
    x0 = 0 if cx == 2 else int(cx / RES)
    x1 = int(cx / RES) + 1 if cx == 2 else NX
    y0 = 0 if cy == 2 else int(cy / RES)
    y1 = int(cy / RES) + 1 if cy == 2 else NY
    for iy in range(y0, min(y1, NY)):
        for ix in range(x0, min(x1, NX)):
            if math.hypot(ix * RES - cx, iy * RES - cy) > 2.0 - EDGE - VIA_D / 2:
                blk[iy, ix] = True
blk[:int(5.3 / RES), :int(7.3 / RES)] = True   # ANT keepout + margin

added = 0
for gy in np.arange(1.0, H - 0.5, PITCH):
    for gx in np.arange(1.0, W - 0.5, PITCH):
        # search nearest free cell within 0.8 mm of grid point
        ix, iy = int(gx / RES), int(gy / RES)
        best = None
        Rc = int(0.8 / RES)
        for dy in range(-Rc, Rc + 1):
            for dx in range(-Rc, Rc + 1):
                jx, jy = ix + dx, iy + dy
                if 0 <= jx < NX and 0 <= jy < NY and not blk[jy, jx]:
                    d2 = dx * dx + dy * dy
                    if best is None or d2 < best[0]:
                        best = (d2, jx, jy)
        if best is None:
            continue
        _, jx, jy = best
        v = pcbnew.PCB_VIA(b)
        v.SetPosition(pcbnew.VECTOR2I(MM(jx * RES), MM(jy * RES)))
        v.SetWidth(MM(VIA_D)); v.SetDrill(MM(VIA_DRILL))
        v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
        v.SetNetCode(GND)
        b.Add(v)
        block_disc(jx * RES, jy * RES, VIA_D + CLR)
        added += 1
print("stitching vias added:", added)
pcbnew.ZONE_FILLER(b).Fill(b.Zones())
b.Save(BOARD)
print("saved")

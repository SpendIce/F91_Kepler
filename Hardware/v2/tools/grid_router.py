"""grid_router.py — A* grid router with rip-up & retry, for the connections
freerouting leaves behind.

3 routing layers (F.Cu / In2.Cu / B.Cu; In1.Cu is the GND plane), 0.05 mm
grid. Two obstacle classes:
  hard: pads, locked tracks/vias, board edge  -> never crossed
  soft: unlocked tracks/vias of other nets    -> crossable at a penalty;
        crossed nets get ripped (their unlocked copper deleted) and are
        re-routed on the next run (DRC -> rerun until stable).
Vias 0.46/0.20 (JLC standard tier), need a hard-free disc on all 3 routing
layers; soft copper under a via joins the rip set.

Corner note: KiCad measures Edge.Cuts arc clearance against the FULL r=2
circle (verified 2026-07-03), so tracks crossing the on-board part of a
corner annulus DRC-flag as false positives. We route through anyway and the
fab pipeline adds verified DRC exclusions for them.

Usage: python3 grid_router.py BOARD.kicad_pcb DRC_UNCONNECTED.json
Re-run (DRC in between) until unconnected count stabilizes.
"""
import sys, json, re, heapq, math
import numpy as np
import pcbnew

BOARD, DRCJSON = sys.argv[1], sys.argv[2]
RES = 0.05                      # mm per cell
W, H = 25.0, 27.0
NX, NY = int(W / RES) + 1, int(H / RES) + 1
CLR = 0.15
VIA_D, VIA_DRILL = 0.46, 0.20   # JLC: annular 0.13 exact, drill 0.2
EDGE = 0.30                     # copper-edge margin (rule 0.22)
GROW = CLR + 0.10 + 0.04        # tracks/vias: clearance + half-width + quantization
PAD_GROW = CLR + 0.10 + 0.01    # pads: tight margin — 0.5-pitch QFN escape gap is
                                # 0.275 vs 0.25 required; +0.04 would wall it off
SOFT_PEN = 12.0                 # extra cost per soft-blocked cell
MAX_RIP = 4                     # skip paths that would rip more nets than this
RIP_CAP = 3                     # nets ripped this many times become un-rippable
HISTFILE = "/tmp/claude-1000/-home-spendice-Documents-F91-Kepler/7705a934-d92f-4e5f-a5fd-73570a1a964a/scratchpad/ripcount.json"

b = pcbnew.LoadBoard(BOARD)
def mm(v): return pcbnew.ToMM(v)
MM = pcbnew.FromMM
KLAYER = {pcbnew.F_Cu: 0, pcbnew.In2_Cu: 1, pcbnew.B_Cu: 2}
RLAYER = {0: pcbnew.F_Cu, 1: pcbnew.In2_Cu, 2: pcbnew.B_Cu}
FCU, IN2, BCU = 0, 1, 2
ALL_L = (FCU, IN2, BCU)

pwr = {"VBAT", "BATN", "+3V0", "VDDS", "VDDR", "DCDC_SW", "DCOUPL", "MOT_P",
       "MOT_N", "PIEZO", "AC1", "AC2", "RECT", "COMM1", "CLAMP1", "BOOT2",
       "DRV_REG", "CLAMP2", "COMM2", "COIL1", "COIL2", "DW_VCC", "BOOT1"}

def width_for(net):
    return 0.35 if net in pwr else 0.20

# --- grids: value = net code owning the cell, -1 free, -2 hard multi-net ---
hard = {i: np.full((NY, NX), -1, np.int32) for i in ALL_L}
soft = {i: np.full((NY, NX), -1, np.int32) for i in ALL_L}

def cells_disc(cx, cy, r):
    x0, x1 = max(0, int((cx - r) / RES)), min(NX - 1, int((cx + r) / RES) + 1)
    y0, y1 = max(0, int((cy - r) / RES)), min(NY - 1, int((cy + r) / RES) + 1)
    xs, ys = np.meshgrid(np.arange(x0, x1 + 1), np.arange(y0, y1 + 1))
    d2 = (xs * RES - cx) ** 2 + (ys * RES - cy) ** 2
    return ys[d2 <= r * r], xs[d2 <= r * r]

def cells_rect(x0, y0, x1, y1, grow):
    # only cells whose CENTER lies inside the grown rect (no overshoot:
    # fine-pitch pin-escape corridors are ~0.03 wider than the inflation)
    x0, y0, x1, y1 = x0 - grow, y0 - grow, x1 + grow, y1 + grow
    ix0, ix1 = max(0, math.ceil(x0 / RES)), min(NX - 1, math.floor(x1 / RES))
    iy0, iy1 = max(0, math.ceil(y0 / RES)), min(NY - 1, math.floor(y1 / RES))
    return slice(iy0, iy1 + 1), slice(ix0, ix1 + 1)

def cells_seg(x1, y1, x2, y2, r):
    n = max(2, int(math.hypot(x2 - x1, y2 - y1) / RES) + 1)
    ys_all, xs_all = [], []
    for i in range(n + 1):
        t = i / n
        cy, cx = cells_disc(x1 + (x2 - x1) * t, y1 + (y2 - y1) * t, r)
        ys_all.append(cy); xs_all.append(cx)
    return np.concatenate(ys_all), np.concatenate(xs_all)

def paint(grid, layer, ys, xs, code):
    g = grid[layer]
    cur = g[ys, xs]
    conflict = (cur != -1) & (cur != code)
    g[ys, xs] = np.where(conflict, -2, code)

print("building obstacle grids...")
for fp in b.GetFootprints():
    for p in fp.Pads():
        bb = p.GetBoundingBox()
        code = p.GetNetCode() if p.GetNetname() else -2
        sl = cells_rect(mm(bb.GetLeft()), mm(bb.GetTop()),
                        mm(bb.GetRight()), mm(bb.GetBottom()), PAD_GROW)
        for layer in ALL_L if p.GetAttribute() == pcbnew.PAD_ATTRIB_PTH else (FCU,):
            g = hard[layer]
            cur = g[sl]
            g[sl] = np.where((cur != -1) & (cur != code), -2, code)

GNDCODE = b.FindNet("GND").GetNetCode()
for t in b.GetTracks():
    code = t.GetNetCode()
    # unlocked GND (stitching vias, plane taps) is infrastructure: hard,
    # never ripped — rip("GND") would nuke all of it board-wide
    grid = hard if (t.IsLocked() or code == GNDCODE) else soft
    if t.GetClass() == "PCB_VIA":
        x, y = mm(t.GetStart().x), mm(t.GetStart().y)
        ys, xs = cells_disc(x, y, mm(t.GetWidth()) / 2 + GROW)
        for L in ALL_L:
            paint(grid, L, ys, xs, code)
    else:
        layer = KLAYER.get(t.GetLayer())
        if layer is None:
            continue
        ys, xs = cells_seg(mm(t.GetStart().x), mm(t.GetStart().y),
                           mm(t.GetEnd().x), mm(t.GetEnd().y),
                           mm(t.GetWidth()) / 2 + GROW)
        paint(grid, layer, ys, xs, code)

# board edge + off-board corner regions (hard on all layers)
edge_cells = int(EDGE / RES)
for g in hard.values():
    g[:edge_cells, :] = -2; g[-edge_cells:, :] = -2
    g[:, :edge_cells] = -2; g[:, -edge_cells:] = -2
for cx, cy in ((2, 2), (23, 2), (2, 25), (23, 25)):
    x0 = 0.0 if cx < W / 2 else cx
    x1 = cx if cx < W / 2 else W
    y0 = 0.0 if cy < H / 2 else cy
    y1 = cy if cy < H / 2 else H
    for iy in range(int(y0 / RES), min(NY - 1, int(y1 / RES)) + 1):
        for ix in range(int(x0 / RES), min(NX - 1, int(x1 / RES)) + 1):
            if math.hypot(ix * RES - cx, iy * RES - cy) > 2.0 - EDGE:
                for L in ALL_L:
                    hard[L][iy, ix] = -2

# --- unconnected list from DRC json ---
def item_layers(desc):
    m = re.search(r"on (F|In1|In2|B)\.Cu", desc)
    if m:
        l = {"F": FCU, "In2": IN2, "B": BCU}.get(m.group(1))
        return (l,) if l is not None else ALL_L
    if desc.startswith("Via") or "PTH" in desc:
        return ALL_L
    return (FCU,)

import os
ripcount = {}
if os.path.exists(HISTFILE):
    ripcount = json.load(open(HISTFILE))

d = json.load(open(DRCJSON))
conns = []
zone_stubs = []     # (net, (x,y), layers): item pairs vs a Zone (bogus pos 0,0)
for u in d.get("unconnected_items", []):
    net = re.search(r"\[(.*?)\]", u["items"][0]["description"]).group(1)
    items = u["items"]
    if len(items) != 2:
        continue
    zi = [i for i in items if i["description"].startswith("Zone")]
    ni = [i for i in items if not i["description"].startswith("Zone")]
    if len(zi) == 2:
        continue        # pour-fragment pair: fixed by fragment stitching pass
    if zi and ni:
        zone_stubs.append((net, (ni[0]["pos"]["x"], ni[0]["pos"]["y"]),
                           item_layers(ni[0]["description"])))
        continue
    pts = [((i["pos"]["x"], i["pos"]["y"]), item_layers(i["description"]))
           for i in items]
    conns.append((net, pts[0], pts[1]))

def prio(c):
    net = c[0]
    d2 = math.hypot(c[1][0][0] - c[2][0][0], c[1][0][1] - c[2][0][1])
    if ripcount.get(net, 0) > 0:
        return (-2 - ripcount[net], d2)   # repeat victims route first
    if net.startswith("LCD_"):
        group = -1
    elif net == "GND":
        group = 0
    elif net in pwr:
        group = 1
    else:
        group = 2
    return (group, d2)
conns.sort(key=prio)
print(f"{len(conns)} connections to route")

DIRS = [(1, 0, 1.0), (-1, 0, 1.0), (0, 1, 1.0), (0, -1, 1.0),
        (1, 1, 1.414), (1, -1, 1.414), (-1, 1, 1.414), (-1, -1, 1.414)]
VIA_COST = 40

def dilate(mask, r):
    out = mask.copy()
    for dy in range(-r, r + 1):
        for dx in range(-r, r + 1):
            if dx * dx + dy * dy > r * r or (dx == 0 and dy == 0):
                continue
            sh = np.zeros_like(mask)
            ys0, ys1 = max(0, dy), NY + min(0, dy)
            xs0, xs1 = max(0, dx), NX + min(0, dx)
            sh[ys0:ys1, xs0:xs1] = mask[max(0, -dy):NY + min(0, -dy),
                                        max(0, -dx):NX + min(0, -dx)]
            out |= sh
    return out

def astar(code, sx, sy, gx, gy, hw, s_layers, g_layers, allow_soft):
    six, siy = int(round(sx / RES)), int(round(sy / RES))
    gix, giy = int(round(gx / RES)), int(round(gy / RES))
    grow_extra = int(max(0, hw - 0.10) / RES + 0.5)
    blkh, blks = {}, {}
    for l in ALL_L:
        mh = (hard[l] != -1) & (hard[l] != code)
        ms = (soft[l] != -1) & (soft[l] != code)
        blkh[l] = dilate(mh, grow_extra) if grow_extra else mh
        blks[l] = dilate(ms, grow_extra) if grow_extra else ms
    vr = int(math.ceil((VIA_D / 2 + CLR - GROW) / RES))
    viahard = dilate(blkh[FCU] | blkh[IN2] | blkh[BCU], max(2, vr + 1))
    viasoft = dilate(blks[FCU] | blks[IN2] | blks[BCU], max(2, vr + 1))
    h = lambda ix, iy: math.hypot(ix - gix, iy - giy)
    openq = []
    came, gcost = {}, {}
    for sl in s_layers:
        st = (six, siy, sl)
        gcost[st] = 0.0
        heapq.heappush(openq, (h(six, siy), 0.0, st, None))
    seen = set()
    expansions = 0
    while openq:
        f, gc, node, parent = heapq.heappop(openq)
        if node in seen:
            continue
        seen.add(node)
        came[node] = parent
        expansions += 1
        if expansions > 1500000:
            return None
        ix, iy, l = node
        if (ix, iy) == (gix, giy) and l in g_layers:
            path = []
            while node:
                path.append(node)
                node = came[node]
            return path[::-1]
        BH, BS = blkh[l], blks[l]
        for dx, dy, cost in DIRS:
            nx_, ny_ = ix + dx, iy + dy
            if not (0 <= nx_ < NX and 0 <= ny_ < NY):
                continue
            nn = (nx_, ny_, l)
            if nn in seen or BH[ny_, nx_]:
                continue
            if dx and dy and (BH[iy, nx_] or BH[ny_, ix]):
                continue   # no corner cutting through diagonal hard gaps
            step = cost
            if BS[ny_, nx_]:
                if not allow_soft:
                    continue
                step += SOFT_PEN
            elif dx and dy and (BS[iy, nx_] or BS[ny_, ix]):
                if allow_soft:
                    step += SOFT_PEN / 2
                else:
                    continue
            ng = gc + step
            if ng < gcost.get(nn, 1e18):
                gcost[nn] = ng
                heapq.heappush(openq, (ng + h(nx_, ny_), ng, nn, node))
        if not viahard[iy, ix]:
            via_soft_pen = SOFT_PEN * 3 if viasoft[iy, ix] else 0
            if via_soft_pen == 0 or allow_soft:
                for ol in ALL_L:
                    if ol == l:
                        continue
                    nn = (ix, iy, ol)
                    ng = gc + VIA_COST + via_soft_pen
                    if nn not in seen and ng < gcost.get(nn, 1e18):
                        gcost[nn] = ng
                        heapq.heappush(openq, (ng + h(ix, iy), ng, nn, node))
    return None

def rip_codes_for(path, hw):
    codes = set()
    r = int((hw + CLR) / RES) + 1
    for (x, y, l) in path:
        y0, y1 = max(0, y - r), min(NY - 1, y + r)
        x0, x1 = max(0, x - r), min(NX - 1, x + r)
        region = soft[l][y0:y1 + 1, x0:x1 + 1]
        codes.update(int(c) for c in np.unique(region) if c >= 0)
    # vias touch every layer
    for i in range(len(path) - 1):
        if path[i][2] != path[i + 1][2]:
            x, y = path[i][0], path[i][1]
            rr = int((VIA_D / 2 + CLR) / RES) + 1
            y0, y1 = max(0, y - rr), min(NY - 1, y + rr)
            x0, x1 = max(0, x - rr), min(NX - 1, x + rr)
            for L in ALL_L:
                region = soft[L][y0:y1 + 1, x0:x1 + 1]
                codes.update(int(c) for c in np.unique(region) if c >= 0)
    return codes

def rip(codes):
    names = []
    for c in codes:
        for L in ALL_L:
            g = soft[L]
            g[g == c] = -1
        for t in list(b.GetTracks()):
            if not t.IsLocked() and t.GetNetCode() == c:
                b.Delete(t)
        names.append(b.FindNet(c).GetNetname())
    return names

def emit(code, path, hw):
    segs, vias = [], []
    i = 0
    while i < len(path) - 1:
        if path[i][2] != path[i + 1][2]:
            vias.append(path[i])
            i += 1
            continue
        j = i + 1
        dx, dy = path[j][0] - path[i][0], path[j][1] - path[i][1]
        while (j + 1 < len(path) and path[j + 1][2] == path[j][2]
               and (path[j + 1][0] - path[j][0], path[j + 1][1] - path[j][1]) == (dx, dy)):
            j += 1
        segs.append((path[i], path[j]))
        i = j
    for (x1, y1, l), (x2, y2, _) in segs:
        t = pcbnew.PCB_TRACK(b)
        t.SetStart(pcbnew.VECTOR2I(MM(x1 * RES), MM(y1 * RES)))
        t.SetEnd(pcbnew.VECTOR2I(MM(x2 * RES), MM(y2 * RES)))
        t.SetWidth(MM(hw * 2))
        t.SetLayer(RLAYER[l])
        t.SetNetCode(code)
        b.Add(t)
        ys, xs = cells_seg(x1 * RES, y1 * RES, x2 * RES, y2 * RES, hw + GROW)
        paint(soft, l, ys, xs, code)
    for (x, y, _) in vias:
        v = pcbnew.PCB_VIA(b)
        v.SetPosition(pcbnew.VECTOR2I(MM(x * RES), MM(y * RES)))
        v.SetWidth(MM(VIA_D)); v.SetDrill(MM(VIA_DRILL))
        v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
        v.SetNetCode(code)
        b.Add(v)
        ys, xs = cells_disc(x * RES, y * RES, VIA_D / 2 + GROW)
        for L in ALL_L:
            paint(soft, L, ys, xs, code)

def emit_direct(code, x1, y1, x2, y2, hw, layer):
    t = pcbnew.PCB_TRACK(b)
    t.SetStart(pcbnew.VECTOR2I(MM(x1), MM(y1)))
    t.SetEnd(pcbnew.VECTOR2I(MM(x2), MM(y2)))
    t.SetWidth(MM(hw * 2))
    t.SetLayer(RLAYER[layer])
    t.SetNetCode(code)
    b.Add(t)
    ys, xs = cells_seg(x1, y1, x2, y2, hw + GROW)
    paint(soft, layer, ys, xs, code)

ok = fail = 0
fails = []
ripped_names = set()
for net, ((x1, y1), sl), ((x2, y2), gl) in conns:
    if net in ripped_names:
        continue                    # its copper is gone; next run re-pairs it
    code = b.FindNet(net).GetNetCode()
    hw = width_for(net) / 2
    common = set(sl) & set(gl)
    if math.hypot(x2 - x1, y2 - y1) < 0.35 and common:
        # sub-grid gap (track end shy of a pad, etc.): bridge exactly
        emit_direct(code, x1, y1, x2, y2, min(hw, 0.1), min(common))
        ok += 1
        continue
    path = astar(code, x1, y1, x2, y2, hw, sl, gl, allow_soft=False)
    if path is None and hw > 0.1:
        path = astar(code, x1, y1, x2, y2, 0.1, sl, gl, allow_soft=False)
        hw = 0.1
    if path is None:
        hw2 = width_for(net) / 2
        path = astar(code, x1, y1, x2, y2, hw2, sl, gl, allow_soft=True)
        if path is None and hw2 > 0.1:
            path = astar(code, x1, y1, x2, y2, 0.1, sl, gl, allow_soft=True)
            hw2 = 0.1
        if path is not None:
            hw = hw2
            codes = rip_codes_for(path, hw)
            codes.discard(code)
            cand = [b.FindNet(c).GetNetname() for c in codes]
            if (len(codes) > MAX_RIP
                    or any(ripcount.get(n, 0) >= RIP_CAP for n in cand)):
                path = None
            else:
                names = rip(codes)
                ripped_names.update(names)
                for n in names:
                    ripcount[n] = ripcount.get(n, 0) + 1
                if names:
                    print(f"  {net}: ripped {names}")
    if path is None:
        fail += 1; fails.append((net, x1, y1, x2, y2))
        continue
    emit(code, path, hw)
    ok += 1
# zone-pair stubs: drop a via to the In1 GND plane next to the item
for net, (zx, zy), zl in zone_stubs:
    code = b.FindNet(net).GetNetCode()
    hw = width_for(net) / 2
    blkh = {l: (hard[l] != -1) & (hard[l] != code) for l in ALL_L}
    blks = {l: (soft[l] != -1) & (soft[l] != code) for l in ALL_L}
    vr = max(2, int(math.ceil((VIA_D / 2 + CLR - GROW) / RES)) + 1)
    viafree = ~dilate(blkh[FCU] | blkh[IN2] | blkh[BCU]
                      | blks[FCU] | blks[IN2] | blks[BCU], vr)
    ix, iy = int(round(zx / RES)), int(round(zy / RES))
    best = None
    R = int(1.5 / RES)
    for dy in range(-R, R + 1):
        for dx in range(-R, R + 1):
            jx, jy = ix + dx, iy + dy
            if 0 <= jx < NX and 0 <= jy < NY and viafree[jy, jx]:
                d2 = dx * dx + dy * dy
                if best is None or d2 < best[0]:
                    best = (d2, jx, jy)
    if best is None:
        print(f"  ZONE-STUB FAIL {net} ({zx},{zy})")
        fail += 1
        continue
    _, jx, jy = best
    v = pcbnew.PCB_VIA(b)
    v.SetPosition(pcbnew.VECTOR2I(MM(jx * RES), MM(jy * RES)))
    v.SetWidth(MM(VIA_D)); v.SetDrill(MM(VIA_DRILL))
    v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
    v.SetNetCode(code)
    b.Add(v)
    ys, xs = cells_disc(jx * RES, jy * RES, VIA_D / 2 + GROW)
    for L in ALL_L:
        paint(soft, L, ys, xs, code)
    emit_direct(code, zx, zy, jx * RES, jy * RES, hw, min(zl))
    ok += 1

print(f"routed {ok}, failed {fail}, ripped {len(ripped_names)} nets: {sorted(ripped_names)}")
for f in fails:
    print("  FAIL", f[0], f"({f[1]:.2f},{f[2]:.2f})->({f[3]:.2f},{f[4]:.2f})")

json.dump(ripcount, open(HISTFILE, "w"))
pcbnew.ZONE_FILLER(b).Fill(b.Zones())
b.Save(BOARD)
print("saved", BOARD)

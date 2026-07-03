"""grid_router.py — A* grid router for the connections freerouting left.

2-layer, 0.05 mm grid. Obstacles per layer: pads, tracks, vias of OTHER
nets inflated by (clearance + half-width). Via moves need a both-layer
free disc. Routes GND first (usually a stub+via to the B plane), then
POWER (0.35), then signals (0.2). Rasterizes each routed net into the
obstacle grid before the next. DRC is the final arbiter.

Usage: python3 grid_router.py BOARD.kicad_pcb DRC_UNCONNECTED.json
"""
import sys, json, re, heapq, math
import numpy as np
import pcbnew

BOARD, DRCJSON = sys.argv[1], sys.argv[2]
RES = 0.05                      # mm per cell
W, H = 25.0, 27.0
NX, NY = int(W / RES) + 1, int(H / RES) + 1
CLR = 0.15
RF_CLR = 0.15
VIA_D, VIA_DRILL = 0.46, 0.20   # JLC: annular 0.13 exact, drill 0.2
EDGE = 0.30                     # copper-edge margin (rule 0.25)

b = pcbnew.LoadBoard(BOARD)
def mm(v): return pcbnew.ToMM(v)
MM = pcbnew.FromMM

pwr = {"VBAT", "BATN", "+3V0", "VDDS", "VDDR", "DCDC_SW", "DCOUPL", "MOT_P",
       "MOT_N", "PIEZO", "AC1", "AC2", "RECT", "COMM1", "CLAMP1", "BOOT2",
       "DRV_REG", "CLAMP2", "COMM2", "COIL1", "COIL2", "DW_VCC", "BOOT1"}

def width_for(net):
    return 0.35 if net in pwr else 0.20

# --- obstacle grids: block[layer][y,x] = net-code that owns, -1 free, -2 hard ---
# store as int32 net codes; collision if cell owned by different net
own = {0: np.full((NY, NX), -1, np.int32), 1: np.full((NY, NX), -1, np.int32)}
FCU, BCU = 0, 1

def cells_disc(cx, cy, r):
    x0, x1 = max(0, int((cx - r) / RES)), min(NX - 1, int((cx + r) / RES) + 1)
    y0, y1 = max(0, int((cy - r) / RES)), min(NY - 1, int((cy + r) / RES) + 1)
    xs, ys = np.meshgrid(np.arange(x0, x1 + 1), np.arange(y0, y1 + 1))
    d2 = (xs * RES - cx) ** 2 + (ys * RES - cy) ** 2
    return ys[d2 <= r * r], xs[d2 <= r * r]

def cells_rect(x0, y0, x1, y1, grow):
    # mark only cells whose CENTER lies inside the grown rect (no overshoot:
    # fine-pitch pin-escape corridors are ~0.03 wider than the inflation)
    x0, y0, x1, y1 = x0 - grow, y0 - grow, x1 + grow, y1 + grow
    ix0, ix1 = max(0, math.ceil(x0 / RES)), min(NX - 1, math.floor(x1 / RES))
    iy0, iy1 = max(0, math.ceil(y0 / RES)), min(NY - 1, math.floor(y1 / RES))
    return slice(iy0, iy1 + 1), slice(ix0, ix1 + 1)

def cells_seg(x1, y1, x2, y2, r):
    n = max(2, int(math.hypot(x2 - x1, y2 - y1) / (RES)) + 1)
    ys_all, xs_all = [], []
    for i in range(n + 1):
        t = i / n
        cy, cx = cells_disc(x1 + (x2 - x1) * t, y1 + (y2 - y1) * t, r)
        ys_all.append(cy); xs_all.append(cx)
    return np.concatenate(ys_all), np.concatenate(xs_all)

def paint(layer, ys, xs, code):
    g = own[layer]
    cur = g[ys, xs]
    conflict = (cur != -1) & (cur != code)
    g[ys, xs] = np.where(conflict, -2, code)   # -2 = multiple nets: hard block

GROW = CLR + 0.10 + 0.04    # clearance + passing half-width + quantization margin

print("building obstacle grid...")
for fp in b.GetFootprints():
    for p in fp.Pads():
        bb = p.GetBoundingBox()
        code = p.GetNetCode() if p.GetNetname() else -2
        grow = GROW
        sl = cells_rect(mm(bb.GetLeft()), mm(bb.GetTop()),
                        mm(bb.GetRight()), mm(bb.GetBottom()), grow)
        for layer in (FCU, BCU) if p.GetAttribute() == pcbnew.PAD_ATTRIB_PTH else (FCU,):
            g = own[layer]
            cur = g[sl]
            g[sl] = np.where((cur != -1) & (cur != code), -2, code)

for t in b.GetTracks():
    code = t.GetNetCode()
    if t.GetClass() == "PCB_VIA":
        x, y = mm(t.GetStart().x), mm(t.GetStart().y)
        ys, xs = cells_disc(x, y, mm(t.GetWidth()) / 2 + GROW)
        paint(FCU, ys, xs, code); paint(BCU, ys, xs, code)
    else:
        layer = FCU if t.GetLayer() == pcbnew.F_Cu else BCU
        ys, xs = cells_seg(mm(t.GetStart().x), mm(t.GetStart().y),
                           mm(t.GetEnd().x), mm(t.GetEnd().y),
                           mm(t.GetWidth()) / 2 + GROW)
        paint(layer, ys, xs, code)

# board edge + corner phantom rings (hard)
edge_cells = int(EDGE / RES)
for g in own.values():
    g[:edge_cells, :] = -2; g[-edge_cells:, :] = -2
    g[:, :edge_cells] = -2; g[:, -edge_cells:] = -2
for cx, cy in ((2, 2), (23, 2), (2, 25), (23, 25)):
    for rr in np.arange(2 - EDGE, 2 + EDGE, RES / 2):
        for a in np.arange(0, 2 * math.pi, 0.01):
            x, y = cx + rr * math.cos(a), cy + rr * math.sin(a)
            if 0 <= x < W and 0 <= y < H:
                own[FCU][int(y / RES), int(x / RES)] = -2
                own[BCU][int(y / RES), int(x / RES)] = -2

# --- unconnected list from DRC json ---
d = json.load(open(DRCJSON))
conns = []
for u in d.get("unconnected_items", []):
    net = re.search(r"\[(.*?)\]", u["items"][0]["description"]).group(1)
    pts = [(i["pos"]["x"], i["pos"]["y"]) for i in u["items"]]
    if len(pts) == 2:
        conns.append((net, pts[0], pts[1]))

def prio(c):
    net = c[0]
    d2 = math.hypot(c[1][0] - c[2][0], c[1][1] - c[2][1])
    if net.startswith("LCD_"):
        group = -1        # scarce north-lane capacity: LCD claims it first
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

def astar(code, sx, sy, gx, gy, hw):
    six, siy = int(round(sx / RES)), int(round(sy / RES))
    gix, giy = int(round(gx / RES)), int(round(gy / RES))
    grow_extra = int(max(0, hw - 0.10) / RES + 0.5)
    blk = {}
    for l in (FCU, BCU):
        m = (own[l] != -1) & (own[l] != code)
        blk[l] = dilate(m, grow_extra) if grow_extra else m
    vr = int(math.ceil((VIA_D / 2 + CLR - GROW) / RES))   # extra beyond GROW
    viablk = dilate(blk[FCU] | blk[BCU], max(1, vr))
    h = lambda ix, iy: math.hypot(ix - gix, iy - giy)
    start = (six, siy, FCU)
    openq = [(h(six, siy), 0.0, start, None)]
    came, gcost = {}, {start: 0.0}
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
        if (ix, iy) == (gix, giy) and l == FCU:
            path = []
            while node:
                path.append(node)
                node = came[node]
            return path[::-1]
        B = blk[l]
        for dx, dy, cost in DIRS:
            nx_, ny_ = ix + dx, iy + dy
            if not (0 <= nx_ < NX and 0 <= ny_ < NY):
                continue
            nn = (nx_, ny_, l)
            if nn in seen or B[ny_, nx_]:
                continue
            if dx and dy and (B[iy, nx_] or B[ny_, ix]):
                continue   # no corner cutting through diagonal gaps
            ng = gc + cost
            if ng < gcost.get(nn, 1e18):
                gcost[nn] = ng
                heapq.heappush(openq, (ng + h(nx_, ny_), ng, nn, node))
        if not viablk[iy, ix]:
            nn = (ix, iy, 1 - l)
            ng = gc + VIA_COST
            if nn not in seen and ng < gcost.get(nn, 1e18):
                gcost[nn] = ng
                heapq.heappush(openq, (ng + h(ix, iy), ng, nn, node))
    return None

def emit(code, path, hw, netname):
    # compress into segments per layer + vias
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
        t.SetLayer(pcbnew.F_Cu if l == FCU else pcbnew.B_Cu)
        t.SetNetCode(code)
        b.Add(t)
        ys, xs = cells_seg(x1 * RES, y1 * RES, x2 * RES, y2 * RES, hw + GROW)
        paint(l, ys, xs, code)
    for (x, y, _) in vias:
        v = pcbnew.PCB_VIA(b)
        v.SetPosition(pcbnew.VECTOR2I(MM(x * RES), MM(y * RES)))
        v.SetWidth(MM(VIA_D)); v.SetDrill(MM(VIA_DRILL))
        v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
        v.SetNetCode(code)
        b.Add(v)
        ys, xs = cells_disc(x * RES, y * RES, VIA_D / 2 + GROW)
        paint(FCU, ys, xs, code); paint(BCU, ys, xs, code)

ok = fail = 0
fails = []
for net, (x1, y1), (x2, y2) in conns:
    code = b.FindNet(net).GetNetCode()
    hw = width_for(net) / 2
    path = astar(code, x1, y1, x2, y2, hw)
    if path is None and hw > 0.1:
        path = astar(code, x1, y1, x2, y2, 0.1)   # narrow fallback
        hw = 0.1
    if path is None:
        fail += 1; fails.append((net, x1, y1, x2, y2))
        continue
    emit(code, path, hw, net)
    ok += 1
print(f"routed {ok}, failed {fail}")
for f in fails:
    print("  FAIL", f[0], f"({f[1]:.2f},{f[2]:.2f})->({f[3]:.2f},{f[4]:.2f})")

pcbnew.ZONE_FILLER(b).Fill(b.Zones())
b.Save(BOARD)
print("saved", BOARD)

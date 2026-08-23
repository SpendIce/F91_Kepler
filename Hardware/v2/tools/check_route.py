"""check_route.py — exact-geometry clearance checker + emitter for hand routes.

v2: pads are rectangles (bbox), not conservative discs; unlocked foreign
blockers are collected into a rip list instead of failing; locked/pad
blockers still fail. 'emit' rips the collected nets, writes the locked
routes, refills zones.

Usage: python3 tools/check_route.py BOARD check|emit
"""
import sys, math
import pcbnew

BOARD = sys.argv[1]
MODE = sys.argv[2] if len(sys.argv) > 2 else "check"
CLR = 0.15
EDGE = 0.32
F, IN2, B = "F", "In2", "B"
LAYMAP = {F: pcbnew.F_Cu, IN2: pcbnew.In2_Cu, B: pcbnew.B_Cu}
NORIP = {"GND", "ANT_FEED", "RF_UNBAL", "RF_N", "RF_P", "X48M_N", "X48M_P"}

EXTRA_VIAS = [
    ("+3V0", 10.40, 23.10),   # R14.2 pad edge
    ("+3V0", 13.90, 23.45),   # FB1.1
    ("+3V0", 23.60,  9.05),   # SW2.2 lower pad
    ("+3V0", 22.60,  3.15),   # C33.1 in-pad
]

ROUTES = [
    ("+3V0", 0.20, [(10.70, 22.80, F), (11.40, 22.80, F)]),
    ("+3V0", 0.25, [(5.60, 25.50, B), (6.50, 25.00, B),
                    (9.00, 24.60, B), (9.95, 23.90, B),
                    (10.40, 23.10, B)]),
    ("+3V0", 0.25, [(13.90, 23.45, IN2), (12.90, 23.35, IN2),
                    (10.40, 23.10, IN2)]),
    ("+3V0", 0.25, [(5.60, 25.50, B), (8.00, 26.00, B),
                    (16.00, 26.05, B), (20.50, 26.00, B),
                    (22.60, 25.35, B), (22.90, 24.40, B),
                    (23.30, 22.60, B), (23.65, 20.00, B),
                    (23.70, 17.50, B), (23.60, 10.50, B),
                    (23.60, 9.05, B)]),
    ("+3V0", 0.25, [(23.60, 9.05, IN2), (23.45, 10.60, IN2),
                    (23.35, 11.90, IN2), (21.30, 12.60, IN2),
                    (21.20, 14.50, IN2), (20.49, 14.60, IN2)]),
    ("+3V0", 0.25, [(23.60, 8.50, F), (23.60, 5.50, F)]),
    ("+3V0", 0.25, [(22.60, 3.15, B), (22.05, 3.30, B),
                    (21.90, 3.90, B),
                    (21.95, 5.30, B), (22.35, 6.60, B),
                    (23.10, 7.60, B), (23.60, 9.05, B)]),
]

b = pcbnew.LoadBoard(BOARD)
mm = pcbnew.ToMM
ALLK = set(LAYMAP.values())

# obstacle: (netcode, layers, kind, geom, halfw/None, locked, label)
# kind 'seg': geom=(x1,y1,x2,y2); kind 'rect': geom=(cx,cy,hw,hh)
obstacles = []
for t in b.GetTracks():
    if t.GetClass() == "PCB_VIA":
        x, y = mm(t.GetStart().x), mm(t.GetStart().y)
        obstacles.append((t.GetNetCode(), ALLK, "seg", (x, y, x, y),
                          mm(t.GetWidth(pcbnew.F_Cu)) / 2,
                          t.IsLocked() or t.GetNetname() in NORIP,
                          f"via {t.GetNetname()} ({x:.2f},{y:.2f})"))
    else:
        if t.GetLayer() not in ALLK:
            continue
        obstacles.append((t.GetNetCode(), {t.GetLayer()}, "seg",
                          (mm(t.GetStart().x), mm(t.GetStart().y),
                           mm(t.GetEnd().x), mm(t.GetEnd().y)),
                          mm(t.GetWidth()) / 2, t.IsLocked() or t.GetNetname() in NORIP,
                          f"trk {t.GetNetname()}"))
for fp in b.GetFootprints():
    for p in fp.Pads():
        bb = p.GetBoundingBox()
        lays = ALLK if p.GetAttribute() == pcbnew.PAD_ATTRIB_PTH else {pcbnew.F_Cu}
        obstacles.append((p.GetNetCode(), lays, "rect",
                          (mm(bb.GetCenter().x), mm(bb.GetCenter().y),
                           mm(bb.GetWidth()) / 2, mm(bb.GetHeight()) / 2),
                          None, True,
                          f"pad {fp.GetReference()}.{p.GetName()} [{p.GetNetname()}]"))

def pt_seg(px, py, x1, y1, x2, y2):
    vx, vy = x2 - x1, y2 - y1
    L2 = vx * vx + vy * vy
    if L2 == 0:
        return math.hypot(px - x1, py - y1)
    t = max(0, min(1, ((px - x1) * vx + (py - y1) * vy) / L2))
    return math.hypot(px - (x1 + vx * t), py - (y1 + vy * t))

def pt_rect(px, py, cx, cy, hw, hh):
    dx = max(0.0, abs(px - cx) - hw)
    dy = max(0.0, abs(py - cy) - hh)
    return math.hypot(dx, dy)

def segs_intersect(ax, ay, bx, by, cx, cy, dx, dy):
    def ccw(px, py, qx, qy, rx, ry):
        return (ry - py) * (qx - px) - (qy - py) * (rx - px)
    d1 = ccw(cx, cy, dx, dy, ax, ay)
    d2 = ccw(cx, cy, dx, dy, bx, by)
    d3 = ccw(ax, ay, bx, by, cx, cy)
    d4 = ccw(ax, ay, bx, by, dx, dy)
    return ((d1 > 0) != (d2 > 0)) and ((d3 > 0) != (d4 > 0))

def seg_obstacle_dist(x1, y1, x2, y2, ob):
    kind, geom = ob[2], ob[3]
    if kind == "seg":
        ox1, oy1, ox2, oy2 = geom
        if (ox1, oy1) != (ox2, oy2) and (x1, y1) != (x2, y2) and \
                segs_intersect(x1, y1, x2, y2, ox1, oy1, ox2, oy2):
            return -9.0
        return min(pt_seg(x1, y1, ox1, oy1, ox2, oy2),
                   pt_seg(x2, y2, ox1, oy1, ox2, oy2),
                   pt_seg(ox1, oy1, x1, y1, x2, y2),
                   pt_seg(ox2, oy2, x1, y1, x2, y2)) - ob[4]
    cx, cy, hw, hh = geom
    n = max(2, int(math.hypot(x2 - x1, y2 - y1) / 0.02))
    return min(pt_rect(x1 + (x2 - x1) * i / n, y1 + (y2 - y1) * i / n,
                       cx, cy, hw, hh) for i in range(n + 1))

W, H = 25.0, 27.0
riplist = set()
allok = True
plan = []

def check(x1, y1, x2, y2, hw, kl, code, tag, netname):
    global allok
    for ob in obstacles:
        oc, olays, kind, geom, ohw, locked, lbl = ob
        if oc == code or kl not in olays:
            continue
        d = seg_obstacle_dist(x1, y1, x2, y2, ob) - hw
        if d < CLR:
            if not locked and (lbl.startswith("trk") or lbl.startswith("via")):
                riplist.add(lbl.split()[1])
            else:
                print(f"  FAIL {tag}: {d:.3f} vs {lbl}")
                allok = False
    for (px, py) in ((x1, y1), (x2, y2)):
        if min(px, W - px, py, H - py) - hw < EDGE - 0.02:
            print(f"  FAIL {tag}: board edge")
            allok = False

for net, x, y in EXTRA_VIAS:
    code = b.FindNet(net).GetNetCode()
    check(x, y, x, y, 0.20, pcbnew.F_Cu, code, f"newvia {net}", net)
    check(x, y, x, y, 0.20, pcbnew.In2_Cu, code, f"newvia {net}", net)
    check(x, y, x, y, 0.20, pcbnew.B_Cu, code, f"newvia {net}", net)
    plan.append(("via", code, x, y))
    obstacles.append((code, ALLK, "seg", (x, y, x, y), 0.20, True, f"via NEW-{net}"))

for net, w, pts in ROUTES:
    code = b.FindNet(net).GetNetCode()
    hw = w / 2
    print(f"{net}:")
    for i in range(len(pts) - 1):
        x1, y1, l1 = pts[i]
        x2, y2, l2 = pts[i + 1]
        if l1 != l2:
            if (x1, y1) != (x2, y2):
                print(f"  LAYERJUMP with movement"); allok = False
            for kl in ALLK:
                check(x1, y1, x1, y1, 0.20, kl, code, f"jumpvia {net}", net)
            plan.append(("via", code, x1, y1))
            obstacles.append((code, ALLK, "seg", (x1, y1, x1, y1), 0.20, True,
                              f"via NEW-{net}"))
            continue
        check(x1, y1, x2, y2, hw, LAYMAP[l1], code,
              f"({x1},{y1})->({x2},{y2}) {l1}", net)
        plan.append(("seg", code, hw, x1, y1, x2, y2, LAYMAP[l1]))
        obstacles.append((code, {LAYMAP[l1]}, "seg", (x1, y1, x2, y2), hw, True,
                          f"trk NEW-{net}"))

print("riplist:", sorted(riplist))
print("ALL CLEAR" if allok else "FAILURES PRESENT")

if MODE == "emit":
    if not allok:
        print("refusing to emit with failures"); sys.exit(1)
    codes = {b.FindNet(n).GetNetCode(): n for n in riplist if b.FindNet(n)}
    n = 0
    for t in list(b.GetTracks()):
        if not t.IsLocked() and t.GetNetCode() in codes:
            b.Delete(t); n += 1
    print(f"ripped {n} items from {len(codes)} nets")
    MM = pcbnew.FromMM
    for item in plan:
        if item[0] == "via":
            _, code, x, y = item
            v = pcbnew.PCB_VIA(b)
            v.SetPosition(pcbnew.VECTOR2I(MM(x), MM(y)))
            v.SetWidth(MM(0.40)); v.SetDrill(MM(0.20))
            v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
            v.SetNetCode(code); v.SetLocked(True); b.Add(v)
        else:
            _, code, hw, x1, y1, x2, y2, kl = item
            t = pcbnew.PCB_TRACK(b)
            t.SetStart(pcbnew.VECTOR2I(MM(x1), MM(y1)))
            t.SetEnd(pcbnew.VECTOR2I(MM(x2), MM(y2)))
            t.SetWidth(MM(hw * 2)); t.SetLayer(kl)
            t.SetNetCode(code); t.SetLocked(True); b.Add(t)
    pcbnew.ZONE_FILLER(b).Fill(b.Zones())
    b.Save(BOARD)
    print("emitted and saved")

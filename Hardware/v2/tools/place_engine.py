"""
place_engine.py — authoritative geometry-driven placement for F91 Kepler v2.

Replaces the fix2..fix5 whack-a-mole. Strategy:
  1. Big parts (ICs, connectors, crystals, antenna, buttons, battery, DCDC
     inductor/balun, pogo test-point row) are pinned to a reasoned floorplan.
  2. Every passive is placed at the NEAREST free grid slot to its home IC,
     so caps stay near what they decouple, and overlap is impossible by
     construction (each placement is collision-tested against real pad bboxes).
  3. Edge (0.5 mm) and corner-arc (r2 @ corners + 0.5) clearances enforced.

Headless usage (no GUI):
  python3 place_engine.py IN.kicad_pcb OUT.kicad_pcb
Then verify with overlaps.py + `kicad-cli pcb drc`.
"""
import sys, math
import pcbnew

IN, OUT = sys.argv[1], sys.argv[2]
b = pcbnew.LoadBoard(IN)

def MM(v): return pcbnew.FromMM(v)
def mm(v): return v / 1e6

# ── FIXED parts: pinned, never moved (edges / mechanical / MCU) ──────────────
FIXED = {
    "U1": (12.5, 13.5,   0),   # CC2652R7 MCU, centre
    "J1": (12.5,  4.5, 180),   # LCD FPC, top edge
    "AE1": (1.2, 13.5,   0),   # IFA antenna feed, far-left edge
    "SW1": (2.2,  9.0, 270),   # BTN1 / BSL, left edge
    "SW2": (22.8, 7.0,  90),   # BTN2, right-top edge (clear of TR arc)
    "SW3": (22.8, 19.0, 90),   # BTN3, right-bottom edge
    "BT1": (4.0, 19.5,  90),   # LiPo connector, bottom-left (clear of pogo row)
    # pogo charging row, bottom edge, 2.3 mm pitch (mechanical — fixed)
    # inboard x=5.6..19.4 so end pads clear the corner-arc rings
    "TP1": (5.6, 25.5, 0), "TP2": (7.9, 25.5, 0), "TP3": (10.2, 25.5, 0),
    "TP4": (12.5, 25.5, 0), "TP5": (14.8, 25.5, 0), "TP6": (17.1, 25.5, 0),
    "TP7": (19.4, 25.5, 0),
}

# ── SEED parts: movable big parts, placed nearest-free to a home hint ─────────
# (target_x, target_y, rot) — rot is honoured; position is collision-resolved.
SEED = {
    "U2": (19.5, 22.0,   0),   # BQ51050B Qi RX, bottom-right
    "U3": (22.5, 12.5,   0),   # DW01A protection, right edge
    "Q1": (21.5,  8.5,   0),   # FS8205A dual-FET, upper-right
    "U4": (19.5, 14.5, 180),   # XC6206 LDO
    "U5": ( 6.5,  8.5,   0),   # LIS2DW12 accel, left
    "U6": ( 4.0, 18.0,   0),   # DRV2605L haptic, left-mid
    "U7": ( 6.0,  5.5,   0),   # ST25DV NFC, top-left
    "Y1": (10.0, 19.0,   0),   # 48 MHz xtal, below U1
    "Y2": ( 5.5, 13.0,   0),   # 32 kHz xtal, left of U1
    "FL1": (15.5, 18.5,  0),   # RF balun, below U1
    "FB1": (16.5, 10.0,  0),   # DCDC ferrite, right of U1
    "L2": (18.5, 16.5,   0),   # DCDC inductor, right of U1
    "R2":  (3.5, 16.0,  90),   # 0R antenna series jumper
}

# home IC for each passive cluster (refined from net-share analysis + intent)
HOME = {
    # U1 MCU
    "C2":"U1","C3":"U1","C4":"U1","C5":"U1","C10":"U1","C6":"U1",
    "C7":"U1","C8":"U1","C9":"U1","C11":"U1","C12":"U1","C13":"U1",
    "R1":"U1","R3":"U1","R9":"U1","R4":"U1",
    # U2 Qi
    "C16":"U2","C17":"U2","C18":"U2","C19":"U2","C20":"U2","C21":"U2",
    "C22":"U2","C23":"U2","C24":"U2","C25":"U2","C26":"U2","C27":"U2",
    "C28":"U2","R5":"U2","R6":"U2","R7":"U2","R8":"U2","W1":"U2",
    # U3/Q1 protection
    "C31":"U3","R10":"U3","R11":"U3",
    # U4 LDO
    "C29":"U4","C30":"U4","C32":"U4","C33":"U4",
    # U5 accel
    "C34":"U5","C35":"U5",
    # U6 haptic
    "C36":"U6","C37":"U6",
    # U7 NFC + I2C pull-ups
    "C38":"U7","R13":"U7","R14":"U7","R12":"U7",
    # J1 LCD
    "C39":"J1","C40":"J1",
    # RF caps near antenna/U1 RF
    "C14":"AE1","C15":"AE1",
    # generic test points (flexible) -> home near functional block
    "TP8":"U2","TP9":"U2","TP10":"U2","TP11":"U2","TP12":"U2",
    "TP13":"U6","TP14":"U6","TP15":"U7","TP16":"U7",
}

# ── board usable region + corner arcs ────────────────────────────────────────
# Board outline: rect [0,25]x[0,27], corners rounded r=2 (centres inset at
# (2,2),(23,2),(2,25),(23,25)). Copper must stay EDGE_CLR inside every edge.
EDGE_CLR = 0.55
BW, BH, RAD = 25.0, 27.0, 2.0
X0, Y0, X1, Y1B = EDGE_CLR, EDGE_CLR, BW-EDGE_CLR, BH-EDGE_CLR
# corner centre + quadrant predicate (point is in the rounded region)
ARCS = [(2,2,'TL'), (23,2,'TR'), (2,25,'BL'), (23,25,'BR')]
RMAX = RAD - EDGE_CLR   # in a corner quadrant, copper must be within this of centre

ANNULUS_HI = RAD + EDGE_CLR   # KiCad measures arc clearance vs the full r2 circle
def region_ok(l, t, r, btm):
    if l < X0 or r > X1 or t < Y0 or btm > Y1B:
        return False
    # rectangle [l,r]x[t,btm] must not straddle the danger ring of any corner
    for cx, cy, _ in ARCS:
        nx = min(max(cx, l), r); ny = min(max(cy, t), btm)   # nearest point in rect
        dmin = math.hypot(nx-cx, ny-cy)
        dmax = max(math.hypot(px-cx, py-cy)                   # farthest corner
                   for px,py in ((l,t),(r,t),(l,btm),(r,btm)))
        if dmin < ANNULUS_HI and dmax > RMAX:   # rect overlaps ring [RMAX,HI]
            return False
    return True

# ── place a footprint, return its inflated copper bbox (mm) ──────────────────
CLR = 0.25  # inter-part copper gap target (>= DRC 0.2)

def fp_bbox_mm(fp):
    L=T=1e15; R=Bm=-1e15
    for p in fp.Pads():
        bb=p.GetBoundingBox()
        L=min(L,bb.GetLeft()); R=max(R,bb.GetRight())
        T=min(T,bb.GetTop());  Bm=max(Bm,bb.GetBottom())
    return mm(L),mm(T),mm(R),mm(Bm)

def set_pos(fp,x,y,rot):
    fp.SetOrientationDegrees(rot)
    fp.SetPosition(pcbnew.VECTOR2I(MM(x),MM(y)))

occupied = []  # list of (l,t,r,b) inflated bboxes

def collides(l,t,r,bm):
    for ol,ot,orr,ob in occupied:
        if l < orr and r > ol and t < ob and bm > ot:
            return True
    return False

def add_occ(l,t,r,bm):
    occupied.append((l-CLR, t-CLR, r+CLR, bm+CLR))

# candidate grid
GRID=0.25
cands=[]
x=X0
while x<=X1:
    y=Y0
    while y<=Y1B:
        cands.append((x,y)); y+=GRID
    x+=GRID

def half_extents(fp,rot):
    set_pos(fp,0,0,rot)
    l,t,r,bm=fp_bbox_mm(fp)
    return (r-l)/2.0,(bm-t)/2.0

final_xy={}   # ref -> placed (x,y)

def place_near(fp, hx, hy, rots):
    """Place fp at nearest free grid slot to (hx,hy); return True/False."""
    best=None
    for rot in rots:
        hw,hh=half_extents(fp,rot)
        for (cx,cy) in sorted(cands,key=lambda c:(c[0]-hx)**2+(c[1]-hy)**2):
            l,t,r,bm=cx-hw,cy-hh,cx+hw,cy+hh
            if not region_ok(l,t,r,bm): continue
            if collides(l,t,r,bm): continue
            best=(cx,cy,rot,l,t,r,bm); break
        if best: break
    if not best: return False
    cx,cy,rot,l,t,r,bm=best
    set_pos(fp,cx,cy,rot); add_occ(l,t,r,bm)
    final_xy[fp.GetReference()]=(cx,cy)
    return True

def area_now(fp):
    l,t,r,bm=fp_bbox_mm(fp); return (r-l)*(bm-t)

# ── signal connectivity (power/ground excluded — they get poured) ────────────
from collections import defaultdict
POWER={"GND","+3V0","VDDS","VDDR","VBAT","BATN","DCOUPL","DCDC_SW"}
allfps={fp.GetReference():fp for fp in b.GetFootprints()}
net_refs=defaultdict(set)
for ref,fp in allfps.items():
    for p in fp.Pads():
        n=p.GetNetname()
        if n and n not in POWER: net_refs[n].add(ref)
sig_nets={n:rs for n,rs in net_refs.items() if 2<=len(rs)<=8}
adj=defaultdict(set)
for n,rs in sig_nets.items():
    for a in rs:
        adj[a] |= (rs-{a})

ROT={r:rot for r,(_,_,rot) in SEED.items()}          # seed rotations
for r in FIXED: ROT[r]=FIXED[r][2]
def rots_for(ref):
    return [ROT[ref]] if ref in ROT else [90,0]

MOVABLE=[r for r in allfps if r not in FIXED]

def hpwl():
    tot=0.0
    for n,rs in sig_nets.items():
        xs=[final_xy[r][0] for r in rs if r in final_xy]
        ys=[final_xy[r][1] for r in rs if r in final_xy]
        if len(xs)>=2: tot+=(max(xs)-min(xs))+(max(ys)-min(ys))
    return tot

def place_pass(hints, order):
    occupied.clear(); final_xy.clear(); fails=[]
    for ref,(x,y,rot) in FIXED.items():               # pin fixed
        fp=allfps.get(ref)
        if not fp: continue
        set_pos(fp,x,y,rot); l,t,r,bm=fp_bbox_mm(fp); add_occ(l,t,r,bm)
        final_xy[ref]=(x,y)
    for ref in order:                                  # place movable
        fp=allfps[ref]; hx,hy=hints[ref]
        if not place_near(fp,hx,hy,rots_for(ref)): fails.append(ref)
    # absorb any failures into ANY free slot so the board stays complete+clean
    still=[]
    for ref in fails:
        if not place_near(allfps[ref], 12.5, 13.5, rots_for(ref)): still.append(ref)
    return still

# ── initial hints: SEED targets + passive HOME-IC; degree/area order ─────────
hints={}
for ref in MOVABLE:
    if ref in SEED: hints[ref]=(SEED[ref][0],SEED[ref][1])
    else:           hints[ref]=(12.5,13.5)
for ref in MOVABLE:                                    # passive -> home IC seed
    if ref not in SEED:
        home=HOME.get(ref,"U1")
        if home in FIXED:   hints[ref]=(FIXED[home][0],FIXED[home][1])
        elif home in SEED:  hints[ref]=(SEED[home][0],SEED[home][1])

def place_order():
    # big parts (>=4 mm^2) first so they claim space, then by signal degree
    big=sorted([r for r in MOVABLE if area_now(allfps[r])>=4.0],
               key=lambda r:-area_now(allfps[r]))
    small=sorted([r for r in MOVABLE if area_now(allfps[r])<4.0],
                 key=lambda r:(-len(adj[r]),-area_now(allfps[r])))
    return big+small
order=place_order()
fails=place_pass(hints,order)
best=dict(final_xy); best_cost=hpwl(); best_fails=list(fails)
b.Save(OUT)                                    # pass 0 is the current best on disk
print(f"pass 0  HPWL={best_cost:7.1f}  fails={len(fails)}  (saved)")

# ── force-directed refinement: pull toward signal-neighbour centroid ─────────
# Each accepted-best pass is saved immediately, so OUT always holds the best
# complete, collision-free layout — no fragile re-apply afterwards.
# anneal: try decreasing pull strengths; accept only fails==0 improvements so
# the saved board stays complete. Restart from best each alpha level.
for ALPHA in (0.6,0.45,0.35,0.25,0.18,0.12):
    for it in range(6):
        newh=dict(hints)
        for ref in MOVABLE:
            nb=[best[o] for o in adj[ref] if o in best]
            if nb and ref in best:
                cxx=sum(p[0] for p in nb)/len(nb); cyy=sum(p[1] for p in nb)/len(nb)
                bx,by=best[ref]
                newh[ref]=((1-ALPHA)*bx+ALPHA*cxx, (1-ALPHA)*by+ALPHA*cyy)
        fails=place_pass(newh,place_order())
        c=hpwl()
        tag=""
        if len(fails)==0 and c<best_cost-0.3:
            best=dict(final_xy); best_cost=c; best_fails=[]; hints=newh
            b.Save(OUT); tag=" * saved"
        print(f"a={ALPHA:.2f} it{it}  HPWL={c:7.1f}  fails={len(fails)}{tag}")

total=len(list(allfps))
print(f"BEST  placed: {total-len(best_fails)}/{total}  HPWL={best_cost:.1f}")
if best_fails: print("UNPLACED:", best_fails)

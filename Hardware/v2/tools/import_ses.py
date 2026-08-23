"""import_ses.py — minimal Specctra session importer (KiCad's
ImportSpecctraSES segfaults on freerouting 1.9 output for this board).

Replaces all UNLOCKED tracks/vias with the session's network_out wiring,
skipping (type protect) entries (those are the locked tracks already on
the board). Coordinates: value/10 um, y negated. Refills zones.

Usage: python3 import_ses.py BOARD.kicad_pcb SESSION.ses
       [--keep-existing] [--nets NET1,NET2]
"""
import sys, re, pcbnew

BOARD, SES = sys.argv[1], sys.argv[2]
KEEP_EXISTING = "--keep-existing" in sys.argv[3:]
NET_FILTER = None
if "--nets" in sys.argv[3:]:
    i = sys.argv.index("--nets")
    NET_FILTER = set(sys.argv[i + 1].split(","))
text = open(SES).read()

# --- tokenize s-expressions ---
toks = re.findall(r'\(|\)|"[^"]*"|[^\s()]+', text)
pos = 0
def parse():
    global pos
    assert toks[pos] == "("
    pos += 1
    out = []
    while toks[pos] != ")":
        if toks[pos] == "(":
            out.append(parse())
        else:
            out.append(toks[pos].strip('"'))
            pos += 1
    pos += 1
    return out
tree = parse()

def find(node, tag):
    return [c for c in node if isinstance(c, list) and c and c[0] == tag]

routes = find(tree, "routes")[0]
res = find(routes, "resolution")[0]        # ['resolution','um','10']
DIV = float(res[2]) * 1000.0               # units per mm (10 per um -> 10000/mm)
netout = find(routes, "network_out")[0]

b = pcbnew.LoadBoard(BOARD)
LAYER = {"F.Cu": pcbnew.F_Cu, "In1.Cu": pcbnew.In1_Cu,
         "In2.Cu": pcbnew.In2_Cu, "B.Cu": pcbnew.B_Cu}
# In1.Cu is the solid GND plane — signal wires there would carve up the
# return path, so anything the router put on it gets dropped.
PLANE = {pcbnew.In1_Cu}

removed = 0
if not KEEP_EXISTING:
    for t in list(b.GetTracks()):
        if not t.IsLocked():
            b.Delete(t)
            removed += 1
print("removed unlocked:", removed)

def C(v):  # session units -> pcbnew int (mm-based)
    return pcbnew.FromMM(float(v) / DIV)

# geometry of surviving locked tracks, to dedup (type protect) re-emission
def key_seg(x1, y1, x2, y2):
    a, c = (round(x1, -3), round(y1, -3)), (round(x2, -3), round(y2, -3))
    return (a, c) if a <= c else (c, a)
locked_keys = set()
for t in b.GetTracks():
    if t.GetClass() == "PCB_VIA":
        locked_keys.add((round(t.GetStart().x, -3), round(t.GetStart().y, -3)))
    else:
        locked_keys.add(key_seg(t.GetStart().x, t.GetStart().y,
                                t.GetEnd().x, t.GetEnd().y))

added_w = added_v = 0
for net in find(netout, "net"):
    netname = net[1]
    if NET_FILTER is not None and netname not in NET_FILTER:
        continue
    netinfo = b.FindNet(netname)
    if netinfo is None:
        print("WARN: unknown net", netname)
        continue
    code = netinfo.GetNetCode()
    for wire in find(net, "wire"):
        for path in find(wire, "path"):
            layer, width, coords = path[1], path[2], path[3:]
            pts = [(C(coords[i]), C(str(-float(coords[i + 1]))))
                   for i in range(0, len(coords), 2)]
            if LAYER[layer] in PLANE:
                continue
            for (x1, y1), (x2, y2) in zip(pts, pts[1:]):
                if key_seg(x1, y1, x2, y2) in locked_keys:
                    continue
                t = pcbnew.PCB_TRACK(b)
                t.SetStart(pcbnew.VECTOR2I(x1, y1))
                t.SetEnd(pcbnew.VECTOR2I(x2, y2))
                t.SetWidth(pcbnew.FromMM(float(width) / DIV))
                t.SetLayer(LAYER[layer])
                t.SetNetCode(code)
                b.Add(t)
                added_w += 1
    for via in find(net, "via"):
        vx, vy = C(via[2]), C(str(-float(via[3])))
        if (round(vx, -3), round(vy, -3)) in locked_keys:
            continue
        m = re.search(r"_(\d+):(\d+)_um", via[1])
        dia, drill = int(m.group(1)) / 1000.0, int(m.group(2)) / 1000.0
        v = pcbnew.PCB_VIA(b)
        v.SetPosition(pcbnew.VECTOR2I(C(via[2]), C(str(-float(via[3])))))
        v.SetWidth(pcbnew.FromMM(dia))
        v.SetDrill(pcbnew.FromMM(drill))
        v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
        v.SetNetCode(code)
        b.Add(v)
        added_v += 1
print(f"added {added_w} segs, {added_v} vias")

pcbnew.ZONE_FILLER(b).Fill(b.Zones())
b.Save(BOARD)
print("saved", BOARD)

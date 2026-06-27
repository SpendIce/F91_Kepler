import pcbnew, sys
from itertools import combinations

b = pcbnew.LoadBoard(sys.argv[1])
pads = []
for fp in b.GetFootprints():
    ref = fp.GetReference()
    for p in fp.Pads():
        bb = p.GetBoundingBox()
        pads.append((ref, p.GetPadName(), p.GetNetname(),
                     bb.GetLeft(), bb.GetRight(), bb.GetTop(), bb.GetBottom()))

def mm(v): return v/1e6
overlaps = []
for a, c in combinations(pads, 2):
    if a[0] == c[0]:        # same footprint
        continue
    if a[2] and a[2] == c[2]:   # same net -> not a short
        continue
    # AABB overlap on copper bbox
    ox = min(a[4], c[4]) - max(a[3], c[3])
    oy = min(a[6], c[6]) - max(a[5], c[5])
    if ox > 0 and oy > 0:
        overlaps.append((round(mm(min(ox,oy)),3), a[0],a[1],a[2], c[0],c[1],c[2]))

overlaps.sort(reverse=True)
print(f"PAD-BBOX OVERLAPS (different nets): {len(overlaps)}")
for o in overlaps:
    print(f"  {o[0]:+.3f}mm  {o[1]}.{o[2]}[{o[3]}]  X  {o[4]}.{o[5]}[{o[6]}]")

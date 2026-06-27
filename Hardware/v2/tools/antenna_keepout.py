"""antenna_keepout.py — add a ground-pour keepout (rule area) at the chip
antenna corner. No copper pour under/around AE1 (both layers); tracks/vias
allowed so the feed + match can route. Datasheet wants ~6.5x6.5mm clearance.
Usage: python3 antenna_keepout.py IN.kicad_pcb OUT.kicad_pcb
"""
import sys,pcbnew
IN,OUT=sys.argv[1],sys.argv[2]
b=pcbnew.LoadBoard(IN)
# remove any existing antenna keepout (idempotent re-runs)
for z in list(b.Zones()):
    if z.GetZoneName()=="ANT_KEEPOUT": b.Delete(z)
def MM(v): return pcbnew.FromMM(v)
z=pcbnew.ZONE(b)
z.SetIsRuleArea(True)
z.SetZoneName("ANT_KEEPOUT")
z.SetDoNotAllowZoneFills(True)    # no ground fill in the antenna near-field
z.SetDoNotAllowTracks(False)      # feed + match traces allowed
z.SetDoNotAllowVias(False)
z.SetDoNotAllowPads(False)
z.SetDoNotAllowFootprints(False)
ls=pcbnew.LSET()
ls.AddLayer(pcbnew.F_Cu); ls.AddLayer(pcbnew.B_Cu)
z.SetLayerSet(ls)
# polygon: top-left corner, x[0,7] y[0,5] (6.5mm clearance under top-edge antenna)
pts=[(0.0,0.0),(7.0,0.0),(7.0,5.0),(0.0,5.0)]
z.Outline().NewOutline()
for x,y in pts: z.Outline().Append(MM(x),MM(y))
b.Add(z)
print("added ANT_KEEPOUT rule area x[0,7] y[0,9] on F.Cu+B.Cu (no pour)")
b.Save(OUT)
print("saved",OUT)

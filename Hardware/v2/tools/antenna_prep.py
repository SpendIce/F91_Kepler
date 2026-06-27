"""antenna_prep.py — swap AE1 from the IFA feed-pad to the Johanson chip antenna.
Preserves net ANT_FEED on pad1 (Feed); pad2 (NC) left unconnected.
Usage: python3 antenna_prep.py IN.kicad_pcb OUT.kicad_pcb
"""
import sys,pcbnew
IN,OUT=sys.argv[1],sys.argv[2]
LIB="/home/spendice/Documents/F91_Kepler/Hardware/v2/lib/f91_footprints.pretty"
b=pcbnew.LoadBoard(IN)
old=b.FindFootprintByReference("AE1")
# capture feed net + a default position
feed_net=None
for p in old.Pads():
    if p.GetNetname(): feed_net=p.GetNet()
pos=old.GetPosition()
b.Delete(old)
new=pcbnew.FootprintLoad(LIB,"ANT_2450AT18A100")
new.SetReference("AE1")
new.SetPosition(pos)
b.Add(new)
# assign ANT_FEED to pad1 (Feed); pad2 stays no-net
for p in new.Pads():
    if p.GetPadName()=="1" and feed_net is not None:
        p.SetNet(feed_net)
print("swapped AE1 -> ANT_2450AT18A100; feed net =", feed_net.GetNetname() if feed_net else None)
b.Save(OUT)
print("saved",OUT)

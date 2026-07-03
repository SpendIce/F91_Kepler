"""stackup4.py — convert f91_kepler_v2 to 4 layers (JLC04161H).

- copper count 2 -> 4 (F.Cu, In1.Cu, In2.Cu, B.Cu)
- In1.Cu: solid GND plane (RF return directly under F.Cu, 0.21mm prepreg)
- In2.Cu: routing layer (empty here; routers use it)
- ANT_KEEPOUT rule area extended to all copper layers
- B.Cu GND pour stays

Usage: python3 stackup4.py  (in Hardware/v2)
"""
import pcbnew

BOARD = "f91_kepler_v2.kicad_pcb"
b = pcbnew.LoadBoard(BOARD)
MM = pcbnew.FromMM

ds = b.GetDesignSettings()
ds.SetCopperLayerCount(4)
print("copper layers -> 4")

# extend antenna keepout to inner layers
for z in b.Zones():
    if z.GetZoneName() == "ANT_KEEPOUT":
        ls = pcbnew.LSET()
        for l in (pcbnew.F_Cu, pcbnew.In1_Cu, pcbnew.In2_Cu, pcbnew.B_Cu):
            ls.AddLayer(l)
        z.SetLayerSet(ls)
        print("ANT_KEEPOUT -> all copper layers")

# In1 GND plane
for z in list(b.Zones()):
    if z.GetZoneName() == "GND_IN1":
        b.Delete(z)
z = pcbnew.ZONE(b)
z.SetLayer(pcbnew.In1_Cu)
z.SetNetCode(b.FindNet("GND").GetNetCode())
z.SetZoneName("GND_IN1")
z.SetLocalClearance(MM(0.2))
z.SetMinThickness(MM(0.2))
z.SetPadConnection(pcbnew.ZONE_CONNECTION_FULL)
z.Outline().NewOutline()
for x, y in ((0, 0), (25, 0), (25, 27), (0, 27)):
    z.Outline().Append(MM(x), MM(y))
b.Add(z)
print("GND_IN1 plane added")

pcbnew.ZONE_FILLER(b).Fill(b.Zones())
b.Save(BOARD)
print("saved")

"""add_gnd_plane.py — full-board GND pour on B.Cu (pre-route so the
autorouter treats GND as a plane), then export Specctra DSN.

The ANT_KEEPOUT rule area already blocks the fill at the antenna corner.
Usage: python3 add_gnd_plane.py BOARD.kicad_pcb OUT.dsn
"""
import sys, pcbnew

BOARD, DSN = sys.argv[1], sys.argv[2]
b = pcbnew.LoadBoard(BOARD)
MM = pcbnew.FromMM

for z in list(b.Zones()):
    if z.GetZoneName() == "GND_B":
        b.Delete(z)

z = pcbnew.ZONE(b)
z.SetLayer(pcbnew.B_Cu)
z.SetNetCode(b.FindNet("GND").GetNetCode())
z.SetZoneName("GND_B")
z.SetLocalClearance(MM(0.2))
z.SetMinThickness(MM(0.2))
z.SetThermalReliefGap(MM(0.3))
z.SetThermalReliefSpokeWidth(MM(0.4))
z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
z.Outline().NewOutline()
for x, y in ((0, 0), (25, 0), (25, 27), (0, 27)):
    z.Outline().Append(MM(x), MM(y))
b.Add(z)

filler = pcbnew.ZONE_FILLER(b)
filler.Fill(b.Zones())
print("GND_B zone added and filled")

b.Save(BOARD)
ok = pcbnew.ExportSpecctraDSN(b, DSN)
print("DSN export:", ok, DSN)

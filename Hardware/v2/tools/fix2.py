import pcbnew
board = pcbnew.GetBoard()

def place(ref, x, y, rot=0):
    fp = board.FindFootprintByReference(ref)
    if fp:
        fp.SetPosition(pcbnew.VECTOR2I(pcbnew.FromMM(x), pcbnew.FromMM(y)))
        fp.SetOrientationDegrees(rot)
        print(f"{ref} -> ({x}, {y})")

# SW2 was at (22.5, 11.5) = exact same coords as U3 — critical collision
place("SW2",  22.5,  6.5,  90)   # top-right button, above Q1/U3 cluster
place("SW3",  22.5, 23.0,  90)   # bottom-right button, below U2 cluster

# SW1 too close to RF chain and antenna — move above crystals
place("SW1",   2.5, 11.0, 270)

# U7 (SOIC-8) overlapping J1 (FPC connector) — move U7 left
place("U7",    5.5,  5.5)

# Spread U3 slightly down away from Q1 bottom edge
place("U3",   22.5, 12.5)

# Move C36/C37 away from SW1
place("C36",   3.0, 10.5,  90)
place("C37",   3.0, 12.0,  90)

pcbnew.Refresh()
print("done — re-run DRC")

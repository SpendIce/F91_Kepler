import pcbnew
board = pcbnew.GetBoard()

def place(ref, x, y, rot=0):
    fp = board.FindFootprintByReference(ref)
    if fp:
        fp.SetPosition(pcbnew.VECTOR2I(pcbnew.FromMM(x), pcbnew.FromMM(y)))
        fp.SetOrientationDegrees(rot)
        print(f"{ref} -> ({x}, {y})")

place("J1",   12.5,  5.0, 180)
place("C39",  15.0,  5.0,  90)
place("C40",  16.0,  5.0,  90)
place("BT1",  20.0, 14.0)
place("TP11", 19.5, 25.5)
place("TP12", 20.5, 25.5)

pcbnew.Refresh()
print("done")

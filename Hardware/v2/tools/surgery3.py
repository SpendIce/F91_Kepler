"""surgery3.py — final consolidated surgery for the last ~15 nets.

Moves:
  TP15/TP16 (NFC taps) -> BT1 inter-pad gap, adjacent to U7 NFC pins
  U6 -> (4.85,14.05): U6-U1 west channel 0.49 -> 0.89 (2 signal lanes)
  Y2 -> (6.00,8.75): Y2-U5 gap 0.42 -> 0.47 (HAPTIC_EN top lane)
  L2 -> (17.85,12.40): U1-L2 east corridor 0.41 -> 0.81 (2 lanes)
  C24 -> (17.40,16.05): U1-C24 corridor 0.55 -> 0.65 (1 lane)
Re-pins (CC2652 IOC mux; pads 15-17 freed by the LCD re-pin, they reach
the now-1.05mm U1-U2 south channel):
  UART_RX  DIO_2 -> DIO_9  (pad 15)
  UART_TX  DIO_3 -> DIO_10 (pad 16)
  ACC_INT2 DIO_8 -> DIO_11 (pad 17)
Then rips unlocked copper for reroute.

Usage: python3 surgery3.py  (in Hardware/v2)
"""
import re, sys, pcbnew
import xml.etree.ElementTree as ET

SCH = "f91_kepler_v2.kicad_sch"
BOARD = "f91_kepler_v2.kicad_pcb"
MMf = pcbnew.FromMM

# --- schematic re-pins ---
s = open(SCH).read()
X = "134.62"
LABEL_MOVES = {          # DIO_2 y=83.82, DIO_3 86.36, DIO_8 99.06
    "UART_RX":  ("83.82", "101.6"),   # -> DIO_9
    "UART_TX":  ("86.36", "104.14"),  # -> DIO_10
    "ACC_INT2": ("99.06", "106.68"),  # -> DIO_11
}
NC_MOVES = {"101.6": "83.82", "104.14": "86.36", "106.68": "99.06"}
for old, new in NC_MOVES.items():
    pat = f"(no_connect\n\t\t(at {X} {old})"
    if pat not in s:
        sys.exit(f"NC at {old} not found")
    s = s.replace(pat, f"(no_connect\n\t\t(at {X} {new})", 1)
def move_label(text, name, oldy, newy):
    for m in re.finditer(r'\(global_label "%s"' % re.escape(name), text):
        start = m.start()
        block = text[start:start + 600]
        tok = f"(at {X} {oldy} "
        if tok in block:
            return text[:start] + block.replace(tok, f"(at {X} {newy} ") + text[start + 600:]
    sys.exit(f"label {name}@{oldy} not found")
for name, (o, n) in LABEL_MOVES.items():
    s = move_label(s, name, o, n)
    print(f"sch: {name} y {o} -> {n}")
open(SCH, "w").write(s)

# --- board moves ---
b = pcbnew.LoadBoard(BOARD)
MOVES = {
    "TP15": (3.05, 19.50, 0),
    "TP16": (4.95, 19.50, 0),
    "U6":   (4.85, 14.05, None),
    "Y2":   (6.00, 8.75, None),
    "L2":   (17.85, 12.40, None),
    "C24":  (17.40, 16.05, None),
}
for ref, (x, y, rot) in MOVES.items():
    fp = b.FindFootprintByReference(ref)
    fp.SetPosition(pcbnew.VECTOR2I(MMf(x), MMf(y)))
    if rot is not None:
        fp.SetOrientationDegrees(rot)
    print(f"{ref} -> ({x},{y})")

# --- board re-pins from fresh netlist (needs sch export first) ---
import subprocess
subprocess.run(["kicad-cli", "sch", "export", "netlist", "--format", "kicadxml",
                "-o", "/tmp/net_s3.xml", SCH], check=True, capture_output=True)
t = ET.parse("/tmp/net_s3.xml")
want = {}
for net in t.getroot().iter("net"):
    for node in net.iter("node"):
        if node.get("ref") == "U1":
            want[node.get("pin")] = net.get("name")
u1 = b.FindFootprintByReference("U1")
for p in u1.Pads():
    n = p.GetNumber()
    new = want.get(n)
    if n and new and new != p.GetNetname():
        ni = b.FindNet(new)
        if ni is None:
            ni = pcbnew.NETINFO_ITEM(b, new)
            b.Add(ni)
        print(f"pad {n}: {p.GetNetname()} -> {new}")
        p.SetNet(ni)

ripped = 0
for tr in list(b.GetTracks()):
    if not tr.IsLocked():
        b.Delete(tr)
        ripped += 1
print("ripped unlocked:", ripped)

pcbnew.ZONE_FILLER(b).Fill(b.Zones())
b.Save(BOARD)
print("saved")

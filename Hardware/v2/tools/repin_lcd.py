"""repin_lcd.py — move LCD/BTN nets to routable U1 DIOs (schematic side).

U1 south pads are geometrically trapped (U1-U2 gap 0.35mm). CC2652 IOC
muxes any signal to any DIO, so:
  LCD_MOSI      DIO_9  -> DIO_27 (pad 40, top edge)
  LCD_SCLK      DIO_10 -> DIO_28 (pad 41)
  LCD_CS        DIO_11 -> DIO_29 (pad 42)
  LCD_DISP      DIO_12 -> DIO_30 (pad 43)
  LCD_EXTCOMIN  DIO_13 -> DIO_26 (pad 39, ex BTN_3)
  BTN_3         DIO_26 -> DIO_20 (pad 30)
  BTN_1         DIO_15 -> DIO_0  (pad 5)
Labels sit directly on pin endpoints at x=134.62; no_connect markers swap
into the vacated rows. Firmware kepler_config.h must follow (same commit).

Usage: python3 repin_lcd.py f91_kepler_v2.kicad_sch
"""
import sys, re

SCH = sys.argv[1]
s = open(SCH).read()
X = "134.62"

# label -> (old_y, new_y)
LABEL_MOVES = {
    "LCD_MOSI":     ("101.6",  "147.32"),
    "LCD_SCLK":     ("104.14", "149.86"),
    "LCD_CS":       ("106.68", "152.4"),
    "LCD_DISP":     ("109.22", "154.94"),
    "LCD_EXTCOMIN": ("111.76", "144.78"),
    "BTN_3":        ("144.78", "129.54"),
    "BTN_1":        ("116.84", "78.74"),
}
# consumed NC -> vacated row that now needs an NC
NC_MOVES = {
    "147.32": "101.6",
    "149.86": "104.14",
    "152.4":  "106.68",
    "154.94": "109.22",
    "129.54": "111.76",
    "78.74":  "116.84",
}

# 1. no_connect swaps first (unique single-line elements)
for old, new in NC_MOVES.items():
    pat = f"(no_connect\n\t\t(at {X} {old})"
    if pat not in s:
        sys.exit(f"NC at {old} not found")
    s = s.replace(pat, f"(no_connect\n\t\t(at {X} {new})", 1)
print("no_connects swapped:", len(NC_MOVES))

# 2. label moves: order matters (BTN_3 row receives LCD_EXTCOMIN afterwards),
#    process by replacing inside each label's own block.
def move_label(text, name, oldy, newy):
    # find the global_label block for `name` whose at-x is 134.62 and y oldy
    for m in re.finditer(r'\(global_label "%s"' % re.escape(name), text):
        start = m.start()
        block = text[start:start + 600]
        tok = f"(at {X} {oldy} "
        if tok not in block:
            continue
        nb = block.replace(tok, f"(at {X} {newy} ")
        return text[:start] + nb + text[start + 600:]
    sys.exit(f"label {name}@{oldy} not found")

# BTN_3 must move out of 144.78 before LCD_EXTCOMIN moves in — dict order ok
# only if we do BTN_3 first; force explicit order:
ORDER = ["BTN_3", "BTN_1", "LCD_MOSI", "LCD_SCLK", "LCD_CS", "LCD_DISP",
         "LCD_EXTCOMIN"]
for name in ORDER:
    oldy, newy = LABEL_MOVES[name]
    s = move_label(s, name, oldy, newy)
    print(f"{name}: y {oldy} -> {newy}")

open(SCH, "w").write(s)
print("saved", SCH)

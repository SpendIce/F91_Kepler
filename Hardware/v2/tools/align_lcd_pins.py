"""Align the five LCD nets between adjacent U1 and J1 pads.

The CC2652 IOC permits these signals on any reserved LCD DIO. This mechanical
rewrite keeps MOSI/DISP fixed and cycles SCLK/CS/EXTCOMIN onto DIO_26/28/29 so
the U1 top-edge order matches the FPC order without trace crossings.

Usage:
    python3 tools/align_lcd_pins.py f91_kepler_v2.kicad_sch
"""

import re
import sys


path = sys.argv[1]
text = open(path).read()
x = "134.62"
target_y = {
    "LCD_SCLK": "144.78",
    "LCD_MOSI": "147.32",
    "LCD_CS": "149.86",
    "LCD_EXTCOMIN": "152.4",
    "LCD_DISP": "154.94",
}

for name, y in target_y.items():
    for match in re.finditer(r'\(global_label "%s"' % re.escape(name), text):
        start = match.start()
        block = text[start:start + 600]
        at = re.search(rf"\(at {x} [0-9.]+ ", block)
        if at is None:
            continue
        block = block[:at.start()] + f"(at {x} {y} " + block[at.end():]
        text = text[:start] + block + text[start + 600:]
        break
    else:
        raise SystemExit(f"U1-side label {name} not found")

open(path, "w").write(text)
print("aligned LCD labels on U1 DIO_26..30")

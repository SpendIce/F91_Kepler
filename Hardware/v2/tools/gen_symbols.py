#!/usr/bin/env python3
"""Generate Hardware/v2/lib/f91_kepler.kicad_sym.

Custom symbols not present in the stock KiCad libraries:
  CC2652R7RGZ  - TI SWRS253B pin table (VQFN-48 + EGP thermal pad as pin 49)
  LIS2DW12     - ST DS (DocID029682) LGA-12 pin table
  FS8205A      - Fortune FS8205A-DS-12_EN TSSOP-8 pin table (common-drain dual NMOS)
  ST25DV04K_SO8 - ST DS10925 SO8 pin table (no UFDFPN exposed pad)
  XC6206PxxxMR_F91 - XC6206 SOT-23 regulator with fixed pin contract

Pin data is transcribed from docs/phase2/v2_reference_circuits.md (verified
against primary datasheets). Regenerate with:  python3 gen_symbols.py
"""

import os

LIB_PATH = os.path.join(os.path.dirname(__file__), "..", "lib", "f91_kepler.kicad_sym")

# (number, name, side, etype)  side: L/R/T/B, listed top->bottom (L/R) or left->right (T/B)
CC2652 = [
    ("44", "VDDS",      "L", "power_in"),
    ("13", "VDDS2",     "L", "power_in"),
    ("22", "VDDS3",     "L", "power_in"),
    ("34", "VDDS_DCDC", "L", "power_in"),
    ("45", "VDDR",      "L", "power_in"),
    ("48", "VDDR_RF",   "L", "power_in"),
    ("33", "DCDC_SW",   "L", "passive"),
    ("23", "DCOUPL",    "L", "passive"),
    ("35", "RESET_N",   "L", "input"),
    ("24", "JTAG_TMSC", "L", "bidirectional"),
    ("25", "JTAG_TCKC", "L", "input"),
    ("47", "X48M_P",    "L", "passive"),
    ("46", "X48M_N",    "L", "passive"),
    ("3",  "X32K_Q1",   "L", "passive"),
    ("4",  "X32K_Q2",   "L", "passive"),
    ("1",  "RF_P",      "L", "passive"),
    ("2",  "RF_N",      "L", "passive"),
    ("49", "EGP",       "L", "power_in"),
] + [
    (str(n), f"DIO_{i}", "R", "bidirectional")
    for i, n in enumerate(
        # DIO_0..DIO_30 -> package pins (SWRS253B table 6-1)
        [5, 6, 7, 8, 9, 10, 11, 12, 14, 15, 16, 17, 18, 19, 20, 21,
         26, 27, 28, 29, 30, 31, 32, 36, 37, 38, 39, 40, 41, 42, 43]
    )
]

LIS2DW12 = [
    ("9",  "VDD",     "L", "power_in"),
    ("10", "VDD_IO",  "L", "power_in"),
    ("2",  "CS",      "L", "input"),
    ("3",  "SDO/SA0", "L", "passive"),
    ("4",  "SDA",     "L", "bidirectional"),
    ("1",  "SCL",     "L", "input"),
    ("12", "INT1",    "R", "output"),
    ("11", "INT2",    "R", "output"),
    ("5",  "NC",      "B", "no_connect"),
    ("7",  "RES",     "B", "passive"),
    ("6",  "GND",     "B", "power_in"),
    ("8",  "GND",     "B", "power_in"),
]

FS8205A = [
    ("4", "G1",  "L", "input"),
    ("2", "S1",  "L", "passive"),
    ("3", "S1",  "L", "passive"),
    ("5", "G2",  "R", "input"),
    ("6", "S2",  "R", "passive"),
    ("7", "S2",  "R", "passive"),
    ("1", "D12", "T", "passive"),
    ("8", "D12", "T", "passive"),
]

# Murata integrated balun-filter for CC26xx 2.4GHz.
# Pad map verified against Electrolama zzh rev A (Eagle netlist):
# pad1 UNBAL, pad3 BAL_N (RF_N), pad4 BAL_P (RF_P), pads 2/5/6 GND.
LFB18 = [
    ("4", "BAL_P", "L", "passive"),
    ("3", "BAL_N", "L", "passive"),
    ("1", "UNBAL", "R", "passive"),
    ("2", "GND",   "B", "power_in"),
    ("5", "GND",   "B", "power_in"),
    ("6", "GND",   "B", "power_in"),
]

ST25DV04K_SO8 = [
    ("1", "V_EH", "R", "power_out"),
    ("2", "AC0", "R", "passive"),
    ("3", "AC1", "R", "passive"),
    ("4", "VSS", "B", "power_in"),
    ("5", "SDA", "L", "bidirectional"),
    ("6", "SCL", "L", "input"),
    ("7", "GPO", "L", "open_collector"),
    ("8", "VCC", "T", "power_in"),
]

XC6206 = [
    ("3", "VI",  "L", "power_in"),
    ("2", "VO",  "R", "power_out"),
    ("1", "VSS", "B", "power_in"),
]

PIN_LEN = 2.54
PITCH = 2.54


def fmt(v):
    s = f"{v:.2f}"
    return s.rstrip("0").rstrip(".") if "." in s else s


def pin_sexpr(number, name, x, y, angle, etype):
    return (
        f'\t\t\t(pin {etype} line\n'
        f'\t\t\t\t(at {fmt(x)} {fmt(y)} {angle})\n'
        f'\t\t\t\t(length {fmt(PIN_LEN)})\n'
        f'\t\t\t\t(name "{name}"\n'
        f'\t\t\t\t\t(effects (font (size 1.27 1.27)))\n'
        f'\t\t\t\t)\n'
        f'\t\t\t\t(number "{number}"\n'
        f'\t\t\t\t\t(effects (font (size 1.27 1.27)))\n'
        f'\t\t\t\t)\n'
        f'\t\t\t)\n'
    )


def prop(name, value, x, y, hide):
    h = "\n\t\t\t\t(hide yes)" if hide else ""
    return (
        f'\t\t(property "{name}" "{value}"\n'
        f'\t\t\t(at {fmt(x)} {fmt(y)} 0)\n'
        f'\t\t\t(effects\n'
        f'\t\t\t\t(font (size 1.27 1.27)){h}\n'
        f'\t\t\t)\n'
        f'\t\t)\n'
    )


def symbol(name, pins, value, footprint, datasheet, descr):
    left = [p for p in pins if p[2] == "L"]
    right = [p for p in pins if p[2] == "R"]
    top = [p for p in pins if p[2] == "T"]
    bottom = [p for p in pins if p[2] == "B"]

    rows = max(len(left), len(right))
    half_h = ((rows + 1) * PITCH) / 2
    half_h = (int(half_h / PITCH) + 1) * PITCH
    cols = max(len(top), len(bottom), 4)
    half_w = ((cols + 1) * PITCH) / 2
    half_w = max((int(half_w / PITCH) + 1) * PITCH, 12.7)

    body = ""
    body += prop("Reference", "U", -half_w, half_h + 2.54, False)
    body += prop("Value", value, half_w - 10, half_h + 2.54, False)
    body += prop("Footprint", footprint, 0, -half_h - 2.54, True)
    body += prop("Datasheet", datasheet, 0, 0, True)
    body += prop("Description", descr, 0, 0, True)

    rect = (
        f'\t\t(symbol "{name}_0_1"\n'
        f'\t\t\t(rectangle\n'
        f'\t\t\t\t(start {fmt(-half_w)} {fmt(half_h)})\n'
        f'\t\t\t\t(end {fmt(half_w)} {fmt(-half_h)})\n'
        f'\t\t\t\t(stroke (width 0.254) (type default))\n'
        f'\t\t\t\t(fill (type background))\n'
        f'\t\t\t)\n'
        f'\t\t)\n'
    )

    pins_s = f'\t\t(symbol "{name}_1_1"\n'
    y = half_h - PITCH
    for n, nm, _, et in left:
        pins_s += pin_sexpr(n, nm, -half_w - PIN_LEN, y, 0, et)
        y -= PITCH
    y = half_h - PITCH
    for n, nm, _, et in right:
        pins_s += pin_sexpr(n, nm, half_w + PIN_LEN, y, 180, et)
        y -= PITCH
    x = -((len(top) - 1) * PITCH) / 2
    for n, nm, _, et in top:
        pins_s += pin_sexpr(n, nm, x, half_h + PIN_LEN, 270, et)
        x += PITCH
    x = -((len(bottom) - 1) * PITCH) / 2
    for n, nm, _, et in bottom:
        pins_s += pin_sexpr(n, nm, x, -half_h - PIN_LEN, 90, et)
        x += PITCH
    pins_s += "\t\t)\n"

    return (
        f'\t(symbol "{name}"\n'
        f'\t\t(pin_names (offset 1.016))\n'
        f'\t\t(exclude_from_sim no)\n'
        f'\t\t(in_bom yes)\n'
        f'\t\t(on_board yes)\n'
        + body + rect + pins_s +
        f'\t\t(embedded_fonts no)\n'
        f'\t)\n'
    )


def main():
    out = '(kicad_symbol_lib\n\t(version 20251024)\n\t(generator "f91gen")\n\t(generator_version "1.0")\n'
    out += symbol(
        "CC2652R7RGZ", CC2652, "CC2652R7RGZ",
        "Package_DFN_QFN:QFN-48-1EP_7x7mm_P0.5mm_EP5.15x5.15mm",
        "https://www.ti.com/lit/ds/symlink/cc2652r7.pdf",
        "SimpleLink 2.4GHz BLE5.2 MCU, 704KB flash, VQFN-48 RGZ",
    )
    out += symbol(
        "LIS2DW12", LIS2DW12, "LIS2DW12",
        "Package_LGA:LGA-12_2x2mm_P0.5mm",
        "https://www.st.com/resource/en/datasheet/lis2dw12.pdf",
        "3-axis MEMS accelerometer, I2C/SPI, LGA-12",
    )
    out += symbol(
        "FS8205A", FS8205A, "FS8205A",
        "Package_SO:TSSOP-8_3x3mm_P0.65mm",
        "https://www.ic-fortune.com/upload/Download/FS8205A-DS-12_EN.pdf",
        "Dual N-ch MOSFET, common drain, battery protection, TSSOP-8",
    )
    out += symbol(
        "LFB182G45BG5D920", LFB18, "LFB182G45BG5D920",
        "f91_footprints:Murata_LFB18_1608",
        "https://www.murata.com/products/productdata/8796762800158/QNET2207.pdf",
        "Integrated balun-filter 2.4GHz for CC26xx, 1.6x0.8mm",
    )
    out += symbol(
        "ST25DV04K_SO8", ST25DV04K_SO8, "ST25DV04K-IER8C3",
        "Package_SO:SOIC-8_3.9x4.9mm_P1.27mm",
        "https://www.st.com/resource/en/datasheet/st25dv04k.pdf",
        "Dynamic NFC/RFID tag IC with 4-Kbit EEPROM, SO8, no exposed pad",
    )
    out += symbol(
        "XC6206PxxxMR_F91", XC6206, "XC6206P302MR",
        "f91_footprints:Back_SOT-23",
        "https://product.torexsemi.com/system/files/series/xc6206.pdf",
        "3.0 V low-power LDO regulator, SOT-23",
    )
    out += ")\n"

    os.makedirs(os.path.dirname(LIB_PATH), exist_ok=True)
    with open(LIB_PATH, "w") as f:
        f.write(out)
    print(f"wrote {os.path.normpath(LIB_PATH)}")


if __name__ == "__main__":
    main()

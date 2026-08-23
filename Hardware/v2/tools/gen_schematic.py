#!/usr/bin/env python3
"""Generate Hardware/v2/f91_kepler_v2.kicad_sch from netlist.py.

Emission strategy: every component is placed at a fixed grid position and
every pin gets a global label sitting exactly on the pin's connection point
(global-label wiring — no drawn wires).  Net "NC" emits a no_connect marker.

Run:  python3 gen_schematic.py   then  kicad-cli sch erc  on the output.
"""

import os
import re
import uuid as uuidlib

from netlist import INSTANCES, PWR_FLAGS, PAPER, PROJECT_NAME

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "f91_kepler_v2.kicad_sch")
STOCK = "/usr/share/kicad/symbols"
CUSTOM = {"f91": os.path.join(HERE, "..", "lib", "f91_kepler.kicad_sym")}

ROOT_UUID = "11111111-2222-3333-4444-555555555555"


def u():
    return str(uuidlib.uuid4())


def fmt(v):
    s = f"{v:.4f}".rstrip("0").rstrip(".")
    return s if s else "0"


# --- s-expression helpers ----------------------------------------------------

def extract_block(text, header):
    i = text.find(header)
    if i < 0:
        return None
    d = 0
    j = i
    while True:
        c = text[j]
        if c == "(":
            d += 1
        elif c == ")":
            d -= 1
            if d == 0:
                return text[i:j + 1]
        j += 1


_lib_cache = {}


def load_symbol(lib_id):
    """Return (renamed_symbol_text, pins) for 'LIB:NAME'.
    pins = list of (number, x, y, angle)."""
    if lib_id in _lib_cache:
        return _lib_cache[lib_id]
    lib, name = lib_id.split(":")
    path = CUSTOM.get(lib, os.path.join(STOCK, f"{lib}.kicad_sym"))
    text = open(path).read()
    block = extract_block(text, f'(symbol "{name}"')
    if block is None:
        raise SystemExit(f"symbol {lib_id} not found in {path}")
    m = re.search(r'\(extends "([^"]+)"\)', block)
    if m:
        # flatten: use the parent's geometry/pins under this symbol's name
        parent_name = m.group(1)
        parent_block = extract_block(text, f'(symbol "{parent_name}"')
        block = parent_block.replace(f'"{parent_name}', f'"{name}')
    pins = []
    for pm in re.finditer(
        r'\(pin\s+\w+\s+\w+\s*\(at\s+([-\d.]+)\s+([-\d.]+)\s+(\d+)\)'
        r'.*?\(number\s+"([^"]+)"', block, re.S):
        pins.append((pm.group(4), float(pm.group(1)), float(pm.group(2)),
                     int(pm.group(3))))
    renamed = block.replace(f'(symbol "{name}"', f'(symbol "{lib_id}"', 1)
    _lib_cache[lib_id] = (renamed, pins)
    return _lib_cache[lib_id]


# --- emission -----------------------------------------------------------------

def prop(name, value, x, y, hide):
    h = "\n\t\t\t\t(hide yes)" if hide else ""
    return (
        f'\t\t(property "{name}" "{value}"\n'
        f'\t\t\t(at {fmt(x)} {fmt(y)} 0)\n'
        f'\t\t\t(effects\n\t\t\t\t(font (size 1.27 1.27)){h}\n\t\t\t)\n'
        f'\t\t)\n'
    )


def snap(v):
    """Snap to the 1.27mm (50 mil) schematic grid so pin endpoints land on it.
    Stock + custom symbol pins are all on 1.27mm multiples relative to origin,
    so a snapped origin keeps every pin on grid."""
    return round(v / 1.27) * 1.27


def emit_instance(ref, lib_id, value, footprint, x, y, pin_nets):
    x, y = snap(x), snap(y)
    sym_text, pins = load_symbol(lib_id)
    out = (
        f'\t(symbol\n'
        f'\t\t(lib_id "{lib_id}")\n'
        f'\t\t(at {fmt(x)} {fmt(y)} 0)\n'
        f'\t\t(unit 1)\n'
        f'\t\t(exclude_from_sim no)\n'
        f'\t\t(in_bom {"no" if ref.startswith("TP") else "yes"})\n'
        f'\t\t(on_board yes)\n'
        f'\t\t(dnp no)\n'
        f'\t\t(uuid "{u()}")\n'
    )
    out += prop("Reference", ref, x, y - 2.54, False)
    out += prop("Value", value, x, y + 2.54, False)
    out += prop("Footprint", footprint, x, y, True)
    out += prop("Datasheet", "", x, y, True)
    for n, _, _, _ in pins:
        out += f'\t\t(pin "{n}"\n\t\t\t(uuid "{u()}")\n\t\t)\n'
    out += (
        f'\t\t(instances\n'
        f'\t\t\t(project "{PROJECT_NAME}"\n'
        f'\t\t\t\t(path "/{ROOT_UUID}"\n'
        f'\t\t\t\t\t(reference "{ref}")\n'
        f'\t\t\t\t\t(unit 1)\n'
        f'\t\t\t\t)\n'
        f'\t\t\t)\n'
        f'\t\t)\n'
        f'\t)\n'
    )

    labels = ""
    seen = set()
    for n, px, py, pang in pins:
        if n in seen:
            # stacked duplicate pin numbers (none expected) — skip
            continue
        seen.add(n)
        if n not in pin_nets:
            raise SystemExit(f"{ref} ({lib_id}): pin {n} has no net mapping")
        net = pin_nets[n]
        # symbol coords are Y-up; schematic is Y-down
        ax, ay = x + px, y - py
        if net == "NC":
            labels += f'\t(no_connect\n\t\t(at {fmt(ax)} {fmt(ay)})\n\t\t(uuid "{u()}")\n\t)\n'
            continue
        # label angle: extend away from the symbol body
        lang = {0: 180, 90: 270, 180: 0, 270: 90}[pang]
        # justification per angle so text reads outward
        labels += (
            f'\t(global_label "{net}"\n'
            f'\t\t(shape passive)\n'
            f'\t\t(at {fmt(ax)} {fmt(ay)} {lang})\n'
            f'\t\t(effects\n\t\t\t(font (size 1.27 1.27))\n\t\t)\n'
            f'\t\t(uuid "{u()}")\n'
            f'\t)\n'
        )
    extra = {k for k in pin_nets if k not in {p[0] for p in pins}}
    if extra:
        raise SystemExit(f"{ref} ({lib_id}): nets for nonexistent pins {sorted(extra)}")
    return out, labels


def main():
    body_syms = ""
    body_labels = ""
    lib_ids = []
    for inst in INSTANCES:
        ref, lib_id, value, footprint, (x, y), nets = inst
        if lib_id not in lib_ids:
            lib_ids.append(lib_id)
        s, l = emit_instance(ref, lib_id, value, footprint, x, y, nets)
        body_syms += s
        body_labels += l

    # PWR_FLAG instances
    flag_text, flag_pins = load_symbol("power:PWR_FLAG")
    if "power:PWR_FLAG" not in lib_ids:
        lib_ids.append("power:PWR_FLAG")
    fx = 30
    for net in PWR_FLAGS:
        ref = f"#FLG_{net}"
        s, l = emit_instance(ref, "power:PWR_FLAG", net, "", fx, 270, {"1": net})
        body_syms += s
        body_labels += l
        fx += 20

    libs = "".join(
        "\t" + load_symbol(lid)[0].replace("\n", "\n\t").rstrip("\t") + "\n"
        for lid in lib_ids
    )

    out = (
        f'(kicad_sch\n'
        f'\t(version 20250114)\n'
        f'\t(generator "f91gen")\n'
        f'\t(generator_version "1.0")\n'
        f'\t(uuid "{ROOT_UUID}")\n'
        f'\t(paper "{PAPER}")\n'
        f'\t(lib_symbols\n{libs}\t)\n'
        + body_syms + body_labels +
        f'\t(sheet_instances\n\t\t(path "/"\n\t\t\t(page "1")\n\t\t)\n\t)\n'
        f'\t(embedded_fonts no)\n'
        f')\n'
    )
    with open(OUT, "w") as f:
        f.write(out)
    print(f"wrote {os.path.normpath(OUT)}  ({len(INSTANCES)} instances)")


if __name__ == "__main__":
    main()

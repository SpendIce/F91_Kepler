# PCB v2 — Layout guide (the remaining human/GUI work)

> Status (2026-06-13): schematic **done + ERC clean**; `Hardware/v2/f91_kepler_v2.kicad_pcb`
> generated with all 89 footprints placed (rough, off-board spread) + full
> netlist/ratsnest + 2-layer / 1.0 mm stackup + JLCPCB design rules + a 30×28 mm
> placeholder outline. DRC on the unrouted board: 202 unconnected (the ratsnest)
> + 12 clearance (overlapping spread placement) — both expected, no structural
> errors. Everything below needs the KiCad **GUI** + the physical watch; it
> cannot be done headlessly and parts of it need RF/mechanical judgment.

## 0. Why this is not auto-routed

`kicad-cli` has no schematic→board sync and no autorouter, and three regions
(RF front-end, the 2.4 GHz antenna, the charge/NFC coils) must follow proven
geometry rather than a solver. Auto-routing them risks an unprovable RF result
on a board where a respin costs ~2 weeks. So the board is handed off at the
"placed + connected" stage.

## 1. First open

1. Open `Hardware/v2/f91_kepler_v2.kicad_pro`. Symbols/footprints resolve via the
   project `sym-lib-table` / `fp-lib-table` (absolute paths to the stock libs +
   `${KIPRJMOD}/lib`).
2. In the PCB editor: **Tools → Update PCB from Schematic** (F8). It reconciles by
   reference, so it just confirms the generated board matches the schematic.
3. Regenerating: editing `tools/netlist.py` → `python3 tools/gen_schematic.py`
   then F8 in the GUI re-syncs the board (placement/routing is preserved by ref).

## 2. Board outline (replace the placeholder)

The 30×28 mm rounded rect is a stand-in. Redraw `Edge.Cuts` to the **real F91W
internal cavity** traced from the watch (the bracket is internal; case stays
original Casio). Keep edges off the gasket seat. Thickness stays **1.0 mm**.

## 3. Placement order (case-driven)

1. **MCU U1** central, EGP thermal pad with a via array to the back ground pour.
2. **RF chain** (U1 RF_P/RF_N → FL1 balun → R2 → AE1) in a straight short line to
   the antenna feed. **Copy the LP-CC26X2R1 / zzh balun+match geometry exactly**
   (see `v2_reference_circuits.md`). Crystals Y1/Y2 next to the MCU, load-cap
   loops short and symmetric, no ground between the caps.
3. **Antenna AE1**: copy the v1 inverted-F copper geometry + keepout (the
   `ANT_v1_IFA` footprint is only a feed pad + courtyard — draw the actual copper
   on F.Cu, keep v1's matching network and placement).
4. **Power/charging** (U2 BQ51050B, U3/Q1 protection, U4 LDO) grouped away from RF.
   DCDC inductor L2 + C7 right at U1 pin 33 with a low-inductance ground.
5. **I2C peripherals** (U5/U6/U7) near the MCU on the I2C bus; DRV2605L motor pads
   and battery pads exit toward the bracket recess.
6. **LCD FPC J1** at the display edge; **buttons** SW1-3 at the case-button
   positions (see §7); **pogo pads** TP1-7 on the rear, clear of the coil zones.

## 4. Rear copper — three concentric zones (Plan Maestro §5.4)

From outside in: **NFC loop** (1 turn, ~30 mm perimeter, to U7 AC0/AC1 = TP15/TP16)
→ **charge coil** (4–5 turns, ~20 mm ID, to BQ AC1/AC2 = TP9/TP10) → **ground pour
+ routing**. **No ground pour under the coil or the NFC loop.** The coil is an
external wirewound part landing on TP9/TP10 if the PCB spiral can't hit Qi Q>77.

## 5. Decoupling / DCDC / crystal layout rules (TI SWRA640)

- Each decoupling cap on the same layer as its pin, its own ground via, placed at
  the pin it decouples (the schematic groups them by pin already).
- DCDC: L2 + C7 hard against pin 33, short direct ground.
- Crystal: keep the crystal↔load-cap loop short and symmetric; never route ground
  between or around the caps.

## 6. [BENCH-TUNE] before committing Gerbers

- **Qi resonant caps C19–C22**: compute C1 (series) / C2 (parallel) from the
  *measured* coil Ls′ per WPC v1.2 (`C1 = 1/((2π·100 kHz)²·Ls′)`, Q>77, 25 V).
  The BOM ships placeholders + DNP parallel pads for trimming.
- **RF pi-match C14/C15**: shunt pads are DNP; populate only if antenna return
  loss measurement requires it. R2 = 0 Ω series jumper by default.

## 7. Buttons (decision needed at layout)

`SW1-3` are stand-in tactile footprints (`SW_Push_1P1T_NO_CK_KMR2`). The F91W
uses external case buttons pressing onto the board — consider replacing these
with **exposed contact-pad pairs** (like the battery pads) the case button
shorts, instead of SMD tactiles. SW1 also is the **BSL backdoor** button.

## 8. Pre-Gerber checklist (fab doc §7)

- [ ] Outline = real case cavity; 1.0 mm; edges off gasket seat
- [ ] RF + antenna geometry copied from reference, not re-engineered
- [ ] Coil / NFC keepouts honoured, no pour under them
- [ ] DRC clean at JLCPCB rules (min track/space 0.127 mm, via 0.45/0.3) — 0 errors
- [ ] EGP + power pads have ground via stitching
- [ ] 3D check: PCB + LiPo + motor + bracket inside the case scan
- [ ] [BENCH-TUNE] coil coupling prototyped before sealing
- [ ] Gerbers + drill + pos + BOM exported (`kicad-cli pcb export gerbers/drill/pos`),
      reviewed in a separate viewer
- [ ] JLCPCB order: 2-layer, **1.0 mm**, ENIG, 10 pcs, 2 assembled (fab doc §6)

## 9. Headless exports (once routed + DRC-clean)

```
cd Hardware/v2
kicad-cli pcb export gerbers -o output/gerbers f91_kepler_v2.kicad_pcb
kicad-cli pcb export drill   -o output/gerbers f91_kepler_v2.kicad_pcb
kicad-cli pcb export pos     -o output/f91_pos.csv --format csv --units mm f91_kepler_v2.kicad_pcb
```
BOM with LCSC fields + the position file are JLCPCB's assembly inputs
(LCSC part numbers in `v2_schematic_verification.md`).

#!/usr/bin/env bash
# gen_fab.sh — JLCPCB fabrication package for f91_kepler_v2.
# Gerber X2 + Excellon drill + pick&place CSV + BOM CSV, zipped.
# Run from Hardware/v2. Output: fab/ + f91_kepler_v2_fab.zip
set -euo pipefail

BOARD=f91_kepler_v2.kicad_pcb
SCH=f91_kepler_v2.kicad_sch
OUT=fab

# A fabrication archive is only meaningful when the design's hard electrical
# and physical checks pass.  Keep reports temporary so generated verification
# artefacts never become accidental release inputs.
DRC_REPORT=$(mktemp)
ERC_REPORT=$(mktemp)
trap 'rm -f "$DRC_REPORT" "$ERC_REPORT"' EXIT

kicad-cli pcb drc \
  --schematic-parity --severity-error --exit-code-violations \
  --output "$DRC_REPORT" "$BOARD"
kicad-cli sch erc \
  --severity-error --exit-code-violations \
  --output "$ERC_REPORT" "$SCH"

rm -rf "$OUT" && mkdir -p "$OUT/gerbers"

kicad-cli pcb export gerbers \
  --layers F.Cu,In1.Cu,In2.Cu,B.Cu,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,F.Mask,B.Mask,Edge.Cuts \
  --subtract-soldermask \
  -o "$OUT/gerbers/" "$BOARD"

kicad-cli pcb export drill \
  --format excellon --drill-origin absolute \
  --excellon-oval-format alternate --excellon-units mm \
  --generate-map --map-format gerberx2 \
  -o "$OUT/gerbers/" "$BOARD"

kicad-cli pcb export pos \
  --format csv --units mm --side front --use-drill-file-origin \
  -o "$OUT/f91_kepler_v2_cpl.csv" "$BOARD"

kicad-cli sch export bom \
  --fields "Reference,Value,Footprint,\${QUANTITY},LCSC" \
  --labels "Designator,Value,Footprint,Quantity,LCSC" \
  --group-by Value,Footprint \
  --exclude-dnp \
  -o "$OUT/f91_kepler_v2_bom.csv" "$SCH"

( cd "$OUT/gerbers" && zip -q ../f91_kepler_v2_gerbers.zip ./* )
echo "--- fab package ---"
ls -la "$OUT"

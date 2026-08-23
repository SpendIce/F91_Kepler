#!/usr/bin/env bash
set -euo pipefail

# Accept FreeRouting copper one net at a time only when KiCad proves that the
# net reduces unrouted items without introducing a hard DRC violation.
# RF, crystal, switch-node and coil/antenna nets remain manual by policy.

if [[ $# -ne 4 ]]; then
  echo "usage: $0 BOARD SESSION BASE_DRC_JSON OUTPUT_BOARD" >&2
  exit 2
fi

board=$1
session=$2
base_drc=$3
output=$4
script_dir=$(cd "$(dirname "$0")" && pwd)
project_dir=$(cd "$script_dir/.." && pwd)
work=/tmp/f91-ses-net-accept

mkdir -p "$work"
cp -a "$board" "$work/accepted.kicad_pcb"
cp "$project_dir/f91_kepler_v2.kicad_pro" "$work/trial.kicad_pro"
cp "$project_dir/f91_kepler_v2.kicad_dru" "$work/trial.kicad_dru"

opens=$(jq '.unconnected_items | length' "$base_drc")
manual='^(RF_P|RF_N|RF_UNBAL|ANT_FEED|X48M_P|X48M_N|X32K_1|X32K_2|DCDC_SW|AC1|AC2|COIL1|COIL2|NFC_A|NFC_B)$'

mapfile -t nets < <(
  jq -r '.unconnected_items[].items[0].description
    | capture("\\[(?<net>[^]]+)\\]").net' "$base_drc" \
    | sort -u \
    | awk -v reject="$manual" '$0 !~ reject'
)

for net in "${nets[@]}"; do
  cp -a "$work/accepted.kicad_pcb" "$work/trial.kicad_pcb"
  python3 "$script_dir/import_ses.py" "$work/trial.kicad_pcb" "$session" \
    --keep-existing --nets "$net" >/dev/null
  kicad-cli pcb drc --format json --severity-all \
    --output "$work/trial.json" "$work/trial.kicad_pcb" >/dev/null
  errors=$(jq '[.violations[] | select(.severity == "error")] | length' "$work/trial.json")
  trial_opens=$(jq '.unconnected_items | length' "$work/trial.json")
  if (( errors == 0 && trial_opens < opens )); then
    cp -a "$work/trial.kicad_pcb" "$work/accepted.kicad_pcb"
    echo "ACCEPT $net: $opens -> $trial_opens"
    opens=$trial_opens
  else
    echo "REJECT $net: errors=$errors opens=$trial_opens baseline=$opens"
  fi
done

cp -a "$work/accepted.kicad_pcb" "$output"
echo "final opens=$opens output=$output"

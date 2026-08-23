"""Accept useful GND stitching vias one at a time through KiCad DRC.

Usage: python3 accept_gnd_vias.py BASE STITCHED BASE_DRC_JSON OUTPUT
"""

import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

import pcbnew


base_path, stitched_path, drc_path, output_path = map(Path, sys.argv[1:5])


def via_key(via):
    p = via.GetPosition()
    return p.x, p.y, via.GetWidth(pcbnew.F_Cu), via.GetDrillValue()


def hard_errors(report):
    return sum(v["severity"] == "error" for v in report["violations"])


base = pcbnew.LoadBoard(str(base_path))
stitched = pcbnew.LoadBoard(str(stitched_path))
existing = {
    via_key(item)
    for item in base.GetTracks()
    if item.GetClass() == "PCB_VIA"
}
additions = [
    item
    for item in stitched.GetTracks()
    if item.GetClass() == "PCB_VIA"
    and item.GetNetname() == "GND"
    and via_key(item) not in existing
]
report = json.loads(drc_path.read_text())
opens = len(report["unconnected_items"])

with tempfile.TemporaryDirectory(prefix="f91-accept-gnd-") as temp_name:
    temp = Path(temp_name)
    working = temp / "candidate.kicad_pcb"
    project = working.with_suffix(".kicad_pro")
    rules = working.with_suffix(".kicad_dru")
    shutil.copy2(base_path, working)
    shutil.copy2(base_path.with_suffix(".kicad_pro"), project)
    shutil.copy2(base_path.with_suffix(".kicad_dru"), rules)
    report_path = temp / "drc.json"

    for source_via in additions:
        trial = temp / "trial.kicad_pcb"
        shutil.copy2(working, trial)
        shutil.copy2(project, trial.with_suffix(".kicad_pro"))
        shutil.copy2(rules, trial.with_suffix(".kicad_dru"))
        board = pcbnew.LoadBoard(str(trial))
        via = source_via.Duplicate()
        via.SetNetCode(board.FindNet("GND").GetNetCode())
        board.Add(via)
        pcbnew.ZONE_FILLER(board).Fill(board.Zones())
        board.Save(str(trial))
        subprocess.run(
            ["kicad-cli", "pcb", "drc", "--format", "json", "--severity-all",
             "--output", str(report_path), str(trial)],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=False,
        )
        candidate = json.loads(report_path.read_text())
        candidate_opens = len(candidate["unconnected_items"])
        errors = hard_errors(candidate)
        p = source_via.GetPosition()
        pos = f"{pcbnew.ToMM(p.x):.2f},{pcbnew.ToMM(p.y):.2f}"
        if errors == 0 and candidate_opens < opens:
            shutil.copy2(trial, working)
            shutil.copy2(working, output_path)
            opens = candidate_opens
            print(f"ACCEPT {pos}: opens={opens}", flush=True)
        else:
            print(f"REJECT {pos}: errors={errors} opens={candidate_opens}", flush=True)

    shutil.copy2(working, output_path)
    print(f"final opens={opens} output={output_path}", flush=True)

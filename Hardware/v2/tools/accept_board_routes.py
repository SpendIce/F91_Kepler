"""Merge routed copper from a candidate board one net at a time.

Only copper absent from the baseline is considered.  Each net is accepted
when KiCad reports zero hard DRC violations and fewer unconnected items.

Usage: python3 accept_board_routes.py BASE ROUTED BASE_DRC_JSON OUTPUT
"""

import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

import pcbnew


base_path, routed_path, drc_path, output_path = map(Path, sys.argv[1:5])
critical = {
    "RF_P", "RF_N", "RF_BAL", "RF_UNBAL", "ANT_FEED",
    "X48M_P", "X48M_N", "X32K_1", "X32K_2", "DCDC_SW",
    "AC1", "AC2", "COIL1", "COIL2", "NFC_A", "NFC_B",
}


def point_key(point):
    return point.x, point.y


def item_key(item):
    if item.GetClass() == "PCB_VIA":
        return ("via", point_key(item.GetPosition()), item.GetWidth(pcbnew.F_Cu),
                item.GetDrillValue(), item.TopLayer(), item.BottomLayer())
    ends = sorted((point_key(item.GetStart()), point_key(item.GetEnd())))
    return ("track", tuple(ends), item.GetWidth(), item.GetLayer())


def hard_errors(report):
    return sum(v["severity"] == "error" for v in report["violations"])


base = pcbnew.LoadBoard(str(base_path))
routed = pcbnew.LoadBoard(str(routed_path))
existing = {(track.GetNetname(), item_key(track)) for track in base.GetTracks()}
additions = {}
for track in routed.GetTracks():
    net = track.GetNetname()
    if net and net not in critical and (net, item_key(track)) not in existing:
        additions.setdefault(net, []).append(track)

report = json.loads(drc_path.read_text())
opens = len(report["unconnected_items"])

with tempfile.TemporaryDirectory(prefix="f91-accept-board-") as temp_name:
    temp = Path(temp_name)
    working = temp / "candidate.kicad_pcb"
    project = working.with_suffix(".kicad_pro")
    rules = working.with_suffix(".kicad_dru")
    shutil.copy2(base_path, working)
    shutil.copy2(base_path.with_suffix(".kicad_pro"), project)
    shutil.copy2(base_path.with_suffix(".kicad_dru"), rules)
    report_path = temp / "drc.json"

    for net in sorted(additions):
        trial = temp / "trial.kicad_pcb"
        shutil.copy2(working, trial)
        shutil.copy2(project, trial.with_suffix(".kicad_pro"))
        shutil.copy2(rules, trial.with_suffix(".kicad_dru"))
        board = pcbnew.LoadBoard(str(trial))
        net_code = board.FindNet(net).GetNetCode()
        for source_item in additions[net]:
            item = source_item.Duplicate()
            item.SetNetCode(net_code)
            board.Add(item)
        pcbnew.ZONE_FILLER(board).Fill(board.Zones())
        board.Save(str(trial))
        subprocess.run(
            ["kicad-cli", "pcb", "drc", "--format", "json", "--severity-all",
             "--output", str(report_path), str(trial)],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False,
        )
        candidate = json.loads(report_path.read_text())
        candidate_opens = len(candidate["unconnected_items"])
        errors = hard_errors(candidate)
        if errors == 0 and candidate_opens < opens:
            shutil.copy2(trial, working)
            opens = candidate_opens
            print(f"ACCEPT {net}: opens={opens}")
        else:
            print(f"REJECT {net}: errors={errors} opens={candidate_opens}")

    shutil.copy2(working, output_path)
    print(f"final opens={opens} output={output_path}")

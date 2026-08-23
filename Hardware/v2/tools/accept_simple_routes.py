"""Accept only simple routes that KiCad proves reduce the ratsnest safely.

The script tries straight and orthogonal same-layer routes for non-critical
nets.  Every candidate is checked with the project's KiCad DRC rules and is
kept only when hard DRC errors remain zero and the open-item count decreases.

Usage: python3 accept_simple_routes.py BOARD DRC_JSON OUTPUT_BOARD
"""

import json
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

import pcbnew


source = Path(sys.argv[1]).resolve()
initial_drc = Path(sys.argv[2]).resolve()
output = Path(sys.argv[3]).resolve()
project = source.with_suffix(".kicad_pro")
rules = source.with_suffix(".kicad_dru")

manual_nets = {
    "RF_P", "RF_N", "RF_BAL", "RF_UNBAL", "ANT_FEED",
    "X48M_P", "X48M_N", "X32K_1", "X32K_2", "DCDC_SW",
    "AC1", "AC2", "COIL1", "COIL2", "NFC_A", "NFC_B",
}
power_nets = {
    "+3V0", "VBAT", "BATN", "VDDS", "VDDR", "DCOUPL", "PIEZO",
    "RECT", "CLAMP1", "CLAMP2", "BOOT1", "BOOT2", "MOT_P", "MOT_N",
}
layer_ids = {"F": pcbnew.F_Cu, "In2": pcbnew.In2_Cu, "B": pcbnew.B_Cu}


def hard_errors(report):
    return sum(v["severity"] == "error" for v in report["violations"])


def endpoint(item):
    match = re.search(r"on (F|In2|B)\.Cu", item["description"])
    if not match or item["description"].startswith(("Zone", "Via")):
        return None
    return (item["pos"]["x"], item["pos"]["y"], match.group(1))


def net_name(item):
    match = re.search(r"\[([^]]+)]", item["description"])
    return match.group(1) if match else None


def routes(a, b):
    x1, y1 = a
    x2, y2 = b
    candidates = [((x1, y1), (x2, y2))]
    candidates += [((x1, y1), (x2, y1), (x2, y2)),
                   ((x1, y1), (x1, y2), (x2, y2))]
    return candidates


def add_route(board_path, net, layer, points):
    board = pcbnew.LoadBoard(str(board_path))
    code = board.FindNet(net).GetNetCode()
    width = pcbnew.FromMM(0.25 if net in power_nets else 0.127)
    for start, end in zip(points, points[1:]):
        if start == end:
            continue
        track = pcbnew.PCB_TRACK(board)
        track.SetStart(pcbnew.VECTOR2I(pcbnew.FromMM(start[0]), pcbnew.FromMM(start[1])))
        track.SetEnd(pcbnew.VECTOR2I(pcbnew.FromMM(end[0]), pcbnew.FromMM(end[1])))
        track.SetWidth(width)
        track.SetLayer(layer_ids[layer])
        track.SetNetCode(code)
        board.Add(track)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    board.Save(str(board_path))


with tempfile.TemporaryDirectory(prefix="f91-simple-route-") as tmp_name:
    tmp = Path(tmp_name)
    working = tmp / "candidate.kicad_pcb"
    tmp_project = working.with_suffix(".kicad_pro")
    tmp_rules = working.with_suffix(".kicad_dru")
    shutil.copy2(source, working)
    shutil.copy2(project, tmp_project)
    shutil.copy2(rules, tmp_rules)
    report_path = tmp / "candidate-drc.json"
    report = json.loads(initial_drc.read_text())
    baseline = len(report["unconnected_items"])
    accepted = []

    while True:
        improved = False
        connections = sorted(
            report["unconnected_items"],
            key=lambda connection: (
                (connection["items"][0]["pos"]["x"] - connection["items"][1]["pos"]["x"]) ** 2
                + (connection["items"][0]["pos"]["y"] - connection["items"][1]["pos"]["y"]) ** 2
            ) if len(connection["items"]) == 2 else float("inf"),
        )
        for connection in connections:
            items = connection["items"]
            if len(items) != 2:
                continue
            net = net_name(items[0])
            ends = [endpoint(item) for item in items]
            if net in manual_nets or not net or None in ends or ends[0][2] != ends[1][2]:
                continue
            layer = ends[0][2]
            for points in routes(ends[0][:2], ends[1][:2]):
                trial = tmp / "trial.kicad_pcb"
                shutil.copy2(working, trial)
                shutil.copy2(tmp_project, trial.with_suffix(".kicad_pro"))
                shutil.copy2(tmp_rules, trial.with_suffix(".kicad_dru"))
                add_route(trial, net, layer, points)
                result = subprocess.run(
                    ["kicad-cli", "pcb", "drc", "--format", "json", "--severity-all",
                     "--output", str(report_path), str(trial)],
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL,
                    check=False,
                )
                if result.returncode not in (0, 5) or not report_path.exists():
                    continue
                candidate_report = json.loads(report_path.read_text())
                opens = len(candidate_report["unconnected_items"])
                if hard_errors(candidate_report) == 0 and opens < baseline:
                    shutil.copy2(trial, working)
                    report = candidate_report
                    baseline = opens
                    accepted.append(net)
                    shutil.copy2(working, output)
                    print(f"ACCEPT {net}: opens={opens}", flush=True)
                    improved = True
                    break
            if improved:
                break
        if not improved:
            break

    shutil.copy2(working, output)
    print(f"accepted={len(accepted)} opens={baseline} output={output}", flush=True)

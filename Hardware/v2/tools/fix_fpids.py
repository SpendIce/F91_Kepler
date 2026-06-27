"""fix_fpids.py — set board footprint library nicknames to match the schematic
(board was generated with blank nicknames -> 105 footprint_symbol_mismatch).
Only sets the nickname when the item name already matches (no geometry change).
Usage: python3 fix_fpids.py IN.kicad_pcb OUT.kicad_pcb map.json
"""
import sys,json,pcbnew
IN,OUT,MAP=sys.argv[1],sys.argv[2],sys.argv[3]
m=json.load(open(MAP))
b=pcbnew.LoadBoard(IN)
changed=0; warn=[]
for fp in b.GetFootprints():
    ref=fp.GetReference()
    if ref not in m: warn.append((ref,"no schematic fp")); continue
    lib,_,item=m[ref].partition(":")
    bitem=fp.GetFPID().GetLibItemName()
    if str(bitem)!=item:
        warn.append((ref,f"item mismatch board='{bitem}' sch='{item}'")); continue
    fp.SetFPID(pcbnew.LIB_ID(lib,item))
    changed+=1
print(f"set FPID nickname on {changed} footprints")
for r,w in warn: print("  WARN",r,w)
b.Save(OUT)
print("saved",OUT)

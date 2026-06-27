"""fix_unconnected_nets.py — assign schematic 'unconnected-(...)' single-pad
nets to the corresponding netless board pads (spare/unused IC pins), so
PCB<->schematic parity matches. Harmless single-pad nets; no routing impact.
Usage: python3 fix_unconnected_nets.py IN.kicad_pcb OUT.kicad_pcb parity.json
"""
import sys,re,json,pcbnew
IN,OUT,PAR=sys.argv[1],sys.argv[2],sys.argv[3]
b=pcbnew.LoadBoard(IN)
d=json.load(open(PAR))
n=0
for v in d.get('schematic_parity',[]):
    if v.get('type')!='net_conflict': continue
    m=re.search(r'given by schematic \((.+)\)$',v['description'])
    if not m: continue
    netname=m.group(1)
    it=v['items'][0]['description']
    mm=re.search(r'Pad (\S+) .* of (\w+) on',it)
    if not mm: continue
    pad_no,ref=mm.group(1),mm.group(2)
    fp=b.FindFootprintByReference(ref)
    if not fp: continue
    net=b.FindNet(netname)
    if net is None:
        net=pcbnew.NETINFO_ITEM(b,netname); b.Add(net)
    for p in fp.Pads():
        if p.GetPadName()==pad_no:
            p.SetNet(net); n+=1; break
print("assigned unconnected nets:",n)
b.Save(OUT)
print("saved",OUT)

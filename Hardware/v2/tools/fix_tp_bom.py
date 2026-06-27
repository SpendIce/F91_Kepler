import re,sys
src=open(sys.argv[1],encoding='utf-8').read()
# split into top-level symbol blocks at "\n\t\t(symbol\n" (schematic placement symbols)
marker="\n\t(symbol\n"
parts=src.split(marker)
out=[parts[0]]
changed=0
for blk in parts[1:]:
    # does this block belong to a TP? (reference "TPnn")
    if re.search(r'\(reference "TP\d+"\)',blk):
        b2=blk.replace("(in_bom yes)","(in_bom no)").replace("(in_pos_files yes)","(in_pos_files no)")
        if b2!=blk: changed+=1
        blk=b2
    out.append(blk)
res=marker.join(out)
open(sys.argv[2],'w',encoding='utf-8').write(res)
print("TP symbol blocks updated:",changed)

#!/usr/bin/env python3
import sys, capstone
data = open('/data/gaz/src/nokia3210/ghidra_out/3210f600a_swap16.bin','rb').read()
FLASH=0x200000
def dis(va, n):
    off=va-FLASH
    md=capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
    md.detail=False
    for ins in md.disasm(data[off:off+n], va):
        # resolve pc-relative literal loads
        extra=""
        if ins.mnemonic.startswith('ldr') and '[pc' in ins.op_str:
            import re
            m=re.search(r'#(0x[0-9a-f]+)', ins.op_str)
            if m:
                pcbase=(ins.address+4)&~3
                lit=pcbase+int(m.group(1),16)
                word=int.from_bytes(data[lit-FLASH:lit-FLASH+4],'little')
                extra=f"   ; [{lit:08x}] = {word:08x}"
        print(f"{ins.address:08x}  {ins.bytes.hex():<10} {ins.mnemonic:7} {ins.op_str}{extra}")
    print()
for arg in sys.argv[1:]:
    va,n=arg.split(":"); va=int(va,16); n=int(n,0)
    print(f"==== {va:08x} (+{n}) ====")
    dis(va,n)

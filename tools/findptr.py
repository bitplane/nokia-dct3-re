import sys
import os
BIN=os.environ.get('NOKI_BIN', os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),'roms','3210f600a_swap16.bin'))
BASE=0x200000
data=open(BIN,'rb').read()
for a in sys.argv[1:]:
    v=int(a,0)
    # candidate byte encodings of a 32-bit pointer value v:
    le      = v.to_bytes(4,'little')
    hwswap  = ((v & 0xffff)<<16 | (v>>16)).to_bytes(4,'little')  # halfword-swapped
    for name,pat in (('LE',le),('HWSWAP',hwswap)):
        i=0; hits=[]
        while True:
            j=data.find(pat,i)
            if j<0: break
            hits.append(BASE+j); i=j+1
        print(f"{a} as {name} {pat.hex()}: {len(hits)} hits -> {[hex(h) for h in hits[:12]]}")

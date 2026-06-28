import sys
import os
BIN=os.environ.get('NOKI_BIN', os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),'roms','3210f600a_swap16.bin'))
BASE=0x200000
data=open(BIN,'rb').read()
addr=int(sys.argv[1],0); n=int(sys.argv[2],0) if len(sys.argv)>2 else 0x40
for o in range(addr-BASE, addr-BASE+n, 4):
    w=data[o:o+4]
    le=int.from_bytes(w,'little')
    hw=((le & 0xffff)<<16 | (le>>16)) & 0xffffffff   # halfword-swapped (pointer form)
    h0=int.from_bytes(w[0:2],'little'); h1=int.from_bytes(w[2:4],'little')
    asc=''.join(chr(b) if 32<=b<127 else '.' for b in w)
    print(f"{BASE+o:08x}: bytes={w.hex()}  hw16=[{h0:04x} {h1:04x}]  ptr(hwswap)={hw:08x}  '{asc}'")

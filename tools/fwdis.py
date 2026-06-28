import sys, capstone
import os
BIN=os.environ.get('NOKI_BIN', os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),'roms','3210f600a_swap16.bin'))
BASE=0x200000
data=open(BIN,'rb').read()
addr=int(sys.argv[1],16); n=int(sys.argv[2],0) if len(sys.argv)>2 else 64
off=addr-BASE
code=data[off:off+n]
md=capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
md.detail=False
for ins in md.disasm(code, addr):
    print(f"{ins.address:08x}: {ins.mnemonic:8s} {ins.op_str}")

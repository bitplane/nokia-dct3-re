import sys, capstone
import os
BIN=os.environ.get('NOKI_BIN', os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),'roms','3210f600a_swap16.bin'))
BASE=0x200000
data=open(BIN,'rb').read()
targets=[int(a,0) for a in sys.argv[1:]]
md=capstone.Cs(capstone.CS_ARCH_ARM, capstone.CS_MODE_THUMB)
# scan every even offset, decode one insn, check bl/b/blx target
found={t:[] for t in targets}
for off in range(0, len(data)-2, 2):
    for ins in md.disasm(data[off:off+4], BASE+off):
        if ins.mnemonic in ('bl','b','blx','beq','bne','bgt','blt','bge','ble'):
            op=ins.op_str
            if op.startswith('#'):
                try: tgt=int(op[1:],0)
                except: break
                if tgt in found:
                    found[tgt].append((ins.address, ins.mnemonic))
        break
for t in targets:
    print(f"\n=== callers/branchers to {t:#08x} ({len(found[t])}) ===")
    for a,m in found[t][:40]:
        print(f"  {a:08x}: {m} {t:#x}")

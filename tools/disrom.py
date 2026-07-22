#!/usr/bin/env python3
# ARMv4T disassembler for the swap16 flash image. Thumb-1 is the default;
# set NOKI_MODE=arm for copied ARM-mode helpers and veneers.
#
# Capstone's CS_MODE_THUMB also decodes 32-bit Thumb-2 instructions, but the
# ARM7TDMI supports only Thumb-1: the ONLY valid 4-byte instruction is the
# BL/BLX immediate pair. When a 16-bit data/instruction word matches a Thumb-2
# prefix (0xE800-0xFFFF), capstone greedily eats 4 bytes, misaligns, and emits
# garbage (vqrshrun/ldc2l/...) for the rest of the region. We step manually and
# reject any 4-byte decode that isn't bl/blx, keeping 16-bit alignment.
import os, sys, re, capstone
from pathlib import Path

default_image = Path(__file__).resolve().parent.parent / 'roms/3210f600a_swap16.bin'
data = Path(os.environ.get('NOKI_BIN', default_image)).read_bytes()
FLASH=0x200000

def resolve_lit(ins):
    # resolve pc-relative literal loads: ldr rX,[pc,#imm]
    if ins.mnemonic.startswith('ldr') and '[pc' in ins.op_str:
        m=re.search(r'#(0x[0-9a-f]+)', ins.op_str)
        if m:
            pcbase=(ins.address+4)&~3
            lit=pcbase+int(m.group(1),16)
            if 0 <= lit-FLASH <= len(data)-4:
                raw=int.from_bytes(data[lit-FLASH:lit-FLASH+4],'little')
                word=((raw & 0xffff) << 16) | (raw >> 16)
                return f"   ; [{lit:08x}] = {word:08x}"
    return ""

def emit(ins):
    print(f"{ins.address:08x}  {ins.bytes.hex():<10} {ins.mnemonic:7} {ins.op_str}{resolve_lit(ins)}")

def dis(va, n):
    arm_mode = os.environ.get("NOKI_MODE", "thumb").lower() == "arm"
    md=capstone.Cs(
        capstone.CS_ARCH_ARM,
        capstone.CS_MODE_ARM if arm_mode else capstone.CS_MODE_THUMB,
    )
    md.detail=False

    if arm_mode:
        for ins in md.disasm(data[va-FLASH:va-FLASH+n], va):
            emit(ins)
        return
    addr=va
    end=va+n
    while addr < end:
        off=addr-FLASH
        got=list(md.disasm(data[off:off+4], addr, count=1))
        # Accept a 16-bit instruction, or a genuine 4-byte bl/blx pair.
        if got and not (got[0].size==4 and got[0].mnemonic not in ('bl','blx')):
            ins=got[0]
            emit(ins)
            addr += ins.size
            continue
        # Otherwise the 4-byte decode is a Thumb-2 mis-decode (invalid on ARMv4T)
        # or this halfword is data. Try just the low 16 bits as a Thumb-1 insn.
        got2=list(md.disasm(data[off:off+2], addr, count=1))
        if got2 and got2[0].size==2:
            emit(got2[0])
        else:
            hw=int.from_bytes(data[off:off+2],'little')
            print(f"{addr:08x}  {data[off:off+2].hex():<10} {'.hword':7} 0x{hw:04x}")
        addr += 2

for arg in sys.argv[1:]:
    va,n=arg.split(":"); va=int(va,16); n=int(n,0)
    print(f"==== {va:08x} (+{n}) ====")
    dis(va,n)
    print()

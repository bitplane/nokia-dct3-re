"""Own NSK-3 integrity-only fixtures; not factory identity/calibration."""
import argparse
import hashlib
from pathlib import Path
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsk3_boot_check import check_run,NATIVE_EEPROM


def repair(image,low_block=False):
    if len(image)!=8192 or hashlib.sha256(image).hexdigest()!=NATIVE_EEPROM:
        raise ValueError('requires qualified own erased-source NSK-3 initialized EEPROM')
    result=bytearray(image)
    # Own 23aa20 allocated path sums 40..11d; 295c1e excludes 74/75.
    checksum=(sum(image[0x40:0x11e])-sum(image[0x74:0x76]))&0xffff
    result[0x11e:0x120]=checksum.to_bytes(2,'big')
    if low_block:
        # Own 26f556 compares sum16(00..3b) with a BE32 field at3c.
        result[0x3c:0x40]=(sum(image[:0x3c])&0xffff).to_bytes(4,'big')
    return bytes(result)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('source','capture','native_roms','output'):
        parser.add_argument(name,type=Path)
    parser.add_argument('--low-block',action='store_true',help='also compute the own 003c BE32 checksum; no semantic fields changed')
    args=parser.parse_args()
    try:
        if args.output.exists():raise ValueError('checksum fixture requires a fresh output')
        check_run(args.source,args.capture,'boot4',args.native_roms)
        before=(args.capture/'nvram/nsk3r4t/eeprom').read_bytes()
        result=repair(before,args.low_block)
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_bytes(result)
        print(f'wrote {args.output}; checksum011e={result[0x11e:0x120].hex()}, low-block repair={args.low_block}')
    except (ValueError,OSError) as error:
        print(f'NSK-3 checksum fixture failed: {error}',file=sys.stderr)
        return 1
    return 0


if __name__=='__main__':raise SystemExit(main())

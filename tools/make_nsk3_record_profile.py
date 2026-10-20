"""Own-capture NSK-3 published-record diagnostic; not factory provisioning."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsk3_boot_check import check_run
from tools.nsk3_startup_check import check_record_math
from tools.make_nsk3_checksum_profile import repair
from tools.dct3_rom4_record_codec import encode_record,retained_record_transform
from tools.dct3_lock_record_example import SOURCE,EXAMPLE_RECORDS


def build(image,chip):
    result=bytearray(repair(image,True))
    encoded=b''.join(encode_record(EXAMPLE_RECORDS[i:i+12],chip) for i in (0,12))
    # Own 26ec6a reads20..37; 26e0be applies the independently checked involution.
    result[0x20:0x38]=retained_record_transform(encoded,image[:12])
    result[0x3c:0x40]=(sum(result[:0x3c])&0xffff).to_bytes(4,'big')
    return bytes(result)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('source','capture','native_roms','output'):
        parser.add_argument(name,type=Path)
    args=parser.parse_args()
    try:
        if args.output.exists():raise ValueError('record diagnostic requires a fresh output')
        check_run(args.source,args.capture,'boot4',args.native_roms)
        records=check_record_math((args.capture/'error.log').read_text())
        chips={record['chip_field'] for record in records}
        if len(chips)!=1:raise ValueError('own modeled chip field is not stable')
        chip=bytes.fromhex(chips.pop())
        before=(args.capture/'nvram/nsk3r4t/eeprom').read_bytes()
        if before[:64]!=b'\xff'*64:raise ValueError('own low block is no longer erased')
        result=build(before,chip)
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_bytes(result)
        metadata={'classification':'published-record diagnostic, not factory data',
                  'example_source':SOURCE,'own_chip_field':chip.hex(),
                  'source_capture':str(args.capture.resolve()),
                  'source_eeprom_sha256':hashlib.sha256(before).hexdigest(),
                  'output_sha256':hashlib.sha256(result).hexdigest(),
                  'written_ranges':['0020..0037','003c..003f','011e..011f'],
                  'identity_and_calibration':'preserved from own erased-source control'}
        args.output.with_name('record_input.json').write_text(json.dumps(metadata,indent=2)+'\n')
        print(f'wrote {args.output}; own chip {chip.hex()}, diagnostic records/checksums only')
    except (ValueError,OSError) as error:
        print(f'NSK-3 record diagnostic failed: {error}',file=sys.stderr)
        return 1
    return 0


if __name__=='__main__':raise SystemExit(main())

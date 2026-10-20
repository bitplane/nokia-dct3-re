"""NSM-1 own-capture record diagnostic; not factory or donor provisioning."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsm1_record_exchange_check import check
from tools.dct3_rom4_record_codec import encode_record,retained_record_transform
from tools.dct3_lock_record_example import EXAMPLE_RECORDS,SOURCE


def build(before,chip):
    if len(before)!=0x4000:raise ValueError('NSM-1 EEPROM extent differs')
    if chip!=bytes.fromhex('00160010'):raise ValueError('unexpected measured compatibility chip field')
    result=bytearray(before)
    encoded=b''.join(encode_record(EXAMPLE_RECORDS[i:i+12],chip) for i in (0,12))
    result[0x20:0x38]=retained_record_transform(encoded,before[:12])
    result[0x3c:0x40]=(sum(result[:0x3c])&0xffff).to_bytes(4,'big')
    return bytes(result)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('control','eeprom','output'):parser.add_argument(name,type=Path)
    args=parser.parse_args()
    try:
        if args.output.exists():raise ValueError('fresh output required')
        report=check((args.control/'error.log').read_text())
        chips={bytes.fromhex(identity)[4:8] for identity in report['identity_completions']}
        if len(chips)!=1:raise ValueError('inconsistent measured chip identity')
        before=args.eeprom.read_bytes()
        if hashlib.sha1(before).hexdigest()!='07335e492f72ba9da9c3436890c9eb4af67fd9a9':
            raise ValueError('requires exact own checksum-consistent NSM-1 repair template')
        result=build(before,chips.pop())
        args.output.parent.mkdir(parents=True,exist_ok=True)
        args.output.write_bytes(result)
        args.output.with_suffix('.json').write_text(json.dumps({
            'classification':'published-record diagnostic, not factory data',
            'example_source':SOURCE,'control':str(args.control),
            'changed_offsets':[i for i,(a,b) in enumerate(zip(before,result)) if a!=b],
            'sha256':hashlib.sha256(result).hexdigest()},indent=2)+'\n')
    except (ValueError,OSError) as error:
        print(f'NSM-1 diagnostic preparation failed: {error}',file=sys.stderr);return 1
    return 0


if __name__=='__main__':raise SystemExit(main())

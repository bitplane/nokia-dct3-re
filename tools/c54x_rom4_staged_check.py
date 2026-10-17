"""Pin the NSE-1 staged-frontend stop; this is not a boot acceptance gate."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import sys


SOURCE_OFFSET = 0x99704
STOP = ('staged ADDM register/peripheral destination is unsupported '
        'instruction=000f1f word=6bf8 destination=0013')
ROW = re.compile(r'rom4_staged_frontier: pc=([0-9a-f]{4}) xpc=([0-9a-f]{2}) '
                 r'illegal=([01]) pmst=([0-9a-f]{4}) swwsr=([0-9a-f]{4}) '
                 r'bscr=([0-9a-f]{4}) swcr=([0-9a-f]{4}) words=([0-9a-f,]+) t=([0-9.]+)')


def check(text, firmware):
    rows = ROW.findall(text)
    if len(rows) != 1 or text.count('rom4_staged_frontier:') != 1:
        raise ValueError('missing, malformed or duplicate staged frontier observation')
    pc, xpc, illegal, pmst, swwsr, bscr, swcr, encoded, timestamp = rows[0]
    if (pc, xpc, illegal, pmst, swwsr, bscr, swcr) != ('0f23', '00', '1', 'ffa8', '2000', '0000', '0000') or not 0.5 <= float(timestamp) <= 1:
        raise ValueError('staged CPU stop changed; reassess the actual frontier')
    if text.count(STOP) != 1 or 'unsupported staged C54x form' in text:
        raise ValueError('missing or ambiguous unsupported instruction')
    words = [int(word, 16) for word in encoded.split(',')]
    if (len(words) != 40 or words[5:9] != [0x7728, 0x2000, 0x7758, 0x3146]
            or words[18:21] != [0x7cf8, 0x0802, 0xff87]
            or words[31:34] != [0x6bf8, 0x0013, 0x0800]):
        raise ValueError('uploaded instruction or operand changed')
    packed = struct.pack('>40H', *words)
    if firmware.find(packed) != SOURCE_OFFSET or firmware.find(packed, SOURCE_OFFSET + 1) != -1:
        raise ValueError('observed upload does not bind uniquely to the reviewed NSE-1 MCU source')
    return dict(status='unsupported_staged_ar3_addm', source_offset=SOURCE_OFFSET,
                observed_words=40, firmware_sha256=hashlib.sha256(firmware).hexdigest(),
                instruction_address=0x0f1f, instruction=0x6bf8,
                destination=0x0013, immediate=0x0800,
                swwsr=0x2000, boot_claim=False,
                upload_complete_claim=False, fitted_bus_dialect_claim=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('firmware', type=Path)
    args = parser.parse_args()
    try:
        result = check(args.log.read_text(errors='replace'), args.firmware.read_bytes())
    except (OSError, ValueError, struct.error) as error:
        print(f'Staged ROM4 observation: FAIL: {error}', file=sys.stderr)
        return 1
    print(json.dumps(result, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

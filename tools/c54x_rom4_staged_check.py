"""Pin the NSE-1 staged-frontend stop; this is not a boot acceptance gate."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import sys


SOURCE_OFFSET = 0x94cf8
STOP = 'unsupported staged C54x form 34f8 at 0031b3'
ROW = re.compile(r'rom4_staged_frontier: pc=([0-9a-f]{4}) xpc=([0-9a-f]{2}) '
                 r'illegal=([01]) pmst=([0-9a-f]{4}) swwsr=([0-9a-f]{4}) '
                 r'bscr=([0-9a-f]{4}) swcr=([0-9a-f]{4}) words=([0-9a-f,]+) t=([0-9.]+)')


def check(text, firmware):
    rows = ROW.findall(text)
    if len(rows) != 1 or text.count('rom4_staged_frontier:') != 1:
        raise ValueError('missing, malformed or duplicate staged frontier observation')
    pc, xpc, illegal, pmst, swwsr, bscr, swcr, encoded, timestamp = rows[0]
    if (pc, xpc, illegal, pmst, swwsr, bscr, swcr) != ('31b2', '00', '1', 'ffac', '2000', '0000', '0000') or not 0.5 <= float(timestamp) <= 1:
        raise ValueError('staged CPU stop changed; reassess the actual frontier')
    if text.count(STOP) != 1 or text.count('unsupported staged C54x form') != 1:
        raise ValueError('missing or ambiguous unsupported instruction')
    summaries = re.findall(r'^rom4_interface_summary: completion_strobes=(\d+) mailbox_writes=(\d+) ', text, re.M)
    if summaries != [('1', '70')]:
        raise ValueError('missing or changed first completion-strobe boundary')
    words = [int(word, 16) for word in encoded.split(',')]
    if (len(words) != 40 or words[:7] != [0x7718, 0x1ec6, 0x7728, 0x2000, 0xf7be, 0x7758, 0x3146]
            or words[13:16] != [0xf071, 0x079f, 0x8093]
            or words[18:21] != [0xf071, 0x1fff, 0x8093]):
        raise ValueError('uploaded instruction or operand changed')
    packed = struct.pack('>40H', *words)
    if firmware.find(packed) != SOURCE_OFFSET or firmware.find(packed, SOURCE_OFFSET + 1) != -1:
        raise ValueError('observed upload does not bind uniquely to the reviewed NSE-1 MCU source')
    return dict(status='unsupported_staged_absolute_bitt', source_offset=SOURCE_OFFSET,
                observed_words=40, firmware_sha256=hashlib.sha256(firmware).hexdigest(),
                instruction_address=0x31b3, instruction=0x34f8, completion_strobes=1,
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

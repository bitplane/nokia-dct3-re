"""Check unprovisioned NSB-7 compact self-test and remaining checksum failure."""
import argparse
import hashlib
from pathlib import Path
import re

try:
    from tools.nsb7_boot_check import IMAGE_SHA1
except ModuleNotFoundError:
    from nsb7_boot_check import IMAGE_SHA1


def check_trace(text):
    if re.findall(r'nsb7_profile: machine=(\w+) observer_writes=0', text) != ['nsb7hle']:
        raise ValueError('requires explicit NSB-7 research HLE')
    patterns = [
        r'loader2_verified words=623 entry=0a00',
        r'runtime_hle_handoff pc=2c75 native_suspended=1',
        r'TX pending type=05 payload=10 data=1eff00d000030101e000',
        r'RX enqueue type=8e payload=10 [^\n]*data=1e0002d000030401c100',
        r'TX pending type=70 payload=2 data=0d00',
        r'RX enqueue type=74 payload=2 [^\n]*data=0d00',
        r'nsb7_selftest: pc=00248b74 command=0d reply=00 flags=84 faults=ff,ff,ff',
        r'nsb7_selftest: pc=00248c28 command=0d reply=00 flags=80 faults=00,00,00',
        r'TX pending type=70 payload=2 data=0a09',
    ]
    positions = []
    for pattern in patterns:
        matches = list(re.finditer(pattern, text))
        if len(matches) != 1:
            raise ValueError('missing or repeated lifecycle event: ' + pattern)
        positions.append(matches[0].start())
    # Follow-up submission can occur before the traced handler join. Require
    # both after consumer entry without inventing an ordering between them.
    if positions[:7] != sorted(positions[:7]) or min(positions[7:]) <= positions[6]:
        raise ValueError('self-test lifecycle out of order')
    for pattern in (
            r'nsb7_calibration_compare: computed=bd42 stored=ffff marker=ffff',
            r'nsb7_fault12: pc=00248948 value=0c mask=ff000000',
            r'nsb7_prefix_compare: computed=00001ae4 stored=ffffffff',
            r'nsb7_result: index=12 pc=002e8ffc value=fd mask=0000ff00',
            r'nsb7_status: flags=00 faults=ffff00ff00ff000000ffff000cff00000000120000ffffffffffffffff t=6\.000000000'):
        if len(re.findall(pattern, text)) != 1:
            raise ValueError('unprovisioned checksum boundary differs')
    if 'LUA ERROR' in text or 'c54x_failure:' in text:
        raise ValueError('runtime observation failed')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('run', type=Path)
    args = parser.parse_args()
    try:
        if hashlib.sha1(args.image.read_bytes()).hexdigest() != IMAGE_SHA1:
            raise ValueError('requires qualified own NSB-7 firmware')
        check_trace((args.run / 'error.log').read_text())
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSB-7 service: FAIL: {error}\n')
    print('PASS: own compact self-test consumed; erased checksum failure remains; no idle claim')


if __name__ == '__main__':
    main()

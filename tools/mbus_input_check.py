"""Require byte-event delivery and firmware reads, not service-command success."""
import argparse
from decimal import Decimal
import json
from pathlib import Path
import re


REPEATED = bytes.fromhex('1f0010d10006a5a5ffff000000d8')
VERSION_QUERY = bytes.fromhex('1f0010d10005000100030001d8')


def verify(text, *, version_query=False):
    expected = VERSION_QUERY if version_query else REPEATED
    start = Decimal(20 if version_query else 8)
    if '[LUA ERROR]' in text or text.count('mbus_input_probe: complete') != 1:
        raise ValueError('MBUS byte probe did not complete')
    rows = list(re.finditer(r'mbus_input_probe: index=(\d+) data=([0-9a-f]{2}) t=([0-9.]+)', text))
    if len(rows) != len(expected):
        raise ValueError('missing or duplicated byte-input events')
    for index, (byte, row) in enumerate(zip(expected, rows)):
        number, data, time = row.groups()
        if (int(number) != index + 1 or int(data, 16) != byte or
                Decimal(time) != start + Decimal('0.0011') * index):
            raise ValueError('byte input or sub-frame cadence differs')
        end = rows[index + 1].start() if index + 1 < len(rows) else text.index('mbus_input_probe: complete')
        interval = text[row.end():end]
        accepted = re.findall(r'mbus_fixture: data=([0-9a-f]{2}) accepted=(\d+)', interval)
        reads = re.findall(r'mbus_device: event=rx_read data=([0-9a-f]{2})\b', interval)
        if accepted != [(f'{byte:02x}', '1')] or reads != [f'{byte:02x}']:
            raise ValueError('byte was dropped, coalesced, rejected or not read before the next input')
    transmitted = re.findall(r'mbus_device: event=tx_complete data=([0-9a-f]{2})\b', text[rows[0].start():])
    return {'bytes_delivered_and_read': len(expected), 'gap_ms': 1.1,
            'logged_tx_completions': ''.join(transmitted),
            'scope': 'Controller ingress and firmware byte reads only; no service reply or provisioning acceptance.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--version-query', action='store_true')
    args = parser.parse_args()
    try:
        print(json.dumps(verify(args.log.read_text(errors='replace'), version_query=args.version_query), indent=2))
    except (OSError, ValueError) as error:
        parser.exit(1, f'MBUS byte ingress FAIL: {error}\n')


if __name__ == '__main__':
    main()

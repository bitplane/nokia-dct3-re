"""Check observed NSK-1 native radio boundary, not registration or RF validity."""
import argparse
import json
from pathlib import Path
import re


def check(text):
    packets = re.findall(r'nsk1_radio_packet: direction=(tx|rx) index=(\d+) type=([0-9a-f]{2}) length=(\d+) data=([0-9a-f]+)', text)
    tx = [(kind, int(length)) for direction, _, kind, length, _ in packets if direction == 'tx']
    rx = [(kind, int(length)) for direction, _, kind, length, _ in packets if direction == 'rx']
    if tx != [('51', 80)] * 6 + [('51', 16), ('1a', 68), ('4a', 4)] or rx != [('8b', 162)]:
        raise ValueError('native packet order or lengths changed')
    counts = {'tx': 0, 'rx': 0}
    for direction, index, kind, length, payload in packets:
        if len(payload) != int(length) * 2:
            raise ValueError('packet payload length mismatch')
        counts[direction] += 1
        if int(index) != counts[direction]:
            raise ValueError('packet index discontinuity')
    if 'nsk1_radio_summary: tx=9 rx=1 cap=128 start=2 end=59 observer_writes=0' not in text:
        raise ValueError('missing bounded transport summary')
    entries = re.findall(r'nsk1_radio_native_entry: pc=([0-9a-f]+)', text)
    allowed = {'3000', '3076', '30bf', '30f5', '3d5e', '313f', '3153', '3aac', '4a3c', '4a49'}
    if not {'3000', '3076', '3d5e'} <= set(entries) or not set(entries) <= allowed:
        raise ValueError('missing positive DSP entry control or wrong CPU scope')
    if 'nsk1_radio_native_completion:' in text:
        raise ValueError('previously absent native completion publisher executed')
    writes = re.findall(r'nsk1_radio_native_control_write: address=(06d[01]) value=([0-9a-f]{4})', text)
    resets = [index for index, row in enumerate(writes) if row == ('06d0', '0000')]
    if not resets:
        raise ValueError('missing native control initialization')
    final = writes[resets[-1] + 1:]
    if [value for address, value in final if address == '06d0'] != ['0001', '0002', '0008', '0003']:
        raise ValueError('post-reset pending-index request order changed')
    for address in ('06d0', '06d1'):
        if sum(row[0] == address for row in writes) >= 32:
            raise ValueError('native control observation cap reached')
    completions = []
    for address, value in final:
        if address == '06d1' and (not completions or completions[-1] != value):
            completions.append(value)
    if completions != ['0000', '0002', '0006', '0106', '010e']:
        raise ValueError('native control completion progression changed')
    stack = 'words=0e13,008b,00a2,4814,0a2e,30ed,38ca,3976'
    if not re.search(r'nsk1_radio_native_rx_stack: pc=3805 sp=[0-9a-f]{4} ' + stack, text):
        raise ValueError('missing live RSSI publisher stack')
    return {'tx': len(tx), 'rx': len(rx), 'pending_indexes': [1, 2, 8, 3],
            'live_rssi_stack': True, 'registration_proved': False, 'rf_validity_proved': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(check(args.log.read_text()), indent=2))
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSK-1 radio boundary: FAIL: {error}\n')


if __name__ == '__main__':
    main()

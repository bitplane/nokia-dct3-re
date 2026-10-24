"""Check bounded native NSB-3 MCU/DSP ring activity, not RF registration."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb3_nominal_input import IMAGE_SHA256
from tools.nsb3_menu_check import check_trace as check_menu

ROUTINES = {
    0x29419e: (124, '668f5e265ec279f4b7dac74fa134d86b40a514e31ad8eb74f40aed45fe48187b'),
    0x29421a: (52, 'b33ff740416584b129ca4b0549f153491dbea737d8fc98a87fc5dd59d9c91d9b'),
    0x294268: (118, 'd7cece5ab68a2ca720f62526e0414dbb53ad3b72af6c4543f7630205fdbfdda5'),
    0x2942fe: (94, '0ef90d2704f1c0b75d05071a9aaf285907cbef9fc9a8133c6e847425f9ad2edb'),
    0x294374: (36, 'bfc81a96569c4fa0e811fcdef4755fd2fcd046dfd62b5e894fbac7a391d57c80'),
    0x219c7e: (202, '2511fcb04e00cefa92b82b2420fbc7a8f0462de07b41fa6f0c0fbf39376db7d5'),
    0x219d82: (116, 'fc4ab7df6028e3f8df7f194a2fc40d79eb0cdbf4a3148cd5f82cad0b1cb6d9cc'),
    0x218bb0: (76, '24d2f695f12c54349e6cce0e18452cdc2b3e12af1e1b55a4809f62545294f903'),
    0x28a2a6: (54, '2bf04be3da5c7dbc0a736bbd919a527c3dfbb72c159d19972895d00b576cf9b4'),
    0x2126f2: (28, '8d3dc05de851e18069af856669ce9759e2bb542a9752f56a5c79b5392bb13859'),
    0x212880: (32, '552a2a65c8bc0c183055a0ab438f8e5bdd7f7100ba81bd6e57eb34947b93e487'),
}
DIGESTS = {'tx': '4d27ec28fd959770247d0e858d7052dd61fa20975584ca10d7dd761123632e6c',
           'rx': '521c35ae4087fcd95e1eef6abeecf73a749cdbf5ea418e0b97c5f53c083b03ed'}


def contract(image):
    if hashlib.sha256(image).hexdigest() != IMAGE_SHA256:
        raise ValueError('requires own NSB-3 image')
    for address, (length, digest) in ROUTINES.items():
        offset = address - 0x200000
        if hashlib.sha256(image[offset:offset + length]).hexdigest() != digest:
            raise ValueError(f'own ring routine differs at {address:x}')
    if image[0x9457c:0x94588] != bytes.fromhex('000101ca000101c800010100'):
        raise ValueError('own receive-ring literals differ')
    if (int.from_bytes(image[0xb3ab8:0xb3abc], 'big') != 0x2b3ae4 or
            image[0xb3ae4:0xb3af2] != bytes.fromhex('1c20f7d6fd5ce0411c20f7d6fcb7') or
            image[0x8a690:0x8a694] != bytes.fromhex('000013b8')):
        raise ValueError('own 8b dispatcher/forwarder status differs')
    if image[0x8a650:0x8a654] != bytes.fromhex('0000040b'):
        raise ValueError('own type80 decoder status differs')


def packets(text, direction):
    rows = re.findall(r'nsb3_radio_' + direction + r': index=(\d+) type=([0-9a-f]{2}) length=(\d+) '
                      r'data=([0-9a-f]*) cursor=([0-9a-f]+) producer=([0-9a-f]+) '
                      r'(?:dsp_)?pc=([0-9a-f]+) t=([0-9.]+) observer_writes=0', text)
    limit, start, size = (256, 0, 0x52) if direction == 'tx' else (128, 0x80, 0x64)
    if not rows or len(rows) >= limit:
        raise ValueError('missing/capped ring capture')
    cursor = start
    for index, row in enumerate(rows, 1):
        number, kind, length, body, position, producer, pc, time = row
        length, position, producer = int(length), int(position, 16), int(producer, 16)
        if int(number) != index or len(body) != length * 2 or position != cursor:
            raise ValueError('packet order, byte count or ring cursor differs')
        if not start <= producer < start + size:
            raise ValueError('producer outside own ring')
        words = (length + 3) // 2
        if words > (producer - position) % size:
            raise ValueError('packet exceeds published ring extent')
        cursor = start + ((position - start + words) % size)
    digest = hashlib.sha256('\n'.join(f'{kind}:{length}:{body}'
                            for _, kind, length, body, *_ in rows).encode()).hexdigest()
    if digest != DIGESTS[direction]:
        raise ValueError('qualified ' + direction + ' packet inventory differs')
    return rows


def check(text):
    check_menu(text)
    tx, rx = packets(text, 'tx'), packets(text, 'rx')
    if len(tx) != 32 or len(rx) != 15 or 'nsb3_radio_invalid:' in text:
        raise ValueError('native inventory or extent failure differs')
    states = re.findall(r'nsb3_radio_state: tx=32 invalid=0 producer=002d consumer=002d '
                        r'rx_producer=008b rx_consumer=008b dsp_pc=09b9 illegal=0 '
                        r't=([0-9.]+) observer_writes=0', text)
    if [float(time) for time in states] != [8, 20, 35]:
        raise ValueError('bounded settled ring/native state differs')
    receives = re.findall(r'nsb3_radio_receive_count: packets=14 invalid=0 t=([0-9.]+) observer_writes=0', text)
    if [float(time) for time in receives] != [8, 20, 35]:
        raise ValueError('receive capture count differs')
    later = re.findall(r'nsb3_radio_state: tx=32 invalid=0 producer=002d consumer=002d '
                       r'rx_producer=00dd rx_consumer=00dd dsp_pc=([0-9a-f]+) illegal=0 '
                       r't=([0-9.]+) observer_writes=0', text)
    if [float(time) for _, time in later] != [45, 60]:
        raise ValueError('late native receive/consumption state differs')
    if rx[-1][1:3] != ('8b', '162') or not 40 < float(rx[-1][-1]) < 45:
        raise ValueError('late 8b result missing from qualified interval')
    if 'nsb3_radio_8b_forward:' not in text:
        raise ValueError('own MCU 8b forwarder was not observed')
    return {'tx_packets': len(tx), 'rx_packets': len(rx),
            'tx_last': {'type': tx[-1][1], 'body': tx[-1][3]},
            'rx_types': sorted({row[1] for row in rx}),
            'rf_registration_proved': False, 'fitted_mask_identity_proved': False}


def check_consumer(text, cached):
    received = re.findall(r'nsb3_radio_rx: index=15 type=8b length=162 data=([0-9a-f]+) ', text)
    if len(received) != 1 or len(cached) != 160 or bytes.fromhex(received[0])[2:] != cached:
        raise ValueError('cached vector differs from native 8b payload')
    objects = re.findall(r'nsb3_radio_8b_forward: message=([0-9a-f]{8}) header=1802a28b observer_writes=0', text)
    if len(objects) != 1:
        raise ValueError('own native object forwarding evidence missing')
    token = ('nsb3_radio_object_read: object=' + objects[0] +
             ' data=13b80000 mask=ffff0000 pc=00219c7e')
    if token not in text:
        raise ValueError('same 13b8 object was not read by task12 consumer')
    if ('nsb3_radio_result_branch: object=' + objects[0] +
            ' state=03 phase=01 cache=00000000 observer_writes=0') not in text:
        raise ValueError('deferred result branch state differs')
    tails = re.findall(r'nsb3_radio_result_tail: state=03 phase=01 cache=([0-9a-f]{8}) observer_writes=0', text)
    captures = re.findall(r'nsb3_radio_cached_vector: pointer=([0-9a-f]{8}) bytes=160 state=03 phase=01 t=45 observer_writes=0', text)
    if len(tails) != 1 or tails != captures or not 0x100000 <= int(tails[0], 16) <= 0x110000 - 166:
        raise ValueError('cached object/retained lifecycle evidence differs')
    writes = re.findall(r'nsb3_radio_controller_state: value=([0-9a-f]+) pc=([0-9a-f]+) ', text)
    if writes != [('00', '0020010e'), ('03', '00219c3e')]:
        raise ValueError('controller initialization/retained state differs')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('run', type=Path)
    parser.add_argument('--consumer', action='store_true', help='also verify same-object task12 cache/defer contract')
    args = parser.parse_args()
    try:
        contract(args.image.read_bytes())
        text = (args.run / 'error.log').read_text()
        result = check(text)
        if args.consumer:
            check_consumer(text, (args.run / 'nsb3_radio_cached_vector.bin').read_bytes())
            result['task12_result_cached_while_state3'] = True
    except (OSError, ValueError) as exc:
        parser.exit(1, f'NSB-3 native radio inventory: FAIL: {exc}\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()

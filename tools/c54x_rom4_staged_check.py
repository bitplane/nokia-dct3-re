"""Pin the NSE-1 staged-frontend stop; this is not a boot acceptance gate."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import sys


SOURCE_OFFSET = 0x94cf8
TAIL_SOURCE_OFFSET = 0x98a60
REPEAT_SOURCE_OFFSET = 0x98af0
STOP = 'unsupported staged C54x form d6e1 at 003260'
ROW = re.compile(r'rom4_staged_frontier: pc=([0-9a-f]{4}) xpc=([0-9a-f]{2}) '
                 r'illegal=([01]) pmst=([0-9a-f]{4}) swwsr=([0-9a-f]{4}) '
                 r'bscr=([0-9a-f]{4}) swcr=([0-9a-f]{4}) words=([0-9a-f,]+) t=([0-9.]+)')


def check(text, firmware):
    rows = ROW.findall(text)
    if len(rows) != 1 or text.count('rom4_staged_frontier:') != 1:
        raise ValueError('missing, malformed or duplicate staged frontier observation')
    pc, xpc, illegal, pmst, swwsr, bscr, swcr, encoded, timestamp = rows[0]
    if (pc, xpc, illegal, pmst, swwsr, bscr, swcr) != ('3262', '00', '1', 'ffac', '2000', '0000', '0000') or not 0.5 <= float(timestamp) <= 1:
        raise ValueError('staged CPU stop changed; reassess the actual frontier')
    if text.count(STOP) != 1 or text.count('unsupported') != 1:
        raise ValueError('missing or ambiguous unsupported instruction')
    if (text.count('rom4_staged_dagen:') != 1
            or re.findall(r'rom4_staged_dagen: bk=([0-9a-f]{4}) ar0=([0-9a-f]{4}) ar2=([0-9a-f]{4}) st0=([0-9a-f]{4}) st1=([0-9a-f]{4})', text) != [('0010', '0002', '06d2', '081f', 'e900')]):
        raise ValueError('missing, duplicate or changed native status/address-generator observation')
    summaries = re.findall(r'^rom4_interface_summary: completion_strobes=(\d+) mailbox_writes=(\d+) ', text, re.M)
    if summaries != [('4', '74')]:
        raise ValueError('missing or changed bootstrap completion boundary')
    rf = re.findall(r'^rom4_rf_read: sample=(\d+) pc=([0-9a-f]{4}) frame=(\d+) t=([0-9.]+)$', text, re.M)
    if (text.count('rom4_rf_read:') != 4
            or [row[:3] for row in rf] != [('1', '324e', '30'), ('2', '3254', '30'), ('3', '325a', '30'), ('4', '3260', '30')]
            or not all(0.15 < float(row[3]) < float(timestamp) for row in rf)
            or not all(float(before[3]) <= float(after[3]) for before, after in zip(rf, rf[1:]))
            or re.findall(r'^rom4_interface_summary: .* rf_reads=(\d+) ', text, re.M) != ['4']):
        raise ValueError('missing, malformed or changed native RF provider reads')
    bsp = re.findall(r'rom4_staged_bsp_write: value=([0-9a-f]{4}) mask=([0-9a-f]{4}) t=([0-9.]+)', text)
    if text.count('rom4_staged_bsp_write:') != 6 or [(value, mask) for value, mask, _ in bsp] != [('c008', 'ffff'), ('c0c8', 'ffff')] + [('c008', 'ffff')] * 4:
        raise ValueError('missing, duplicate or changed native BSP reset/release writes')
    bsp_times = [float(row[2]) for row in bsp]
    if not 0 < bsp_times[0] < bsp_times[-1] < float(timestamp) or not all(before < after for before, after in zip(bsp_times, bsp_times[1:])):
        raise ValueError('native BSP publication order changed')
    tx = re.findall(r'rom4_staged_bsp_tx: index=(\d+) value=([0-9a-f]{4}) mask=([0-9a-f]{4}) t=([0-9.]+)', text)
    if (text.count('rom4_staged_bsp_tx:') != 2 or len(tx) != 2
            or [row[:3] for row in tx] != [('1', '0aaa', 'ffff'), ('2', 'ffd5', 'ffff')]
            or not float(bsp[1][2]) < float(tx[0][3]) < float(tx[1][3]) < float(bsp[2][2])
            or re.findall(r'rom4_staged_bsp_tx_count: count=(\d+)', text) != ['2']):
        raise ValueError('missing, duplicate or changed native BSP transmit publication')
    rx = re.findall(r'rom4_staged_bsp_rx: index=(\d+) value=([0-9a-f]{4}) mask=([0-9a-f]{4}) t=([0-9.]+)', text)
    if (text.count('rom4_staged_bsp_rx:') != 2 or len(rx) != 2
            or [row[:3] for row in rx] != [('1', '0aaa', 'ffff'), ('2', '0aaa', 'ffff')]
            or not float(tx[0][3]) < float(rx[0][3]) < float(rx[1][3]) < float(tx[1][3]) < float(timestamp)
            or re.findall(r'rom4_staged_bsp_rx_count: count=(\d+)', text) != ['2']):
        raise ValueError('missing, duplicate or changed native BSP receive observations')
    expected_low = {
        'low_write': [('00f1', '0000', 'ffff', '0f10'), ('00f2', '0000', 'ffff', '0f10'),
                      ('00f2', '0003', 'ffff', 'a5e5'), ('00f2', '0003', 'ffff', 'a5e5')],
        'low_read': [('00f1', '0000', 'ffff', 'a5ed'), ('00f2', '0003', 'ffff', 'a5ef')] * 2,
        'gap_read': [('0003', '0000', 'ffff', pc) for pc in ('a5f4', 'a5f8', 'a601')] * 2,
    }
    low = {}
    for tag, expected in expected_low.items():
        prefix = f'rom4_staged_{tag}:'
        low[tag] = re.findall(prefix + r' address=([0-9a-f]{4}) value=([0-9a-f]{4}) mask=([0-9a-f]{4}) pc_context=([0-9a-f]{4}) t=([0-9.]+)', text)
        if text.count(prefix) != len(expected) or [row[:4] for row in low[tag]] != expected:
            raise ValueError('missing, malformed or changed low-page bus observation')
    ordered = [*low['low_write'][:3], *low['low_read'][:2], *low['gap_read'][:3],
               low['low_write'][3], *low['low_read'][2:], *low['gap_read'][3:]]
    times = [float(row[4]) for row in ordered]
    if (not all(0 < before < after < float(timestamp) for before, after in zip(times, times[1:]))
            or re.findall(r'rom4_staged_low_count: reads=(\d+) writes=(\d+)', text) != [('4', '4')]
            or re.findall(r'rom4_staged_gap_count: reads=(\d+)', text) != ['6']):
        raise ValueError('changed low-page observation ordering or counts')
    words = [int(word, 16) for word in encoded.split(',')]
    if (len(words) != 40 or words[:7] != [0x7718, 0x1ec6, 0x7728, 0x2000, 0xf7be, 0x7758, 0x3146]
            or words[13:16] != [0xf071, 0x079f, 0x8093]
            or words[18:21] != [0xf071, 0x1fff, 0x8093]):
        raise ValueError('uploaded instruction or operand changed')
    packed = struct.pack('>40H', *words)
    if firmware.find(packed) != SOURCE_OFFSET or firmware.find(packed, SOURCE_OFFSET + 1) != -1:
        raise ValueError('observed upload does not bind uniquely to the reviewed NSE-1 MCU source')
    tails = re.findall(r'^(?:\[luaengine\] )?rom4_staged_uploaded_tail: base=0e10 words=([0-9a-f,]+)$', text, re.M)
    if len(tails) != 1 or text.count('rom4_staged_uploaded_tail:') != 1:
        raise ValueError('missing, malformed or duplicate uploaded tail')
    tail = [int(word, 16) for word in tails[0].split(',')]
    if (len(tail) != 24 or tail[:8] != [0x7683, 1, 0xf020, 0x0e05, 0x7ef8, 0x001a, 0x0083, 0xf072]
            or tail[16:20] != [0x7722, 0xc008, 0x7722, 0xc0c8]):
        raise ValueError('uploaded READA/BSP control instructions changed')
    packed_tail = struct.pack('>24H', *tail)
    if firmware.find(packed_tail) != TAIL_SOURCE_OFFSET or firmware.find(packed_tail, TAIL_SOURCE_OFFSET + 1) != -1:
        raise ValueError('uploaded tail does not bind uniquely to the reviewed MCU source')
    loops = re.findall(r'^(?:\[luaengine\] )?rom4_staged_repeat_tail: base=0e58 words=([0-9a-f,]+)$', text, re.M)
    if len(loops) != 1 or text.count('rom4_staged_repeat_tail:') != 1:
        raise ValueError('missing, malformed or duplicate uploaded repeat loop')
    loop = [int(word, 16) for word in loops[0].split(',')]
    if (len(loop) != 24 or loop[:6] != [0x771a, 0x1388, 0xf072, 0x0e62, 0x4920, 0x4816]
            or loop[7:11] != [0xf845, 0x0e66, 0x7721, 0x0aaa] or loop[14] != 0xf6bf):
        raise ValueError('uploaded loop exit or serial instructions changed')
    packed_loop = struct.pack('>24H', *loop)
    if firmware.find(packed_loop) != REPEAT_SOURCE_OFFSET or firmware.find(packed_loop, REPEAT_SOURCE_OFFSET + 1) != -1:
        raise ValueError('uploaded loop does not bind uniquely to the reviewed MCU source')
    return dict(status='unsupported_staged_parallel_mac', source_offset=SOURCE_OFFSET,
                tail_source_offset=TAIL_SOURCE_OFFSET, observed_tail_words=24,
                repeat_source_offset=REPEAT_SOURCE_OFFSET, observed_repeat_words=24,
                observed_words=40, firmware_sha256=hashlib.sha256(firmware).hexdigest(),
                instruction_address=0x3260, instruction=0xd6e1, low_gap_bus_reads=6, rf_sample_reads=4,
                bsp_transmit_words=[0x0aaa, 0xffd5],
                bsp_receive_words=[0x0aaa, 0x0aaa],
                bsp_control_writes=[0xc008, 0xc0c8] + [0xc008] * 4, completion_strobes=4,
                swwsr=0x2000, boot_claim=False,
                upload_complete_claim=False, fitted_bus_dialect_claim=False,
                physical_low_gap_identity_claim=False, rf_acquisition_claim=False)


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

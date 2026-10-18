"""Pin organic NSE-1 host-packet entry and its staged instruction stop."""
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
ROW = re.compile(r'rom4_staged_frontier: pc=([0-9a-f]{4}) xpc=([0-9a-f]{2}) '
                 r'illegal=([01]) pmst=([0-9a-f]{4}) swwsr=([0-9a-f]{4}) '
                 r'bscr=([0-9a-f]{4}) swcr=([0-9a-f]{4}) words=([0-9a-f,]+) t=([0-9.]+)')


def check(text, firmware):
    if (text.count('rom4_staged_profile:') != 1
            or re.findall(r'rom4_staged_profile: system=(\w+)', text) != ['nse1pipe']):
        raise ValueError('missing or wrong staged machine identity')
    rows = ROW.findall(text)
    if len(rows) != 1 or text.count('rom4_staged_frontier:') != 1:
        raise ValueError('missing, malformed or duplicate staged frontier observation')
    pc, xpc, illegal, pmst, swwsr, bscr, swcr, encoded, timestamp = rows[0]
    if (pc, xpc, illegal, pmst, swwsr, bscr, swcr) != ('408d', '00', '0', 'ffac', '2000', '0000', '0000') or not 0.5 <= float(timestamp) <= 1:
        raise ValueError('staged CPU snapshot changed; reassess the actual frontier')
    stop = 'staged block control/count modification is unsupported instruction=004749 word=fa30 block=00473c..004777'
    if (text.count(stop) != 1
            or text.count('unsupported') != 1 or 'unimplemented' in text):
        raise ValueError('missing or ambiguous staged host-packet instruction stop')
    ends = re.findall(r'rom4_staged_end: pc=([0-9a-f]{4}) xpc=([0-9a-f]{2}) illegal=([01]) reads=(\d+) writes=(\d+) gap_reads=(\d+) host_consumed=(\d+) host_headers=(\d+) t=([0-9.]+)', text)
    if (text.count('rom4_staged_end:') != 1 or len(ends) != 1
            or ends[0][:8] != ('474c', '00', '1', '16', '14', '15', '21', '7')
            or not 3 <= float(ends[0][8]) <= 3.05):
        raise ValueError('missing or changed bounded execution endpoint')
    end_time = float(ends[0][8])
    terminal = re.findall(r'rom4_staged_end_state: a=([0-9a-f]{10}) b=([0-9a-f]{10}) t=([0-9a-f]{4}) st0=([0-9a-f]{4}) st1=([0-9a-f]{4}) bk=([0-9a-f]{4}) ar0=([0-9a-f]{4}) ar1=([0-9a-f]{4}) ar2=([0-9a-f]{4}) ar3=([0-9a-f]{4})', text)
    if text.count('rom4_staged_end_state:') != 1 or terminal != [('0000001a08', '0000000000', '5555', '081f', 'e103', '0048', '0001', '06b8', '1a04', '1250')]:
        raise ValueError('missing or changed terminal architectural snapshot')
    headers = re.findall(r'rom4_staged_host_header: index=(\d+) value=([0-9a-f]{4}) mask=([0-9a-f]{4}) pc_context=([0-9a-f]{4}) t=([0-9.]+)', text)
    if (text.count('rom4_staged_host_header:') != 7
            or [row[:4] for row in headers] != [(str(i + 1), value, 'ffff', context) for i, (value, context) in enumerate([
                ('0a05', '3871'), ('0a05', '387f'), ('0a05', '3871'), ('0a05', '387f'), ('ff6e', '3b0c'), ('0001', '3c63'), ('354a', '3c67')])]
            or not 0.17 < float(headers[0][4]) < float(headers[1][4]) < float(timestamp) < float(headers[2][4])
            or not all(float(a[4]) < float(b[4]) < end_time for a, b in zip(headers, headers[1:]))):
        raise ValueError('missing or changed organic first host-packet header reads')
    consumed = re.findall(r'rom4_staged_host_consume: index=(\d+) previous=([0-9a-f]{4}) next=([0-9a-f]{4}) pc_context=([0-9a-f]{4}) t=([0-9.]+)', text)
    # The passive tap logs only the first eight nonzero consumer advances.
    if (text.count('rom4_staged_host_consume:') != 8
            or [row[:4] for row in consumed] != [(str(i + 1), previous, following, '3907') for i, (previous, following) in enumerate([
                ('0000', '0006'), ('0006', '000b'), ('000b', '000f'), ('000f', '0017'), ('0017', '0023'), ('0023', '0031'), ('0031', '0033'), ('0000', '0006')])]
            or not float(headers[1][4]) < float(consumed[0][4])
            or not float(consumed[6][4]) < float(timestamp) < float(headers[3][4]) < float(consumed[7][4])
            or not all(float(a[4]) < float(b[4]) < end_time for a, b in zip(consumed, consumed[1:]))):
        raise ValueError('missing or changed organic host-ring consumption')
    if (text.count('rom4_staged_dagen:') != 1
            or re.findall(r'rom4_staged_dagen: bk=([0-9a-f]{4}) ar0=([0-9a-f]{4}) ar2=([0-9a-f]{4}) st0=([0-9a-f]{4}) st1=([0-9a-f]{4})', text) != [('00fa', '0001', '06e4', '081f', '6103')]):
        raise ValueError('missing, duplicate or changed native status/address-generator observation')
    accumulators = re.findall(r'rom4_staged_accumulators: a=([0-9a-f]{10}) b=([0-9a-f]{10}) t=([0-9a-f]{4}) ar3=([0-9a-f]{4}) ar4=([0-9a-f]{4})', text)
    if (text.count('rom4_staged_accumulators:') != 1
            or accumulators != [('0000000000', '0000000000', '000b', '06e3', '0000')]):
        raise ValueError('missing, malformed or changed live accumulator snapshot')
    summaries = re.findall(r'^rom4_interface_summary: completion_strobes=(\d+) mailbox_writes=(\d+) ', text, re.M)
    if summaries != [('357', '74')] or text.count('rom4_interface_summary:') != 1:
        raise ValueError('missing or changed bootstrap completion boundary')
    if not re.search(r'^rom4_interface_summary: completion_strobes=357 mailbox_writes=74 slot_expiries=329 frame_expiries=573 rf_reads=10688 rf_port32_writes=12 rf_port38_reads=0 rf_port39_reads=0 pc=474c pmst=ffac ifr=0225 imr=035f mode_aa=0000 mode_ac=0001 transfer_expiries=32$', text, re.M):
        raise ValueError('changed bounded RF-loop transport or interrupt state')
    rf = re.findall(r'^rom4_rf_read: sample=(\d+) pc=([0-9a-f]{4}) frame=(\d+) t=([0-9.]+)$', text, re.M)
    if (text.count('rom4_rf_read:') != 32
            or [row[:3] for row in rf] != [(str(i + 1), ('324e', '3254', '325a', '3260')[i % 4], frame) for frame in ('30', '28') for i in range(16)]
            or not all(0.15 < float(row[3]) < float(timestamp) for row in rf[:16])
            or not all(float(timestamp) < float(row[3]) < end_time for row in rf[16:])
            or not all(float(before[3]) <= float(after[3]) for before, after in zip(rf, rf[1:]))
            or re.findall(r'^rom4_interface_summary: .* rf_reads=(\d+) ', text, re.M) != ['10688']):
        raise ValueError('missing, malformed or changed native RF provider reads')
    bsp = re.findall(r'rom4_staged_bsp_write: value=([0-9a-f]{4}) mask=([0-9a-f]{4}) t=([0-9.]+)', text)
    if text.count('rom4_staged_bsp_write:') != 18 or [(value, mask) for value, mask, _ in bsp] != ([('c008', 'ffff'), ('c0c8', 'ffff')] + [('c008', 'ffff')] * 4) * 2 + [('c008', 'ffff')] * 3 + [('c0c8', 'ffff'), ('c008', 'ffff'), ('c0c8', 'ffff')]:
        raise ValueError('missing, duplicate or changed native BSP reset/release writes')
    bsp_times = [float(row[2]) for row in bsp]
    if not 0 < bsp_times[0] < bsp_times[11] < float(timestamp) < bsp_times[12] < bsp_times[-1] < end_time or not all(before < after for before, after in zip(bsp_times, bsp_times[1:])):
        raise ValueError('native BSP publication order changed')
    tx = re.findall(r'rom4_staged_bsp_tx: index=(\d+) value=([0-9a-f]{4}) mask=([0-9a-f]{4}) t=([0-9.]+)', text)
    if (text.count('rom4_staged_bsp_tx:') != 4 or len(tx) != 4
            or [row[:3] for row in tx] != [(str(i + 1), ('0aaa', 'ffd5')[i % 2], 'ffff') for i in range(4)]
            or re.findall(r'rom4_staged_bsp_tx_count: count=(\d+)', text) != ['4']):
        raise ValueError('missing, duplicate or changed native BSP transmit publication')
    rx = re.findall(r'rom4_staged_bsp_rx: index=(\d+) value=([0-9a-f]{4}) mask=([0-9a-f]{4}) t=([0-9.]+)', text)
    if (text.count('rom4_staged_bsp_rx:') != 4 or len(rx) != 4
            or [row[:3] for row in rx] != [(str(i + 1), '0aaa', 'ffff') for i in range(4)]
            or re.findall(r'rom4_staged_bsp_rx_count: count=(\d+)', text) != ['4']):
        raise ValueError('missing, duplicate or changed native BSP receive observations')
    for cycle in range(2):
        i, j = cycle * 6, cycle * 2
        if not float(bsp[i + 1][2]) < float(tx[j][3]) < float(rx[j][3]) < float(rx[j + 1][3]) < float(tx[j + 1][3]) < float(bsp[i + 2][2]) < float(timestamp):
            raise ValueError('native BSP echo order changed')
    expected_low = {
        'low_write': [('00f1', '0000', 'ffff', '0f10'), ('00f2', '0000', 'ffff', '0f10'),
                      ('00f2', '0003', 'ffff', 'a5e5'), ('00f2', '0003', 'ffff', 'a5e5')] * 2 + [('00f2', '0003', 'ffff', 'a5e5'), ('00f2', '0000', 'ffff', 'a5b8'), ('00f1', 'b736', 'ffff', 'a5c4'), ('00f2', '0003', 'ffff', 'a5e5'), ('00f2', '0000', 'ffff', 'a5b8'), ('00f1', 'b71a', 'ffff', 'a5c4')],
        'low_read': [('00f1', '0000', 'ffff', 'a5ed'), ('00f2', '0003', 'ffff', 'a5ef')] * 5 + [('00f1', 'b736', 'ffff', 'a5ed'), ('00f2', '0000', 'ffff', 'a5ef'), ('00f1', 'b736', 'ffff', 'a5ed'), ('00f2', '0003', 'ffff', 'a5ef'), ('00f1', 'b71a', 'ffff', 'a5ed'), ('00f2', '0000', 'ffff', 'a5ef')],
        'gap_read': [('0003', '0000', 'ffff', pc) for pc in ('a5f4', 'a5f8', 'a601')] * 5,
    }
    low = {}
    for tag, expected in expected_low.items():
        prefix = f'rom4_staged_{tag}:'
        low[tag] = re.findall(prefix + r' address=([0-9a-f]{4}) value=([0-9a-f]{4}) mask=([0-9a-f]{4}) pc_context=([0-9a-f]{4}) t=([0-9.]+)', text)
        if text.count(prefix) != len(expected) or [row[:4] for row in low[tag]] != expected:
            raise ValueError('missing, malformed or changed low-page bus observation')
    ordered = []
    for cycle in range(2):
        w, r, g = cycle * 4, cycle * 4, cycle * 6
        ordered.extend([*low['low_write'][w:w + 3], *low['low_read'][r:r + 2], *low['gap_read'][g:g + 3],
                        low['low_write'][w + 3], *low['low_read'][r + 2:r + 4], *low['gap_read'][g + 3:g + 6]])
    ordered.extend([low['low_write'][8], *low['low_read'][8:10], *low['gap_read'][12:]])
    ordered.extend([*low['low_write'][9:11], *low['low_read'][10:12], low['low_write'][11], *low['low_read'][12:14], *low['low_write'][12:], *low['low_read'][14:]])
    times = [float(row[4]) for row in ordered]
    if (not all(0 < before < after < end_time for before, after in zip(times, times[1:]))
            or not times[27] < float(timestamp) < bsp_times[12] < times[28]
            or re.findall(r'rom4_staged_low_count: reads=(\d+) writes=(\d+)', text) != [('8', '8')]
            or re.findall(r'rom4_staged_gap_count: reads=(\d+)', text) != ['12']):
        raise ValueError('changed low-page observation ordering or counts')
    controls = re.findall(r'^rom4_rf_port32: sequence=(\d+) port31=([0-9a-f]{4}) port32=([0-9a-f]{4}) pc=([0-9a-f]{4}) t=([0-9.]+)$', text, re.M)
    if (text.count('rom4_rf_port32:') != 12
            or [row[:4] for row in controls] != [(str(i + 1), p31, p32, pc) for i, (p31, p32, pc) in enumerate([
                ('2a04', '0006', 'a23f'), ('0041', '0040', '3711'), ('27a2', '0030', '3711'), ('0041', '0020', '3711'), ('0488', '0010', '3711'), ('0000', '0050', '3711'), ('2a04', '0006', 'a23f'), ('0041', '0040', '3711'), ('27a2', '0030', '3711'), ('0041', '0020', '3711'), ('0488', '0010', '3711'), ('2813', '0030', '4026')])]
            or not times[33] < float(controls[0][4]) < float(controls[-1][4]) < times[-1]):
        raise ValueError('missing or changed native RF control writes')
    transfers = re.findall(r'^rom4_transfer_timer: expiry=(\d+) target=([0-9a-f]{4}) counter=([0-9a-f]{4}) delay=(\d+) t=([0-9.]+)$', text, re.M)
    expected_transfers = [(target, target, delay) for target, delay in [('0884', '2160'), ('08d4', '77'), ('0924', '77'), ('0974', '77'), ('0299', '3242'), ('0884', '2158'), ('08d4', '77'), ('0924', '77'), ('0974', '77')]] + [('0032', '0032', '36')] + [('0032', '0031', '35')] * 21 + [('03e8', '03e8', '984')]
    if (text.count('rom4_transfer_timer:') != 32
            or [row[:4] for row in transfers] != [(str(i + 1), *row) for i, row in enumerate(expected_transfers)]
            or not all(float(controls[i][4]) < float(row[4]) < float(controls[i + 1][4]) < end_time for i, row in enumerate(transfers[:5]))
            or not all(float(controls[i + 6][4]) < float(row[4]) < float(controls[i + 7][4]) for i, row in enumerate(transfers[5:9]))
            or not float(controls[10][4]) < float(transfers[9][4])
            or not float(controls[-1][4]) < float(transfers[-1][4]) < bsp_times[-2]
            or not all(float(a[4]) < float(b[4]) < end_time for a, b in zip(transfers, transfers[1:]))):
        raise ValueError('missing or changed native transfer-timer/control ordering')
    stop_offset = text.index(stop)
    before_stop = sum(match.start() < stop_offset for match in re.finditer(r'^rom4_transfer_timer:', text, re.M))
    if before_stop != 32:
        raise ValueError('timer expiries before CPU stop changed; do not count stopped-device events as firmware progress')
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
    return dict(status='unsupported_staged_delayed_tc_branch_in_block', source_offset=SOURCE_OFFSET,
                tail_source_offset=TAIL_SOURCE_OFFSET, observed_tail_words=24,
                repeat_source_offset=REPEAT_SOURCE_OFFSET, observed_repeat_words=24,
                observed_words=40, firmware_sha256=hashlib.sha256(firmware).hexdigest(),
                instruction_address=0x4749, instruction=0xfa30, block_start=0x473c, block_end=0x4777,
                host_header_reads=7, host_consumed=21,
                low_gap_bus_reads=15, rf_sample_reads=10688, rf_sample_blocks=334, rf_sample_remainder=0,
                rf_control_writes=12, transfer_expiries=32, transfer_expiries_before_stop=32, transfer_expiries_after_stop=0,
                bsp_transmit_words=[0x0aaa, 0xffd5] * 2,
                bsp_receive_words=[0x0aaa] * 4,
                bsp_control_writes=([0xc008, 0xc0c8] + [0xc008] * 4) * 2 + [0xc008] * 3 + [0xc0c8, 0xc008, 0xc0c8], completion_strobes=357,
                checkpoint_illegal=False, uninterrupted_session_claim=False,
                swwsr=0x2000, boot_claim=False,
                preceding_instruction_retirement_claim=False,
                upload_complete_claim=False, fitted_bus_dialect_claim=False,
                physical_low_gap_identity_claim=False, rf_acquisition_claim=False, parallel_store_carry_claim=False)


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

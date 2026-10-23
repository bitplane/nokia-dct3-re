"""Diagnose the observed NSE-2 reason-68 restart; this is not a stable-boot gate."""
import argparse
import json
from pathlib import Path
import re


def diagnose(text):
    reasons = re.findall(r'nse2_reset_reason: value=68 pc=0028db6e lr=00290361 t=([0-9.]+) observer_writes=0', text)
    resets = re.findall(r'nse2_reset_entry: reason=00000068 lr=0023616d stack=([^ ]+) t=([0-9.]+) observer_writes=0', text)
    identities = re.findall(r'nse2_native_identity: n=(\d+) data=0004 mask=ffff pc=0f0f t=([0-9.]+) observer_writes=0', text)
    if len(reasons) != 1 or len(resets) != 1 or len(identities) != 2:
        raise ValueError('NSE-2 own reason-68 reset and two native startups missing')
    reason_time, reset_time = float(reasons[0]), float(resets[0][1])
    if not 30.9 < reason_time < 31.1 or not reason_time < reset_time < reason_time + 1.1:
        raise ValueError('NSE-2 observed restart timing differs')
    if not float(identities[0][1]) < reason_time < reset_time < float(identities[1][1]):
        raise ValueError('NSE-2 restart did not separate the two native startups')
    if '000000d5' not in resets[0][0].split(','):
        raise ValueError('NSE-2 delayed reset event d5 missing from captured stack')
    return dict(stable_long_boot=False, reset_reason='68', reason_time=reason_time,
                reset_time=reset_time, policy_caller='290361', native_startups=2,
                scope='observed failure diagnostic; no successful long-run claim')


def diagnose_transport(text):
    rows = re.findall(r'nse2_native_transport: t=([0-9.]+) pc=([0-9a-f]{4}) imr=([0-9a-f]{4}) '
                      r'ifr=([0-9a-f]{4}) words=([^ ]+) mcu_packets=([0-9a-f]{4}) observer_writes=0', text)
    snapshots = {}
    for time, pc, imr, ifr, words, packets in rows:
        values = {}
        for item in words.split(','):
            address, value, count = item.split(':')
            values[int(address, 16)] = (int(value, 16), int(count))
        snapshots[round(float(time))] = (pc, values, int(packets, 16))
    if not all(second in snapshots for second in (1, 7, 15, 30)):
        raise ValueError('NSE-2 complete first native receive window missing')
    _, initial, count = snapshots[1]
    if count != 4 or initial.get(0x8e4) != (0xa9, 4) or initial.get(0x8e5, (None,))[0] != 0xa9:
        raise ValueError('NSE-2 initial four native publications/MCU consumption differ')
    for second in (7, 15, 30):
        pc, values, count = snapshots[second]
        if (pc != '09ac' or count != 0 or values.get(0x8e4) != (0xa9, 4)
                or values.get(0x8e5, (None,))[0] != 0xa9
                or values.get(0x852, (None,))[0] != values.get(0x853, (None,))[0]):
            raise ValueError('NSE-2 drained/stationary native ring frontier differs')
    clear = re.findall(r'nse2_packet_count: value=0000 mask=ffff pc=0020feae lr=00254e67 t=([0-9.]+) observer_writes=0', text)
    if not clear or not 4 < float(clear[0]) < 5:
        raise ValueError('NSE-2 initial MCU packet-count clear missing')
    return dict(initial_native_rx_packets=4, initial_rx_consumed=True,
                packet_count_clear_time=float(clear[0]), tx_ring_drained=True,
                native_rx_publications_unchanged_through=30,
                native_idle_pc='09ac', broken_initial_fiq_delivery=False)


def diagnose_schedule(text):
    rows = re.findall(r'nse2_native_work_writer: pc=([0-9a-f]{4}) writes=(\d+) bit0_set=(\d+) '
                      r'bits=([0-9a-f]{4}) t=([0-9.]+) observer_writes=0', text)
    writers = {}
    for pc, writes, sets, bits, time in rows:
        if pc in writers or not 29.9 <= float(time) <= 30.1:
            raise ValueError('NSE-2 bounded work-writer summary differs')
        writers[pc] = (int(writes), int(sets), int(bits, 16))
    setter, clearer = writers.get('32c1'), writers.get('446d')
    if (setter is None or clearer is None or setter[0] < 1000 or setter[1] != setter[0]
            or not setter[2] & 1 or clearer[0] < setter[0] or clearer[1] != 0):
        raise ValueError('NSE-2 active native bit-zero scheduling evidence missing')
    requests = re.findall(r'nse2_native_host_packet: previous=001e next=0041 header=441a words=([^ ]+) '
                          r't=([0-9.]+) observer_writes=0', text)
    if len(requests) != 1:
        raise ValueError('NSE-2 own final type-1a request consumption missing')
    words, time = requests[0]
    if words.split(',') != ['0011', '9800'] + ['0000'] * 32 or not 4 < float(time) < 5:
        raise ValueError('NSE-2 own type-1a body/horizon differs')
    return dict(native_scheduling_active=True, bit0_set_writes=setter[1], bit0_clear_writes=clearer[0],
                own_type1a_request_consumed=True, missing_work_bit_falsified=True,
                stable_long_boot=False, scope='active scheduling does not prove continued native RX')


def diagnose_processing(text):
    def counts(prefix):
        result = {}
        for address, count in re.findall(prefix + r': address=([0-9a-f]{4}) count=(\d+)\b', text):
            if address in result:
                raise ValueError('NSE-2 duplicate processing count')
            result[address] = int(count)
        return result
    dispatch = counts('rom4_dispatch_count')
    comparison = counts('rom4_comparison_count')
    if dispatch.get('32f4') != 2 or any(dispatch.get(site, 0) < 1000 for site in ('09a4', '4414', '33f3')):
        raise ValueError('NSE-2 two-frame then sustained control-service frontier differs')
    if any(comparison.get(site) != count for site, count in
           (('330a', 16), ('332b', 2), ('3347', 0), ('33ac', 1))):
        raise ValueError('NSE-2 measured filter/reduction path differs')
    samples = re.findall(r'rom4_comparison_input: reads=(\d+) nonzero=(\d+) reduction_nonzero=(\d+)\b', text)
    if len(samples) != 1 or int(samples[0][0]) < 100000 or samples[0][1:] != ('0', '0'):
        raise ValueError('NSE-2 unattached zero-input/output census differs')
    modes = re.findall(r'rom4_mode_write: t=([0-9.]+) pc=([0-9a-f]{6}) address=00ac data=([0-9a-f]{4}) mask=ffff', text)
    if [row[2] for row in modes[:5]] != ['0000', '0001', '0002', '000c', '0004']:
        raise ValueError('NSE-2 native mode lifecycle differs')
    if not all(4 < float(row[0]) < 5 for row in modes[1:5]):
        raise ValueError('NSE-2 native mode transition horizon differs')
    if any(row[2] != ('000c' if index % 2 == 0 else '0004') for index, row in enumerate(modes[5:])):
        raise ValueError('NSE-2 repeated control mode lifecycle differs')
    return dict(mode1_filter_frames=2, observed_sample_reads=int(samples[0][0]),
                all_observed_samples_zero=True, native_control_service_active=True,
                stable_long_boot=False, acquisition_proved=False,
                scope='measured processing frontier, not proof that nonzero samples suffice')


def diagnose_scan_progress(text):
    rows = re.findall(r'nse2_native_scan_loop: address=(09c4|09cb) pc=([0-9a-f]{4}) ar5=ffff '
                      r'ar6=([0-9a-f]{4}) repeat=([0-9a-f]{4}) t=([0-9.]+) observer_writes=0', text)
    expected = []
    for outer in range(7, 1, -1):
        expected.extend((('09c4', outer, 1), ('09c4', outer, 0), ('09cb', outer, 0xffff)))
    expected.append(('09c4', 1, 1))
    actual = [(address, int(outer, 16), int(repeat, 16)) for address, _, outer, repeat, _ in rows]
    if actual != expected or len(rows) != text.count('nse2_native_scan_loop:'):
        raise ValueError('NSE-2 advancing outer measurement-loop sequence differs')
    times = [float(row[4]) for row in rows]
    if times != sorted(times) or not 6 < times[0] < 7 or not 29 < times[-1] < 30:
        raise ValueError('NSE-2 measured loop progression horizon differs')
    return dict(native_measurement_progressing=True, observed_outer_branch_values=list(range(7, 1, -1)),
                last_observed_outer_counter=1, scan_completed=False,
                scope='advancing nested loop before deadline; no completed RF measurement claim')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('trace', type=Path)
    parser.add_argument('--transport', action='store_true', help='also require passive first-window ring evidence')
    parser.add_argument('--schedule', action='store_true', help='diagnose thirty-second scheduling without requiring a reset capture')
    parser.add_argument('--processing', action='store_true', help='also require reused passive ROM4 processing evidence')
    parser.add_argument('--scan-progress', action='store_true', help='also require advancing measurement-loop evidence')
    args = parser.parse_args()
    try:
        text = args.trace.read_text()
        result = diagnose_schedule(text) if args.schedule else diagnose(text)
        if args.transport:
            result.update(diagnose_transport(text))
        if args.processing:
            result['processing'] = diagnose_processing(text)
        if args.scan_progress:
            result['measurement'] = diagnose_scan_progress(text)
        print(json.dumps(result, indent=2))
    except (OSError, ValueError) as error:
        parser.exit(1, str(error) + '\n')


if __name__ == '__main__':
    main()

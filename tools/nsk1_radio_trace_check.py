"""Check observed NSK-1 native radio boundary, not registration or RF validity."""
import argparse
import json
from pathlib import Path
import re

from tools.c54x_rom4_rf_delivery_check import pack_tuning_nonnegative, nsk1_dcs1800_tuning_consistency


def check_scan_progress(text):
    rows = re.findall(r'nsk1_scan_progress: t=([0-9.]+) counter_hi=([0-9a-f]{4}) counter_lo=([0-9a-f]{4}) qualified=([0-9a-f]{4}) flags=([0-9a-f]{4}) producer=([0-9a-f]{4}) consumer=([0-9a-f]{4}) observer_writes=0', text)
    if len(rows) != 5 or [float(row[0]) for row in rows] != [40,45,50,55,60]:
        raise ValueError('missing bounded scan progress samples')
    counters = [int(hi + lo,16) for _,hi,lo,*_ in rows]
    if any(right <= left for left,right in zip(counters,counters[1:])):
        raise ValueError('scan counter is not progressing')
    if any(qualified != '0000' or flags != '0000' or producer != consumer
           for _,_,_,qualified,flags,producer,consumer in rows):
        raise ValueError('scan qualification or sampled queue boundary changed')
    return {'samples':len(rows),'counter_advance':counters[-1]-counters[0],
            'qualified_results_observed':False}


def check_scan_qualification(text):
    metrics = re.findall(r'nsk1_scan_metric: index=(\d+) pc=([0-9a-f]{4}) a=([0-9a-f]{10}) threshold=([0-9a-f]{4}) t=([0-9.]+) observer_writes=0',text)
    if len(metrics) != 8 or [int(row[0]) for row in metrics] != list(range(1,9)):
        raise ValueError('missing bounded scan measurement observations')
    if any((pc,a,threshold) != ('0ae7','0000000000','0574') or not 39 <= float(time) < 40
           for _,pc,a,threshold,time in metrics):
        raise ValueError('scan measurement CPU scope, value or threshold changed')
    times = [float(row[4]) for row in metrics]
    if times != sorted(set(times)):
        raise ValueError('scan measurement times are not ordered')
    rows = re.findall(r'nsk1_scan_late: t=([0-9.]+) counter_hi=([0-9a-f]{4}) counter_lo=([0-9a-f]{4}) qualified=([0-9a-f]{4}) flags=([0-9a-f]{4}) observer_writes=0',text)
    if len(rows) != 6 or [float(row[0]) for row in rows] != [65,70,75,80,85,90]:
        raise ValueError('missing late scan observation horizon')
    counters = [int(hi+lo,16) for _,hi,lo,_,_ in rows]
    if any(hi != 'ffff' or qualified != '0000' or flags != '0000' for _,hi,_,qualified,flags in rows):
        raise ValueError('late scan qualification or negative-counter boundary changed')
    if any(right <= left for left,right in zip(counters,counters[1:])):
        raise ValueError('late scan counter is not progressing')
    result = {'measurement':0,'threshold':0x574,'observed_through':90,
              'timeout_counter_still_negative':True,'qualified_results_observed':False}
    if 'nsk1_publisher_entry:' in text:
        result['serial_publishers'] = check_serial_publishers(text)
    return result


def check_serial_publishers(text):
    rows = re.findall(r'nsk1_publisher_entry: address=([0-9a-f]{4}) index=(\d+) ar4=([0-9a-f]{4}) ar5=([0-9a-f]{4}) serial06b6=([0-9a-f]{4}) t=([0-9.]+) observer_writes=0',text)
    expected = [('3f50',str(i),'0000','0004','0000') for i in range(1,5)]
    # AR5 is sampled before the publisher CALLD's length-setting delay slot.
    expected.append(('4810','1','008b','03ff','0000'))
    if [row[:5] for row in rows] != expected:
        raise ValueError('serial publisher boundary or positive RSSI control changed')
    times = [float(row[5]) for row in rows]
    if not all(2 <= time < 3 for time in times[:4]) or not 38 < times[4] < 40:
        raise ValueError('serial publisher observation timing changed')
    return {'empty_queue_dispatch_samples':4,'rssi_positive_control':True,
            'serial_publishers_observed':False}


def check_serial_queue(text):
    check_serial_publishers(text)
    queue = re.findall(r'rom4_serial_queue_summary: cpu_bus_writes=(\d+) nonzero=(\d+) observer_writes=0',text)
    if queue != [('2','0')]:
        raise ValueError('serial queue CPU-bus write boundary changed')
    rows = re.findall(r'rom4_serial_summary: port=(35|36) direction=(read|write) count=(\d+) last_value=([0-9a-f]{4}) last_pc=([0-9a-f]{4})',text)
    expected = [('35','read','0','0000','0000'),('35','write','0','0000','0000'),
                ('36','read','4','0080','9183'),('36','write','2','0080','90bc')]
    if rows != expected:
        raise ValueError('serial data/status port observation changed or truncated')
    return {'data_port':'0x35','status_port':'0x36','data_reads':0,
            'cpu_bus_queue_writes':2,'nonzero_queue_writes':0,
            'physical_serial_identity_proved':False}


def check_tuning_provenance(text):
    reads = re.findall(r'nsk1_tuning_source: address=([0-9a-f]{8}) value=([0-9a-f]{8}) mask=([0-9a-f]{8}) pc=00296104 t=([0-9.]+) observer_writes=0',text)
    if [row[:3] for row in reads] != [('00298228','000417bf','ffffffff'),('0029822c','17971d00','ffffffff')]:
        raise ValueError('own tuning payload source reads changed or missing')
    uploads = re.findall(r'nsk1_tuning_upload: space=program address=([0-9a-f]{4}) value=([0-9a-f]{4}) pc=31d7 t=([0-9.]+) observer_writes=0',text)
    if [row[:2] for row in uploads] != [('216a','0004'),('216b','17bf'),('216c','1797')]:
        raise ValueError('native tuning overlay upload changed or missing')
    pairs = re.findall(r'nsk1_tuning_pair: port=(31|32) value=([0-9a-f]{4}) pc=(4023|4028) channel=([0-9a-f]{4}) flags=([0-9a-f]{4}) addend=([0-9a-f]{4}) a=([0-9a-f]{10}) t=([0-9.]+) observer_writes=0',text)
    packed = pack_tuning_nonnegative(0x374+0x17bf)
    expected = [('31',f'{packed&0xffff:04x}','4023','0374','0020','17bf',f'{packed:010x}'),
                ('32',f'{packed>>16:04x}','4028','0374','0020','17bf',f'{packed:010x}')]
    if [row[:7] for row in pairs] != expected:
        raise ValueError('selected-entry tuning arithmetic or port pair changed')
    times = [float(row[-1]) for row in reads+uploads+pairs]
    if times != sorted(times) or not times[-1] < 59:
        raise ValueError('own tuning source/upload/port ordering changed')
    return {'channel_word':'0x0374','receive_addend':'0x17bf','transmit_addend':'0x1797',
            'packed_word':hex(packed),'own_firmware_source_proved':True,
            'frequency_consistency':nsk1_dcs1800_tuning_consistency(packed,'rx'),
            'physical_synthesizer_semantics_proved':False}


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
    returns = re.findall(r'nsk1_rssi_return: pc=([0-9a-f]{4}) st0=([0-9a-f]{4}) st1=[0-9a-f]{4} control06be=([0-9a-f]{4}) control06e1=([0-9a-f]{4}) control06e3=([0-9a-f]{4}) list11f9=([0-9a-f]{4}) list11fa=([0-9a-f]{4})', text)
    if 'nsk1_rssi_return:' in text:
        if [row[0] for row in returns] != ['0a2e','0a2f','30ed','30f0','30f2']:
            raise ValueError('post-RSSI return path changed or truncated')
        if any(int(row[1],16) & 0x1000 or tuple(row[2:]) != ('0020','0000','0000','03ff','03ff') for row in returns):
            raise ValueError('post-RSSI TC/control/list state changed')
    work = re.findall(r'nsk1_rssi_work: pc=([0-9a-f]{4}) ar3=([0-9a-f]{4}) pointer1983=([0-9a-f]{4}) word03ff=([0-9a-f]{4}) control06be=([0-9a-f]{4}) control06e3=([0-9a-f]{4})', text)
    if 'nsk1_rssi_work:' in text:
        if [row[0] for row in work] != ['3103','311a','312c','3130','0a71']:
            raise ValueError('post-RSSI channel-work branch changed or truncated')
        if [row[1] for row in work] != ['03ff','03ff','0008','0008','0008']:
            raise ValueError('post-RSSI work register progression changed')
        if any(tuple(row[2:]) != ('03ff','0374','0020','0000') for row in work):
            raise ValueError('post-RSSI work cursor/control state changed')
        gateway = 'nsk1_rssi_gateway: word2420=f073 word2421=0a71 word0a5c=891a word0a71=76f8'
        if gateway not in text:
            raise ValueError('live uploaded scan gateway differs or is unobserved')
    progress = check_scan_progress(text) if 'nsk1_scan_progress:' in text else None
    return {'tx': len(tx), 'rx': len(rx), 'pending_indexes': [1, 2, 8, 3],
            'live_rssi_stack': True, 'post_rssi_return_observed':bool(returns),
            'post_rssi_channel_work_observed':bool(work),
            'scan_progress':progress,
            'registration_proved': False, 'rf_validity_proved': False}


def diagnose_liveness_reset(text):
    counts = re.findall(r'nsk1_dsp_liveness_check: count=([0-9a-f]{4})', text)
    if counts != ['0001', '0001', '0000']:
        raise ValueError('expected two active packet windows followed by an empty window')
    position = 0
    for marker in ('nsk1_dsp_liveness_check: count=0000',
                   'nsk1_dsp_liveness_fault: kind=1 caller=00294191',
                   'nsk1_supervisor_reason_set: reason=68 caller=0028c351',
                   'nsk1_supervisor_reset_path: event=d5 reason=68',
                   'nsk1_radio_reset_request: reason=00000068 caller=0022f117'):
        position = text.find(marker, position)
        if position < 0:
            raise ValueError('missing ordered native liveness-reset predecessor: ' + marker)
        position += len(marker)
    if 'nsk1_supervisor_reset_path: event=d3' in text:
        raise ValueError('independent idle-supervision reset also observed')
    before_reset = text.split('nsk1_radio_reset_request:', 1)[0]
    idle_counts = re.findall(r'nsk1_supervisor_write: address=0010fdea value=([0-9a-f]{2})', before_reset)
    if not idle_counts or max(int(value, 16) for value in idle_counts) >= 15:
        raise ValueError('idle counter exclusion is missing or contradicted')
    return {'observed_failure': 'native DSP packet-liveness reset', 'reset_reason': '0x68',
            'packet_windows': [1,1,0], 'idle_counter_threshold_cause': False,
            'long_run_stability_proved': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('--diagnose-liveness-reset', action='store_true',
                        help='classify the known late failure; does not qualify stability')
    parser.add_argument('--scan-qualification', action='store_true',
                        help='also require the passive 92-second scan qualification fixture')
    parser.add_argument('--serial-queue', action='store_true',
                        help='also require the passive serial-port/queue observation fixture')
    parser.add_argument('--tuning-provenance', action='store_true',
                        help='also require own tuning source, native upload and selected-entry port pair')
    args = parser.parse_args()
    try:
        checker = diagnose_liveness_reset if args.diagnose_liveness_reset else check
        text = args.log.read_text()
        result = checker(text)
        if args.scan_qualification:
            result['scan_qualification'] = check_scan_qualification(text)
        if args.serial_queue:
            result['serial_queue'] = check_serial_queue(text)
        if args.tuning_provenance:
            result['tuning_provenance'] = check_tuning_provenance(text)
        print(json.dumps(result, indent=2))
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSK-1 radio boundary: FAIL: {error}\n')


if __name__ == '__main__':
    main()

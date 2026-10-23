"""Qualify NSE-2's live ROM4 result paths; no runtime completion is implied."""
import argparse
import hashlib
import json
import re
from pathlib import Path
import struct
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.c54x_rom4_port_census import direct_callers
from tools.nsk1_radio_contract import (live_publisher_census, native_completion_contract,
                                     serial_publisher_contract, publisher_type_inventory,
                                     PUBLISHER_TYPE_LOADS)

FRAGMENTS = {
    0x3d70: '7714195df13000f06884ff0f1b84818461840040f495e97cff30f120012b61840020771306bcff30f120017661840080f495f495ff30f12001f281f8028461f8000800016883fff7ff2069830008',
    0x3d9b: '10daf03000ff771412008094ec1ee5caf8303db1e7267712121f77130286f07448f781f8028476837fffe762',
    0x092d: '60f802840001f8300939e87b881a80f80285',
    0x0952: '7712006076820546f4957582000e',
    0x0975: '60f802840001f8300a1a76f80e130001f273098a7716000776f80e130001771a007bf0720989f074407c7215028577140e1577170e14',
    0x09c2: '6c8d099077120e131082fa44098a6b82ffff6c8e0981',
    0x3c04: '61f806bc0100fc3048117714195df03000f06884ff0f1a84808461f800110040e87c69f806e30020',
    0x6790: '771206bc771006e31180f2300020f84567a9f330ffdf818044f806ad770e0005f58c81f806ab69820100',
    0x0a15: 'f0200e13f07447f6f4bcfc20',
    0x47ff: '7714008b',
    0x4810: 'f27437ce771500a2',
    0x3ca4: '61840060f0200e13ff30f0200f0bf07447f6fc00',
    0x6d19: '61f806bc0100f8206d80',
    0x6d6f: '10f806abf8446d8068f806bcfeff61f8195d0060f0200e13ff30f0200f0bf07447f6',
    0x397a: 'f1100051f84f398d',
    0x398d: 'f000b00f4a1188117710000110818a11f4e2771306bd',
}


def inspect_scan_request_selection(text):
    rows=re.findall(r'nse2_scan_selector: address=([0-9a-f]{4}) ar2=([0-9a-f]{4}) input=([0-9a-f]{4}) extent=([0-9a-f]{4}) flags=([0-9a-f]{4}) mode=([0-9a-f]{4}) st0=([0-9a-f]{4}) t=([0-9.]+) observer_writes=0',text)
    if [row[0] for row in rows]!=['3d5e','3d8f','3da3','3db1','092d']:
        raise ValueError('scan request selector path differs or list-conversion path ran')
    if rows[0][1:4]!=('081f','0011','0000') or rows[1][1:4]!=('0820','9800','007c'):
        raise ValueError('scan extent does not follow the captured host request')
    if any(row[3]!='007c' for row in rows[1:]) or rows[1][5]!='0010' or not int(rows[2][6],16)&0x1000:
        raise ValueError('scan default extent or supplied-list branch condition differs')
    times=[float(row[-1]) for row in rows]
    if times!=sorted(times) or not 4.6<times[0]<times[-1]<4.9:
        raise ValueError('scan request timing/order differs')
    writes=re.findall(r'nse2_scan_extent: address=(028[45]) value=([0-9a-f]{4}) mask=ffff pc=([0-9a-f]{4}) n=(\d+) t=([0-9.]+) observer_writes=0',text)
    if [(a,v,pc,int(n)) for a,v,pc,n,_ in writes]!=[
            ('0284','0000','0f10',1),('0285','0000','0f10',1),
            ('0284','007c','3d8f',2),('0285','007b','0936',2)]:
        raise ValueError('scan selector/inner-count write attribution differs')
    return dict(host_control='0011',following_word='9800',selected_extent=124,inner_banz_initial=123,
                supplied_list_conversion=False,observed_request_decode_consistent=True,
                fitted_mask_proved=False,physical_scan_duration_proved=False)


def inspect_scan_bitmap_source(image,text):
    fragments={0x20fd44:'980d287cdb02980c2806d125',
               0x20fd9c:'210146587940430146587141',
               0x28531a:'b5101c04200ff7cfffb61c20490d2230f015fced',
               0x28535c:'0010a75c'}
    for address,encoded in fragments.items():
        expected=bytes.fromhex(encoded)
        if image[address-0x200000:address-0x200000+len(expected)]!=expected:
            raise ValueError(f'MCU scan bitmap contract differs at {address:06x}')
    copies=re.findall(r'nse2_scan_bitmap_copy: source=0010a75c length=48 bitmap=([0-9a-f,]+) t=([0-9.]+) observer_writes=0',text)
    choices=re.findall(r'nse2_scan_bitmap: address=0020fd9c category=([0-9a-f]{8}) count=([0-9a-f]{8}) detail=([0-9a-f]{8}) bitmap=([0-9a-f,]+) t=([0-9.]+) observer_writes=0',text)
    expected=['003f']+['ffff']*23
    if len(copies)!=2 or len(choices)!=1 or any(bits.split(',')!=expected for bits,_ in copies):
        raise ValueError('complete MCU bitmap-source observations differ')
    category,count,_,bitmap,time=choices[0]
    population=sum(int(word,16).bit_count() for word in bitmap.split(','))
    if bitmap.split(',')!=expected or int(count,16)!=population or population!=374 or int(category,16)!=2:
        raise ValueError('MCU full-scan branch does not follow its bitmap population')
    times=[float(t) for _,t in copies]+[float(time)]
    if times!=sorted(times) or not 4.6<times[0]<times[-1]<4.9:
        raise ValueError('MCU bitmap-copy/branch ordering differs')
    return dict(bitmap_source='10a75c',bytes=48,set_bits=population,category=2,
                full_scan_threshold=124,full_scan_branch_observed=True,
                authentic_factory_bitmap_proved=False)


def inspect_scan_bitmap_writers(image,text):
    fragments={0x224a4c:'b5304ce87d202800d1104de51c2821ff2230f076fdf3203f8028207c84a01c28f060fb808ca0f060fb72bd30',
               0x224dec:'0010ac2c0010abbc',
               0x285170:'b5101c04200ff7d0f88b48771c21f7e9ff2d48761c212230f015fdbe200ff7d0f8e3bd10'}
    for address,encoded in fragments.items():
        expected=bytes.fromhex(encoded)
        if image[address-0x200000:address-0x200000+len(expected)]!=expected:
            raise ValueError(f'bitmap default/setter contract differs at {address:06x}')
    rows=re.findall(r'nse2_bitmap_write: address=([0-9a-f]{8}) value=([0-9a-f]{8}) mask=ffffffff pc=([0-9a-f]{8}) lr=([0-9a-f]{8}) n=(\d+) t=([0-9.]+) observer_writes=0',text)
    summaries=re.findall(r'nse2_bitmap_write_summary: writes=(\d+) setters=(\d+) t=([0-9.]+) observer_writes=0',text)
    if len(summaries)!=1 or summaries[0][:2]!=('48','3') or len(rows)!=48 or not 29.9<=float(summaries[0][2])<=30.1:
        raise ValueError('complete bitmap writer coverage through thirty seconds missing')
    for i,(address,value,pc,lr,n,_) in enumerate(rows):
        expected=0 if i<12 else 0x003fffff if i%12==0 else 0xffffffff
        if int(address,16)!=0x10a75c+(i%12)*4 or int(value,16)!=expected or int(n)!=i+1:
            raise ValueError('bitmap writer address/value ordering differs')
        if i<12:
            valid=pc in ('0020010c','0020010e') and lr=='0022032b'
        else:
            valid=pc=='0029ad2a' and lr=='0028518d'
        if not valid:
            raise ValueError('bitmap writer provenance differs')
    setters=re.findall(r'nse2_bitmap_setter: lr=([0-9a-f]{8}) source=([0-9a-f]{8}) bitmap=([0-9a-f,]+) t=([0-9.]+) observer_writes=0',text)
    if len(setters)!=3 or any(lr!='00224a71' or source!='0010ac2c' or bitmap.split(',')!=['003f']+['ffff']*23 for lr,source,bitmap,_ in setters):
        raise ValueError('observed bitmap setters do not originate in the explicit default branch')
    return dict(default_constructor='224a4c',alternative_list_flag='10abd0',source='10ac2c',
                live_bitmap='10a75c',writes=48,default_setter_calls=3,
                firmware_constructed_default=True,erased_ram_explanation=False,
                authentic_factory_nv_required_for_this_bitmap=False)


def inspect_receive_activity(image,text,*,expiry=False):
    for address,encoded in {0x29909e:'88603001806078e878612900d156',
            0x2991b2:'2dddd000e77288602800d100e76a201d4903f7bbfdb98067e7e3',
            0x2991cc:'0010c764000009cd'}.items():
        expected=bytes.fromhex(encoded)
        if image[address-0x200000:address-0x200000+len(expected)]!=expected:
            raise ValueError('receive activity-counter consumer code differs')
    rows=re.findall(r'nse2_receive_activity: kind=(packet|expiry) n=(\d+) base=0010c764 count=([0-9a-f]{4}) message=([0-9a-f]{8}) type=([0-9a-f]{2}) t=([0-9.]+) observer_writes=0',text)
    initial=[row for row in rows if row[0]=='packet' and float(row[-1])<30]
    if len(initial)!=4 or [(int(n),int(count,16),kind) for _,n,count,_,kind,_ in initial]!=[(i,i-1,'74') for i in range(1,5)]:
        raise ValueError('initial pre-classification receive activity differs')
    deadlines=[row for row in rows if row[0]=='expiry']
    if expiry:
        if len(deadlines)!=1 or deadlines[0][1:5]!=('1','0000','000000dd','00') or not 30.9<float(deadlines[0][-1])<31.1:
            raise ValueError('empty activity count at receive expiry missing')
    elif deadlines:
        raise ValueError('unexpected receive expiry in thirty-second acceptance')
    return dict(counter='10c766',increment_before_packet_classification=True,
                initial_counted_types=['74']*4,expiry_requires_any_activity=True,
                particular_scan_result_required=False,empty_expiry_observed=expiry,
                missing_physical_source_identified=False,stable_long_boot=False)
MCU_REQUEST_FRAGMENTS = {
    0x2160ec: '27017507',  # R7 supplies the one-byte length.
    0x2160f4: '48a4468078002804d145',
    0x21615a: '2008f040fa281c0120038008708f200770c8201071082003f03ffb0f',
    0x277fb8: '781c430c04200c011c5861d01c28f7ffffa8',
}
NV_POLICY_FRAGMENTS = {
    0x290e08: '466889001c21466a8952f002ff3c',
    0x290490: 'b5102401487619004a7600615a512200f000fc9f1c201e4106090e0c2800d1f1',
    0x235656: '08a8d30df05aff19',
    0x2902e4: '20002d01d1002001466978094201d0012468e000240020002d02d10020014669780908494208d1022c00d101e0292469',
}


def inspect_program(program, data):
    if len(program) != 0x20000 or len(data) != 0x20000:
        raise ValueError('native result audit requires full page-zero program and companion data images')
    for address, encoded in FRAGMENTS.items():
        expected = bytes.fromhex(encoded)
        if program[address * 2:address * 2 + len(expected)] != expected:
            raise ValueError(f'NSE-2 live result code differs at {address:04x}')
    callers = direct_callers(program, 0x47f6)
    if callers != [(0xa17, False), (0x3cab, False), (0x6d7e, False)]:
        raise ValueError('NSE-2 page-zero direct type-8b caller candidates differ')
    entries = [index for index in range(0x52)
               if struct.unpack_from('>H', data, (0xb00f + index) * 2)[0] == 0x3c92]
    if entries != [0x4b]:
        raise ValueError('ROM4 host table does not uniquely select 3c92 for type 4b')
    request_entries = [index for index in range(0x52)
                       if struct.unpack_from('>H', data, (0xb00f + index) * 2)[0] == 0x3c04]
    if request_entries != [0x07]:
        raise ValueError('ROM4 host table does not uniquely select 3c04 for type 07')
    return dict(program_sha256=hashlib.sha256(program).hexdigest(),
                type8b=dict(publisher='47f6', ring_call='4810', payload_bytes=162,
                            direct_caller_candidates=[hex(address) for address, _ in callers]),
                measurement_caller='0a17', host_query=dict(type='4b', entry='3c92', call='3cab'),
                conditional_processing=dict(call='6d7e', eligibility_flag='06bc mask 0100',
                                            additional_gate='06ab equals zero',
                                            setter='67a3', control_word='06e3 mask 0020',
                                            control_bit_consumed_at='679b',
                                            countdown_initialized_at='67a1',
                                            host_request_type='07', host_request_entry='3c04',
                                            request_arm_at='3c15',
                                            setter_runtime_observed=False),
                unconditional_heartbeat_proved=False, runtime_result_observed=False,
                indirect_producers_closed=False, fitted_mask_proved=False)


def inspect_capture(program, resident, data):
    native_completion_contract(resident, data)
    result = inspect_program(program, data)
    result['publisher_census'] = live_publisher_census(program, resident)
    result['serial_contract'] = serial_publisher_contract(program)
    result['publisher_types'] = publisher_type_inventory(program)
    return result


def inspect_control_trace(text):
    """Require complete writes, not a capped log, before excluding this bit."""
    rows = re.findall(r'nse2_native_ring: address=06e3 data=([0-9a-f]{4}) mask=ffff '
                      r'pc=([0-9a-f]{4}) t=([0-9.]+) observer_writes=0', text)
    samples = re.findall(r'nse2_native_transport: t=([0-9.]+) [^\n]*?'
                         r'06e3:([0-9a-f]{4}):(\d+) [^\n]*observer_writes=0', text)
    terminal = [(value, int(count)) for time, value, count in samples
                if 29.9 <= float(time) <= 30.1]
    if len(terminal) != 1 or not rows or len(rows) != terminal[0][1]:
        raise ValueError('complete NSE-2 control-write coverage through thirty seconds missing')
    times = [float(time) for _, _, time in rows]
    if times != sorted(times) or not all(0 <= time <= 30.1 for time in times):
        raise ValueError('NSE-2 control-write observation order/window differs')
    if rows[-1][0] != terminal[0][0]:
        raise ValueError('NSE-2 control-write final value differs from terminal snapshot')
    if any(int(value, 16) & 0x20 for value, _, _ in rows):
        raise ValueError('NSE-2 conditional result request was armed; absence diagnostic no longer applies')
    return dict(control_word='06e3', request_mask='0020', writes=len(rows),
                observed_values=[value for value, _, _ in rows],
                writer_pcs=[pc for _, pc, _ in rows],
                request_bit_observed=False, observation_seconds=30,
                scope='complete tapped writes in this capture; not a ROM-wide producer exclusion')


def inspect_mcu_request(image):
    """Own MCU constructor candidate; do not infer normal-boot reachability."""
    from tools.nse5_roller_contract import thumb_bl_candidates
    # Pin the producer and its mode literal separately from the generic
    # envelope boundary; a coincidental immediate 7 is not a producer proof.
    for address, encoded in MCU_REQUEST_FRAGMENTS.items():
        expected = bytes.fromhex(encoded)
        if image[address - 0x200000:address - 0x200000 + len(expected)] != expected:
            raise ValueError(f'NSE-2 MCU request contract differs at {address:06x}')
    if struct.unpack_from('>I', image, 0x16388)[0] != 0x1094b7:
        raise ValueError('NSE-2 request constructor mode literal differs')
    callers = thumb_bl_candidates(image, (0x2160a8,))[0x2160a8]
    if callers != [0x216250, 0x21764a]:
        raise ValueError('NSE-2 request constructor direct-call candidates differ')
    return dict(constructor='2160a8', mode_byte='1094b7', required_mode=4,
                packet_type='07', payload='10', task=3,
                direct_call_candidates=[f'{address:06x}' for address in callers],
                normal_boot_reachability_proved=False,
                scope='checked constructor and raw aligned direct calls; other producers unclosed')


def inspect_nv_policy(image):
    """Recover own ROM initialization, not a donor/factory EEPROM guess."""
    for address, encoded in NV_POLICY_FRAGMENTS.items():
        expected = bytes.fromhex(encoded)
        if image[address - 0x200000:address - 0x200000 + len(expected)] != expected:
            raise ValueError(f'NSE-2 NV policy contract differs at {address:06x}')
    if len(image) < 0xb376c:
        raise ValueError('NSE-2 NV initialization table truncated')
    source, records = struct.unpack_from('>II', image, 0x90670)
    if (source, records) != (0x2b3764, 0x2b3768):
        raise ValueError('NSE-2 NV initialization pointers differ')
    defaults = list(image[source - 0x200000:source - 0x200000 + 2])
    ids = list(struct.unpack_from('>HH', image, records - 0x200000))
    if ids != [0x401, 0x607] or defaults != [1, 0xff]:
        raise ValueError('NSE-2 NV initialization record/default pairing differs')
    return dict(writer='290490', record_write_api='290de2', callsite='2904a0',
                records=[dict(record=f'{record:04x}', value=f'{value:02x}')
                         for record, value in zip(ids, defaults)],
                caller='23565a', caller_condition='argument bit 1 set (LSRS #2 carry)',
                policy_bits={'0': 'argument-1 reason 68 selection', '1': 'argument-2 reason 69 selection'},
                record_0607_reader_only=False, zero_factory_value_proved=False,
                initialization_runtime_observed=False,
                scope='own table-driven initializer; not complete writer census or factory provisioning proof')


def inspect_request_lifecycle(text):
    rows = re.findall(r'nse2_request_mode: value=([0-9a-f]{2}) pc=([0-9a-f]{8}) '
                      r't=([0-9.]+) observer_writes=0', text)
    summaries = re.findall(r'nse2_request_summary: mode=([0-9a-f]{2}) mode_writes=(\d+) '
                           r'constructor_entries=(\d+) t=([0-9.]+) observer_writes=0', text)
    if len(summaries) != 1:
        raise ValueError('NSE-2 request lifecycle terminal summary missing or duplicated')
    mode, count, entries, time = summaries[0]
    if mode != '02' or int(count) != len(rows) or int(entries) != 0 or not 29.9 <= float(time) <= 30.1:
        raise ValueError('NSE-2 request lifecycle coverage/frontier differs')
    if [(value, pc) for value, pc, _ in rows] != [
            ('00', '0020010c'), ('01', '0020cafc'), ('02', '0020feb6')]:
        raise ValueError('NSE-2 request lifecycle mode transitions differ')
    times = [float(time) for _, _, time in rows]
    if not 0 < times[0] < times[1] < times[2] < 5:
        raise ValueError('NSE-2 request lifecycle transition ordering differs')
    if 'nse2_request_constructor:' in text:
        raise ValueError('NSE-2 constructor entry contradicts terminal summary')
    return dict(mode_transitions=['00', '01', '02'], terminal_mode='02',
                constructor_entry_observations=0, required_mode_observed=False,
                mode_writes=len(rows), observation_seconds=30,
                scope='observed boot lifecycle; type-07 normal-boot necessity not proved')


def inspect_scan_pacing(text):
    rows = re.findall(r'nse2_native_scan_stage: address=([0-9a-f]{4}) n=(\d+) '
                      r'pc=([0-9a-f]{4}) mode=([0-9a-f]{4}) work=([0-9a-f]{4}) '
                      r't=([0-9.]+) observer_writes=0', text)
    stages = {}
    for address, index, pc, mode, work, time in rows:
        key = (int(index), int(address, 16))
        if key in stages or int(pc, 16) not in (key[1], key[1] + 1):
            raise ValueError('NSE-2 scan-stage duplicate or execution-PC mismatch')
        stages[key] = (float(time), int(mode, 16), int(work, 16))
    addresses = (0x990, 0x99d, 0x9a6, 0x9b1, 0x9c2)
    if set(stages) != {(index, address) for index in range(1, 9) for address in addresses}:
        raise ValueError('NSE-2 complete first-eight measurement stage observations missing')
    setup, processing, cycles = [], [], []
    # A model-derived period, not an independent measurement of silicon.
    period = (0x546 + 1) * 12 / 13_000_000
    for index in range(1, 9):
        sequence = [stages[index, address][0] for address in addresses]
        if sequence != sorted(sequence) or len(set(sequence)) != len(sequence):
            raise ValueError('NSE-2 scan-stage ordering differs')
        if stages[index, 0x9a6][1] != 12 or stages[index, 0x9b1][1] != 4:
            raise ValueError('NSE-2 scan processing mode transition differs')
        if not stages[index, 0x9b1][2] & 1:
            raise ValueError('NSE-2 scan completion work bit missing')
        setup.append(sequence[1] - sequence[0])
        processing.append(sequence[3] - sequence[2])
        if index < 8:
            cycles.append(stages[index + 1, 0x990][0] - sequence[0])
    if (not all(0.95 < value / period < 1.05 for value in setup)
            or not all(10.95 < value / period < 11.05 for value in processing)
            or not all(11.95 < value / period < 12.05 for value in cycles)):
        raise ValueError('NSE-2 observed one/eleven/twelve-period scan pacing differs')
    return dict(measurements=8, modeled_frame_period_seconds=period,
                setup_mean_seconds=sum(setup) / len(setup),
                processing_wait_mean_seconds=sum(processing) / len(processing),
                measurement_period_mean_seconds=sum(cycles) / len(cycles),
                modeled_periods_per_measurement=12, hardware_clock_fault_proved=False,
                scope='first-eight observed scan timings relative to current model; not physical timing validation')


def inspect_completion_counters(text):
    rows = re.findall(r'nse2_native_scan_completion: address=([0-9a-f]{4}) n=(\d+) '
                      r'mode=([0-9a-f]{4}) skip=([0-9a-f]{4}) accumulation=([0-9a-f]{4}) '
                      r'ar2=([0-9a-f]{4}) t=([0-9.]+) observer_writes=0', text)
    groups = {}
    for address, index, mode, skip, accumulation, ar2, time in rows:
        group = groups.setdefault(int(address, 16), [])
        if int(index) != len(group) + 1:
            raise ValueError('NSE-2 completion counter observation sequence differs')
        group.append((int(mode, 16), int(skip, 16), int(accumulation, 16),
                      int(ar2, 16), float(time)))
    frames = groups.get(0x3204, [])
    expected = [(12, 1, 1), (12, 0, 1), (12, 2, 0), (12, 1, 0), (12, 0, 1),
                (4, 2, 0), (4, 1, 0), (4, 0, 4), (4, 0, 3), (4, 0, 2), (4, 0, 1)]
    if [row[:3] for row in frames] != expected:
        raise ValueError('NSE-2 first-measurement frame counter progression differs')
    period = 1351 * 12 / 13_000_000
    if not all(abs((right[4] - left[4]) / period - 1) < 0.01
               for left, right in zip(frames, frames[1:])):
        raise ValueError('NSE-2 first-measurement frame cadence differs')
    if (len(groups.get(0x326d, [])) != 6 or len(groups.get(0x32c5, [])) != 5
            or len(groups.get(0x32b3, [])) != 1 or len(groups.get(0x32be, [])) != 1):
        raise ValueError('NSE-2 first-measurement skip/process/completion coverage differs')
    completion = groups[0x32be][0]
    if completion[:3] != (4, 0, 0) or not 0 < completion[4] - frames[-1][4] < period:
        raise ValueError('NSE-2 work-bit completion does not follow last accumulation')
    return dict(frame_interrupts=11, processing_entries=6, skip_entries=5,
                mode_sequence=[12, 4], completion_accumulation=0,
                missing_frame_in_observed_wait=False, physical_timing_validated=False,
                scope='first native measurement only; does not establish supervision policy correctness')


def compare_scan_budget(text):
    from tools.nse2_lifecycle_check import diagnose_scan_progress, diagnose_timer_boundary
    # The progression oracle covers the first thirty seconds; a failure
    # capture also contains later loop entries and a second native boot.
    progress_text = '\n'.join(line for line in text.splitlines()
                              if 'nse2_native_scan_loop:' not in line
                              or (match := re.search(r' t=([0-9.]+) ', line)) is None
                              or float(match[1]) <= 30.1)
    diagnose_scan_progress(progress_text)
    boundary = diagnose_timer_boundary(text)
    pacing = inspect_scan_pacing(text)
    inspect_completion_counters(text)
    arm = boundary['receive_timer_start']
    # Projection from the decoded eight inclusive outer counts, two passes,
    # and 124 inclusive inner counts. This is not an observed completion.
    iterations = 8 * 2 * 124
    stages = re.findall(r'nse2_native_scan_stage: address=0990 n=1 [^\n]*t=([0-9.]+) observer_writes=0', text)
    if len(stages) != 1:
        raise ValueError('NSE-2 first measurement start missing or duplicated')
    start = float(stages[0])
    projected_work = iterations * pacing['measurement_period_mean_seconds']
    # diagnose_timer_boundary pins the reason/reset observations, including
    # the actual policy time; do not replace it with a nominal timer rate.
    reason = re.findall(r'nse2_reset_reason: value=68 pc=0028db6e lr=00290361 t=([0-9.]+) observer_writes=0', text)
    policy = float(reason[0])
    if not arm < start < policy < start + projected_work:
        raise ValueError('NSE-2 projected scan/supervision ordering differs')
    return dict(iterations=iterations, first_measurement_start=start,
                projected_measurement_work_seconds=projected_work,
                projected_end_without_outer_gaps=start + projected_work,
                observed_policy_time=policy, arm_to_policy_seconds=policy - arm,
                projection_exceeds_observed_policy=True, scan_completion_observed=False,
                root_cause_proved=False,
                scope='same-run timing projection; excludes outer gaps, does not validate clock or NV policy')


def inspect_publisher_runtime(text):
    from tools.nse2_lifecycle_check import diagnose_transport
    diagnose_transport(text)
    sites = sorted((*PUBLISHER_TYPE_LOADS,0x9404,0x37ce))
    counts = {}
    for address in sites:
        rows = re.findall(fr'rom4_dispatch_count: address={address:04x} count=(\d+)',text)
        if len(rows) != 1:
            raise ValueError('NSE-2 complete direct-publisher count summary missing')
        counts[address] = int(rows[0])
    rows = re.findall(r'rom4_publisher_entry: n=(\d+) type=([0-9a-f]{4}) '
                      r'length=([0-9a-f]{4}) t=([0-9.]+) observer_writes=0',text)
    if (len(rows) != counts[0x37ce] or len(rows) != 4
            or [int(row[0]) for row in rows] != list(range(1,5))
            or [(row[1],int(row[2],16)) for row in rows] != [('0074',16),('0074',52),('0074',2),('0074',4)]):
        raise ValueError('NSE-2 initial control-response publisher entries differ')
    times = [float(row[3]) for row in rows]
    if times != sorted(times) or not all(0 < time < 1 for time in times):
        raise ValueError('NSE-2 initial publisher entry ordering differs')
    active = {address:count for address,count in counts.items() if count and address != 0x37ce}
    if active != {0x4a11:1,0x4a21:1,0x4b1a:1,0x4ba8:1}:
        raise ValueError('NSE-2 direct publisher attribution differs')
    return dict(common_entries=4, packet_types=['74']*4,
                active_direct_sites=[f'{address:04x}' for address in sorted(active)],
                inactive_direct_sites=[f'{address:04x}' for address in sites
                                       if address != 0x37ce and not counts[address]],
                observation_seconds=30, initial_response_delivery_proved=True,
                scope='observed direct sites and common entry match four initial RX publications; not all possible ring writers')


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('live_program', type=Path)
    parser.add_argument('--trace', type=Path, help='check complete conditional-request write coverage')
    parser.add_argument('--mcu', type=Path, help='check own big-endian NSE-2 request constructor')
    parser.add_argument('--failure-trace', type=Path, help='compare measured scan workload with observed restart')
    parser.add_argument('--resident', type=Path, default=root / 'roms/noki5110/nse1_rom4_dsp_program.bin')
    parser.add_argument('--data', type=Path, default=root / 'roms/noki5110/nse1_rom4_dsp_data.bin')
    args = parser.parse_args()
    try:
        program, resident, data = args.live_program.read_bytes(), args.resident.read_bytes(), args.data.read_bytes()
        result = inspect_capture(program, resident, data)
        if args.trace:
            result['control_observation'] = inspect_control_trace(args.trace.read_text())
        if args.mcu:
            mcu = args.mcu.read_bytes()
            result['mcu_request'] = inspect_mcu_request(mcu)
            result['nv_policy'] = inspect_nv_policy(mcu)
        if args.failure_trace:
            result['scan_budget'] = compare_scan_budget(args.failure_trace.read_text())
            if args.mcu:
                result['receive_activity'] = inspect_receive_activity(mcu,args.failure_trace.read_text(),expiry=True)
        print(json.dumps(result, indent=2))
    except (OSError, ValueError) as error:
        parser.exit(1, str(error) + '\n')


if __name__ == '__main__':
    main()

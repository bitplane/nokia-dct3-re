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
from tools.nsk1_radio_contract import live_publisher_census, native_completion_contract, serial_publisher_contract

FRAGMENTS = {
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


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('live_program', type=Path)
    parser.add_argument('--trace', type=Path, help='check complete conditional-request write coverage')
    parser.add_argument('--resident', type=Path, default=root / 'roms/noki5110/nse1_rom4_dsp_program.bin')
    parser.add_argument('--data', type=Path, default=root / 'roms/noki5110/nse1_rom4_dsp_data.bin')
    args = parser.parse_args()
    try:
        program, resident, data = args.live_program.read_bytes(), args.resident.read_bytes(), args.data.read_bytes()
        result = inspect_capture(program, resident, data)
        if args.trace:
            result['control_observation'] = inspect_control_trace(args.trace.read_text())
        print(json.dumps(result, indent=2))
    except (OSError, ValueError) as error:
        parser.exit(1, str(error) + '\n')


if __name__ == '__main__':
    main()

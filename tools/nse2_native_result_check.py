"""Qualify NSE-2's live ROM4 result paths; no runtime completion is implied."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.c54x_rom4_port_census import direct_callers
from tools.nsk1_radio_contract import live_publisher_census, native_completion_contract, serial_publisher_contract

FRAGMENTS = {
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
    return dict(program_sha256=hashlib.sha256(program).hexdigest(),
                type8b=dict(publisher='47f6', ring_call='4810', payload_bytes=162,
                            direct_caller_candidates=[hex(address) for address, _ in callers]),
                measurement_caller='0a17', host_query=dict(type='4b', entry='3c92', call='3cab'),
                conditional_processing=dict(call='6d7e', eligibility_flag='06bc mask 0100',
                                            additional_gate='06ab equals zero'),
                unconditional_heartbeat_proved=False, runtime_result_observed=False,
                indirect_producers_closed=False, fitted_mask_proved=False)


def inspect_capture(program, resident, data):
    native_completion_contract(resident, data)
    result = inspect_program(program, data)
    result['publisher_census'] = live_publisher_census(program, resident)
    result['serial_contract'] = serial_publisher_contract(program)
    return result


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('live_program', type=Path)
    parser.add_argument('--resident', type=Path, default=root / 'roms/noki5110/nse1_rom4_dsp_program.bin')
    parser.add_argument('--data', type=Path, default=root / 'roms/noki5110/nse1_rom4_dsp_data.bin')
    args = parser.parse_args()
    try:
        program, resident, data = args.live_program.read_bytes(), args.resident.read_bytes(), args.data.read_bytes()
        result = inspect_capture(program, resident, data)
        print(json.dumps(result, indent=2))
    except (OSError, ValueError) as error:
        parser.exit(1, str(error) + '\n')


if __name__ == '__main__':
    main()

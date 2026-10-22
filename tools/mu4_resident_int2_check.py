"""Paired native resident wake fixture; not a normal MSP430 waveform claim."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.mu4_context_contract import contract
from tools.mu4_playback_frame_check import rows
from tools.mu4_playback_serial_control_check import check as serial_check

SITES = {'enter': ('028914', '4a00'), 'resume': ('028925', '8a01'),
         'restore_mask': ('028926', '8a00'), 'caller': ('03ac0d', 'f020')}


def check_branch(text, console, pulse, *, resident_status=None):
    if resident_status is None:
        serial_check(text, console)
    else:
        serial_check(text, console, resident_status=resident_status)
    states = rows(text, 'mu4_int2_state')
    if [row['phase'] for row in states] != ['before', 'released', 'after']:
        raise ValueError('wake snapshot sequence differs')
    if [float(row['t']) for row in states] != [11, 11.05, 11.2]:
        raise ValueError('wake snapshot window differs')
    for row in states:
        if row['illegal'] != '0' or row['idle'] != '1':
            raise ValueError('wake snapshot is not a clean idle')
    before = states[0]
    if (before['pc'], before['imr'], before['ifr'], before['st1']) != ('028923', '0004', '0c38', '6113'):
        raise ValueError('resident wake precondition differs')
    expected = ('0298ea', '0ac5') if pulse else ('028923', '0004')
    if any((row['pc'], row['imr']) != expected for row in states[1:]):
        raise ValueError('wake continuation differs')
    paths = rows(text, 'mu4_int2_path')
    kinds = [row['kind'] for row in paths]
    if kinds != (['enter', 'resume', 'restore_mask', 'caller', 'enter'] if pulse else ['enter']):
        raise ValueError('native resident path sequence differs')
    for index, row in enumerate(paths, 1):
        if (int(row['ordinal']) != index or (row['pc'], row['opcode']) != SITES[row['kind']] or
                row.get('identity') != 'Execute_entry' or row.get('operands') != 'context_only'):
            raise ValueError('native resident source identity differs')
    terminal = rows(text, 'mu4_int2_paths_terminal')
    if len(terminal) != 1 or terminal[0]['cap'] != '32' or int(terminal[0]['events']) != len(paths):
        raise ValueError('resident path terminal differs')
    writes = rows(text, 'mu4_int2_register_write')
    terminal = rows(text, 'mu4_int2_writes_terminal')
    if (len(terminal) != 1 or terminal[0]['cap'] != '1024' or
            int(terminal[0]['events']) != len(writes) or len(writes) >= 1024):
        raise ValueError('interrupt-register observation is incomplete or capped')
    for index, row in enumerate(writes):
        if (int(row['index']) != index or row['address'] not in ('0000', '0001') or
                row['mask'] != 'ffff' or row.get('identity') != 'data_write' or
                row.get('operands') != 'context_only' or not 11 <= float(row['t']) < 11.2):
            raise ValueError('interrupt-register write identity differs')
    if not pulse and writes:
        raise ValueError('no-pulse control wrote interrupt registers')
    edges = re.findall(r'mu4_mcu_int2_input: asserted=([01]) net=UC_DSP_INT cpu_irq=2 t=([0-9.]+) source=bench_pin firmware_state_forcing=0', text)
    if text.count('mu4_mcu_int2_input:') != len(edges):
        raise ValueError('unclassified pin edge')
    if pulse:
        if [edge[0] for edge in edges] != ['1', '0'] or abs(float(edges[1][1])-float(edges[0][1])-0.05) > 1e-8:
            raise ValueError('declared pin pulse differs')
        prefix = [('0000', '0004', '0000'), ('0000', '0000', '0004'),
                  ('0001', '0c38', '0438'), ('0000', '0004', '0ac5')]
        actual = [(row['address'], row['before'], row['value']) for row in writes]
        if len(actual) != 258 or actual[:4] != prefix:
            raise ValueError('native interrupt restore publication differs')
        expected_tail = [('0000', '0ac5', '0000'), ('0000', '0000', '0ac5')] * 127
        if actual[4:] != expected_tail:
            raise ValueError('native worker mask preservation differs')
        if paths[2]['ifr'] != '0808' or paths[3]['imr'] != '0ac5':
            raise ValueError('native W1C/mask continuation differs')
    elif edges:
        raise ValueError('no-pulse control contains a pin edge')
    marker = ('mu4_int2_fixture: complete=1 pulse_asserted=' + str(int(pulse)) +
              ' declared_pulse_ms=50 normal_mcu_waveform=unproved firmware_state_forcing=0')
    if text.count(marker) != 1:
        raise ValueError('declared fixture scope differs')
    return states, writes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('input_nand', type=Path)
    parser.add_argument('control', type=Path)
    parser.add_argument('pulse', type=Path)
    args = parser.parse_args()
    try:
        contract(args.source.read_bytes())
        original = args.input_nand.read_bytes()
        if len(original) != 69206016:
            raise ValueError('input NAND size differs')
        results = []
        for directory, pulse in ((args.control, False), (args.pulse, True)):
            if (directory / 'nvram/mu4nand_30/nand').read_bytes() != original:
                raise ValueError('retained input NAND differs; no durability claim')
            results.append(check_branch((directory / 'error.log').read_text(),
                                        Path(str(directory) + '-launch.log').read_text(), pulse))
        if results[0][0][0] != results[1][0][0]:
            raise ValueError('paired pre-pulse states differ')
    except (OSError, ValueError, RuntimeError, KeyError) as error:
        parser.exit(1, f'MU4 resident INT2: FAIL: {error}\n')
    print('PASS: paired native INT2 wake; logical bench pin only; normal MSP430 waveform/audio completion unproved')


if __name__ == '__main__':
    main()

import json
from pathlib import Path
import tempfile
import unittest
import xml.etree.ElementTree as ET

from tools import nsk3_toolkit_check as checker
from tools.nsk3_toolkit_prepare import profile_metadata
from tools.test_nsk3_native_state_check import trace as startup_fixture


def wire(direction, data):
    return '\n'.join(f'nsk3_sim_wire: direction={direction} data={value:02x} t=62.0 observer_writes=0'
                     for value in data)


def trace(control=False):
    lines = [line for line in startup_fixture().splitlines() if not line.startswith('nsk3_state')]
    lines.append('read-binary fid=6fae offset=0 length=1 first=' + ('02' if control else '03'))
    if control:
        lines += [wire('TX', bytes.fromhex('a0a40000023f00')), wire('RX', b'\x90\x00')]
    else:
        lines += [wire('TX', bytes.fromhex('a010000004') + checker.TERMINAL_PROFILE),
                  'SIM status ins=10 sw=9000 chv=3/3 puk=10/10 enabled=1',
                  'SIM completion ins=f2 sw=9116',
                  wire('TX', bytes.fromhex('a012000016')),
                  wire('RX', checker.DISPLAY + b'\x90\x00'),
                  wire('TX', bytes.fromhex('a01400000c') + checker.REFUSAL),
                  'SIM status ins=14 sw=9000 chv=3/3 puk=10/10 enabled=1']
    lines += ['nsk3_toolkit_native: sample=1 pc=00408d stopped=0 idle=1 observer_writes=0',
              'nsk3_toolkit_physical: action=dismiss down=1 t=68.000000000',
              'nsk3_toolkit_physical: action=dismiss down=0 t=68.200000000',
              'nsk3_toolkit_native: sample=2 pc=00408d stopped=0 idle=1 observer_writes=0']
    return '\n'.join(lines)


class Nsk3ToolkitTest(unittest.TestCase):
    def test_refusal_and_matched_phase2_control(self):
        checker.check_trace(trace())
        checker.check_trace(trace(True), True)
        for text, control in ((trace(), True), (trace(True), False)):
            with self.assertRaises(ValueError):
                checker.check_trace(text, control)

    def test_not_success_not_sibling_profile_and_not_mutated_payload(self):
        text = trace()
        for bad in (text.replace('data=30 ', 'data=00 ', 1),
                    text.replace('data=30 ', 'data=31 ', 1),
                    text.replace('data=03 ', 'data=0f ', 1),
                    text.replace('direction=RX data=44 ', 'direction=RX data=45 ', 1),
                    text.replace('ins=14 sw=9000', 'ins=14 sw=6a80'),
                    text.replace('sw=9116', 'sw=9000'),
                    text.replace('first=03', 'first=02'),
                    text + '\n' + wire('TX', bytes.fromhex('a0dc010401ff'))):
            with self.subTest(tail=bad[-100:]), self.assertRaises(ValueError):
                checker.check_trace(bad)

    def test_refusal_precedes_key_and_ui_and_native_markers_are_required(self):
        text = trace()
        response = wire('TX', bytes.fromhex('a01400000c') + checker.REFUSAL)
        marker = 'nsk3_toolkit_physical: action=dismiss down=1 t=68.000000000'
        late = text.replace(response, '').replace(marker, marker + '\n' + response)
        for bad in (late, text.replace('down=0 t=68.200000000', 'down=1 t=68.200000000'),
                    text.replace('stopped=0', 'stopped=1'),
                    text.replace('sample=2', 'sample=3'),
                    text + '\nnsk3_toolkit_native: malformed',
                    text + '\nnsk3_toolkit_physical: malformed',
                    text + '\nnsk3_reset_request: command=05',
                    text + '\nunhandled opcode'):
            with self.subTest(tail=bad[-100:]), self.assertRaises(ValueError):
                checker.check_trace(bad)

    def test_wire_records_cannot_be_absent_malformed_or_capped(self):
        for direction in ('TX', 'RX'):
            for text in ('', wire(direction, b'\x00' * 8192),
                         wire(direction, b'\x90') + f'\nnsk3_sim_wire: direction={direction} data=zz '):
                with self.assertRaises(ValueError):
                    checker.wire(text, direction, 'nsk3')

    def test_external_profile_only_and_exact_provenance(self):
        for profile in (0, 1):
            with tempfile.TemporaryDirectory() as temporary:
                seed, run = (Path(temporary) / name for name in ('seed', 'run'))
                for directory in (seed, run):
                    (directory / 'cfg').mkdir(parents=True)
                xml = '<mameconfig><system name="nsk3r4t"><input><port tag=":SIM_REMOVED" value="0"/></input></system></mameconfig>'
                (seed / 'cfg/nsk3r4t.cfg').write_text(xml)
                tree = ET.ElementTree(ET.fromstring(xml))
                inputs = tree.find('./system/input')
                sat = ET.SubElement(inputs, 'port', tag=':SATCFG', type='CONFIG',
                                    mask='15', defvalue='0', value=str(profile))
                path = run / 'cfg/nsk3r4t.cfg'
                tree.write(path)
                metadata = run / 'toolkit_input.json'
                metadata.write_text(json.dumps(profile_metadata(profile)))
                checker.check_configuration(seed, run, profile)
                sat.set('value', str(1-profile))
                tree.write(path)
                with self.assertRaises(ValueError):
                    checker.check_configuration(seed, run, profile)
                sat.set('value', str(profile))
                inputs.find('port').set('value', '1')
                tree.write(path)
                with self.assertRaises(ValueError):
                    checker.check_configuration(seed, run, profile)
                inputs.find('port').set('value', '0')
                inputs.remove(sat)
                tree.write(path)
                if profile:
                    with self.assertRaises(ValueError):
                        checker.check_configuration(seed, run, profile)
                else:
                    checker.check_configuration(seed, run, profile)
                metadata.write_text(json.dumps(dict(profile=profile)))
                with self.assertRaises(ValueError):
                    checker.check_configuration(seed, run, profile)

    def test_unsupported_profiles_are_not_silently_selected(self):
        for profile in (-1, 2, 7, 15):
            with self.assertRaises(ValueError):
                profile_metadata(profile)


if __name__ == '__main__':
    unittest.main()

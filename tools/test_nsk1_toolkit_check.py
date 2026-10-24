import hashlib
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest
from unittest.mock import patch

from tools import nsk1_toolkit_check as checker
from tools.sim_toolkit_display_check import DISPLAY, REFUSAL
from tools.test_nsk1_diagnostic_display_check import trace as authentication


def wire(direction, data):
    return '\n'.join(f'nsk1_sim_wire: direction={direction} data={value:02x} t=62.0 observer_writes=0'
                     for value in data)


def trace(control=False):
    lines = [authentication(True),
             'read-binary fid=6fae offset=0 length=1 first=' + ('02' if control else '03')]
    if control:
        lines += [wire('TX', bytes.fromhex('a0a40000023f00')), wire('RX', b'\x90\x00')]
    else:
        lines += [wire('TX', bytes.fromhex('a010000004') + checker.PROFILE),
                  'SIM status ins=10 sw=9000 chv=3/3 puk=10/10 enabled=0',
                  'SIM completion ins=f2 sw=9116', wire('TX', bytes.fromhex('a012000016')),
                  wire('RX', DISPLAY + b'\x90\x00'),
                  wire('TX', bytes.fromhex('a01400000c') + REFUSAL),
                  'SIM status ins=14 sw=9000 chv=3/3 puk=10/10 enabled=0']
    lines += ['nsk1_toolkit_native: sample=1 pc=0047a1 stopped=0 idle=0 observer_writes=0',
              'nsk1_toolkit_physical: action=dismiss down=1 t=68.000000000',
              'nsk1_code_key: value=19 t=68.1 observer_writes=0',
              'nsk1_toolkit_physical: action=dismiss down=0 t=68.200000000',
              'nsk1_toolkit_native: sample=2 pc=0047a1 stopped=0 idle=0 observer_writes=0']
    return '\n'.join(lines)


class Nsk1ToolkitTest(unittest.TestCase):
    def test_record_geometry_is_own_static_evidence_not_assigned_semantics(self):
        image = bytearray(0xa8a14)
        for offset, encoded in checker.NV_RECORDS.items():
            image[offset:offset+8] = bytes.fromhex(encoded)
        checker.check_record_catalogue(image)
        for offset in checker.NV_RECORDS:
            changed = bytearray(image)
            changed[offset+4] ^= 1
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                checker.check_record_catalogue(changed)

    def test_card_port_order_default_elision_and_unrelated_inputs(self):
        with TemporaryDirectory() as temporary:
            seed, run = (Path(temporary) / name for name in ('seed','run'))
            for directory in (seed,run):
                (directory / 'cfg').mkdir(parents=True)
            template = '<mameconfig><system name="nsk1r4t"><input><keyboard enabled="1"/>{}<port tag=":SIM_REMOVED" value="0"/></input></system></mameconfig>'
            (seed / 'cfg/nsk1r4t.cfg').write_text(template.format(''))
            path = run / 'cfg/nsk1r4t.cfg'
            for profile in (0,1):
                port = f'<port tag=":SATCFG" type="CONFIG" mask="15" defvalue="0" value="{profile}"/>'
                path.write_text(template.format(port))
                checker.check_configuration(seed, run, not profile)
                with self.assertRaises(ValueError):
                    checker.check_configuration(seed, run, bool(profile))
                path.write_text(template.format(port+port))
                with self.assertRaises(ValueError):
                    checker.check_configuration(seed, run, not profile)
            path.write_text(template.format(''))
            checker.check_configuration(seed, run, True)
            path.write_text(template.format('').replace('enabled="1"', 'enabled="0"'))
            with self.assertRaises(ValueError):
                checker.check_configuration(seed, run, True)

    def test_refusal_is_not_success_and_control_has_no_proactive_exchange(self):
        checker.check_trace(trace())
        checker.check_trace(trace(True), True)
        for text, control in ((trace(), True), (trace(True), False)):
            with self.assertRaises(ValueError):
                checker.check_trace(text, control)

    def test_own_profile_refusal_order_native_input_and_wire_are_required(self):
        good = trace()
        response = wire('TX', bytes.fromhex('a01400000c') + REFUSAL)
        late = good.replace(response, '') + '\n' + response
        for bad in (late, good.replace('data=30 ', 'data=00 ', 1),
                    good.replace('data=03 ', 'data=0b ', 1),
                    good.replace('ins=14 sw=9000', 'ins=14 sw=6a80'),
                    good.replace('stopped=0', 'stopped=1'),
                    good.replace('value=19 t=68.1', 'value=18 t=68.1'),
                    good + '\nnsk1_code_key: malformed',
                    good + '\nnsk1_toolkit_native: malformed',
                    good + '\nnsk1_restart_request:',
                    good + '\nunhandled opcode'):
            with self.subTest(tail=bad[-100:]), self.assertRaises(ValueError):
                checker.check_trace(bad)

    def test_known_eeprom_transition_is_never_integrity_acceptance(self):
        before = bytearray(2048)
        before[0x299] = 0x3d
        after = bytearray(before)
        after[0x551] = 1
        with patch('tools.nsk1_diagnostic_display_check.SELECTED', hashlib.sha256(before).hexdigest()), \
                patch.object(checker, 'CHANGED_BANK', hashlib.sha256(after).hexdigest()):
            result = checker.check_bank_transition(before, after)
            self.assertEqual(result['eeprom_changed_bytes'], 1)
            self.assertFalse(result['eeprom_integrity_validated'])
            self.assertEqual(checker.check_bank_transition(before, before, True)['eeprom_changed_bytes'], 0)
            with self.assertRaises(ValueError):
                checker.check_bank_transition(before, after, True)
            after[0] ^= 1
            with self.assertRaises(ValueError):
                checker.check_bank_transition(before, after)


if __name__ == '__main__':
    unittest.main()

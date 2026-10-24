import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

from tools import nsm1_toolkit_check as checker
from tools.sim_toolkit_display_check import DISPLAY, RESPONSE
from tools.test_nsm1_native_state_check import trace as startup


def wire(direction, data):
    return '\n'.join(f'nsm1_sim_wire: direction={direction} data={value:02x} t=62.0 observer_writes=0'
                     for value in data)


def trace(control=False):
    lines = [line for line in startup().splitlines() if not line.startswith('nsm1_state')]
    lines.append('read-binary fid=6fae offset=0 length=1 first=' + ('02' if control else '03'))
    if control:
        lines += [wire('TX', bytes.fromhex('a0a40000023f00')), wire('RX', b'\x90\x00')]
    else:
        lines += [wire('TX', bytes.fromhex('a010000005') + checker.PROFILE),
                  'SIM status ins=10 sw=9000 chv=3/3 puk=10/10 enabled=1',
                  'SIM completion ins=f2 sw=9116',
                  wire('TX', bytes.fromhex('a012000016')),
                  wire('RX', DISPLAY + b'\x90\x00')]
    lines += ['nsm1_toolkit_physical: action=dismiss down=1 t=68.000000000',
              'nsm1_toolkit_physical: action=dismiss down=0 t=68.200000000']
    if not control:
        lines += [wire('TX', bytes.fromhex('a01400000c') + RESPONSE),
                  'SIM status ins=14 sw=9000 chv=3/3 puk=10/10 enabled=1']
    return '\n'.join(lines)


class Nsm1ToolkitTest(unittest.TestCase):
    def test_configuration_allows_only_declared_card_and_default_elision(self):
        with TemporaryDirectory() as temporary:
            seed, run = (Path(temporary) / name for name in ('seed', 'run'))
            for directory in (seed, run):
                (directory / 'cfg').mkdir(parents=True)
            template = '<mameconfig><system name="nsm1r4t"><input><port tag=":A" value="0"/>{}</input></system></mameconfig>'
            (seed / 'cfg/nsm1r4t.cfg').write_text(template.format(''))
            path = run / 'cfg/nsm1r4t.cfg'
            for profile in (0, 1):
                path.write_text(template.format(f'<port tag=":SATCFG" type="CONFIG" mask="15" defvalue="0" value="{profile}"/>'))
                checker.check_configuration(seed, run, not profile)
                with self.assertRaises(ValueError):
                    checker.check_configuration(seed, run, bool(profile))
            path.write_text(template.format(''))
            checker.check_configuration(seed, run, True)
            with self.assertRaises(ValueError):
                checker.check_configuration(seed, run)
            path.write_text(template.format('').replace('value="0"', 'value="1"'))
            with self.assertRaises(ValueError):
                checker.check_configuration(seed, run, True)

    def test_own_success_and_negative_control_are_distinct(self):
        checker.check_trace(trace())
        checker.check_trace(trace(True), True)
        for text, control in ((trace(), True), (trace(True), False)):
            with self.assertRaises(ValueError):
                checker.check_trace(text, control)

    def test_response_requires_physical_input_not_automatic_success(self):
        text = trace()
        response = wire('TX', bytes.fromhex('a01400000c') + RESPONSE)
        text = text.replace(response, '')
        marker = 'nsm1_toolkit_physical: action=dismiss down=1'
        text = text.replace(marker, response + '\n' + marker)
        with self.assertRaisesRegex(ValueError, 'precedes physical'):
            checker.check_trace(text)

    def test_profile_payload_status_contact_and_authentication_mutations_fail(self):
        text = trace()
        for bad in (text.replace('data=0b ', 'data=03 ', 1),
                    text.replace('direction=RX data=44 ', 'direction=RX data=45 ', 1),
                    text.replace('ins=14 sw=9000', 'ins=14 sw=6a80'),
                    text.replace('sw=9116', 'sw=9000'),
                    text.replace('first=03', 'first=02'),
                    text.replace('down=0 t=68.200000000', 'down=1 t=68.200000000'),
                    text.replace('ins=20 sw=9000', 'ins=20 sw=9804'),
                    text + '\nnsm1_restart_request: reason=00000068',
                    text + '\nunhandled opcode'):
            with self.subTest(tail=bad[-100:]), self.assertRaises(ValueError):
                checker.check_trace(bad)


if __name__ == '__main__':
    unittest.main()

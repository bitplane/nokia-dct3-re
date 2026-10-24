import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import xml.etree.ElementTree as ET

from tools import nsb1_toolkit_check as checker
from tools.nsb1_toolkit_prepare import profile_metadata
from tools.sim_toolkit_interactive_check import INKEY,INPUT,INKEY_RESPONSE,INPUT_RESPONSE


def wire(direction, data):
    return '\n'.join(f'nsb1_sim_wire: direction={direction} data={value:02x} t=62.0 observer_writes=0'
                     for value in data)


def trace(control=False):
    text = ['read-binary fid=6fae offset=0 length=1 first=' + ('02' if control else '03')]
    if control:
        text.extend((wire('TX', bytes.fromhex('a0a40000023f00')), wire('RX', b'\x90\x00')))
    else:
        text.extend((wire('TX', bytes.fromhex('a010000005') + checker.TERMINAL_PROFILE),
                     'SIM status ins=10 sw=9000 chv=3/3 puk=10/10 enabled=1',
                     'SIM completion ins=f2 sw=9116',
                     wire('TX', bytes.fromhex('a012000016')),
                     wire('RX', checker.DISPLAY + b'\x90\x00')))
    text.append('nsb1_toolkit_physical: action=dismiss down=1 t=68.000000000')
    if not control:
        text.extend((wire('TX', bytes.fromhex('a01400000c') + checker.RESPONSE),
                     'SIM status ins=14 sw=9000 chv=3/3 puk=10/10 enabled=1'))
    text.append('nsb1_toolkit_physical: action=dismiss down=0 t=68.200000000')
    return '\n'.join(text)


def interactive_trace():
    text=trace().replace('SIM status ins=14 sw=9000','SIM status ins=14 sw=9115')
    for payload,response,actions,status in (
            (INKEY,INKEY_RESPONSE,('inkey_5','inkey_confirm'),0x911a),
            (INPUT,INPUT_RESPONSE,('input_4','input_2','input_confirm'),0x9000)):
        text+='\n'+wire('TX',bytes((0xa0,0x12,0,0,len(payload))))
        text+='\n'+wire('RX',payload+b'\x90\x00')
        for action in actions:
            for down in (1,0):
                text+=f'\nnsb1_toolkit_input: action={action} down={down}'
        text+='\n'+wire('TX',bytes((0xa0,0x14,0,0,len(response)))+response)
        text+=f'\nSIM status ins=14 sw={status:04x} chv=3/3 puk=10/10 enabled=1'
    return text


class Nsb1ToolkitTest(unittest.TestCase):
    def test_interactive_input_is_own_profile_and_physical_result(self):
        text=interactive_trace()
        checker.check_trace(text,interactive=True)
        response=wire('TX',bytes.fromhex('a014000011')+INPUT_RESPONSE)
        early=text.replace(response,'').replace('nsb1_toolkit_input: action=input_confirm down=1',
                    response+'\nnsb1_toolkit_input: action=input_confirm down=1')
        for bad in (early,text.replace('data=0f ','data=0b ',1),
                    text.replace('action=input_2','action=input_3'),
                    text.replace('ins=14 sw=911a','ins=14 sw=9000'),
                    text+'\nnsb1_toolkit_input: malformed',text+'\nnsb1_reset_reason:68'):
            with self.subTest(tail=bad[-100:]), self.assertRaises(ValueError):
                checker.check_trace(bad,interactive=True)
        with self.assertRaises(ValueError):
            checker.check_trace(text,True,True)
        with self.assertRaises(ValueError):
            checker.check_trace(trace(),interactive=True)
        with self.assertRaises(ValueError):
            checker.check_trace(text)

    def test_own_terminal_profile_and_paired_control(self):
        checker.check_trace(trace())
        checker.check_trace(trace(True), True)
        for text, control in ((trace(), True), (trace(True), False)):
            with self.subTest(control=control), self.assertRaises(ValueError):
                checker.check_trace(text, control)

    def test_corrupted_protocol_result_payload_or_input_rejects(self):
        good = trace()
        for bad in (good.replace('data=ff ', 'data=97 ', 1),
                    good.replace('direction=RX data=44 ', 'direction=RX data=45 ', 1),
                    good.replace('ins=14 sw=9000', 'ins=14 sw=6a80'),
                    good.replace('sw=9116', 'sw=9000'),
                    good.replace('first=03', 'first=02'),
                    good.replace('down=0 t=68.200000000', 'down=1 t=68.200000000'),
                    good + '\nnsb1_reset_reason: reason=68',
                    good + '\n' + wire('TX', bytes.fromhex('a0dc010401ff')),
                    good + '\n' + wire('TX', bytes.fromhex('a012000016'))):
            with self.subTest(bad=bad[-100:]), self.assertRaises(ValueError):
                checker.check_trace(bad)

    def test_completion_must_follow_physical_dismissal(self):
        good = trace()
        response = wire('TX', bytes.fromhex('a01400000c') + checker.RESPONSE)
        early = good.replace(response, '')
        marker = 'nsb1_toolkit_physical: action=dismiss down=1 t=68.000000000'
        early = early.replace(marker, response + '\n' + marker)
        with self.assertRaisesRegex(ValueError, 'precedes physical'):
            checker.check_trace(early)

    def test_absent_and_capped_wires_reject(self):
        for direction in ('TX', 'RX'):
            with self.assertRaises(ValueError):
                checker.wire('', direction)
            with self.assertRaises(ValueError):
                checker.wire(wire(direction, b'\x00' * 8192), direction)

    def test_only_external_card_configuration_may_differ(self):
        for profile in (0, 1, 3):
            with tempfile.TemporaryDirectory() as temporary:
                source, run = Path(temporary) / 'source', Path(temporary) / 'run'
                for path in (source, run):
                    (path / 'cfg').mkdir(parents=True)
                xml = '<mameconfig version="10"><system name="nsb1r4t"><input><port tag=":SIM_REMOVED" value="0" /></input></system></mameconfig>'
                (source / 'cfg/nsb1r4t.cfg').write_text(xml)
                tree = ET.ElementTree(ET.fromstring(xml))
                inputs = tree.find('./system/input')
                sat = ET.SubElement(inputs, 'port', tag=':SATCFG', type='CONFIG',
                                    mask='15', defvalue='0', value=str(profile))
                path = run / 'cfg/nsb1r4t.cfg'
                tree.write(path)
                (run / 'input_manifest.json').write_text(json.dumps({
                    'external_toolkit_card': profile_metadata(profile)}))
                checker.check_configuration(source, run, profile)
                sat.set('value', str(1-profile))
                tree.write(path)
                with self.assertRaises(ValueError):
                    checker.check_configuration(source, run, profile)
                sat.set('value', str(profile))
                inputs.find('port').set('value', '1')
                tree.write(path)
                with self.assertRaisesRegex(ValueError, 'unrelated'):
                    checker.check_configuration(source, run, profile)
                inputs.find('port').set('value', '0')
                inputs.remove(sat)
                tree.write(path)
                if profile == 0:
                    checker.check_configuration(source, run, profile)
                else:
                    with self.assertRaises(ValueError):
                        checker.check_configuration(source, run, profile)

    def test_unqualified_profile_rejects(self):
        for profile in (-1, 2, 4, 15):
            with self.assertRaises(ValueError):
                profile_metadata(profile)

    def test_whole_persistent_card_cannot_change(self):
        card = checker.make_current_profile(True)
        with tempfile.TemporaryDirectory() as temporary:
            run = Path(temporary)
            (run / 'exit_status.json').write_text('{"returncode":0}')
            for damaged in (card[:-1], card+b'\xff', bytes([card[0]^1])+card[1:]):
                with patch.object(checker, 'check_cold_link'), \
                        patch.object(checker, 'check_configuration'), \
                        patch.object(checker, 'check_card_run', return_value=(trace(), damaged)), \
                        self.assertRaisesRegex(ValueError, 'persistent laboratory card'):
                    checker.check(run, run)


if __name__ == '__main__':
    unittest.main()

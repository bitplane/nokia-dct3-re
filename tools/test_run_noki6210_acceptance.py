import hashlib
import unittest
from unittest.mock import patch
from PIL import Image
from tools import run_noki6210_acceptance as runner


class MenuAcceptanceTest(unittest.TestCase):
    def fixture(self):
        text = '\n'.join(f'sim_device: header cla=a0 ins=b2 p1={i:02x} p2=04 p3=20 selected=6f3a'
                         for i in range(1, 51))
        text += '\n6210_menu_physical: press=1\n6210_keypad_decoded: key=19'
        return text, Image.new('L', (96, 60), 255)

    def test_complete(self):
        text, frame = self.fixture()
        with patch.object(runner, 'MENU_SHA256', hashlib.sha256(frame.tobytes()).hexdigest()):
            runner.check_menu(text, frame)

    def test_incomplete_sim_initialization(self):
        text, frame = self.fixture()
        with self.assertRaisesRegex(ValueError, '50 ADN'):
            runner.check_menu(text.replace('p1=32', 'p1=31'), frame)

    def test_host_press_alone_is_not_decoded_input(self):
        text, frame = self.fixture()
        with self.assertRaises(ValueError):
            runner.check_menu(text.replace('key=19', 'key=5a'), frame)

    def test_wrong_frame_is_not_success(self):
        text, frame = self.fixture()
        with self.assertRaisesRegex(ValueError, 'Messages screen'):
            runner.check_menu(text, frame)


class ApplicationAcceptanceTest(unittest.TestCase):
    def test_host_sms_frame_rejects_blank_or_wrong_geometry(self):
        for size in ((96, 60), (84, 48)):
            with self.assertRaisesRegex(ValueError, 'Message sent'):
                runner.check_host_sms_sent(Image.new('L', size, 255))

    def test_accessory_requires_observed_high_unattached_decision(self):
        frame = Image.new('L', (96, 60), 255)
        with patch.object(runner, 'OPERATOR_SHA256', hashlib.sha256(frame.tobytes()).hexdigest()):
            runner.check_accessory('6210_accessory_decision: state=0f sample=03ff', frame)
            for text in ('', '6210_accessory_decision: state=10 sample=0000',
                         '6210_accessory_decision: state=0f sample=0000'):
                with self.assertRaises(ValueError):
                    runner.check_accessory(text, frame)

    def test_sim_persistence_can_precede_or_follow_radio_release(self):
        records = [
            'TX packet type=56 payload=160 data=0023',
            'TX packet type=02 radio_phase=candidate_channel_change data=040000000000005050000023',
            'TX packet type=0c radio_phase=random_access',
            'RX enqueue type=89 payload=8 data=0100000000000000',
            'TX packet type=1b data=0080013f4905087000f000fffe33080910101032547698',
            'LAPDm Location Updating Accept acknowledged nr=1',
            'LAPDm Channel Release acknowledged nr=2',
            'TX packet type=02 radio_phase=release_channel_change data=040000000000001a600000230000000f']
        updates = ['update-binary fid=6f7e offset=4 length=5',
                   'update-binary fid=6f7e offset=10 length=1']
        storage = bytearray(3524)
        storage[1604:1609] = bytes.fromhex('00f1100001')
        for index in (6, 7, 8):
            runner.check_registration('\n'.join(records[:index] + updates + records[index:]), storage)
        with self.assertRaises(ValueError):
            runner.check_registration('\n'.join(updates + records), storage)

    def test_artifact_failure_cannot_pass(self):
        for error in ('Disk quota exceeded', 'Error writing NVRAM file', 'Error generating PNG'):
            with self.assertRaisesRegex(ValueError, 'acceptance artifacts'):
                runner.check_output(error)
        runner.check_output('Average speed: 450%')

    def test_lua_error_cannot_pass(self):
        with self.assertRaises(ValueError):
            runner.check_output('[LUA ERROR] fixture failed')

    def test_registration_rejects_missing_exchange(self):
        with self.assertRaisesRegex(ValueError, 'ordered NPE-3'):
            runner.check_registration('', bytes(3524))

    def test_calculator_requires_ordered_keys(self):
        frame = Image.new('L', (96, 60), 255)
        actions = ('application', 'input_1', 'input_12', 'operation_options',
                   'subtract', 'minus', 'input_3', 'options', 'result')
        text = '\n'.join('6210_application_physical: action=' + action for action in actions)
        with patch.object(runner, 'CALCULATOR_SHA256', hashlib.sha256(frame.tobytes()).hexdigest()):
            runner.check_calculator(text, frame)
            with self.assertRaisesRegex(ValueError, 'ordered Calculator'):
                runner.check_calculator(text.replace('action=minus', 'action=plus'), frame)

    def test_phonebook_requires_successful_update(self):
        with self.assertRaisesRegex(ValueError, 'ordered SIM save'):
            runner.check_phonebook('6210_phonebook_physical: action=save', '', b'',
                                   Image.new('L', (96, 60), 255))

    def test_phonebook_rejects_cold_write(self):
        write = '\n'.join(('6210_phonebook_physical: action=save',
                            'header cla=a0 ins=dc p1=01 p2=04 p3=20 selected=6f3a',
                            'body ins=dc length=32 selected=6f3a', 'SIM status ins=dc sw=9000'))
        read = '\n'.join(('header cla=a0 ins=b2 p1=01 p2=04 p3=20 selected=6f3a',
                           '6210_phonebook_read_physical: action=contact', 'ins=dc'))
        with self.assertRaisesRegex(ValueError, 'without writing SIM'):
            runner.check_phonebook(write, read, b'', Image.new('L', (96, 60), 255))


if __name__ == '__main__':
    unittest.main()

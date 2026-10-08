from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from PIL import Image
from tools import noki8890_power_check as check


def sample():
    text = '''8890_power_physical: action=shutdown_press
8890_power_physical: action=shutdown_release
ccont_power: event=off t=51.4
ccont_rtc: event=second time=12:00:52 day=0 status=13 mask=50 t=52.0
8890_power_physical: action=restart_press
ccont_power: event=wake cause=02 t=52.02
ccont_power: event=cause_read data=13
ccont_rtc: event=counter_write reg=07 data=00
8890_power_physical: action=restart_release
'''
    for index in range(1, 6):
        text += f'8890_power_security: key=Keypad {index}\n8890_keypad_decoded: key={index:02x}\n'
    return text + '8890_power_security: key=Menu\n8890_keypad_decoded: key=19\n'


def storage():
    value = bytearray(1611)
    value[1604:1609] = bytes.fromhex('00f1100001')
    return value


class PowerTest(unittest.TestCase):
    def check(self, text, data=None):
        with patch.object(check, 'verify_stage') as stage, \
                patch.object(check, 'verify_registration') as registration:
            check.verify(text, storage() if data is None else data)
            self.assertEqual(stage.call_count, 2)
            for call in stage.call_args_list:
                self.assertEqual(call.kwargs, {'runtime': True, 'selftest': True})
            self.assertEqual(registration.call_args_list[0].kwargs, {'configured_gsm900': True})
            self.assertEqual(registration.call_args_list[1].kwargs,
                             {'configured_gsm900': True, 'preserved_location': True})

    def test_two_independently_checked_boots(self):
        self.check(sample())

    def test_wrong_cause_order_rtc_or_input_fails(self):
        for text in (
            sample().replace('cause=02', 'cause=04'),
            sample().replace('cause_read data=13', 'cause_read data=15'),
            sample().replace('event=off t=51.4', 'event=off t=52.01'),
            sample().replace('time=12:00:52', 'time=12:00:00'),
            sample().replace('counter_write reg=07 data=00', 'counter_write reg=07 data=01'),
            sample().replace('action=restart_press', 'action=charger_connect'),
            sample().replace('key=03', 'key=04'),
            sample() + 'ccont_power: event=wake cause=02 t=80.0\n',
            sample() + '[LUA ERROR]\n',
        ):
            with self.subTest(text=text), self.assertRaises(ValueError):
                self.check(text)

    def test_persisted_location_required(self):
        value = storage()
        value[1610] = 1
        with self.assertRaises(ValueError):
            self.check(sample(), value)

    def test_restored_pixels_are_not_blank_or_wrong_geometry(self):
        with tempfile.TemporaryDirectory() as directory:
            frames = Path(directory)
            # Real hashes are exercised by the executable MAME gate.
            with patch.object(check, 'FRAMES', {'frame.png': ((0, 0, 84, 48),
                    '7d9978ed11e23fdb98a9251da90ed9a3c299066e104d69c32f161a9b86d119b9')}):
                Image.new('L', (84, 48), 255).save(frames / 'frame.png')
                check.check_frames(frames)
                for size, color in (((84, 48), 0), ((96, 60), 255)):
                    Image.new('L', size, color).save(frames / 'frame.png')
                    with self.assertRaises(ValueError):
                        check.check_frames(frames)


if __name__ == '__main__':
    unittest.main()

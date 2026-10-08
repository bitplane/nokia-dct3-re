import unittest
from unittest.mock import patch
from tools import noki8210_power_check as check


def sample():
    text = ('update-binary fid=6f7e offset=10 length=1\n'
            '8210_power_physical: action=shutdown_press\n'
            '8210_power_physical: action=shutdown_release\n'
            'ccont_power: event=off t=31.59\n')
    for second in range(32, 44):
        text += f'ccont_rtc: event=second time=12:00:{second} day=1 status=13 t={second}.0\n'
    text += ('8210_power_physical: action=restart_press\n'
             'ccont_power: event=wake cause=02 t=43.02\n'
             'ccont_power: event=cause_read data=13\n'
             'ccont_rtc: event=counter_write reg=07 data=00\n'
             '8210_power_physical: action=restart_release\n'
             'read-binary fid=6f7e offset=0 length=11\n'
             'TX packet type=1b data=0080013f4905087200f110000133080910101032547698\n')
    for index, name in enumerate(('Keypad 1', 'Keypad 2', 'Keypad 3', 'Keypad 4', 'Keypad 5', 'Menu')):
        text += f'8210_power_security: key={name}\n8210_keypad_decoded: key={0x19 if index == 5 else index + 1:02x}\n'
    return text


class PowerTest(unittest.TestCase):
    def verify(self, text):
        with patch.object(check, 'check_host_registration') as boot:
            check.verify(text, b'card')
            self.assertEqual(boot.call_count, 2)
            self.assertTrue(all(call.args[1] == b'card' for call in boot.call_args_list))

    def test_two_independently_checked_boots(self):
        self.verify(sample())

    def test_off_domain_activity_and_clock_discontinuity_rejected(self):
        for marker in ('dspif_transport: RX enqueue', 'dspif_transport: FIQ0 notify',
                       'dspif_transport: peer RAM W', 'rom4_port_write:',
                       'staged_dsp: publication', 'radio_peer: LAPDm', 'dsp_hle: speech'):
            with self.subTest(marker=marker), self.assertRaisesRegex(ValueError, 'activity while off'):
                self.verify(sample().replace('8210_power_physical: action=restart_press',
                                            marker + '\n8210_power_physical: action=restart_press'))
        with self.assertRaisesRegex(ValueError, 'keep ticking'):
            self.verify(sample().replace('time=12:00:35', 'time=12:00:34'))

    def test_cold_lai_status_rewrite_wrong_input_or_observer_failure_rejected(self):
        for old, new in (
                ('cause=02', 'cause=04'),
                ('cause_read data=13', 'cause_read data=15'),
                ('counter_write reg=07 data=00', 'counter_write reg=07 data=01'),
                ('05087200f1100001', '05087000f000fffe'),
                ('8210_keypad_decoded: key=19', '8210_keypad_decoded: key=0d'),
                ('action=restart_press', 'action=charger_connect')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                self.verify(sample().replace(old, new))
        for extra in ('[LUA ERROR] broken\n', 'update-binary fid=6f7e offset=10 length=1\n'):
            with self.subTest(extra=extra), self.assertRaises(ValueError):
                self.verify(sample() + extra)


if __name__ == '__main__':
    unittest.main()

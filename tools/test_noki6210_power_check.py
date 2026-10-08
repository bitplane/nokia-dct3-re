import unittest
from unittest.mock import patch
from tools import noki6210_power_check as check


def sample():
    text = ('6210_power_physical: action=shutdown_press\n'
            '6210_power_physical: action=shutdown_release\n'
            'ccont_power: event=off t=31.68\n')
    for second in range(32, 41):
        text += f'ccont_rtc: event=second time=12:00:{second} day=0 status=11 mask=70 t={second}.0\n'
    text += ('6210_power_physical: action=restart_press\n'
             'ccont_power: event=wake cause=02 t=40.02\n'
             'ccont_power: event=cause_read data=13\n'
             'ccont_rtc: event=counter_write reg=07 data=00\n'
             '6210_power_physical: action=restart_release\n'
             '6210_power_physical: action=menu\n'
             '6210_keypad_decoded: key=19\n')
    return text


class PowerTest(unittest.TestCase):
    def verify(self, text):
        with patch.object(check, 'verify_stage') as stage, \
                patch.object(check, 'check_registration') as registration:
            check.verify(text, b'card')
            self.assertEqual(stage.call_count, 2)
            self.assertTrue(all(call.kwargs == {'runtime': True, 'selftest': True}
                                for call in stage.call_args_list))
            self.assertEqual(registration.call_args_list[0].kwargs, {})
            self.assertEqual(registration.call_args_list[1].kwargs,
                             {'preserved_location': True})

    def test_two_own_boots_and_distinct_registration_checks(self):
        self.verify(sample())

    def test_silent_off_domain_and_continuous_rtc_required(self):
        for marker in ('dspif_transport: RX enqueue', 'dspif_transport: FIQ0 notify',
                       'dspif_transport: peer RAM W', 'rom4_port_write:',
                       'staged_dsp: publication', 'radio_peer: LAPDm', 'dsp_hle: speech'):
            with self.subTest(marker=marker), self.assertRaisesRegex(ValueError, 'generated activity'):
                self.verify(sample().replace('6210_power_physical: action=restart_press',
                                             marker + '\n6210_power_physical: action=restart_press'))
        with self.assertRaisesRegex(ValueError, 'keep ticking'):
            self.verify(sample().replace('time=12:00:35', 'time=12:00:34'))

    def test_wrong_cause_input_order_or_lua_failure_rejected(self):
        for broken in (
                sample().replace('cause=02', 'cause=04'),
                sample().replace('cause_read data=13', 'cause_read data=15'),
                sample().replace('event=off t=31.68', 'event=off t=33.68'),
                sample().replace('counter_write reg=07 data=00', 'counter_write reg=07 data=01'),
                sample().replace('key=19', 'key=0d'),
                sample().replace('action=restart_press', 'action=charger_connect'),
                sample() + '[LUA ERROR] broken\n'):
            with self.subTest(text=broken), self.assertRaises(ValueError):
                self.verify(broken)


if __name__ == '__main__':
    unittest.main()

import unittest
from unittest.mock import patch

from tools.mu4_resident_status_check import STATUS,check
from tools.test_mu4_playback_serial_control_check import response


def fixture(wake):
    lines=[]
    for index in range(6):
        lines.append(response(selector=0x02,token=index).replace('index=0 selector',f'index={index} selector'))
    lines.append('mu4_manual_status_input: request=1 token=04 t=11.200000000 source=bench_serial_command firmware_state_forcing=0')
    lines.append('mu4_playback_control_input: phase=resident_status49 decoded_calls=5 replies=6 t=11.201000000 firmware_state_forcing=0 audio_acceptance=0')
    lines[:0]=['mu4_native_recorder_control_ack: token=02 pin_output=1',
               'mu4_native_recorder_control_ack: token=03 pin_output=1']
    if wake:
        lines.append('mu4_native_recorder_control_ack: token=04 pin_output=1')
        lines.append('mu4_native_recorder_response: index=6 selector=71 words=11 token=02 class=1\n'+
                     ''.join(f'mu4_native_recorder_response_word: index={index} value={word:04x}\n' for index,word in enumerate(STATUS)))
    lines.append('mu4_resident_status_fixture: complete=1 token=04 source=bench_serial_command normal_mcu_policy=unproved firmware_state_forcing=0')
    return '\n'.join(lines)


class ResidentStatusTests(unittest.TestCase):
    def run_check(self,text,wake):
        with patch('tools.mu4_resident_status_check.check_wake') as base:
            result=check(text,'console',wake)
            base.assert_called_once_with(text,'console',wake,resident_status='answered' if wake else 'blocked')
            return result

    def test_both_branches(self):
        self.assertTrue(self.run_check(fixture(True),True)['query_answered'])
        self.assertFalse(self.run_check(fixture(False),False)['query_answered'])

    def test_reply_and_scope_mutations_fail(self):
        original=fixture(True)
        for old,new in [('token=04 pin_output','token=05 pin_output'),
                        ('normal_mcu_policy=unproved','normal_mcu_policy=proved'),
                        ('value=00c2','value=00c3'),('t=11.201000000','t=11.001000000'),
                        ('decoded_calls=5 replies=6','decoded_calls=6 replies=6')]:
            with self.subTest(old=old),self.assertRaises(ValueError):
                self.run_check(original.replace(old,new),True)

    def test_control_cannot_reply(self):
        with self.assertRaises(ValueError):self.run_check(fixture(True),False)


if __name__=='__main__':
    unittest.main()

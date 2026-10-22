"""Pending SMS state/epoch corruption cannot pass a successful host exchange."""
import re
import unittest
from unittest.mock import patch

from tools.nsb7_host_sms_submit_state_check import check
from tools.test_nsb7_outgoing_sms_check import fixture as reply_fixture


def fixture():
    text = re.sub(r't=(\d+)\.', lambda match: f't={int(match[1])+4}.', reply_fixture())
    def decode(match):
        code = {'Left Softkey / Menu':'19', 'Scroll Down':'18', 'Keypad 2':'02'}[match[1]]
        return match[0] + f'\nnsb7_key_decode: key={code} observer_writes=0'
    text = re.sub(r'nsb7_sms_reply_input: key=(.*?) state=1 t=[0-9.]+', decode, text)
    cpu = ','.join(f'{word:08x}' for word in range(37))
    rows = ['gsm_call_adapter: sms request id=1 epoch=1 recipient=5551234 alphabet=gsm7 octets=1']
    for event in ('saved', 'restored'):
        rows.append(f'nsb7_sms_submit_state: event={event} pc=00200000 sp=00110000 ram=12345678 cpu={cpu} t=48.000000000')
    rows += ['state_roundtrip: result=pass scenario=nsb7_sms_submit',
             'gsm_call_adapter: sms request id=1 epoch=2 recipient=5551234 alphabet=gsm7 octets=1',
             'gsm_call_adapter: stale epoch=1 current=2 type=outgoing_sms_decision id=1 result=rejected',
             'gsm_call_adapter: sms decision id=1 outcome=1 result=rejected',
             'gsm_call_adapter: sms decision id=2 outcome=1 result=rejected',
             'gsm_call_adapter: sms decision id=1 outcome=0 result=accepted',
             'gsm_call_adapter: sms state id=1 epoch=2 phase=accepted',
             'gsm_call_adapter: sms decision id=1 outcome=0 result=rejected']
    cp = 'GSM service downlink kind=17 sapi=3 pd=09 message=04'
    text = text.replace(cp, cp+'\n'+'\n'.join(rows))
    text = text.replace('GSM service downlink kind=18 sapi=3 pd=09 message=01',
                        'GSM service downlink kind=18 sapi=3 pd=09 message=01 length=5')
    rr = 'LAPDm service Channel Release acknowledged nr=2'
    text = text.replace(rr, rr+'\ngsm_call_adapter: sms state id=1 epoch=2 phase=ended')
    for key, time, code in [('End', 78, '0f'), ('Left Softkey / Menu', 82, '19'), ('End', 85, '0f')]:
        text += f'\nnsb7_sms_recovery: key={key} state=1 t={time}.000000000\n'
        text += f'nsb7_key_decode: key={code} observer_writes=0\n'
        text += f'nsb7_sms_recovery: key={key} state=0 t={time}.200000000'
    return text+'\nnsb7_sms_recovery: done t=88.000000000'


class PendingSmsStateTest(unittest.TestCase):
    @patch('tools.nsb7_host_sms_submit_state_check.check_sms')
    @patch('tools.nsb7_host_sms_submit_state_check.check_authentication')
    def test_complete_and_storage_contract_is_not_skipped(self, authentication, sms):
        text = fixture()
        check(text, b'initial', b'final')
        sms.assert_called_once_with(text, b'initial', b'final')
        sms.side_effect = ValueError('unexpected card mutation')
        with self.assertRaises(ValueError): check(text, b'initial', b'final')

    @patch('tools.nsb7_host_sms_submit_state_check.check_sms')
    @patch('tools.nsb7_host_sms_submit_state_check.check_authentication')
    def test_architecture_epoch_decisions_and_physical_controls(self, authentication, sms):
        text = fixture()
        for bad in (text.replace('event=restored pc=00200000', 'event=restored pc=00200002'),
                    text.replace(',00000024 t=', ' t='),
                    text.replace('nsb7_sms_submit_state: event=restored', 'missing restored'),
                    text+'\nnsb7_sms_submit_state: event=saved invalid',
                    text.replace('epoch=2 recipient', 'epoch=1 recipient'),
                    text.replace('epoch=2 phase=ended', 'epoch=1 phase=ended'),
                    text.replace('type=outgoing_sms_decision', 'type=decision'),
                    text.replace('sms decision id=1 outcome=1 result=rejected',
                                 'sms decision id=1 outcome=1 result=accepted'),
                    text+'\ngsm_call_adapter: sms decision id=1 outcome=0 result=accepted',
                    text.replace('key=19 observer_writes=0', 'key=18 observer_writes=0'),
                    text.replace('state=1 t=82.', 'state=1 t=83.'),
                    text.replace('done t=88.', 'done t=89.'), text+'\n[LUA ERROR]'):
            with self.subTest(bad=bad[-100:]), self.assertRaises(ValueError):
                check(bad, b'initial', b'final')

    @patch('tools.nsb7_host_sms_submit_state_check.check_sms')
    @patch('tools.nsb7_host_sms_submit_state_check.check_authentication')
    def test_save_after_result_is_not_a_pending_submission(self, authentication, sms):
        text = fixture()
        text = 'GSM service downlink kind=18 sapi=3\n'+text
        with self.assertRaises(ValueError): check(text, b'initial', b'final')

    def test_authentication_required(self):
        with self.assertRaises(ValueError): check(fixture(), b'initial', b'final')


if __name__ == '__main__':
    unittest.main()

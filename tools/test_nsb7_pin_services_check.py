"""Authenticated service storage and sender EF_SMSS acceptance."""
import re
import unittest
from unittest.mock import patch

from tools.nsb7_pin_services_check import check_sms, check_sms_storage, check_authentication, SMSS_OFFSET
from tools.nsb7_pin_check import serialized_profile
from tools.nsb7_pin_registration_check import LOCI
from tools.radio_sms_acceptance_common import FIRST_SMS_DELIVER_BODY, SMS_NVRAM_OFFSET
from tools.test_nsb7_outgoing_sms_check import fixture as submit_fixture


def fixture():
    initial = bytearray(serialized_profile())
    initial[1600:1611] = LOCI
    unread = b'\x03'+FIRST_SMS_DELIVER_BODY
    unread += b'\xff'*(176-len(unread))
    read = b'\x01'+unread[1:]
    final = bytearray(initial)
    final[SMS_NVRAM_OFFSET:SMS_NVRAM_OFFSET+176] = read
    final[SMSS_OFFSET] = 0
    text = re.sub(r't=(\d+)\.', lambda m: f't={int(m[1])+4}.', submit_fixture())
    text = text.replace('outcome=0 status_report=0', 'outcome=0 status_report=0 t=47.760763')
    lines = []
    for second in (23,25,27,29):
        lines += [f'nsb7_sms_input: key=Menu state=1 t={second}.000000000',
                  f'nsb7_sms_input: key=Menu state=0 t={second}.200000000']
    wire = bytes.fromhex('a0dc0104b0')+unread+bytes.fromhex('a0dc0104b0')+read+bytes.fromhex('a0d600000200ff')
    lines += [f'nsb7_sim_tx: index={i} value={byte:02x}' for i, byte in enumerate(wire, 1)]
    lines += ['sim_device: update fid=6f3c record=1 length=176 t=21.490',
              'sim_device: update fid=6f3c record=1 length=176 t=29.452',
              'sim_device: update-binary fid=6f43 offset=0 length=2 t=47.810']
    return text+'\n'+'\n'.join(lines), bytes(initial), bytes(final)


class PinServicesTest(unittest.TestCase):
    def test_failed_submission_preserves_smss(self):
        text,initial,final=fixture()
        final=bytearray(final)
        final[SMSS_OFFSET]=initial[SMSS_OFFSET]
        wire=re.findall(r'nsb7_sim_tx: index=(\d+) value=([0-9a-f]{2})',text)
        removed={index for index,_ in wire[-7:]}
        text='\n'.join(row for row in text.splitlines()
                       if not row.startswith('sim_device: update-binary fid=6f43') and
                       not any(row.startswith(f'nsb7_sim_tx: index={index} ') for index in removed))
        check_sms_storage(text,initial,bytes(final),update_reference=False)
        with self.assertRaises(ValueError):
            check_sms_storage(text+'\nsim_device: update-binary fid=6f43 offset=1 length=1 t=48',
                              initial,bytes(final),update_reference=False)
        final[SMSS_OFFSET]=0
        with self.assertRaises(ValueError):
            check_sms_storage(text,initial,bytes(final),update_reference=False)

    def test_complete_sms_and_smss(self):
        check_sms(*fixture())

    def test_sender_counter_must_follow_submit(self):
        text, initial, final = fixture()
        with self.assertRaises(ValueError):
            check_sms(text.replace('length=2 t=47.810', 'length=2 t=47.000'), initial, final)

    def test_unrelated_card_change_rejected(self):
        text, initial, final = fixture()
        changed = bytearray(final)
        changed[3520] = 0
        with self.assertRaises(ValueError):
            check_sms(text, initial, bytes(changed))

    def test_sender_counter_must_be_written(self):
        text, initial, final = fixture()
        final = bytearray(final)
        final[SMSS_OFFSET] = 255
        with self.assertRaises(ValueError):
            check_sms(text, initial, bytes(final))

    @patch('tools.nsb7_pin_services_check.check_registered')
    def test_scopes_preserve_raw_trace(self, registration):
        text = ('raw startup\nnsb7_radio_tx: index=58 service\n'
                'nsb7_security_input: key=Left Softkey / Menu state=0 t=21.200000000\nlater file writes')
        check_authentication(text)
        registration.assert_called_once_with('raw startup\n', cold=True, require_complete=False,
            authentication_text=text.split('\nlater file writes')[0])

    def test_missing_phone_completion(self):
        with self.assertRaises(ValueError):
            check_authentication('no physical phone-code completion')


if __name__ == '__main__':
    unittest.main()

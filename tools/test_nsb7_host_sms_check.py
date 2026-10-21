"""Host SMS composition preserves the independent protocol/card contracts."""
import unittest
from unittest.mock import patch
from tools.nsb7_host_sms_check import check_incoming,check_outgoing
from tools.nsb7_pin_check import serialized_profile
from tools.nsb7_pin_registration_check import LOCI
from tools.radio_sms_acceptance_common import FIRST_SMS_DELIVER_BODY,SMS_NVRAM_OFFSET


class HostSmsTest(unittest.TestCase):
    def initial(self):
        card=bytearray(serialized_profile())
        card[1600:1611]=LOCI
        return bytes(card)

    def test_incoming_composes_exact_read_contract(self):
        text='raw auth\nGSM service downlink kind=16 sapi=3 pd=09 message=01 length=36\nnsb7_sms_input: marker'
        initial=self.initial()
        with patch('tools.nsb7_host_sms_check.check_authentication') as auth, patch(
                'tools.nsb7_host_sms_check.incoming_host') as host, patch(
                'tools.nsb7_host_sms_check.check_read') as read:
            check_incoming(text,initial,b'actual final card')
            auth.assert_called_once_with(text.split('nsb7_sms_input:',1)[0])
            host.assert_called_once_with(text,arfcn=600)
            delivered=bytearray(initial)
            record=b'\x03'+FIRST_SMS_DELIVER_BODY
            delivered[SMS_NVRAM_OFFSET:SMS_NVRAM_OFFSET+176]=record+b'\xff'*(176-len(record))
            read.assert_called_once_with(text,b'actual final card',bytes(delivered),read_times=(23,25,27,29))
            with self.assertRaises(ValueError):
                check_incoming(text+text,initial,b'actual final card')

    def test_outgoing_keeps_host_and_card_checks(self):
        with patch('tools.nsb7_host_sms_check.check_sms') as card, patch(
                'tools.nsb7_host_sms_check.outgoing_host') as host:
            check_outgoing('raw trace',b'initial',b'final')
            card.assert_called_once_with('raw trace',b'initial',b'final')
            host.assert_called_once_with('raw trace',octets=1)

    def test_no_authentication_is_not_host_acceptance(self):
        with self.assertRaises(ValueError):
            check_incoming('host delivered',self.initial(),self.initial())


if __name__=='__main__':
    unittest.main()

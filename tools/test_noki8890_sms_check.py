import unittest

from tools.noki8890_incoming_sms_check import verify as verify_incoming
from tools.noki8890_outgoing_sms_check import verify as verify_outgoing
from tools.test_noki8850_sms_check import FRESH, storage
from tools.test_noki8850_outgoing_sms_check import GOOD
from tools.test_noki8890_registration_check import PCS_LOG

INCOMING = FRESH.replace('0080ffffffff', '0076ffffffff').replace(
    '8850_sms_physical: action=read_4', '8890_sms_physical: action=read_2').replace(
    '8850_keypad_decoded key=19', '8890_keypad_decoded: key=19')
OUTGOING = GOOD.replace('8850_', '8890_').replace(
    '8890_keypad_decoded key=', '8890_keypad_decoded: key=')


class Nokia8890SmsTest(unittest.TestCase):
    def test_incoming(self):
        verify_incoming(INCOMING, storage())

    def test_wrong_cipher_contract(self):
        with self.assertRaises(ValueError):
            verify_incoming(INCOMING.replace('0076ffffffff', '0080ffffffff'), storage())

    def test_duplicate_delivery(self):
        with self.assertRaises(ValueError):
            verify_incoming(INCOMING + '\nPCH IMSI page transmitted channel=60', storage())

    def test_deleted_record(self):
        with self.assertRaises(ValueError):
            verify_incoming(INCOMING, bytes(len(storage())))

    def test_outgoing(self):
        verify_outgoing(OUTGOING)

    def test_pcs_incoming(self):
        verify_incoming(PCS_LOG + '\n' + INCOMING, storage(), pcs1900=True)

    def test_pcs_outgoing(self):
        verify_outgoing(PCS_LOG + '\n' + OUTGOING, pcs1900=True)

    def test_pcs_sms_rejects_unproved_band(self):
        with self.assertRaisesRegex(ValueError, 'candidate window'):
            verify_outgoing(OUTGOING, pcs1900=True)

    def test_wrong_destination(self):
        with self.assertRaises(ValueError):
            verify_outgoing(OUTGOING.replace('destination=5551234', 'destination=5512345'))

    def test_missing_network_ack(self):
        with self.assertRaises(ValueError):
            verify_outgoing(OUTGOING.replace('kind=18', 'kind=19'))


if __name__ == '__main__':
    unittest.main()

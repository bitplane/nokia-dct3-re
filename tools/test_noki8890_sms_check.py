import unittest

from tools.noki8890_incoming_sms_check import verify as verify_incoming
from tools.noki8890_outgoing_sms_check import verify as verify_outgoing
from tools.noki8890_outgoing_sms_check import check_recovery
from tools.noki8890_outgoing_sms_check import verify_silence
from pathlib import Path
from tempfile import TemporaryDirectory
from PIL import Image
from tools.test_noki8850_sms_check import FRESH, storage
from tools.test_noki8850_outgoing_sms_check import GOOD
from tools.test_noki8890_registration_check import PCS_LOG

INCOMING = FRESH.replace('0080ffffffff', '0076ffffffff').replace(
    '8850_sms_physical: action=read_4', '8890_sms_physical: action=read_2').replace(
    '8850_keypad_decoded key=19', '8890_keypad_decoded: key=19')
OUTGOING = GOOD.replace('8850_', '8890_').replace(
    '8890_keypad_decoded key=', '8890_keypad_decoded: key=')
SILENCE = '''8890_sms_send_physical: action=confirm_send
GSM service uplink sapi=3 pd=09 message=01 length=27 data=390118000100069121436587090d11010781551532f40000a70141
gsm_sms_submit:
GSM service downlink kind=17 sapi=3 pd=09 message=04 length=2
gsm_call_adapter: sms decision id=1 outcome=3 result=accepted
TX packet type=1b data=0080015301
RX enqueue type=80 data=800000005b0100010000017301
TX packet type=02 radio_phase=release_channel_change
gsm_call_adapter: sms state id=1 epoch=1 phase=ended
PCH no-identity fill
'''


class Nokia8890SmsTest(unittest.TestCase):
    def test_silence_requires_mobile_release(self):
        verify_silence(SILENCE)
        with self.assertRaisesRegex(ValueError, 'DISC'):
            verify_silence(SILENCE.replace('0080015301', '0080010301'))

    def test_silence_rejects_rp_result(self):
        with self.assertRaisesRegex(ValueError, 'RP result'):
            verify_silence(SILENCE + 'GSM service downlink kind=18 sapi=3')

    def test_recovery_requires_physical_navigation(self):
        with self.assertRaisesRegex(ValueError, 'physical recovery'):
            check_recovery(OUTGOING, Path('missing'))

    def test_recovery_rejects_blank_presentation(self):
        text = ('LAPDm service Channel Release acknowledged\n'
                '8890_sms_recovery_physical: key=End\n'
                '8890_keypad_decoded: key=0f\n'
                '8890_sms_recovery_physical: key=Menu\n'
                '8890_keypad_decoded: key=19\n')
        with TemporaryDirectory() as directory:
            frames = Path(directory)
            Image.new('L', (84, 48)).save(frames / '8890_sms_reject_1.png')
            with self.assertRaisesRegex(ValueError, 'message-not-sent'):
                check_recovery(text, frames)

    def test_pcs_rejection_requires_own_registration(self):
        rejected = OUTGOING.replace('kind=18', 'kind=19').replace(
            'message=01 length=5', 'message=01 length=7')
        verify_outgoing(PCS_LOG + '\n' + rejected, rejected=True, pcs1900=True)
        with self.assertRaisesRegex(ValueError, 'candidate window'):
            verify_outgoing(rejected, rejected=True, pcs1900=True)

    def test_rejected_outgoing(self):
        verify_outgoing(OUTGOING.replace('kind=18', 'kind=19').replace(
            'message=01 length=5', 'message=01 length=7'), rejected=True)

    def test_rejected_cannot_contain_success(self):
        rejected = OUTGOING.replace('kind=18', 'kind=19').replace(
            'message=01 length=5', 'message=01 length=7')
        with self.assertRaisesRegex(ValueError, 'success RP-ACK'):
            verify_outgoing(rejected + '\nGSM service downlink kind=18 sapi=3', rejected=True)

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

import unittest
from pathlib import Path
import tempfile
from PIL import Image
from tools.noki8850_outgoing_sms_check import SUBMIT, verify, check_recovery

GOOD = '\n'.join((
    '8850_sms_send_physical: action=text_A',
    '8850_sms_send_physical: action=confirm_send',
    '8850_keypad_decoded key=19',
    f'GSM service uplink sapi=3 pd=09 message=01 length=27 data={SUBMIT}',
    'gsm_sms_submit: cp=39 rp=01 smsc=1234567890 destination=5551234 alphabet=0 user_length=1 outcome=0 status_report=0',
    'GSM service downlink kind=17 sapi=3 pd=09 message=04 length=2',
    'GSM service downlink kind=18 sapi=3 pd=09 message=01 length=5',
    'GSM service uplink sapi=3 pd=09 message=04 length=2 data=3904',
    'LAPDm service Channel Release acknowledged',
    'PCH no-identity fill',
))


class OutgoingSmsTest(unittest.TestCase):
    def test_complete(self):
        verify(GOOD)

    def test_wrong_text(self):
        with self.assertRaises(ValueError):
            verify(GOOD.replace(SUBMIT, SUBMIT[:-2] + '42'))

    def test_missing_rp_ack(self):
        with self.assertRaises(ValueError):
            verify(GOOD.replace('kind=18', 'kind=19'))

    def test_wrong_key(self):
        with self.assertRaises(ValueError):
            verify(GOOD.replace('key=19', 'key=18'))

    def test_missing_paging(self):
        with self.assertRaises(ValueError):
            verify(GOOD.replace('PCH no-identity fill', ''))

    def test_rejection_requires_rp_error(self):
        rejected = GOOD.replace('kind=18', 'kind=19').replace('length=5', 'length=7')
        verify(rejected, rejected=True)
        with self.assertRaises(ValueError):
            verify(GOOD, rejected=True)

    def test_rejection_forbids_success_result(self):
        rejected = GOOD.replace('kind=18', 'kind=19').replace('length=5', 'length=7')
        with self.assertRaisesRegex(ValueError, 'success RP-ACK'):
            verify(rejected + '\nGSM service downlink kind=18 sapi=3', rejected=True)

    def test_recovery_requires_physical_navigation(self):
        with self.assertRaises(ValueError):
            check_recovery(GOOD, Path('unused'))

    def test_recovery_rejects_blank_error_frame(self):
        text = GOOD + '\n' + '\n'.join((
            '8850_sms_recovery_physical: key=End',
            '8850_keypad_decoded key=0f',
            '8850_sms_recovery_physical: key=Menu',
            '8850_keypad_decoded key=19',
        ))
        with tempfile.TemporaryDirectory() as directory:
            frames = Path(directory)
            Image.new('L', (84, 48)).save(frames / '8850_sms_reject_1.png')
            with self.assertRaisesRegex(ValueError, 'message-not-sent'):
                check_recovery(text, frames)

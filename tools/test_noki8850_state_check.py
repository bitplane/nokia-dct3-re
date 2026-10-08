import unittest
from pathlib import Path
import tempfile
from unittest.mock import patch
from PIL import Image
from tools.noki8850_state_check import verify, check_frames


GOOD = '''8850_state: event=saved pc=0000001c sp=00137000 ram=12345678 t=40.000000000
state_replay: phase=reference event=begin t=40.000000000
RX enqueue type=80 payload=34 data=1234 t=40.500000000
state_replay: phase=reference event=end t=41.000000000
8850_state: event=restored pc=0000001c sp=00137000 ram=12345678 t=40.000000000
state_roundtrip: result=pass scenario=8850_call requested_at=40.000000000 t=40.000000000
state_replay: phase=restored event=begin t=40.000000000
RX enqueue type=80 payload=34 data=1234 t=40.500000000
state_replay: phase=restored event=end t=41.000000000
8850_call_physical: action=end
'''


class StateTest(unittest.TestCase):
    def test_sip_idle_requires_exact_replay_and_physical_exit(self):
        text = GOOD + '8850_sip_cancel: physical Exit\n8850_keypad_decoded key=1a\n'
        verify(text, idle=True, sip_cancel=True)
        with self.assertRaisesRegex(ValueError, 'dismissal'):
            verify(text.replace('key=1a', 'key=19'), idle=True, sip_cancel=True)
        with self.assertRaisesRegex(ValueError, 'idle restoration'):
            verify(text, sip_cancel=True)
        with self.assertRaisesRegex(ValueError, 'diverged'):
            verify(text.replace('data=1234', 'data=5678', 1), idle=True, sip_cancel=True)
        with self.assertRaisesRegex(ValueError, 'initiated a call'):
            verify(text + '8850_call_physical: action=send', idle=True, sip_cancel=True)

    def test_sip_reference_and_restored_pixels_must_match(self):
        with tempfile.TemporaryDirectory() as directory:
            frames = Path(directory)
            reference = Image.new('L', (84, 48), 255)
            reference.save(frames / '8850_state_call_reference.png')
            reference.save(frames / '8850_state_call_restored.png')
            with patch('tools.noki8850_state_check.hashlib.sha256') as digest:
                digest.return_value.hexdigest.return_value = '59b772b8dd4715490911ec43c4969b76a4cb57708473d2345b31f8e22fd77b7b'
                check_frames(frames, idle=True, sip_cancel=True)
                reference.putpixel((0, 0), 0)
                reference.save(frames / '8850_state_call_restored.png')
                with self.assertRaisesRegex(ValueError, 'pixels'):
                    check_frames(frames, idle=True, sip_cancel=True)

    def test_idle_requires_postload_menu(self):
        idle = GOOD + '8850_state_physical: key=Menu\n8850_keypad_decoded key=19\n'
        verify(idle, idle=True)
        with self.assertRaisesRegex(ValueError, 'Menu input'):
            verify(idle.replace('key=19', 'key=0f'), idle=True)

    def test_idle_forbids_call_start(self):
        idle = GOOD + '8850_state_physical: key=Menu\n8850_keypad_decoded key=19\n'
        with self.assertRaisesRegex(ValueError, 'initiated a call'):
            verify(idle + '8850_call_physical: action=send', idle=True)

    def test_sms_continuation_forbids_redelivery(self):
        text = GOOD + 'PCH IMSI page transmitted channel=60\n'
        text += ('sim_device: update fid=6f3c record=1 length=176\n' * 2)
        text += '8850_sms_physical: action=read_4\n'
        with patch('tools.noki8850_state_check.verify_sms'):
            verify(text, sms=True, storage=b'fixture')
            with self.assertRaisesRegex(ValueError, 'redelivered'):
                verify(text + 'PCH IMSI page transmitted channel=60',
                       sms=True, storage=b'fixture')

    def test_sms_requires_persistent_storage(self):
        with self.assertRaisesRegex(ValueError, 'persistent SIM storage'):
            verify(GOOD, sms=True)

    def check(self, text):
        # The product-specific call grammar has its own executable checker tests.
        with patch('tools.noki8850_state_check.verify_call') as call:
            verify(text)
            call.assert_called_once_with(text)

    def test_exact_replay(self):
        self.check(GOOD)

    def test_architectural_mismatch(self):
        with self.assertRaisesRegex(ValueError, 'exactly'):
            self.check(GOOD.replace('event=restored pc=0000001c', 'event=restored pc=00000020'))

    def test_protocol_mismatch(self):
        with self.assertRaisesRegex(ValueError, 'diverged'):
            self.check(GOOD.replace('data=1234', 'data=5678', 1))

    def test_missing_postload_input(self):
        with self.assertRaisesRegex(ValueError, 'post-load'):
            self.check(GOOD.replace('8850_call_physical: action=end', ''))

    def test_incomplete(self):
        with self.assertRaisesRegex(ValueError, 'complete'):
            self.check(GOOD + '8850_state: FAIL incomplete')

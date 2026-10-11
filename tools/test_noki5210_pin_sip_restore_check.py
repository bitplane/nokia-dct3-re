import hashlib
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from PIL import Image
from tools import noki5210_pin_sip_restore_check as checker


def trace():
    return '\n'.join([
        'LAPDm Location Updating Accept acknowledged nr=1',
        *('5210_pin_physical: key=Keypad ' + digit for digit in '5551234'),
        '5210_pin_physical: key=Send',
        'gsm_call_adapter: state id=1 epoch=1 phase=connected',
        '5210_pin_state: event=saved pc=0000001c sp=0013f5d8 ram=12345678 t=42.000000',
        '5210_pin_state: event=restored pc=0000001c sp=0013f5d8 ram=12345678 t=42.000000',
        '5210_pin_state: result=pass', '5210_pin_state_physical: key=End',
        '5210_pin_state: completed=true',
    ])


class SipRecoveryCheckTest(unittest.TestCase):
    def exercise(self, text=None, bad_frame=False, incoming=False):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / 'snap').mkdir()
            (root / 'nvram/noki5210_3').mkdir(parents=True)
            (root / 'nvram/noki5210_3/sim_card').write_bytes(b'own card')
            observed = trace() if text is None else text
            if incoming:
                observed = observed.replace('gsm_call_adapter: state', 'gsm_call_adapter: incoming state')
            (root / 'error.log').write_text(observed)
            frame = Image.new('L', (84, 48), 255)
            digest = hashlib.sha256(frame.tobytes()).hexdigest()
            frame.save(root / 'snap/5210_pin_entered.png')
            if bad_frame:
                frame.putpixel((0, 0), 0)
            frame.save(root / 'snap/5210_pin_recovered.png')
            with patch.object(checker, 'verify_coherent_late') as authentication, \
                    patch.object(checker, 'FRAMES', dict(entered=digest, recovered=digest)):
                checker.verify(root, incoming=incoming)
                authentication.assert_called_once_with(observed, b'own card')

    def test_authentication_architecture_physical_input_and_recovery(self):
        self.exercise()

    def test_incoming_requires_own_connected_state_and_physical_answer(self):
        self.exercise(incoming=True)
        for wrong in (trace().replace('key=Send', 'key=End'),
                      trace().replace('phase=connected', 'phase=alerting')):
            with self.subTest(wrong=wrong), self.assertRaises(ValueError):
                self.exercise(wrong, incoming=True)

    def test_changed_architecture_wrong_digits_and_missing_end_fail(self):
        for wrong in (trace().replace('event=restored pc=0000001c', 'event=restored pc=00000020'),
                      trace().replace('key=Keypad 5', 'key=Keypad 4'),
                      trace().replace('5210_pin_state_physical: key=End', '')):
            with self.subTest(wrong=wrong), self.assertRaises(ValueError):
                self.exercise(wrong)

    def test_wrong_recovery_pixels_fail(self):
        with self.assertRaisesRegex(ValueError, 'pixels'):
            self.exercise(bad_frame=True)

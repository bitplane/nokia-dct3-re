"""Executable negative checks for the authenticated physical USSD gate."""
import hashlib
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from PIL import Image
from tools import noki5210_pin_ussd_check as checker

LINES = [
    'SIM status ins=20 sw=9000',
    'LAPDm Location Updating Accept acknowledged nr=1',
    *('5210_pin_physical: key=' + key for key in
      ('Keypad *', 'Keypad 1', 'Keypad 2', 'Keypad 3', 'Keypad #', 'Send')),
    'GSM service uplink sapi=0 pd=0b message=3b length=27 '
    'data=1b7b1c14a11202010102013b300a04010f0405aa986c36027f0100 t=1',
    'gsm_ss: request=ussd transaction=1b invoke=1 dcs=0f packed_length=5 outcome=0 t=1',
    'GSM service downlink kind=27 sapi=0 pd=0b message=2a length=37 t=1',
    'radio_phase=release_channel_change',
    '5210_pin_physical: key=End',
]


class PinUssdTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.run = Path(self.temporary.name)
        (self.run / 'snap').mkdir()
        card = self.run / 'nvram/noki5210_3/sim_card'
        card.parent.mkdir(parents=True)
        card.write_bytes(b'fixture card; authentication is independently mocked')
        self.frames = {}
        for index, phase in enumerate(('entered', 'idle', 'recovered', 'ussd')):
            image = Image.new('L', (84, 48), index * 40)
            image.save(self.run / f'snap/5210_pin_{phase}.png')
            pixels = image.tobytes()
            data = b'P5\n84 48\n255\n' + pixels if phase == 'ussd' else pixels
            self.frames[phase] = hashlib.sha256(data).hexdigest()
        for mock in (patch.object(checker, 'verify_coherent_late'),
                     patch.object(checker, 'FRAMES', self.frames),
                     patch.object(checker, 'USSD_FRAME', self.frames['ussd'])):
            mock.start()
            self.addCleanup(mock.stop)

    def verify(self, lines=LINES):
        (self.run / 'error.log').write_text('\n'.join(lines))
        checker.verify(self.run)

    def test_complete_physical_protocol_and_pixels(self):
        self.verify()

    def test_every_checkpoint_is_required(self):
        for index in range(len(LINES)):
            with self.subTest(index=index), self.assertRaises(ValueError):
                self.verify(LINES[:index] + LINES[index + 1:])

    def test_release_after_physical_end_does_not_pass(self):
        with self.assertRaises(ValueError):
            self.verify(LINES[:-2] + [LINES[-1], LINES[-2]])

    def test_wrong_result_pixels_fail(self):
        Image.new('L', (84, 48), 0).save(self.run / 'snap/5210_pin_ussd.png')
        with self.assertRaisesRegex(ValueError, 'USSD pixels'):
            self.verify()

    def test_wrong_geometry_fails(self):
        Image.new('L', (102, 72), 0).save(self.run / 'snap/5210_pin_ussd.png')
        with self.assertRaisesRegex(ValueError, 'geometry'):
            self.verify()

    def test_coherent_authentication_validation_is_required(self):
        with patch.object(checker, 'verify_coherent_late', side_effect=ValueError('PIN missing')):
            with self.assertRaisesRegex(ValueError, 'PIN missing'):
                self.verify()


if __name__ == '__main__':
    unittest.main()

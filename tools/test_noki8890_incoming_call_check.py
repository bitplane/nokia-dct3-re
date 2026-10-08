import unittest
import tempfile
from pathlib import Path
from PIL import Image
from tools.noki8890_incoming_call_check import check_host_frames, host_setup_pattern, verify


class IncomingTest(unittest.TestCase):
    def test_blank_host_frame(self):
        with tempfile.TemporaryDirectory() as directory:
            Image.new('L', (84, 48), 255).save(Path(directory) / '8890_incoming_ringing.png')
            with self.assertRaisesRegex(ValueError, 'presentation mismatch'):
                check_host_frames(Path(directory), '447700900123')

    def test_unreviewed_frame_caller(self):
        with self.assertRaisesRegex(ValueError, 'reviewed host frames'):
            check_host_frames(Path('.'), '123')

    def test_host_caller_exact_wire(self):
        packet = ('RX enqueue type=80 payload=34 producer=093 '
                  'data=8000000023300001000003244d030504046002008134015c07814477000910322b2b')
        self.assertIsNotNone(host_setup_pattern('447700900123').search(packet))
        self.assertIsNone(host_setup_pattern('447700900124').search(packet))
        self.assertIsNone(host_setup_pattern('447700900123').search(packet.replace('4d03', '4903')))

    def test_host_caller_invalid(self):
        for caller in ('', '+123', '12a', '1' * 21):
            with self.assertRaisesRegex(ValueError, 'decimal digits'):
                host_setup_pattern(caller)

    def test_requires_registration(self):
        with self.assertRaisesRegex(ValueError, 'registration release'):
            verify('')

    def test_rejects_fixture_error(self):
        with self.assertRaisesRegex(ValueError, 'fixture error'):
            verify('[LUA ERROR]')

    def test_pcs_requires_pcs_registration(self):
        with self.assertRaisesRegex(ValueError, 'candidate window'):
            verify('', pcs1900=True)

    def test_configured_requires_registration(self):
        with self.assertRaisesRegex(ValueError, 'candidate window'):
            verify('', configured_gsm900=True)

    def test_distinct_band_compositions(self):
        with self.assertRaisesRegex(ValueError, 'distinct compositions'):
            verify('', pcs1900=True, configured_gsm900=True)

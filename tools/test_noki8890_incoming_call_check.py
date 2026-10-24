import unittest
import tempfile
from pathlib import Path
from PIL import Image
from tools.noki8890_incoming_call_check import (
    check_host_frames, host_setup_pattern, verify, CIPHER_CHECKPOINTS)
from tools.radio_call_lifecycle_common import require_ordered


class IncomingTest(unittest.TestCase):
    def test_mm_information_follows_consumed_cipher_completion(self):
        command = 'RX enqueue type=80 payload=34 data=80' + '00' * 9 + '03000d063500\n'
        complete = 'GSM service uplink sapi=0 pd=06 message=32 length=2 data=0632\n'
        information = 'RX enqueue type=80 payload=34 data=80' + '00' * 9 + '03002905324762704221000000\n'
        require_ordered(command + complete + information, CIPHER_CHECKPOINTS, 'NSB-6 cipher')
        for invalid in (command + information + complete, command + information,
                        complete + information, command + complete):
            with self.subTest(invalid=invalid), self.assertRaises(ValueError):
                require_ordered(invalid, CIPHER_CHECKPOINTS, 'NSB-6 cipher')

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

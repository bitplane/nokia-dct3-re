import hashlib
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from PIL import Image

from tools.nam2_c3_checkpoint_check import check_boot, check_pmm, check_frames


class Nam2C3CheckpointTest(unittest.TestCase):
    def test_frame_oracle_rejects_changed_pixels_and_geometry(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            image = Image.new('RGB', (96, 65), 'white')
            image.save(directory / 'frame.png')
            with patch('tools.nam2_c3_checkpoint_check.FRAMES', {
                    'frame.png': hashlib.sha256(image.tobytes()).hexdigest()}):
                check_frames(directory)
                for size, color in [((96, 65), 'black'), ((84, 48), 'white')]:
                    Image.new('RGB', size, color).save(directory / 'frame.png')
                    with self.assertRaisesRegex(ValueError, 'frame differs'):
                        check_frames(directory)

    def trace(self):
        return ('flash_family: family=00000003 id0=0000 maker=0089 device=88c2 third=0001\n'
                'nam2_v521_checksum: computed=0000a0aa stored=0000a0aa\n'
                'nam2_v521_release: result=00000001\n')

    def test_boot_requires_actual_c3_checksum_and_release(self):
        check_boot(self.trace())
        for text in [self.trace().replace('family=00000003', 'family=00000001'),
                     self.trace().replace('computed=0000a0aa', 'computed=000098b2'),
                     self.trace().replace('result=00000001', 'result=00000000'),
                     self.trace() + '[LUA ERROR]']:
            with self.assertRaises(ValueError):
                check_boot(text)

    def test_storage_requires_own_catalogue_and_valid_checksum(self):
        source = bytes(0x200000)
        flash = bytearray(source)
        flash[0x1fc006:0x1fc00c] = b'EEPROM'
        with patch('tools.noki2100_v521_security_check.V521_SHA1', hashlib.sha1(source).hexdigest()):
            check_pmm(source, flash)
            otp = bytes.fromhex('fffe' + 'ffff' * 8)
            check_pmm(source, flash + otp)
            with self.assertRaisesRegex(ValueError, 'factory-lock'):
                check_pmm(source, flash + bytes([0xff]) * 18)
            with self.assertRaisesRegex(ValueError, 'executable'):
                check_pmm(source, flash + otp[:-1])
            flash[0x1fc030] = 1
            with self.assertRaisesRegex(ValueError, 'checksum'):
                check_pmm(source, flash)
            flash[0x1fc006] = 0
            with self.assertRaisesRegex(ValueError, 'catalogue'):
                check_pmm(source, flash)

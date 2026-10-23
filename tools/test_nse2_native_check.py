import hashlib
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.nse2_native_check import verify_native
from tools.run_nse2_native import prepare_native
from tools.test_nse2_boot_check import transcript as boot_transcript
from tools.test_nse2_upload_check import transcript as upload_transcript


def native_transcript():
    upload, payload = upload_transcript()
    upload = upload[:upload.index('nse2_startup_poll:')]
    upload += ('nse2_startup_poll: n=1 address=00010004 data=0004 mask=ffff '
               'pc=002781fa t=0.003000 observer_writes=0\n')
    text = '\n'.join(row for row in boot_transcript().splitlines()
                     if not row.startswith('nse2_state_byte:')) + '\n' + upload
    text += ('nse2_native_identity: n=1 data=0004 mask=ffff pc=0f0f '
             't=0.002900 observer_writes=0\n'
             'nse2_state_byte: address=0010b9f4 value=01 pc=002787e2 '
             't=0.250 observer_writes=0\n')
    text += f'nse2_native_verifier_image: words=186 body={payload[:372].hex()} observer_writes=0\n'
    for second in range(1, 9):
        text += (f'nse2_native_state: t={second}.0 pc=408d illegal=0 idle=1 observer_writes=0\n')
    # The synthetic MCU transcript has seven samples; the native horizon is eight.
    text += next(row for row in text.splitlines() if row.startswith('nse2_state: t=7.')).replace('t=7.', 't=8.') + '\n'
    return text, payload


class NativeTest(unittest.TestCase):
    def check(self, text, payload, **kwargs):
        return verify_native(text, payload, bytes([255]) * 4032, (84, 48), **kwargs)

    def test_native_verifier_and_readiness_are_not_usable_boot(self):
        result = self.check(*native_transcript())
        self.assertTrue(result['native_verifier_executed'])
        self.assertTrue(result['organic_ready_state'])
        self.assertTrue(result['frame_blank'])
        for field in ('usable_boot', 'graphical_boot', 'physical_key_delivery', 'fitted_mask_proved'):
            self.assertFalse(result[field])

    def test_native_evidence_cannot_be_replaced_by_a_ready_flag(self):
        text, payload = native_transcript()
        for old, new in (('pc=0f0f', 'pc=ffff'), ('data=0004 mask=ffff', 'data=0004 mask=00ff'),
                         ('words=186 body=00000001', 'words=186 body=00010001'),
                         ('t=0.002900', 't=0.004000'), ('illegal=0', 'illegal=1'),
                         ('pc=002787e2', 'pc=002782d8'), ('t=8.0 pc=408d', 't=7.0 pc=408d')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                self.check(text.replace(old, new), payload)

    def test_reset_hle_geometry_and_missing_frames_rejected(self):
        text, payload = native_transcript()
        for extra in ('Soft reset\n', 'dsp_hle: bootstrap complete'):
            with self.assertRaises(ValueError):
                self.check(text + extra, payload)
        with self.assertRaises(ValueError):
            verify_native(text, payload, b'', (84, 48))

    def test_preparation_copies_only_explicit_masks_and_own_inputs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            masks = root / 'roms/noki5110'
            masks.mkdir(parents=True)
            specs = []
            for name, data in (('program.bin', b'program'), ('data.bin', b'data')):
                (masks / name).write_bytes(data)
                specs.append((name, len(data), hashlib.sha1(data).hexdigest()))
            packages = root / 'roms/archive-dct3-packages'
            packages.mkdir()
            for suffix in ('zip', 'exe'):
                (packages / f'nse2_553.{suffix}').write_bytes(b'package')
            run = root / 'run'
            with patch('tools.run_nse2_native.MASK_INPUTS', specs), \
                    patch('tools.run_nse2_boundary.inspect', side_effect=[
                        (b'own flash', {'archive_sha256': 'zip'}),
                        (b'own flash', {'archive_sha256': 'exe'})]):
                report = prepare_native(root, run, record_checksum=True)
            self.assertFalse(report['fitted_mask_proved'])
            self.assertFalse(report['authentic_provisioning'])
            self.assertEqual((run / 'roms/nse2r4t/5110i_5.53A.fls').read_bytes(), b'own flash')
            self.assertEqual({path.name for path in (run / 'roms/nse2r4t').iterdir()},
                             {'5110i_5.53A.fls', 'program.bin', 'data.bin'})
            self.assertEqual({path.name for path in (run / 'nvram/nse2r4t').iterdir()}, {'eeprom'})

    def test_wrong_mask_rejected_before_preparing_any_run(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / 'roms/noki5110'
            path.mkdir(parents=True)
            (path / 'program.bin').write_bytes(b'wrong')
            with patch('tools.run_nse2_native.MASK_INPUTS', [('program.bin', 1, 'bad')]), \
                    patch('tools.run_nse2_native.prepare') as prepare:
                with self.assertRaisesRegex(ValueError, 'mask differs'):
                    prepare_native(root, root / 'run')
                prepare.assert_not_called()
            self.assertFalse((root / 'run').exists())


if __name__ == '__main__':
    unittest.main()

import hashlib
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools import nse2_boot_check as check
from tools.run_nse2_boundary import prepare


def transcript():
    text = 'Soft reset\n'
    writes = [(0x10000 + i * 2, value, 0x27dca8)
              for i, value in enumerate((0xea, 0xff, 0xff, 0xea))]
    writes += [(0x10000, 0, 0x27817a), (0x10002, 0xffff, 0x278182),
               (0x10004, 0xffff, 0x278184), (0x10006, 0xffff, 0x278188)]
    for address, value, pc in writes:
        text += f'nse2_shared: address={address:08x} data={value:04x} mask=ffff pc={pc:08x} t=0.01 observer_writes=0\n'
    for second in range(1, 8):
        text += (f'nse2_state: t={second}.000000000 pc=0028db3a cpsr=2000003f sp=0010c928 '
                 'shared_writes=8 gensio_tx=200 gensio_rx=80 lcd_commands=104 '
                 'lcd_bytes=3024 eeprom_signal_writes=10000 observer_writes=0\n')
    return text


class BoundaryTest(unittest.TestCase):
    def verify(self, text):
        pixels = bytes(84 * 48)
        with patch.object(check, 'FRAME', hashlib.sha256(pixels).hexdigest()):
            return check.verify(text, pixels, (84, 48))

    def test_failure_presentation_does_not_promote_boot(self):
        result = self.verify(transcript())
        self.assertTrue(result['graphical_failure_frame'])
        for field in ('usable_boot', 'native_dsp', 'physical_key_delivery'):
            self.assertFalse(result[field])

    def test_peer_completion_is_not_this_boundary(self):
        for event in ('dspif_transport: RX enqueue type=80',
                      'dspif_transport: peer RAM W off=002 data=0001',
                      'dsp_hle: bootstrap complete'):
            with self.subTest(event=event), self.assertRaisesRegex(ValueError, 'peer completion'):
                self.verify(transcript() + event)

    def test_shared_sequence_and_horizon_required(self):
        for old, new in (('data=00ea', 'data=00eb'),
                         ('mask=ffff', 'mask=00ff'),
                         ('shared_writes=8', 'shared_writes=9'),
                         ('t=7.000000000', 't=6.000000000'),
                         ('pc=0028db3a', 'pc=00000000')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                self.verify(transcript().replace(old, new))

    def test_missing_bus_activity_or_reset_loop_rejected(self):
        for field, value in (('gensio_rx', '80'), ('lcd_bytes', '3024'),
                             ('eeprom_signal_writes', '10000')):
            with self.subTest(field=field), self.assertRaises(ValueError):
                self.verify(transcript().replace(f'{field}={value}', f'{field}=0'))
        with self.assertRaisesRegex(ValueError, 'reset repeated'):
            self.verify(transcript() + 'Soft reset\n')

    def test_blank_or_wrong_geometry_is_not_reviewed_frame(self):
        for size in ((84, 48), (96, 60)):
            with self.subTest(size=size), self.assertRaisesRegex(ValueError, 'frame differs'):
                check.verify(transcript(), bytes(size[0] * size[1]), size)

    def test_existing_run_is_refused_before_inputs_are_read(self):
        with tempfile.TemporaryDirectory() as directory, \
                patch('tools.run_nse2_boundary.inspect') as inspect:
            with self.assertRaisesRegex(ValueError, 'new run directory'):
                prepare(Path(directory), Path(directory))
            inspect.assert_not_called()

    def test_preparation_uses_no_retained_storage_or_donor(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            packages = root / 'roms/archive-dct3-packages'
            packages.mkdir(parents=True)
            for suffix in ('zip', 'exe'):
                (packages / f'nse2_553.{suffix}').write_bytes(b'fixture')
            run = root / 'run'
            with patch('tools.run_nse2_boundary.inspect', side_effect=[
                    (b'own flash', {'archive_sha256': 'zip'}),
                    (b'own flash', {'archive_sha256': 'exe'})]):
                report = prepare(root, run)
            self.assertEqual((run / 'roms/noki5110i/5110i_5.53A.fls').read_bytes(), b'own flash')
            self.assertEqual(list((run / 'nvram').iterdir()), [])
            self.assertIn('erased', report['initial_eeprom'])
            self.assertIn('no mask', report['dsp_backend'])


if __name__ == '__main__':
    unittest.main()

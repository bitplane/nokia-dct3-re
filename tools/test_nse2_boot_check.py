import hashlib
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools import nse2_boot_check as check
from tools.run_nse2_boundary import checksum_fixture, prepare


def transcript():
    text = 'Soft reset\n'
    for i, (_, length, _, pc) in enumerate(check.NV_BLOCKS):
        expected = (~((255 * length) & 0xffff)) & 0xffffffff
        text += (f'nse2_nv_checksum: lr={pc:08x} length={length} expected={expected:08x} '
                 f'stored=ffffffff t=0.{i + 1} observer_writes=0\n')
    text += ('nse2_nv_verdict: address=0010fcb2 value=fd pc=0027dc14 '
             't=0.6 observer_writes=0\n')
    for i, (address, value, pc) in enumerate((
            (0x10b9f4, 0, 0x20010c), (0x10b9f5, 0, 0x20010c),
            (0x10b9f5, 1, 0x2781f4), (0x10b9f5, 0, 0x278226),
            (0x10b9f5, 0, 0x2782d6), (0x10b9f4, 0, 0x2782d8))):
        text += (f'nse2_state_byte: address={address:08x} value={value:02x} '
                 f'pc={pc:08x} t=0.00{i + 1} observer_writes=0\n')
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
    def test_record_result_requires_the_own_success_store(self):
        success = ('nse2_selftest_write: address=0010fcac value=00 pc=0023340a '
                   't=0.270 observer_writes=0\n')
        check.verify_record(success)
        for text in ('', success * 2, success.replace('value=00', 'value=0c'),
                     success.replace('pc=0023340a', 'pc=00233470'),
                     success + success.replace('value=00 pc=0023340a', 'value=0c pc=00233470')):
            with self.subTest(text=text), self.assertRaisesRegex(ValueError, 'record checksum'):
                check.verify_record(text)

    def test_record_fixture_changes_only_independently_decoded_checksums(self):
        base = checksum_fixture()
        image = checksum_fixture(record_checksum=True)
        self.assertEqual(image[:0x31e], base[:0x31e])
        self.assertEqual(image[0x320:], base[0x320:])
        self.assertEqual(image[0x31e:0x320], bytes.fromhex('db24'))
        self.assertEqual(int.from_bytes(image[0x31e:0x320], 'big'),
                         (sum(image[0x240:0x31e]) - sum(image[0x274:0x276])) & 0xffff)

    def test_checksum_fixture_changes_only_the_five_integrity_words(self):
        image = checksum_fixture()
        self.assertEqual(len(image), 8192)
        allowed = set()
        for start, length, stored, _ in check.NV_BLOCKS:
            allowed.update(range(stored, stored + 4))
            self.assertEqual(image[start:start + length], b'\xff' * length)
            self.assertEqual(int.from_bytes(image[stored:stored + 4], 'big'),
                             (~((255 * length) & 0xffff)) & 0xffffffff)
        self.assertTrue(all(value == 255 for i, value in enumerate(image) if i not in allowed))

    def test_checksum_experiment_requires_success_not_the_erased_failure(self):
        text = transcript()
        for _, length, _, pc in check.NV_BLOCKS:
            expected = (~((255 * length) & 0xffff)) & 0xffffffff
            prefix = f'nse2_nv_checksum: lr={pc:08x} length={length} expected={expected:08x} '
            text = text.replace(prefix + 'stored=ffffffff', prefix + f'stored={expected:08x}')
        text = text.replace('value=fd', 'value=00')
        self.assertEqual(check.verify_nv(text, checksum_only=True), 5)
        result = self.verify(text, checksum_only=True)
        self.assertEqual(result['invalid_nv_checksum_blocks'], 0)
        self.assertFalse(result['usable_boot'])
        with self.assertRaisesRegex(ValueError, 'peer completion'):
            self.verify(text + 'dspif_transport: RX enqueue type=80', checksum_only=True)
        with self.assertRaises(ValueError):
            check.verify_nv(transcript(), checksum_only=True)
        with self.assertRaises(ValueError):
            check.verify_nv(text)

    def test_nv_checks_require_all_five_erased_blocks_in_order(self):
        self.assertEqual(check.verify_nv(transcript()), 5)
        for old, new in (('length=480', 'length=479'), ('stored=ffffffff', 'stored=00000000'),
                         ('expected=ffff21df', 'expected=ffff21de'),
                         ('lr=00258f81', 'lr=00258f83'), ('t=0.5', 't=0.1'),
                         ('value=fd', 'value=00'), ('t=0.6', 't=0.4')):
            with self.subTest(old=old), self.assertRaisesRegex(ValueError, 'NV checksum'):
                check.verify_nv(transcript().replace(old, new))
        with self.assertRaises(ValueError):
            check.verify_nv('')

    def verify(self, text, *, checksum_only=False):
        pixels = bytes(84 * 48)
        with patch.object(check, 'FRAME', hashlib.sha256(pixels).hexdigest()):
            return check.verify(text, pixels, (84, 48), checksum_only=checksum_only)

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

    def test_loader_failure_is_observed_not_inferred_from_missing_hits(self):
        for old, new in (('value=01 pc=002781f4', 'value=02 pc=002781f4'),
                         ('pc=00278226', 'pc=00278224'),
                         ('address=0010b9f4 value=00 pc=002782d8',
                          'address=0010b9f4 value=01 pc=002782d8')):
            with self.subTest(old=old), self.assertRaisesRegex(ValueError, 'loader startup'):
                self.verify(transcript().replace(old, new))

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
            self.assertEqual((run / 'roms/noki5110i/5110i_5.53a.fls').read_bytes(), b'own flash')
            self.assertEqual(list((run / 'nvram').iterdir()), [])
            self.assertIn('erased', report['initial_eeprom'])
            self.assertIn('no mask', report['dsp_backend'])


if __name__ == '__main__':
    unittest.main()

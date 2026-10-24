import hashlib
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.nse2_native_check import verify_native, verify_menu_probe, verify_nominal_voltage
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


def menu_transcript():
    text = 'nse2_key_mask: value=3f pc=00299066 t=0.411 observer_writes=0\n'
    text += ('nse2_report14_sample: valid=0 first=19d6 second=19d6 remaining=0a '
             'pc=002645b2 t=0.31 observer_writes=0\n')
    for index, down, time in ((1, 1, 3.0), (1, 0, 3.2), (2, 1, 5.0), (2, 0, 5.2)):
        old, new = ('1f', '1d') if down else ('1d', '1f')
        text += (f'nse2_menu_contact: index={index} down={down} t={time} physical_only=1\n'
                 f'kbgpio: columns={old}->{new} changed=00 row=e0 dir=1f mask=3f t={time}\n')
    for time in range(2, 9):
        text += (f'nse2_ui_state: t={time}.0 state_10c664=0010 flags_10c5c9=0e '
                 'phase_10ffac=06 observer_writes=0\n')
    for code in ('0015', '0016', '0017'):
        text += (f'nse2_startup_message: address=0010c662 value={code} pc=0025c584 '
                 't=1.1 observer_writes=0\n')
    return text


def nominal_transcript():
    text = 'ccont_input: adc_select=2 raw=22a ctrl=28 t=0.1\n'
    for i, remaining in enumerate(range(9, -1, -1)):
        text += (f'nse2_average_count: remaining={remaining:02x} first=0dfd second=0dfd '
                 f'pc=002645d0 t=1.{i:02d} observer_writes=0\n')
    text += ('nse2_startup_message: address=0010c662 value=0014 pc=0025c584 '
             't=1.2 observer_writes=0\n'
             'nse2_key_mask: value=20 pc=0025c8c0 t=1.3 observer_writes=0\n')
    for second in range(2, 9):
        text += (f'nse2_ui_state: t={second}.0 state_10c664=0004 flags_10c5c9=0f '
                 'phase_10ffac=03 observer_writes=0\n')
    return text


class NativeTest(unittest.TestCase):
    def test_nominal_readiness_is_scoped_before_later_application_trace_cap(self):
        result = verify_nominal_voltage(nominal_transcript() +
            'nse2_startup_message_cap: reached=256 t=28.0\n', b'', empty_socket_frame=False)
        self.assertTrue(result['later_mailbox_trace_capped'])
        self.assertEqual(result['startup_message_coverage_seconds'],3)
        for suffix in ('', ' t=2.0', ' t=3.0'):
            with self.subTest(suffix=suffix), self.assertRaises(ValueError):
                verify_nominal_voltage(nominal_transcript() +
                    'nse2_startup_message_cap: reached=256' + suffix, b'', empty_socket_frame=False)

    def test_nominal_voltage_requires_the_complete_organic_chain(self):
        with patch('tools.nse2_native_check.NOMINAL_FRAME', hashlib.sha256(b'frame').hexdigest()):
            result = verify_nominal_voltage(nominal_transcript(), b'frame')
        self.assertTrue(result['graphical_boot'])
        self.assertFalse(result['measured_board_calibration'])
        self.assertNotIn('usable_boot', result)

    def test_nominal_samples_report_order_and_frame_are_checked(self):
        for old, new in (('remaining=09', 'remaining=0a'), ('first=0dfd', 'first=19d6'),
                         ('raw=22a', 'raw=3ff'), ('t=1.2 ', 't=0.2 '),
                         ('pc=0025c8c0', 'pc=00299066'), ('flags_10c5c9=0f', 'flags_10c5c9=0e')):
            with self.subTest(old=old), \
                    patch('tools.nse2_native_check.NOMINAL_FRAME', hashlib.sha256(b'frame').hexdigest()), \
                    self.assertRaises(ValueError):
                verify_nominal_voltage(nominal_transcript().replace(old, new), b'frame')
        with self.assertRaisesRegex(ValueError, 'frame differs'):
            verify_nominal_voltage(nominal_transcript(), b'wrong frame')

    def test_nominal_input_requires_decoder_activity_during_both_contacts(self):
        contacts = '\n'.join(row for row in menu_transcript().splitlines() if row.startswith('nse2_menu_contact:'))
        text = nominal_transcript() + contacts + '\n'
        with self.assertRaisesRegex(ValueError, 'own decoder'):
            verify_nominal_voltage(text, b'', physical_menu=True)
        for time in (3.1, 5.1):
            text += f'nse2_key_decode: raw=11 key=19 pc=00299044 t={time} observer_writes=0\n'
        result = verify_nominal_voltage(text, b'', physical_menu=True)
        self.assertTrue(result['physical_key_delivery'])
        self.assertNotIn('graphical_boot', result)

    def test_long_observation_cannot_pass_with_only_eight_seconds(self):
        text, payload = native_transcript()
        with self.assertRaisesRegex(ValueError, 'horizon'):
            self.check(text, payload, minimum_time=30)

    def test_physical_probe_distinguishes_columns_from_decoded_delivery(self):
        result = verify_menu_probe(menu_transcript())
        self.assertTrue(result['gpio_columns_respond'])
        self.assertFalse(result['physical_key_delivery'])
        self.assertEqual(result['unobserved_startup_report'], '0x14')

    def test_contacts_alone_are_not_input_acceptance(self):
        for old, new in (('mask=3f', 'mask=20'), ('changed=00', 'changed=02'),
                         ('t=5.2', 't=6.2'), ('pc=00299066', 'pc=00000000')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                verify_menu_probe(menu_transcript().replace(old, new))

    def test_probe_fails_when_frontier_advances_or_evidence_disappears(self):
        for text in (menu_transcript().replace('flags_10c5c9=0e', 'flags_10c5c9=0f'),
                     menu_transcript().replace('value=0015', 'value=0014'),
                     menu_transcript().replace('t=8.0', 't=7.0'),
                     menu_transcript() + 'nse2_startup_message_cap: reached=256',
                     menu_transcript() + 'nse2_key_decode: raw=11 key=19'):
            with self.assertRaises(ValueError):
                verify_menu_probe(text)

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
            self.assertEqual((run / 'roms/nse2r4t/5110i_5.53a.fls').read_bytes(), b'own flash')
            self.assertEqual({path.name for path in (run / 'roms/nse2r4t').iterdir()},
                             {'5110i_5.53a.fls', 'program.bin', 'data.bin'})
            self.assertEqual({path.name for path in (run / 'nvram/nse2r4t').iterdir()}, {'eeprom'})
            nominal = root / 'nominal'
            with patch('tools.run_nse2_native.MASK_INPUTS', specs), \
                    patch('tools.run_nse2_boundary.inspect', side_effect=[
                        (b'own flash', {'archive_sha256': 'zip'}),
                        (b'own flash', {'archive_sha256': 'exe'})]):
                nominal_report = prepare_native(root, nominal, record_checksum=True, nominal_vbat=True)
            self.assertEqual(nominal_report['machine'], 'nse2r4v')
            self.assertEqual(nominal_report['adc2_input']['raw'], 554)
            self.assertFalse(nominal_report['adc2_input']['measured_nse2_calibration'])
            self.assertEqual((nominal / 'nvram/nse2r4v/eeprom').read_bytes(),
                             (run / 'nvram/nse2r4t/eeprom').read_bytes())
            self.assertEqual((nominal / 'roms/nse2r4v/5110i_5.53a.fls').read_bytes(), b'own flash')
            zero = root / 'retry-zero'
            with patch('tools.run_nse2_native.MASK_INPUTS', specs), \
                    patch('tools.run_nse2_boundary.inspect', side_effect=[
                        (b'own flash', {'archive_sha256': 'zip'}),
                        (b'own flash', {'archive_sha256': 'exe'})]):
                zero_report = prepare_native(root, zero, record_checksum=True, nominal_vbat=True,
                                             insert_sim=True, phone_retry_zero=True)
            before = (nominal / 'nvram/nse2r4v/eeprom').read_bytes()
            after = (zero / 'nvram/nse2r4v/eeprom').read_bytes()
            self.assertEqual([i for i, (a, b) in enumerate(zip(before, after)) if a != b], [0x4a4])
            self.assertEqual(after[0x4a4], 0)
            self.assertFalse(zero_report['authentic_provisioning'])
            self.assertIn('value="0"', (zero / 'cfg/nse2r4v.cfg').read_text())

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

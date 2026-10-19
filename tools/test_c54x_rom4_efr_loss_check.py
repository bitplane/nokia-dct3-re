import math
import struct
import unittest

from tools.c54x_rom4_efr_loss_check import COMPLETE, REPLAY, characterize, lost
from tools.run_c54x_rom4_crc import check_result, check_staged_frontend


def fixture(changes=True):
    serial = bytearray(163 * 494)
    reference = bytes(163 * 320)
    rows = []
    for frame in range(163):
        struct.pack_into('<H', serial, frame * 494, int(lost(frame)))
        samples = [0] * 160
        if changes and frame == 9:
            samples[0] = 32767
        if changes and frame == 32:
            samples[-1] = 32768
        count = sum(bool(sample) for sample in samples)
        rows.append(f'ROM4 EFR loss observation: frame={frame} mismatches={count} pcm=' +
                    ''.join(f'{sample:04x}' for sample in samples))
    summary = ('TMS320C54x ROM4 EFR loss observation: COMPLETE frames=163 samples=26080 '
               'erased=28 bursts=1,3,8,16 mismatches=' + ('2' if changes else '0') +
               ' conformance_claim=0 handset_audio_claim=0')
    return '\n'.join([*rows, REPLAY, summary, COMPLETE]), reference, bytes(serial)


class EfrLossObservationTests(unittest.TestCase):
    def test_verbose_teardown_is_explicit_and_bounded(self):
        text, reference, serial = fixture()
        tail = '\nEnter sdlwindow_exit\nLeave sdlwindow_exit'
        characterize(text + tail, 3, reference, serial, verbose_teardown=True)
        for bad, enabled in ((text + tail, False), (text + tail + '\nextra', True)):
            with self.subTest(enabled=enabled), self.assertRaises(ValueError):
                characterize(bad, 3, reference, serial, verbose_teardown=enabled)

    def test_staged_selection_does_not_promote_characterization(self):
        text, reference, serial = fixture()
        marker = 'ROM4 EFR erasure decoder frontend: staged handset_audio_claim=0'
        staged = marker + '\n' + text
        check_staged_frontend(staged, 'EFR erasure decoder')
        report, _ = characterize(staged, 3, reference, serial)
        self.assertFalse(report['conformance_claim'])
        with self.assertRaises(ValueError):
            check_result(staged, 3, efr_erasure_decoder=True, staged=True)
        for bad in (text, marker + '\n' + staged,
                    staged.replace('erasure decoder frontend', 'decoder frontend')):
            with self.subTest(text=bad[:90]), self.assertRaises(ValueError):
                check_staged_frontend(bad, 'EFR erasure decoder')

    def test_driver_validity_errors_invalidate_complete_observation(self):
        text, reference, serial = fixture()
        for count in (1, 3):
            invalid = f'Driver tms54rom4 (file tms320c54x_test.cpp): {count} errors, 0 warnings\n' + text
            with self.subTest(count=count), self.assertRaisesRegex(ValueError, 'validity errors'):
                characterize(invalid, 3, reference, serial)

    def test_signed_samples_and_independent_difference_accounting(self):
        text, reference, serial = fixture()
        report, pcm = characterize(text, 3, reference, serial)
        self.assertEqual(report['status'], 'characterized')
        self.assertEqual(report['first_differing_frame'], 9)
        self.assertEqual(report['mismatches'], 2)
        self.assertEqual(report['differing_frames'], 2)
        self.assertEqual(report['max_abs'], 32768)
        self.assertEqual(report['rms'], math.sqrt((32767 ** 2 + 32768 ** 2) / 26080))
        self.assertEqual(struct.unpack_from('<h', pcm, 32 * 320 + 318)[0], -32768)
        self.assertFalse(report['reference_equal'])
        self.assertFalse(report['conformance_claim'])
        self.assertFalse(report['handset_audio_claim'])
        self.assertEqual(sum(row['erased'] for row in report['records']), 28)

    def test_equal_synthetic_output_still_does_not_claim_conformance(self):
        text, reference, serial = fixture(False)
        report, pcm = characterize(text, 3, reference, serial)
        self.assertEqual(pcm, reference)
        self.assertTrue(report['reference_equal'])
        self.assertIsNone(report['first_differing_frame'])
        self.assertFalse(report['conformance_claim'])

    def test_observation_is_not_accepted_by_strict_decoder_checker(self):
        text, reference, serial = fixture()
        characterize(text, 3, reference, serial)
        with self.assertRaises(ValueError):
            check_result(text, 3, efr_erasure_decoder=True)

    def test_rejects_incomplete_duplicate_unordered_and_corrupted_capture(self):
        text, reference, serial = fixture()
        rows = text.splitlines()
        bad = [text.replace(rows[9] + '\n', ''), text.replace(rows[9], rows[8]),
               '\n'.join(rows[:9] + [rows[10], rows[9]] + rows[11:]),
               text.replace('frame=9 mismatches=1', 'frame=9 mismatches=0'),
               text.replace('frame=9 mismatches=1 pcm=7fff', 'frame=9 mismatches=1 pcm=8000'),
               text.replace('mismatches=2 conformance_claim', 'mismatches=3 conformance_claim'),
               text.replace('conformance_claim=0', 'conformance_claim=1'),
               text.replace('handset_audio_claim=0', 'handset_audio_claim=1'),
               text.replace(REPLAY, ''), text + '\n' + REPLAY,
               REPLAY + '\n' + text.replace(REPLAY + '\n', ''),
               text.replace(COMPLETE, 'Fatal error: decoder failed'),
               text.replace('pcm=' + '0' * 640, 'pcm=' + '0' * 639, 1),
               'NOT FOUND\n' + text, 'WRONG CHECKSUMS\n' + text, 'Errors:\n' + text,
               'Fatal error: earlier failure\n' + text]
        # A changed sample with the same mismatch count is valid observation data,
        # not an invalid capture. Its hash and measured error must change.
        changed = bad.pop(4)
        before, _ = characterize(text, 3, reference, serial)
        after, _ = characterize(changed, 3, reference, serial)
        self.assertNotEqual(before['native_pcm_sha256'], after['native_pcm_sha256'])
        self.assertNotEqual(before['rms'], after['rms'])
        for value in bad:
            with self.subTest(value=value[:100]), self.assertRaises(ValueError):
                characterize(value, 3, reference, serial)

    def test_input_schedule_sizes_bits_and_exit_are_mandatory(self):
        text, reference, serial = fixture()
        for offset, value in ((8 * 494, 0), (490, 2), (492, 2), (2, 2)):
            changed = bytearray(serial)
            struct.pack_into('<H', changed, offset, value)
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                characterize(text, 3, reference, changed)
        for ref, inp in ((reference[:-1], serial), (reference, serial[:-1])):
            with self.assertRaises(ValueError):
                characterize(text, 3, ref, inp)
        for code in (0, 1, -9):
            with self.assertRaises(ValueError):
                characterize(text, code, reference, serial)
        for truncated in ('', COMPLETE, REPLAY + '\n' + COMPLETE):
            with self.assertRaises(ValueError):
                characterize(truncated, 3, reference, serial)
        characterize(text + '\nAverage speed: 1200.0% (6 seconds)', 3, reference, serial)


if __name__ == '__main__':
    unittest.main()

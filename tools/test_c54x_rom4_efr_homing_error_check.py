"""Acceptance scope for the generated EFR homing/error conversation."""
import unittest

from tools import c54x_rom4_efr_homing_error_check as checker


REFERENCE = b'\x08\x00' * (checker.FRAMES * 160)


def capture(mismatches=None):
    mismatches = mismatches or {}
    rows = [
        f'ROM4 EFR homing error frame: frame={frame} '
        f'mismatches={mismatches.get(frame, 0)} '
        f'resets={int(frame in checker.RESETS)} '
        f'reference_settled={int(frame not in checker.UNSETTLED)} '
        f'pcm={"0000" * mismatches.get(frame, 0)}{"0008" * (160 - mismatches.get(frame, 0))}'
        for frame in range(checker.FRAMES)
    ]
    recovery = []
    for frame in (1, 24, 47):
        recovery.extend([
            f'ROM4 homing recovery parameter: frame={frame} pc=c8dc actual=000b table=000b',
            f'ROM4 homing recovery store: frame={frame} a=0000000000 b=0000000000 parameter=000b',
            f'ROM4 homing recovery parameter: frame={frame} pc=c930 actual=0000 table=000b',
        ])
    return '\n'.join(recovery + rows + [checker.REPLAY, checker.OBSERVED,
        f'ROM4 EFR homing error total: mismatches={sum(mismatches.values())}',
        checker.COMPLETE])


class HomingErrorCheckTest(unittest.TestCase):
    def test_staged_selection_is_unique_and_does_not_relax_reference(self):
        marker = 'ROM4 EFR homing decoder frontend: staged handset_audio_claim=0'
        text = marker + '\n' + capture({1: 17})
        self.assertEqual(checker.check(text, 3, REFERENCE, staged=True)['frontend'], 'staged')
        for invalid in (capture(), marker + '\n' + text,
                        text.replace('homing decoder frontend', 'decoder frontend')):
            with self.subTest(invalid=invalid[:100]), self.assertRaisesRegex(ValueError, 'frontend selection'):
                checker.check(invalid, 3, REFERENCE, staged=True)
        with self.assertRaisesRegex(ValueError, 'strict homing/error'):
            checker.check(text, 3, REFERENCE, staged=True, strict=True)

    def test_driver_validity_failure_invalidates_characterization(self):
        for staged in (False, True):
            marker = 'ROM4 EFR homing decoder frontend: staged handset_audio_claim=0\n' if staged else ''
            with self.subTest(staged=staged), self.assertRaisesRegex(ValueError, 'validity errors'):
                checker.check('Driver tms54rom4: 1 errors, 0 warnings\n' + marker + capture(),
                              3, REFERENCE, staged=staged)

    def test_exact_sequence(self):
        result = checker.check(capture(), 3, REFERENCE, strict=True)
        self.assertTrue(result['full_reference_equal'])
        self.assertTrue(result['reference_reset_equal'])
        self.assertFalse(result['handset_audio_claim'])

    def test_unsettled_difference_is_reported_not_hidden(self):
        text = capture({1: 17, 24: 4})
        self.assertEqual(checker.check(text, 3, REFERENCE)['total_mismatches'], 21)
        with self.assertRaisesRegex(ValueError, 'strict homing/error'):
            checker.check(text, 3, REFERENCE, strict=True)

    def test_settled_difference_is_reported_and_fails_strict(self):
        text = capture({3: 1})
        self.assertEqual(checker.check(text, 3, REFERENCE)['reference_settled_mismatch_frames'], [3])
        with self.assertRaisesRegex(ValueError, 'strict homing/error'):
            checker.check(text, 3, REFERENCE, strict=True)

    def test_bad_frame_cannot_reset(self):
        text = capture().replace('frame=0 mismatches=0 resets=0',
                                 'frame=0 mismatches=0 resets=1')
        with self.assertRaisesRegex(ValueError, 'bad homing frame'):
            checker.check(text, 3, REFERENCE)

    def test_missing_clean_reset_is_not_promoted_to_conformance(self):
        text = capture().replace('frame=1 mismatches=0 resets=1',
                                 'frame=1 mismatches=0 resets=0')
        self.assertEqual(checker.check(text, 3, REFERENCE)['reset_mismatch_frames'], [1])
        with self.assertRaisesRegex(ValueError, 'strict homing/error'):
            checker.check(text, 3, REFERENCE, strict=True)

    def test_missing_recovery_evidence_fails(self):
        with self.assertRaisesRegex(ValueError, 'recovery parameter observations'):
            checker.check(capture().replace('pc=c930 actual=0000', 'pc=c930 actual=000b'), 3, REFERENCE)

    def test_reported_count_cannot_override_captured_pcm(self):
        with self.assertRaisesRegex(ValueError, 'disagree with capture'):
            checker.check(capture().replace('frame=1 mismatches=0', 'frame=1 mismatches=7'), 3, REFERENCE)

    def test_missing_frame_and_wrong_exit_fail(self):
        with self.assertRaisesRegex(ValueError, 'frame coverage'):
            checker.check('\n'.join(line for line in capture().splitlines()
                                    if not line.startswith('ROM4 EFR homing error frame: frame=0 ')), 3, REFERENCE)
        with self.assertRaisesRegex(ValueError, 'terminal'):
            checker.check(capture(), 0, REFERENCE)

    def test_aggregate_and_rom_errors_fail(self):
        with self.assertRaisesRegex(ValueError, 'aggregate'):
            checker.check(capture().replace('total: mismatches=0', 'total: mismatches=1'), 3, REFERENCE)
        with self.assertRaisesRegex(ValueError, 'inputs'):
            checker.check('WRONG CHECKSUMS\n' + capture(), 3, REFERENCE)


if __name__ == '__main__':
    unittest.main()

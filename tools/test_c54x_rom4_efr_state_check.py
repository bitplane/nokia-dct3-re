import unittest

from tools.c54x_rom4_efr_state_check import WIDTHS, compare, parse, parse_policy
from tools.c54x_rom4_efr_loss_check import lost


def capture():
    result = []
    for frame in range(163):
        recovering = frame in (9, 35, 88, 136)
        if recovering:
            result.extend([f'EFRFLAG frame={frame} stage=before value=0001',
                           f'EFRFLAG frame={frame} stage=after value=0000'])
        for kind in ('R2', 'LSF'):
            result.append(f'EFRSTATE frame={frame} kind={kind} values=' + '0000' * WIDTHS[kind])
        for subframe in range(4):
            for kind in ('pitch', 'code'):
                result.append(f'EFRGAIN frame={frame} kind={kind} subframe={subframe} value=1234')
            if not lost(frame):
                for prefix in ('EFRPOLICY', 'EFRCANDIDATE'):
                    previous = int(recovering and prefix == 'EFRPOLICY')
                    result.append(f'{prefix} frame={frame} subframe={subframe} prev_bf={previous:04x} previous_gain=1234 value=1234')
        for kind in ('RAW', 'LPC'):
            result.append(f'EFRSTATE frame={frame} kind={kind} values=' + '0000' * WIDTHS[kind])
    return '\n'.join(result)


class EfrStateTests(unittest.TestCase):
    def test_equal_boundaries_are_not_conformance(self):
        text = capture()
        result = compare(text, text)
        self.assertTrue(all(value is None for value in result['first_differing_frame'].values()))
        self.assertFalse(result['conformance_claim'])
        self.assertFalse(result['handset_audio_claim'])

    def test_independent_first_difference_by_boundary(self):
        reference = capture()
        native = reference
        for frame, kind in ((8, 'R2'), (9, 'LSF'), (9, 'RAW'), (10, 'LPC')):
            native = native.replace(f'frame={frame} kind={kind} values=0000',
                                    f'frame={frame} kind={kind} values=ffff')
        native = native.replace('frame=35 kind=code subframe=2 value=1234',
                                'frame=35 kind=code subframe=2 value=5678')
        native = native.replace('EFRCANDIDATE frame=35 subframe=2 prev_bf=0000 previous_gain=1234 value=1234',
                                'EFRCANDIDATE frame=35 subframe=2 prev_bf=0000 previous_gain=1234 value=5678')
        result = compare(native, reference)
        self.assertEqual(result['first_differing_frame'],
                         dict(R2=8, LSF=9, RAW=9, LPC=10, pitch=None, code=35))
        self.assertEqual(result['records'][35]['differences']['code'], 1)

    def test_missing_duplicate_malformed_and_extra_boundaries_fail(self):
        text = capture()
        first = text.splitlines()[0]
        for changed in (text.replace(first + '\n', ''), first + '\n' + text,
                        text.replace('kind=R2 values=0000', 'kind=R2 values=000', 1),
                        text.replace('frame=0 kind=R2', 'frame=163 kind=R2', 1),
                        text.replace('kind=pitch subframe=0', 'kind=pitch subframe=4', 1),
                        text.replace('kind=LSF', 'kind=OTHER', 1),
                        text.replace('value=1234', 'value=12345', 1)):
            with self.subTest(changed=changed[:100]), self.assertRaises(ValueError):
                parse(changed)

    def test_reordered_records_and_partial_captures_fail(self):
        rows = capture().splitlines()
        rows[0], rows[1] = rows[1], rows[0]
        for text in ('', '\n'.join(rows[:12]), '\n'.join(rows)):
            with self.assertRaises(ValueError):
                parse(text)

    def test_unrelated_banners_do_not_change_state_records(self):
        text = capture()
        self.assertEqual(parse(text), parse('reference banner\n' + text + '\nframe counter'))

    def test_policy_requires_every_good_frame_and_consistent_gain(self):
        text = capture()
        line = 'EFRPOLICY frame=0 subframe=0 prev_bf=0000 previous_gain=1234 value=1234'
        for changed in (text.replace(line, ''), text + '\n' + line,
                        text.replace(line, line.replace('frame=0', 'frame=8'))):
            with self.assertRaises(ValueError):
                parse_policy(changed)
        with self.assertRaisesRegex(ValueError, 'published code gain'):
            compare(text, text.replace(line, line.replace('value=1234', 'value=1235')))
        with self.assertRaisesRegex(ValueError, 'published gain'):
            compare(text.replace('EFRCANDIDATE frame=0 subframe=0 prev_bf=0000 previous_gain=1234 value=1234',
                                 'EFRCANDIDATE frame=0 subframe=0 prev_bf=0000 previous_gain=1234 value=1235'), text)

    def test_recovery_clamp_is_reported_not_fitted(self):
        native = capture().replace('EFRCANDIDATE frame=35 subframe=0 prev_bf=0000 previous_gain=1234 value=1234',
                                   'EFRCANDIDATE frame=35 subframe=0 prev_bf=0000 previous_gain=0017 value=1234')
        reference = capture().replace('EFRPOLICY frame=35 subframe=0 prev_bf=0001 previous_gain=1234 value=1234',
                                      'EFRPOLICY frame=35 subframe=0 prev_bf=0001 previous_gain=0017 value=0017')
        reference = reference.replace('frame=35 kind=code subframe=0 value=1234',
                                      'frame=35 kind=code subframe=0 value=0017')
        result = compare(native, reference)
        self.assertEqual(len(result['recovery_code_gain']), 16)
        recovery = next(row for row in result['recovery_code_gain'] if row['frame'] == 35 and row['subframe'] == 0)
        self.assertTrue(recovery['reference_clamp_matches'])
        self.assertFalse(result['conformance_claim'])

    def test_original_flag_clear_is_mandatory_and_not_synthesized(self):
        text = capture()
        for changed in (text.replace('EFRFLAG frame=35 stage=before value=0001', ''),
                        text.replace('EFRFLAG frame=35 stage=after value=0000',
                                     'EFRFLAG frame=35 stage=after value=0001'),
                        text + '\nEFRFLAG frame=136 stage=after value=0000'):
            with self.assertRaisesRegex(ValueError, 'recovery-flag clear'):
                compare(changed, text)


if __name__ == '__main__':
    unittest.main()

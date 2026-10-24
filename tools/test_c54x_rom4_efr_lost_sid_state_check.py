import unittest

from tools.c54x_rom4_efr_lost_sid_state_check import compare, parse_native, characterize


def fixture(*, complete=False, difference_frame=None, synthesis=False, raw_difference_frame=None):
    native, reference = [], []
    for frame in range(518):
        group = ['efr_boundary: kind=entry']
        for kind in ('R2', 'LSF'):
            reference.append(f'EFRSTATE frame={frame} kind={kind} values=' + '0000' * 10)
            values = ('0001' + '0000' * 9 if frame == difference_frame and kind == 'R2'
                      else '0000' * 10)
            group.append(f'efr_boundary: kind={kind} values=' + values)
        for sub in range(4):
            pitch, code = 0, 8 if frame == 97 else 0
            reference.extend((f'EFRGAIN frame={frame} kind=pitch subframe={sub} value={pitch:04x}',
                              f'EFRGAIN frame={frame} kind=code subframe={sub} value={code:04x}'))
            group.append('efr_boundary: kind=pitch values=0000')
            if frame == 97:
                candidate = [8, 9, 12, 84][sub]
                gain = [10, 8, 9, 12][sub]
                group.append(f'efr_gain_policy: previous=0000 gain={gain:04x} candidate_b={candidate << 16:010x}')
                code = candidate
            group.append(f'efr_boundary: kind=code values={code:04x}')
        for kind, width in (('RAW', 160), ('LPC', 44)):
            reference.append(f'EFRSTATE frame={frame} kind={kind} values=' + '0000' * width)
            if synthesis:
                values = ('0001' + '0000' * (width - 1)
                          if frame == raw_difference_frame and kind == 'RAW' else '0000' * width)
                group.append(f'efr_boundary: kind={kind} values=' + values)
        if frame == 97:
            for sub in range(4):
                reference.append(f'EFRPOLICY frame=97 subframe={sub} prev_bf=0001 previous_gain={10 if sub == 0 else 8:04x} value=0008')
        if frame < 98 or complete:
            native.extend(group * 2)
    return '\n'.join(native), '\n'.join(reference)


class LostSidStateTests(unittest.TestCase):
    def test_synthesis_characterization_checks_whole_frames(self):
        native, reference = fixture(complete=True, synthesis=True, raw_difference_frame=118)
        result = characterize(native, reference, synthesis=True)
        self.assertEqual(result['boundaries']['RAW']['compared_words'], 82880)
        self.assertEqual(result['boundaries']['RAW']['differing_frames'],
                         [dict(frame=118, words=1)])
        self.assertEqual(result['boundaries']['LPC']['compared_words'], 22792)
        self.assertEqual(result['boundaries']['LPC']['differing_words'], 0)
        with self.assertRaisesRegex(ValueError, 'boundary order'):
            characterize(*fixture(complete=True), synthesis=True)
        with self.assertRaisesRegex(ValueError, 'field width'):
            characterize(native.replace('kind=RAW values=', 'kind=RAW values=0000'),
                         reference, synthesis=True)

    def test_complete_semantic_characterization_retains_known_prefix(self):
        report = characterize(*fixture(complete=True, difference_frame=100))
        self.assertEqual(report['replayed_frames'], 518)
        self.assertEqual(report['boundaries']['R2']['differing_frames'],
                         [dict(frame=100, words=1)])
        self.assertEqual(report['boundaries']['code']['last_difference'], 97)
        self.assertIsNone(report['boundaries']['LSF']['first_difference'])
        self.assertTrue(report['prefix']['reference_cap_reproduced'])
        self.assertFalse(report['conformance_claim'])

    def test_complete_mode_rejects_prefix_and_changed_known_entrance(self):
        with self.assertRaisesRegex(ValueError, '518 original'):
            characterize(*fixture())
        native, reference = fixture(complete=True)
        with self.assertRaisesRegex(ValueError, 'previous-bad'):
            characterize(native, reference.replace('prev_bf=0001', 'prev_bf=0000'))
        with self.assertRaisesRegex(ValueError, '518 original'):
            characterize(native.rsplit('efr_boundary: kind=entry', 1)[0], reference)
        head, last = native.rsplit('efr_boundary: kind=entry', 1)
        with self.assertRaisesRegex(ValueError, 'restoration'):
            characterize(head + 'efr_boundary: kind=entry' +
                         last.replace('kind=R2 values=0000', 'kind=R2 values=0001'), reference)

    def test_bounded_gain_difference(self):
        report = compare(*fixture())
        self.assertTrue(report['reference_cap_reproduced'])
        self.assertFalse(report['conformance_claim'])
        self.assertFalse(report['complete_fixture_claim'])

    def test_missing_native_frame(self):
        native, _ = fixture()
        with self.assertRaisesRegex(ValueError, '98 original'):
            parse_native(native.rsplit('efr_boundary: kind=entry', 1)[0])

    def test_changed_replay(self):
        native, _ = fixture()
        with self.assertRaisesRegex(ValueError, 'restoration'):
            parse_native(native.replace('values=0000', 'values=0001', 1))

    def test_earlier_state_difference(self):
        native, reference = fixture()
        with self.assertRaisesRegex(ValueError, 'earlier LSF'):
            compare(native.replace('kind=LSF values=0000', 'kind=LSF values=0001'), reference)

    def test_changed_candidate(self):
        native, reference = fixture()
        with self.assertRaisesRegex(ValueError, 'unclamped candidate'):
            compare(native.replace('candidate_b=0000540000', 'candidate_b=0000550000'), reference)

    def test_reference_history_required(self):
        native, reference = fixture()
        with self.assertRaisesRegex(ValueError, 'previous-bad'):
            compare(native, reference.replace('prev_bf=0001', 'prev_bf=0000'))

    def test_complete_reference_required(self):
        native, reference = fixture()
        with self.assertRaisesRegex(ValueError, 'coverage'):
            compare(native, reference.replace('EFRSTATE frame=517 kind=RAW', 'ignored'))

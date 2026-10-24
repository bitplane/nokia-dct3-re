import unittest

from tools.c54x_rom4_efr_excitation_check import compare, native_records, reference_records


def fixture():
    native, reference = [], []
    for frame in range(2):
        group = ['efr_boundary: kind=entry']
        for sub in range(4):
            ctrl = 0x401 if frame == 0 else 0x290
            group.append(f'efr_excitation_call: ctrl={ctrl:04x} base=1514 offset={sub * 40:04x}')
            if frame == 0:
                reference.extend((f'EFREXCPRED frame=0 subframe={sub} length=40',
                                  f'EFREXC frame=0 subframe={sub} kind=ADAPT values=' + '0000' * 40))
            reference.extend((f'EFREXCCALL frame={frame} subframe={sub} ctrl={ctrl | 0x100:04x} length=40',
                              f'EFREXC frame={frame} subframe={sub} kind=FIXED values=' + '0000' * 40))
            for kind in ('FIXED', 'ADAPT'):
                changed = ((frame, sub, kind) in ((1, 2, 'FIXED'), (0, 1, 'ADAPT')) or
                           (frame == 1 and kind == 'ADAPT'))
                values = ('0001' if changed else '0000') + '0000' * 39
                group.append(f'efr_excitation: kind={kind} values={values}')
        native.extend(group * 2)
    return '\n'.join(native), '\n'.join(reference)


class ExcitationCheckTests(unittest.TestCase):
    def test_compares_fixed_and_only_speech_admitted_adaptive_vectors(self):
        result = compare(*fixture(), frames=2)
        self.assertEqual(result['boundaries']['FIXED']['compared_words'], 320)
        self.assertEqual(result['boundaries']['ADAPT']['compared_words'], 160)
        self.assertEqual(result['boundaries']['ADAPT']['differences'],
                         [dict(frame=0, subframe=1, words=1)])
        self.assertFalse(result['conformance_claim'])

    def test_changed_native_replay_or_offset_fails(self):
        native, _ = fixture()
        with self.assertRaisesRegex(ValueError, 'replay'):
            native_records(native.replace('kind=FIXED values=0000', 'kind=FIXED values=0001', 1), frames=2)
        with self.assertRaisesRegex(ValueError, 'ownership'):
            native_records(native.replace('offset=0028', 'offset=0b93'), frames=2)

    def test_missing_or_short_native_vectors_fail(self):
        native, _ = fixture()
        with self.assertRaisesRegex(ValueError, 'width'):
            native_records(native.replace('kind=ADAPT values=', 'kind=ADAPT values=0000'), frames=2)
        with self.assertRaisesRegex(ValueError, 'frames'):
            native_records(native.rsplit('efr_boundary: kind=entry', 1)[0], frames=2)

    def test_reference_predictor_presence_and_call_length_are_checked(self):
        _, reference = fixture()
        for bad in (reference.replace('EFREXCPRED', 'ignored'),
                    reference.replace('kind=ADAPT', 'kind=FIXED'),
                    reference.replace('length=40', 'length=39')):
            with self.assertRaises(ValueError):
                reference_records(bad, frames=2)

    def test_speech_admission_must_agree_without_claiming_all_control_bits(self):
        native, reference = fixture()
        with self.assertRaisesRegex(ValueError, 'speech-admission'):
            compare(native.replace('ctrl=0290', 'ctrl=0291'), reference, frames=2)

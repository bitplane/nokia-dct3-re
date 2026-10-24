from unittest.mock import patch
import unittest

from tools import c54x_rom4_efr_predictor_check as check


def fixture():
    controls = {(frame, sub): int((frame, sub) in ((0, 0), (1, 2)))
                for frame in range(2) for sub in range(4)}
    native, reference = [], []
    for frame, sub in ((0, 0), (1, 2)):
        group = ['efr_boundary: kind=entry',
                 f'efr_predictor_call: return=c7fa output={0x1514 + sub * 40:04x} '
                 'coefficients=eaaa lag=002d fraction=0003 saved_lag=002d saved_fraction=0003',
                 'efr_predictor: kind=HISTORY values=' +
                 ('0001' if frame else '0000') + '0000' * 153]
        native.extend(group * 2)
        reference.extend((f'EFRPREDINPUT frame={frame} subframe={sub} lag=002d fraction=0003 length=40',
                          f'EFRPREDHISTORY frame={frame} subframe={sub} values=' + '0000' * 154))
    return '\n'.join(native), '\n'.join(reference), controls


class PredictorCheckTests(unittest.TestCase):
    def compare(self, native=None, reference=None):
        good_native, good_reference, controls = fixture()
        with patch.object(check, 'excitation_native', return_value=(controls, {})), \
                patch.object(check, 'excitation_reference', return_value=(controls, {})):
            return check.compare(native if native is not None else good_native,
                                 reference if reference is not None else good_reference, frames=2)

    def test_reports_history_without_conformance_claim(self):
        result = self.compare()
        self.assertEqual(result['matched_lag_fraction_pairs'], 2)
        self.assertEqual(result['history_words'], 308)
        self.assertEqual(result['last'], dict(frame=1, subframe=2, words=1))
        self.assertFalse(result['conformance_claim'])

    def test_changed_replay_and_wrong_pointer_fail(self):
        native, _, _ = fixture()
        with self.assertRaisesRegex(ValueError, 'replay'):
            self.compare(native=native.replace('values=0000', 'values=0001', 1))
        with self.assertRaisesRegex(ValueError, 'ownership'):
            self.compare(native=native.replace('output=1514', 'output=1515'))
        with self.assertRaisesRegex(ValueError, 'ownership'):
            self.compare(native=native.replace('saved_fraction=0003', 'saved_fraction=0000'))

    def test_arguments_must_match_reference(self):
        native, _, _ = fixture()
        with self.assertRaisesRegex(ValueError, 'lag or fraction'):
            self.compare(native=native.replace('lag=002d', 'lag=002e'))

    def test_missing_or_short_reference_history_fails(self):
        _, reference, _ = fixture()
        for bad in (reference.replace('values=', 'values=0000'),
                    reference.replace('length=40', 'length=39')):
            with self.assertRaisesRegex(ValueError, 'order/width'):
                self.compare(reference=bad)
        with self.assertRaisesRegex(ValueError, 'incomplete'):
            self.compare(reference=reference.splitlines()[0])

    def test_argument_domain_includes_fractional_minimum(self):
        check.valid_arguments(17, 3)
        check.valid_arguments(143, 0xfffe)
        for lag, fraction in ((16, 0), (144, 0), (45, 4), (45, 0xfffd)):
            with self.assertRaisesRegex(ValueError, 'decoded contract'):
                check.valid_arguments(lag, fraction)

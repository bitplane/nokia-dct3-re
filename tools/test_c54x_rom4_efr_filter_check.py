import unittest

from tools.c54x_rom4_efr_filter_check import compare, native_records, reference_records, WIDTHS


def fixture():
    native, reference = [], []
    for frame in range(2):
        group = ['efr_boundary: kind=entry']
        for sub in range(4):
            group.append(f'efr_filter_call: return=c890 coefficients={0x1317 + 11 * sub:04x} '
                         f'input=0af2 output={0x1388 + 40 * sub:04x} brc=0013')
            reference.append(f'EFRFILTERCALL frame={frame} subframe={sub} length=40 update=1')
            for kind, width in WIDTHS.items():
                values = '0000' * width
                reference.append(f'EFRFILTER frame={frame} subframe={sub} kind={kind} values={values}')
                if (frame, sub, kind) == (1, 2, 'MEM'):
                    values = '0001' + values[4:]
                group.append(f'efr_filter: kind={kind} values={values}')
        native.extend(group * 2)
    return '\n'.join(native), '\n'.join(reference)


class FilterCheckTests(unittest.TestCase):
    def test_reports_history_difference_without_conformance_claim(self):
        result = compare(*fixture(), frames=2)
        self.assertEqual(result['boundaries']['MEM']['differences'],
                         [dict(frame=1, subframe=2, words=1)])
        self.assertEqual(result['boundaries']['EXC']['differing_words'], 0)
        self.assertEqual(result['filter_calls'], 8)
        self.assertFalse(result['conformance_claim'])

    def test_missing_frame_or_changed_replay_rejected(self):
        native, _ = fixture()
        with self.assertRaisesRegex(ValueError, 'original/replay'):
            native_records(native.rsplit('efr_boundary: kind=entry', 1)[0], frames=2)
        with self.assertRaisesRegex(ValueError, 'replay'):
            native_records(native.replace('kind=A values=0000', 'kind=A values=0001', 1), frames=2)

    def test_wrong_native_call_ownership_rejected(self):
        native, _ = fixture()
        for old, new in (('return=c890', 'return=c5e0'), ('output=1388', 'output=1389'),
                         ('brc=0013', 'brc=0014'), ('input=0af2', 'input=0000')):
            with self.subTest(new=new), self.assertRaisesRegex(ValueError, 'ownership'):
                native_records(native.replace(old, new), frames=2)

    def test_short_or_reordered_native_vector_rejected(self):
        native, _ = fixture()
        with self.assertRaisesRegex(ValueError, 'incomplete filter vector'):
            native_records(native.replace('kind=A values=', 'kind=A values=0000'), frames=2)
        with self.assertRaisesRegex(ValueError, 'unordered'):
            native_records(native.replace('kind=EXC', 'kind=MEM'), frames=2)

    def test_reference_requires_complete_ordered_contract(self):
        _, reference = fixture()
        for old, new in (('length=40', 'length=39'), ('update=1', 'update=0'),
                         ('frame=0 subframe=0 kind=A', 'frame=0 subframe=1 kind=A')):
            with self.subTest(new=new), self.assertRaises(ValueError):
                reference_records(reference.replace(old, new), frames=2)
        with self.assertRaisesRegex(ValueError, 'coverage'):
            reference_records(reference + '\n' + reference.splitlines()[0], frames=2)

import unittest

from tools.run_efr_agc_replay import agc_records, check_chain


def fixture():
    states = [dict(pitch=[4096, 2048, 0, 2049]), dict(pitch=[0] * 4)]
    lines = []
    for frame in range(2):
        group = ['efr_boundary: kind=entry']
        if frame == 0:
            for sub in (0, 3):
                group.append('efr_agc_call: return=c885 '
                             f'input={0x1514 + sub * 40:04x} output=0af2 mode=0001')
                for kind in ('INPUT', 'PRE', 'POST'):
                    group.append(f'efr_agc: kind={kind} values=' + '0000' * 40)
        lines.extend(group * 2)
    return '\n'.join(lines), states


class AgcRecordsTests(unittest.TestCase):
    def test_pitch_threshold_selects_only_executed_calls(self):
        records = agc_records(*fixture())
        self.assertEqual(list(records), [(0, 0), (0, 3)])
        self.assertEqual(records[0, 3]['POST'], (0,) * 40)

    def test_replay_and_ownership_are_checked(self):
        native, states = fixture()
        with self.assertRaisesRegex(ValueError, 'replay'):
            agc_records(native.replace('values=0000', 'values=0001', 1), states)
        for old, new in (('return=c885', 'return=c608'), ('mode=0001', 'mode=0000'),
                         ('input=1514', 'input=1515')):
            with self.subTest(new=new), self.assertRaisesRegex(ValueError, 'ownership'):
                agc_records(native.replace(old, new), states)

    def test_missing_short_or_unadmitted_calls_fail(self):
        native, states = fixture()
        with self.assertRaisesRegex(ValueError, 'width'):
            agc_records(native.replace('values=', 'values=0000'), states)
        with self.assertRaisesRegex(ValueError, 'admission'):
            agc_records(native, [dict(pitch=[4096, 4096, 0, 2049]), states[1]])
        with self.assertRaisesRegex(ValueError, 'frames'):
            agc_records(native.rsplit('efr_boundary: kind=entry', 1)[0], states)

    def test_input_output_and_bypass_chain(self):
        total, normalized = (1,) * 40, (2,) * 40
        totals = [[total] * 4]
        records = {(0, 0): dict(INPUT=total, POST=normalized)}
        filters = {(0, sub, 'EXC'): normalized if sub == 0 else total for sub in range(4)}
        self.assertEqual(check_chain(records, filters, totals),
                         dict(normalized_calls=1, bypass_calls=3))
        for sub in range(4):
            changed = dict(filters)
            changed[0, sub, 'EXC'] = (99,) * 40
            with self.assertRaisesRegex(ValueError, 'synthesis input'):
                check_chain(records, changed, totals)
        with self.assertRaisesRegex(ValueError, 'AGC input'):
            check_chain({(0, 0): dict(INPUT=normalized, POST=normalized)}, filters, totals)

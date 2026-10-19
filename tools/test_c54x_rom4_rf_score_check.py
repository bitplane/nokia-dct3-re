import unittest

from tools.c54x_rom4_rf_score_check import check, observe, check_window, observe_execution, check_execution


ZERO = "rom4_rf_score_summary: mode=zero reads=900000 results=5000 nonzero=0 maximum=0000000000 second=0 accepted=0 acquisition_claim=0"
TONE = "rom4_rf_score_summary: mode=tone reads=900000 results=5000 nonzero=5000 maximum=0000007fff second=400 accepted=8 acquisition_claim=0"


class ScoreTests(unittest.TestCase):
    def execution(self, mode='tone', value=0x7fff, advance=True):
        rows = ['rom4_rf_execution_probe: points=0ad1,0add,0ae4 phase=instruction_entry']
        for _ in range(1000):
            rows.append(f'rom4_rf_execute: pc=0ad1 a={value:010x}')
            if advance:
                rows += ['rom4_rf_execute: pc=0add a=0000000000', 'rom4_rf_execute: pc=0ae4 a=0000000000']
        return '\n'.join(rows + [TONE if mode == 'tone' else ZERO])

    def test_execution_uses_entries_not_fetch_snapshot_scores(self):
        text = self.execution()
        sample = observe_execution(text)
        self.assertEqual((sample['results'], sample['second'], sample['accepted']), (1000, 1000, 1000))
        self.assertEqual(check_execution(text), ('tone', 1000, 1000))
        self.assertEqual(check_execution(self.execution('zero', 0, False)), ('zero', 1000, 0))

    def test_execution_rejects_missing_duplicate_malformed_and_unordered(self):
        text = self.execution()
        marker = 'rom4_rf_execution_probe: points=0ad1,0add,0ae4 phase=instruction_entry'
        for bad in (TONE, text + '\n' + marker, text.replace('pc=0ad1', 'pc=ffff', 1),
                    text.replace('pc=0ad1', 'pc=0ae4', 1),
                    text.replace('pc=0add', 'pc=0ad1', 1)):
            with self.subTest(bad=bad[:120]), self.assertRaises(ValueError):
                observe_execution(bad)

    def test_execution_characterization_does_not_relax_acceptance(self):
        text = self.execution(value=0x538, advance=False)
        self.assertEqual(observe_execution(text)['maximum'], 0x538)
        with self.assertRaisesRegex(ValueError, 'accepted-value store'):
            check_execution(text)
        with self.assertRaisesRegex(ValueError, 'silent/DC'):
            check_execution(self.execution('zero', value=10, advance=False))

    def test_window_observation_does_not_require_or_promote_acceptance(self):
        text = TONE.replace('0000007fff', '0000000538').replace('second=400', 'second=0').replace('accepted=8', 'accepted=0')
        text += '\nrom4_rf_window_summary: writes=5 transitions=2 reads_on=800000 reads_off=100000 physical_gate_claim=0'
        text += '\nrom4_rf_window_mode: mode=0000 reads_on=0 reads_off=100000'
        text += '\nrom4_rf_window_mode: mode=0001 reads_on=800000 reads_off=0'
        text += '\nrom4_rf_window_state: mode=0000 control=0000 reads=100000 aged=90000 minimum_us=2 maximum_us=5000'
        text += '\nrom4_rf_window_state: mode=0001 control=0005 reads=800000 aged=0 minimum_us=0 maximum_us=0'
        self.assertEqual(check_window(text)['modes'][1], dict(on=800000, off=0))
        self.assertFalse(check_window(text)['physical_gate_claim'])
        with self.assertRaises(ValueError):
            check(text)
        for bad in ('', text + '\nrom4_rf_window_summary: writes=5 transitions=2 reads_on=800000 reads_off=100000 physical_gate_claim=0',
                    text.replace('writes=5', 'writes=1'), text.replace('transitions=2', 'transitions=6'),
                    text.replace('reads=900000', 'reads=900001'), text.replace('mode=0001', 'mode=0000'),
                    text.replace('mode=0001 reads_on=800000', 'mode=0001 reads_on=799999'),
                    text.replace('reads=100000 aged=90000', 'reads=99999 aged=90000'),
                    text.replace('aged=90000', 'aged=100001'), text.replace('minimum_us=2', 'minimum_us=5001'),
                    text.replace('aged=0 minimum_us=0', 'aged=1 minimum_us=0'),
                    text.replace('maximum_us=0', 'maximum_us=1'),
                    text.replace('physical_gate_claim=0', 'physical_gate_claim=1')):
            with self.subTest(bad=bad[-100:]), self.assertRaises(ValueError):
                check_window(bad)

    def test_failed_acceptance_still_has_an_unpromoted_observation(self):
        text = TONE.replace('0000007fff', '0000000538').replace('second=400', 'second=0').replace('accepted=8', 'accepted=0')
        self.assertEqual(observe(text)['maximum'], 0x538)
        self.assertEqual(observe(text)['accepted'], 0)
        with self.assertRaisesRegex(ValueError, 'accepted-value store'):
            check(text)
        for invalid in ('', text + '\n' + text):
            with self.assertRaises(ValueError):
                observe(invalid)

    def test_controls(self):
        self.assertEqual(check(ZERO), ("zero", 5000, 0))
        self.assertEqual(check(ZERO.replace("mode=zero", "mode=dc")), ("dc", 5000, 0))
        self.assertEqual(check(TONE), ("tone", 5000, 8))

    def test_reject_insufficient_inconsistent_and_duplicate(self):
        for text in ("", ZERO + "\n" + ZERO, ZERO.replace("results=5000", "results=1"),
                     ZERO.replace("nonzero=0", "nonzero=1"), TONE.replace("accepted=8", "accepted=0"),
                     TONE.replace("accepted=8", "accepted=401"), TONE.replace("nonzero=5000", "nonzero=4999"),
                     TONE.replace("0000007fff", "0000000574")):
            with self.subTest(text=text), self.assertRaises(ValueError):
                check(text)


if __name__ == "__main__":
    unittest.main()

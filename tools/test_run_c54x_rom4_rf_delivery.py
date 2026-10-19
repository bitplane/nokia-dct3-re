import unittest
import hashlib
from pathlib import Path
import tempfile
from unittest.mock import patch

from tools.run_c54x_rom4_rf_delivery import check_repeats, input_provenance, inspect_case, machine_profile, check_cpu_health
from tools.test_c54x_rom4_rf_score_check import TONE


class RepeatTests(unittest.TestCase):
    def test_cpu_stop_and_wrong_profile_are_not_rf_input_failures(self):
        text = 'rom4_rf_profile: system=nse1pipe\nrom4_rf_cpu_state: illegal=0 pc=47f8\n'
        self.assertEqual(check_cpu_health(text, 'nse1pipe')['pc'], '47f8')
        for bad in ('', text + text, text.replace('illegal=0', 'illegal=1'),
                    text.replace('system=nse1pipe', 'system=noki5110')):
            with self.subTest(text=bad), self.assertRaises(ValueError):
                check_cpu_health(bad, 'nse1pipe')

    def test_machine_profiles_keep_frontend_and_bus_dialect_distinct(self):
        for machine, frontend, bus in (
            ('noki5110', 'atomic', 'legacy'), ('nse1astd', 'atomic', 'standard'),
            ('nse1plg', 'staged', 'legacy'), ('nse1pipe', 'staged', 'standard')):
            with self.subTest(machine=machine):
                self.assertEqual(machine_profile(machine), dict(machine=machine,
                    frontend=frontend, swwsr_model=bus, physical_dialect_claim=False))
        with self.assertRaises(KeyError):
            machine_profile('noki3310')

    def case_checks(self, **overrides):
        functions = dict(check_observer_health=lambda text: None,
                         check_cpu_health=lambda text, machine: {'machine': machine, 'illegal': False},
                         check_window=lambda text: {'reads_on': 800000, 'reads_off': 100000, 'physical_gate_claim': False},
                         check_consumption=lambda text, scan=False: {'scan': scan, 'reads': 64},
                         check_execution_scan=lambda text: {'entries': 64},
                         check_delivery=lambda text, **kwargs: ('00101438', '006d'),
                         check_request=lambda text: 109,
                         check_tuning_upload=lambda text: (),
                         check_execution_tuning=lambda text: 0x00302813,
                         nse1_gsm900_tuning_consistency=lambda words: {'channel': 109},
                         check_tuning_blob=lambda text: 0x299894)
        return patch.multiple('tools.run_c54x_rom4_rf_delivery', **(functions | overrides))

    def test_independent_results_survive_rejected_score(self):
        text = TONE.replace('0000007fff', '0000000538').replace('second=400', 'second=0').replace('accepted=8', 'accepted=0')
        with self.case_checks():
            report = inspect_case(text, '', 'tone')
        self.assertEqual(report['status'], 'failed')
        self.assertEqual(set(report['failures']), {'score_acceptance'})
        self.assertEqual(report['score']['maximum'], 0x538)
        self.assertEqual(report['native_body'], '006d')
        self.assertEqual(report['checks']['scan_consumption']['value'], {'scan': True, 'reads': 64})
        self.assertEqual(report['checks']['tuning']['status'], 'pass')

    def test_staged_fetch_snapshots_cannot_be_promoted_to_scores(self):
        with self.case_checks():
            report = inspect_case(TONE, '', 'tone', 'nse1pipe')
        self.assertEqual(set(report['failures']), {'score_observation', 'score_acceptance'})
        self.assertIn('execution-owned', report['failures']['score_observation'])
        self.assertNotIn('score', report)
        self.assertEqual(report['checks']['delivery']['status'], 'pass')

    def test_staged_execution_probes_use_the_separate_score_parser(self):
        sample = dict(mode='tone', reads=900000, results=5000, nonzero=5000,
                      maximum=0x7fff, second=400, accepted=8)
        with self.case_checks(observe_execution=lambda text: sample,
                              check_execution=lambda text: ('tone', 5000, 8)):
            report = inspect_case(TONE, '', 'tone', 'nse1pipe', execution_probes=True)
        self.assertEqual(report['status'], 'pass')
        self.assertEqual(report['score'], sample)

    def test_passing_case_keeps_all_checks(self):
        with self.case_checks():
            report = inspect_case(TONE, '', 'tone')
        self.assertEqual(report['status'], 'pass')
        self.assertFalse(report['failures'])
        self.assertEqual(len(report['checks']), 11)
        self.assertEqual(report['results'], 5000)
        self.assertEqual(report['accepted'], 8)

    def test_window_failure_does_not_discard_delivery(self):
        def failed(text):
            raise ValueError('window counts differ')
        with self.case_checks(check_window=failed):
            report = inspect_case(TONE, '', 'tone')
        self.assertEqual(set(report['failures']), {'window_observation'})
        self.assertEqual(report['checks']['delivery']['status'], 'pass')
        self.assertEqual(report['status'], 'failed')

    def test_mode_and_correlations_cannot_pass_silently(self):
        with self.case_checks(check_request=lambda text: 110,
                              nse1_gsm900_tuning_consistency=lambda words: {'channel': 111}):
            report = inspect_case(TONE, '', 'zero')
        self.assertEqual(report['status'], 'failed')
        self.assertEqual(set(report['failures']), {'mode', 'request_correlation', 'tuning_correlation'})

    def test_missing_summary_does_not_discard_other_failures_or_results(self):
        def failed(text):
            raise ValueError('observer failed')
        with self.case_checks(check_observer_health=failed):
            report = inspect_case('', '', 'tone')
        self.assertEqual(set(report['failures']), {'observer_health', 'score_observation', 'score_acceptance'})
        self.assertEqual(report['checks']['delivery']['status'], 'pass')
        self.assertNotIn('results', report)

    def test_provenance_hashes_actual_bytes_and_marks_missing_inputs(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'input.bin'
            path.write_bytes(b'own ROM input')
            result = input_provenance({'present': path, 'missing': path.with_name('missing')})
            self.assertEqual(result['present']['sha256'],
                             hashlib.sha256(b'own ROM input').hexdigest())
            self.assertEqual(result['present']['bytes'], 13)
            self.assertEqual(result['present']['path'], str(path.resolve()))
            self.assertFalse(result['missing']['present'])
            self.assertNotIn('sha256', result['missing'])

    def case(self, mode="tone", **changes):
        return dict(mode=mode, results=5146, accepted=9,
                    object="00101438", native_body="006d") | changes

    def test_identical_repeats(self):
        check_repeats([self.case(), self.case()])

    def test_modes_are_independent(self):
        check_repeats([self.case(), self.case("zero", results=5158, accepted=0)])

    def test_each_observable_must_repeat(self):
        for key, value in (("results", 5556), ("accepted", 8),
                           ("object", "00101480"), ("native_body", "0001")):
            with self.subTest(key=key), self.assertRaises(ValueError):
                check_repeats([self.case(), self.case(**{key: value})])


if __name__ == "__main__":
    unittest.main()

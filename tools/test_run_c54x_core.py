"""Runner acceptance grammar, not CPU arithmetic or silicon conformance.

Profile-specific expectations belong to c54x_test_oracles; test the common
validator against every declared profile instead of restating each contract.
"""
from itertools import combinations
from pathlib import Path
import re
import unittest

from tools.c54x_test_reviews import reviewed_profiles
from tools.run_c54x_core import (
    COMPLETION, EXTRA_PROFILES, NAMED_SCOPES, REQUIRED, check_result,
)


def scopes():
    for profile, (markers, completion) in EXTRA_PROFILES.items():
        yield profile, {'profile': profile}, markers, completion
    for flag, (profile, markers, completion) in NAMED_SCOPES.items():
        yield flag, {flag: True}, markers, completion


def transcript(name, markers, completion):
    markers = (markers,) if isinstance(markers, str) else markers
    review = reviewed_profiles().get(name, {})
    ids = [f'C54x ISA case: PASS id={case}' for case in review.get('case_ids', [])]
    return markers, '\n'.join([*ids, *markers, completion]) + '\n'


class IsolatedResultTests(unittest.TestCase):
    def test_every_declared_scope_accepts_its_complete_result(self):
        for name, kwargs, markers, completion in scopes():
            with self.subTest(scope=name):
                _, output = transcript(name, markers, completion)
                check_result(output, 3, **kwargs)

    def test_every_marker_is_required_exactly_once(self):
        for name, kwargs, markers, completion in scopes():
            markers, output = transcript(name, markers, completion)
            for marker in markers:
                for mutation in ('missing', 'duplicate', 'extra detail'):
                    replacement = {'missing': '', 'duplicate': marker + '\n' + marker + '\n',
                                   'extra detail': marker + ' unexpected=1\n'}[mutation]
                    bad = output.replace(marker + '\n', replacement)
                    with self.subTest(scope=name, marker=marker, mutation=mutation):
                        with self.assertRaises(ValueError):
                            check_result(bad, 3, **kwargs)

    def test_every_declared_field_is_checked(self):
        for name, kwargs, markers, completion in scopes():
            markers, output = transcript(name, markers, completion)
            for marker in markers:
                for field in marker.split():
                    if '=' not in field:
                        continue
                    bad_marker = marker.replace(field, field.split('=')[0] + '=invalid')
                    with self.subTest(scope=name, field=field), self.assertRaises(ValueError):
                        check_result(output.replace(marker, bad_marker), 3, **kwargs)

    def test_completion_is_required_and_terminal(self):
        for name, kwargs, markers, completion in scopes():
            _, output = transcript(name, markers, completion)
            for bad in (output.replace(completion, ''), output + 'late output\n',
                        output.replace(completion, COMPLETION),
                        'Fatal error: unexpected failure\n' + output):
                with self.subTest(scope=name, tail=bad[-100:]), self.assertRaises(ValueError):
                    check_result(bad, 3, **kwargs)

    def test_isolated_result_cannot_satisfy_default(self):
        for name, _, markers, completion in scopes():
            _, output = transcript(name, markers, completion)
            with self.subTest(scope=name), self.assertRaises(ValueError):
                check_result(output, 3)

    def test_one_scope_at_a_time(self):
        choices = [{flag: True} for flag in NAMED_SCOPES]
        choices.append({'profile': 'source-domain'})
        for left, right in combinations(choices, 2):
            with self.subTest(left=left, right=right), self.assertRaises(ValueError):
                check_result('', 3, **left, **right)
        with self.assertRaisesRegex(ValueError, 'unknown isolated profile'):
            check_result('', 3, profile='unknown')

    def test_same_family_profiles_cannot_substitute_for_each_other(self):
        for left, right in (('mvmd-ar', 'mvdm-ar'), ('mvkd-delay', 'mvdm-delay'),
                            ('far-call-delay', 'mvdm-delay')):
            for source, target in ((left, right), (right, left)):
                _, output = transcript(source, *EXTRA_PROFILES[source])
                with self.subTest(source=source, target=target), self.assertRaises(ValueError):
                    check_result(output, 3, profile=target)

    def test_fixture_bios_metadata_fits_mame_encoding(self):
        source = (Path(__file__).resolve().parents[1] / 'tests/c54x/tms320c54x_test.cpp').read_text()
        blocks = re.findall(r'ROM_START\(([^)]+)\)(.*?)ROM_END', source, re.S)
        self.assertTrue(blocks)
        for driver, body in blocks:
            entries = re.findall(r'ROM_SYSTEM_BIOS\((\d+),\s*"([^"]+)"', body)
            indices = [int(index) for index, _ in entries]
            names = [name for _, name in entries]
            with self.subTest(driver=driver):
                self.assertEqual(len(indices), len(set(indices)))
                self.assertEqual(len(names), len(set(names)))
                # ROM_BIOS encodes index + 1 in eight bits; 255 wraps to zero.
                self.assertTrue(all(0 <= index < 255 for index in indices))
                self.assertTrue(all(0 < len(name) <= 16 for name in names))


class DefaultResultTests(unittest.TestCase):
    def setUp(self):
        self.output = '\n'.join([*REQUIRED, COMPLETION]) + '\n'

    def test_complete(self):
        check_result(self.output, 3)

    def test_default_prefix_markers_allow_detail(self):
        check_result(self.output.replace(
            'stack address latency conformance: PASS\n',
            'stack address latency conformance: PASS variants=24\n'), 3)

    def test_each_marker_required_once(self):
        for marker in REQUIRED:
            for bad in (self.output.replace(marker + '\n', ''), marker + '\n' + self.output):
                with self.subTest(marker=marker), self.assertRaises(ValueError):
                    check_result(bad, 3)

    def test_interrupt_checkpoint_requires_both_order_claims(self):
        for field in (' intm_at_iack=1', ' decode_before_access=1'):
            self.assertIn(field, self.output)
            with self.subTest(field=field), self.assertRaises(ValueError):
                check_result(self.output.replace(field, ''), 3)

    def test_completion_must_be_terminal(self):
        for bad in (self.output.replace(COMPLETION, ''), self.output + 'crash\n',
                    'Fatal error: unexpected failure\n' + self.output):
            with self.assertRaises(ValueError):
                check_result(bad, 3)

    def test_wrong_exit(self):
        for status in (0, 1, -11):
            with self.subTest(status=status), self.assertRaises(ValueError):
                check_result(self.output, status)

    def test_failure_diagnostics_override_complete_markers(self):
        failures = ('c54x_failure: phase=1 case=0\n',
                    'Invalid BIOS selection\n', 'Reverting to default BIOS\n',
                    'Errors:\n BIOS name exceeds maximum 16 characters\n')
        _, isolated = transcript('conditional-ret', *EXTRA_PROFILES['conditional-ret'])
        for prefix in failures:
            for kwargs, output in (({}, self.output), ({'profile': 'conditional-ret'}, isolated)):
                with self.subTest(prefix=prefix, kwargs=kwargs), self.assertRaises(ValueError):
                    check_result(prefix + output, 3, **kwargs)

    def test_verbose_teardown_is_exact_and_opt_in(self):
        suffix = 'Enter sdlwindow_exit\nLeave sdlwindow_exit\n'
        check_result(self.output + suffix, 3, verbose_teardown=True)
        check_result(self.output, 3, verbose_teardown=True)
        with self.assertRaises(ValueError):
            check_result(self.output + suffix, 3)
        for bad in (self.output + suffix + 'crash\n',
                    self.output + 'unexpected\n' + suffix,
                    self.output + 'Enter sdlwindow_exit\n',
                    self.output.replace(COMPLETION, '') + suffix,
                    self.output.replace(REQUIRED[0] + '\n', '') + suffix):
            with self.subTest(tail=bad[-100:]), self.assertRaises(ValueError):
                check_result(bad, 3, verbose_teardown=True)
        with self.assertRaises(ValueError):
            check_result(self.output + suffix, 0, verbose_teardown=True)


if __name__ == '__main__':
    unittest.main()

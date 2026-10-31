"""Firmware result grammar; generic CPU acceptance belongs to section execution."""
import unittest
from tools.run_c54x_core import EXTRA_PROFILES, check_result


class FirmwareResultTests(unittest.TestCase):
    def test_exact_results_and_rejection_controls(self):
        for profile, (marker, completion) in EXTRA_PROFILES.items():
            valid = marker + '\n' + completion + '\n'
            with self.subTest(profile=profile):
                check_result(valid, 3, profile=profile)
                check_result(valid + 'Enter sdlwindow_exit\nLeave sdlwindow_exit\n',
                             3, profile=profile, verbose_teardown=True)
                for text, status in (
                    (valid, 0), (completion, 3), (marker + '\n' + valid, 3),
                    (valid + 'late output\n', 3), (valid.replace(marker, marker + ' wrong=1'), 3),
                    ('c54x_failure: broken\n' + valid, 3),
                    ('invalid BIOS\n' + valid, 3),
                    ('Driver test: 1 errors,\n' + valid, 3),
                    ('Fatal error: unrelated\n' + valid, 3),
                ):
                    with self.subTest(text=text, status=status), self.assertRaises(ValueError):
                        check_result(text, status, profile=profile)

    def test_profiles_have_unique_declared_owners(self):
        from tools.c54x_profiles import machine_for_profile
        self.assertEqual({name: machine_for_profile(name) for name in EXTRA_PROFILES},
                         {'rom4-context': 'tms54test', 'cold-pipeline': 'tms54rom4'})

    def test_retired_generic_profile_is_not_accepted(self):
        with self.assertRaises(ValueError):
            check_result('', 3, profile='source-domain')


if __name__ == '__main__':
    unittest.main()

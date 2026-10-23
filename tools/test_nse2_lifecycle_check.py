import unittest

from tools.nse2_lifecycle_check import diagnose


def transcript():
    return '\n'.join((
        'nse2_native_identity: n=1 data=0004 mask=ffff pc=0f0f t=0.007 observer_writes=0',
        'nse2_reset_reason: value=68 pc=0028db6e lr=00290361 t=31.0008 observer_writes=0',
        'nse2_reset_entry: reason=00000068 lr=0023616d stack=00000000,000000d5 t=31.9694 observer_writes=0',
        'nse2_native_identity: n=2 data=0004 mask=ffff pc=0f0f t=31.9748 observer_writes=0'))


class LifecycleTest(unittest.TestCase):
    def test_reset_is_reported_as_failure_not_stable_boot(self):
        result = diagnose(transcript())
        self.assertFalse(result['stable_long_boot'])
        self.assertEqual(result['native_startups'], 2)

    def test_wrong_owner_missing_reboot_and_wrong_order_fail(self):
        for old, new in (('lr=00290361', 'lr=00290363'), ('n=2 data=0004', 'n=2 data=0000'),
                         ('t=31.9748', 't=30.1'), ('000000d5', '000000d4')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                diagnose(transcript().replace(old, new))


if __name__ == '__main__':
    unittest.main()

import unittest
from tools.nsb7_boot_check import check_trace


def trace():
    return 'nsb7_profile: machine=noki8290 observer_writes=0\n' + '\n'.join(f'nsb7_boot: pc=002f8300 lr=00304e35 cpsr=2000003f '
                     f'reset=02 ctrl=05 gensio=03 shared=0000,ffff,0001,0001 '
                     f't={time:.9f} observer_writes=0' for time in [0.25, 1, 3, 5, 6])


class BootCheckTests(unittest.TestCase):
    def test_complete_observation(self):
        check_trace(trace())

    def test_reject_missing_or_repeated_sample(self):
        for text in ('\n'.join(trace().splitlines()[:-1]), trace() + '\n' + trace().splitlines()[0]):
            with self.assertRaises(ValueError):
                check_trace(text)

    def test_reject_changed_boundary(self):
        for old, new in [('ctrl=05', 'ctrl=0a'), ('0000,ffff', '0000,0006'),
                         ('machine=noki8290', 'machine=nsb7hle'),
                         ('pc=002f8300', 'pc=00200040'), ('6.000000000', '6.100000000')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_trace(trace().replace(old, new))


if __name__ == '__main__':
    unittest.main()

import unittest
from tools.nsb7_service_check import check_trace


def fixture():
    return '\n'.join([
        'nsb7_profile: machine=nsb7hle observer_writes=0',
        'loader2_verified words=623 entry=0a00',
        'runtime_hle_handoff pc=2c75 native_suspended=1',
        'TX pending type=05 payload=10 data=1eff00d000030101e000',
        'RX enqueue type=8e payload=10 producer=08c data=1e0002d000030401c100',
        'TX pending type=70 payload=2 data=0d00',
        'RX enqueue type=74 payload=2 producer=08e data=0d00',
        'nsb7_selftest: pc=00248b74 command=0d reply=00 flags=84 faults=ff,ff,ff',
        'nsb7_selftest: pc=00248c28 command=0d reply=00 flags=80 faults=00,00,00',
        'TX pending type=70 payload=2 data=0a09',
        'nsb7_calibration_compare: computed=bd42 stored=ffff marker=ffff',
        'nsb7_fault12: pc=00248948 value=0c mask=ff000000',
        'nsb7_prefix_compare: computed=00001ae4 stored=ffffffff',
        'nsb7_result: index=12 pc=002e8ffc value=fd mask=0000ff00',
        'nsb7_status: flags=00 faults=ffff00ff00ff000000ffff000cff00000000120000ffffffffffffffff t=6.000000000'])


class ServiceCheckTests(unittest.TestCase):
    def test_complete_baseline(self):
        check_trace(fixture())

    def test_reject_wrong_profile_result_or_fault_owner(self):
        for old, new in [('machine=nsb7hle', 'machine=noki8290'), ('flags=80', 'flags=84'),
                         ('faults=00,00,00', 'faults=00,10,00'), ('stored=ffff', 'stored=bd42'),
                         ('pc=00248948', 'pc=00200040'), ('words=623', 'words=638')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_trace(fixture().replace(old, new))

    def test_reject_missing_or_reordered_response(self):
        lines = fixture().splitlines()
        del lines[6]
        with self.assertRaises(ValueError):
            check_trace('\n'.join(lines))
        with self.assertRaises(ValueError):
            check_trace('\n'.join(fixture().splitlines()[::-1]))

    def test_reject_partial_startup_result_table(self):
        with self.assertRaises(ValueError):
            check_trace(fixture().replace('00120000ffffffffffffffff t=', ' t='))


if __name__ == '__main__':
    unittest.main()

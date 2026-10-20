import unittest
from tools.nsm1_liveness_check import check_trace, source_trace


def trace():
    rows = []
    for index, count in enumerate((0, 1, 0)):
        cycle = 100 + index * 100
        rows.append(f'nsm1_liveness_window: counter={count:04x} context=00112860 cycles={cycle:016x} observer_writes=0')
        if count:
            continue
        rows += [f'nsm1_liveness_entry: argument=00000001 caller=002c1b95 cycles={cycle+1:016x} observer_writes=0',
                 f'nsm1_liveness_config: argument=00000001 bytes=0002 cycles={cycle+2:016x} observer_writes=0',
                 f'nsm1_liveness_exit: reason=00000000 cycles={cycle+3:016x} observer_writes=0']
    return '\n'.join(rows)


class Nsm1LivenessTests(unittest.TestCase):
    def test_record_source_requires_matched_serial_read(self):
        rows = []
        for index in range(3):
            cycle = 100 + 100 * index
            caller = '002b4d21' if index == 0 else '002b4b8b'
            rows += [f'nsm1_liveness_record: record=0607 pointer=0011335c caller={caller} cycles={cycle:016x} observer_writes=0',
                     f'nsm1_liveness_serial: offset=03ca pointer=0011335c length=0001 cycles={cycle+1:016x} observer_writes=0']
            if index:
                rows.append(f'nsm1_liveness_read_result: result=00000001 byte=00 cycles={cycle+2:016x} observer_writes=0')
        value = '\n'.join(rows)
        source_trace(value)
        for bad in (value.replace('offset=03ca', 'offset=03cb', 1),
                    value.replace('length=0001', 'length=0002', 1),
                    value.replace('byte=00', 'byte=01', 1), value + '\nnsm1_liveness_serial: malformed'):
            with self.assertRaises(ValueError):source_trace(bad)

    def test_empty_windows_are_suppressed_by_observed_policy(self):
        check_trace(trace())

    def test_changed_policy_missing_window_and_restart_rejected(self):
        value = trace()
        for bad in (value.replace('bytes=0002', 'bytes=0102', 1),
                    value.replace('counter=0001', 'counter=0000'),
                    value.replace('caller=002c1b95', 'caller=002ab50d'),
                    value + '\nnsm1_native_long_restart: reason=00000068',
                    value + '\nnsm1_liveness_config: malformed'):
            with self.subTest(value=bad), self.assertRaises(ValueError):
                check_trace(bad)


if __name__ == '__main__':
    unittest.main()

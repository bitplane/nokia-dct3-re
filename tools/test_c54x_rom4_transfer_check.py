import unittest

from tools.c54x_rom4_transfer_check import check


def fixture():
    controls, tables, pairs, timers = [], [], [], []
    positions = [2100, 2180, 2260, 2340, 2420, 15000]
    descriptor = ','.join(f'{value:04x}' for pair in zip([5, 1, 1, 1, 1, 0], positions) for value in pair)
    payload = [0x2a04, 6, 0x41, 0x40, 0x27a2, 0x30, 0x41, 0x20, 0x488, 0x10, 0, 0x50]
    for burst in range(4):
        base = burst + 1.0
        tables.append(f"rom4_transfer_table: descriptors={descriptor} payload={','.join(f'{value:04x}' for value in payload)} t={base:.9f}")
        pairs.append(f'rom4_transfer_pair: sequence={len(pairs) + 1} low=2a04 high=0006 pc=a240 t={base - .000001:.9f}')
        for index, position in enumerate(positions):
            time = base + index * .0001
            pointers = '1914,1920' if not index else f'{0x1914 + index * 2:04x},{0x1922 + index * 2:04x}'
            pc = 'a244' if not index else '3717'
            controls.append(f'rom4_transfer_control: sequence={len(controls) + 1} value={position:04x} mask=ffff pc={pc} '
                            f'flags=0040,0010 pointers={pointers} descriptor=0005,0834,0001,0884 counter=0000 counter_age=0.1 t={time:.9f}')
            if index == 5:
                continue
            timers.append(f'rom4_transfer_timer: expiry={len(timers) + 1} target={position:04x} counter={position:04x} delay=80 t={time + .00005:.9f}')
            low, high = payload[index * 2 + 2:index * 2 + 4]
            pairs.append(f'rom4_transfer_pair: sequence={len(pairs) + 1} low={low:04x} high={high:04x} pc=3712 t={time + .00006:.9f}')
    inventory = [f'rom4_transfer_pair_count: pc={pc} low={low:04x} high={high:04x} count=4'
                 for pc, low, high in [('a240', payload[0], payload[1])] +
                 [('3712', payload[i], payload[i + 1]) for i in range(2, 12, 2)]]
    return '\n'.join(controls + tables + pairs + timers + inventory + [
        'rom4_transfer_request_count: value=1000 count=20',
        'rom4_transfer_summary: controls=24 requests=20 service_requests=20 pairs=24 completion_injected=0 t=8.0',
        'rom4_interface_summary: transfer_expiries=20',
    ])


class TransferTests(unittest.TestCase):
    def test_four_native_chains_are_correlated_without_rf_claim(self):
        report = check(fixture())
        self.assertEqual(len(report['bursts']), 4)
        self.assertEqual(report['queued_pairs'], 20)
        self.assertFalse(report['physical_timing_claim'])
        self.assertFalse(report['acquisition_claim'])
        self.assertEqual(report['pairs_by_producer'], {'a240': 4, '3712': 20})

    def test_inventory_and_requested_duration_cannot_be_truncated(self):
        text = fixture()
        line = 'rom4_transfer_pair_count: pc=a240 low=2a04 high=0006 count=4'
        for changed in (text.replace(line, ''), text + '\n' + line,
                        text.replace(line, line.replace('count=4', 'count=0')),
                        text.replace(line, line.replace('pc=a240', 'pc=4028'))):
            with self.assertRaises(ValueError):
                check(changed)
        with self.assertRaisesRegex(ValueError, 'complete transfer summary'):
            check(text, minimum_seconds=30)

    def test_missing_or_truncated_accounting_fails(self):
        text = fixture()
        for first, second in [('controls=24', 'controls=25'), ('requests=20', 'requests=21'),
                              ('pairs=24', 'pairs=25'), ('t=8.0', 't=7.0'),
                              ('transfer_expiries=20', 'transfer_expiries=21'),
                              ('completion_injected=0', 'completion_injected=1')]:
            with self.subTest(first=first), self.assertRaises(ValueError):
                check(text.replace(first, second))

    def test_wrong_payload_or_producer_fails(self):
        for first, second in [('low=0041', 'low=0042'), ('pc=3712', 'pc=4028'),
                              ('low=2a04', 'low=2a05'), ('pc=a240', 'pc=3712')]:
            with self.subTest(first=first), self.assertRaises(ValueError):
                check(fixture().replace(first, second))

    def test_descriptor_rearm_pointer_and_sentinel_are_checked(self):
        for first, second in [('value=0884', 'value=0885'), ('value=3a98', 'value=0000'),
                              ('pointers=1916,1924', 'pointers=1916,1922'),
                              ('pointers=1914,1920', 'pointers=1916,1920')]:
            with self.subTest(first=first), self.assertRaises(ValueError):
                check(fixture().replace(first, second))

    def test_timer_cannot_be_missing_late_or_at_the_wrong_counter(self):
        for first, second in [('counter=0834 delay', 'counter=0833 delay'),
                              ('target=0834', 'target=0884'),
                              ('t=1.000050000', 't=1.000070000')]:
            with self.subTest(first=first), self.assertRaises(ValueError):
                check(fixture().replace(first, second))

    def test_dropped_duplicated_and_unmodeled_request_records_fail(self):
        text = fixture()
        for line in (text.splitlines()[0], next(line for line in text.splitlines() if line.startswith('rom4_transfer_table:')),
                     next(line for line in text.splitlines() if line.startswith('rom4_transfer_timer:'))):
            with self.subTest(line=line), self.assertRaises(ValueError):
                check(text.replace(line, '', 1))
        with self.assertRaises(ValueError):
            check(text + '\nrom4_transfer_request_count: value=1000 count=20')
        with self.assertRaisesRegex(ValueError, 'unmodeled request'):
            check(text.replace('value=1000 count=20', 'value=9000 count=20'))


if __name__ == '__main__':
    unittest.main()

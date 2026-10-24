import unittest

from tools.c54x_rom4_rf_consumers_check import FAMILIES
from tools.nsb3_rf_consumers_check import check_consumers


def consumer_trace():
    groups = {
        'initial_filter': [('0000', '0000', 24568), ('0000', '0100', 8),
                           ('0004', '0104', 21768), ('0004', '0105', 76544),
                           ('0009', '0105', 16), ('000c', '0105', 38272)],
        'unfiltered_history': [('0001', '0105', 24), ('0002', '0105', 8),
                               ('0004', '0105', 38272), ('0009', '0105', 24),
                               ('000c', '0105', 57408)],
        'scan_filter': [('0001', '0105', 16128)],
        'capture_filter': [('0002', '0105', 32)],
    }
    rows = []
    for family, sites in FAMILIES.items():
        for site in sites:
            for mode, control, count in groups[family]:
                rows.append(f'rom4_rf_consumer: xpc=00 pc={site+2:04x} mode={mode} control={control} '
                            f'holdoff={int(family == "unfiltered_history")} count={count} first=0.2 last=59.99')
    return '\n'.join(rows + [
        'rom4_rf_consumer_summary: reads=1092288 buckets=52 physical_rate_claim=0 t=60.016666667',
        'rom4_interface_summary: frame_expiries=34170 rf_reads=1092288'])


class Nsb3RfConsumersTest(unittest.TestCase):
    def test_complete_accounting_has_no_acquisition_claim(self):
        report = check_consumers(consumer_trace())
        self.assertEqual(report['reads'], 1092288)
        self.assertFalse(report['physical_rate_claim'])
        self.assertFalse(report['acquisition_claim'])

    def test_missing_unknown_duplicated_faulted_and_mismatched_rows_fail(self):
        good = consumer_trace()
        for bad in (good.replace('pc=330c', 'pc=330d'),
                    good.replace('count=16128', 'count=16127', 1),
                    good.replace('xpc=00', 'xpc=01', 1),
                    good + '\n' + good.splitlines()[0],
                    good + '\nnsb3_board_reset_request: data=05',
                    good.replace('rf_reads=1092288', 'rf_reads=1092287'),
                    good.replace('last=59.99', 'last=30.0')):
            with self.assertRaises(ValueError):
                check_consumers(bad)


if __name__ == '__main__':
    unittest.main()

from decimal import Decimal
from functools import reduce
from operator import xor
import unittest
from tools.mbus_input_check import verify, REPEATED, VERSION_QUERY


def fixture(version=False):
    rows = []
    for index, byte in enumerate(VERSION_QUERY if version else REPEATED):
        time = Decimal(20 if version else 8) + Decimal('0.0011') * index
        rows += [f'mbus_input_probe: index={index + 1} data={byte:02x} t={time:.9f}',
                 f'mbus_fixture: data={byte:02x} accepted=1',
                 f'mbus_device: event=rx_read data={byte:02x} ctrl=4c']
    return '\n'.join(rows) + '\nmbus_input_probe: complete\n'


class MbusInputCheckTest(unittest.TestCase):
    def test_repeated_values_and_version_ingress_are_not_reply_claims(self):
        for version, count in ((False, 14), (True, 13)):
            self.assertEqual(reduce(xor, VERSION_QUERY if version else REPEATED), 0)
            result = verify(fixture(version), version_query=version)
            self.assertEqual(result['bytes_delivered_and_read'], count)
            self.assertEqual(result['logged_tx_completions'], '')
            self.assertIn('no service reply', result['scope'])

    def test_frame_coalescing_rejection_missing_read_and_incomplete_run_fail(self):
        good = fixture()
        for bad in (good.replace('accepted=1', 'accepted=0', 1),
                    good.replace('event=rx_read data=a5', 'event=rx_read data=ff', 1),
                    good.replace('8.001100000', '8.016666667'),
                    good.replace('mbus_input_probe: complete', ''),
                    good + '[LUA ERROR] failed'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                verify(bad)

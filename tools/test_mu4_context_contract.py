import unittest

from tools.mu4_context_contract import BASE, MASK, WITNESS, contract, decode_words, route_for_status
from tools.mu4_context_contract import WORKER_BASE, WORKER_WITNESS, decode_worker_entry, worker_route


class ContextContractTest(unittest.TestCase):
    def words(self):
        return dict(enumerate(WITNESS, BASE))

    def test_exact_delayed_branch_and_worker_boundary(self):
        result = decode_words(self.words())
        self.assertEqual(result['matched_words'], 51)
        self.assertTrue(result['branch']['delayed'])
        self.assertEqual(result['branch']['condition'], 'ANEQ')
        self.assertEqual(result['branch']['delay_slots'][0]['documented_base_cycles'], 3)
        self.assertEqual(result['worker_call']['delay_words'], ['770e', '0006'])
        self.assertEqual(result['worker_call']['continuation'], '023d96')

    def test_exhaustive_status_predicate(self):
        for status in range(65536):
            expected = 'call_worker' if all(status & bit for bit in (0x8000, 0x4000, 4)) else 'skip_worker'
            self.assertEqual(route_for_status(status), expected)
        self.assertEqual(decode_words(self.words())['branch']['status_coverage'],
                         {'tested': 65536, 'skip_worker': 57344, 'call_worker': 8192})

    def test_each_required_bit_controls_the_route(self):
        self.assertEqual(route_for_status(MASK), 'call_worker')
        self.assertEqual(route_for_status(0x8001), 'skip_worker')
        for bit in (0x8000, 0x4000, 4):
            self.assertEqual(route_for_status(MASK ^ bit), 'skip_worker')

    def test_each_witness_word_is_required(self):
        for address in self.words():
            words = self.words()
            words[address] ^= 1
            with self.assertRaisesRegex(ValueError, f'{address:06x}'):
                decode_words(words)
            del words[address]
            with self.assertRaises(ValueError):
                decode_words(words)

    def test_source_identity_is_required(self):
        with self.assertRaisesRegex(ValueError, 'original R060'):
            contract(b'not the original upload')

    def test_status_input_domain(self):
        for value in (-1, 65536, 'c004', None):
            with self.assertRaises(ValueError):
                route_for_status(value)

    def test_worker_entry_branch_is_not_delayed(self):
        result = decode_worker_entry(dict(enumerate(WORKER_WITNESS, WORKER_BASE)))
        self.assertEqual(result['matched_words'], 9)
        self.assertFalse(result['branch']['delayed'])
        self.assertEqual(result['branch']['target'], '023ddb')
        self.assertEqual(result['branch']['documented_cycles'], {'taken': 5, 'not_taken': 3})
        self.assertEqual(result['branch']['control_coverage'],
                         {'tested': 65536, 'enter_worker_body': 1, 'skip_worker_body': 65535})

    def test_worker_route_exhaustive(self):
        for value in range(65536):
            self.assertEqual(worker_route(value), 'skip_worker_body' if value else 'enter_worker_body')
        for value in (-1, 65536, '0', None):
            with self.assertRaises(ValueError):
                worker_route(value)

    def test_worker_witness_mutation_and_absence(self):
        for address in range(WORKER_BASE, WORKER_BASE + len(WORKER_WITNESS)):
            words = dict(enumerate(WORKER_WITNESS, WORKER_BASE))
            words[address] ^= 1
            with self.assertRaisesRegex(ValueError, f'{address:06x}'):
                decode_worker_entry(words)
            del words[address]
            with self.assertRaises(ValueError):
                decode_worker_entry(words)


if __name__ == '__main__':
    unittest.main()

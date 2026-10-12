import unittest

from tools.mu4_context_contract import BASE, MASK, WITNESS, contract, decode_words, route_for_status
from tools.mu4_context_contract import WORKER_BASE, WORKER_WITNESS, decode_worker_entry, worker_route
from tools.mu4_context_contract import WORKER_TAIL_BASE, WORKER_TAIL_WITNESS, decode_worker_return, restored_intm
from tools.mu4_context_contract import WORKER_COPY_BASE, WORKER_COPY_WITNESS, decode_worker_copy
from tools.mu4_context_contract import WORKER_LOGIC_BASE, WORKER_LOGIC_WITNESS, decode_worker_logic
from tools.mu4_context_contract import FULL_WORKER_WITNESS, decode_full_worker
from tools.mu4_context_contract import TIMER_VECTOR_BASE, TIMER_VECTOR_WITNESS, TIMER_RETURN_BASE, decode_timer_wrapper
from tools.mu4_context_contract import IDLE_BASE, IDLE_WITNESS, IDLE_WRAPPER_BASE, IDLE_WRAPPER_WITNESS, decode_idle_contract, idle_route


class ContextContractTest(unittest.TestCase):
    def idle_words(self):
        return dict(enumerate(IDLE_WITNESS, IDLE_BASE)) | dict(enumerate(IDLE_WRAPPER_WITNESS, IDLE_WRAPPER_BASE))

    def test_idle_setup_is_locally_gated_not_an_unconditional_short_timer(self):
        result = decode_idle_contract(self.idle_words())
        self.assertEqual(result['matched_words'], 44)
        self.assertEqual(result['flag']['coverage'], {'tested': 65536, 'initialize_before_idle': 1, 'skip_setup': 65535})
        self.assertEqual([row['address'] for row in result['zero_path']], ['0298d8', '0298dc', '0298e0', '0298e4', '0298e6'])
        self.assertEqual(result['wrapper']['call'], 'FCALL 02:98d0')
        self.assertEqual(result['common_tail'][-1], 'FRET')

    def test_idle_route_exhaustive_and_domain(self):
        for flag in range(65536):
            self.assertEqual(idle_route(flag), 'skip_setup' if flag else 'initialize_before_idle')
        for flag in (-1, 65536, None, '1'):
            with self.assertRaises(ValueError):
                idle_route(flag)

    def test_idle_witness_mutation_and_absence(self):
        for address in self.idle_words():
            words = self.idle_words()
            words[address] ^= 1
            with self.assertRaisesRegex(ValueError, f'{address:06x}'):
                decode_idle_contract(words)
            del words[address]
            with self.assertRaises(ValueError):
                decode_idle_contract(words)

    def test_timer_wrapper_saves_xpc_and_returns_with_frete(self):
        words = dict(enumerate(TIMER_VECTOR_WITNESS, TIMER_VECTOR_BASE))
        words[TIMER_RETURN_BASE] = 0xf4e5
        result = decode_timer_wrapper(words)
        self.assertEqual(result['matched_words'], 5)
        self.assertEqual(result['save']['register'], 'XPC')
        self.assertEqual(result['branch']['target'], '023d73')
        self.assertEqual(result['return']['operation'], 'FRETE')
        self.assertTrue(result['return']['clears_intm'])

    def test_timer_wrapper_witness_mutation_and_absence(self):
        original = dict(enumerate(TIMER_VECTOR_WITNESS, TIMER_VECTOR_BASE))
        original[TIMER_RETURN_BASE] = 0xf4e5
        for address in original:
            words = original.copy()
            words[address] ^= 1
            with self.assertRaisesRegex(ValueError, f'{address:06x}'):
                decode_timer_wrapper(words)
            del words[address]
            with self.assertRaises(ValueError):
                decode_timer_wrapper(words)

    def test_full_worker_covers_the_zero_control_body(self):
        result = decode_full_worker(dict(enumerate(FULL_WORKER_WITNESS, WORKER_BASE)))
        self.assertEqual(result['matched_words'], 38)
        self.assertEqual(result['body_words'], 22)
        self.assertEqual(result['matched_word_range'], ['023dbc', '023de1'])
        self.assertIn('second source word equals zero', result['body_entry'])
        self.assertIn('CMPR EQ,AR3', result['conditional_store'])

    def test_full_worker_witness_mutation_and_absence(self):
        for address in range(WORKER_BASE, WORKER_BASE + len(FULL_WORKER_WITNESS)):
            words = dict(enumerate(FULL_WORKER_WITNESS, WORKER_BASE))
            words[address] ^= 1
            with self.assertRaisesRegex(ValueError, f'{address:06x}'):
                decode_full_worker(words)
            del words[address]
            with self.assertRaises(ValueError):
                decode_full_worker(words)

    def test_worker_tail_is_logical_not_memory_add_sub(self):
        result = decode_worker_logic(dict(enumerate(WORKER_LOGIC_WITNESS, WORKER_LOGIC_BASE)))
        self.assertEqual(result['matched_words'], 6)
        self.assertEqual(result['sequence'], ['ADD #4,A', 'AND *AR2+,A', 'OR *AR2-,A', 'MAR *AR2-', 'STL A,*AR2'])
        self.assertIn('zero-extend', result['memory_operands'])

    def test_worker_logical_witness_mutation_and_absence(self):
        for address in range(WORKER_LOGIC_BASE, WORKER_LOGIC_BASE + len(WORKER_LOGIC_WITNESS)):
            words = dict(enumerate(WORKER_LOGIC_WITNESS, WORKER_LOGIC_BASE))
            words[address] ^= 1
            with self.assertRaisesRegex(ValueError, f'{address:06x}'):
                decode_worker_logic(words)
            del words[address]
            with self.assertRaises(ValueError):
                decode_worker_logic(words)

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

    def test_original_return_restores_only_intm(self):
        result = decode_worker_return(dict(enumerate(WORKER_TAIL_WITNESS, WORKER_TAIL_BASE)))
        self.assertEqual(result['matched_words'], 7)
        self.assertEqual(result['return']['delay_words'], ['fd45', 'f6bb'])
        self.assertEqual(result['status_coverage'], {'tested': 65536, 'unmasked': 32768, 'masked': 32768})
        self.assertIn('not all ST1', result['restores'])

    def test_return_intm_domain_and_exhaustive_predicate(self):
        for value in range(65536):
            self.assertEqual(restored_intm(value), 'masked' if value & 0x800 else 'unmasked')
        for value in (-1, 65536, '0800', None):
            with self.assertRaises(ValueError):
                restored_intm(value)

    def test_return_witness_mutation_and_absence(self):
        for address in range(WORKER_TAIL_BASE, WORKER_TAIL_BASE + len(WORKER_TAIL_WITNESS)):
            words = dict(enumerate(WORKER_TAIL_WITNESS, WORKER_TAIL_BASE))
            words[address] ^= 1
            with self.assertRaisesRegex(ValueError, f'{address:06x}'):
                decode_worker_return(words)
            del words[address]
            with self.assertRaises(ValueError):
                decode_worker_return(words)


    def test_original_worker_copy_word_ownership(self):
        result = decode_worker_copy(dict(enumerate(WORKER_COPY_WITNESS, WORKER_COPY_BASE)))
        self.assertEqual(result['matched_words'], 6)
        self.assertEqual(result['sequence'], ['ADDM #2,*AR2', 'PSHD *AR2+', 'MVDK *AR2,AR3', 'POPD *AR3+'])
        self.assertIn('old source', result['daram_order'])
        self.assertIn('operand meanings', result['scope'])

    def test_worker_copy_witness_mutation(self):
        for address in range(WORKER_COPY_BASE, WORKER_COPY_BASE + len(WORKER_COPY_WITNESS)):
            words = dict(enumerate(WORKER_COPY_WITNESS, WORKER_COPY_BASE))
            words[address] ^= 1
            with self.assertRaisesRegex(ValueError, f'{address:06x}'):
                decode_worker_copy(words)

    def test_worker_copy_witness_absence(self):
        for address in range(WORKER_COPY_BASE, WORKER_COPY_BASE + len(WORKER_COPY_WITNESS)):
            words = dict(enumerate(WORKER_COPY_WITNESS, WORKER_COPY_BASE))
            del words[address]
            with self.assertRaisesRegex(ValueError, f'{address:06x}'):
                decode_worker_copy(words)


if __name__ == '__main__':
    unittest.main()

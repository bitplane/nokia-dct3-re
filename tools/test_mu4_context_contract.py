import unittest

from tools.mu4_context_contract import BASE, MASK, WITNESS, contract, decode_words, route_for_status
from tools.mu4_context_contract import WORKER_BASE, WORKER_WITNESS, decode_worker_entry, worker_route
from tools.mu4_context_contract import WORKER_TAIL_BASE, WORKER_TAIL_WITNESS, decode_worker_return, restored_intm
from tools.mu4_context_contract import WORKER_COPY_BASE, WORKER_COPY_WITNESS, decode_worker_copy
from tools.mu4_context_contract import WORKER_LOGIC_BASE, WORKER_LOGIC_WITNESS, decode_worker_logic
from tools.mu4_context_contract import FULL_WORKER_WITNESS, decode_full_worker
from tools.mu4_context_contract import TIMER_VECTOR_BASE, TIMER_VECTOR_WITNESS, TIMER_RETURN_BASE, decode_timer_wrapper
from tools.mu4_context_contract import IDLE_BASE, IDLE_WITNESS, IDLE_WRAPPER_BASE, IDLE_WRAPPER_WITNESS, decode_idle_contract, idle_route
from tools.mu4_context_contract import BOOTSTRAP_BASE, BOOTSTRAP_WITNESS, decode_bootstrap_repeat
from tools.mu4_context_contract import BOOT_MEMORY_SHIFT_BASE, BOOT_MEMORY_SHIFT_WITNESS, decode_boot_memory_shift, decode_extended_shift_load
from tools.mu4_context_contract import BOOT_BLOCK_REPEAT_BASE, BOOT_BLOCK_REPEAT_WITNESS, decode_boot_block_repeat
from tools.mu4_context_contract import decode_block_repeat_setup
from tools.mu4_context_contract import REL_REPEAT_BASE, REL_REPEAT_WITNESS, REL_CALLEE_BASE, REL_CALLEE_WITNESS, decode_rel_repeat_call
from tools.mu4_context_contract import REL_AR_STORE_BASE, REL_AR_STORE_WITNESS, decode_rel_ar_store, decode_shifted_store
from tools.mu4_context_contract import REL_COPY_ENTRY_BASE, REL_COPY_ENTRY_WITNESS, REL_COPY_BASE, REL_COPY_WITNESS, decode_rel_copy_loop, rel_copy_destination
from tools.mu4_context_contract import REL_COPY_DEPENDENCY_BASE, REL_COPY_DEPENDENCY_WITNESS
from tools.mu4_context_contract import REL_CLEANUP_BASE, REL_CLEANUP_WITNESS, REL_RELOAD_API, decode_rel_cleanup, rel_cleanup_route
from tools.mu4_context_contract import MEASUREMENT_RESPONSE_BASE, MEASUREMENT_RESPONSE_WITNESS, decode_measurement_response, measurement_result
from tools.mu4_context_contract import DMA_PAUSE_WITNESSES, decode_dma_pause
from tools.mu4_context_contract import COPY_ADAPTER_WITNESSES, decode_copy_adapter
from tools.mu4_context_contract import RECORDER_MAC_BASE, RECORDER_MAC_WITNESS, decode_recorder_memory_mac
from tools.mu4_context_contract import FILTER_PARENT_WITNESSES, FILTER_CALL_SITES, decode_filter_parent
from tools.mu4_context_contract import playback_control_literal_census, PLAYBACK_SWITCH_CALLS


class ContextContractTest(unittest.TestCase):
    def test_playback_control_census_rejects_missing_extra_and_changed_records(self):
        words = dict(enumerate((6, 0x804a, 2, 0x495, 0xd7b8, 0xd796, 1, 0x54f, 2), 0x27167))
        words.update(dict(enumerate((0x76f8, 0xaa56, 0), 0x27a04)))
        words.update(dict(enumerate((0x76f8, 0xaa59, 0xd796), 0x27a0d)))
        words.update(dict(enumerate((0x10e4, 4, 0xf844, 0x483, 0x6d94, 0x1394,
                                    0x1094, 0x7194, 0x12, 0xf5e3, 0xfc00, 0x4918,
                                    0x68f8, 0x18, 0xfffe, 0xf7be, 0xf495, 0x4a0b,
                                    0xf495, 0x4594, 0x1b94, 0x4b94, 0x1094, 0xf5e7,
                                    0xf6be, 0xee01, 0x8a18, 0xf495, 0xfc00), 0x20478)))
        words.update({0x2049a: 0xaa59, 0x27a2e: 0xaa59,
                      0x204b4: 0xaa56, 0x204d0: 0xaa56,
                      0x204d3: 0xaa56, 0x205af: 0xaa56})
        for site in PLAYBACK_SWITCH_CALLS:
            words.update({site: 0xf074, site + 1: 0x478})
        result = playback_control_literal_census(words)
        self.assertEqual(result['literal_hits']['0495'], ['02716a'])
        self.assertEqual(result['mapped_words_scanned'], len(words))
        self.assertEqual(result['destination'], '00804a')
        self.assertEqual(result['initialized_values'][:2], ['0002', '0495'])
        for address in words:
            changed = dict(words)
            changed[address] ^= 1
            with self.subTest(address=address), self.assertRaises(ValueError):
                playback_control_literal_census(changed)
        for value in (0x495, 0xaa56, 0xaa59):
            with self.assertRaises(ValueError):
                playback_control_literal_census({**words, 0x20000: value})
        with self.assertRaisesRegex(ValueError, 'immediate call census'):
            playback_control_literal_census({**words, 0x20000: 0xf074, 0x20001: 0x478})

    def test_filter_parent_relationships_and_literal_coverage(self):
        words = {address: value for base, witness in FILTER_PARENT_WITNESSES
                 for address, value in enumerate(witness, base)}
        for site in FILTER_CALL_SITES:
            words.update({site: 0xfb82, site + 1: 0xa2c6})
        result = decode_filter_parent(words)
        self.assertEqual(result['literal_hits'], 5)
        self.assertEqual(result['mapped_words_scanned'], len(words))
        self.assertEqual(result['pinned_words'], 141)
        self.assertEqual(result['dispatch']['receiver_references'], ['00aa5c', '00aa5d'])
        self.assertEqual(result['upstream_selector']['for_header_c6f0']['field'], 13)
        self.assertEqual(result['far_pointer_reference'], '00c72a')
        for address in words:
            changed = dict(words)
            changed[address] ^= 1
            with self.subTest(address=address), self.assertRaises(ValueError):
                decode_filter_parent(changed)
        with self.assertRaisesRegex(ValueError, 'literal census'):
            decode_filter_parent({**words, 0x20000: 0xa2c6})
        with self.assertRaises(ValueError):
            decode_filter_parent({address - 0x10000: value for address, value in words.items()})

    def test_recorder_mac_original_image_and_syntax_are_pinned(self):
        words = dict(enumerate(RECORDER_MAC_WITNESS, RECORDER_MAC_BASE))
        result = decode_recorder_memory_mac(words)
        self.assertEqual(result['operation'], 'MAC *AR2,#9,A,B')
        self.assertEqual(result['instruction'], '03ba1a')
        self.assertEqual(result['t_publication'], 'Read')
        for address in words:
            changed = dict(words)
            changed[address] ^= 1
            with self.subTest(address=address), self.assertRaises(ValueError):
                decode_recorder_memory_mac(changed)
        with self.assertRaises(ValueError):
            decode_recorder_memory_mac({address - 0x10000: value for address, value in words.items()})

    def test_copy_adapter_pins_pointer_roles_and_every_word(self):
        words = {address: value for base, witness in COPY_ADAPTER_WITNESSES
                 for address, value in enumerate(witness, base)}
        result = decode_copy_adapter(words)
        self.assertEqual(result['pinned_words'], 37)
        self.assertEqual(result['equal_path']['source_register'], 'AR3')
        self.assertEqual(result['equal_path']['destination_register'], 'AR2')
        self.assertEqual(result['double_fields_compared'], [4, 2])
        for address in words:
            changed = dict(words)
            changed[address] ^= 1
            with self.subTest(address=address), self.assertRaises(ValueError):
                decode_copy_adapter(changed)
        with self.assertRaises(ValueError):
            decode_copy_adapter({address + 0x10000: value for address, value in words.items()})

    def test_dma_pause_sequences_require_separate_overlay_namespaces(self):
        sources = {segment: dict(enumerate(witness, base)) for segment, base, witness in DMA_PAUSE_WITNESSES}
        result = decode_dma_pause(sources['aa22'], sources['aa99'])
        self.assertEqual(result['pinned_words'], 22)
        self.assertEqual(result['channel'], 3)
        self.assertEqual([item['paused_call'] for item in result['sequences']], ['0297be', '027b92'])
        for segment, words in sources.items():
            for address in words:
                changed = {name: dict(image) for name, image in sources.items()}
                changed[segment][address] ^= 1
                with self.subTest(segment=segment, address=address), self.assertRaises(ValueError):
                    decode_dma_pause(changed['aa22'], changed['aa99'])
        with self.assertRaises(ValueError):
            decode_dma_pause(sources['aa99'], sources['aa22'])

    def test_measurement_result_thresholds_and_right_precedence(self):
        threshold = 10_000_000
        for left in (0, threshold - 1, threshold, threshold + 1, 0x7FFFFFFF):
            for right in (0, threshold - 1, threshold, threshold + 1, 0x7FFFFFFF):
                expected = 1
                if left < threshold:
                    expected = 0x1F
                if right < threshold:
                    expected = 0x20
                self.assertEqual(measurement_result(left, right), expected)
        self.assertEqual(measurement_result(0x029ABD24, 0x3AD39FC4), 1)
        for invalid in (-1, 0x80000000, None, '1', True):
            for pair in ((invalid, 0), (0, invalid)):
                with self.assertRaises(ValueError):
                    measurement_result(*pair)

    def test_measurement_response_pins_every_word_and_namespace(self):
        words = dict(enumerate(MEASUREMENT_RESPONSE_WITNESS, MEASUREMENT_RESPONSE_BASE))
        result = decode_measurement_response(words)
        self.assertEqual(result['threshold'], 10_000_000)
        self.assertTrue(result['right']['overrides_left'])
        for address in words:
            changed = dict(words)
            changed[address] ^= 1
            with self.subTest(address=address), self.assertRaises(ValueError):
                decode_measurement_response(changed)
        with self.assertRaises(ValueError):
            decode_measurement_response({a - 0x10000: v for a, v in words.items()})

    def test_rel_cleanup_control_partition(self):
        words = dict(enumerate(REL_CLEANUP_WITNESS, REL_CLEANUP_BASE))
        api = dict(enumerate(REL_RELOAD_API, 0x2080))
        result = decode_rel_cleanup(words, api)
        self.assertEqual(result['active_control_partition'],
                         {'tested': 65536, 'state5': 65535, 'state2': 1, 'state2_control': 2})
        self.assertFalse(result['control']['is_result_publication'])
        self.assertFalse(result['branch']['ar2_modified'])
        self.assertEqual(result['reload']['descriptor'], 0)
        for control in range(65536):
            route = rel_cleanup_route(4, control)
            self.assertEqual(route['state'], 2 if control == 2 else 5)
            self.assertEqual(route['loader_index'], 0)
        for state in range(65536):
            if state != 4:
                self.assertFalse(rel_cleanup_route(state, 0)['cleanup'])
        for state, control in ((-1, 0), (65536, 0), (4, -1), (4, 65536)):
            with self.assertRaises(ValueError):
                rel_cleanup_route(state, control)

    def test_rel_cleanup_rejects_changed_code_or_wrong_api_namespace(self):
        words = dict(enumerate(REL_CLEANUP_WITNESS, REL_CLEANUP_BASE))
        api = dict(enumerate(REL_RELOAD_API, 0x2080))
        for source in (words, api):
            for address in source:
                changed = dict(source)
                changed[address] ^= 1
                with self.subTest(address=address), self.assertRaises(ValueError):
                    decode_rel_cleanup(changed if source is words else words,
                                       changed if source is api else api)
        with self.assertRaises(ValueError):
            decode_rel_cleanup(words, {address + 0x20000: value for address, value in api.items()})

    def test_rel_copy_cpl_and_frame_contract(self):
        words = {**dict(enumerate(REL_COPY_ENTRY_WITNESS, REL_COPY_ENTRY_BASE)),
                 **dict(enumerate(REL_COPY_DEPENDENCY_WITNESS, REL_COPY_DEPENDENCY_BASE)),
                 **dict(enumerate(REL_COPY_WITNESS, REL_COPY_BASE))}
        result = decode_rel_copy_loop(words)
        self.assertEqual(result['pinned_words'], 34)
        self.assertEqual(result['entry_cpl_required'], 1)
        self.assertEqual(result['frame']['sp_delta_before_arguments'], -12)
        self.assertEqual(result['bk_writer']['caller_stack_offset_with_two_word_far_frame'], 0)
        self.assertFalse(result['bk_writer']['source_is_mapped_t'])
        self.assertFalse(result['destination']['short_direct_operand_is_mapped_al'])
        self.assertTrue(result['dependency']['copy_source_is_absolute_ar1'])
        self.assertEqual(result['dependency']['copy'], 'MVKD $0011,*(SP+8) at 02459c')
        self.assertEqual(result['dependency']['consumer'], 'LD *(SP+8),A at 02459e')
        self.assertEqual(result['loop']['setup']['rsa'], 0x45B1)
        self.assertEqual(result['loop']['setup']['rea'], 0x45B2)
        self.assertEqual(result['loop']['words_copied_per_iteration'], 2)
        self.assertEqual([row['destination'] for row in result['examples']], ['c8f0', '40f0'])
        self.assertIn('not input validity', result['scope'])

    def test_rel_copy_destination_word_arithmetic(self):
        for stack8 in range(0x10000):
            for bk in (0, 1, 0x7000, 0xE800, 0xFFFF):
                self.assertEqual(rel_copy_destination(stack8, bk),
                                 (bk + stack8 + stack8) % 65536)
        for values in ((-1, 0), (0, -1), (65536, 0), (0, 65536), (None, 0), (0, 'e800')):
            with self.assertRaises(ValueError):
                rel_copy_destination(*values)

    def test_rel_copy_every_word_is_required(self):
        original = {**dict(enumerate(REL_COPY_ENTRY_WITNESS, REL_COPY_ENTRY_BASE)),
                    **dict(enumerate(REL_COPY_DEPENDENCY_WITNESS, REL_COPY_DEPENDENCY_BASE)),
                    **dict(enumerate(REL_COPY_WITNESS, REL_COPY_BASE))}
        for address in original:
            with self.subTest(address=f'{address:06x}'):
                words = original.copy()
                words[address] ^= 1
                with self.assertRaisesRegex(ValueError, f'{address:06x}'):
                    decode_rel_copy_loop(words)
                del words[address]
                with self.assertRaisesRegex(ValueError, f'{address:06x}'):
                    decode_rel_copy_loop(words)

    def test_shifted_store_fields_and_extension_order(self):
        for shift in range(-16, 16):
            for source in range(2):
                for operation, code in (('STL', 0x0C80), ('STH', 0x0C60)):
                    for mode in (0x15, 0x85, 0xE5, 0xF8):
                        extension = code | source << 8 | (shift & 31)
                        values = [0x6F00 | mode]
                        if mode >= 0xE0:
                            values.append(0x0015)
                        values.append(extension)
                        result = decode_shifted_store(dict(enumerate(values, 0x600)), 0x600)
                        self.assertEqual(result['operation'], operation)
                        self.assertEqual(result['source'], 'B' if source else 'A')
                        self.assertEqual(result['shift'], shift)
                        self.assertEqual(result['words'], len(values))
                        self.assertEqual(result['operation_extension_address'], f'{0x600 + len(values) - 1:06x}')
                        self.assertEqual(result['absolute_destination'], '0015' if mode == 0xF8 else None)
                        self.assertIn('no mapped-register publication', result['scope'])

    def test_shifted_store_rejects_other_operations_and_missing_words(self):
        for values in ([0x6FF8, 0x15, 0x0C41], [0x6FF8, 0x15, 0x0C21],
                       [0x6FF8, 0x0C81], [0x6FF9, 0x15, 0x0C81],
                       [0x8815, 0x0C81], [0x6F85]):
            with self.assertRaises(ValueError):
                decode_shifted_store(dict(enumerate(values)), 0)
        with self.assertRaises(ValueError):
            decode_shifted_store({0: 0x6FF8, 2: 0x0C81}, 0)

    def test_rel_ar5_store_and_consumer_spacing(self):
        result = decode_rel_ar_store(dict(enumerate(REL_AR_STORE_WITNESS, REL_AR_STORE_BASE)))
        self.assertEqual(result['pinned_words'], 13)
        self.assertEqual(result['preceding_store']['operation'], 'STLM A,AR4')
        self.assertEqual(result['store']['address'], '025fdd')
        self.assertEqual(result['store']['operation'], 'STL')
        self.assertEqual(result['store']['source'], 'A')
        self.assertEqual(result['store']['shift'], 1)
        self.assertEqual(result['store']['absolute_destination'], '0015')
        self.assertEqual(result['destination_register'], 'AR5')
        self.assertEqual(result['words_before_first_ar5_consumer'], 4)
        self.assertEqual(result['required_store_latency_cycles'], 2)
        self.assertEqual(result['following_instructions'][-1]['address'], '025fe4')
        self.assertIn('not a whole-function census', result['scope'])

    def test_rel_ar_store_every_word_is_required_and_pinned(self):
        original = dict(enumerate(REL_AR_STORE_WITNESS, REL_AR_STORE_BASE))
        for address in original:
            with self.subTest(address=f'{address:06x}'):
                words = original.copy()
                words[address] ^= 1
                with self.assertRaisesRegex(ValueError, f'{address:06x}'):
                    decode_rel_ar_store(words)
                del words[address]
                with self.assertRaisesRegex(ValueError, f'{address:06x}'):
                    decode_rel_ar_store(words)

    @staticmethod
    def rel_words():
        return {**dict(enumerate(REL_REPEAT_WITNESS, REL_REPEAT_BASE)),
                **dict(enumerate(REL_CALLEE_WITNESS, REL_CALLEE_BASE))}

    def test_rel_call_geometry_and_complete_callee(self):
        result = decode_rel_repeat_call(self.rel_words())
        self.assertEqual(result['pinned_words'], 80)
        self.assertEqual(result['loop']['setup']['rsa'], 0x6116)
        self.assertEqual(result['loop']['setup']['rea'], 0x6120)
        self.assertEqual(result['loop']['nominal_body_executions'], 7)
        self.assertEqual(result['call']['target'], '02510c')
        self.assertEqual(result['call']['return_address'], '02611a')
        self.assertEqual(result['call']['stack_words'], 2)
        self.assertEqual(result['call']['argument_a'], 1)
        self.assertEqual(result['callee']['pinned_words'], 55)
        self.assertEqual(result['callee']['last_word'], '025142')
        self.assertEqual(result['callee']['branch']['paths'], 2)
        self.assertEqual(result['callee']['branch']['backward_edges'], 0)
        self.assertEqual(result['callee']['repeat_control_writes'], [])
        self.assertEqual(result['callee']['braf_writes'], 0)
        self.assertEqual(result['callee']['cpl_writes'], 0)
        self.assertEqual(result['callee']['calls'], 0)
        self.assertIn('no physical mapping', result['scope'])

    def test_rel_callee_fixed_stores_are_not_loop_control(self):
        stores = decode_rel_repeat_call(self.rel_words())['callee']['fixed_ram_writes']
        self.assertEqual([store['instruction'] for store in stores],
                         ['025127', '02512a', '02512d', '025130'])
        self.assertEqual([store['destination'] for store in stores],
                         ['a858', 'b331', 'a854', 'a854'])
        self.assertEqual([store['words'] for store in stores], [2, 1, 1, 1])

    def test_rel_every_word_is_required_and_pinned(self):
        original = self.rel_words()
        for address in original:
            with self.subTest(address=f'{address:06x}'):
                words = original.copy()
                words[address] ^= 1
                with self.assertRaisesRegex(ValueError, f'{address:06x}'):
                    decode_rel_repeat_call(words)
                del words[address]
                with self.assertRaisesRegex(ValueError, f'{address:06x}'):
                    decode_rel_repeat_call(words)

    def test_block_repeat_architectural_setup(self):
        for opcode, words, cycles in ((0xF072, 2, 4), (0xF272, 4, 2)):
            for address in (0, 0x32EE, 0xFFFF, 0x12FFFF, 0x7FFFFF):
                result = decode_block_repeat_setup(opcode, 0x32F4, address)
                self.assertEqual(result['rsa'], (address + words) & 0xFFFF)
                self.assertEqual(result['rea'], 0x32F4)
                self.assertEqual(result['delay_words'], words - 2)
                self.assertEqual(result['nominal_cycles'], cycles)
                self.assertTrue(result['interruptible'])
                self.assertFalse(result['repeatable'])
                self.assertEqual(result['counter_phase'], 'last block instruction Decode')

    def test_block_repeat_setup_rejects_invalid_forms(self):
        for arguments in ((0xF070, 0, 0), (0xF072, -1, 0),
                          (0xF272, 0x10000, 0), (0xF072, 0, -1),
                          (0xF072, 0, 0x800000)):
            with self.assertRaises(ValueError):
                decode_block_repeat_setup(*arguments)

    def test_block_repeat_store_spacing_and_bounds(self):
        result = decode_boot_block_repeat(dict(enumerate(BOOT_BLOCK_REPEAT_WITNESS, BOOT_BLOCK_REPEAT_BASE)))
        self.assertEqual(result['store']['operation'], 'STLM B,BRC')
        self.assertEqual(result['latency_word']['operation'], 'NOP')
        self.assertEqual(result['setup']['end_literal'], '32f4')
        self.assertEqual(result['block']['first_word'], '0032f0')
        self.assertEqual(result['block']['last_word'], '0032f4')
        self.assertEqual(result['block']['words'], ['1386', 'f678', 'f3e8', 'f1a0', '8196'])
        self.assertEqual(result['block']['instructions'],
                         ['LDU *AR6,B', 'SFTA B,-8,A', 'SFTL B,8,B',
                          'OR A,0,B', 'STL B,*AR6+'])

    def test_original_block_byte_swap_desk_arithmetic(self):
        # Unsigned LD bounds B to 16 bits; guard/sign and saturation cannot
        # change this body's low-word result. This is not a CPU test.
        for source in range(0x10000):
            a = source >> 8
            b = ((source << 8) & 0xFFFFFFFF) | a
            stored = b & 0xFFFF
            expected = int.from_bytes(source.to_bytes(2, 'little'), 'big')
            self.assertEqual(stored, expected)

    def test_block_repeat_witness_mutation_and_absence(self):
        original = dict(enumerate(BOOT_BLOCK_REPEAT_WITNESS, BOOT_BLOCK_REPEAT_BASE))
        for address in original:
            words = original.copy()
            words[address] ^= 1
            with self.assertRaisesRegex(ValueError, f'{address:06x}'):
                decode_boot_block_repeat(words)
            del words[address]
            with self.assertRaises(ValueError):
                decode_boot_block_repeat(words)

    def test_boot_memory_shift_extension_order(self):
        words = dict(enumerate(BOOT_MEMORY_SHIFT_WITNESS, BOOT_MEMORY_SHIFT_BASE))
        result = decode_boot_memory_shift(words)
        self.assertEqual(result['destination'], 'B')
        self.assertEqual(result['shift'], 8)
        self.assertEqual(result['address_extension'], '0010')
        self.assertEqual(result['operation_extension_address'], '003293')
        self.assertEqual(result['words'], 3)

    def test_boot_memory_shift_witness_mutation_and_absence(self):
        original = dict(enumerate(BOOT_MEMORY_SHIFT_WITNESS, BOOT_MEMORY_SHIFT_BASE))
        for address in original:
            words = original.copy()
            words[address] ^= 1
            with self.assertRaisesRegex(ValueError, f'{address:06x}'):
                decode_boot_memory_shift(words)
            del words[address]
            with self.assertRaises(ValueError):
                decode_boot_memory_shift(words)

    def test_shift_load_signed_five_bit_and_destination(self):
        for shift in range(-16, 16):
            for destination in range(2):
                for opcode in (0x6f0a, 0x6f86, 0x6fe6, 0x6ff8):
                    extension = 0x0c40 | destination << 8 | shift & 31
                    values = [opcode, 0xf495, extension] if opcode & 0xff >= 0xe0 else [opcode, extension]
                    result = decode_extended_shift_load(dict(enumerate(values, 0x600)), 0x600)
                    self.assertEqual(result['shift'], shift)
                    self.assertEqual(result['destination'], 'B' if destination else 'A')
                    self.assertEqual(result['words'], len(values))

    def test_shift_load_rejects_other_operations_and_missing_extensions(self):
        for values in ({0: 0x6fe6, 1: 0x10, 2: 0x0d68}, {0: 0x6fe6, 2: 0x0d48},
                       {0: 0x6fe6, 1: 0x10}, {0: 0x6ff9, 1: 0x0d48}, {0: 0x440a}):
            with self.assertRaises(ValueError):
                decode_extended_shift_load(values, 0)

    def test_bootstrap_repeat_is_program_copy_not_nop_or_constant_fill(self):
        words = dict(enumerate(BOOTSTRAP_WITNESS, BOOTSTRAP_BASE))
        result = decode_bootstrap_repeat(words)
        self.assertEqual(result['matched_words'], 12)
        repeat = result['repeat']
        self.assertEqual(repeat['executions'], 65313)
        self.assertEqual(repeat['program_source_range'], ['0060', 'ff80'])
        self.assertEqual(repeat['data_destination_range'], ['0060', 'ff80'])
        self.assertEqual(repeat['ar0_after'], 'ff81')
        self.assertEqual(repeat['operation'], 'MVPD 0060,*AR0+')
        self.assertEqual(repeat['uploaded_source_words'], 1)
        self.assertEqual(repeat['source_words_not_in_upload'], 65312)

    def test_bootstrap_source_coverage_does_not_invent_erased_or_zero_words(self):
        words = dict(enumerate(BOOTSTRAP_WITNESS, BOOTSTRAP_BASE))
        words[0x60] = 0x1234
        words[0x61] = 0xffff
        words[0xff90] = 0
        repeat = decode_bootstrap_repeat(words)['repeat']
        self.assertEqual(repeat['uploaded_source_words'], 3)
        self.assertEqual(repeat['uploaded_source_words'] + repeat['source_words_not_in_upload'], 65313)

    def test_bootstrap_repeat_witness_mutation_and_absence(self):
        original = dict(enumerate(BOOTSTRAP_WITNESS, BOOTSTRAP_BASE))
        for address in original:
            words = original.copy()
            words[address] ^= 1
            with self.assertRaisesRegex(ValueError, f'{address:06x}'):
                decode_bootstrap_repeat(words)
            del words[address]
            with self.assertRaises(ValueError):
                decode_bootstrap_repeat(words)

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

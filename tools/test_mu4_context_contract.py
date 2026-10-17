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
from tools.mu4_context_contract import playback_lifecycle_literal_census
from tools.mu4_context_contract import MEASUREMENT_RESPONSE_BASE, MEASUREMENT_RESPONSE_WITNESS, decode_measurement_response, measurement_result
from tools.mu4_context_contract import DMA_PAUSE_WITNESSES, decode_dma_pause
from tools.mu4_context_contract import COPY_ADAPTER_WITNESSES, decode_copy_adapter, reference_copy_adapter
from tools.mu4_context_contract import DECODER_OUTPUT_WITNESSES, decode_decoder_output
from tools.mu4_context_contract import DECODER_DISPATCH_WITNESSES, decode_decoder_dispatch, reference_decoder_local_gate
from tools.mu4_context_contract import PCM_CONVERSION_WITNESSES, decode_pcm_conversion, reference_pcm_conversion
from tools.mu4_context_contract import RECORDER_MAC_BASE, RECORDER_MAC_WITNESS, decode_recorder_memory_mac
from tools.mu4_context_contract import FILTER_PARENT_WITNESSES, FILTER_CALL_SITES, decode_filter_parent
from tools.mu4_context_contract import playback_control_literal_census, PLAYBACK_SWITCH_CALLS
from tools.mu4_context_contract import reference_bit_window
from tools.mu4_context_contract import BIT_WINDOW_BASE, BIT_WINDOW_WITNESS, decode_bit_window
from tools.mu4_context_contract import NIBBLE_BASE, NIBBLE_WITNESS, decode_nibble_selector, reference_nibble
from tools.mu4_context_contract import decode_prefix_transform, reference_transform_block
from tools.mu4_context_contract import decode_recording_prefix_transform
from tools.mu4_context_contract import decode_transform_copy
from tools.mu4_context_contract import BYTE_COUNT_BASE, BYTE_COUNT_WITNESS, decode_byte_count_prefix, reference_byte_count
from tools.mu4_context_contract import ESCAPE_STORE_BASE, ESCAPE_STORE_WITNESS, decode_escape_store_prefix
from tools.mu4_context_contract import POLY_CALLER_BASE, POLY_CALLER_WITNESS, decode_poly_caller
from tools.mu4_context_contract import reference_poly_step, playback_poly_observation
from tools.mu4_context_contract import SQUARE_LOOP_BASE, SQUARE_LOOP_WITNESS, decode_square_loop
from tools.mu4_context_contract import REGISTER_PRODUCT_BASE, REGISTER_PRODUCT_WITNESS, decode_register_product_caller
from tools.mu4_context_contract import SERVICE_GATE_BASE, SERVICE_GATE_WITNESS, decode_service_gate
from tools.mu4_context_contract import POST_FLAG_BASE, POST_FLAG_WITNESS, decode_post_flag_branch, post_flag_route
from tools.mu4_context_contract import FLAG_CONTEXT_BASE, FLAG_CONTEXT_WITNESS, decode_flag_context, reference_flag_context
from tools.mu4_context_contract import RECORD_HEADER_BASE, RECORD_HEADER_WIDTHS, decode_recording_header_writer, reference_recording_header
from tools.mu4_context_contract import recording_header_observations
from tools.mu4_context_contract import RECORD_WINDOW_BASE, RECORD_WINDOW_WITNESS, decode_recording_window_arguments, reference_recording_windows


class ContextContractTest(unittest.TestCase):
    def test_lifecycle_census_keeps_unknowns_and_distinguishes_store_from_compare(self):
        words = dict(enumerate((0x76f8, 0x373e, 4, 0x60f8, 0x373e, 4,
                                0x10f8, 0x3742, 0xffff, 0x3742), 0x20000))
        report = playback_lifecycle_literal_census(words)
        self.assertEqual(report['mapped_words'], 10)
        self.assertEqual(report['literal_references'], 4)
        self.assertEqual([row['classification'] for row in report['references']], [
            'absolute_immediate_store_pattern', 'absolute_immediate_compare_pattern',
            'absolute_load_a_pattern', 'unclassified_literal'])
        self.assertEqual(report['references'][0]['instruction_address'], '020000')
        self.assertEqual(report['references'][0]['immediate'], '0004')

    def test_lifecycle_census_does_not_bridge_missing_words(self):
        report = playback_lifecycle_literal_census({0x100: 0x76f8, 0x101: 0x373e,
                                                   0x103: 4, 0x200: 0x3742})
        self.assertEqual(report['classes'], {'unclassified_literal': 2})

    def test_recording_window_prefix_pins_parameter_roles(self):
        words = dict(enumerate(RECORD_WINDOW_WITNESS, RECORD_WINDOW_BASE))
        self.assertEqual(decode_recording_window_arguments(words)['pinned_words'], 16)
        for address in words:
            changed = dict(words)
            changed[address] ^= 1
            with self.subTest(address=address), self.assertRaises(ValueError):
                decode_recording_window_arguments(changed)

    def test_recording_window_model_preserves_clear_bytes_and_rejects_partial_chunks(self):
        source, state = bytes(range(64)), list(range(82))
        encoded = reference_recording_windows(source, state, reverse=False)
        self.assertEqual(reference_recording_windows(encoded, state), source)
        for offset in (0,32):
            self.assertEqual(encoded[offset+4:offset+32], source[offset+4:offset+32])
            expected = reference_transform_block(int.from_bytes(source[offset:offset+2],'little'),
                                                 int.from_bytes(source[offset+2:offset+4],'little'), state, reverse=False)
            self.assertEqual(encoded[offset:offset+4], b''.join(word.to_bytes(2,'little') for word in expected))
        for data, key in ((b'1',state),(bytes(31),state),(source,state[:-1]),(source,[-1]+state[1:])):
            with self.assertRaises(ValueError):
                reference_recording_windows(data, key)

    @staticmethod
    def header_observation_fixture():
        lines = ['mu4_native_recorder_overlay: PASS marker=aa88 records=4622 words=73766 '
                 'checked_ram_words=73762 excluded_register_words=4 before_execution=1']
        for frame in range(8):
            values = [4095,1,0,1,1,4,0,2,0,0,0,0,373 if frame == 0 else 372,384,0]
            for field, (value, width) in enumerate(zip(values, RECORD_HEADER_WIDTHS)):
                lines.append(f'mu4_record_header_argument: field={field} ordinal={frame+1} caller={0x3a93a+10*field:06x} '
                             f'high=0000 low={value:04x} width={width} receiver=efc2 sp=6678 '
                             f't={1+(frame*15+field)/1000:.9f} context_pc=03accc '
                             'boundary=data_read ownership=unvalidated observer_writes=0')
        return '\n'.join(lines + ['mu4_record_header_taps_closed: t=2.016666667 observer_writes=0'])

    def test_header_observations_pack_eight_complete_ordered_frames(self):
        result = recording_header_observations(self.header_observation_fixture())
        self.assertEqual(result['argument_records'], 120)
        self.assertEqual(result['frames'][0]['header_hex'], 'fff950802ea600')
        self.assertEqual(result['frames'][7]['header_hex'], 'fff950802e8600')

    def test_header_observations_reject_missing_malformed_and_wrong_scope(self):
        text = self.header_observation_fixture()
        lines = text.splitlines()
        invalid = [
            '\n'.join(lines[1:]), text+'\n'+lines[0], text+'\n'+lines[-1],
            '\n'.join([lines[1],lines[0]]+lines[2:]),
            text+'\n'+lines[1], '\n'.join(lines[:2]+lines[3:]),
            '\n'.join([lines[0],lines[2],lines[1]]+lines[3:]),
            text.replace('ordinal=1', 'ordinal=2', 1),
            text+'\nmu4_record_header_taps_closed: malformed',
            text.replace('width=12', 'width=11', 1),
            text.replace('caller=03a93a', 'caller=03a93b', 1),
            text.replace('receiver=efc2', 'receiver=efc3', 1),
            text.replace('t=1.000000000', 't=0.100000000', 1),
            text.replace('high=0000', 'high=0001', 1),
            text.replace('ownership=unvalidated', 'ownership=claimed', 1),
            text.replace('context_pc=03accc', 'context_pc=03a000', 1),
            text.replace('t=2.016666667', 't=1.900000000'),
        ]
        for index, bad in enumerate(invalid):
            with self.subTest(index=index), self.assertRaises(ValueError):
                recording_header_observations(bad)
        for count in (0, 9):
            with self.assertRaises(ValueError):
                recording_header_observations(text, count)

    def test_recording_header_writer_recovers_all_fields(self):
        words = {}
        for index, width in enumerate(RECORD_HEADER_WIDTHS):
            block = (0xf7b8,0x10e6,0x227+index,0x7602,width,0x4e00,0xfb83,0xacc5,0xf495,0x4811)
            words.update(dict(enumerate(block, RECORD_HEADER_BASE + 10*index)))
        self.assertEqual(decode_recording_header_writer(words)['total_bits'], 56)
        for address in words:
            changed = dict(words)
            changed[address] ^= 1
            with self.subTest(address=address), self.assertRaises(ValueError):
                decode_recording_header_writer(changed)

    def test_recording_header_model_matches_independent_candidate_reader(self):
        from tools.mu4_recorder_media_inventory import adts_header
        values = [4095,1,0,1,1,4,0,2,0,0,0,0,372,384,0]
        self.assertEqual(reference_recording_header(values).hex(), 'fff950802e8600')
        for size in range(7,8192):
            values[12] = size
            header = adts_header(reference_recording_header(values), 0)
            self.assertEqual(header['frame_bytes'], size)
            self.assertEqual(header['channel_configuration'], 2)
        for index, width in enumerate(RECORD_HEADER_WIDTHS):
            bad = list(values)
            bad[index] = 1 << width
            with self.assertRaises(ValueError):
                reference_recording_header(bad)
        for bad in ([], values[:-1], values+[0], [-1]+values[1:]):
            with self.assertRaises(ValueError):
                reference_recording_header(bad)

    def test_flag_context_helper_source_is_pinned(self):
        words = dict(enumerate(FLAG_CONTEXT_WITNESS, FLAG_CONTEXT_BASE))
        self.assertEqual(decode_flag_context(words)['pinned_words'], 45)
        for address in words:
            changed = dict(words)
            changed[address] ^= 1
            with self.subTest(address=address), self.assertRaises(ValueError):
                decode_flag_context(changed)

    def test_flag_context_selector_has_one_zero_route(self):
        self.assertEqual([reference_flag_context(value) for value in range(4)], [1, 1, 0, 1])
        for value in (-1, 4, 65535):
            with self.assertRaises(ValueError):
                reference_flag_context(value)

    def test_post_flag_branch_source_is_pinned(self):
        words = dict(enumerate(POST_FLAG_WITNESS, POST_FLAG_BASE))
        self.assertEqual(decode_post_flag_branch(words)['pinned_words'], 21)
        for address in words:
            changed = dict(words)
            changed[address] ^= 1
            with self.subTest(address=address), self.assertRaises(ValueError):
                decode_post_flag_branch(changed)

    def test_post_flag_route_exhausts_local_condition_domain(self):
        for word in range(65536):
            self.assertEqual(post_flag_route(0, word), 'skip')
            self.assertEqual(post_flag_route(1, word), 'restore' if word == 0 else 'conditional_parser')
        for flag, word in ((-1, 0), (2, 0), (1, -1), (1, 65536)):
            with self.assertRaises(ValueError):
                post_flag_route(flag, word)

    def test_service_gate_and_receiver_direction_are_source_pinned(self):
        words = dict(enumerate(SERVICE_GATE_WITNESS, SERVICE_GATE_BASE))
        words.update(dict(enumerate((0x7211,0xaa59,0x7217,0xaa60), 0x20499)))
        result = decode_service_gate(words)
        self.assertEqual(result['pinned_words'], 25)
        self.assertIn('memory to register', result['receiver'])
        self.assertIn('AEQ', result['tail'])
        for address in words:
            changed = dict(words)
            changed[address] ^= 1
            with self.subTest(address=address), self.assertRaises(ValueError):
                decode_service_gate(changed)

    def test_original_byte_count_prefix_source_is_pinned(self):
        words = dict(enumerate(BYTE_COUNT_WITNESS, BYTE_COUNT_BASE))
        self.assertEqual(decode_byte_count_prefix(words)['pinned_words'], 13)
        for address in words:
            changed = dict(words)
            changed[address] ^= 1
            with self.subTest(address=address), self.assertRaises(ValueError):
                decode_byte_count_prefix(changed)

    def test_original_escape_store_prefix_source_is_pinned(self):
        words = dict(enumerate(ESCAPE_STORE_WITNESS, ESCAPE_STORE_BASE))
        self.assertEqual(decode_escape_store_prefix(words)['pinned_words'], 13)
        for address in words:
            changed = dict(words)
            changed[address] ^= 1
            with self.subTest(address=address), self.assertRaises(ValueError):
                decode_escape_store_prefix(changed)

    def test_original_poly_caller_source_is_pinned(self):
        words = dict(enumerate(POLY_CALLER_WITNESS, POLY_CALLER_BASE))
        self.assertEqual(decode_poly_caller(words)['pinned_words'], 10)
        for address in words:
            changed = dict(words)
            changed[address] ^= 1
            with self.subTest(address=address), self.assertRaises(ValueError):
                decode_poly_caller(changed)

    def test_original_square_loop_source_is_pinned(self):
        words = dict(enumerate(SQUARE_LOOP_WITNESS,SQUARE_LOOP_BASE))
        self.assertEqual(decode_square_loop(words)['pinned_words'],14)
        for address in words:
            changed = dict(words)
            changed[address] ^= 1
            with self.subTest(address=address), self.assertRaises(ValueError):
                decode_square_loop(changed)

    def test_original_register_product_caller_source_is_pinned(self):
        words = dict(enumerate(REGISTER_PRODUCT_WITNESS,REGISTER_PRODUCT_BASE))
        self.assertEqual(decode_register_product_caller(words)['pinned_words'],9)
        for address in words:
            changed = dict(words)
            changed[address] ^= 1
            with self.subTest(address=address), self.assertRaises(ValueError):
                decode_register_product_caller(changed)

    def test_poly_manual_example_and_signed_high17(self):
        self.assertEqual(reference_poly_step(0x12340000,0x10000,0x5678,0x2000,0), (0x06270000,0x20000000,False))
        self.assertEqual(reference_poly_step(0x80000000,0,1,0xffff,0x100), (0x10000,0xffffff0000,False))
        self.assertEqual(reference_poly_step(0x80000000,0,0x7fff,0x8000,0x340), (0x7fff0000,0xff80000000,False))
        self.assertEqual(reference_poly_step(0x80000000,0x7fffffff,0x7fff,0,0x240)[0],0x7fffffff)
        with self.assertRaises(ValueError):
            reference_poly_step(1<<40,0,0,0,0)

    @staticmethod
    def poly_bus_fixture():
        values = (0x4600,0x1282,0xbe62,0x76db,0x3842,0,0x68a9,0)
        addresses = (0x1170,0xc22a,0xc22b,0xc22c,0xc22d,0xc22e,0x1174,0x1175)
        return '\n'.join(f'mu4_playback_poly_bus: kind={"read" if i<6 else "write"} address={address:04x} value={value:04x} ordinal={i+1 if i<6 else i-5} st0=1f01 st1=6144 pmst=202c t={1.01+i*0.000001:.9f} context_pc=0260ba boundary=data_bus ownership=unvalidated observer_writes=0' for i,(address,value) in enumerate(zip(addresses,values)))

    def test_native_poly_bus_matches_independent_three_step_model(self):
        result = playback_poly_observation(self.poly_bus_fixture())
        self.assertEqual(result['result'],'68a90000')
        self.assertTrue(result['scalar_match'])
        self.assertFalse(result['native_audio_validated'])

    def test_native_poly_bus_rejects_incomplete_or_changed_evidence(self):
        text = self.poly_bus_fixture()
        for bad in (text.split('\n',1)[1], text+'\n'+text.splitlines()[0],
                    text.replace('value=68a9','value=68a8'),
                    text.replace('address=c22d','address=c22c'),
                    text.replace('ordinal=4','ordinal=3'),
                    text.replace('observer_writes=0','observer_writes=1'),
                    text.replace('ownership=unvalidated','ownership=validated'),
                    text.replace('pmst=202c','pmst=202e'),
                    text.replace('st1=6144','st1=ffff',1),
                    text.replace('context_pc=0260ba','context_pc=010000'),
                    text.replace(' t=1.010000000',' t=1.500000000'),
                    text.replace(' st0=1f01','')):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                playback_poly_observation(bad)

    def test_byte_count_reference_counts_groups_independently(self):
        for bits in range(1, 65536):
            self.assertEqual(reference_byte_count(bits), len(range(0, bits, 8)))
        for bits in (-1, 0, 65536):
            with self.assertRaises(ValueError):
                reference_byte_count(bits)

    def test_transform_copy_source_is_not_mistaken_for_zero_fill(self):
        for segment, base in (('aa88', 0x3e438), ('aa99', 0x2d01a)):
            source = (0x1303,0x7102,0x0012,0xfa4d,(base+14)&65535,0xf495,
                      0x8813,0xf310,1,0x891a,0xf495,0xf072,(base+13)&65535,
                      0xe589,0xf495,0xf495,0xf4e4)
            words = dict(enumerate(source, base))
            report = decode_transform_copy(words, segment)
            self.assertEqual(report['operation'], 'MVDD Xmem,Ymem copy, not fill')
            self.assertEqual(report['pinned_words'], 17)
            for address in words:
                changed = dict(words)
                changed[address] ^= 1
                with self.subTest(segment=segment, address=address), self.assertRaises(ValueError):
                    decode_transform_copy(changed, segment)
        with self.assertRaises(ValueError):
            decode_transform_copy({}, 'aa22')

    def test_recorded_native_block_matches_model_and_independent_saved_bytes(self):
        state = [int(word, 16) for word in (
            '038f d4c4 63fd 5f08 8d2f d427 9d03 7986 4d6e d591 d525 ad1f 8a59 265a af39 057c 1432 1e68 '
            '2eaf 682b b991 49ec 3c0d f25a 8020 2f59 7b34 8060 7e4b f765 e6b7 2712 8eef 8954 '
            'ec34 a6ef 2878 049a 7f78 e8ab 8e37 5a27 2487 c372 fc24 66f0 0e17 7884 a6d5 6543 '
            '2b3b cdb3 e234 8ebc 5219 8626 0dda f6d0 997f d1be 21a3 2608 a562 a5ed 24f2 d374 '
            'b9cb 0611 36f0 c51c f3de a969 90e6 7bd4 490a ebc3 ace7 72d0 350c 346f f925 584f').split()]
        encoded = reference_transform_block(0xf9ff, 0x8050, state, reverse=False)
        self.assertEqual(encoded, (0x2ee7, 0xdcc0))
        self.assertEqual(b''.join(word.to_bytes(2, 'little') for word in encoded), bytes.fromhex('e72ec0dc'))
        self.assertEqual(reference_transform_block(*encoded, state), (0xf9ff, 0x8050))
        # The fourth source header predicts suffix 80 2e 86 00. Its saved
        # 4f cd d2 01 bytes are explained by the transform, not a bad length.
        for plain, wire in (((0x2e80,0x0086), '4fcdd201'), ((0,0), '90fe56b1')):
            encoded = reference_transform_block(*plain, state, reverse=False)
            self.assertEqual(b''.join(word.to_bytes(2,'little') for word in encoded), bytes.fromhex(wire))
            self.assertEqual(reference_transform_block(*encoded, state), plain)
        with self.assertRaises(ValueError):
            decode_recording_prefix_transform({})

    def test_prefix_transform_matches_independently_captured_native_block(self):
        state = [int(word, 16) for word in (
            '19ab 3f55 0747 8eaa b29e f452 7c29 2860 b697 6f5a b372 e61e 1aef 91df dc01 02ec 7967 3b13 '
            '44ce da5d 71b6 a02e 39ab eaec fe91 6f35 6485 b819 e850 4dee 195f 70f1 1d95 d9d5 '
            'e3e8 54d7 79dd 0394 f65f 693c b5b1 955b e75d 67e3 9672 ab64 2434 dc78 9bb8 178b '
            'a09d 5c8e 4965 f7de 5546 96b9 66e6 d459 f92e 31ed 4f90 1272 c41f ab19 e607 190c '
            '8bae a8cc a98a 4ee0 9987 ef84 8767 2609 7bc7 b5fc b37d 3193 cd0f 1a4a ca87 bf1b').split()]
        self.assertEqual(reference_transform_block(0xf9ff, 0x8050, state), (0x8bca, 0x3641))
        self.assertEqual(reference_transform_block(0x8bca, 0x3641, state, reverse=False), (0xf9ff, 0x8050))
        # Fresh native recording playback supplies zero input under the same state.
        self.assertEqual(reference_transform_block(0, 0, state), (0xc641, 0x474f))

    def test_prefix_transform_domain_and_roundtrip_controls(self):
        self.assertEqual(reference_transform_block(0x1234, 0xabcd, [0] * 82), (0xabcd, 0x1234))
        state = [(index * 1999 + 1357) & 0xffff for index in range(82)]
        for left, right in ((0, 0), (0xffff, 0x8000), (0x1234, 0x5678)):
            transformed = reference_transform_block(left, right, state)
            self.assertEqual(reference_transform_block(*transformed, state, reverse=False), (left, right))
        for bad in ([], [0] * 81, [0] * 83, [-1] + [0] * 81, [65536] + [0] * 81):
            with self.assertRaises(ValueError):
                reference_transform_block(0, 0, bad)
        with self.assertRaises(ValueError):
            decode_prefix_transform({})

    def test_original_nibble_selector_source_rejects_each_mutation(self):
        words = dict(enumerate(NIBBLE_WITNESS, NIBBLE_BASE))
        self.assertEqual(decode_nibble_selector(words)['pinned_words'], 23)
        for address in words:
            changed = dict(words)
            changed[address] ^= 1
            with self.subTest(address=address), self.assertRaises(ValueError):
                decode_nibble_selector(changed)

    def test_nibble_reference_matches_each_hex_digit(self):
        for value in range(65536):
            for selector, digit in enumerate(f'{value:04x}'):
                self.assertEqual(reference_nibble(value, selector), int(digit, 16))
        for value, selector in ((-1, 0), (65536, 0), (0, -1), (0, 4)):
            with self.assertRaises(ValueError):
                reference_nibble(value, selector)

    def test_original_bit_reader_source_is_pinned_without_cross_overlay_aliases(self):
        words = dict(enumerate(BIT_WINDOW_WITNESS, BIT_WINDOW_BASE))
        self.assertEqual(decode_bit_window(words)['pinned_words'], 55)
        for address in words:
            changed = dict(words)
            changed[address] ^= 1
            with self.subTest(address=address), self.assertRaises(ValueError):
                decode_bit_window(changed)
        with self.assertRaises(ValueError):
            decode_bit_window({address - 0x10000: value for address, value in words.items()})

    def test_bit_window_reference_uses_msb_order_and_one_word_refill(self):
        for width in range(17):
            for remaining in range(33):
                for word in (0, 0xffff, 0x8001, 0x1234):
                    reservoir = 0x89abcdef
                    refill = remaining <= 16
                    payload = ((reservoir << 16) | word) & 0xffffffff if refill else reservoir
                    available = remaining + (16 if refill else 0)
                    bits = f'{payload:032b}'[32 - available:]
                    result = reference_bit_window(remaining, reservoir, 0xffff, word, width)
                    self.assertEqual(result['value'], int('0' + bits[:width], 2))
                    self.assertEqual(result['remaining'], available - width)
                    self.assertEqual(result['index'], 0 if refill else 0xffff)
                    self.assertEqual(result['reservoir'], payload)
        for args in ((33, 0, 0, 0, 1), (0, 0, 0, 0, -1), (0, 0, 0, 0, 17)):
            with self.assertRaises(ValueError):
                reference_bit_window(*args)

    def test_playback_control_census_rejects_missing_extra_and_changed_records(self):
        words = dict(enumerate((6, 0x804a, 2, 0x495, 0xd7b8, 0xd796, 1, 0x54f, 2), 0x27167))
        words.update(dict(enumerate((0x76f8, 0xaa56, 0), 0x27a04)))
        words.update(dict(enumerate((0x76f8, 0xaa59, 0xd796), 0x27a0d)))
        words.update(dict(enumerate((0xf6be, 0x4a06, 0x8812, 0x46f8, 0x7f, 0xf495,
                                    0xf074, 0x2d7, 0x8a06, 0xf7be, 0xf4e4), 0x20419)))
        words.update(dict(enumerate((6, 0xd7b0, 2, 0x419, 0, 0x8044, 1, 0x54f), 0x27079)))
        words.update(dict(enumerate((0x10e4, 4, 0xf844, 0x483, 0x6d94, 0x1394,
                                    0x1094, 0x7194, 0x12, 0xf5e3, 0xfc00, 0x4918,
                                    0x68f8, 0x18, 0xfffe, 0xf7be, 0xf495, 0x4a0b,
                                    0xf495, 0x4594, 0x1b94, 0x4b94, 0x1094, 0xf5e7,
                                    0xf6be, 0xee01, 0x8a18, 0xf495, 0xfc00), 0x20478)))
        words.update(dict(enumerate((0xf6be, 0x8812, 0xf495, 0xf495, 0xf495, 0xf495,
                                    0x6dea, 0x13, 0x4492, 0xf074, 0x1b3, 0xf7be, 0xf4e4), 0x20454)))
        words.update(dict(enumerate((0xf6be, 0x8812, 0xf495, 0xf495, 0xf495, 0xf495,
                                    0x4482, 0xf274, 0x1b3, 0x6dea, 4, 0xf7be, 0xf4e4), 0x2046b)))
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
        self.assertEqual(result['pinned_words'], 74)
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

    def test_copy_adapter_minimum_and_advance_before_copy(self):
        for source in range(9):
            for destination in range(9):
                for sc in range(source+1):
                    for dc in range(destination+1):
                        result = reference_copy_adapter(source,sc,destination,dc)
                        used = 0
                        while sc+used < source and dc+used < destination:
                            used += 1
                        self.assertEqual(result['count'],used)
                        self.assertEqual(result['source_cursor_after'],sc+used)
                        self.assertEqual(result['destination_cursor_after'],dc+used)
                        self.assertEqual(result['set_source_exhausted'],sc+used==source)
                        self.assertEqual(result['set_destination_full'],dc+used==destination)
        self.assertEqual(reference_copy_adapter(1024,0,64,0)['source_cursor_after'],64)
        self.assertEqual(reference_copy_adapter(32767,0,32767,0)['count'],32767)
        for values in ((1,2,1,0),(-1,0,1,0),(32768,0,1,0),(1,0,1,2)):
            with self.assertRaises(ValueError):
                reference_copy_adapter(*values)

    def test_decoder_output_loop_pins_processing_and_two_channel_conversion(self):
        words = {address:value for base,witness in DECODER_OUTPUT_WITNESSES
                 for address,value in enumerate(witness,base)}
        result = decode_decoder_output(words)
        self.assertEqual(result['pinned_words'],59)
        self.assertEqual(result['conversion_loop']['count'],1024)
        for address in words:
            changed = dict(words)
            changed[address] ^= 1
            with self.subTest(address=address),self.assertRaises(ValueError):
                decode_decoder_output(changed)

    def test_decoder_dispatch_pins_method_gate_and_quantifies_raw_census(self):
        words = {address:value for base,witness in DECODER_DISPATCH_WITNESSES
                 for address,value in enumerate(witness,base)}
        result = decode_decoder_dispatch(words)
        self.assertEqual(result['method'],13)
        self.assertEqual(result['census']['encoded_far_reference_candidates'],['00c6a0'])
        self.assertEqual(result['census']['recognized_direct_wrapper_candidates'],['0293cd'])
        for address in words:
            changed = dict(words)
            changed[address] ^= 1
            with self.subTest(address=address), self.assertRaises(ValueError):
                decode_decoder_dispatch(changed)
        words.update({0x4000:2,0x4001:0x3d5d})
        self.assertEqual(decode_decoder_dispatch(words)['census']['encoded_far_reference_candidates'],['004000','00c6a0'])

    def test_decoder_local_gate_keeps_unsigned_threshold_and_activity_distinct(self):
        for values,route in (((0,0x3ff,0,1,0),'insufficient_input'),
                             ((0,0x400,0,1,0),'call_method_13'),
                             ((0,0x400,0,0,0),'inactive'),
                             ((0,0,1,0,1),'call_method_13'),
                             ((0,0xffff,0,1,0),'call_method_13'),
                             ((0xffff,0,0,1,0),'insufficient_input')):
            with self.subTest(values=values):
                self.assertEqual(reference_decoder_local_gate(*values)['route'],route)
        for values in ((-1,0,0,1,0),(0,0,65536,1,0)):
            with self.assertRaises(ValueError):
                reference_decoder_local_gate(*values)

    def test_pcm_conversion_pins_rounding_shift_and_symmetric_clamp(self):
        words = {address:value for base,witness in PCM_CONVERSION_WITNESSES
                 for address,value in enumerate(witness,base)}
        self.assertEqual(decode_pcm_conversion(words)['pinned_words'],152)
        for address in words:
            changed = dict(words)
            changed[address] ^= 1
            with self.subTest(address=address),self.assertRaises(ValueError):
                decode_pcm_conversion(changed)
        for raw,shifted,word in ((0,0,0),(8191,0,0),(8192,1,1),
                                 (0xffffe000,0,0),(0xffffdfff,-1,65535),
                                 (0x7fffffff,131072,32767),(0xff80000000,-131072,32769),
                                 (0x00fff59b08,-42,65494),(0xffffbd0b70,-268,65268)):
            self.assertEqual(reference_pcm_conversion(raw),{'shifted':shifted,'word':word})
        for raw in (-1,1<<40,0x0100000000,0xff00000000):
            with self.assertRaises(ValueError):
                reference_pcm_conversion(raw)

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

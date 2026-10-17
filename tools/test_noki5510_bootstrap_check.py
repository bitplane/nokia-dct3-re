import unittest
from tools.noki5510_bootstrap_check import (
    CHAIN, RUNTIME_CHAIN, verify, verify_serial_readiness, verify_input_lifecycle,
    verify_input_timer, verify_dsp_service_route, verify_resident_memory, verify_playback_observation,
    verify_playback_lifecycle, verify_playback_dma, verify_playback_banks, verify_recorder_input, verify_playback_parent, verify_playback_range,
    verify_playback_switch_history,
    verify_playback_callback_reads,
    verify_playback_notify_context,
    verify_playback_path,
    verify_playback_method,
    verify_playback_gates,
    compare_playback_bit_context,
    verify_playback_optional_window,
)


class BootstrapCheckTest(unittest.TestCase):
    def test_optional_window_preserves_scope_and_requires_boundary_calls(self):
        text = ('mu4_playback_optional_window: start=02532e t=1.000000000 identity=Execute_entry observer_writes=0\n'
                'mu4_playback_optional_terminal: end=025359 samples=3 capped=0 t=1.100000000 '
                'identity=Execute_entry observer_writes=0 audio_acceptance=0')
        bits = [{'capture_group': 3, 'entry_time': 1.01 + i * .02, 'return_time': 1.02 + i * .02, 'caller': caller}
                for i, caller in enumerate(('02532f', '025344', '025354'))]
        self.assertFalse(verify_playback_optional_window(text, bits, [.8, 2.2])['capped'])
        for old, new in (('samples=3', 'samples=4'), ('capped=0', 'capped=1'), ('t=1.100000000', 't=2.200000000'),
                         ('audio_acceptance=0', 'audio_acceptance=1')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                verify_playback_optional_window(text.replace(old, new), bits, [.8, 2.2])
        for index in range(3):
            changed = [dict(row) for row in bits]
            changed[index]['caller'] = '025355'
            with self.assertRaises(ValueError):
                verify_playback_optional_window(text, changed, [.8, 2.2])

    def test_bit_context_comparison_does_not_promote_context_to_operand_acceptance(self):
        entry = ('mu4_playback_bit_context: boundary=entry ordinal=1 address=02510c a=0000000001 '
                 'remaining=0000 reservoir=00000000 index=0000 base=1000 next=8001 caller=025354 group=1 t=1.000000000 '
                 'identity=Execute_entry context_only=1 observer_writes=0')
        returned = entry.replace('boundary=entry', 'boundary=return').replace('address=02510c', 'address=025142').replace(
            'remaining=0000 reservoir=00000000 index=0000', 'remaining=000f reservoir=00008001 index=0001').replace(
            't=1.000000000', 't=1.000001000')
        text = entry + '\n' + returned
        words = {0x2510c: 0xed00, 0x25142: 0xf4e4, 0x25353: 0xe801, 0x25354: 0xf982, 0x25355: 0x510c}
        self.assertEqual(compare_playback_bit_context(text, words, [0.8, 2.2])[0]['comparison'], 'matches')
        self.assertEqual(compare_playback_bit_context(text.replace('boundary=return ordinal=1 address=025142 a=0000000001',
            'boundary=return ordinal=1 address=025142 a=0000000000'), words, [0.8, 2.2])[0]['comparison'], 'differs')
        self.assertEqual(compare_playback_bit_context(text.replace('a=0000000001', 'a=0000000011'), words, [0.8, 2.2])[0]['comparison'], 'unvalidated_input_context')
        for bad in (entry, returned + '\n' + entry, text.replace('context_only=1', 'context_only=0'),
                    text.replace('t=1.000001000', 't=2.200000000')):
            with self.assertRaises(ValueError):
                compare_playback_bit_context(bad, words, [0.8, 2.2])

    def test_gate_accesses_require_all_identities_and_observed_terminal_values(self):
        reports = [f'mu4_playback_gate_terminal: direction={direction} address={address} accesses=0 samples=0 last=unobserved '
                   'selection=common_first_session_window observer_writes=0 audio_acceptance=0'
                   for direction in ('read', 'write') for address in ('ba4f', 'ba51')]
        reports[0] = reports[0].replace('accesses=0 samples=0 last=unobserved', 'accesses=1 samples=1 last=0001')
        entry = ('mu4_playback_gate_access: direction=read address=ba4f value=0001 ordinal=1 access=1 '
                 't=1.000000000 context_pc=023e76 ownership=unvalidated observer_writes=0')
        text = entry + '\n' + '\n'.join(reports)
        self.assertEqual(verify_playback_gates(text, [0.8, 2.2])['read_ba4f']['last'], '0001')
        for old, new in (('ordinal=1', 'ordinal=2'), ('access=1', 'access=0'), ('t=1.000000000', 't=1.800000000'),
                         ('last=0001', 'last=0000'), ('ownership=unvalidated', 'ownership=proven'),
                         ('accesses=1 samples=1', 'accesses=2 samples=1')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                verify_playback_gates(text.replace(old, new), [0.8, 2.2])
        with self.assertRaises(ValueError):
            verify_playback_gates(text.replace(reports[3], reports[2]), [0.8, 2.2])
        with self.assertRaises(ValueError):
            verify_playback_gates(text.replace(reports[3], ''), [0.8, 2.2])
        capped = '\n'.join(entry.replace('ordinal=1 access=1', f'ordinal={i} access={i}')
                           for i in range(1, 33)) + '\n' + '\n'.join(reports)
        capped = capped.replace('accesses=1 samples=1 last=0001', 'accesses=100 samples=32 last=0000')
        result = verify_playback_gates(capped, [0.8, 2.2])['read_ba4f']
        self.assertTrue(result['sample_cap_reached'])
        self.assertEqual(result['last'], '0000')

    def test_method_coverage_preserves_absence_and_checks_source_and_totals(self):
        terminal = ('mu4_playback_method_terminal: entry=1 early=0 body=0 returned=0 result=0 publish=0 '
                    'opcode_mismatch=0 selection=first_verified_aa99_session identity=Execute_entry observer_writes=0 audio_acceptance=0')
        entry = ('mu4_playback_method_entry: site=0 address=023e64 opcode=4a11 ordinal=1 '
                 't=1.000000000 identity=Execute_entry observer_writes=0')
        text = entry + '\n' + terminal
        self.assertEqual(verify_playback_method(text, {0x23e64: 0x4a11}, [0.8, 2.2])['entry'], 1)
        self.assertEqual(verify_playback_method(terminal.replace('entry=1', 'entry=0'), {}, [0.8, 2.2])['entry'], 0)
        for old, new in (('site=0', 'site=1'), ('opcode=4a11', 'opcode=4a10'), ('ordinal=1', 'ordinal=2'),
                         ('entry=1 early', 'entry=2 early'), ('opcode_mismatch=0', 'opcode_mismatch=1'),
                         ('t=1.000000000', 't=2.200000000')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                verify_playback_method(text.replace(old, new), {0x23e64: 0x4a11}, [0.8, 2.2])
        for bad in (entry, terminal, terminal + '\n' + entry, text + '\n' + terminal):
            with self.assertRaises(ValueError):
                verify_playback_method(bad, {0x23e64: 0x4a11}, [0.8, 2.2])

    def test_bounded_path_requires_source_order_and_first_callback_pairing(self):
        words = {0x201d0: 0xf050, 0x201d2: 0xf844, 0x201dd: 0xe734, 0x201de: 0xf074}
        history = [f'{a:06x}/{op:04x}' for a, op in words.items()]
        callbacks = [{'time': 1.0, 'history': history}]
        rows = '\n'.join(f'mu4_playback_path_entry: index={i} address={a:06x} opcode={op:04x} identity=Execute_entry observer_writes=0'
                         for i, (a, op) in enumerate(words.items()))
        terminal = ('mu4_playback_path_terminal: entries=4 t=1.000000000 selection=before_first_d7b0_callback '
                    'identity=Execute_entry observer_writes=0 audio_acceptance=0')
        text = rows + '\n' + terminal
        self.assertEqual(verify_playback_path(text, words, callbacks), history)
        for old, new in (('index=0', 'index=1'), ('opcode=f050', 'opcode=f051'),
                         ('entries=4', 'entries=5'), ('t=1.000000000', 't=1.000000001'),
                         ('audio_acceptance=0', 'audio_acceptance=1')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                verify_playback_path(text.replace(old, new), words, callbacks)
        for bad in (rows, terminal, terminal + '\n' + rows, text + '\n' + terminal):
            with self.assertRaises(ValueError):
                verify_playback_path(bad, words, callbacks)
        with self.assertRaises(ValueError):
            verify_playback_path(text, words, [{'time': 1.0, 'history': history[::-1]}])
        restore = text.replace('mu4_playback_path_', 'mu4_playback_restore_path_').replace(
            'before_first_d7b0_callback', 'before_first_restore_wrapper')
        self.assertEqual(verify_playback_path(restore, words, callbacks, kind='restore'), history)
        with self.assertRaises(ValueError):
            verify_playback_path(restore.replace('entries=4', 'entries=5'), words, callbacks, kind='restore')

    def test_notify_register_context_is_not_an_unpaired_operand_claim(self):
        text = ('mu4_playback_notify_context: ordinal=1 ar1=d796 ar2=d7ac '
                'a=0000000001 b=0000000002 t=1.000000000 context_only=1 observer_writes=0')
        history = [{'time': 1.0}]
        self.assertEqual(verify_playback_notify_context(text, history)[0]['ar2'], 'd7ac')
        for old, new in (('ordinal=1', 'ordinal=2'), ('t=1.000000000', 't=1.000000001'),
                         ('context_only=1', 'context_only=0'), ('observer_writes=0', 'observer_writes=1')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                verify_playback_notify_context(text.replace(old, new), history)
        for bad in ('', text + '\n' + text):
            with self.assertRaises(ValueError):
                verify_playback_notify_context(bad, history)

    def test_callback_bus_reads_require_original_initializer_and_scoped_ordinals(self):
        text = ('mu4_playback_callback_descriptor_read: address=d7b1 value=0419 ordinal=1 '
                't=1.000000000 context_pc=020490 reader_identity=unvalidated observer_writes=0')
        words = {0x2707c: 0x419}
        self.assertEqual(verify_playback_callback_reads(text, words, [0.8, 2.2]), {'d7b1': 1})
        for old, new in (('value=0419', 'value=041a'), ('ordinal=1', 'ordinal=2'),
                         ('address=d7b1', 'address=d7b6'), ('t=1.000000000', 't=1.800000000'),
                         ('reader_identity=unvalidated', 'reader_identity=proven')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                verify_playback_callback_reads(text.replace(old, new), words, [0.8, 2.2])
        with self.assertRaises(ValueError):
            verify_playback_callback_reads(text + '\n' + text, words, [0.8, 2.2])

    def test_switch_history_requires_original_words_and_session_scoping(self):
        text = ('mu4_playback_switch_history: ordinal=1 address=020478 opcode=10e4 ar4=804a '
                'p0=020370/1234 p1=020371/5678 p2=020374/9abc p3=020375/f074 '
                't=1.000000000 identity=Execute_entry selection=live_ar4_context '
                'receiver_ownership=unvalidated observer_writes=0')
        words = {0x20370: 0x1234, 0x20371: 0x5678, 0x20374: 0x9abc, 0x20375: 0xf074}
        self.assertEqual(len(verify_playback_switch_history(text, words, [0.8, 2.2])), 1)
        for old, new in (('ordinal=1', 'ordinal=2'), ('t=1.000000000', 't=2.200000000'),
                         ('p3=020375/f074', 'p3=020375/f075'), ('ar4=804a', 'ar4=804b'),
                         ('receiver_ownership=unvalidated', 'receiver_ownership=proven')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                verify_playback_switch_history(text.replace(old, new), words, [0.8, 2.2])
        with self.assertRaises(ValueError):
            verify_playback_switch_history(text + '\n' + text, words, [0.8, 2.2])
        enqueue = text.replace('switch_history', 'enqueue_history').replace(
            'address=020478 opcode=10e4 ar4=804a', 'address=0202d7 opcode=487e ar2=8044').replace(
            'live_ar4_context', 'live_ar2_context')
        self.assertEqual(len(verify_playback_switch_history(enqueue, words, [0.8, 2.2], kind='enqueue')), 1)
        with self.assertRaises(ValueError):
            verify_playback_switch_history(enqueue.replace('ar2=8044', 'ar2=8045'), words, [0.8, 2.2], kind='enqueue')
        activation = text.replace('switch_history', 'activation_history').replace(
            'address=020478 opcode=10e4 ar4=804a', 'address=020419 opcode=f6be paired_ar2=8044').replace(
            'live_ar4_context', 'paired_enqueue_ar2_context')
        self.assertEqual(len(verify_playback_switch_history(activation, words, [0.8, 2.2], kind='activation')), 1)
        with self.assertRaises(ValueError):
            verify_playback_switch_history(activation.replace('paired_ar2=8044', 'paired_ar2=8045'), words, [0.8, 2.2], kind='activation')
        with self.assertRaises(ValueError):
            verify_playback_switch_history(text, words, [0.8, 2.2], kind='unknown')
        callback = text.replace('switch_history', 'callback_history').replace('ar4=804a', 'ar4=d7b0')
        self.assertEqual(len(verify_playback_switch_history(callback, words, [0.8, 2.2], kind='callback')), 1)
        with self.assertRaises(ValueError):
            verify_playback_switch_history(callback.replace('ar4=d7b0', 'ar4=d7b1'), words, [0.8, 2.2], kind='callback')

    def test_range_census_requires_complete_original_word_coverage_and_scoped_totals(self):
        words = {address: 0 for address in range(0x200a0, 0x20cd2)}
        words[0x200a0], words[0x200a1] = 0x1234, 0xabcd
        rows = ('mu4_playback_range_entry: address=0200a0 opcode=1234 entries=2 selection=first_verified_aa99_session identity=Execute_entry observer_writes=0\n'
                'mu4_playback_range_entry: address=0200a1 opcode=abcd entries=3 selection=first_verified_aa99_session identity=Execute_entry observer_writes=0\n'
                'mu4_playback_range_terminal: first=0200a0 last=020cd1 unique=2 entries=5 opcode_changes=0 '
                'selection=first_verified_aa99_session identity=Execute_entry observer_writes=0 audio_acceptance=0')
        text = self.PARENT + '\n' + rows
        result = verify_playback_range(text, words)
        self.assertEqual(result['source_range_words'], 3122)
        self.assertEqual(result['visited_words'], 2)
        self.assertEqual(result['execute_entries'], 5)
        for old, new in (('address=0200a0', 'address=02009f'), ('opcode=1234 entries=2', 'opcode=1235 entries=2'),
                         ('entries=2 selection', 'entries=0 selection'), ('unique=2', 'unique=3'),
                         ('entries=5 opcode_changes', 'entries=6 opcode_changes'), ('opcode_changes=0', 'opcode_changes=1'),
                         ('audio_acceptance=0', 'audio_acceptance=1'), ('observer_writes=0', 'observer_writes=1')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                verify_playback_range(text.replace(old, new), words)
        for line in rows.splitlines():
            for bad in (text.replace(line, ''), text + '\n' + line):
                with self.assertRaises(ValueError):
                    verify_playback_range(bad, words)
        missing = dict(words)
        del missing[0x20cd1]
        with self.assertRaisesRegex(ValueError, 'complete selected'):
            verify_playback_range(text, missing)

    PARENT = '\n'.join((
        'mu4_playback_execute_entry: address=026d62 opcode=1234 t=0.200000000 sp=1000',
        'mu4_playback_audio_window: start=decoder_execute_entry t=0.800000000 sample_source=converted_sample_bench music_decode=0',
        'mu4_playback_execute_entry: address=026d62 opcode=1234 t=2.200000000 sp=1000',
        'mu4_playback_parent_context: site=0 ordinal=1 address=02a10c opcode=4a11 sp=1000 ar1=0040 ar6=1000 ar7=1000 a=0000000000 b=0000000000 '
        'stack=' + ','.join(['0000'] * 16) + ' frame=unobserved t=1.000000000 identity=Execute_entry context_only=1 observer_writes=0',
        'mu4_playback_parent_terminal: wrapper=1 internal=0 body=0 optional=0 returned=0 loop=0 dispatch=0 select=0 target=0 fallthrough=0 opcode_mismatch=0 first_session_complete=1 '
        'overlay_verified=1 illegal=0 t=3.000000000 selection=first_verified_aa99_session identity=Execute_entry observer_writes=0 audio_acceptance=0',
    ))

    def test_parent_coverage_allows_verified_absence_without_audio_claim(self):
        result = verify_playback_parent(self.PARENT)
        self.assertEqual(result['wrapper'], 1)
        self.assertFalse(result['decoded_audio'])
        self.assertEqual(result['subsystem_identity'], 'unvalidated')
        context = next(line for line in self.PARENT.splitlines() if 'parent_context:' in line)
        body = context.replace('site=0', 'site=2').replace('address=02a10c opcode=4a11', 'address=02a22e opcode=11e1')
        expanded = self.PARENT.replace('wrapper=1 internal=0 body=0', 'wrapper=1 internal=0 body=1') + '\n' + body
        self.assertEqual(verify_playback_parent(expanded)['body'], 1)
        with self.assertRaises(ValueError):
            verify_playback_parent(expanded.replace('opcode=11e1', 'opcode=6deb'))
        absent = '\n'.join(line for line in self.PARENT.splitlines() if 'parent_context:' not in line).replace('wrapper=1', 'wrapper=0')
        self.assertEqual(verify_playback_parent(absent)['wrapper'], 0)

    def test_parent_coverage_rejects_unbounded_mismatched_or_unsafe_observations(self):
        for old, new in (('opcode_mismatch=0', 'opcode_mismatch=1'), ('first_session_complete=1', 'first_session_complete=0'),
                         ('overlay_verified=1', 'overlay_verified=0'), ('illegal=0', 'illegal=1'),
                         ('observer_writes=0', 'observer_writes=1'), ('audio_acceptance=0', 'audio_acceptance=1'),
                         ('context_only=1', 'context_only=0'), ('address=02a10c', 'address=02a139'),
                         ('t=1.000000000', 't=0.500000000'), ('t=1.000000000', 't=2.300000000'),
                         ('sp=1000 ar1=0040', 'sp=0040 ar1=0040'), ('frame=unobserved', 'frame=0000'),
                         ('wrapper=1', 'wrapper=2')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                verify_playback_parent(self.PARENT.replace(old, new))
        for line in self.PARENT.splitlines():
            for bad in (self.PARENT.replace(line, ''), self.PARENT + '\n' + line):
                with self.subTest(line=line), self.assertRaises(ValueError):
                    verify_playback_parent(bad)

    def test_parent_selector_contexts_are_scoped_raw_receiver_values(self):
        context = next(line for line in self.PARENT.splitlines() if 'parent_context:' in line)
        parent = context.replace('site=0', 'site=7').replace('address=02a10c opcode=4a11', 'address=0205ac opcode=6c81')
        words = ','.join(['c6f0'] + ['0000'] * 12 + ['0001', '0000', '0000'])
        selector = ('mu4_playback_selector_context: ordinal=1 receiver=13e0 words=' + words +
                    ' ar1=0001 a=0000000001 t=1.000000000 identity=Execute_entry context_only=1 observer_writes=0')
        text = self.PARENT.replace('select=0', 'select=1') + '\n' + parent + '\n' + selector
        sample = verify_playback_parent(text)['selector_samples'][0]
        self.assertEqual(sample, {'receiver': 0x13e0, 'header': 0xc6f0, 'field13': 1, 'ar1': 1, 'a': 1})
        for old, new in (('receiver=13e0', 'receiver=0021'), ('ordinal=1 receiver', 'ordinal=2 receiver'),
                         ('t=1.000000000 identity', 't=2.500000000 identity'), ('context_only=1', 'context_only=0')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                verify_playback_parent(text.replace(old, new))
        for bad in (text.replace(selector, ''), text + '\n' + selector):
            with self.assertRaises(ValueError):
                verify_playback_parent(bad)

    AC_INPUT = ('mu4_recorder_ac_input: generated=64,64,64,64 '
                'received=60,60,60,60 other=0 other_owner_reads=5 '
                'shape=square periods=64,32 amplitudes=8192,4096 '
                'ownership=actual_channel2_source_read analog_adc=0 decoded_audio=0')

    def test_recorder_input_requires_actual_dma_and_does_not_accept_encoding(self):
        result = verify_recorder_input(self.AC_INPUT)
        self.assertEqual(result['generated_frames'], 128)
        self.assertEqual(result['dma_symbol_reads'], [60] * 4)
        self.assertEqual(result['encoder_consumption'], 'unvalidated')
        self.assertFalse(result['decoded_audio'])
        partial = self.AC_INPUT.replace('generated=64,64,64,64', 'generated=65,64,65,64')
        self.assertEqual(verify_recorder_input(partial)['generated_frames'], 129)

    def test_recorder_input_rejects_bad_shape_missing_dma_and_overclaims(self):
        for old, new in (('generated=64,64,64,64', 'generated=63,65,64,64'),
                         ('generated=64,64,64,64', 'generated=64,64,65,64'),
                         ('generated=64,64,64,64', 'generated=1,0,1,0'),
                         ('received=60,60,60,60', 'received=0,60,60,60'),
                         ('received=60,60,60,60', 'received=65,65,65,65'),
                         ('other=0', 'other=1'), ('periods=64,32', 'periods=32,32'),
                         ('amplitudes=8192,4096', 'amplitudes=1,1'),
                         ('actual_channel2_source_read', 'enabled_channel'),
                         ('analog_adc=0', 'analog_adc=1'), ('decoded_audio=0', 'decoded_audio=1')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                verify_recorder_input(self.AC_INPUT.replace(old, new))
        for text in ('', self.AC_INPUT + '\n' + self.AC_INPUT):
            with self.subTest(text=text), self.assertRaises(ValueError):
                verify_recorder_input(text)

    BANKS = '\n'.join((
        'mu4_playback_banks_scope: first=1000 last=107f upstream_first=7000 upstream_last=73ff start=1.000000000 finish=2.000000000 boundary=data_write context_pc=not_owner observer_writes=0 audio_acceptance=0',
        *(f'mu4_playback_copy_context: ordinal={index} destination={0x1000 + index - 1:04x} '
          f'ar3={0x7042 + index:04x} current=0000 previous=0000 ar4=13e0 '
          'fields=c6f0,0000,0000,ac44,0000,ac44,7000,0400,0080,0000,1000,0040,0040,0001 '
          'context_pc=02a367 source_identity=unvalidated observer_writes=0' for index in range(1, 5)),
        'mu4_playback_bank_context: context_pc=02a367 writes=100 nonzero_driven=0 identity=unvalidated',
        'mu4_playback_banks_terminal: pc=025beb illegal=0 writes=100 nonzero_driven=0 partial=0 contexts=1 overflow=0 t=3.000000000 observer_writes=0 audio_acceptance=0',
        'mu4_playback_upstream_context: context_pc=025dcf writes=200 nonzero_driven=3 identity=unvalidated',
        'mu4_playback_upstream_terminal: writes=200 nonzero_driven=3 partial=0 contexts=1 overflow=0 observer_writes=0 audio_acceptance=0',
    ))

    def test_playback_bank_contexts_are_not_audio_or_writer_acceptance(self):
        result = verify_playback_banks(self.BANKS)
        self.assertEqual(result['banks']['writes'], 100)
        self.assertEqual(result['upstream']['nonzero_driven'], 3)
        self.assertEqual(result['writer_ownership'], 'unvalidated')
        self.assertFalse(result['audio_acceptance'])
        empty = '\n'.join(line for line in self.BANKS.splitlines() if not line.startswith('mu4_playback_upstream_context:'))
        empty = empty.replace('writes=200 nonzero_driven=3 partial=0 contexts=1', 'writes=0 nonzero_driven=0 partial=0 contexts=0')
        self.assertEqual(verify_playback_banks(empty)['upstream']['writes'], 0)

    def test_playback_bank_checker_rejects_incomplete_or_overclaimed_census(self):
        for old, new in (('context_pc=not_owner', 'context_pc=owner'), ('identity=unvalidated', 'identity=Execute'),
                         ('observer_writes=0', 'observer_writes=1'), ('audio_acceptance=0', 'audio_acceptance=1'),
                         ('illegal=0', 'illegal=1'), ('partial=0', 'partial=1'), ('overflow=0', 'overflow=1'),
                         ('t=3.000000000', 't=1.900000000'), ('contexts=1', 'contexts=2'),
                         ('context_pc=02a367 writes=100', 'context_pc=02a367 writes=99'),
                         ('context_pc=025dcf writes=200 nonzero_driven=3', 'context_pc=025dcf writes=200 nonzero_driven=4'),
                         ('ar3=7043', 'ar3=6043'), ('7000,0400', '7000,0800'),
                         ('ordinal=4 destination=1003', 'ordinal=3 destination=1003')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                verify_playback_banks(self.BANKS.replace(old, new))
        for line in self.BANKS.splitlines():
            for bad in (self.BANKS.replace(line, ''), self.BANKS + '\n' + line):
                with self.subTest(line=line), self.assertRaises(ValueError):
                    verify_playback_banks(bad)

    LIFECYCLE = '\n'.join((
        'mu4_playback_lifecycle_scope: boundary=data_write context_pc=not_owner observer_writes=0 audio_acceptance=0',
        'mu4_playback_lifecycle_write: address=373e value=0004 mask=ffff ordinal=1 t=1.000000000 context_pc=020b11 observer_writes=0',
        'mu4_playback_lifecycle_write: address=3742 value=0000 mask=ffff ordinal=1 t=1.010000000 context_pc=029d7e observer_writes=0',
        'mu4_playback_lifecycle_write: address=3750 value=0002 mask=ffff ordinal=1 t=1.020000000 context_pc=03b3e5 observer_writes=0',
        'mu4_playback_lifecycle_write: address=3762 value=0002 mask=ffff ordinal=1 t=1.030000000 context_pc=020b74 observer_writes=0',
        'mu4_playback_control_read: value=0000 t=1.100000000 context_pc=02957a selection=context_range reader_identity=unvalidated observer_writes=0',
        'mu4_playback_lifecycle_write: address=373e value=0005 mask=ffff ordinal=2 t=1.110000000 context_pc=029591 observer_writes=0',
        'mu4_playback_lifecycle_write: address=373e value=0004 mask=ffff ordinal=3 t=2.000000000 context_pc=020b11 observer_writes=0',
        'mu4_playback_lifecycle_terminal: pc=023e1e illegal=0 lifecycle_writes=3 control_writes=1 control_last=0000 control_reads=1 cleanup_context_reads=1 mode_writes=1 continuation_writes=1 observer_writes=0 audio_acceptance=0',
    ))

    def test_passive_playback_lifecycle_witness(self):
        result = verify_playback_lifecycle(self.LIFECYCLE)
        self.assertEqual(result['control_read_time'], 1.1)
        self.assertEqual(result['state5_time'], 1.11)
        self.assertFalse(result['audio_acceptance'])

    def test_passive_playback_lifecycle_rejects_incomplete_or_overclaimed_scope(self):
        for old, new in (('context_pc=not_owner', 'context_pc=owner'),
                         ('observer_writes=0', 'observer_writes=1'),
                         ('reader_identity=unvalidated', 'reader_identity=Execute'),
                         ('context_pc=02957a', 'context_pc=00957a'),
                         ('mask=ffff', 'mask=00ff'), ('illegal=0', 'illegal=1'),
                         ('control_read: value=0000', 'control_read: value=0001'),
                         ('value=0005 mask=ffff ordinal=2', 'value=0002 mask=ffff ordinal=2'),
                         ('value=0004 mask=ffff ordinal=3', 'value=0001 mask=ffff ordinal=3'),
                         ('control_writes=1', 'control_writes=2'),
                         ('audio_acceptance=0', 'audio_acceptance=1')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                verify_playback_lifecycle(self.LIFECYCLE.replace(old, new))
        for line in self.LIFECYCLE.splitlines():
            for bad in (self.LIFECYCLE.replace(line, ''), self.LIFECYCLE + '\n' + line):
                with self.subTest(line=line), self.assertRaises(ValueError):
                    verify_playback_lifecycle(bad)
        for prefix in ('mu4_playback_lifecycle_scope:', 'mu4_playback_lifecycle_terminal:',
                       'mu4_playback_control_read:'):
            with self.subTest(prefix=prefix), self.assertRaises(ValueError):
                verify_playback_lifecycle(self.LIFECYCLE + '\n' + prefix + ' partial')

    TRANSFERS = '\n'.join(
        f'mu4_playback_entry_transfer: target={target} index={index} '
        'p0=002000/f980 p1=002002/f495 p2=002003/f495 p3=002004/f495 '
        'a=0000000000 lifecycle=0004 mode=0002 continuation=0002 identity=Execute_entry'
        for target, count in (('000e41', 1), ('026d62', 8), ('002080', 2)) for index in range(count))
    PLAYBACK = '\n'.join((
        TRANSFERS,
        'mu4_playback_audio_window: start=decoder_execute_entry t=1.234000000 '
        'sample_source=converted_sample_bench music_decode=0',
        'mu4_native_playback_execution: bootstrap_fetch=3 bootstrap_execute=1 '
        'resident_fetch=24 resident_execute=8 filter_calls=10 filter_iterations=640 '
        'filter_returns=10 instruction_entries=20000 loader_execute=2 observer=Execute_entry retirement_claim=0',
        'mu4_native_playback_audio: tx_words=100 din_words=99 tx_nonzero=12 din_nonzero=11 '
        'tx_hash=12345678 din_hash=87654321 clocks=5000 frames=50 power=0000 format=0053 '
        'rate=0023 active=0001 tx_last_nonzero=1.235000000 din_last_nonzero=1.235000040 '
        'window=decoder_entry stream_alignment=unvalidated music_decode=0 analog_audio=0',
        'mu4_native_playback_observe: requests=1 inspections=1 decoder_loads=1 replies=14 '
        'input_cursor=12 title_verified=1 overlay_verified=1 pc=02a2d5 illegal=0 audio_acceptance=0',
    ))

    def test_playback_observation_is_not_audio_acceptance(self):
        result = verify_playback_observation(self.PLAYBACK)
        self.assertEqual(result['bootstrap_execute'], 1)
        self.assertEqual(result['filter_iterations'], 640)
        self.assertEqual(result['entry_transfer_reports'], 11)
        self.assertEqual(result['tx_hash'],'12345678')
        self.assertEqual(result['din_hash'],'87654321')
        self.assertNotEqual(result['tx_words'], result['din_words'])
        self.assertNotIn('audio_acceptance', result)

    def playback_dma_log(self):
        events = []
        for index in range(1, 9):
            kind, ordinal, value = ('prefix', index, 0) if index <= 4 else ('nonzero', index - 4, index - 4)
            events.append(f'mu4_playback_dma_word: kind={kind} ordinal={ordinal} read={index} '
                          f'address={0xba79 + index:04x} word={value:04x} t=1.234{index:03d}000 '
                          'channel=3 space=data ownership=active_source_read')
        summary = ('mu4_playback_dma_source: reads=100 nonzero=12 minimum=ba79 maximum=bb78 '
                   'last_nonzero=1.235000000 channel=3 space=data ownership=active_source_read '
                   'window=decoder_entry tx_pairing=unvalidated music_decode=0')
        return self.PLAYBACK.replace('mu4_native_playback_execution:', '\n'.join(events) + '\nmu4_native_playback_execution:').replace(
            'mu4_native_playback_observe:', summary + '\nmu4_native_playback_observe:')

    def test_actual_playback_dma_observation_does_not_claim_pairing(self):
        result = verify_playback_dma(self.playback_dma_log())
        self.assertEqual(result['dma_reads'], 100)
        self.assertEqual(result['dma_nonzero'], 12)
        self.assertEqual(result['tx_pairing'], 'unvalidated')
        self.assertFalse(result['music_decode'])
        silent = '\n'.join(line for line in self.playback_dma_log().splitlines() if 'kind=nonzero' not in line)
        silent = silent.replace('reads=100 nonzero=12', 'reads=100 nonzero=0').replace(
            'last_nonzero=1.235000000 channel=3', 'last_nonzero=0.000000000 channel=3')
        self.assertEqual(verify_playback_dma(silent)['dma_nonzero'], 0)

    def test_playback_dma_rejects_incomplete_or_overclaimed_evidence(self):
        output = self.playback_dma_log()
        for old, new in (('channel=3', 'channel=2'), ('space=data', 'space=program'),
                         ('ownership=active_source_read', 'ownership=enabled_source'),
                         ('tx_pairing=unvalidated', 'tx_pairing=validated'),
                         ('music_decode=0', 'music_decode=1'),
                         ('reads=100 nonzero=12', 'reads=10 nonzero=12'),
                         ('minimum=ba79', 'minimum=bb79'),
                         ('read=5', 'read=3'), ('ordinal=4 read=4', 'ordinal=3 read=4'),
                         ('word=0001', 'word=0000'), ('t=1.234005000', 't=1.000000000'),
                         ('last_nonzero=1.235000000', 'last_nonzero=0.000000000')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                verify_playback_dma(output.replace(old, new))
        for line in output.splitlines():
            if not line.startswith('mu4_playback_dma_'):
                continue
            for bad in (output.replace(line, ''), output + '\n' + line):
                with self.subTest(line=line), self.assertRaises(ValueError):
                    verify_playback_dma(bad)
        for prefix in ('mu4_playback_dma_word:', 'mu4_playback_dma_source:'):
            with self.assertRaises(ValueError):
                verify_playback_dma(output + '\n' + prefix + ' partial')
        with self.assertRaises(ValueError):
            verify_playback_dma(output.replace('address=ba7a word=0000', 'address=ba7a word=0001'))

    def test_playback_observation_rejects_scope_changes(self):
        for old, new in (('observer=Execute_entry', 'observer=fetch'),
                         ('retirement_claim=0', 'retirement_claim=1'),
                         ('stream_alignment=unvalidated', 'stream_alignment=validated'),
                         ('music_decode=0', 'music_decode=1'), ('audio_acceptance=0', 'audio_acceptance=1'),
                         ('illegal=0', 'illegal=1'), ('overlay_verified=1', 'overlay_verified=0'),
                         ('bootstrap_execute=1', 'bootstrap_execute=0'),
                         ('resident_execute=8', 'resident_execute=0'),
                         ('loader_execute=2', 'loader_execute=0'),
                         ('target=026d62', 'target=006d62'),
                         ('target=002080', 'target=022080'),
                         ('target=000e41 index=0', 'target=000e41 index=1'),
                         ('instruction_entries=20000', 'instruction_entries=0'),
                         ('filter_iterations=640', 'filter_iterations=641'),
                         ('tx_nonzero=12', 'tx_nonzero=101'), ('din_nonzero=11', 'din_nonzero=100')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                verify_playback_observation(self.PLAYBACK.replace(old, new))

    def test_playback_nonzero_times_follow_the_window(self):
        for old, new in (('tx_last_nonzero=1.235000000', 'tx_last_nonzero=0.000000000'),
                         ('din_last_nonzero=1.235000040', 'din_last_nonzero=1.000000000'),
                         ('tx_nonzero=12', 'tx_nonzero=0'),
                         ('identity=Execute_entry', 'identity=fetch')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                verify_playback_observation(self.PLAYBACK.replace(old, new))

    def test_playback_transfer_provenance_has_a_bounded_cap(self):
        template = next(line for line in self.TRANSFERS.splitlines()
                        if 'target=026d62 index=0 ' in line)
        extra = '\n'.join(template.replace('index=0 ', f'index={index} ')
                          for index in range(8, 16))
        output = self.PLAYBACK.replace('resident_execute=8', 'resident_execute=20')
        output = output.replace('mu4_playback_audio_window:', extra + '\nmu4_playback_audio_window:')
        self.assertEqual(verify_playback_observation(output)['entry_transfer_reports'], 19)

    def test_playback_observation_requires_each_report_once_in_order(self):
        lines = self.PLAYBACK.splitlines()
        for line in lines:
            for bad in (self.PLAYBACK.replace(line, ''), self.PLAYBACK + '\n' + line):
                with self.subTest(line=line), self.assertRaises(ValueError):
                    verify_playback_observation(bad)
        with self.assertRaises(ValueError):
            verify_playback_observation('\n'.join(reversed(lines)))
        with self.assertRaises(ValueError):
            verify_playback_observation(self.PLAYBACK + '\nmu4_native_playback_audio: partial')

    RESIDENT = '\n'.join((
        'mu4_original_resident_program: PASS checked_words=48261 mismatch_words=0 '
        'excluded_data_words=3660 source_words=51921 before_resident_execution=1 '
        'physical_mapping=unvalidated full_boot=0',
        'mu4_original_resident_data: PASS checked_words=3656 mismatch_words=0 '
        'excluded_register_words=4 source_data_words=3660 before_resident_execution=1 '
        'physical_mapping=unvalidated full_boot=0',
    ))

    def test_resident_memory_complete_scope(self):
        verify_resident_memory(self.RESIDENT)

    def test_resident_memory_rejects_partial_or_changed_scope(self):
        for old, new in (('48261', '48260'), ('3656', '3655'),
                         ('excluded_register_words=4', 'excluded_register_words=5'),
                         ('source_words=51921', 'source_words=51920'),
                         ('source_data_words=3660', 'source_data_words=3656'),
                         ('excluded_data_words=3660', 'excluded_data_words=0'),
                         ('before_resident_execution=1', 'before_resident_execution=0'),
                         ('physical_mapping=unvalidated', 'physical_mapping=validated'),
                         ('full_boot=0', 'full_boot=1')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                verify_resident_memory(self.RESIDENT.replace(old, new))

    def test_resident_memory_rejects_failure_or_duplicate_evidence(self):
        for bad in (self.RESIDENT.replace('PASS', 'MISMATCH'),
                    self.RESIDENT.replace('mismatch_words=0', 'mismatch_words=1'),
                    self.RESIDENT + '\n' + self.RESIDENT,
                    self.RESIDENT + '\n' + self.RESIDENT.splitlines()[1].replace('full_boot=0', 'full_boot=1'),
                    self.RESIDENT.splitlines()[0], self.RESIDENT.splitlines()[1],
                    '\n'.join(reversed(self.RESIDENT.splitlines())),
                    self.RESIDENT + '\nmu4_original_resident_data_mismatch: address=0080',
                    self.RESIDENT + '\n[LUA ERROR] observer failed'):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                verify_resident_memory(bad)

    def test_dsp_service_route_requires_forward_then_both_service_ingresses(self):
        forward = ('5510_input_dsp_forward: caller=003a53f7 object=0012e9cc '
                   'length=0a type=8e transport=1e destination=00 source=02 class=d0')
        ingress = ('5510_input_serial_ingress: caller=002f58b1 object=0012eaac '
                   'length=0001 transport=1e destination=00 source=02 class=')
        text = '\n'.join((forward, ingress + '01', ingress + '04'))
        verify_dsp_service_route(text)
        for bad in (text.replace(forward, ''), text.replace(ingress + '04', ''),
                    text.replace('source=02', 'source=28'),
                    '\n'.join((ingress + '01', ingress + '04', forward)),
                    text.replace('caller=003a53f7', 'caller=003a53f5')):
            with self.assertRaises(ValueError):
                verify_dsp_service_route(bad)

    def test_input_timer_requires_arm_then_delivery(self):
        armed = '5510_input_timer_armed: link=0010b824 delta=02ff flags=01 state=02 owner=1d'
        delivered = '5510_input_timer_dispatch: event=01e7'
        verify_input_timer(armed + '\n' + delivered)
        for text in (armed, delivered, delivered + '\n' + armed,
                     (armed + '\n' + delivered).replace('owner=1d', 'owner=01'),
                     (armed + '\n' + delivered).replace('state=02', 'state=01')):
            with self.assertRaises(ValueError):
                verify_input_timer(text)

    SERIAL = '\n'.join((
        '5510_input_tasks_created: task29_state=05 stack=0012e6b0',
        'address=00120c70 data=01', 'address=0011cddc data=01',
        'address=0011cddc data=00', 'address=00120c70 data=00',
        '5510_input_startup_gate: ready=01 busy=00,00,00,00 fiqmask=c8',
    ))

    def test_serial_queues_drain_in_order(self):
        verify_serial_readiness(self.SERIAL)

    def test_input_lifecycle_requires_positive_consumers(self):
        events = (
            '5510_task_enter: index=1d entry=00335f1e',
            '5510_input_serial_ui_init: mode=00 caller=00335f25',
            '5510_input_serial_ui_receive: message=0012ea8c',
        )
        verify_input_lifecycle('\n'.join(events))
        for text in ('\n'.join(events[:1]), '\n'.join(events[1:]),
                     '\n'.join(reversed(events))):
            with self.assertRaises(ValueError):
                verify_input_lifecycle(text)

    def test_serial_final_state_alone_is_insufficient(self):
        with self.assertRaises(ValueError):
            verify_serial_readiness(self.SERIAL.splitlines()[-1])

    def test_disabled_timer_or_missing_drain_fails(self):
        for text in (self.SERIAL.replace('ready=01', 'ready=00'),
                     self.SERIAL.replace('address=00120c70 data=00', ''),
                     '\n'.join(reversed(self.SERIAL.splitlines()))):
            with self.assertRaises(ValueError):
                verify_serial_readiness(text)

    def test_ordered_native_boundary(self):
        verify('\n'.join(CHAIN))

    def test_hybrid_requires_own_service_consumer(self):
        verify('\n'.join(RUNTIME_CHAIN), runtime=True)
        with self.assertRaises(ValueError):
            verify('\n'.join(event for event in RUNTIME_CHAIN if 'service_reply' not in event), runtime=True)

    def test_missing_reordered_or_forced_evidence(self):
        for text in ('\n'.join(CHAIN[:-1]), '\n'.join(reversed(CHAIN)),
                     '\n'.join(CHAIN) + '\nruntime_hle_handoff'):
            with self.assertRaises(ValueError):
                verify(text)


if __name__ == '__main__':
    unittest.main()

import unittest
from tools.noki5510_bootstrap_check import (
    CHAIN, RUNTIME_CHAIN, verify, verify_serial_readiness, verify_input_lifecycle,
    verify_input_timer, verify_dsp_service_route, verify_resident_memory, verify_playback_observation,
)


class BootstrapCheckTest(unittest.TestCase):
    PLAYBACK = '\n'.join((
        'mu4_playback_audio_window: start=decoder_execute_entry t=1.234000000 '
        'sample_source=converted_sample_bench music_decode=0',
        'mu4_native_playback_execution: bootstrap_fetch=3 bootstrap_execute=1 '
        'resident_fetch=24 resident_execute=8 filter_calls=10 filter_iterations=640 '
        'filter_returns=10 instruction_entries=20000 observer=Execute_entry retirement_claim=0',
        'mu4_native_playback_audio: tx_words=100 din_words=99 tx_nonzero=12 din_nonzero=11 '
        'tx_hash=12345678 din_hash=87654321 clocks=5000 frames=50 power=0000 format=0053 '
        'rate=0023 active=0001 window=decoder_entry stream_alignment=unvalidated music_decode=0 analog_audio=0',
        'mu4_native_playback_observe: requests=1 inspections=1 decoder_loads=1 replies=14 '
        'input_cursor=12 title_verified=1 overlay_verified=1 pc=02a2d5 illegal=0 audio_acceptance=0',
    ))

    def test_playback_observation_is_not_audio_acceptance(self):
        result = verify_playback_observation(self.PLAYBACK)
        self.assertEqual(result['bootstrap_execute'], 1)
        self.assertEqual(result['filter_iterations'], 640)
        self.assertNotEqual(result['tx_words'], result['din_words'])
        self.assertNotIn('audio_acceptance', result)

    def test_playback_observation_rejects_scope_changes(self):
        for old, new in (('observer=Execute_entry', 'observer=fetch'),
                         ('retirement_claim=0', 'retirement_claim=1'),
                         ('stream_alignment=unvalidated', 'stream_alignment=validated'),
                         ('music_decode=0', 'music_decode=1'), ('audio_acceptance=0', 'audio_acceptance=1'),
                         ('illegal=0', 'illegal=1'), ('overlay_verified=1', 'overlay_verified=0'),
                         ('bootstrap_execute=1', 'bootstrap_execute=0'),
                         ('resident_execute=8', 'resident_execute=0'),
                         ('instruction_entries=20000', 'instruction_entries=0'),
                         ('filter_iterations=640', 'filter_iterations=641'),
                         ('tx_nonzero=12', 'tx_nonzero=101'), ('din_nonzero=11', 'din_nonzero=100')):
            with self.subTest(mutation=old), self.assertRaises(ValueError):
                verify_playback_observation(self.PLAYBACK.replace(old, new))

    def test_playback_observation_requires_each_report_once_in_order(self):
        lines = self.PLAYBACK.splitlines()
        for line in lines:
            for bad in (self.PLAYBACK.replace(line, ''), self.PLAYBACK + '\n' + line):
                with self.subTest(line=line), self.assertRaises(ValueError):
                    verify_playback_observation(bad)
        with self.assertRaises(ValueError):
            verify_playback_observation('\n'.join(reversed(lines)))

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

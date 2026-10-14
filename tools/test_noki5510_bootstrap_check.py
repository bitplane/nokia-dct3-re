import unittest
from tools.noki5510_bootstrap_check import (
    CHAIN, RUNTIME_CHAIN, verify, verify_serial_readiness, verify_input_lifecycle,
    verify_input_timer, verify_dsp_service_route, verify_resident_memory,
)


class BootstrapCheckTest(unittest.TestCase):
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

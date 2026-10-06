import unittest
from tools.noki5510_bootstrap_check import (
    CHAIN, RUNTIME_CHAIN, verify, verify_serial_readiness, verify_input_lifecycle,
)


class BootstrapCheckTest(unittest.TestCase):
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

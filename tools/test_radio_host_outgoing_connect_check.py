import unittest
from tools.radio_host_outgoing_connect_check import verify, verify_restored

GOOD = '''gsm_call_adapter: request id=1 epoch=1 digits=1234567 clients=1
gsm_call_adapter: decision id=2 outcome=1 result=rejected
gsm_session: outgoing decision queued id=1 outcome=0
gsm_call_adapter: decision id=1 outcome=0 result=accepted
gsm_call_adapter: decision id=1 outcome=0 result=rejected
gsm_call_adapter: state id=1 epoch=1 phase=connected
gsm_call_adapter: state id=1 epoch=1 phase=ended
'''


class HostConnectTest(unittest.TestCase):
    def test_restoration_requires_attached_client_stale_rejection_and_physical_release(self):
        lines = [
            'gsm_call_adapter: request id=1 epoch=1 digits=1234567 clients=1',
            'gsm_call_adapter: decision id=1 outcome=0 result=accepted',
            'gsm_call_adapter: state id=1 epoch=1 phase=connected',
            '5210_pin_state: event=saved', '5210_pin_state: event=restored',
            'gsm_call_adapter: request id=1 epoch=2 digits=1234567 clients=1',
            'gsm_call_adapter: state id=1 epoch=2 phase=connected',
            'gsm_call_adapter: stale epoch=1 current=2 type=decision id=1',
            '5210_pin_state_physical: key=End',
            'gsm_call_adapter: state id=1 epoch=2 phase=ended',
        ]
        verify_restored('\n'.join(lines), '1234567')
        for missing in range(len(lines)):
            with self.subTest(missing=missing), self.assertRaises(ValueError):
                verify_restored('\n'.join(line for i, line in enumerate(lines) if i != missing),
                                '1234567')
        with self.assertRaises(ValueError):
            verify_restored('\n'.join(lines).replace('clients=1', 'clients=0'), '1234567')
        with self.assertRaises(ValueError):
            verify_restored('\n'.join(lines + [lines[1]]), '1234567')

    def test_correlated_connect(self):
        verify(GOOD, '1234567')

    def test_wrong_number(self):
        with self.assertRaisesRegex(ValueError, 'own host request'):
            verify(GOOD, '123')

    def test_duplicate_acceptance(self):
        with self.assertRaisesRegex(ValueError, 'exactly one'):
            verify(GOOD + 'gsm_call_adapter: decision id=1 outcome=0 result=accepted', '1234567')

    def test_unclosed_call(self):
        with self.assertRaisesRegex(ValueError, 'ended'):
            verify(GOOD.replace('phase=ended', 'phase=connected'), '1234567')

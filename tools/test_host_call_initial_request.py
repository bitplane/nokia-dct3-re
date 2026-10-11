import unittest
import json
from unittest.mock import AsyncMock

from tools.run_host_call_adapter_gate import verify_initial_request, observe_restored_call


REQUEST = dict(type='outgoing_call', request_id=1, epoch=1,
               digits='5551234', decision_pending=True)


class HostCallInitialRequestTest(unittest.TestCase):
    def test_pending_request_is_accepted(self):
        verify_initial_request(REQUEST, '5551234')

    def test_decided_or_missing_pending_flag_is_rejected(self):
        for event in ({**REQUEST, 'decision_pending': False},
                      {key: value for key, value in REQUEST.items() if key != 'decision_pending'}):
            with self.assertRaises(RuntimeError):
                verify_initial_request(event, '5551234')

    def test_wrong_digits_or_identity_is_rejected(self):
        for event in ({**REQUEST, 'digits': '123'}, {**REQUEST, 'request_id': 2}):
            with self.assertRaises(RuntimeError):
                verify_initial_request(event, '5551234')


class HostCallRestoreObserverTest(unittest.IsolatedAsyncioTestCase):
    async def test_connected_next_epoch_and_ended_without_reconnecting(self):
        socket = AsyncMock()
        socket.recv.side_effect = [json.dumps(dict(type='outgoing_call_state',
            request_id=1, epoch=epoch, phase=phase)) for epoch, phase in (
                (1, 'connected'), (2, 'connected'), (2, 'ended'))]
        await observe_restored_call(socket, 1, 1)
        self.assertEqual(json.loads(socket.send.call_args.args[0]), dict(
            type='outgoing_call_decision', epoch=1, request_id=1, decision='connect'))

    async def test_missing_predecessor_wrong_epoch_or_identity_is_rejected(self):
        for event in (dict(epoch=2, request_id=1, phase='connected'),
                      dict(epoch=3, request_id=1, phase='connected'),
                      dict(epoch=1, request_id=2, phase='connected'),
                      dict(epoch=1, request_id=1, phase='ended')):
            socket = AsyncMock()
            socket.recv.return_value = json.dumps(dict(type='outgoing_call_state', **event))
            with self.subTest(event=event), self.assertRaises(RuntimeError):
                await observe_restored_call(socket, 1, 1)


if __name__ == '__main__':
    unittest.main()

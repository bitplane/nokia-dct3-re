import queue
import unittest
from collections import deque
from types import SimpleNamespace
from unittest.mock import Mock

from tools.dct3_sip_bridge import MediaQueues, SipEndpoint, incoming_caller, failure_messages


class SipDialogMediaOwnershipTest(unittest.TestCase):
    def endpoint(self):
        class Port:
            def __init__(self):
                self.createPort = Mock()
                self.startTransmit = Mock()

        class Call:
            def __init__(self, account, call_id):
                self.getInfo = Mock(return_value=SimpleNamespace(media=[
                    SimpleNamespace(type=1, status=2, index=0)]))
                self.audio = SimpleNamespace(startTransmit=Mock())
                self.getAudioMedia = Mock(return_value=self.audio)

        endpoint = SipEndpoint.__new__(SipEndpoint)
        endpoint.account = object()
        endpoint.events = deque()
        endpoint.pj = SimpleNamespace(Call=Call, AudioMediaPort=Port,
            MediaFormatAudio=SimpleNamespace, PJMEDIA_TYPE_AUDIO=1,
            PJSUA_CALL_MEDIA_ACTIVE=2, PJSIP_INV_STATE_CONFIRMED=3,
            PJSIP_INV_STATE_DISCONNECTED=4)
        return endpoint

    def test_replaced_dialog_cannot_publish_call_state(self):
        for replacement_identity in ((1, 2), (1, 1)):
            for state, phase in ((3, 'confirmed'), (4, 'disconnected')):
                with self.subTest(identity=replacement_identity, state=state):
                    endpoint = self.endpoint()
                    endpoint._new_call((1, 1))
                    old = endpoint.call
                    old.getInfo.return_value = SimpleNamespace(state=state, lastStatusCode=200)
                    endpoint._new_call(replacement_identity)
                    current = endpoint.call
                    old.onCallState(None)
                    self.assertEqual(list(endpoint.events), [])
                    old.getInfo.assert_not_called()
                    current.getInfo.return_value = SimpleNamespace(state=state, lastStatusCode=200)
                    current.onCallState(None)
                    self.assertEqual(list(endpoint.events), [(replacement_identity, phase, 200)])

    def test_released_dialog_cannot_publish_call_state(self):
        endpoint = self.endpoint()
        endpoint._new_call((3, 7))
        call = endpoint.call
        call.getInfo.return_value = SimpleNamespace(state=4, lastStatusCode=200)
        endpoint.release((3, 7))
        call.onCallState(None)
        self.assertEqual(list(endpoint.events), [])
        call.getInfo.assert_not_called()

    def test_replaced_dialog_cannot_attach_to_new_call_media(self):
        for replacement_identity in ((1, 2), (1, 1)):
            with self.subTest(identity=replacement_identity):
                endpoint = self.endpoint()
                endpoint._new_call((1, 1))
                old = endpoint.call
                old_media = endpoint.media
                endpoint._new_call(replacement_identity)
                current = endpoint.call
                old.onCallMediaState(None)
                self.assertIsNone(endpoint.media_port)
                old.getInfo.assert_not_called()
                old.getAudioMedia.assert_not_called()
                self.assertIsNot(endpoint.media, old_media)
                current.onCallMediaState(None)
                port = endpoint.media_port
                current.audio.startTransmit.assert_called_once_with(port)
                port.startTransmit.assert_called_once_with(current.audio)
                old.onCallMediaState(None)
                self.assertIs(endpoint.media_port, port)
                port.startTransmit.assert_called_once_with(current.audio)

    def test_released_dialog_cannot_recreate_media_port(self):
        endpoint = self.endpoint()
        endpoint._new_call((3, 7))
        call = endpoint.call
        call.onCallMediaState(None)
        call.getInfo.reset_mock()
        endpoint.release((3, 7))
        call.onCallMediaState(None)
        self.assertIsNone(endpoint.media_port)
        call.getInfo.assert_not_called()


class SipMediaQueuesTest(unittest.TestCase):
    def test_busy_uses_existing_gsm_busy_decision_without_second_clear(self):
        for status in (486, 600):
            self.assertEqual(failure_messages((2, 7), status), [{
                'epoch': 2, 'request_id': 7, 'type': 'outgoing_call_decision',
                'decision': 'busy'}])

    def test_final_unavailable_suppresses_connect_then_requests_release(self):
        self.assertEqual(failure_messages((2, 7), 480), [
            {'epoch': 2, 'request_id': 7, 'type': 'outgoing_call_decision', 'decision': 'no_answer'},
            {'epoch': 2, 'request_id': 7, 'type': 'outgoing_call_terminate', 'cause': 18}])

    def test_other_documented_causes_and_explicit_fallback(self):
        for status, cause in ((403, 21), (404, 1), (408, 102),
                              (500, 41), (503, 41), (603, 21), (604, 1), (488, 41)):
            replies = failure_messages((1, 3), status)
            self.assertEqual(replies[0]['decision'], 'no_answer')
            self.assertEqual(replies[1]['cause'], cause)

    def test_numeric_sip_identity_is_required_for_incoming_cli(self):
        self.assertEqual(incoming_caller('sip:5551234@127.0.0.1:25100'), '5551234')
        self.assertEqual(incoming_caller('"Caller" <sip:123@localhost>'), '123')
        self.assertEqual(incoming_caller('"<sip:999@fake>" <sip:123@localhost>'), '123')
        for uri in ('sip:alice@localhost', 'sip:12x@localhost',
                    'sip:' + '1' * 21 + '@localhost', 'tel:123',
                    '"sip:123@fake" <sip:alice@localhost>'):
            self.assertIsNone(incoming_caller(uri))

    def test_bounded_media_queue_does_not_block(self):
        media = MediaQueues()
        for index in range(12):
            media.put(media.uplink, bytes([index]) * 320)
        self.assertEqual(media.dropped, 4)
        self.assertEqual(media.uplink.qsize(), 8)
        self.assertEqual([media.uplink.get_nowait()[0] for _ in range(8)], list(range(8)))
        with self.assertRaises(queue.Empty):
            media.uplink.get_nowait()

    def test_wrong_pcm_geometry_is_not_queued(self):
        media = MediaQueues()
        for data in (b'', bytes(318), bytes(640)):
            media.put(media.downlink, data)
        self.assertTrue(media.downlink.empty())
        media.put(media.downlink, bytes(320))
        self.assertEqual(media.downlink.qsize(), 1)

    def test_downlink_backlog_is_paced_without_catchup_bursts(self):
        media = MediaQueues()
        for index in range(8):
            media.put(media.downlink, bytes([index]) * 320)
        self.assertEqual(media.take_downlink(1.0)[0], 0)
        for now in (1.0, 1.005, 1.019):
            with self.assertRaises(queue.Empty):
                media.take_downlink(now)
        self.assertEqual(media.take_downlink(1.021)[0], 1)
        self.assertEqual(media.take_downlink(2.0)[0], 2)
        with self.assertRaises(queue.Empty):
            media.take_downlink(2.0)
        self.assertEqual(media.take_downlink(2.021)[0], 3)

    def test_empty_downlink_does_not_delay_the_next_arrival(self):
        media = MediaQueues()
        with self.assertRaises(queue.Empty):
            media.take_downlink(3.0)
        media.put(media.downlink, bytes(320))
        self.assertEqual(media.take_downlink(3.001), bytes(320))


if __name__ == '__main__':
    unittest.main()

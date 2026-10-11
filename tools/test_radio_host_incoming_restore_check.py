import unittest
import json
from tools.radio_host_incoming_restore_check import verify, verify_media


TRACE = '''SIM status ins=20 sw=9000
gsm_call_adapter: network registered=1 arfcn=86
gsm_call_adapter: incoming id=1 result=accepted
gsm_call_adapter: incoming state id=1 epoch=1 phase=paging
5210_pin_physical: key=Send
gsm_call_adapter: incoming state id=1 epoch=1 phase=connected
5210_pin_state: event=saved
5210_pin_state: event=restored
gsm_call_adapter: incoming state id=1 epoch=2 phase=connected
gsm_call_adapter: stale epoch=1 current=2 type=termination id=1 result=rejected
5210_pin_state_physical: key=End
GSM service uplink sapi=0 pd=03 message=25
GSM service downlink kind=25 sapi=0 pd=03 message=2d
GSM service uplink sapi=0 pd=03 message=2a
gsm_call_adapter: incoming state id=1 epoch=2 phase=ended
'''


class IncomingRestoreTest(unittest.TestCase):
    def test_media_requires_published_cursor_continuity_before_end(self):
        accepted = ''.join(f'gsm_call_adapter: media direction=downlink id=1 sequence={n} result=accepted\n'
                           for n in (42, 43))
        trace = TRACE.replace('type=termination', 'type=media').replace(
            '5210_pin_state_physical: key=End', accepted + '5210_pin_state_physical: key=End')
        console = json.dumps(dict(type='incoming_call_state', epoch=2, request_id=1,
                                  phase='connected', media_downlink_sequence=42))
        verify_media(trace, console, 2)
        for text, output in ((trace, ''), (trace.replace('sequence=43', 'sequence=44'), console),
                             (trace.replace(accepted, '') + accepted, console),
                             (trace, console.replace('42', '41')),
                             (trace, console.replace('"epoch": 2', '"epoch": 1'))):
            with self.subTest(output=output), self.assertRaises(ValueError):
                verify_media(text, output, 2)

    def test_complete_lifecycle(self):
        verify(TRACE)

    def test_every_checkpoint_is_required(self):
        for line in TRACE.strip().splitlines():
            with self.subTest(line=line), self.assertRaises(ValueError):
                verify(TRACE.replace(line, '', 1))

    def test_replayed_host_request_or_paging_is_rejected(self):
        for line in ('gsm_call_adapter: incoming id=1 result=accepted',
                     'gsm_call_adapter: incoming state id=1 epoch=2 phase=paging'):
            with self.subTest(line=line), self.assertRaises(ValueError):
                verify(TRACE + line)

    def test_missing_authentication_and_wrong_key_are_rejected(self):
        for before, after in (('sw=9000', 'sw=9804'), ('key=Send', 'key=Send_extra'),
                              ('current=2', 'current=3')):
            with self.subTest(before=before), self.assertRaises(ValueError):
                verify(TRACE.replace(before, after))

import unittest
from tools.noki6210_ussd_termination_check import verify


TEXT='\n'.join([
    'gsm_ss: network_initiated operation=request',
    'incoming ussd termination id=1 result=rejected',
    'incoming ussd termination id=999 result=rejected',
    'gsm_ss: network_terminated transaction=0b',
    'incoming ussd termination id=1 result=accepted',
    'pd=0b message=2a length=2',
    'pd=06 message=0d length=3',
    'incoming ussd state id=1 epoch=1 phase=cancelled',
    'incoming ussd termination id=1 result=rejected',
    '6210_host_ussd_termination: complete=1 firmware_state_forcing=0'])
EVENTS=[dict(type='incoming_ussd_state',request_id=1,epoch=1,phase=p)
        for p in ('queued','cancelled')]


class TerminationCheckTest(unittest.TestCase):
    def test_restored_epoch_owns_termination(self):
        text=TEXT.replace('epoch=1 phase=cancelled','epoch=2 phase=cancelled')
        text=text.replace('gsm_ss: network_terminated',
                          '6210_ussd_termination_restore: architecture=pass\ngsm_ss: network_terminated')
        events=[dict(type='call_adapter_ready',epoch=1),EVENTS[0],
                dict(type='call_adapter_ready',epoch=2),
                dict(EVENTS[0],epoch=2),dict(EVENTS[1],epoch=2)]
        verify(text,events,restore=True)
        for bad in (events[:-1]+[EVENTS[1]],events[:2]+events[3:],events+[events[-1]]):
            with self.subTest(events=bad),self.assertRaises(ValueError):
                verify(text,bad,restore=True)
        with self.assertRaises(ValueError):
            verify(TEXT,events,restore=True)
        with self.assertRaises(ValueError):
            verify(text+'\ngsm_ss: network_initiated operation=request',events,restore=True)

    def test_complete_network_release(self):
        verify(TEXT,EVENTS)

    def test_missing_and_unordered_boundaries(self):
        for needle in ('network_terminated','pd=0b','pd=06','complete=1'):
            with self.subTest(needle=needle),self.assertRaises(ValueError):
                verify(TEXT.replace(needle,'absent'),EVENTS)
        with self.assertRaises(ValueError):
            verify(TEXT.replace('pd=0b message=2a length=2','pd=06 message=0d length=3')
                   .replace('pd=06 message=0d length=3\nincoming','pd=0b message=2a length=2\nincoming'),EVENTS)

    def test_no_fabricated_result(self):
        with self.assertRaises(ValueError):
            verify(TEXT,EVENTS+[dict(type='incoming_ussd_response')])
        with self.assertRaises(ValueError):
            verify(TEXT+'\ngsm_ss: network_result',EVENTS)

    def test_identity_duplicates_and_early_success(self):
        for events in (EVENTS[1:],EVENTS+EVENTS[1:],
                       [dict(e,epoch=2) for e in EVENTS],
                       [dict(e,request_id=2) for e in EVENTS]):
            with self.subTest(events=events),self.assertRaises(ValueError):
                verify(TEXT,events)
        with self.assertRaises(ValueError):
            verify(TEXT.replace('id=999 result=rejected','id=999 result=accepted'),EVENTS)


if __name__=='__main__':unittest.main()

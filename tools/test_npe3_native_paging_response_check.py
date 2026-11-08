import unittest
from unittest.mock import patch
from tools.npe3_native_paging_response_check import check


class NativePagingResponseTest(unittest.TestCase):
    def fixture(self):
        identity='0910101032547698'
        page='3106211008'+identity+'2b'*10
        text=f'npe3_bts_page_request: t=18.016666667 identity={identity} phase=1 offset=6 multiframes=2 after_frame=3904 octets={page} raw_adc_only=1 port_bits_identity=1 physical_claim=0'
        access=dict(attempts=2,reported_frames=[2708,4043],normal_control_blocks=[
            dict(start_frame=2740,octets='013f4905087000f000fffe3308'+identity+'2b2b'),
            dict(start_frame=4066,octets='013f410627070333198108'+identity+'2b'*4)])
        agch=dict(responses=2,agch_start_frames=[2709,4045])
        packets=[dict(type=128,payload='602a00000fc9002e0001'+page,time='18.16')]
        return text,access,agch,packets

    def assess(self,text,access,agch,packets):
        with patch('tools.npe3_native_paging_response_check.check_access',return_value=access) as decode, \
                patch('tools.npe3_native_paging_response_check.check_agch',return_value=agch), \
                patch('tools.npe3_native_paging_response_check.check_rx_publications',return_value=dict(packets=packets)):
            result=check(text,'journal')
            decode.assert_called_once_with(text,'journal',require_windows=True,require_releases=True,allow_normal=True)
            return result

    def test_joined_page_access_assignment_and_native_identity(self):
        result=self.assess(*self.fixture())
        self.assertTrue(result['native_paging_response_qualified'])
        self.assertFalse(result['stable_camping_qualified'])
        self.assertFalse(result['call_signalling_qualified'])

    def test_reject_wrong_identity_missing_decode_or_new_assignment(self):
        text,access,agch,packets=self.fixture()
        for values in ((text,dict(access,attempts=1),agch,packets),
                       (text,access,dict(agch,agch_start_frames=[2709]),packets),
                       (text,access,agch,[]),
                       (text,access,agch,[dict(packets[0],time='19.0')]),
                       (text.replace('phase=1','phase=0'),access,agch,packets)):
            with self.assertRaises(ValueError):self.assess(*values)
        for old,new in (('198108091010','198108081010'),('06270703','06270704')):
            altered=[access['normal_control_blocks'][0],dict(access['normal_control_blocks'][1],
                octets=access['normal_control_blocks'][1]['octets'].replace(old,new))]
            with self.assertRaises(ValueError):self.assess(text,dict(access,normal_control_blocks=altered),agch,packets)


if __name__=='__main__':unittest.main()

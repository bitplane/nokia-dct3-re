import unittest
from unittest.mock import patch
from tools.npe3_raw_call_setup_check import check


class RawCallSetupTest(unittest.TestCase):
    def fixture(self):
        octets='030045030504046002008134015c0581551532f4'+'2b'*3
        text=f'npe3_raw_call_setup: uplink_start_frame=4066 start_frame=4153 octets={octets} policy=plain_test_cell raw_adc_only=1 physical_claim=0'
        blocks=[dict(start_frame=4066,octets='013f41062707033319810809101010325476982b2b2b2b')]
        packets=[dict(type=128,payload='802a00'+(4156).to_bytes(3,'big').hex()+'00230001'+octets,time='19.2')]
        return text,blocks,packets

    def test_native_decode_does_not_imply_call_confirmation_or_speech(self):
        text,blocks,packets=self.fixture()
        with patch('tools.npe3_raw_call_setup_check.check_rx_publications',return_value=dict(packets=packets)):
            result=check(text,blocks)
            self.assertTrue(result['native_setup_decode_qualified'])
            self.assertFalse(result['native_call_confirmed_qualified'])
            self.assertFalse(result['speech_transport_qualified'])
            blocks+= [dict(start_frame=4219,octets='0120098308'+'2b'*18),
                      dict(start_frame=4270,octets='0140098301'+'2b'*18)]
            result=check(text,blocks)
            self.assertTrue(result['native_call_confirmed_qualified'])
            self.assertTrue(result['native_alerting_qualified'])
            for bad in ('',text+text,text.replace('4153','4154'),text.replace('81551532f4','81551532f5')):
                with self.assertRaises(ValueError):check(bad,blocks)
            with self.assertRaises(ValueError):check(text,blocks[1:])
        for bad in ([],packets*2,[dict(packets[0],payload=packets[0]['payload'].replace('802a00','802a01'))],
                    [dict(packets[0],time='18.0')]):
            with patch('tools.npe3_raw_call_setup_check.check_rx_publications',return_value=dict(packets=bad)):
                with self.assertRaises(ValueError):check(text,blocks)


if __name__=='__main__':unittest.main()

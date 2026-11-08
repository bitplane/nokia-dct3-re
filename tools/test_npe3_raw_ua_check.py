import unittest
from unittest.mock import patch
from tools.npe3_raw_ua_check import check


class RawUaCheckTest(unittest.TestCase):
    def test_contention_reply_requires_independent_complete_uplink(self):
        sabm='013f4905087000f000fffe330809101010325476982b2b'
        ua='01734905087000f000fffe330809101010325476982b2b'
        uplink=f'npe3_bts_uplink: start_frame=3403 octets={sabm} exact_code=1 port_bits_only=1 physical_claim=0\n'
        reply=f'npe3_raw_ua: uplink_start_frame=3403 start_frame=3439 octets={ua} raw_adc_only=1 physical_claim=0'
        blocks=[dict(start_frame=3403,octets=sabm)]
        self.assertEqual(check(uplink+reply,blocks,require_native=False)['raw_ua_schedules'],1)
        self.assertFalse(check(uplink+reply,blocks,require_native=False)['native_ua_decode_qualified'])
        for bad in (reply+uplink,uplink,reply,uplink+reply+reply,
                    uplink+reply.replace('3439','3440'),uplink+reply.replace(ua,ua.replace('0173','0373')),
                    uplink.replace(sabm,sabm.replace('013f','013e'))+reply):
            with self.assertRaises(ValueError):check(bad,blocks,require_native=False)
        with self.assertRaises(ValueError):check(uplink+reply,[],require_native=False)
        packet=dict(type=128,payload='802a00'+(3442).to_bytes(3,'big').hex()+'0023fffe'+ua,time='16.0')
        with patch('tools.npe3_raw_ua_check.check_rx_publications',return_value=dict(packets=[packet])):
            self.assertTrue(check(uplink+reply,blocks)['native_ua_decode_qualified'])
        for packets in ([],[packet,packet],[dict(packet,payload=packet['payload'].replace('802a00','802a01'))],
                        [dict(packet,time='1.0')]):
            with patch('tools.npe3_raw_ua_check.check_rx_publications',return_value=dict(packets=packets)):
                with self.assertRaises(ValueError):check(uplink+reply,blocks)


if __name__=='__main__':unittest.main()

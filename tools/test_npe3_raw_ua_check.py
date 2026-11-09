import unittest
from unittest.mock import patch
from tools.npe3_raw_ua_check import check,check_location_update,check_release,check_paging_fill,check_frame_pages


class RawUaCheckTest(unittest.TestCase):
    def test_unique_page_payload_binds_source_without_fitting_reported_fn(self):
        frame=216
        payload=bytes.fromhex('2506211005f4')+(0x80000000|frame).to_bytes(4,'big')+bytes([0x2b])*13
        text=f'npe3_bts_frame_page: start_frame={frame} octets={payload.hex()} unmatched_tmsi=1 raw_adc_only=1 physical_claim=0'
        packet=dict(type=128,payload='602a00'+(frame+2).to_bytes(3,'big').hex()+'002e0001'+payload.hex(),time='1.1')
        with patch('tools.npe3_raw_ua_check.check_rx_publications',return_value=dict(packets=[packet])):
            result=check_frame_pages(text,1.05)
            self.assertEqual(result['matches'][0]['end_frame_delta'],-1)
            self.assertTrue(result['unique_source_payloads_qualified'])
            self.assertFalse(result['reported_source_fn_agrees'])
            self.assertFalse(result['stable_camping_qualified'])
            packet['time']='1.005'
            early=check_frame_pages(text,1.0)
            self.assertTrue(early['matches'][0]['before_fourth_source_burst'])
            packet['time']='1.1'
            for changed in ('',text+'\n'+text,text.replace('start_frame=216','start_frame=217')):
                with self.assertRaises(ValueError):check_frame_pages(changed,1.05)
            with self.assertRaises(ValueError):check_frame_pages(text,2)
            packet['time']='0.9'
            with self.assertRaises(ValueError):check_frame_pages(text,0.8)

    def test_disconnect_releases_contention_for_a_different_initial_message(self):
        first='013f4905087000f000fffe330809101010325476982b2b'
        second='013f41062707033319810809101010325476982b2b2b2b'
        disc='015301'+'2b'*20
        def capture(with_disc):
            blocks=[dict(start_frame=3403,octets=first)]
            if with_disc:blocks.append(dict(start_frame=3454,octets=disc))
            blocks.append(dict(start_frame=3505,octets=second))
            text=''
            for block in blocks:
                frame=block['start_frame'];data=block['octets']
                text+=f'npe3_bts_uplink: start_frame={frame} octets={data} exact_code=1 port_bits_only=1 physical_claim=0\n'
                if data!=disc:
                    reply='0173'+data[4:]
                    start=((frame+3)//51+1)*51+22
                    text+=f'npe3_raw_ua: uplink_start_frame={frame} start_frame={start} octets={reply} raw_adc_only=1 physical_claim=0\n'
            return text,blocks
        self.assertEqual(check(*capture(True),require_native=False)['raw_ua_schedules'],2)
        with self.assertRaises(ValueError):check(*capture(False),require_native=False)

    def test_paging_fill_requires_native_post_release_reception_but_not_fitted_carrier(self):
        fill='1506210001f0'+'2b'*17
        text=f'npe3_bts_pch: starts=6,12,16 octets={fill} tsc=2 no_identity=1 raw_adc_only=1 physical_claim=0'
        packets=[dict(type=128,payload='602a00'+frame.to_bytes(3,'big').hex()+'002e0001'+fill,time=str(stamp))
                 for frame,stamp in ((219,1.02),(519,2.4))]
        with patch('tools.npe3_raw_ua_check.check_rx_publications',return_value=dict(packets=packets)):
            report=check_paging_fill(text,1.5)
            self.assertEqual(report['blocks'],2)
            self.assertEqual(report['reported_receiver_arfcns'],[46])
            self.assertFalse(report['stable_camping_qualified'])
            self.assertFalse(report['source_frame_binding_qualified'])
            for bad in ('',text+text,text.replace('starts=6,12,16','starts=6,12,22')):
                with self.assertRaises(ValueError):check_paging_fill(bad,1.5)
            with self.assertRaises(ValueError):check_paging_fill(text,2.5)
        with patch('tools.npe3_raw_ua_check.check_rx_publications',return_value=dict(packets=[dict(packets[1],time='2.3')])):
            report=check_paging_fill(text,1.5)
            self.assertEqual(report['early_reported_frames'],[dict(reported_frame=519,time='2.3')])
            self.assertFalse(report['source_frame_binding_qualified'])
        for bad in ([],[dict(packets[1],payload=packets[1]['payload'].replace('150621','140621'))],
                    [dict(packets[1],payload=packets[1]['payload'].replace('602a00','602a01'))],
                    [dict(packets[1],time='1.0')]):
            with patch('tools.npe3_raw_ua_check.check_rx_publications',return_value=dict(packets=bad)):
                with self.assertRaises(ValueError):check_paging_fill(text,1.5)

    def test_release_requires_two_decodes_ack_disc_and_immediate_common_reconfiguration(self):
        release='03020d060d00'+'2b'*17
        ua='017301'+'2b'*20
        text=('npe3_raw_lu_accept: uplink_start_frame=3454 start_frame=3541 octets='+'00'*23+
              ' policy=plain_test_cell raw_adc_only=1 physical_claim=0\n'
              f'npe3_raw_release: ack_start_frame=3556 start_frame=3592 octets={release} raw_adc_only=1 physical_claim=0\n'
              f'npe3_raw_disc_ua: disc_start_frame=3658 start_frame=3694 octets={ua} raw_adc_only=1 physical_claim=0')
        blocks=[dict(start_frame=frame,octets=header+'2b'*20) for frame,header in
                ((3556,'032101'),(3607,'034101'),(3658,'015301'))]
        native=[dict(type=128,payload='802a00'+frame.to_bytes(3,'big').hex()+'0023fffe'+data,time=str(stamp))
                for frame,data,stamp in ((3595,release,16.6),(3697,ua,17.07))]
        common=[dict(time=17.08,kind=2,body=bytes(8)+b'\x60'+bytes(15))]
        with patch('tools.npe3_raw_ua_check.check_rx_publications',return_value=dict(packets=native)), \
                patch('tools.npe3_dedicated_check.packets',return_value=common):
            self.assertTrue(check_release(text,blocks)['native_normal_release_qualified'])
            for bad in (text.replace('3592','3593'),text.replace(release,release.replace('0302','0300')),
                        text.replace('3694','3695'),text.replace('ack_start_frame=3556','ack_start_frame=3607')):
                with self.assertRaises(ValueError):check_release(bad,blocks)
            for bad in (blocks[:2],blocks[::2],blocks[1:],blocks+blocks[:1]):
                with self.assertRaises(ValueError):check_release(text,bad)
        for packets in ([],native[:1],native+native[:1]):
            with patch('tools.npe3_raw_ua_check.check_rx_publications',return_value=dict(packets=packets)), \
                    patch('tools.npe3_dedicated_check.packets',return_value=common):
                with self.assertRaises(ValueError):check_release(text,blocks)
        for packets in ([],[dict(common[0],time=17.5)],[dict(common[0],body=bytes(24))]):
            with patch('tools.npe3_raw_ua_check.check_rx_publications',return_value=dict(packets=native)), \
                    patch('tools.npe3_dedicated_check.packets',return_value=packets):
                with self.assertRaises(ValueError):check_release(text,blocks)

    def test_lu_accept_binds_identity_native_decode_and_uplink_ack(self):
        sabm='013f4905087000f000fffe330809101010325476982b2b'
        accept='030045050200f1100001170809101010325476982b2b2b'
        ua='npe3_raw_ua: uplink_start_frame=3403 start_frame=3439 octets=01734905087000f000fffe330809101010325476982b2b raw_adc_only=1 physical_claim=0\n'
        lu=f'npe3_raw_lu_accept: uplink_start_frame=3403 start_frame=3490 octets={accept} policy=plain_test_cell raw_adc_only=1 physical_claim=0'
        blocks=[dict(start_frame=3403,octets=sabm),dict(start_frame=3505,octets='032101'+'2b'*20)]
        packet=dict(type=128,payload='802a00'+(3493).to_bytes(3,'big').hex()+'0023fffe'+accept,time='16.2')
        with patch('tools.npe3_raw_ua_check.check_rx_publications',return_value=dict(packets=[packet])):
            report=check_location_update(ua+lu,blocks)
            self.assertTrue(report['native_lapdm_ack_qualified'])
            self.assertFalse(report['mm_registration_qualified'])
            for bad in (lu+ua,ua,ua+lu+lu,ua+lu.replace('3490','3491'),
                        ua+lu.replace('091010','081010')):
                with self.assertRaises(ValueError):check_location_update(bad,blocks)
            with self.assertRaises(ValueError):check_location_update(ua+lu,blocks[:1])
            with self.assertRaises(ValueError):check_location_update(ua+lu,[blocks[0],dict(blocks[1],octets='034101'+'2b'*20)])
        for bad in ([],[packet,packet],[dict(packet,payload=packet['payload'].replace('802a00','802a01'))]):
            with patch('tools.npe3_raw_ua_check.check_rx_publications',return_value=dict(packets=bad)):
                with self.assertRaises(ValueError):check_location_update(ua+lu,blocks)

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

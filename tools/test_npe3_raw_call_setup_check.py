import unittest
from unittest.mock import patch
from tools.npe3_raw_call_setup_check import check,check_assignment,check_call_rr,check_call_clearing


class RawCallSetupTest(unittest.TestCase):
    def test_call_clearing_requires_air_complete_and_return_to_common(self):
        release='036209032d'+'2b'*18
        channel='03840d060d00'+'2b'*17
        ua='017301'+'2b'*20
        text=(f'npe3_raw_cc_release: disconnect_start_frame=4372 start_frame=4408 octets={release} raw_adc_only=1 physical_claim=0\n'
              f'npe3_raw_call_channel_release: complete_start_frame=4423 start_frame=4459 octets={channel} raw_adc_only=1 physical_claim=0\n'
              f'npe3_raw_disc_ua: disc_start_frame=4525 start_frame=4561 octets={ua} raw_adc_only=1 physical_claim=0\n')
        blocks=[dict(start_frame=4372,octets='012415832502e091'+'2b'*15),
                dict(start_frame=4423,octets='014609832a'+'2b'*18),
                dict(start_frame=4474,octets='036101'+'2b'*20),
                dict(start_frame=4525,octets='015301'+'2b'*20)]
        rr=dict(native_disconnect_frames=[4372],acknowledgements=[
            dict(source_frame=4372,start_frame=4408),dict(source_frame=4423,start_frame=4459)])
        packets=[dict(type=128,payload='802a00'+(frame+3).to_bytes(3,'big').hex()+'00230001'+octets,time=str(t))
                 for frame,octets,t in ((4408,release,20.37),(4459,channel,20.60),(4561,ua,21.07))]
        body=bytearray(24);body[8]=0x60
        common=[dict(time=21.08,kind=2,body=body)]
        with patch('tools.npe3_raw_call_setup_check.check_call_rr',return_value=rr), \
                patch('tools.npe3_raw_call_setup_check.check_rx_publications',return_value=dict(packets=packets)), \
                patch('tools.npe3_dedicated_check.packets',return_value=common):
            result=check_call_clearing(text,blocks)
            self.assertTrue(result['native_call_clearing_qualified'])
            self.assertFalse(result['connected_call_qualified'])
            for bad in ('',text+text,text.replace('036209','034209'),text.replace('4408','4409'),
                        text.replace('complete_start_frame=4423','complete_start_frame=4424'),
                        text.replace('017301','017201')):
                with self.assertRaises(ValueError):check_call_clearing(bad,blocks)
            with self.assertRaises(ValueError):check_call_clearing(text,blocks[:-1])
            with self.assertRaises(ValueError):check_call_clearing(text,[blocks[0],dict(blocks[1],octets='0146098308'+'2b'*18),*blocks[2:]])
            with patch('tools.npe3_dedicated_check.packets',return_value=[]):
                with self.assertRaises(ValueError):check_call_clearing(text,blocks)
            with patch('tools.npe3_raw_call_setup_check.check_rx_publications',return_value=dict(packets=packets[:-1])):
                with self.assertRaises(ValueError):check_call_clearing(text,blocks)

    def test_standalone_rr_requires_confirmation_and_native_receive(self):
        octets='032101'+'2b'*20
        text=f'npe3_raw_call_rr: uplink_start_frame=4219 start_frame=4255 octets={octets} raw_adc_only=1 physical_claim=0'
        confirmed=dict(start_frame=4219,octets='0120098308'+'2b'*18)
        packets=[dict(type=128,payload='802a00'+(4258).to_bytes(3,'big').hex()+'00230001'+octets,time='19.7')]
        with patch('tools.npe3_raw_call_setup_check.check',return_value=dict(native_call_confirmed_frames=[4219])), \
                patch('tools.npe3_raw_call_setup_check.check_rx_publications',return_value=dict(packets=packets)):
            result=check_call_rr(text,[confirmed])
            self.assertTrue(result['native_call_rr_decode_qualified'])
            self.assertFalse(result['native_disconnect_qualified'])
            blocks=[confirmed,dict(start_frame=4423,octets='0140158325028095'+'2b'*15)]
            self.assertTrue(check_call_rr(text,blocks)['native_disconnect_qualified'])
            self.assertFalse(check_call_rr(text,[confirmed,dict(blocks[1],octets='0140118325028095'+'2b'*15)])['native_disconnect_qualified'])
            second='npe3_raw_call_rr: uplink_start_frame=4270 start_frame=4306 octets=034101'+'2b'*20+' raw_adc_only=1 physical_claim=0'
            alerting=dict(start_frame=4270,octets='0122098341'+'2b'*18)
            packets.append(dict(type=128,payload='802a00'+(4309).to_bytes(3,'big').hex()+'00230001'+'034101'+'2b'*20,time='19.9'))
            self.assertEqual([r['ns'] for r in check_call_rr(text+'\n'+second,[confirmed,alerting])['acknowledgements']],[0,1])
            with self.assertRaises(ValueError):check_call_rr(text+'\n'+second.replace('034101','032101'),[confirmed,alerting])
            third='npe3_raw_cc_release: disconnect_start_frame=4372 start_frame=4408 octets=036209032d'+'2b'*18+' raw_adc_only=1 physical_claim=0'
            fourth='npe3_raw_call_channel_release: complete_start_frame=4423 start_frame=4459 octets=03840d060d00'+'2b'*17+' raw_adc_only=1 physical_claim=0'
            packets.extend(dict(type=128,payload='802a00'+(frame+3).to_bytes(3,'big').hex()+'00230001'+octets,time=str(t))
                           for frame,octets,t in ((4408,'036209032d'+'2b'*18,20.37),(4459,'03840d060d00'+'2b'*17,20.60)))
            clearing_blocks=[confirmed,alerting,dict(start_frame=4372,octets='012415832502e091'+'2b'*15),
                             dict(start_frame=4423,octets='014609832a'+'2b'*18)]
            clearing_text='\n'.join((text,second,third,fourth))
            self.assertEqual([r['kind'] for r in check_call_rr(clearing_text,clearing_blocks)['acknowledgements']],
                             ['call_rr','call_rr','cc_release','call_channel_release'])
            with self.assertRaises(ValueError):check_call_rr(clearing_text.replace('03840d','03640d'),clearing_blocks)
            del packets[-2:]
            packets.pop()
            for bad in ('',text+text,text.replace('4255','4256'),text.replace('032101','034101'),text.replace('4219','4218')):
                with self.assertRaises(ValueError):check_call_rr(bad,blocks)
        for bad in ([],packets*2,[dict(packets[0],time='18.0')]):
            with patch('tools.npe3_raw_call_setup_check.check',return_value=dict(native_call_confirmed_frames=[4219])), \
                    patch('tools.npe3_raw_call_setup_check.check_rx_publications',return_value=dict(packets=bad)):
                with self.assertRaises(ValueError):check_call_rr(text,[confirmed])

    def test_assignment_requires_real_confirmation_and_exact_native_decode(self):
        octets='032221062e094023006301'+'2b'*12
        text=f'npe3_raw_assignment: confirmed_start_frame=4219 start_frame=4255 octets={octets} raw_adc_only=1 physical_claim=0'
        packets=[dict(type=128,payload='802a00'+(4258).to_bytes(3,'big').hex()+'00230001'+octets,time='19.7')]
        with patch('tools.npe3_raw_call_setup_check.check',return_value=dict(native_call_confirmed_frames=[4219])), \
                patch('tools.npe3_raw_call_setup_check.check_rx_publications',return_value=dict(packets=packets)):
            result=check_assignment(text,[])
            self.assertTrue(result['native_assignment_decode_qualified'])
            self.assertFalse(result['traffic_channel_qualified'])
            for bad in ('',text+text,text.replace('4255','4256'),text.replace('032221','030221'),text.replace('4219','4218')):
                with self.assertRaises(ValueError):check_assignment(bad,[])
        for bad in ([],packets*2,[dict(packets[0],time='18.0')]):
            with patch('tools.npe3_raw_call_setup_check.check',return_value=dict(native_call_confirmed_frames=[4219])), \
                    patch('tools.npe3_raw_call_setup_check.check_rx_publications',return_value=dict(packets=bad)):
                with self.assertRaises(ValueError):check_assignment(text,[])

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
            self.assertFalse(result['native_connect_emission_qualified'])
            self.assertFalse(result['speech_transport_qualified'])
            blocks+= [dict(start_frame=4219,octets='0120098308'+'2b'*18),
                      dict(start_frame=4270,octets='0140098301'+'2b'*18)]
            result=check(text,blocks)
            self.assertTrue(result['native_call_confirmed_qualified'])
            self.assertTrue(result['native_alerting_qualified'])
            blocks.append(dict(start_frame=4372,octets='0124098307'+'2b'*18))
            self.assertTrue(check(text,blocks)['native_connect_emission_qualified'])
            for bad in ('',text+text,text.replace('4153','4154'),text.replace('81551532f4','81551532f5')):
                with self.assertRaises(ValueError):check(bad,blocks)
            with self.assertRaises(ValueError):check(text,blocks[1:])
        for bad in ([],packets*2,[dict(packets[0],payload=packets[0]['payload'].replace('802a00','802a01'))],
                    [dict(packets[0],time='18.0')]):
            with patch('tools.npe3_raw_call_setup_check.check_rx_publications',return_value=dict(packets=bad)):
                with self.assertRaises(ValueError):check(text,blocks)


if __name__=='__main__':unittest.main()

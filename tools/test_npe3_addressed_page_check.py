import unittest
from unittest.mock import patch

from tools.npe3_addressed_page_check import check,check_mcu_admission,check_rr_delivery


class AddressedPageTest(unittest.TestCase):
    def fixture(self):
        identity='0910101032547698'
        page='3106211008'+identity+'2b'*10
        text=('rf_adc_broadcast_si: index=2 arfcn=35 octets=49061b000100f11000014100000000000000002b2b2b2b physical_claim=0\n'
              'npe3_bts_uplink: start_frame=2740 octets=013f4905087000f000fffe3308'+identity+'2b2b exact_code=1 port_bits_only=1 physical_claim=0\n'
              'npe3_raw_disc_ua: disc_start_frame=2944 start_frame=2980 octets=017301'+'2b'*20+' raw_adc_only=1 physical_claim=0\n'
              f'npe3_bts_page_request: t=18.000000000 identity={identity} phase=1 offset=6 multiframes=2 after_frame=3901 octets={page} raw_adc_only=1 port_bits_identity=1 physical_claim=0\n'
              'npe3_bts_page_air: start_frame=4035 t=18.623076923 raw_adc_only=1 physical_claim=0\n')
        packet=dict(type=128,payload='602a00000fc6002e0001'+page,time='18.64')
        return text,packet

    def checked(self,text=None,packets=None):
        original,packet=self.fixture()
        with patch('tools.npe3_addressed_page_check.check_rx_publications',return_value=dict(packets=[packet] if packets is None else packets)):
            return check(original if text is None else text,18)

    def test_joins_uplink_identity_release_and_combined_paging_slot(self):
        result=self.checked()
        self.assertEqual((result['phase'],result['offset']),(1,6))
        self.assertEqual(result['observed_air_blocks'],1)
        self.assertTrue(result['native_addressed_decode_qualified'])
        self.assertFalse(result['stable_camping_qualified'])
        self.assertFalse(result['native_paging_response_qualified'])

    def test_no_native_decode_is_a_result_not_a_false_positive(self):
        result=self.checked(packets=[])
        self.assertFalse(result['native_addressed_decode_qualified'])
        self.assertEqual(result['native_publications'],0)

    def test_wrong_identity_source_timing_or_schedule_reject(self):
        text,packet=self.fixture()
        for old,new in (('phase=1','phase=0'),('offset=6','offset=36'),
                        ('after_frame=3901','after_frame=3900'),('start_frame=4035','start_frame=3984'),
                        ('t=18.623076923','t=19.0'),('t=18.000000000','t=17.0'),
                        ('3106211008','3106211009'),('33080910101032547698','33080910101032547697'),
                        ('0001410000','0001400000'),
                        ('start_frame=2980','start_frame=4000'),('port_bits_identity=1','port_bits_identity=0')):
            with self.subTest(old=old),self.assertRaises(ValueError):self.checked(text.replace(old,new))
        with self.assertRaises(ValueError):self.checked(text+text)
        with self.assertRaises(ValueError):self.checked(packets=[dict(packet,time='17.9')])

    def admission(self):
        text,packet=self.fixture()
        entries=(0x457db8,0x457dd0,0x457e2a,0x457e80,0x457a32)
        for index,entry in enumerate(entries):
            text+=(f'npe3_paging_admission: entry={entry:08x} hit=1 t={18.6401+index*0.0001:.7f} '
                   f'r0=00106304 r1=00000000 r2=00000001 r5=00000000 r6=00000001 r7=00000fc6 '
                   f'controller=00000004 control=4100 group=000001 object=18022180{packet["payload"]} observer_writes=0\n')
        for entry in (*entries,0x457e6e,0x457ebe,0x457eec,0x327b5e):
            text+=f'npe3_paging_admission_complete: entry={entry:08x} hits={int(entry in entries)} cap=32 end=24 observer_writes=0\n'
        return text,packet

    def test_actual_mcu_group_decision_and_forwarder_not_paging_response(self):
        text,packet=self.admission()
        with patch('tools.npe3_addressed_page_check.check_rx_publications',return_value=dict(packets=[packet])):
            page=check(text,18)
            result=check_mcu_admission(text,page)
            self.assertEqual(result['admitted'],1)
            self.assertFalse(result['native_paging_response_qualified'])
            for old,new in (('r6=00000001','r6=00000000'),('group=000001','group=000000'),
                            ('r7=00000fc6','r7=00000fc5'),('controller=00000004','controller=00000003'),
                            ('observer_writes=0','observer_writes=1'),('hits=1','hits=33'),
                            ('entry=00457e80','entry=00457e82')):
                with self.subTest(old=old),self.assertRaises(ValueError):check_mcu_admission(text.replace(old,new),page)

    def test_rr_delivery_is_not_a_claim_that_the_controller_answers(self):
        text,packet=self.admission()
        text+=('npe3_paging_post: sequence=1 t=18.6406 target=15 header=0623 object=00106304 payload=00000000 observer_writes=0\n'
               'npe3_paging_post: sequence=2 t=18.6410 target=18 header=05eb object=00102ac4 payload=001042ac observer_writes=0\n'
               'npe3_paging_parser_return: sequence=1 t=18.6413 event=083e message=00102ac4 context=00172fa4 observer_writes=0\n'
               'npe3_paging_delivery_complete: posts=2 post_cap=8 parsed=1 parse_cap=8 end=24 observer_writes=0\n')
        with patch('tools.npe3_addressed_page_check.check_rx_publications',return_value=dict(packets=[packet])):
            page=check(text,18)
        self.assertEqual(check_rr_delivery(text,page)['parsed'],1)
        self.assertFalse(check_rr_delivery(text,page)['native_paging_response_qualified'])
        for old,new in (('event=083e','event=0000'),('message=00102ac4','message=00102ac8'),
                        ('header=0623','header=05eb'),('target=18','target=15'),('t=18.6413','t=18.639'),
                        ('posts=2','posts=1'),('parsed=1','parsed=9'),('observer_writes=0','observer_writes=1')):
            with self.subTest(old=old),self.assertRaises(ValueError):check_rr_delivery(text.replace(old,new),page)


if __name__=='__main__':unittest.main()

import unittest
from unittest.mock import patch

from tools.npe3_addressed_page_check import check,check_mcu_admission,check_rr_delivery,check_controller,check_controller_lifecycle,CONTROLLER_CODE


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

    def test_controller_latches_frees_and_repeats_without_qualifying_answer(self):
        image=bytearray(0x400000)
        for address,encoded in CONTROLLER_CODE.items():
            data=bytes.fromhex(encoded);offset=address-0x200000
            image[offset:offset+len(data)]=data
        context=bytearray(48);context[6]=1;context[40:44]=bytes.fromhex('00115b78')
        text=''
        for index,entry in enumerate((0x315450,0x315472,0x31543e)):
            if entry==0x31543e:context[11]=1;context[40:44]=bytes(4)
            event=4 if entry==0x31543e else 0x83e
            text+=(f'npe3_paging_wait: entry={entry:08x} hit=1 t={18.6401+index*.0001:.7f} '
                   f'r0={event:08x} r4=00169be8 r7=00169c14 r9=00000004 saved=0000083e peer=00000004 context={context.hex()} observer_writes=0\n')
        for entry in (0x315450,0x315472,0x31543e,0x314e04,0x3135c2):
            text+=f'npe3_paging_wait_complete: entry={entry:08x} hits={int(entry not in (0x314e04,0x3135c2))} cap=64 begin=12 end=24 observer_writes=0\n'
        page=dict(native_decode_times=[18.64])
        result=check_controller(text,page,image)
        self.assertEqual(result['deferred_pages'],1)
        self.assertFalse(result['phase_semantics_qualified'])
        self.assertFalse(result['native_paging_response_qualified'])
        for old,new in (('saved=0000083e','saved=000007d4'),('peer=00000004','peer=00000002'),
                        ('r9=00000004','r9=00000000'),('hits=1','hits=65'),
                        ('observer_writes=0','observer_writes=1')):
            with self.subTest(old=old),self.assertRaises(ValueError):check_controller(text.replace(old,new),page,image)
        for address in CONTROLLER_CODE:
            bad=image.copy();bad[address-0x200000]^=1
            with self.assertRaises(ValueError):check_controller(text,page,bad)


class ControllerLifecycleTest(unittest.TestCase):
    def test_a_clear_then_reentry_is_not_a_missing_initial_completion(self):
        image=bytearray(0x1153c2)
        image[0x115262:0x115276]=bytes.fromhex('2004468120019002a0b1f15bf98c980271a0e0e3')
        image[0x1153c0:0x1153c2]=bytes.fromhex('71a5')
        lines=[]
        for n,(field,stamp,old,new,pc,saved) in enumerate((
                ('procedure',2,0,1,0x315272,1),('procedure',12,1,0,0x3153c0,0x7ef),
                ('procedure',16,0,1,0x315272,1),('pending',18.002,0,1,0x31547a,0x83e)),1):
            lines.append(f'npe3_controller_transition: sequence={n} field={field} t={stamp:.9f} old={old:08x} new={new:08x} pc={pc:08x} lr=00470595 r0=00000001 r1=00000000 r4=00169be8 saved={saved:08x} observer_writes=0')
        text='\n'.join(lines)+'\nnpe3_controller_transition_complete: changes=4 cap=256 end=24 observer_writes=0'
        page={'native_decode_times':[18.0,22.0]}
        result=check_controller_lifecycle(text,page,image)
        self.assertTrue(result['missing_initial_clear_disproved'])
        self.assertFalse(result['stable_camping_qualified'])
        for old,new in (('saved=000007ef','saved=000007ee'),('old=00000001','old=00000000'),
                        ('t=16.000000000','t=19.000000000'),('changes=4','changes=5')):
            with self.subTest(old=old),self.assertRaises(ValueError):
                check_controller_lifecycle(text.replace(old,new),page,image)


if __name__=='__main__':unittest.main()

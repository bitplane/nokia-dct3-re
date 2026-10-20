import struct
import unittest
from tools.nsk3_packet_check import parse, check_delivery, check_cache, check_controller, check_mode_chain, check_completion_wait


class PacketCensusTest(unittest.TestCase):
    def test_bounded_empty_server(self):
        trace='nsk3_completion_wait: cycles=105d13c observer_writes=0\nnsk3_boot_sample: t=45.000000'
        self.assertEqual(check_completion_wait(trace)['receive_returns'],0)
        for bad in (trace.replace('45.000000','44.000000'),trace+'\nnsk3_completion_receive: object=1',
                    trace+'\nnsk3_completion_construct: input=05e6',trace+'\n'+trace,
                    trace.replace('nsk3_completion_wait:','missing:')):
            with self.assertRaises(ValueError):check_completion_wait(bad)

    def test_mode_chain(self):
        events=(
            'nsk3_mode_decision: status=09ec flag=00000000 owner=0010a7d8 mode=01',
            'nsk3_07d6_construct: caller=0020e397 argument=00000000',
            'nsk3_07d6_post: target=11 pointer=00101760 header=07d601fb caller=00208e3f',
            'nsk3_mode_construct: caller=002541a7',
            'nsk3_mode_post: target=0b pointer=00102eb4 header=03e90100 caller=00287d77',
            'nsk3_radio_argument2: object=00102eb4 header=03e90100',
            'nsk3_043c_send: caller=00212c9f pointer=002c6750',
            'nsk3_controller_result: value=043c',
            'nsk3_controller_wait: owner=0010ade0 selector=0047 status=043c')
        trace='\n'.join(f'{event} cycles={i+1:x} observer_writes=0' for i,event in enumerate(events))
        self.assertEqual(check_mode_chain(trace)['owner_mode'],1)
        for old,new in (('status=09ec','status=09ef'),('mode=01','mode=10'),
                        ('target=11','target=10'),('cycles=4','cycles=1')):
            with self.subTest(old=old),self.assertRaises(ValueError):check_mode_chain(trace.replace(old,new))

    def test_controller_reply_and_wait(self):
        trace='\n'.join((
            'nsk3_043c_send: caller=00212c9f pointer=002c6750 cycles=10 observer_writes=0',
            'nsk3_controller_result: value=043c cycles=20 observer_writes=0',
            'nsk3_controller_wait: owner=0010ade0 selector=0047 status=043c cycles=30 observer_writes=0'))
        self.assertEqual(check_controller(trace)['destination'],0x12)
        for old,new in (('00212c9f','00214029'),('002c6750','00106750'),
                        ('value=043c','value=09f0'),('cycles=20','cycles=40'),
                        ('selector=0047','selector=0048')):
            with self.subTest(old=old),self.assertRaises(ValueError):check_controller(trace.replace(old,new))

    def trace(self):
        packets=[bytes((0x18,2,0,kind)) for kind in (0x74,0x74,0x74,0x74,0x99)]
        body=b'\0\x20'+b''.join(struct.pack('>Hh',884-i,-148) for i in range(40))
        packets.append(bytes((0x18,2,len(body),0x8b))+body)
        return '\n'.join(f'nsk3_liveness_packet: pointer=00102000 data={packet.hex()} t={38.808 if i==5 else i*.1:.6f} observer_writes=0' for i,packet in enumerate(packets))

    def test_capacity_is_not_leading_word(self):
        result=parse(self.trace())
        self.assertEqual(result['leading_word'],32)
        self.assertEqual(result['serialized_pair_capacity'],40)
        self.assertFalse(result['radio_or_physical_units_proved'])
        self.assertEqual(result['own_forward_target'],12)
        self.assertEqual(result['own_forward_header'],'0x13b8')
        self.assertNotIn('own_forward_primitive',result)

    def test_truncation_type_and_signed_value_rejected(self):
        trace=self.trace()
        for bad in (trace.replace('1802a28b','1802208b'),trace.replace('ff6c','0000'),
                    trace.replace('18020099','18020098'),trace.replace('pointer=00102000','pointer=00202000'),
                    trace+'\nnsk3_liveness_packet: malformed'):
            with self.assertRaises(ValueError):parse(bad)

    def test_delivery_requires_same_object_and_order(self):
        trace=self.trace()+'\n'+ '\n'.join((
            'nsk3_packet_forward: target=0c pointer=00102000 header=13b8a28b cycles=10 observer_writes=0',
            'nsk3_packet_receive: task=0c pointer=00102000 header=13b8a28b caller=0021a1dd cycles=20 observer_writes=0',
            'nsk3_packet_consumer: pointer=00102000 header=13b8a28b owner=00106ce8 state=03 phase=01 cache=00000000 cycles=30 observer_writes=0'))
        self.assertEqual(check_delivery(trace)['state'],'03')
        for old,new in (('target=0c','target=0d'),('task=0c','task=0d'),
                        ('caller=0021a1dd','caller=0021a1df'),('cycles=20','cycles=40'),
                        ('header=13b8a28b','header=00a8a28b'),
                        ('owner=00106ce8','pointer=00102004')):
            with self.subTest(old=old),self.assertRaises(ValueError):
                check_delivery(trace.replace(old,new))

    def test_exact_cache_body(self):
        trace=self.trace()+'\n'+ '\n'.join((
            'nsk3_packet_forward: target=0c pointer=00102000 header=13b8a28b cycles=10 observer_writes=0',
            'nsk3_packet_receive: task=0c pointer=00102000 header=13b8a28b caller=0021a1dd cycles=20 observer_writes=0',
            'nsk3_packet_consumer: pointer=00102000 header=13b8a28b owner=00106ce8 state=03 phase=01 cache=00000000 cycles=30 observer_writes=0'))
        body=b''.join(struct.pack('>Hh',884-i,-148) for i in range(40)).hex()
        trace+=f'\nnsk3_packet_cache: pointer=00103000 source=00102000 data={body} cycles=40 observer_writes=0'
        trace+='\nnsk3_packet_park: state=03 phase=01 cache=00103000 cycles=50 observer_writes=0'
        self.assertEqual(check_cache(trace)['cached_pair_bytes'],160)
        for old,new in ((body,body[:-2]),('source=00102000','source=00102004'),
                        ('cycles=50','cycles=35'),('cache=00103000','cache=00103004')):
            with self.subTest(old=old),self.assertRaises(ValueError):check_cache(trace.replace(old,new))


if __name__=='__main__':unittest.main()

import struct
import unittest
from tools.nsk3_packet_check import parse, check_delivery, check_cache


class PacketCensusTest(unittest.TestCase):
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

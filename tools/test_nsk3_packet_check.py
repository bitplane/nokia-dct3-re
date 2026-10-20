import struct
import unittest
from tools.nsk3_packet_check import parse


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

    def test_truncation_type_and_signed_value_rejected(self):
        trace=self.trace()
        for bad in (trace.replace('1802a28b','1802208b'),trace.replace('ff6c','0000'),
                    trace.replace('18020099','18020098'),trace.replace('pointer=00102000','pointer=00202000'),
                    trace+'\nnsk3_liveness_packet: malformed'):
            with self.assertRaises(ValueError):parse(bad)


if __name__=='__main__':unittest.main()

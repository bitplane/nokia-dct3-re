import unittest
from tools.nsm1_native_radio_check import samples,packets


def sample_trace():
    rows=[]
    for time,scan,producer,consumer,count in ((5,0,551,21,6),(20,0,2021,22,6),
            (40,0,2021,22,6),(45,0,2021,22,6),(65,0xffffcf03,5435,3436,7),
            (90,0xffffe42c,10852,8853,7),(100,0xffffeca2,13018,11019,7)):
        rows.append(f'nsm1_native_radio_sample: t={time:.6f} pending=0003 completed=010e scan={scan:08x} qualified=0000 flags=0000 producer=1a00 consumer=1a00 threshold=0574 packets={count} queue_writes={producer}/{consumer} observer_writes=0')
    return '\n'.join(rows)


class NativeRadioTests(unittest.TestCase):
    def test_quiet_initial_window_is_not_a_permanent_queue_stall(self):
        trace=sample_trace();self.assertEqual(len(samples(trace)),7)
        for bad in (trace.replace('qualified=0000','qualified=0001',1),
                    trace.replace('13018/11019','13018/11018'),
                    trace.replace('t=100.000000','t=99.000000'),trace.split('\n',1)[1]):
            with self.assertRaises(ValueError):samples(bad)

    def test_packet_inventory_and_signed_measurements(self):
        payload=bytearray(b'\x00\x80')
        for index in range(40):payload+=(512+index).to_bytes(2,'big')+(-129-index%2).to_bytes(2,'big',signed=True)
        rows=[]
        for index,kind in enumerate((0x74,0x74,0x74,0x74,0x8a,0x87,0x8b),1):
            body=payload if kind==0x8b else b'\x00'
            packet=bytes((0x18,2,len(body),kind))+body
            rows.append(f'nsm1_native_radio_packet: index={index} type={kind:02x} pointer=00102000 bytes={packet.hex()} t={49.2 if kind==0x8b else 2.5} observer_writes=0')
        trace='\n'.join(rows);packets(trace)
        for bad in (trace.replace('type=8a','type=99'),trace.replace('ff7f','ff6c'),trace.replace('t=49.2','t=38.8'),
                    trace.replace('bytes=1802017400','bytes=00',1)):
            with self.assertRaises(ValueError):packets(bad)


if __name__=='__main__':unittest.main()

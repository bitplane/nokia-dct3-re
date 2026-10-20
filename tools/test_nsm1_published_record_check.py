import unittest
from tools.nsm1_published_record_check import records
from tools.dct3_rom4_record_codec import encode_record
from tools.dct3_lock_record_example import EXAMPLE_RECORDS


class RecordLifecycleTest(unittest.TestCase):
    def fixture(self,cold=False):
        encoded=b''.join(encode_record(EXAMPLE_RECORDS[i:i+12],bytes.fromhex('00160010')) for i in (0,12))
        lines=[]
        for update in ([False] if cold else [False,True,False]):
            plain=bytearray(EXAMPLE_RECORDS)
            for i in (10,22):plain[i:i+2]=b'\xa5\xa5' if update else bytes(2)
            p=bytearray(bytes((0x18,2,54 if update else 52,0x74,0x35,0x34 if update else 0x32,0,0))+plain+encoded)
            if update:p+=((sum(int.from_bytes(p[i:i+2],'big') for i in range(6,56,2))^65535)&65535).to_bytes(2,'big')
            lines.extend((f'nsm1_restart_packet: object=00102030 bytes={p.hex()}',
                          'nsm1_record_validation: pc=0027e798 r0=00000010 r1=00000007 r5=00000001'))
        if not cold:lines.append('nsm1_restart_request: reason=00000004 caller=00242c91')
        return '\n'.join(lines)

    def test_native_update_and_cold_control(self):
        self.assertEqual(records(self.fixture()),records(self.fixture(True),True))
        trace=self.fixture()
        for bad in (trace.replace('r5=00000001','r5=00000000',1),trace.replace('reason=00000004','reason=00000068'),trace.replace('3534','3532',1),trace.replace('a5a5','a5a4',1)):
            with self.assertRaises(ValueError):records(bad)


if __name__=='__main__':unittest.main()

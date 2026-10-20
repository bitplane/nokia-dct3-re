import unittest
from unittest.mock import patch

from tools.dct3_lock_record_example import EXAMPLE_RECORDS
from tools.dct3_rom4_record_codec import encode_record
from tools.nsk3_record_check import replies


class RecordPersistenceTest(unittest.TestCase):
    def fixture(self,update=False):
        chip=bytes.fromhex('00160010')
        encoded=b''.join(encode_record(EXAMPLE_RECORDS[i:i+12],chip) for i in (0,12))
        plain=b''.join(EXAMPLE_RECORDS[i:i+10]+(b'\xa5\xa5' if update else bytes(2)) for i in (0,12))
        body=bytes.fromhex('35340000' if update else '35320000')+plain+encoded
        size=54 if update else 52
        packet=bytes.fromhex('00000074')+(size+2).to_bytes(2,'big')+bytes.fromhex('0100')+body
        if update:
            checksum=(sum(int.from_bytes(packet[i:i+2],'big') for i in range(10,60,2))^0xffff)&0xffff
            packet+=checksum.to_bytes(2,'big')
        packet=packet.ljust(64,b'\0')
        return chip,encoded,plain,packet

    @patch('tools.nsk3_record_check.check_upload_handoffs')
    def test_cold_acceptance_and_missing_copy(self,uploads):
        chip,encoded,plain,packet=self.fixture()
        trace=f'nsk3_record_reply: data={packet.hex()} cycles=0\n'
        copy=f'nsk3_record_accept: context=0010c018 copied={plain.hex()} cycles=0\n'
        self.assertEqual(replies(trace+copy,chip,True),encoded)
        uploads.assert_called_once_with(trace+copy,1)
        for bad in (trace,trace+copy+'nsk3_restart_entry: reason=00000004 caller=0023d83b',
                    (trace+copy).replace('35320000','35330000'),
                    trace+copy+'\nunsupported opcode'):
            with self.assertRaises(ValueError):replies(bad,chip,True)

    @patch('tools.nsk3_record_check.check_upload_handoffs')
    def test_update_checksum_rejected_before_lifecycle(self,uploads):
        chip,_,_,packet=self.fixture(True)
        bad=bytearray(packet)
        bad[61]^=1
        with self.assertRaisesRegex(ValueError,'update checksum'):
            replies(f'nsk3_record_reply: data={bad.hex()} cycles=0',chip)
        uploads.assert_not_called()


if __name__=='__main__':unittest.main()

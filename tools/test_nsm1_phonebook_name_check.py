import unittest
from tools.nsm1_phonebook_name_check import check_capture


class NameSourceTest(unittest.TestCase):
    def test_actual_template_read_and_buffer_replacement(self):
        record=bytes.fromhex('000000000000050000014900113288002a0797003645ca000000000000000000')
        trace='\n'.join(f'nsm1_name_record_request: pointer=001137a0 record=00000728 selector=00007ff0 offset={i:08x}\nnsm1_name_record_result: result=00000001 data={record.hex()}' for i in (0,1))
        trace+='\nnsm1_name_write: index=2 value=41000000 mask=ff000000 pc=002042a2 '
        trace+='\n'+'\n'.join(f'nsm1_name_write: index={i} value=2a079700 mask=ffffffff pc=002c3a0e caller=00259eed ' for i in (4,5))
        self.assertTrue(check_capture(trace,record)['template_source_proved'])
        for bad in (trace.replace('record=00000728','record=00000729',1),trace.replace('result=00000001','result=00000000',1),trace.replace('value=41000000','value=00000000'),trace.replace('caller=00259eed','caller=00259eef',1)):
            with self.assertRaises(ValueError):check_capture(bad,record)


if __name__=='__main__':unittest.main()

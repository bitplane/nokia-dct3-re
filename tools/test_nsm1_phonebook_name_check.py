import unittest
import hashlib
from pathlib import Path
import tempfile
from unittest.mock import patch
from PIL import Image
from tools.nsm1_phonebook_name_check import check_capture,check_namespace,check_list


class NameSourceTest(unittest.TestCase):
    def test_formatter_namespaces_are_distinct_rows(self):
        trace='\n'.join(f'nsm1_name_format_entry: mode={mode:08x} index={index:08x} data=00111ce8 output=00111cb8 caller=0025b4c7' for mode,index in ((2,1),(1,1),(1,2)))
        self.assertFalse(check_namespace(trace)['selected_name_missing'])
        with self.assertRaises(ValueError):check_namespace(trace.replace('mode=00000002','mode=00000001'))

    def test_extra_rows_can_change_without_changing_selected_band(self):
        clean=Image.new('RGB',(84,48),'white')
        clean.putpixel((3,9),(0,0,0))
        before=clean.copy();before.putpixel((3,20),(0,0,0))
        band=bytes(value for y in range(7,17) for x in range(84) for value in clean.getpixel((x,y)))
        with tempfile.TemporaryDirectory() as directory:
            first=Path(directory)/'before.png';last=Path(directory)/'after.png'
            before.save(first);clean.save(last)
            with patch('tools.nsm1_phonebook_name_check.SELECTED',hashlib.sha256(band).hexdigest()),patch('tools.nsm1_phonebook_name_check.CLEAN',hashlib.sha256(clean.tobytes()).hexdigest()):
                self.assertTrue(check_list(first,last)['selected_row_unchanged'])
                clean.putpixel((3,9),(255,255,255));clean.save(last)
                with self.assertRaises(ValueError):check_list(first,last)

    def test_actual_template_read_and_buffer_replacement(self):
        record=bytes.fromhex('000000000000050000014900113288002a0797003645ca000000000000000000')
        trace='\n'.join(f'nsm1_name_record_request: pointer=001137a0 record=00000728 selector=00007ff0 offset={i:08x}\nnsm1_name_record_result: result=00000001 data={record.hex()}' for i in (0,1))
        trace+='\nnsm1_name_write: index=2 value=41000000 mask=ff000000 pc=002042a2 '
        trace+='\n'+'\n'.join(f'nsm1_name_write: index={i} value=2a079700 mask=ffffffff pc=002c3a0e caller=00259eed ' for i in (4,5))
        self.assertTrue(check_capture(trace,record)['template_source_proved'])
        for bad in (trace.replace('record=00000728','record=00000729',1),trace.replace('result=00000001','result=00000000',1),trace.replace('value=41000000','value=00000000'),trace.replace('caller=00259eed','caller=00259eef',1)):
            with self.assertRaises(ValueError):check_capture(bad,record)


if __name__=='__main__':unittest.main()

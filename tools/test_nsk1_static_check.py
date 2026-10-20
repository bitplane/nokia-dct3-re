import hashlib
import struct
import unittest
from unittest.mock import patch

from tools.nsk1_static_check import inspect,read_catalogue,read_eeprom_output_contract


class Nsk1StaticTest(unittest.TestCase):
    table = 0x200100

    def image(self):
        image = bytearray(0x100000)
        image[5] = 0x92
        image[0xe4:0xec] = bytes.fromhex('e28f0001e12fff10')
        struct.pack_into('>II',image,0xf8,112,0x100020)
        for index in range(28):
            if index in (11,26,27):
                continue
            pointer = 0x201000+index*0x200
            struct.pack_into('>I',image,0x100+index*4,pointer)
            header = (0x0f00,0,186,0x0700,180,0) if index == 25 else (0x1000,0,1,0,0,0)
            struct.pack_into('>6H',image,pointer-0x200000,*header)
        return image

    def test_all_slots_and_bootstrap_preserved_without_runtime_claim(self):
        image = self.image()
        report = read_catalogue(image,self.table)
        self.assertEqual((28,25),(report['slots'],report['nonzero']))
        self.assertEqual([i for i in range(28) if i not in (11,26,27)],
                         [entry['slot'] for entry in report['entries']])
        self.assertFalse(report['execution_or_mask_identity_proved'])
        profiles = {'test':(hashlib.sha1(image).hexdigest(),self.table)}
        with patch('tools.nsk1_static_check.PROFILES',profiles), \
                patch('tools.nsk1_static_check.read_eeprom_output_contract',return_value={}):
            result = inspect(image,'test')
            self.assertEqual('0x2000ec',result['thumb_entry'])
            self.assertFalse(result['boot_or_fitted_hardware_proved'])
            image[0] ^= 1
            with self.assertRaisesRegex(ValueError,'pinned'):
                inspect(image,'test')

    def test_bounds_null_layout_and_payload_fail_closed(self):
        for offset,format_,value in ((0xf8,'>I',108),(0xfc,'>I',0x10ffff),
                (0x100+11*4,'>I',0x201000),(0x100,'>I',0x1ffffe),
                (0x100,'>I',0x201001),(0x1000+4,'>H',0xffff)):
            image = self.image()
            struct.pack_into(format_,image,offset,value)
            with self.subTest(offset=offset),self.assertRaises(ValueError):
                read_catalogue(image,self.table)
        with self.assertRaises(ValueError):
            read_catalogue(b'',self.table)

    def test_exact_end_of_ram_initializer_is_valid(self):
        image = self.image()
        struct.pack_into('>I',image,0xfc,0x110000-112)
        self.assertEqual(hex(0x110000-112),read_catalogue(image,self.table)['initializer_destination'])

    def test_unknown_variant_rejected(self):
        with self.assertRaisesRegex(ValueError,'variant'):
            inspect(b'','unknown')

    def test_gpio_contract_and_each_fragment_mutation(self):
        fragments = (
            (0x29095a, '22012380029b33207819431170197919431171192004781943017019'),
            (0x290998, '781943917019'),
            (0x2909ba, '781943817019'),
            (0x2909cc, '22012380029b33207819439170197919431171192004781943017019'),
            (0x290a08, '781943117019'),
        )
        for variant,shift in (('standard',0),('2915',-0x10)):
            image = bytearray(0x100000)
            for address,encoded in fragments:
                start = address+shift-0x200000
                image[start:start+len(bytes.fromhex(encoded))] = bytes.fromhex(encoded)
            result = read_eeprom_output_contract(image,variant)
            self.assertEqual((0,2),(result['sda_output_bit'],result['scl_output_bit']))
            self.assertFalse(result['input_wiring_or_fitted_part_proved'])
            for address,_ in fragments:
                offset = address+shift-0x200000
                image[offset] ^= 1
                with self.subTest(variant=variant,address=address),self.assertRaises(ValueError):
                    read_eeprom_output_contract(image,variant)
                image[offset] ^= 1
            with self.assertRaises(ValueError):
                read_eeprom_output_contract(b'',variant)


if __name__ == '__main__':
    unittest.main()

import hashlib
import struct
import unittest
from unittest.mock import patch

from tools.nsk1_static_check import inspect,read_catalogue,read_eeprom_output_contract,read_peripheral_contract,read_keymap_contract,phonecode_contract


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
                patch('tools.nsk1_static_check.read_eeprom_output_contract',return_value={}), \
                patch('tools.nsk1_static_check.read_peripheral_contract',return_value={}), \
                patch('tools.nsk1_static_check.read_keymap_contract',return_value={}), \
                patch('tools.nsk1_static_check.phonecode_contract',return_value={}):
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

    def test_phone_retry_uses_own_record_directory(self):
        for variant,shift in (('standard',0),('2915',-0x10)):
            image = bytearray(0x100000)
            image[0x8f4e0+shift:0x8f4ec+shift] = bytes.fromhex('b510493e78092905db05202c')
            for offset,value in ((0x8f5dc,0x10bee8),(0x8f5d4,0x10bee4),(0x8f5e8,0x709),(0x8ca64,0x200100)):
                struct.pack_into('>I',image,offset+shift,value)
            struct.pack_into('>IHH',image,0x138,0x200200,9,0)
            for index,offset,length in ((1,0x2a0,8),(8,0x2a0,4),(9,0x2a4,1)):
                struct.pack_into('>IHH',image,0x200+index*8,0x700+index,offset,length)
            result = phonecode_contract(image,variant)
            self.assertEqual('0x2a4',result['retry_eeprom'])
            self.assertEqual(5,result['rejected_count_at_least'])
            for offset in (0x8f4e0+shift,0x8f5dc+shift,0x8f5e8+shift,0x138,0x24f):
                image[offset] ^= 1
                with self.subTest(variant=variant,offset=offset),self.assertRaises(ValueError):
                    phonecode_contract(image,variant)
                image[offset] ^= 1

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

    def test_serial_and_input_fragments_fail_closed(self):
        fragments = (
            (0x29088e, '25802601270446a6200002ab3320791a781c43b2711a'),
            (0x2908c8, '433c701c'),
            (0x2908e6, '78194231d000432843bc701c086dd1d6'),
            (0x291830, '4835222821225411222a21044321541121295c0908c9d3fb212d5c08'),
            (0x291908, '00020000'),
            (0x292172, '4a7621295c890849d3fb212c548846f7'),
            (0x2921d8, '4a5c21295c890849d3fb212b548846f7'),
            (0x29234c, '00020000'),
            (0x2933f8, '2031231f5503202f26e0550620305d0006c00ec006000e0125ff291f'),
            (0x293424, '203143d7550720305d0006c00ec006000e004681281f'),
            (0x29346a, '2805dbe8203155021c4806000e012905dbcf'),
            (0x293558, '00020000'),
            (0x281008, '213820ff550820c07220f00afd6c7aa006400e4072a0f00afd73f00afd6420bf7aa1400872a0f00afd6b213a20035508f00afd5920207b6143087360f00afd60213d201855082012550820065508213e201a550820105508200455082039213255012001'),
            (0x281188, '00020000'),
            (0x280c22, '203919012080780a43107008'),
        )
        for variant,shift in (('standard',0),('2915',-0x10)):
            image = bytearray(0x100000)
            for address,encoded in fragments:
                start = address+shift-0x200000
                image[start:start+len(bytes.fromhex(encoded))] = bytes.fromhex(encoded)
            result = read_peripheral_contract(image,variant)
            self.assertEqual(0,result['eeprom_sda_input_bit'])
            self.assertEqual(2,result['ccont_ready_bit'])
            self.assertEqual([6,7],result['simi_initial_fiq_mask_bits'])
            self.assertFalse(result['electrical_timing_proved'])
            for address,_ in fragments:
                offset = address+shift-0x200000
                image[offset] ^= 1
                with self.subTest(variant=variant,address=address),self.assertRaises(ValueError):
                    read_peripheral_contract(image,variant)
                image[offset] ^= 1

    def test_own_key_tables_and_power_comparison(self):
        decoder = bytes.fromhex('b500f7fefd7128ffd101203ebd0009c12900d00949167809008a188918404913180838807800bd0021194a1178124351184049105c08bd00')
        for variant,shift in (('standard',0),('2915',-0x10)):
            image = bytearray(0x100000)
            start = 0x948dc+shift
            image[start:start+len(decoder)] = decoder
            image[0x951be+shift:0x951c6+shift] = bytes.fromhex('f7fef9012884d1f9')
            struct.pack_into('>I',image,0x94950+shift,0x200800)
            struct.pack_into('>I',image,0x94948+shift,0x200840)
            struct.pack_into('>I',image,0x9494c+shift,0x10be96)
            image[0x800:0x819] = bytes.fromhex('3e3e3e3e3e3e1a0102033e19040506183e0708093e170c0a0b')
            image[0x840:0x845] = bytes.fromhex('3e3e3e3e0d')
            report = read_keymap_contract(image,variant)
            self.assertEqual(0x19,report['layout_zero_logical_keys'][11])
            self.assertEqual('0x10',report['power_column_mask'])
            for offset in (0x800,0x844,start,0x951c1+shift):
                image[offset] ^= 1
                with self.subTest(variant=variant,offset=offset),self.assertRaises(ValueError):
                    read_keymap_contract(image,variant)
                image[offset] ^= 1
            struct.pack_into('>I',image,0x94950+shift,0x300000)
            with self.assertRaisesRegex(ValueError,'outside'):
                read_keymap_contract(image,variant)


if __name__ == '__main__':
    unittest.main()

import struct
import unittest

from tools.nsk1_startup_census import flag_contract,power_window_contract


class Nsk1StartupCensusTest(unittest.TestCase):
    def image(self, shift=0):
        image = bytearray(0x100000)
        image[0x415be+shift:0x415ce+shift] = bytes.fromhex('200a1a08280bd903385a2800d01de060')
        handlers = [0x24166c-index*8 for index in range(9)] + [0x241690,0x241624,0x24161c]
        struct.pack_into('>12I',image,0x415d8+shift,*(address+shift for address in handlers))
        code = bytes.fromhex('499e20017288e026499c20017248e022499a20017208e01e4998200171c8e01a499620017188e016499420017148e012499220017108e00e4990200170c8e00a498e20017088e006498c20017048e002498a200170082000')
        image[0x4161c+shift:0x41674+shift] = code
        struct.pack_into('>I',image,0x41898+shift,0x10bd88)
        return image

    def test_branched_and_fallthrough_handlers(self):
        for variant,shift in (('standard',0),('2915',-0x10)):
            result = flag_contract(self.image(shift),variant)
            self.assertEqual(list(range(9))+[None,9,10],
                             [row['flag_slot'] for row in result['inputs']])
            self.assertEqual('0x10bd88',result['flag_base'])
            self.assertEqual('0x64',result['reset_input'])

    def test_mutations_and_truncation_fail_closed(self):
        for offset in (0x415be,0x415d8,0x4166c,0x41672,0x41898):
            image = self.image()
            image[offset] ^= 1
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                flag_contract(image,'standard')
        with self.assertRaises(ValueError):
            flag_contract(b'','standard')

    def test_own_power_window_and_adc_source_table(self):
        fragments = ((0x6276c,'2007f030f8788028'),
                     (0x6278e,'4898434821e8f033f83004080c008028'),
                     (0x62814,'2000882922e100d24291db0b4a754291dc08882122e100d24291db034a714291dc002001bd70'),
                     (0x2a13a,'f038fb7e48a446804e7578302800d1074f9c20ef78394008703821284689e01b'))
        for variant,shift in (('standard',0),('2915',-0x10)):
            image = bytearray(0x100000)
            for offset,encoded in fragments:
                code = bytes.fromhex(encoded)
                image[offset+shift:offset+shift+len(code)] = code
            for offset,value in ((0x629f0,1500),(0x629f8,5500),(0x2a318,0x10a051),(0x9290c,0x200100)):
                struct.pack_into('>I',image,offset+shift,value)
            image[0x100:0x10a] = bytes([0,4,5,6,7,3,1,2,2,1])
            result = power_window_contract(image,variant)
            self.assertEqual([1800,5500],result['inclusive_window'])
            self.assertEqual(2,result['adc_channel'])
            for offset in (0x6278e,0x62814,0x629f8,0x9290c):
                index = offset+shift
                image[index] ^= 1
                with self.subTest(variant=variant,offset=offset),self.assertRaises(ValueError):
                    power_window_contract(image,variant)
                image[index] ^= 1
            image[0x107] = 1
            with self.assertRaises(ValueError):
                power_window_contract(image,variant)


if __name__ == '__main__':
    unittest.main()

import struct
import unittest

from tools.nsk1_startup_census import flag_contract


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


if __name__ == '__main__':
    unittest.main()

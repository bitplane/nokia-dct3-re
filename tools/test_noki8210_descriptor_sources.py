import unittest

from tools.noki8210_radio_contract import DESCRIPTOR_CALLS, neighbour_descriptor_sources


class DescriptorSourcesTest(unittest.TestCase):
    def image(self):
        image = bytearray(0x20000)
        for call in DESCRIPTOR_CALLS:
            # LDR r0,[PC,#16] uses the aligned architectural PC, not call PC.
            address = call - 2
            image[address - 0x200000:address - 0x200000 + 2] = b'\x48\x04'
            delta = 0x2b3026 - call - 4
            branch = ((0xf000 | ((delta >> 12) & 0x7ff)).to_bytes(2, 'big') +
                      (0xf800 | ((delta >> 1) & 0x7ff)).to_bytes(2, 'big'))
            image[call - 0x200000:call - 0x200000 + 4] = branch
            pool = ((address + 4) & ~3) + 16
            image[pool - 0x200000:pool - 0x200000 + 4] = bytes.fromhex('00132b18')
        return image

    def test_six_literal_sites_share_one_ram_descriptor(self):
        sources = neighbour_descriptor_sources(self.image())
        self.assertEqual([item['call'] for item in sources], list(DESCRIPTOR_CALLS))
        self.assertEqual({item['descriptor'] for item in sources}, {0x132b18})
        self.assertTrue(all(item['storage'] == 'ram' and item['mode'] is None
                            for item in sources))
        self.assertEqual(sources[0]['pool'], 0x21c330)
        self.assertEqual(sources[1]['pool'], 0x21c980)

    def test_non_literal_argument_is_rejected(self):
        for instruction in (b'\x48\x01', b'\x49\x00', b'\x20\x04'):
            image = self.image()
            offset = DESCRIPTOR_CALLS[0] - 2 - 0x200000
            image[offset:offset + 2] = instruction
            with self.subTest(instruction=instruction), self.assertRaises(ValueError):
                neighbour_descriptor_sources(image)

    def test_rom_template_and_wrong_ram_owner_are_rejected(self):
        for pointer in (0x2b3026, 0x132b1c):
            image = self.image()
            pool = ((DESCRIPTOR_CALLS[0] + 2) & ~3) + 16 - 0x200000
            image[pool:pool + 4] = pointer.to_bytes(4, 'big')
            with self.subTest(pointer=pointer), self.assertRaises(ValueError):
                neighbour_descriptor_sources(image)

    def test_wrong_constructor_call_is_rejected(self):
        image = self.image()
        offset = DESCRIPTOR_CALLS[0] - 0x200000
        image[offset:offset + 4] = b'\x20\x00\x20\x00'
        with self.assertRaises(ValueError):
            neighbour_descriptor_sources(image)

    def test_incomplete_image_is_rejected(self):
        with self.assertRaises(ValueError):
            neighbour_descriptor_sources(self.image()[:DESCRIPTOR_CALLS[0] - 0x200000])


if __name__ == '__main__':
    unittest.main()

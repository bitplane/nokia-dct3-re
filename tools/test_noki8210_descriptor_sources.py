import unittest

from tools.noki8210_radio_contract import DESCRIPTOR_CALLS, neighbour_descriptor_sources, verify_descriptor_observation


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


class DescriptorObservationTest(unittest.TestCase):
    def trace(self, dcs):
        text = ''.join(f'8210_shared_descriptor_write: address={address:08x} '
            'data=00000000 mask=ffffffff pc=00200168 t=0.009891\n'
            for address in (0x132b18, 0x132b1c, 0x132b20))
        if dcs:
            text += ('8210_neighbour_constructor: descriptor=0013ac98 caller=00286d8d '
                     'data=04137f5c0338796800138038 t=12.262148\n'
                     'TX packet type=11 payload=12 data=040000000000033800000000\n')
        return text + 'dsp_shared_observe: off=1c8 data=00cc pc=002cab02 t=46.000247\n'

    def test_paired_bounded_observations(self):
        for dcs in (False, True):
            verify_descriptor_observation(self.trace(dcs), dcs1800=dcs)

    def test_truncated_observation_cannot_prove_absence(self):
        for dcs in (False, True):
            text = self.trace(dcs)
            for changed in (text.rsplit('dsp_shared_observe:', 1)[0],
                            text.replace('t=46.000247', 't=12.000247')):
                with self.subTest(dcs=dcs), self.assertRaisesRegex(ValueError, 'horizon'):
                    verify_descriptor_observation(changed, dcs1800=dcs)

    def test_missing_duplicate_and_later_writes_are_not_absence(self):
        text = self.trace(False)
        for changed in ('', text.split('\n', 1)[1], text + text,
                        text.replace('t=0.009891', 't=12.1'),
                        text + '8210_shared_descriptor_write: malformed\n',
                        text + '8210_neighbour_constructor: malformed\n'):
            with self.subTest(text=changed), self.assertRaises(ValueError):
                verify_descriptor_observation(changed, dcs1800=False)

    def test_shared_call_wrong_carrier_and_wire_replay_are_rejected(self):
        text = self.trace(True)
        for changed in (text.replace('descriptor=0013ac98', 'descriptor=00132b18'),
                        text.replace('0338', '0337'),
                        text.replace('caller=00286d8d', 'caller=0021c325'),
                        text.replace('t=12.262148', 't=5.1'),
                        text + text.splitlines()[3] + '\n',
                        text + text.splitlines()[4] + '\n',
                        text.replace('040000000000033800000000', '040100000000033800000000')):
            with self.subTest(text=changed), self.assertRaises(ValueError):
                verify_descriptor_observation(changed, dcs1800=True)

    def test_control_cannot_inherit_dcs_constructor(self):
        with self.assertRaises(ValueError):
            verify_descriptor_observation(self.trace(True), dcs1800=False)


if __name__ == '__main__':
    unittest.main()

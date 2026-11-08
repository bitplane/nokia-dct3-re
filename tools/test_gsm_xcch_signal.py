import unittest
from tools.gsm_xcch_signal import control_bits,coded_control,check_vector,TSC2,broadcast_octets,broadcast_si_index,paging_fill_octets


class XcchSourceTest(unittest.TestCase):
    def test_idle_paging_has_no_subscriber_identity_and_full_mac_padding(self):
        block=paging_fill_octets()
        self.assertEqual(len(block),23)
        self.assertEqual(block[:6],bytes.fromhex('1506210001f0'))
        self.assertEqual(block[0]>>2,len(block[:6])-1)
        self.assertEqual(block[4],1)
        self.assertEqual(block[5]&7,0)
        self.assertEqual(block[6:],b'\x2b'*17)

    def test_combined_layout_changes_only_si3_ccch_configuration(self):
        for kind in range(4):
            plain=broadcast_octets(kind)
            combined=broadcast_octets(kind,combined_ccch=True)
            self.assertEqual([at for at in range(23) if plain[at]!=combined[at]],
                             [10] if kind==2 else [])
            if kind==2:self.assertEqual((plain[10],combined[10]),(0x40,0x41))

    def test_broadcast_cycle_and_single_cell_channel_data(self):
        for frame in range(816):
            self.assertEqual(broadcast_si_index(frame),(0,1,2,3,1,1,2,3)[(frame//51)%8])
        blocks=[broadcast_octets(kind) for kind in range(4)]
        self.assertEqual([len(block) for block in blocks],[23]*4)
        self.assertEqual([block[2] for block in blocks],[0x19,0x1a,0x1b,0x1c])
        self.assertEqual(sum(byte.bit_count() for byte in blocks[0][3:19]),1)
        self.assertEqual(blocks[0][14],4) # ARFCN35,bitmap0 bit34.
        self.assertEqual(blocks[1][3:19],bytes(16))
        self.assertEqual(blocks[1][19],0xff)
        self.assertEqual(blocks[1][20:],bytes(3))
        for kind,carrier in ((4,35),(0,0),(0,125)):
            with self.assertRaises(ValueError): broadcast_octets(kind,carrier)

    def fixture(self):
        octets=bytes(23)
        coded=coded_control(octets)
        bursts=[list('000'+'0'*57+'1'+TSC2+'1'+'0'*57+'000') for _ in range(4)]
        for index,bit in enumerate(coded):
            position=2*((49*index)%57)+(index%8)//4
            bursts[index%4][3+position if position<57 else 88+position-57]=str(bit)
        return dict(octets=octets.hex(),tsc=2,information='0'*184,
                    coded=''.join(map(str,coded)),normal_bursts=[''.join(burst) for burst in bursts])

    def test_zero_block_parity_and_octet_bit_order(self):
        self.assertEqual(control_bits(bytes(23)),(0,)*184+(1,)*40+(0,)*4)
        self.assertEqual(control_bits(b'\x01'+bytes(22))[:8],(1,0,0,0,0,0,0,0))
        self.assertEqual(control_bits(b'\x80'+bytes(22))[:8],(0,0,0,0,0,0,0,1))
        self.assertTrue(check_vector(self.fixture())['source_bits_qualified'])

    def test_corrupted_source_layers_reject(self):
        for field in ('information','coded','normal_bursts'):
            vector=self.fixture()
            if field=='normal_bursts':
                vector[field][0]=vector[field][0][:3]+str(1-int(vector[field][0][3]))+vector[field][0][4:]
            else:
                vector[field]=str(1-int(vector[field][0]))+vector[field][1:]
            with self.subTest(field=field),self.assertRaises(ValueError):check_vector(vector)

    def test_bad_geometry_and_training_reject(self):
        for octets in (b'',bytes(22),bytes(24)):
            with self.assertRaises(ValueError):control_bits(octets)
        vector=self.fixture();vector['normal_bursts'][0]=vector['normal_bursts'][0][:61]+'1'+vector['normal_bursts'][0][62:]
        with self.assertRaises(ValueError):check_vector(vector)


if __name__=='__main__':unittest.main()

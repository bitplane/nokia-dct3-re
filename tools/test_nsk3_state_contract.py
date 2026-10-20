import unittest
from pathlib import Path
from tools.nsk3_state_contract import decode, direct_server_sends, direct_call_candidates


class StateContractTest(unittest.TestCase):
    def test_own_image_and_mutations(self):
        path=Path('roms/noki6130/6130-v561-boot4-a.fls')
        if not path.exists():self.skipTest('local own-ROM qualification fixture unavailable')
        image=path.read_bytes()
        self.assertEqual(decode(image)['selector'],0x834)
        dynamic=direct_call_candidates(image,0x287da4)
        self.assertEqual(len(dynamic),33)
        self.assertIn(0x253cc4,dynamic)
        self.assertIn(0x287fdc,dynamic)
        inventory=direct_server_sends(image)
        self.assertEqual(inventory['direct_post_candidates'],363)
        self.assertEqual(inventory['narrow_mailbox0e_sites'],[
            0x213998,0x2146de,0x2147ac,0x214874,0x281748,0x281950,0x2820d2,0x287f3a])
        changed=bytearray(image)
        changed[0x13994:0x13996]=b'\x20\x0f'
        self.assertNotIn(0x213998,direct_server_sends(changed)['narrow_mailbox0e_sites'])
        for address in (0x2536bc,0x2544bc,0x219136,0x21a674,0x21a6a4,
                        0x253994,0x254868,0x2192e4,0x20af86,0x20e9da,
                        0x208df8,0x20ed20,0x208f34,0x295c22,0x21ac36,
                        0x21ac9a,0x21d670,0x2881e0,0x2880b6,0x2883ec,0x2883d4,
                        0x248616,0x248b9e,0x249292,0x24709c,0x24753e,
                        0x2ab522,0x281f16,0x281f50,0x2820a2,0x2820ce,0x281fa8,0x282460,
                        0x2539ba,0x253cbe,0x288170,0x288000,0x2557be,
                        0x253d24,0x2883fc,0x2882e8,0x208a74,0x208e44,0x208df4):
            changed=bytearray(image)
            changed[address-0x200000]^=1
            with self.subTest(address=hex(address)),self.assertRaises(ValueError):decode(changed)


if __name__=='__main__':unittest.main()

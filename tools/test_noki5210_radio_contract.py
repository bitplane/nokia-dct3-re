import unittest
from pathlib import Path
from tools.noki5210_radio_contract import verify, direct_bl_candidates


class RadioContractTest(unittest.TestCase):
    def test_direct_bl_signed_displacement(self):
        self.assertEqual(direct_bl_candidates(bytes.fromhex('f000f802'), 0x200008), [0x200000])
        self.assertEqual(direct_bl_candidates(bytes.fromhex('f7fffffc'), 0x1ffffc), [0x200000])

    def test_direct_bl_rejects_wrong_suffix_and_partial_pair(self):
        for image in (bytes.fromhex('f000e802'), bytes.fromhex('f000f8')):
            self.assertEqual(direct_bl_candidates(image, 0x200008), [])

    def test_foreign_image(self):
        with self.assertRaisesRegex(ValueError, 'NSM-5'):
            verify(b'foreign firmware')

    def test_own_receive_grammar(self):
        path = Path(__file__).resolve().parents[1] / 'roms/noki5210/5210_5.40_ppm_e.fls'
        if not path.exists():
            self.skipTest('acquired firmware unavailable')
        result = verify(path.read_bytes())
        self.assertEqual(result['type_8b_destination_task'], 12)
        self.assertEqual(result['records'], 40)
        self.assertEqual(result['signed_rssi_message_offset'], 9)
        self.assertEqual(result['receive_context_message'], '1802')
        self.assertEqual(result['current_request_dispatch']['03eb'], '2e20dc')
        self.assertEqual(result['current_request_dispatch']['03ec'], '2e202e')
        self.assertEqual(result['initial_request_continuation'], '2e1f5e')
        self.assertEqual(result['direct_census_coverage']['arguments_recovered'], 8)
        self.assertFalse(result['direct_census_coverage']['indirect_and_tail_entries_closed'])


if __name__ == '__main__':
    unittest.main()

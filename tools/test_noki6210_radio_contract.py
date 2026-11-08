import unittest
from pathlib import Path
from tools.noki6210_radio_contract import verify,broadcast_type_targets


class RadioContractTest(unittest.TestCase):
    def test_rejects_foreign_image(self):
        with self.assertRaisesRegex(ValueError, 'NPE-3'):
            verify(b'foreign firmware')

    def test_own_destination(self):
        path = Path(__file__).resolve().parents[1] / 'roms/noki6210/6210_556c.fls'
        if not path.exists():
            self.skipTest('acquired firmware unavailable')
        result = verify(path.read_bytes())
        self.assertEqual(result['type_8b_destination_task'], 14)
        self.assertEqual(result['bcch_destination_task'], 18)
        self.assertEqual(result['bcch_status'], '0800')
        self.assertEqual(result['bcch_object_consumer'], '3b9302')
        self.assertEqual(result['si1_parser'], '43f074')
        self.assertEqual(result['si1_type_handler'], '3ba0c0')
        self.assertEqual(result['si3_type_handler'], '3b9fd0')
        self.assertEqual(result['si3_field_parser'], '3b9764')
        self.assertEqual(result['bcch_type_inputs_checked'], 64)
        self.assertEqual(result['bcch_publication_status'], '03f8')
        self.assertEqual(result['bcch_publication_task'], 14)
        self.assertEqual(result['selection_tx_writer'], '426a3c')
        self.assertEqual(result['selection_tx_ring'], '10000')
        self.assertEqual(result['selection_tx_write_index'], '100a4')
        self.assertEqual(result['selection_completion_kind'], '8f')
        self.assertEqual(result['selection_decoded_channel'], '40')
        self.assertEqual(result['selection_record_result_kind'], '88')
        self.assertEqual(result['traffic_release'], 'runtime physical End observation required')

    def test_changed_subtract_cascade_rejects(self):
        path=Path(__file__).resolve().parents[1]/'roms/noki6210/6210_556c.fls'
        if not path.exists(): self.skipTest('acquired firmware unavailable')
        image=bytearray(path.read_bytes())
        image[0x3b9ead-0x200000]^=1
        with self.assertRaises(ValueError): broadcast_type_targets(image)


if __name__ == '__main__':
    unittest.main()

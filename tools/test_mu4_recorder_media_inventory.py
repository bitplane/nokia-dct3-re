import unittest
from tools.mu4_recorder_media_inventory import inventory


class RecorderMediaInventoryTest(unittest.TestCase):
    def test_unchanged(self):
        report = inventory(bytes(1056), bytes(1056))
        self.assertEqual(report['changed_pages'], [])
        self.assertEqual(report['source_sha256'], report['endpoint_sha256'])

    def test_data_and_spare_changes_are_separate(self):
        old = bytes(528 * 4)
        new = bytearray(old)
        new[528:531] = b'ID3'
        new[528 + 110:528 + 114] = b'POCP'
        new[2 * 528 + 512] = 1
        report = inventory(old, new)
        self.assertEqual(report['changed_pages'], [1, 2])
        self.assertEqual(report['changed_page_ranges_end_exclusive'], [[1, 3]])
        self.assertEqual(report['spare_changed_pages'], [2])
        self.assertEqual([(s['literal'], s['page'], s['data_offset'])
                          for s in report['raw_signatures']], [('ID3', 1, 0), ('POCP', 1, 110)])

    def test_signature_in_spare_is_not_payload(self):
        new = bytearray(528)
        new[512:515] = b'ID3'
        self.assertEqual(inventory(bytes(528), new)['raw_signatures'], [])

    def test_disjoint_runs_and_directory_literal(self):
        new = bytearray(528 * 3)
        new[:8] = b'REL_001 '
        new[1056] = 1
        report = inventory(bytes(len(new)), new)
        self.assertEqual(report['changed_page_ranges_end_exclusive'], [[0, 1], [2, 3]])
        self.assertEqual(report['raw_signatures'][0]['image_offset'], 0)

    def test_invalid_size_and_geometry(self):
        for old, new in ((b'', b''), (b'a', b'a'), (bytes(528), bytes(1056))):
            with self.assertRaises(ValueError):
                inventory(old, new)
        with self.assertRaises(ValueError):
            inventory(bytes(528), bytes(528), 528, 529)


if __name__ == '__main__':
    unittest.main()

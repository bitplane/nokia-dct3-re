import unittest
from unittest.mock import Mock, patch
import struct

from tools import nsm3d_catalogue as inventory


class CatalogueTest(unittest.TestCase):
    def image(self):
        image = bytearray(0x109600)
        struct.pack_into(">2I", image, 0x109560, 0x74, 0x12f040)
        struct.pack_into(">29I", image, 0x109568, *([0x200040] * 28 + [0]))
        struct.pack_into(">6H", image, 0x40, 0xa00, 0x1000, 2, 0x200, 0x3e8, 0)
        image[0x4c:0x50] = b"\x12\x34\x56\x78"
        return image

    def test_complete_count_and_half_open_coverage(self):
        with patch.object(inventory.hashlib, "sha1", return_value=Mock(
                hexdigest=Mock(return_value=inventory.NSM3D_FLASH_SHA1))):
            entries = inventory.catalogue(self.image())
        self.assertEqual(len(entries), 28)
        self.assertEqual(entries[0]["header"], [0xa00, 0x1000, 2, 0x200, 0x3e8, 0])
        self.assertEqual(inventory.covering(entries, 0xa01), list(range(28)))
        self.assertEqual(inventory.covering(entries, 0xa02), [])

    def test_rejects_unpinned_flash(self):
        with self.assertRaisesRegex(ValueError, "pinned"):
            inventory.catalogue(self.image())

    def test_rejects_missing_terminator(self):
        image = self.image()
        struct.pack_into(">I", image, 0x1095d8, 0x200040)
        with patch.object(inventory.hashlib, "sha1", return_value=Mock(
                hexdigest=Mock(return_value=inventory.NSM3D_FLASH_SHA1))):
            with self.assertRaisesRegex(ValueError, "terminator"):
                inventory.catalogue(image)


if __name__ == "__main__":
    unittest.main()

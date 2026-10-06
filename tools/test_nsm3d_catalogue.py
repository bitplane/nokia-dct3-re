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

    def test_8210_catalogues_have_distinct_initialization_origins(self):
        digest = 'c1a0fe95cedb89a92b19654208cc4855e1a4988e'
        image = bytearray(0x10d000)
        for origin, destination, descriptor in (
                (0x10cf50, 0x13579c, 0x200040),
                (0x10ced4, 0x135810, 0x200060)):
            struct.pack_into('>2I', image, origin, 0x74, destination)
            struct.pack_into('>29I', image, origin + 8,
                             *([descriptor] * 28 + [0]))
        struct.pack_into('>6H', image, 0x40, 0xa00, 0x1000, 2, 0x200, 0x3e8, 0)
        struct.pack_into('>6H', image, 0x60, 0xa00, 0x1000, 3, 0x200, 0x3e8, 0)
        with patch.object(inventory.hashlib, 'sha1', return_value=Mock(
                hexdigest=Mock(return_value=digest))):
            rom6 = inventory.catalogue(image, '8210')
            rom5 = inventory.catalogue(image, '8210-rom5')
        self.assertEqual(len(rom6), 28)
        self.assertEqual(len(rom5), 28)
        self.assertEqual(rom6[20]['words'], 2)
        self.assertEqual(rom5[20]['words'], 3)

    def test_rejects_missing_terminator(self):
        image = self.image()
        struct.pack_into(">I", image, 0x1095d8, 0x200040)
        with patch.object(inventory.hashlib, "sha1", return_value=Mock(
                hexdigest=Mock(return_value=inventory.NSM3D_FLASH_SHA1))):
            with self.assertRaisesRegex(ValueError, "terminator"):
                inventory.catalogue(image)


if __name__ == "__main__":
    unittest.main()

import struct
import unittest
from unittest.mock import Mock, patch

from tools import extract_nsm3_verifier as verifier


class VerifierExtractionTest(unittest.TestCase):
    def test_fragment_preserves_acquired_word_order(self):
        image = bytearray(0x1177dc)
        struct.pack_into(">6H", image, 0x117700, 0xff80, 0xff80, 104, 0x200, 0x8c, 0)
        image[0x11770c:0x1177dc] = b"\x12\x34" * 104
        with patch.object(verifier.hashlib, "sha1", side_effect=[
                self.digest(verifier.NSM3D_FLASH_SHA1),
                self.digest("440bf49f1eba4cadb12f7f7581c992b0025807d6")]):
            self.assertEqual(verifier.extract_program_fragment(image), b"\x12\x34" * 104)

    def test_fragment_rejects_unpinned_input(self):
        with self.assertRaisesRegex(ValueError, "pinned"):
            verifier.extract_program_fragment(b"not a mask or firmware")

    def loader_image(self):
        image = bytearray(verifier.NSM3D_LOADER_OFFSET + 12)
        struct.pack_into(">6H", image, verifier.NSM3D_LOADER_OFFSET,
                         0xfd00, 0xff80, 638, 0x500, 0x78, 0)
        return bytes(image) + b"\x56\x78" * 638

    def test_loader_word_order_and_full_extent(self):
        with patch.object(verifier.hashlib, "sha1", side_effect=[
                self.digest(verifier.NSM3D_FLASH_SHA1), self.digest(verifier.NSM3D_LOADER_SHA1)]):
            self.assertEqual(verifier.extract_loader(self.loader_image()), b"\x56\x78" * 638)

    def test_loader_rejects_wrong_flash(self):
        with self.assertRaisesRegex(ValueError, "pinned"):
            verifier.extract_loader(self.loader_image())

    def test_loader_rejects_wrong_payload(self):
        with patch.object(verifier.hashlib, "sha1", side_effect=[
                self.digest(verifier.NSM3D_FLASH_SHA1), self.digest("wrong")]):
            with self.assertRaisesRegex(ValueError, "loader upload"):
                verifier.extract_loader(self.loader_image())

    def image(self, header=None):
        image = bytearray(verifier.DESCRIPTOR_OFFSET + 12)
        struct.pack_into(">6H", image, verifier.DESCRIPTOR_OFFSET,
                         *(header or (0x0f00, 0, 223, 0x0f00, 0xdc, 0)))
        return bytes(image) + b"\x12\x34" * 223

    def digest(self, value):
        return Mock(hexdigest=Mock(return_value=value))

    def test_extract_preserves_word_byte_order(self):
        with patch.object(verifier.hashlib, "sha1", side_effect=[
                self.digest(verifier.FLASH_SHA1), self.digest(verifier.PROGRAM_SHA1)]):
            self.assertEqual(verifier.extract(self.image()), b"\x12\x34" * 223)

    def test_rejects_unpinned_flash(self):
        with self.assertRaisesRegex(ValueError, "pinned"):
            verifier.extract(self.image())

    def test_rejects_changed_descriptor(self):
        with patch.object(verifier.hashlib, "sha1", return_value=self.digest(verifier.FLASH_SHA1)):
            with self.assertRaisesRegex(ValueError, "descriptor"):
                verifier.extract(self.image((0x0f00, 0, 222, 0x0f00, 0xdc, 0)))

    def test_rejects_changed_program(self):
        with patch.object(verifier.hashlib, "sha1", side_effect=[
                self.digest(verifier.FLASH_SHA1), self.digest("incorrect")]):
            with self.assertRaisesRegex(ValueError, "program"):
                verifier.extract(self.image())

    def test_npe3_uses_its_own_descriptor(self):
        image = bytearray(verifier.NPE3_DESCRIPTOR_OFFSET + 12)
        struct.pack_into(">6H", image, verifier.NPE3_DESCRIPTOR_OFFSET,
                         0x0f00, 0, 223, 0x0f00, 0xdc, 0)
        image += b"\x12\x34" * 223
        with patch.object(verifier.hashlib, "sha1", side_effect=[
                self.digest(verifier.NPE3_FLASH_SHA1), self.digest(verifier.PROGRAM_SHA1)]):
            self.assertEqual(verifier.extract(image, "6210"), b"\x12\x34" * 223)

    def test_rejects_unknown_product(self):
        with self.assertRaisesRegex(ValueError, "unsupported"):
            verifier.extract(self.image(), "unknown")

    def test_6250_uses_its_own_descriptor(self):
        image = bytearray(0x1e544 + 12)
        struct.pack_into(">6H", image, 0x1e544, 0x0f00, 0, 223, 0x0f00, 0xdc, 0)
        image += b"\x12\x34" * 223
        with patch.object(verifier.hashlib, "sha1", side_effect=[
                self.digest(verifier.NHM3_FLASH_SHA1), self.digest(verifier.PROGRAM_SHA1)]):
            self.assertEqual(verifier.extract(image, "6250"), b"\x12\x34" * 223)

    def test_nsm3d_uses_its_own_flash_and_descriptor(self):
        image = bytearray(verifier.NSM3D_DESCRIPTOR_OFFSET + 12)
        struct.pack_into(">6H", image, verifier.NSM3D_DESCRIPTOR_OFFSET,
                         0x0f00, 0, 223, 0x0f00, 0xdc, 0)
        image += b"\x12\x34" * 223
        with patch.object(verifier.hashlib, "sha1", side_effect=[
                self.digest(verifier.NSM3D_FLASH_SHA1),
                self.digest(verifier.PROGRAM_SHA1)]):
            self.assertEqual(verifier.extract(image, "8250"), b"\x12\x34" * 223)

    def test_nse5_has_its_own_shorter_program_and_descriptor(self):
        image = bytearray(verifier.NSE5_DESCRIPTOR_OFFSET + 12)
        struct.pack_into(">6H", image, verifier.NSE5_DESCRIPTOR_OFFSET,
                         0x0f00, 0, 210, 0x0700, 0xb4, 0)
        image += b"\x12\x34" * 210
        with patch.object(verifier.hashlib, "sha1", side_effect=[
                self.digest(verifier.NSE5_FLASH_SHA1),
                self.digest(verifier.NSE5_PROGRAM_SHA1)]):
            self.assertEqual(verifier.extract(image, "7110"), b"\x12\x34" * 210)


if __name__ == "__main__":
    unittest.main()

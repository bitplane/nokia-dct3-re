import unittest
from pathlib import Path

from tools.cell_broadcast_static_check import FLASH_BASE, verify_contract


ROM = Path(__file__).resolve().parent.parent / "roms/3210f600a_swap16.bin"


class CellBroadcastStaticCheckTests(unittest.TestCase):
	@classmethod
	def setUpClass(cls):
		cls.image = ROM.read_bytes()

	def test_accepts_recovered_consumer(self):
		result = verify_contract(self.image)
		self.assertEqual(7, result["message_class"])
		self.assertEqual(0x30, result["primitive"])
		self.assertEqual(4, result["header_octets"])
		self.assertEqual(0xAA, result["maximum_blob_octets"])
		self.assertEqual(0x04, result["rejected_raw_packet_type"])
		self.assertEqual(0x19, result["rejected_mdi_direction_type"])

	def test_rejects_changed_blob_bound(self):
		image = bytearray(self.image)
		offset = 0x23CEC2 - FLASH_BASE
		image[offset:offset + 2] = b"\x00\x00"
		with self.assertRaisesRegex(ValueError, "0x23cec2"):
			verify_contract(bytes(image))

	def test_rejects_changed_mmi_router(self):
		image = bytearray(self.image)
		offset = 0x2B2ECC - FLASH_BASE
		image[offset:offset + 2] = b"\x00\x00"
		with self.assertRaisesRegex(ValueError, "0x2b2ecc"):
			verify_contract(bytes(image))


if __name__ == "__main__":
	unittest.main()

import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class UifDeviceSplitTest(unittest.TestCase):
	@classmethod
	def setUpClass(cls):
		cls.device = (ROOT / "driver/nokia_uif.cpp").read_text()
		cls.header = (ROOT / "driver/nokia_uif.h").read_text()
		cls.phone = (ROOT / "driver/nokia_dct3.cpp").read_text()

	def test_device_owns_four_sparse_uif_banks(self):
		self.assertIn("(offset & 0x3c) == 0x30", self.device)
		for bank in ("0x00", "0x40", "0x80", "0xc0"):
			self.assertIn(f"bank == {bank}", self.device)

	def test_device_is_a_neutral_saved_latch_bank(self):
		self.assertIn("save_item(NAME(m_regs))", self.device)
		self.assertNotIn("pc()", self.device + self.header)
		self.assertNotIn("callback", self.device + self.header)

	def test_phone_delegates_uif_accesses(self):
		self.assertIn("required_device<nokia_uif_device> m_uif", self.phone)
		self.assertIn("nokia_uif_device::owns(offset)", self.phone)
		self.assertIn("m_uif->read(offset)", self.phone)
		self.assertIn("m_uif->write(offset, data)", self.phone)
		self.assertIn("NOKIA_UIF(config, m_uif)", self.phone)


if __name__ == "__main__":
	unittest.main()

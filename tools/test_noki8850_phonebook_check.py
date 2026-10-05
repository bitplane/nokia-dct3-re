import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parent))
from noki8850_phonebook_check import check
from sim_phonebook_check import CURRENT_NVRAM_LENGTH


class PhonebookCheckTest(unittest.TestCase):
    def setUp(self):
        self.data = bytearray([0xff] * CURRENT_NVRAM_LENGTH)
        self.data[0] = ord("A")
        self.data[18:22] = bytes((3, 0x81, 0x21, 0xf3))
        self.pixels = bytes(84 * 48)

    def test_save_requires_wire_commit_and_physical_input(self):
        with self.assertRaisesRegex(ValueError, "UPDATE RECORD"):
            check("save", "", self.data, self.pixels, (84, 48))
        trace = "ins=dc p1=01 p2=04 p3=20 selected=6f3a\nupdate fid=6f3a record=1 length=32"
        with self.assertRaisesRegex(ValueError, "physical Save"):
            check("save", trace, self.data, self.pixels, (84, 48))

    def test_readback_requires_read_and_forbids_update(self):
        with self.assertRaisesRegex(ValueError, "rewrote"):
            check("readback", "ins=dc", self.data, self.pixels, (84, 48))
        with self.assertRaisesRegex(ValueError, "read ADN"):
            check("readback", "", self.data, self.pixels, (84, 48))

    def test_unchanged_storage_alone_is_insufficient(self):
        trace = "ins=b2 p1=01 p2=04 p3=20 selected=6f3a\n8850_phonebook_read_physical: action=contact"
        with self.assertRaisesRegex(ValueError, "frame differs"):
            check("readback", trace, self.data, self.pixels, (84, 48))
        with self.assertRaisesRegex(ValueError, "geometry"):
            check("readback", trace, self.data, self.pixels, (96, 48))

    def test_wrong_stored_number_fails(self):
        self.data[20] = 0x43
        with self.assertRaisesRegex(ValueError, "A/123"):
            check("readback", "", self.data, self.pixels, (84, 48))


if __name__ == "__main__":
    unittest.main()

import unittest

from tools.nse6_phonebook_check import check, CONTACTS
from tools.sim_phonebook_check import CURRENT_NVRAM_LENGTH


class PhonebookTests(unittest.TestCase):
    def setUp(self):
        self.data = bytearray(b"\xff" * CURRENT_NVRAM_LENGTH)
        self.data[0] = ord("A")
        self.data[18:22] = bytes((3, 0x81, 0x21, 0xf3))
        self.pixels = bytes(84 * 48)

    def test_storage_alone_is_insufficient(self):
        for stage in CONTACTS:
            with self.subTest(stage=stage), self.assertRaisesRegex(ValueError, "physical"):
                check(stage, "", self.data, self.pixels, (84, 48))
            with self.subTest(stage=stage), self.assertRaisesRegex(ValueError, "frame differs"):
                check(stage, CONTACTS[stage], self.data, self.pixels, (84, 48))

    def test_wrong_storage(self):
        self.data[20] = 0x43
        with self.assertRaisesRegex(ValueError, "A/123"):
            check("recall", CONTACTS["recall"], self.data, self.pixels, (84, 48))

    def test_execution_and_geometry(self):
        with self.assertRaisesRegex(ValueError, "unsupported"):
            check("save", CONTACTS["save"] + " unimplemented C54x opcode",
                  self.data, self.pixels, (84, 48))
        with self.assertRaisesRegex(ValueError, "geometry"):
            check("save", CONTACTS["save"], self.data, self.pixels, (96, 48))

import unittest

from tools.nse6_serial_control_check import check


VALID = """nse6_native_control_post: object=0010cb98 length=04 payload=0080,0104 lr=0029ba3b t=3.64
nse6_native_serial_parameters: address=06f4 value=0880 mask=ffff pc=3bb3 t=3.65
nse6_native_serial_parameters: address=06f6 value=0015 mask=ffff pc=3bbd t=3.66
nse6_native_serial_parameters: address=06f5 value=0362 mask=ffff pc=3bca t=3.67
"""


class SerialControlTests(unittest.TestCase):
    def test_measured_decode(self):
        self.assertEqual(check(VALID), {"posted": 3.64, "decoded": 3.67})

    def test_rejects_missing_delayed_branch_flag(self):
        with self.assertRaisesRegex(ValueError, "ordered"):
            check(VALID.replace("value=0880", "value=0080"))

    def test_rejects_wrong_timeout_or_site(self):
        for old, new in (("value=0362", "value=0363"),
                         ("pc=3bbd", "pc=8d8e"),
                         ("mask=ffff", "mask=00ff")):
            with self.subTest(new=new), self.assertRaises(ValueError):
                check(VALID.replace(old, new))

    def test_rejects_order_or_missing_packet(self):
        for text in (VALID.replace("t=3.67", "t=3.63"),
                     VALID.split("\n", 1)[1]):
            with self.assertRaisesRegex(ValueError, "ordered"):
                check(text)

    def test_rejects_unsupported_opcode(self):
        with self.assertRaisesRegex(ValueError, "unsupported"):
            check(VALID + "unimplemented C54x opcode")


if __name__ == "__main__":
    unittest.main()

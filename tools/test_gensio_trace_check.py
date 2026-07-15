import unittest

from tools.gensio_trace_check import check_accesses, parse_accesses


class GensioTraceCheckTest(unittest.TestCase):
    def test_complete_read_and_write_transactions(self):
        text = """
[:] gensio: W off=2d data=25 old=21 pc=1 t=0
[:] gensio: R off=6d data=03 pc=1 t=0
[:] gensio: W off=2c data=54 old=00 pc=1 t=0
[:] gensio: R off=6d data=07 pc=1 t=0
[:] gensio: R off=6c data=0f pc=1 t=0
[:] gensio: W off=2d data=25 old=25 pc=1 t=0
[:] gensio: W off=2c data=28 old=54 pc=1 t=0
[:] gensio: W off=2c data=31 old=28 pc=1 t=0
"""
        errors, counts = check_accesses(parse_accesses(text))
        self.assertEqual([], errors)
        self.assertEqual({"accesses": 8, "reads": 1, "writes": 1}, counts)

    def test_read_requires_ready_status(self):
        accesses = [
            ("W", 0x2D, 0x25),
            ("W", 0x2C, 0x54),
            ("R", 0x6C, 0x00),
            ("W", 0x2D, 0x25),
            ("W", 0x2C, 0x28),
            ("W", 0x2C, 0x31),
        ]
        errors, _ = check_accesses(accesses)
        self.assertIn("CCONT data consumed before GENSIO status 0x07", errors)

    def test_rejects_unpaired_read(self):
        accesses = [
            ("W", 0x2D, 0x25),
            ("R", 0x6C, 0x00),
        ]
        errors, _ = check_accesses(accesses)
        self.assertIn("CCONT data read without a pending read command", errors)


if __name__ == "__main__":
    unittest.main()

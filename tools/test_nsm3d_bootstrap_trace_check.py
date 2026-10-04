import unittest

from tools.nsm3d_bootstrap_trace_check import check


class Nsm3dBootstrapTest(unittest.TestCase):
    def trace(self):
        return ("nsm3d_verifier_program: words=1234\n" + "".join(
            f"nsm3d_verifier_input: address={a:08x} value={v:04x}\n"
            for a, v in zip(range(0x110f6, 0x11104, 2),
                            (0x100, 0x300, 0, 0xe800, 1, 1, 0x200))) +
            "nsm3d_verifier_boundary: pc=002cb310 result0=0000 result1=ffff "
            "pairs0=58 pairs1=58 order_errors=0\n")

    def test_complete_observation(self):
        check(self.trace(), b"\x12\x34")

    def test_wrong_program(self):
        with self.assertRaisesRegex(ValueError, "program"):
            check(self.trace(), b"\x12\x35")

    def test_bad_geometry(self):
        with self.assertRaisesRegex(ValueError, "geometry"):
            check(self.trace().replace("value=e800", "value=d000"), b"\x12\x34")

    def test_forced_publication(self):
        with self.assertRaisesRegex(ValueError, "publication"):
            check(self.trace().replace("result1=ffff", "result1=0006"), b"\x12\x34")

    def test_wrong_order(self):
        with self.assertRaisesRegex(ValueError, "sequence"):
            check(self.trace().replace("order_errors=0", "order_errors=1"), b"\x12\x34")

    def test_missing_boundary(self):
        with self.assertRaisesRegex(ValueError, "boundary"):
            check(self.trace().split("nsm3d_verifier_boundary:")[0], b"\x12\x34")


if __name__ == "__main__":
    unittest.main()

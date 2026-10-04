import unittest

try:
    from tools.nsm3d_runtime_hle_check import check_handoff
except ModuleNotFoundError:
    from nsm3d_runtime_hle_check import check_handoff


class RuntimeHleTests(unittest.TestCase):
    def fixture(self):
        text = "staged_dsp: runtime_hle_handoff pc=2c75 native_suspended=1\n"
        for command, argument in (("0032", "3fff"), ("0031", "ff00"),
                                  ("0033", "e000"), ("0008", "0002"),
                                  ("0009", "000f"), ("002f", "0000"),
                                  ("002f", "0000")):
            text += f"nsm3d_control_request: command={command} argument={argument}\n"
            text += "dsp_hle: parameter_accept coefficient=3fff pending=0001\n"
        return text + ("nsm3d_loader_boundary: pc=2c75 selector=0000 ack=0000 "
                       "pending=0000 fields=1e2e/1f80 t=8.000000\n")

    def test_boundary(self):
        check_handoff(self.fixture())

    def test_native_and_hle_must_not_overlap(self):
        with self.assertRaises(ValueError):
            check_handoff("dsp_hle: parameter_accept coefficient=3fff pending=0001\n" + self.fixture())

    def test_corrupted_boundary(self):
        for old, new in (("native_suspended=1", "native_suspended=0"),
                         ("coefficient=3fff", "coefficient=ffff"),
                         ("command=0032", "command=0030"),
                         ("pending=0000 fields", "pending=0001 fields")):
            with self.subTest(change=new), self.assertRaises(ValueError):
                check_handoff(self.fixture().replace(old, new))


if __name__ == "__main__":
    unittest.main()

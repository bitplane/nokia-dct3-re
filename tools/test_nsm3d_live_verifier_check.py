import unittest

try:
    from tools.nsm3d_live_verifier_check import check, check_boundary
except ModuleNotFoundError:
    from nsm3d_live_verifier_check import check, check_boundary


class LiveVerifierTests(unittest.TestCase):
    def fixture(self):
        return (
            "nsm3d_verifier_program: words=1234\n"
            "staged_dsp: publication word0=0000 word1=0006 word2=0006 word3=0006 pc=0f65 t=0.996196\n"
            "nsm3d_release: pc=002cb328 control=10 result0=0000 result1=0006 "
            "retained0=0000 retained1=0006 pairs0=58 pairs1=58 order_errors=0 t=0.996202\n"
            "nsm3d_loader_descriptor: address=00311d14 fields=fd00/ff80/027e/0500/0078/0000\n"
            "nsm3d_loader_upload: words=5678\n"
        )

    def test_native_result_retained(self):
        check(self.fixture(), bytes.fromhex("1234"))

    def test_wrong_upload(self):
        with self.assertRaises(ValueError):
            check(self.fixture(), bytes.fromhex("4321"))

    def test_wrong_declared_input_result(self):
        with self.assertRaises(ValueError):
            check(self.fixture().replace("word1=0006", "word1=0004"), bytes.fromhex("1234"))

    def test_missing_pair(self):
        with self.assertRaises(ValueError):
            check(self.fixture().replace("pairs1=58", "pairs1=57"), bytes.fromhex("1234"))

    def test_retention_before_publication(self):
        with self.assertRaises(ValueError):
            check(self.fixture().replace("t=0.996202", "t=0.1"), bytes.fromhex("1234"))

    def test_duplicate_publication(self):
        text = self.fixture()
        with self.assertRaises(ValueError):
            check(text + text.splitlines()[1], bytes.fromhex("1234"))

    def test_loader_bytes(self):
        check(self.fixture(), bytes.fromhex("1234"), bytes.fromhex("5678"))

    def test_wrong_loader_upload(self):
        with self.assertRaises(ValueError):
            check(self.fixture(), bytes.fromhex("1234"), bytes.fromhex("8765"))

    def test_fragment_and_native_delivery(self):
        text = self.fixture() + (
            "nsm3d_program_fragment: words=9abc\n"
            "staged_dsp: request selector=0014\n"
            + "staged_dsp: request selector=0001\n" * 133
            + "staged_dsp: loader2_verified words=613 entry=0a00\n")
        check(text, bytes.fromhex("1234"), bytes.fromhex("5678"), bytes.fromhex("9abc"))

    def test_exact_native_boundary(self):
        text = ("staged_dsp: installed_program words=422 first=0590 last=0735 "
                "target_data=0000 pmst=07ac\n"
                "staged_dsp: readonly_program_write address=ff87 data=0006 pc=0f12\n"
                "staged_dsp: outside_uploaded_code pc=2c75 t=1.02\n")
        check_boundary(text)
        for old, new in (("422", "421"), ("2c75", "938c"), ("0000", "f495")):
            with self.subTest(change=new), self.assertRaises(ValueError):
                check_boundary(text.replace(old, new))

    def test_unmodelled_program_write_fails(self):
        text = ("staged_dsp: installed_program words=422 first=0590 last=0735 "
                "target_data=0000 pmst=07ac\n"
                "staged_dsp: readonly_program_write address=ff87 data=0006 pc=0f12\n"
                "staged_dsp: outside_uploaded_code pc=2c75\n"
                "unmodelled program write")
        with self.assertRaisesRegex(ValueError, "program write"):
            check_boundary(text)

    def test_missing_fragment(self):
        with self.assertRaises(ValueError):
            check(self.fixture(), bytes.fromhex("1234"), bytes.fromhex("5678"), bytes.fromhex("9abc"))

    def test_illegal_instruction_is_not_progress(self):
        with self.assertRaises(ValueError):
            check(self.fixture() + "unimplemented C54x opcode f84f", bytes.fromhex("1234"))


if __name__ == "__main__":
    unittest.main()

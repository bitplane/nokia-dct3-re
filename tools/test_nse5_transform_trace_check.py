import unittest

from tools.nse5_transform_trace_check import check_codec, check_mix, check_nonlinear, check_structure, check_trace, mix_words, rotate32, transform_words


RECORD = (
    "nse5_compat_rotation: base=1202 input=218c:f9a8 output=6a08:633e "
    "other=1206 other_input=c22d:1de4 other_output=845a:3bc9 t=0.671721038\n"
)


class RotationTraceTests(unittest.TestCase):
    def test_observed(self):
        self.assertEqual(check_trace(RECORD), 1)

    def test_mismatch(self):
        with self.assertRaisesRegex(ValueError, "mismatch"):
            check_trace(RECORD.replace("633e", "633f"))

    def test_missing(self):
        with self.assertRaises(ValueError):
            check_trace("")

    def test_width(self):
        with self.assertRaises(ValueError):
            check_trace(RECORD.replace("218c:", ""))

    def test_wrap(self):
        self.assertEqual(rotate32(0x8000, 0, 31), (0, 1))

    def test_observed_mix(self):
        record = "nse5_compat_mix: input=e9b8:f1d4:27e2:e4fa:4cd7:54e6 output=218c:f9a8 t=0.671717596"
        self.assertEqual(check_mix(record), 1)
        with self.assertRaisesRegex(ValueError, "mismatch"):
            check_mix(record.replace("218c", "218d"))

    def test_mix_missing(self):
        with self.assertRaises(ValueError):
            check_mix("")

    def test_mix_width(self):
        with self.assertRaises(ValueError):
            mix_words((0, 0))

    def test_mix_zero(self):
        self.assertEqual(mix_words((0,) * 6), (0, 0))

    def test_observed_structure(self):
        text = (
            "nse5_compat_rounds: count=11 schedule=b6f6 base=1202 t=0.672228096\n"
            "nse5_compat_reverse: base=1202 input=4ce3:6e54:6648:7cd2:7d53:916d "
            "output=b689:cabe:4b3e:1266:2a76:c732 t=0.672239769\n"
        )
        self.assertEqual(check_structure(text), (1, 1))
        for bad in (text.replace("count=11", "count=10"), text.replace("b689", "b688")):
            with self.assertRaises(ValueError):
                check_structure(bad)

    def test_structure_missing(self):
        with self.assertRaises(ValueError):
            check_structure("")

    def test_full_observed_transform(self):
        table = (0x6521, 0x4cda, 0x4d33, 0x3bc6, 0x3342, 0x2c9e)
        schedule = (0xd0d0, 0x1616, 0x2c2c, 0x5858, 0xb0b0, 0x7171,
                    0xe2e2, 0xd5d5, 0x5a5a, 0x6767, 0xcece, 0x8d8d)
        self.assertEqual(transform_words((0x8184, 0xb191, 0x1537, 0x5a67, 0x60ed, 0xee9d), table, schedule),
                         (0xb689, 0xcabe, 0x4b3e, 0x1266, 0x2a76, 0xc732))
        self.assertEqual(transform_words((0x1dd9, 0x5eb8, 0xfe6b, 0xef3c, 0x1337, 0x819e), table, schedule),
                         (0xf409, 0x1f8d, 0xdc07, 0x324f, 0x2ad0, 0x91d9))

    def test_codec_record(self):
        text = (
            "nse5_compat_codec: input=8184:b191:1537:5a67:60ed:ee9d "
            "table=6521:4cda:4d33:3bc6:3342:2c9e "
            "schedule=d0d0:1616:2c2c:5858:b0b0:7171:e2e2:d5d5:5a5a:6767:cece:8d8d "
            "output=b689:cabe:4b3e:1266:2a76:c732 t=0.672239769"
        )
        self.assertEqual(check_codec(text), 1)
        with self.assertRaises(ValueError):
            check_codec(text.replace("b689", "b688"))

    def test_nonlinear_zero(self):
        self.assertEqual(check_nonlinear(
            "nse5_compat_nonlinear: input=0000:0000:0000:0000:0000:0000 "
            "output=ffff:ffff:ffff:ffff:ffff:ffff t=1.0"), 1)

    def test_missing_full_stages(self):
        for check in (check_codec, check_nonlinear):
            with self.assertRaises(ValueError):
                check("")


if __name__ == "__main__":
    unittest.main()

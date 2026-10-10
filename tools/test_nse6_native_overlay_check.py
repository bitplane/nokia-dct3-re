import unittest

from tools.nse6_native_overlay_check import check


def fixture():
    words = [0x7714] + list(range(1, 24))
    lines = [f"nse6_native_context_upload: address={0x2946 + index:04x} value={word:04x} mask=ffff pc=31d7 pmst=ffac t=0.256"
             for index, word in enumerate(words)]
    body = ",".join(f"{word:04x}" for word in words)
    lines.append(f"nse6_native_context_target: address=2946 pmst=ffac program={body} data={body} t=2.475")
    lines.append("nse6_native_stack_depth: address=1ec5 value=0f17 pc=0f17 sp=1ec5 st1=2900 t=0.011")
    lines += [f"nse6_native_snapshot: pc=408d sp=1ec4 imr=035f ifr=0020 t={second}.000"
              for second in (1, 14, 28)]
    lines += [f"nse6_native_rx_index_write: address=08e4 value={value:04x} pc=3805 t=0.48"
              for value in (0x89, 0xa4, 0xa6, 0xa9)]
    return "\n".join(lines)


class NativeOverlayTests(unittest.TestCase):
    def test_valid(self):
        self.assertEqual(check(fixture())["words"], 24)

    def test_missing(self):
        with self.assertRaises(ValueError):
            check("")

    def test_corruptions(self):
        for old, new in (
            ("program=7714", "program=0000"),
            ("data=7714", "data=0000"),
            ("address=295d", "address=295e"),
            ("pmst=ffac program", "pmst=ff8c program"),
            ("t=2.475", "t=0.200"),
            ("sp=1ec4", "sp=1ca4"),
            ("sp=1ec5", "sp=1bff"),
            ("t=28.000", "t=27.000"),
            ("value=00a9", "value=3e1f"),
        ):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check(fixture().replace(old, new))

    def test_unsupported_opcode(self):
        with self.assertRaises(ValueError):
            check(fixture() + "\nunimplemented C54x opcode 1234")


if __name__ == "__main__":
    unittest.main()

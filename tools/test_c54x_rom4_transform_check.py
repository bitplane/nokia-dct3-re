import struct
import tempfile
import unittest
from pathlib import Path

from tools.c54x_rom4_transform_check import (
    EXPECTED_RESPONSE,
    check_trace,
    parse_trace,
    read_response,
)


def line(step, pc, op, ar4=0x1208):
    return (
        f"[cmp] {step} pc={pc:04x} op={op:04x} A=0000000000 B=0000000000 "
        f"T=0000 C=0 ar2=13d9 ar3=13d7 ar4={ar4:04x} "
        "ar5=13dc bk=0052 b=0000"
    )


def valid_trace():
    lines = [line(1, 0x4B73, 0xF074), line(2, 0x7F2D, 0x7712),
             line(3, 0x7FF5, 0xF072)]
    step = 4
    for ar4 in range(0x1208, 0x1202, -1):
        lines.extend((line(step, 0x7FF7, 0xF074, ar4),
                      line(step + 1, 0x7FFA, 0x108A, ar4),
                      line(step + 2, 0x8000, 0xFC00, ar4)))
        step += 3
    return "\n".join(lines)


class C54xRom4TransformCheckTest(unittest.TestCase):
    def test_accepts_six_call_repeat(self):
        self.assertEqual(check_trace(parse_trace(valid_trace()))["rptb_calls"], 6)

    def test_rejects_branch_target_repeat(self):
        records = parse_trace(valid_trace())
        records.pop(next(i for i, record in enumerate(records)
                         if record["pc"] == 0x7FFA))
        with self.assertRaisesRegex(ValueError, "expected six"):
            check_trace(records)

    def test_reads_accepted_big_endian_word_response(self):
        image = bytearray(0x2000)
        struct.pack_into(f">{len(EXPECTED_RESPONSE)}H", image, 0x1400, *EXPECTED_RESPONSE)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "data.bin"
            path.write_bytes(image)
            self.assertEqual(read_response(path), EXPECTED_RESPONSE)

    def test_rejects_base_above_response(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "data.bin"
            path.write_bytes(bytes(32))
            with self.assertRaisesRegex(ValueError, "above response"):
                read_response(path, 0x1300)


if __name__ == "__main__":
    unittest.main()

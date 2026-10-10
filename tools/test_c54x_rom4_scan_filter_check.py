import unittest

from tools.c54x_rom4_scan_filter_check import COEFFICIENTS, HISTORY, PCS, check


def fixture():
    lines = [f"rom4_scan_filter: t=1.64 st0=181f st1=e900 coefficients={COEFFICIENTS}"] * 2
    operands = []
    coefficients = COEFFICIENTS.split(",")
    for index, (pc, history) in enumerate(zip(PCS, HISTORY)):
        if index != 15:
            operands.append((pc, 0x21a9 + index // 2, coefficients[index // 2]))
        operands.append((pc, history, "0000"))
    operands.append((0x3323, 0x84, "0000"))
    lines.extend(f"rom4_scan_operand: index={index} pc={pc:04x} address={address:04x} value={value}"
                 for index, (pc, address, value) in enumerate(operands, 1))
    return "\n".join(lines)


class ScanFilterTests(unittest.TestCase):
    def test_capture(self):
        self.assertEqual(check(fixture()), 32)

    def test_missing_duplicate_and_reordered_reads(self):
        text = fixture()
        for changed in (text.rsplit("\n", 1)[0], text + "\n" + text.splitlines()[-1],
                        text.replace("address=0087", "address=0086")):
            with self.subTest(changed=changed), self.assertRaises(ValueError):
                check(changed)

    def test_arithmetic_and_coefficients(self):
        for old, new in (("st1=e900", "st1=e940"), ("st0=181f", "st0=181e"),
                         ("value=0387", "value=0388"), ("coefficients=0387", "coefficients=0388")):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check(fixture().replace(old, new))

    def test_nonzero_history_is_not_rejected_as_wrong_format(self):
        self.assertEqual(check(fixture().replace("value=0000", "value=1234")), 32)


if __name__ == "__main__":
    unittest.main()

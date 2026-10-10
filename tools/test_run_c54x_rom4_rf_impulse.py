import unittest

from tools.run_c54x_rom4_rf_impulse import check, expected


def fixture(index=0, amplitude=0x4000):
    lines = [f"rom4_rf_impulse_input: index={n} pc={(0x330c, 0x3312, 0x3318, 0x331e)[n % 4]:04x} value={(amplitude if n == index else 0):04x}"
             for n in range(32)]
    lines += [f"rom4_rf_impulse_output: index={n} pc={0x3322 + n % 2:04x} address={0x1a48 + n % 16:04x} value={value:04x}"
              for n, value in enumerate(expected(index, amplitude))]
    lines.append(f"rom4_rf_impulse_summary: impulse={index} amplitude={amplitude:04x} inputs=100 outputs=48 acquisition_claim=0")
    return "\n".join(lines)


class ImpulseTests(unittest.TestCase):
    def test_known_positive_response(self):
        self.assertEqual(expected(0, 0x4000)[:8], [0, 0x469, 0, 0x1000, 0, 0xab6, 0, 0xe2])
        self.assertEqual(expected(2, 0x4000)[:8], [0, 0xe2, 0, 0xab6, 0, 0x1000, 0, 0x469])

    def test_signed_rounding(self):
        self.assertEqual(expected(1, 0xc000)[:8], [0xfb97, 0, 0xf000, 0, 0xf54b, 0, 0xff1e, 0])

    def test_all_positions(self):
        for amplitude in (0x4000, 0xc000):
            for index in range(32):
                with self.subTest(index=index, amplitude=amplitude):
                    check(fixture(index, amplitude), index, amplitude)

    def test_reject_corrupt_evidence(self):
        text = fixture()
        for changed in (text.replace("pc=330c", "pc=324b"),
                        text.replace("value=0469", "value=0468"),
                        text.replace("address=1a48", "address=1a49"),
                        text.rsplit("\n", 1)[0], text + "\n" + text.splitlines()[32]):
            with self.subTest(changed=changed), self.assertRaises(ValueError):
                check(changed, 0, 0x4000)


if __name__ == "__main__":
    unittest.main()

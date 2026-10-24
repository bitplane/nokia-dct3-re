import unittest

from tools.run_c54x_rom4_rf_impulse import check, expected, expected_words, mixed_words


def fixture(index=0, amplitude=0x4000):
    lines = [f"rom4_rf_impulse_input: index={n} pc={(0x330c, 0x3312, 0x3318, 0x331e)[n % 4]:04x} value={(amplitude if n == index else 0):04x}"
             for n in range(32)]
    lines += [f"rom4_rf_impulse_output: index={n} pc={0x3322 + n % 2:04x} address={0x1a48 + n % 16:04x} value={value:04x}"
              for n, value in enumerate(expected(index, amplitude))]
    lines.append(f"rom4_rf_impulse_summary: impulse={index} amplitude={amplitude:04x} inputs=100 outputs=48 acquisition_claim=0")
    return "\n".join(lines)


class ImpulseTests(unittest.TestCase):
    def test_selected_frontend_and_healthy_cpu_are_required(self):
        text=fixture()+'\nrom4_rf_profile: system=nse1astd\nrom4_rf_cpu_state: illegal=0 pc=0123\n'
        check(text,0,0x4000,machine='nse1astd')
        for changed in (text.replace('system=nse1astd','system=noki5110'),
                        text.replace('illegal=0','illegal=1'),
                        fixture(),text+'rom4_rf_profile: system=nse1astd\n'):
            with self.subTest(changed=changed),self.assertRaises(ValueError):
                check(changed,0,0x4000,machine='nse1astd')

    def test_staged_bus_owner_not_prefetch_pc_controls_attribution(self):
        lines=[]
        for line in fixture().splitlines():
            if line.startswith('rom4_rf_impulse_input:'):
                index=int(line.split('index=')[1].split()[0])
                line += f' owner={(0x330a,0x3310,0x3316,0x331c)[index%4]:08x}'
            elif line.startswith('rom4_rf_impulse_output:'):
                index=int(line.split('index=')[1].split()[0])
                line += f' owner={0x3321+index%2:08x}'
            lines.append(line)
        text='\n'.join(lines)+'\nrom4_rf_profile: system=nse1pipe\nrom4_rf_cpu_state: illegal=0 pc=0123\n'
        check(text,0,0x4000,machine='nse1pipe')
        for changed in (text.replace('owner=0000330a','owner=0000330f'),
                        text.replace('owner=00003321','owner=00003322'),
                        text.replace(' owner=0000330a','')):
            with self.subTest(changed=changed),self.assertRaises(ValueError):
                check(changed,0,0x4000,machine='nse1pipe')

    def test_full_scale_capture_and_profile_identity(self):
        words = mixed_words(full_scale=True)
        self.assertEqual(len(words), 32)
        self.assertIn(0x7fff, words)
        self.assertIn(0x8000, words)
        lines = [f'rom4_rf_impulse_input: index={index} '
                 f'pc={(0x330c, 0x3312, 0x3318, 0x331e)[index % 4]:04x} value={word:04x}'
                 for index, word in enumerate(words)]
        lines.extend(f'rom4_rf_impulse_output: index={index} pc={0x3322 + index % 2:04x} '
                     f'address={0x1a48 + index % 16:04x} value={word:04x}'
                     for index, word in enumerate(expected_words(words)))
        lines.append('rom4_rf_mixed_full_scale_summary: samples=32 inputs=100 outputs=48 acquisition_claim=0')
        text = '\n'.join(lines)
        check(text, 0, 0, mixed=True, full_scale=True)
        for changed in (text.replace('value=8000', 'value=7fff'),
                        text.replace('mixed_full_scale_summary', 'mixed_summary')):
            with self.assertRaises(ValueError):
                check(changed, 0, 0, mixed=True, full_scale=True)
        with self.assertRaises(ValueError):
            check(text, 0, 0, mixed=True)
        with self.assertRaises(ValueError):
            check(text, 0, 0, full_scale=True)

    def test_full_scale_constant_rounding(self):
        self.assertEqual(expected_words([0x7fff] * 32)[6:16], [0x7fff] * 10)
        self.assertEqual(expected_words([0x8000] * 32)[6:16], [0x8000] * 10)

    def test_overlap_rounds_after_accumulation(self):
        # Every individual unit impulse rounds to zero, but the eight-tap
        # coefficient sum is 1.0, so the settled constant input must be one.
        self.assertEqual(expected_words([1] * 32)[6:16], [1] * 10)
        self.assertTrue(all(value == 0 for index in range(32)
                            for value in expected(index, 1)))
        self.assertEqual(expected_words([0xffff] * 32)[6:16], [0xffff] * 10)

    def test_bounded_mixed_words_and_full_capture(self):
        words = mixed_words()
        signed = [word if word < 0x8000 else word - 0x10000 for word in words]
        self.assertEqual(len(words), 32)
        self.assertTrue(all(-2048 <= value <= 2047 for value in signed))
        self.assertGreater(len(set(words)), 20)
        lines = [f'rom4_rf_impulse_input: index={index} '
                 f'pc={(0x330c, 0x3312, 0x3318, 0x331e)[index % 4]:04x} value={word:04x}'
                 for index, word in enumerate(words)]
        lines.extend(f'rom4_rf_impulse_output: index={index} pc={0x3322 + index % 2:04x} '
                     f'address={0x1a48 + index % 16:04x} value={word:04x}'
                     for index, word in enumerate(expected_words(words)))
        lines.append('rom4_rf_mixed_summary: samples=32 inputs=100 outputs=48 acquisition_claim=0')
        text = '\n'.join(lines)
        check(text, 0, 0, mixed=True)
        for changed in (text.replace('samples=32', 'samples=31'),
                        text.replace('value=fcd5', 'value=fcd4'),
                        text.replace('index=47 ', 'index=48 ')):
            self.assertNotEqual(text, changed)
            with self.assertRaises(ValueError):
                check(changed, 0, 0, mixed=True)

    def test_rejects_invalid_input_domain(self):
        for words in ([0] * 31, [0] * 33, [-1] * 32, [65536] * 32):
            with self.assertRaises(ValueError):
                expected_words(words)
        for index in (-1, 32):
            with self.assertRaises(ValueError):
                expected(index, 0x4000)

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
        for changed in (text + '\n[LUA ERROR] error in write tap callback',
                        text.replace("pc=330c", "pc=324b"),
                        text.replace("value=0469", "value=0468"),
                        text.replace("address=1a48", "address=1a49"),
                        text.rsplit("\n", 1)[0], text + "\n" + text.splitlines()[32]):
            with self.subTest(changed=changed), self.assertRaises(ValueError):
                check(changed, 0, 0x4000)


if __name__ == "__main__":
    unittest.main()

import unittest
from unittest.mock import patch
from pathlib import Path
from tempfile import TemporaryDirectory

from tools.run_c54x_rom4_pcm_clock import INSTRUCTION_SETS, instruction_report, paired_screen


CAPTURE = ("[opcov] op=6f83 first_pc=0800 count=1\n"
           "[opdecode] op=6f83 supported=0 words=0\n"
           "[opwords] op=6f83 words=2 ext1=0c1e ext2=0000 first_pc=010800 count=1\n"
           "[optupledecode] op=6f83 words=2 ext1=0c1e ext2=0000 supported=1 decoded_words=2\n"
           "[opcov] op=f47f first_pc=a68c count=20\n"
           "[opdecode] op=f47f supported=1 words=1\n")


class NativePcmInstructionReportTests(unittest.TestCase):
    def test_paired_screen_uses_pixels_not_png_metadata(self):
        from PIL import Image, PngImagePlugin
        with TemporaryDirectory() as directory:
            first, reference = (Path(directory) / name for name in ('first.png', 'reference.png'))
            image = Image.new('RGB', (84, 48), 'white')
            image.putpixel((3, 4), (0, 0, 0))
            metadata = PngImagePlugin.PngInfo()
            metadata.add_text('System', 'clocked boot')
            image.save(first, pnginfo=metadata)
            image.save(reference)
            self.assertNotEqual(first.read_bytes(), reference.read_bytes())
            self.assertTrue(paired_screen(first, reference)['paired_pixels_equal'])
            image.putpixel((3, 4), (255, 255, 255))
            image.save(first)
            with self.assertRaisesRegex(ValueError, 'nonblank'):
                paired_screen(first, reference)
            image.putpixel((4, 4), (0, 0, 0))
            image.save(first)
            with self.assertRaisesRegex(ValueError, 'differs'):
                paired_screen(first, reference)
            image.resize((48, 84)).save(first)
            with self.assertRaisesRegex(ValueError, '84x48'):
                paired_screen(first, reference)

    def test_decoder_observations_are_not_arithmetic_assertions(self):
        result = instruction_report(CAPTURE)
        self.assertEqual(result['opcodes'], 2)
        self.assertEqual(result['groups'], 2)
        self.assertEqual(len(result['sha256']), 64)
        self.assertFalse(result['independent_arithmetic_assertion_claim'])

    def test_profile_fingerprints_reject_each_changed_dimension(self):
        from tools.c54x_opcode_coverage import summarize
        base = summarize(CAPTURE)
        for profile, expected in INSTRUCTION_SETS.items():
            for changed in (None, 0, 1, 2):
                values = list(expected)
                if changed is not None:
                    values[changed] = '0' * 64 if changed == 2 else values[changed] + 1
                observed = dict(base, opcodes=values[0], high_byte_groups=values[1], set_sha256=values[2])
                with self.subTest(profile=profile, changed=changed), patch(
                        'tools.run_c54x_rom4_pcm_clock.summarize', return_value=observed):
                    if changed is None:
                        self.assertEqual(instruction_report(CAPTURE, profile)['sha256'], expected[2])
                    else:
                        with self.assertRaisesRegex(ValueError, 'instruction-set drift'):
                            instruction_report(CAPTURE, profile)

    def test_real_small_capture_is_not_a_banked_handset_set(self):
        for profile in INSTRUCTION_SETS:
            with self.subTest(profile=profile), self.assertRaisesRegex(ValueError, 'instruction-set drift'):
                instruction_report(CAPTURE, profile)

    def test_missing_unsupported_and_truncated_records_fail(self):
        for bad in (CAPTURE.replace('[opdecode] op=f47f supported=1 words=1\n', ''),
                    CAPTURE.replace('op=f47f supported=1 words=1', 'op=f47f supported=0 words=0'),
                    CAPTURE.replace('ext1=0c1e ext2=0000 first_pc=010800 count=1', 'ext1=0c1e ext2=0000 first_pc=010800 count=2'),
                    CAPTURE.replace('supported=1 decoded_words=2', 'supported=0 decoded_words=0'),
                    CAPTURE.replace('[optupledecode]', '[lostdecode]')):
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                instruction_report(bad)


if __name__ == '__main__':
    unittest.main()

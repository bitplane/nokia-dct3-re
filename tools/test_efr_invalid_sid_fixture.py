import struct
import unittest

from tools.verify_etsi_efr_reference import BAD_SID_FRAMES, DTX_FRAMES, DTX_SID_FRAMES
from tools.verify_etsi_efr_reference import INVALID_SID_BITS, INVALID_SID_FRAMES
from tools.verify_etsi_efr_reference import check_sid_classes, invalid_sid_input


def source():
    frames = []
    erased = 0
    for index in range(DTX_FRAMES):
        sid = index in DTX_SID_FRAMES
        bfi = int(not sid and erased < 135)
        erased += bfi
        frames.append(struct.pack('<247H', bfi, *([int(sid)] * 244),
                                  2 if sid else 0, int(index % 24 == 23)))
    return b''.join(frames)


class InvalidSidFixtureTests(unittest.TestCase):
    def test_only_declared_codeword_bits_flags_and_bfi_change(self):
        original = source()
        result = invalid_sid_input(original)
        before, after = list(struct.iter_unpack('<247H', original)), list(struct.iter_unpack('<247H', result))
        for index, (old, new) in enumerate(zip(before, after)):
            expected = list(old)
            if index in INVALID_SID_FRAMES:
                for bit in INVALID_SID_BITS:
                    expected[1 + bit] = 0
                expected[245] = 1
            if index in BAD_SID_FRAMES:
                expected[0] = 1
            self.assertEqual(tuple(expected), new)
        self.assertEqual(sum(row[0] for row in after), 137)
        self.assertEqual(sum(row[245] == 1 for row in after), 4)
        self.assertEqual(sum(row[245] == 2 and row[0] for row in after), 1)

    def test_rejects_missing_changed_or_already_corrupted_source(self):
        original = source()
        cases = [original[:-1]]
        for frame, word, value in ((51, 245, 0), (0, 1, 2), (0, 246, 2),
                                    (51, 46, 0), (51, 0, 1), (263, 0, 1)):
            changed = bytearray(original)
            struct.pack_into('<H', changed, frame * 494 + word * 2, value)
            cases.append(changed)
        for changed in cases:
            with self.subTest(changed=changed[:10]), self.assertRaises(ValueError):
                invalid_sid_input(changed)

    def test_payload_classifier_requires_exact_full_correlated_frames(self):
        decoder = invalid_sid_input(source())
        rows = [f'EFRSID frame={index} class={row[245]}'
                for index, row in enumerate(struct.iter_unpack('<247H', decoder))]
        text = '\n'.join(rows)
        check_sid_classes(text, decoder)
        for bad in ('\n'.join(rows[:-1]), '\n'.join([rows[0], *rows]),
                    text.replace('frame=51 class=1', 'frame=51 class=2'),
                    text.replace('frame=51 class=1', 'frame=50 class=1'),
                    text.replace('frame=51 class=1', 'frame=51 class=3')):
            with self.assertRaises(ValueError):
                check_sid_classes(bad, decoder)
        for bad in (decoder[:-1], b''):
            with self.assertRaises(ValueError):
                check_sid_classes(text, bad)


if __name__ == '__main__':
    unittest.main()

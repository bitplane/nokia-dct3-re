import unittest
from tools.npe3_tch_impairment_check import check


def report(frame, rxqual, speech, bad):
    octets = bytes([5, 0, 1, 3, 0x49, 6, 0x15, 0x21, 0x21, rxqual << 4]) + bytes(13)
    return (f'npe3_tch_uplink: channel=sacch start_frame={frame} octets={octets.hex()} exact_code=1 '
            f'speech_blocks={speech} bad_speech_blocks={bad} unreadable_bursts=0 x')


def fixture(during=(4, 7, 6), after=0, bad_inside=48):
    lines = ['npe3_tch_impairment: first_frame=1000 last_frame=1208 bit_errors=1/3 '
             'directions=downlink,uplink sacch=intact physical_claim=0']
    lines += [report(900, 0, 80, 3), report(1000, 2, 100, 3)]
    for n, rxqual in enumerate(during):
        lines.append(report(1104 + 104 * n, rxqual, 100 + 10 * n, 3 + bad_inside * (n + 1) // len(during)))
    for n in range(4):
        lines.append(report(1416 + 104 * n, after, 200 + 60 * n, 3 + bad_inside))
    return '\n'.join(lines)


class ImpairmentTest(unittest.TestCase):
    def test_native_rxqual_rises_and_recovers(self):
        result = check(fixture())
        self.assertEqual((result['peak_rxqual_full'], result['bad_uplink_frames_in_window']), (7, 48))

    def test_rejects_unnoticed_unrecovered_or_missing(self):
        for text in (fixture(during=(2, 3, 1)), fixture(after=5), fixture(bad_inside=10),
                     fixture().split('\n', 1)[1]):
            with self.subTest(text=text[:40]), self.assertRaises(ValueError):
                check(text)


if __name__ == '__main__':
    unittest.main()

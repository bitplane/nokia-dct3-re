import math
import struct
import unittest

from tools.lab_host_call_client import tone_fraction, without_gaps


def tone(frames, hz=500):
    # Offset the phase so no tone sample is itself zero.
    return [round(8000 * math.sin(2 * math.pi * hz * n / 8000 + 0.3)) for n in range(160 * frames)]


def pack(samples):
    return struct.pack(f'<{len(samples)}h', *samples)


class WithoutGapsTest(unittest.TestCase):
    def test_restores_a_tone_gated_by_silent_frames(self):
        speech = tone(20)
        # Frames rarely align with the 160-sample analysis blocks.
        gated = [0] * 80
        for frame in range(20):
            gated += speech[160 * frame:160 * (frame + 1)] + [0] * 160
        self.assertLess(tone_fraction(pack(gated), 500), 0.6)
        self.assertEqual(without_gaps(pack(gated)), pack(speech))
        self.assertGreater(tone_fraction(without_gaps(pack(gated)), 500), 0.99)

    def test_keeps_short_zero_runs_and_trailing_silence_rule(self):
        samples = [5] + [0] * 79 + [5] + [0] * 80
        self.assertEqual(without_gaps(pack(samples)), pack([5] + [0] * 79 + [5]))


if __name__ == '__main__':
    unittest.main()

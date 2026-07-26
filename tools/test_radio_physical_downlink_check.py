from pathlib import Path
from tempfile import TemporaryDirectory
import math
import unittest
import wave

from tools.radio_physical_downlink_check import check


class RadioPhysicalDownlinkCheckTests(unittest.TestCase):
    def write_capture(
        self, directory: str, seconds: float, amplitude: int, frequency: int
    ) -> Path:
        path = Path(directory) / "downlink.wav"
        rate = 8_000
        frames = bytearray()
        for index in range(round(seconds * rate)):
            sample = round(
                amplitude * math.sin(2.0 * math.pi * frequency * index / rate)
            )
            frames.extend(sample.to_bytes(2, "little", signed=True))
        with wave.open(str(path), "wb") as destination:
            destination.setnchannels(1)
            destination.setsampwidth(2)
            destination.setframerate(rate)
            destination.writeframes(frames)
        return path

    def test_sustained_service_tone_passes(self):
        with TemporaryDirectory() as directory:
            result = check(self.write_capture(directory, 2.0, 4_000, 1_000))
            self.assertIn("1 kHz run=2000 ms", result)

    def test_silence_fails(self):
        with TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, "silent"):
                check(self.write_capture(directory, 2.0, 0, 1_000))

    def test_unrelated_tone_fails(self):
        with TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, "1 kHz"):
                check(self.write_capture(directory, 2.0, 4_000, 440))

    def test_short_service_tone_fails(self):
        with TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, "shorter"):
                check(self.write_capture(directory, 0.4, 4_000, 1_000))

    def test_clipping_fails(self):
        with TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, "clipped"):
                check(self.write_capture(directory, 2.0, 32_767, 1_000))


if __name__ == "__main__":
    unittest.main()

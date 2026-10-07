import ctypes.util
import math
import struct
import unittest

from tools.gsm_fr_codec import GsmFrCodec


@unittest.skipUnless(ctypes.util.find_library('gsm'), 'optional libgsm not installed')
class GsmFrCodecTest(unittest.TestCase):
    def test_tone_survives_standard_frame_roundtrip(self):
        with GsmFrCodec() as codec:
            pcm = struct.pack('=160h', *(round(8000 * math.sin(2 * math.pi * 440 * i / 8000))
                                       for i in range(160)))
            frames = [codec.encode(pcm) for _ in range(8)]
            self.assertTrue(all(len(frame) == 33 and frame[0] >> 4 == 0xd for frame in frames))
            output = [codec.decode(frame) for frame in frames]
            samples = struct.unpack('=160h', output[-1])
            self.assertGreater(sum(value * value for value in samples) / 160, 1000000)

    def test_rejects_wrong_lengths_and_invalid_magic(self):
        with GsmFrCodec() as codec:
            for pcm in (b'', bytes(318), bytes(322)):
                with self.assertRaises(ValueError):
                    codec.encode(pcm)
            for frame in (b'', bytes(32), bytes(34), bytes(33)):
                with self.assertRaises(ValueError):
                    codec.decode(frame)

    def test_closed_codec_rejects_use(self):
        codec = GsmFrCodec()
        codec.close()
        codec.close()
        with self.assertRaises(RuntimeError):
            codec.encode(bytes(320))


if __name__ == '__main__':
    unittest.main()

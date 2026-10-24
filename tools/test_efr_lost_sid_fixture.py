import struct
import unittest
from unittest.mock import patch

from tools.verify_etsi_efr_reference import DTX_EXPECTED, DTX_FRAMES, lost_sid_input
from tools.run_efr_lost_sid_reference import check_flags


class LostSidFixtureTests(unittest.TestCase):
    def source(self):
        frames = [struct.pack('<247H', 0, *([0] * 244),
                              2 if index in (71, 95) else 0,
                              int(index % 24 == 23))
                  for index in range(DTX_FRAMES)]
        return b''.join(frames)

    def test_only_two_channel_flag_pairs_change(self):
        original = self.source()
        with patch('tools.verify_etsi_efr_reference.digest', return_value=DTX_EXPECTED['dec']):
            result = lost_sid_input(original)
        expected = bytearray(original)
        for index in (71, 95):
            struct.pack_into('<H', expected, index * 494, 1)
            struct.pack_into('<H', expected, index * 494 + 490, 0)
        self.assertEqual(result, expected)

    def test_unverified_source_rejected(self):
        with self.assertRaisesRegex(ValueError, 'verified DTX'):
            lost_sid_input(self.source())

    def test_wrong_scheduled_flags_rejected(self):
        for word in (0, 245, 246):
            data = bytearray(self.source())
            struct.pack_into('<H', data, 71 * 494 + word * 2, 1 if word != 246 else 0)
            with patch('tools.verify_etsi_efr_reference.digest', return_value=DTX_EXPECTED['dec']):
                with self.assertRaisesRegex(ValueError, 'scheduled SID'):
                    lost_sid_input(data)

    def test_reference_observation_coverage_and_correlation(self):
        decoder = self.source()
        frames = list(struct.iter_unpack('<247H', decoder))
        rows = []
        for index, frame in enumerate(frames):
            ctrl = {95: 0x290, 96: 0x2c0, 97: 0x501, 356: 0x401}.get(index, 1)
            rows.append(f'EFRDTX frame={index} taf={frame[246]} bfi={frame[0]} sid={frame[245]} ctrl={ctrl:04x}')
        text = '\n'.join(rows)
        check_flags(text, decoder)
        for bad in ('\n'.join(rows[:-1]), text.replace('ctrl=0501', 'ctrl=0401'),
                    text.replace('ctrl=0290', 'ctrl=0210'),
                    text.replace('frame=71 taf=1', 'frame=71 taf=0')):
            with self.subTest(bad=bad[:60]), self.assertRaises(ValueError):
                check_flags(bad, decoder)

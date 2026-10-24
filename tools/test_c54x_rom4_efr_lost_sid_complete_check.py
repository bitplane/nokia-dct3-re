import struct
import unittest

from tools.c54x_rom4_efr_loss_check import characterize, LOST_SID_REPLAY, LOST_SID_COMPLETE


def fixture(staged=False):
    sids = {51, 71, 95, 259, 263, 287, 311, 335}
    erased = set([i for i in range(518) if i not in sids][:135]) | {71, 95}
    serial, rows = bytearray(), []
    if staged:
        rows.append('ROM4 EFR lost SID decoder frontend: staged handset_audio_claim=0')
    for frame in range(518):
        serial.extend(struct.pack('<247H', int(frame in erased), *([0] * 244),
                                  2 if frame in sids - {71, 95} else 0, int(frame % 24 == 23)))
        rows.append(f'ROM4 EFR lost SID observation: frame={frame} mismatches={int(frame == 97)} pcm=' +
                    ('0001' if frame == 97 else '0000') + '0000' * 159)
    for frame, actual, reference in ((95, 0x290, 0x290), (96, 0x2c0, 0x2c0), (97, 0x401, 0x501), (356, 0x401, 0x401)):
        rows.append(f'ROM4 EFR lost SID flags: frame={frame} native={actual:04x} reference={reference:04x}')
    rows.extend((LOST_SID_REPLAY, 'TMS320C54x ROM4 EFR lost SID observation: COMPLETE frames=518 samples=82880 lost_sid=2 mismatches=1 conformance_claim=0 handset_audio_claim=0', LOST_SID_COMPLETE))
    return '\n'.join(rows), bytes(518 * 320), bytes(serial)


class LostSidCompleteTests(unittest.TestCase):
    def test_complete_characterization_is_not_conformance(self):
        text, reference, serial = fixture()
        report, pcm = characterize(text, 3, reference, serial, lost_sid=True)
        self.assertEqual(report['frames'], 518)
        self.assertEqual(report['mismatches'], 1)
        self.assertEqual(report['first_differing_frame'], 97)
        self.assertFalse(report['conformance_claim'])
        self.assertFalse(report['reference_equal'])
        self.assertEqual(len(pcm), 165760)

    def test_staged_selection_required(self):
        text, reference, serial = fixture(staged=True)
        characterize(text, 3, reference, serial, lost_sid=True, staged=True)
        with self.assertRaisesRegex(ValueError, 'frontend'):
            characterize(text, 3, reference, serial, lost_sid=True)

    def test_incomplete_or_forged_summary_rejected(self):
        text, reference, serial = fixture()
        for bad in (text.replace(LOST_SID_REPLAY, ''), text.replace('frame=517 ', 'frame=516 '),
                    text.replace('mismatches=1 conformance', 'mismatches=0 conformance'),
                    text.replace('native=0401 reference=0501', 'native=0501 reference=0501'),
                    text.replace('pcm=0001', 'pcm=0000'), text.replace('conformance_claim=0', 'conformance_claim=1')):
            with self.assertRaises(ValueError):
                characterize(bad, 3, reference, serial, lost_sid=True)

    def test_wrong_schedule_rejected(self):
        text, reference, serial = fixture()
        bad = bytearray(serial)
        struct.pack_into('<H', bad, 71 * 494 + 492, 0)
        with self.assertRaisesRegex(ValueError, 'scheduled'):
            characterize(text, 3, reference, bad, lost_sid=True)

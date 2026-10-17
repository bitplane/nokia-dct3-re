import unittest

from tools.mu4_raw_output_check import TRANSFERS, verify


def fixture():
    lines, time = [], 1.019

    def event(kind, address, value):
        nonlocal time
        time += 0.00000001
        lines.append(f'mu4_raw_{kind}: address={address:04x} value={value:04x} mask=ffff '
                     f't={time:.9f} ownership=data_bus observer_writes=0')

    for source, destination, value in TRANSFERS:
        for i, word in enumerate((value >> 16, value & 0xffff)):
            event('writer', source + i, word)
        for i, word in enumerate((value >> 16, value & 0xffff)):
            event('reader', source + i, word)
        for i, word in enumerate((value >> 16, value & 0xffff)):
            event('writer', destination + i, word)
    lines.append(f'mu4_raw_terminal: records={len(lines)} closed=1 capped=0 observer_writes=0 audio_acceptance=0')
    return '\n'.join(lines)


class RawOutputCheckTests(unittest.TestCase):
    def test_complete_copy_chain_does_not_claim_audio(self):
        report = verify(fixture())
        self.assertEqual(len(report['transfers']), 7)
        self.assertFalse(report['native_audio_validated'])
        self.assertFalse(report['arithmetic_ownership_validated'])

    def test_missing_material_wrong_word_and_scope_fail(self):
        text = fixture()
        for before, after in [('value=fff5', 'value=fff4'), ('mask=ffff', 'mask=00ff'),
                              ('ownership=data_bus', 'ownership=Execute_entry'),
                              ('capped=0', 'capped=1'), ('closed=1', 'closed=0'),
                              ('observer_writes=0', 'observer_writes=1'),
                              ('records=42', 'records=41')]:
            with self.subTest(before=before), self.assertRaises(ValueError):
                verify(text.replace(before, after, 1))
        lines = text.splitlines()
        del lines[0]
        lines[-1] = lines[-1].replace('records=42', 'records=41')
        with self.assertRaises(ValueError):
            verify('\n'.join(lines))

    def test_reused_buffer_wrong_channel_fails(self):
        text = fixture().replace('value=ffbd', 'value=fff5').replace('value=0b70', 'value=9b08')
        with self.assertRaises(ValueError):
            verify(text)

    def test_nonchronological_and_late_copy_fail(self):
        text = fixture()
        with self.assertRaises(ValueError):
            verify(text.replace('t=1.019000010', 't=1.022000000', 1))
        with self.assertRaises(ValueError):
            verify(text.replace('t=1.019000050', 't=1.019100050', 1))

    def test_single_word_transfer_interleaving(self):
        lines = fixture().splitlines()
        # Swap the low read/high write identities without changing event times.
        left, right = lines[3].split(' t='), lines[4].split(' t=')
        lines[3] = right[0] + ' t=' + left[1]
        lines[4] = left[0] + ' t=' + right[1]
        self.assertTrue(verify('\n'.join(lines))['raw_copy_chronology_validated'])

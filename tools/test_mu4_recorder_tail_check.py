from pathlib import Path
import unittest

from tools.mu4_recorder_tail_check import pending_tail, source_contract


def fixture():
    packet = bytes.fromhex('fff95080045ffc')+bytes(27)
    captured = packet*2
    plain, carry = captured[:64], captured[64:]
    encoded = bytes(range(64))
    words = lambda data: ','.join(f'{int.from_bytes(data[i:i+2], "big"):04x}' for i in range(0,len(data),2))
    text = ('mu4_recorder_tail_request: ordinal=1 ed=0008 ee=0008 held=0002 '
            'finish=0000,0000 process=0003,e76f '
            f'buffer={words(encoded[48:])} carry={words(carry)} t=10.1 observer_writes=0\n'
            'mu4_recorder_tail_terminal: captures=1 capped=0 ed=0000 ee=0000 held=0002 '
            'illegal=0 t=20.0 observer_writes=0')
    return text, encoded, plain


class RecorderTailTest(unittest.TestCase):
    def test_pending_buffer_and_carry_have_separate_scopes(self):
        report, captured = pending_tail(*fixture(), 48)
        self.assertEqual(report['pending_transformed_bytes'], 16)
        self.assertEqual(report['pending_untransformed_bytes'], 4)
        self.assertEqual(report['captured_packets'], 2)
        self.assertEqual(len(captured), 68)
        self.assertFalse(report['native_flush_validated'])

    def test_rejects_mismatched_or_incomplete_snapshots(self):
        text, encoded, plain = fixture()
        for old,new in [('ee=0008','ee=0007'), ('ed=0008','ed=0009'),
                        ('held=0002','held=0000'), ('captures=1','captures=2'),
                        ('capped=0','capped=1'), ('illegal=0','illegal=1'),
                        ('finish=0000,0000','finish=0003,e76f'),
                        ('t=10.1','t=20.1'), ('observer_writes=0','observer_writes=1')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                pending_tail(text.replace(old,new),encoded,plain,48)
        for payload in (encoded[:-2], bytes(64)):
            with self.assertRaises(ValueError):
                pending_tail(text,payload,plain,48)
        with self.assertRaises(ValueError):
            pending_tail(text,encoded,bytes(64),48)

    def test_rejects_unpinned_source(self):
        with self.assertRaises(ValueError):
            source_contract(b'unrelated source')

    def test_original_source_ranges(self):
        source = Path(__file__).resolve().parents[1]/'roms/5510-mu4-reference/InitData_R060.a00'
        if not source.exists():
            self.skipTest('original research asset not installed')
        self.assertEqual(source_contract(source.read_bytes())['pinned_words'], 307)


if __name__ == '__main__':
    unittest.main()

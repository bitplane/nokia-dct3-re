from pathlib import Path
import unittest

from tools.mu4_recorder_delivery_check import offer_words, source_delivery, verify


def fixture():
    return '\n'.join([
        'mu4_recorder_delivery_post: ordinal=1 length=0005 selector=0021 source=061c words=0200 ea=0001 ed=0300 ee=0300 f0=0001 f3=0000 cycles=10 observer_writes=0',
        'mu4_recorder_delivery_consume: ordinal=1 instruction=038b50 before=0300 amount=0200 after=0100 f0=0001 cycles=20 observer_writes=0',
        'mu4_recorder_delivery_post: ordinal=2 length=0005 selector=0021 source=061c words=0100 ea=0001 ed=0200 ee=0200 f0=0001 f3=0000 cycles=30 observer_writes=0',
        'mu4_recorder_delivery_consume: ordinal=2 instruction=038b87 before=0200 amount=0100 after=0100 f0=0001 cycles=40 observer_writes=0',
        'mu4_recorder_delivery_post: ordinal=3 length=0002 selector=0016 source=0000 ea=0000 ed=0120 ee=0120 f0=0000 f1=0001 cycles=50 observer_writes=0',
        'mu4_recorder_delivery_terminal: posts=3 consumes=2 cap=1000 observer_writes=0',
    ])


class RecorderDeliveryTest(unittest.TestCase):
    def test_thresholds_and_signed_word_domain(self):
        for count in range(0x10000):
            expected = 256 if 256 < count <= 512 else 512 if 512 < count < 0x8000 else 0
            self.assertEqual(offer_words(count),expected)
        for count in (-1,0x10000):
            with self.assertRaises(ValueError):
                offer_words(count)

    def test_complete_offer_acknowledgement_chain(self):
        report = verify(fixture(),persisted_bytes=1536,native_bytes=2112)
        self.assertEqual(report['data_offers'],2)
        self.assertEqual(report['acknowledged_words'],768)
        self.assertFalse(report['file_flush_validated'])
        self.assertFalse(report['per_offer_payload_bytes_validated'])
        self.assertEqual(report['stop_unread_words'],288)
        with self.assertRaises(ValueError):
            verify(fixture(),persisted_bytes=1536,native_bytes=2144)

    def test_rejects_bad_capture_or_lifecycle(self):
        for old,new in [('posts=3','posts=4'), ('cap=1000','cap=3'),
                        ('source=061c','source=061d'), ('ea=0001','ea=0000'),
                        ('f0=0001','f0=0000'), ('f3=0000','f3=0001'),
                        ('amount=0200','amount=0100'), ('after=0100','after=0000'),
                        ('cycles=20','cycles=10'), ('ordinal=1','ordinal='),
                        ('ea=0000','ea=0001'), ('source=0000','source=0001'),
                        ('observer_writes=0','observer_writes=1')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                verify(fixture().replace(old,new),persisted_bytes=1536)
        with self.assertRaises(ValueError):
            verify(fixture(),persisted_bytes=1538)

    def test_original_source(self):
        source = Path(__file__).resolve().parents[1]/'roms/5510-mu4-reference/InitData_R060.a00'
        if not source.exists():
            self.skipTest('original research asset not installed')
        report = source_delivery(source.read_bytes())
        self.assertEqual(report['delivery_pinned_words'],290)
        self.assertEqual(report['buffer_callback']['target'],'03eb4d')


if __name__ == '__main__':
    unittest.main()

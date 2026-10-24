import unittest
from pathlib import Path
import tempfile
from tools.run_noki6210_phone_memory import (
    verify_storage, verify_lifecycle_digests, LIFECYCLE_FRAMES, EMPTY_SHA256)
from tools.run_dct3_phone_memory import run


class PhoneMemoryStorageTest(unittest.TestCase):
    def test_unqualified_product_or_lifecycle_cannot_reuse_6210_acceptance(self):
        with tempfile.TemporaryDirectory() as temporary:
            target=Path(temporary)/'run'
            for product,lifecycle in (('3310',False),('6250',True)):
                with self.subTest(product=product,lifecycle=lifecycle),self.assertRaises(ValueError):
                    run(target,lifecycle,product)
                self.assertFalse(target.exists())

    def test_cold_edit_cancel_and_erase_frames_are_distinct(self):
        digests={(phase,name):digest for phase,name,digest in LIFECYCLE_FRAMES}
        verify_lifecycle_digests(digests)
        # A fixture that accidentally confirms instead of cancelling must fail.
        bad=dict(digests)
        bad['cancelled_cold','6210_phonebook_read_contact']=EMPTY_SHA256
        with self.assertRaises(ValueError):verify_lifecycle_digests(bad)
        for key in digests:
            bad=dict(digests)
            del bad[key]
            with self.subTest(missing=key),self.assertRaises(ValueError):
                verify_lifecycle_digests(bad)

    def setUp(self):
        self.blank=b'\xff'*0x400000
        self.written=self.blank[:-1]+b'\x00'

    def test_changed_handset_storage_with_unchanged_sim(self):
        self.assertEqual(verify_storage([b'card']*3,self.blank,self.written),
                         dict(count=1,first=0x3fffff,last=0x3fffff))

    def test_missing_or_changed_cards_fail(self):
        for cards in ([b'card']*2,[b'']*3,[b'card',b'changed',b'card'],
                      [b'card',b'card',b'changed']):
            with self.subTest(cards=cards),self.assertRaises(ValueError):
                verify_storage(cards,self.blank,self.written)

    def test_unchanged_or_truncated_flash_fails(self):
        for first,second in ((self.blank,self.blank),(b'',b'x'),
                              (self.blank,self.written[:-1])):
            with self.subTest(lengths=(len(first),len(second))),self.assertRaises(ValueError):
                verify_storage([b'card']*3,first,second)


if __name__=='__main__':unittest.main()

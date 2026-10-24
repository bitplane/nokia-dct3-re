import unittest
from pathlib import Path
import tempfile
from tools.run_noki6210_phone_memory import (
    verify_storage, verify_lifecycle_digests, LIFECYCLE_FRAMES, EMPTY_SHA256)
from tools.run_dct3_phone_memory import (run,lifecycle_frames,verify_rename_digests,
    NAME_D_SHA256,RENAMED_CONTACT_SHA256,EDIT_SAVED_SHA256,CONTACT_SHA256)
from tools.run_dct3_phone_memory import (verify_number_digests,SECOND_NUMBER_SHA256,
    NUMBER_ERASE_CONFIRM_SHA256,CONTACT_NAME_SHA256)


class PhoneMemoryStorageTest(unittest.TestCase):
    def test_second_number_must_persist_then_disappear_without_erasing_contact(self):
        reference=dict(before_first=CONTACT_SHA256,before_second=SECOND_NUMBER_SHA256,
            before_third=CONTACT_NAME_SHA256,erase_confirm=NUMBER_ERASE_CONFIRM_SHA256,
            after_first=CONTACT_SHA256,after_second=CONTACT_NAME_SHA256,after_third=CONTACT_SHA256)
        verify_number_digests(reference)
        for key,replacement in (('before_second',CONTACT_SHA256),
                                 ('after_second',SECOND_NUMBER_SHA256),
                                 ('after_first',EMPTY_SHA256),('after_third',SECOND_NUMBER_SHA256)):
            bad=dict(reference)
            bad[key]=replacement
            with self.subTest(key=key),self.assertRaises(ValueError):verify_number_digests(bad)

    def test_rename_requires_changed_name_and_preserved_number_after_cold_boot(self):
        reference=dict(editor=NAME_D_SHA256,saved=EDIT_SAVED_SHA256,cold=RENAMED_CONTACT_SHA256)
        verify_rename_digests(reference)
        for key in reference:
            bad=dict(reference)
            bad[key]=CONTACT_SHA256
            with self.subTest(key=key),self.assertRaises(ValueError):
                verify_rename_digests(bad)
        with self.assertRaises(ValueError):verify_rename_digests(reference,'6250')

    def test_combined_mutation_fixtures_are_rejected_before_run_creation(self):
        with tempfile.TemporaryDirectory() as temporary:
            target=Path(temporary)/'run'
            with self.assertRaises(ValueError):run(target,True,'6210',True)
            self.assertFalse(target.exists())

    def test_unqualified_product_or_lifecycle_cannot_reuse_6210_acceptance(self):
        with tempfile.TemporaryDirectory() as temporary:
            target=Path(temporary)/'run'
            for product,lifecycle in (('3310',False),('8850',True)):
                with self.subTest(product=product,lifecycle=lifecycle),self.assertRaises(ValueError):
                    run(target,lifecycle,product)
                self.assertFalse(target.exists())

    def test_lifecycle_oracles_are_product_specific(self):
        reference={(phase,name):digest for phase,name,digest in lifecycle_frames('6250')}
        verify_lifecycle_digests(reference,'6250')
        with self.assertRaises(ValueError):verify_lifecycle_digests(reference,'6210')
        with self.assertRaises(ValueError):
            verify_lifecycle_digests({(p,n):h for p,n,h in LIFECYCLE_FRAMES},'6250')

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

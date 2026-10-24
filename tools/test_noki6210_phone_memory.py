import unittest
from tools.run_noki6210_phone_memory import verify_storage


class PhoneMemoryStorageTest(unittest.TestCase):
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

import unittest
from tools.noki6250_pin_retry_check import registered_card
from tools.run_noki6250_pin2_cold import check_cold_input


class ColdPin2InputTest(unittest.TestCase):
    def card(self):
        card=bytearray(registered_card())
        card[3492:3500]=b'8765\xff\xff\xff\xff'
        return bytes(card)

    def test_entire_installed_card_is_the_cold_input(self):
        check_cold_input(self.card(),self.card())

    def test_default_or_repaired_card_is_not_a_cold_installation(self):
        with self.assertRaises(ValueError):
            check_cold_input(registered_card(),self.card())
        with self.assertRaises(ValueError):
            check_cold_input(self.card(),registered_card())

    def test_equal_but_unrelated_mutations_are_not_accepted(self):
        for offset in (0,1604,3484,3492,3516,3517,len(self.card())-1):
            changed=bytearray(self.card())
            changed[offset]^=1
            with self.subTest(offset=offset),self.assertRaises(ValueError):
                check_cold_input(bytes(changed),bytes(changed))


if __name__=='__main__':unittest.main()

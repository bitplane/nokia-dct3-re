import unittest
from tools.nsk3_phonebook_check import check_card,expected_record,transmitted
from tools.sim_phonebook_check import CURRENT_NVRAM_LENGTH


class PhonebookTest(unittest.TestCase):
    def test_exact_adn_and_erased_remainder(self):
        data=bytearray(b'\xff'*CURRENT_NVRAM_LENGTH)
        data[:32]=expected_record()
        check_card(data)
        self.assertEqual(data[18:23],bytes.fromhex('04812143f5'))
        for offset in (0,18,20,22,32,1599):
            bad=bytearray(data)
            bad[offset]^=1
            with self.assertRaises(ValueError):check_card(bad)
        with self.assertRaises(ValueError):check_card(data[:-1])

    def test_transmit_stream_requires_order_and_no_cap(self):
        trace='\n'.join(f'nsk3_sim_tx: index={i} value={value:02x}' for i,value in enumerate((0xa0,0xdc,1,4,32),1))
        self.assertEqual(transmitted(trace),bytes.fromhex('a0dc010420'))
        for bad in ('',trace.replace('index=2','index=1'),
                    '\n'.join(f'nsk3_sim_tx: index={i} value=00' for i in range(1,1025))):
            with self.assertRaises(ValueError):transmitted(bad)


if __name__=='__main__':unittest.main()

import unittest
from tools.nsm1_phonebook_check import check_streams
from tools.nsk3_phonebook_check import expected_record,transmitted


def trace(data,status=''):
    return '\n'.join(f'nsm1_sim_tx: index={i} value={value:02x}' for i,value in enumerate(data,1))+status


class PhonebookTest(unittest.TestCase):
    def test_exact_update_and_no_cold_rewrite(self):
        command=bytes.fromhex('a0dc010420')+expected_record()
        read=trace(bytes.fromhex('a0b2010420'))
        write=trace(command,'\nSIM status ins=dc sw=9000')
        check_streams(write,read)
        for bad_write,bad_read in ((write.replace('sw=9000','sw=9402'),read),
                                   (write,write),(trace(command[:-1])+ '\nSIM status ins=dc sw=9000',read)):
            with self.assertRaises(ValueError):check_streams(bad_write,bad_read)
        self.assertEqual(transmitted(read,4096,'nsm1_sim_tx'),bytes.fromhex('a0b2010420'))
        with self.assertRaises(ValueError):transmitted(read,5,'nsm1_sim_tx')


if __name__=='__main__':unittest.main()

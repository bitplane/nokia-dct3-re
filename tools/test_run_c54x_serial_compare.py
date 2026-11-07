import unittest
from tools.run_c54x_serial_compare import check_result,PASS,DONE


class SerialCompareResultTest(unittest.TestCase):
    def test_unique_terminal_success(self):
        text=PASS+'\n'+DONE+'\n'
        check_result(text,3)
        for broken,code in ((text,0),(text+PASS,3),(DONE,3),(PASS,3),
                ('Fatal error: broken\n'+text,3),(text+'later output\n',3)):
            with self.assertRaises(ValueError): check_result(broken,code)


if __name__=='__main__': unittest.main()

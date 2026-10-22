import unittest
from unittest.mock import patch
from tools.dct3_toolkit_check import display_text_events,interactive_events,menu_events
from tools.nsb7_toolkit_check import check


def fixture():
    rows=list(display_text_events('nsb7'))
    rows.insert(rows.index('nsb7_toolkit_physical: action=dismiss')+1,
                'nsb7_key_decode: key=19 observer_writes=0')
    return '\n'.join(rows)


def interactive_fixture(menu=False):
    rows=list((menu_events if menu else interactive_events)('nsb7'))
    rows.insert(rows.index('header cla=a0 ins=14 p1=00 p2=00 p3=10'),
                'nsb7_toolkit_interactive: action=inkey_confirm')
    if menu:
        rows.insert(rows.index('terminal-response data=810304250002028281030100'),
                    'header cla=a0 ins=14 p1=00 p2=00 p3=0c')
        rows.insert(rows.index('nsb7_toolkit_interactive: action=menu_last'),
                    'nsb7_toolkit_interactive: action=menu_skip_netmonitor')
    keys={'dismiss':'19','inkey_5':'05','inkey_confirm':'19',
          'input_4':'04','input_2':'02','confirm':'19','menu':'19',
          'menu_skip_netmonitor':'17','menu_last':'17','menu_open':'19',
          'menu_select':'19','menu_exit':'0f'}
    expanded=[]
    for row in rows:
        expanded.append(row)
        if row.startswith('nsb7_toolkit_interactive: action='):
            expanded.append('nsb7_key_decode: key='+keys[row.split('=')[1]]+' observer_writes=0')
    return '\n'.join(expanded)


class ToolkitTest(unittest.TestCase):
    @patch('tools.nsb7_toolkit_check.check_authentication')
    def test_menu_envelope_and_product_navigation(self,authentication):
        text=interactive_fixture(menu=True)
        check(text,menu=True)
        for bad in (text.replace('d30702020181100101','d30702020181100102'),
                    text.replace('action=menu_skip_netmonitor','action=missing'),
                    text.replace('key=17','key=18'),
                    text+'\nheader cla=a0 ins=c2 p1=00 p2=00 p3=09'):
            with self.subTest(text=bad),self.assertRaises(ValueError):
                check(bad,menu=True)

    @patch('tools.nsb7_toolkit_check.check_authentication')
    def test_interactive_product_specific_confirmation(self,authentication):
        text=interactive_fixture()
        check(text,interactive=True)
        for bad in (text.replace('action=inkey_confirm','action=missing'),
                    text.replace('key=02','key=03'),
                    text.replace('nsb7_key_decode: key=19 observer_writes=0','',1)+
                    '\nnsb7_key_decode: key=19 observer_writes=0',
                    text.replace('0d03043432','0d03043433')):
            with self.subTest(text=bad),self.assertRaises(ValueError):
                check(bad,interactive=True)

    @patch('tools.nsb7_toolkit_check.check_authentication')
    def test_successful_physical_dismissal(self,authentication):
        text=fixture()
        check(text)
        authentication.assert_called_once_with(text.split('nsb7_toolkit_physical:',1)[0])

    @patch('tools.nsb7_toolkit_check.check_authentication')
    def test_timeout_wrong_key_and_missing_negotiation_rejected(self,authentication):
        for bad in (fixture().replace('810301218002028281030100','810301218002028281030112'),
                    fixture().replace('key=19','key=17'),
                    fixture().replace('sw=9116','sw=9000'),
                    fixture().replace('ins=10','ins=11'),
                    fixture()+'\nheader cla=a0 ins=12 p1=00 p2=00 p3=16'):
            with self.subTest(text=bad),self.assertRaises(ValueError):
                check(bad)

    @patch('tools.nsb7_toolkit_check.check_authentication')
    def test_response_before_physical_input_rejected(self,authentication):
        with self.assertRaises(ValueError):
            check(fixture().replace('nsb7_toolkit_physical: action=dismiss','')+
                  '\nnsb7_toolkit_physical: action=dismiss')

    def test_authentication_is_not_optional(self):
        with self.assertRaises(ValueError):
            check(fixture())


if __name__=='__main__':
    unittest.main()

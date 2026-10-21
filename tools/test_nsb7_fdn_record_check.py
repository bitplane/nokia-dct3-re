"""FDN record target, authorization, rejection and whole-card checks."""
import unittest
from unittest.mock import patch
from tools.nsb7_fdn_record_check import check_trace, expected_card, edited_record
from tools.nsb7_pin_check import serialized_profile
from tools.sim_fdn_profile import extend
from tools.nsk3_phonebook_check import expected_record


def fixture(mode):
    codes = [0x19,4,6,4,0x19,0x19,0x18,0x18,0x19]
    if mode in ('cold','cancel','erase','empty','edit','edited-cold'):
        codes += [0x19]*(2 if mode == 'empty' else 3)
        if mode in ('cancel','erase'):
            codes += [0x19,5,6,7,8,0x19,0x18,0x19,0x1a if mode == 'cancel' else 0x19]
        elif mode == 'edit':
            codes += [0x19,5,6,7,8,0x19,0x19,0x19,0x1a,6,0x19]
    else:
        codes += [0x18,0x19,5,6,7,5 if mode == 'wrong' else 8,0x19]
    if mode == 'write':
        codes += [2,0x19,1,2,3,4,5,0x19]
    names = {0x19:'Left Softkey / Menu',0x18:'Scroll Down',0x1a:'Right Softkey / C'}
    rows = []
    for code in codes:
        name = names.get(code,'Keypad '+str(code))
        rows += [f'nsb7_menu_input: key={name} state=1 t=26',
                 f'nsb7_key_decode: key={code:02x} observer_writes=0',
                 f'nsb7_menu_input: key={name} state=0 t=26.2']
    tx = bytes.fromhex('a020000108')+b'1234\xff\xff\xff\xff'
    rows += ['SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1']
    for fid in ('6f7e','6f07'):
        tx += bytes.fromhex('a0a4000002'+fid+'a044000000')
        rows += ['SIM status ins=44 sw=9000 chv=3/3 puk=10/10 enabled=1']
    tx += bytes.fromhex('a0a40000026f3b')
    if mode not in ('cold','empty','edited-cold'):
        tx += bytes.fromhex('a020000208')+(b'5675' if mode == 'wrong' else b'5678')+b'\xff'*4
        rows += [f'SIM status ins=20 sw={"9804" if mode == "wrong" else "9000"} chv=3/{2 if mode == "wrong" else 3} puk=10/10 enabled=1']
    if mode in ('write','erase','edit'):
        body = edited_record() if mode == 'edit' else expected_record() if mode == 'write' else b'\xff'*32
        tx += bytes.fromhex('a0dc010420')+body
        rows += ['SIM status ins=dc sw=9000 chv=3/3 puk=10/10 enabled=1']
    rows += [f'nsb7_sim_tx: index={i} value={value:02x}' for i,value in enumerate(tx,1)]
    return '\n'.join(rows)


class FdnRecordTest(unittest.TestCase):
    @patch('tools.nsb7_fdn_record_check.check_phone')
    @patch('tools.nsb7_fdn_record_check.check_pin')
    def test_modes_and_wrong_target(self, pin, phone):
        for mode in ('write','cold','wrong','cancel','erase','empty','edit','edited-cold'):
            check_trace(fixture(mode),mode)
        text = fixture('write').replace('value=3b','value=3a')
        with self.assertRaises(ValueError):
            check_trace(text,'write')

    @patch('tools.nsb7_fdn_record_check.check_phone')
    @patch('tools.nsb7_fdn_record_check.check_pin')
    def test_rejected_authorization_cannot_be_successful(self, pin, phone):
        with self.assertRaises(ValueError):
            check_trace(fixture('wrong').replace('sw=9804','sw=9000'),'wrong')

    @patch('tools.nsb7_fdn_record_check.check_phone')
    @patch('tools.nsb7_fdn_record_check.check_pin')
    def test_cancel_cannot_erase(self, pin, phone):
        text = fixture('erase')
        last = text.rfind('nsb7_menu_input: key=Left Softkey / Menu state=1')
        text = text[:last]+text[last:].replace('key=Left Softkey / Menu', 'key=Right Softkey / C').replace(
            'key=19 observer_writes=0','key=1a observer_writes=0')
        with self.assertRaises(ValueError):
            check_trace(text,'cancel')

    @patch('tools.nsb7_fdn_record_check.check_phone')
    @patch('tools.nsb7_fdn_record_check.check_pin')
    def test_edit_requires_the_changed_digit(self, pin, phone):
        text = fixture('edit').replace('value=f6','value=f5')
        with self.assertRaises(ValueError):
            check_trace(text,'edit')
        self.assertEqual([i for i,(a,b) in enumerate(zip(expected_record(),edited_record())) if a!=b], [22])

    def test_complete_card_change(self):
        before = bytearray(extend(serialized_profile()))
        before[3575] = 0
        saved = expected_card(bytes(before))
        self.assertEqual(saved[:3578], before[:3578])
        self.assertEqual(saved[3578:3610], expected_record())
        rejected = expected_card(bytes(before),True)
        self.assertEqual([i for i,(a,b) in enumerate(zip(before,rejected)) if a!=b], [3517])
        with self.assertRaises(ValueError):
            expected_card(saved)

    def test_real_authentication_required(self):
        with self.assertRaises(ValueError):
            check_trace(fixture('write'),'write')


if __name__ == '__main__':
    unittest.main()

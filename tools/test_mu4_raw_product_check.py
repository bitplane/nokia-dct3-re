import unittest
from unittest.mock import patch

from tools.mu4_context_contract import RAW_PRODUCT_BASE, RAW_PRODUCT_WITNESS, reference_raw_product
from tools.mu4_raw_product_check import verify


def fixture():
    lines=[]
    for channel,x in enumerate((0x069e0c00,0x2aa01960)):
        time=1.019+channel*0.002
        expected=reference_raw_product(x,0,0x003243e1,0x1fffd885)
        state={'a':0,'b':256}
        for address in range(RAW_PRODUCT_BASE,0x25be0):
            state.update(expected.get(address,{}))
            t=time+(address-RAW_PRODUCT_BASE)*0.00000001
            lines.append(f'mu4_raw_execute: address={address:06x} opcode={RAW_PRODUCT_WITNESS[address-RAW_PRODUCT_BASE]:04x} '
                         f'a={state["a"]:010x} b={state["b"]:010x} st1=6100 t={t:.9f} '
                         'identity=Execute_entry context_only=1 observer_writes=0')
        base=(0x7880,0xe880)[channel]
        for address,value in ((base,x),(base-2,0),(0xf000,0x003243e1),(0xf0fe,0x1fffd885)):
            for offset,word in enumerate((value>>16,value&0xffff)):
                lines.append(f'mu4_product_read: address={address+offset:04x} value={word:04x} mask=ffff '
                             f't={time:.9f} ownership=data_bus observer_writes=0')
    lines.append('mu4_product_terminal: reads=16 closed=1 capped=0 observer_writes=0')
    return '\n'.join(lines)


class RawProductCheckTests(unittest.TestCase):
    def check(self,text):
        copies={'transfers':[{'words':'fff59b08','read_time':1.020},{},{},
                             {'words':'ffbd0b70','read_time':1.022}]}
        with patch('tools.mu4_raw_product_check.contract',return_value={'raw_product':{}}), \
             patch('tools.mu4_raw_product_check.verify_copies',return_value=copies):
            return verify(text,b'source')

    def test_selected_intermediates_only(self):
        report=self.check(fixture())
        self.assertTrue(report['selected_partial_product_arithmetic_validated'])
        self.assertFalse(report['native_audio_validated'])
        self.assertEqual([r['result'] for r in report['iterations']],['fff59b08','ffbd0b70'])

    def test_changed_arithmetic_identity_scope_and_material_fail(self):
        text=fixture()
        for before,after in [('a=0001c12ade','a=0001c12adf'),('opcode=a669','opcode=a668'),
                             ('st1=6100','st1=6140'),('identity=Execute_entry','identity=live_pc'),
                             ('mask=ffff','mask=00ff'),('value=43e1','value=43e0'),
                             ('reads=16','reads=15'),('closed=1','closed=0'),('capped=0','capped=1')]:
            self.assertIn(before,text)
            with self.subTest(before=before),self.assertRaises(ValueError):
                self.check(text.replace(before,after,1))

    def test_missing_instruction_and_operand_fail(self):
        text=fixture()
        for marker in ('address=025bd4','address=7881'):
            modified='\n'.join(line for line in text.splitlines() if marker not in line)
            if marker=='address=7881':
                modified=modified.replace('reads=16','reads=15')
            with self.subTest(marker=marker),self.assertRaises(ValueError):
                self.check(modified)

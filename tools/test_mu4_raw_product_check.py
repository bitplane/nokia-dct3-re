import unittest
from unittest.mock import patch

from tools.mu4_context_contract import RAW_PRODUCT_BASE, RAW_PRODUCT_WITNESS, reference_raw_product
from tools.mu4_raw_product_check import verify, verify_upstream, verify_butterfly
from tools.mu4_context_contract import RAW_UPSTREAM_WITNESSES, reference_raw_complex_prefix, reference_raw_scale, reference_raw_butterfly


def butterfly_fixture(only_channel=None):
    lines=[]
    for channel,inputs in enumerate(((0x0185edda,0x2db9ba35,0x0001432d,0xfffc8948),
                                     (0xf4b3b77f,0xd656547d,0x001a5cc7,0xfff8fb7d))):
        if only_channel is not None and channel!=only_channel:
            continue
        base=0x7800+channel*0x7000
        other=base+0x80
        time=1.0188+channel*0.002
        expected,outputs=reference_raw_butterfly(*inputs)
        state={'a':0,'b':0}
        addresses=[0x2419d,0x2419f,0x241a0,0x241a1,0x241a2,0x241a3,0x241a5,0x241a6,
                   0x241a7,0x241a8,0x241aa,0x241ac,0x241ad,0x241af,0x241b1,0x241b2,
                   0x241b3,0x241b5,0x241b7,0x241b8,0x241ba,0x241bc]
        for i,address in enumerate(addresses):
            state.update(expected.get(address,{}))
            lines.append(f'mu4_upstream_execute: address={address:06x} opcode={RAW_UPSTREAM_WITNESSES[0x2419d][address-0x2419d]:04x} '
                         f'a={state["a"]:010x} b={state["b"]:010x} ar2={base:04x} ar3={other:04x} '
                         f'st1=611f t={time+i*0.00000001:.9f} identity=Execute_entry context_only=1 observer_writes=0')
        for address,source,result in zip((base,other,base+2,other+2),inputs,outputs):
            for offset in (0,1):
                word=(source>>(16*(1-offset)))&0xffff
                out=(result>>(16*(1-offset)))&0xffff
                lines.append(f'mu4_upstream_read: address={address+offset:04x} value={word:04x} mask=ffff '
                             f't={time:.9f} ownership=data_bus observer_writes=0')
                lines.append(f'mu4_raw_writer: address={address+offset:04x} value={out:04x} mask=ffff '
                             f't={time+0.00000020:.9f} ownership=data_bus observer_writes=0')
    return '\n'.join(lines)


class ButterflyCheckTests(unittest.TestCase):
    def test_all_four_output_pairs(self):
        report=verify_butterfly(butterfly_fixture())
        self.assertEqual(report[0]['outputs'],['179fd407','e9e619d2','fffee63a','00025cf2'])
        self.assertEqual(report[1]['outputs'],['e58505fe','0f2eb181','0009ac22','0010b0a5'])

    def test_intermediate_output_operand_and_instruction_mutations(self):
        text=butterfly_fixture()
        for before,after in [('b=000185edda','b=000185eddb'),('value=e9e6','value=e9e7'),
                             ('value=2db9','value=2db8'),('opcode=5583','opcode=5483'),
                             ('identity=Execute_entry','identity=live_pc'),('mask=ffff','mask=00ff')]:
            self.assertIn(before,text)
            with self.subTest(before=before),self.assertRaises(ValueError):
                verify_butterfly(text.replace(before,after,1))


def upstream_fixture():
    lines=[]
    products=[{'input':'069e0c00'},{'input':'2aa01960'}]
    for channel,(x,y,arg) in enumerate(((0xe9e619d2,0x00025cf2,10),(0x0f2eb181,0x0010b0a5,12))):
        lines.extend(butterfly_fixture(channel).splitlines())
        base=(0x7880,0xe880)[channel]
        time=1.019+channel*0.002
        expected,raw=reference_raw_complex_prefix(x,y,0x168ed1ea,0x16b25ced)
        state={'a':0,'b':0}
        def row(address,t,st1):
            origin=0x23f8a if address<0x24000 else 0x25ce2
            lines.append(f'mu4_upstream_execute: address={address:06x} opcode={RAW_UPSTREAM_WITNESSES[origin][address-origin]:04x} '
                         f'a={state["a"]:010x} b={state["b"]:010x} ar2={base:04x} ar4=f240 ar5=f2c1 '
                         f'sp=d6fe st1={st1} t={t:.9f} identity=Execute_entry context_only=1 observer_writes=0')
        for i,address in enumerate(list(range(0x23f8a,0x23f98))+[0x23f99]):
            state.update(expected.get(address,{}))
            row(address,time+i*0.00000001,'e11f')
        for address,value in ((base,x),(base+2,y),(0xf240,0x168ed1ea),(0xf2c0,0x16b25ced)):
            for offset,word in enumerate((value>>16,value&0xffff)):
                lines.append(f'mu4_upstream_read: address={address+offset:04x} value={word:04x} mask=ffff '
                             f't={time:.9f} ownership=data_bus observer_writes=0')
        initial=raw | (0xff00000000 if raw & 0x80000000 else 0)
        delta,final=reference_raw_scale(initial,arg,14)
        time+=0.00001
        state['a']=initial
        row(0x25ce2,time,'6100')
        row(0x25cea,time+0.00000001,'6100')
        for i in range(-delta):
            row(0x25d07,time+(i+2)*0.00000001,'6100')
            state['a']=(state['a']<<1)&((1<<40)-1)
        row(0x25d0e,time+0.00000010,'6100')
        for address,value in ((0xd703,arg),(0xd704,14)):
            lines.append(f'mu4_upstream_read: address={address:04x} value={value:04x} mask=ffff '
                         f't={time:.9f} ownership=data_bus observer_writes=0')
    lines.append('mu4_upstream_terminal: reads=36 closed=1 capped=0 observer_writes=0')
    return '\n'.join(lines),products


class RawUpstreamCheckTests(unittest.TestCase):
    def test_complex_and_scale_chain(self):
        text,products=upstream_fixture()
        report=verify_upstream(text,products)
        self.assertEqual([r['left_shifts'] for r in report],[4,2])
        self.assertEqual([r['complex_result'] for r in report],['f069e0c0','0aa80658'])

    def test_changed_inputs_opcodes_scope_and_repeat_fail(self):
        text,products=upstream_fixture()
        for before,after in [('value=d1ea','value=d1eb'),('value=000a','value=000b'),
                              ('opcode=a668','opcode=a669'),('st1=e11f','st1=e15f'),
                              ('st1=6100','st1=6180'),('mask=ffff','mask=00ff'),
                              ('identity=Execute_entry','identity=live_pc'),('reads=36','reads=35')]:
            self.assertIn(before,text)
            with self.subTest(before=before),self.assertRaises(ValueError):
                verify_upstream(text.replace(before,after,1),products)
        lines=text.splitlines()
        index=next(i for i,line in enumerate(lines) if 'address=025d07' in line)
        del lines[index]
        with self.assertRaises(ValueError):
            verify_upstream('\n'.join(lines),products)


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
             patch('tools.mu4_raw_product_check.verify_copies',return_value=copies), \
             patch('tools.mu4_raw_product_check.verify_upstream',return_value=[]):
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

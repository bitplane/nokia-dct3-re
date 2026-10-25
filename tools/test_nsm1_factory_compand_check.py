import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

from tools import nsm1_factory_compand_check as checker


def trace():
    rows=['nsm1_tone_boundary_compiler: field=00000028 value=00000dca commit=00000000 caller=002a2c29 t=0.290352769 observer_writes=0',
          'nsm1_compand_window: frequency=1b58 flags=0dca xpc=0000 t=86.140000000 observer_writes=0',
          'nsm1_compand_summary: input=80 result=80 serial=80 retained=32 observer_writes=0']
    for i in range(32):
        before,result=((3019,0xff92),(15099,0xffb8))[i%2]
        for j,(address,pc,value) in enumerate(((0x9435,0x9436,before),(0x3587,0x3588,result),(0x358a,0x358b,result))):
            rows.append(f'nsm1_compand_fetch: address={address:04x} index={i+1} pc={pc:08x} a={value:010x} b=0000000000 t={86.14005+i*.000125+j*.000001:.9f} observer_writes=0')
    return '\n'.join(rows)


def images():
    program,data=bytearray(0x1fffe),bytearray(0x20000)
    for address,raw in ((0x9435,checker.ENCODER),(0x24d5,'f0739435'),
                       (0x357f,'61f81f2b0040f82035894a16f07424d58a16f463f47d8821'),
                       (0xa49a,checker.FORMAT_SETUP),(0xa53b,'7714085e71941f2b')):
        raw=bytes.fromhex(raw)
        program[2*address:2*address+len(raw)]=raw
    data[2*0xb3d7:2*0xb3d9]=bytes.fromhex('ffc50045')
    return program,data


class FactoryCompandTest(unittest.TestCase):
    def test_reference_uses_unscaled_signed16_input_and_explicit_codec(self):
        with patch.object(checker.subprocess,'run',return_value=SimpleNamespace(stdout=bytes((0x92,0xb8)))) as run:
            self.assertEqual(checker.reference([3019,15099],Path('/reference/ffmpeg')),bytes((0x92,0xb8)))
        args,kwargs=run.call_args
        self.assertEqual(kwargs['input'],bytes.fromhex('cb0bfb3a'))
        self.assertEqual(args[0][args[0].index('-c:a')+1],'pcm_alaw')
        self.assertEqual(args[0][args[0].index('-f')+1],'s16le')
        with patch.object(checker.subprocess,'run',return_value=SimpleNamespace(stdout=b'')),self.assertRaises(ValueError):
            checker.reference([3019],Path('/reference/ffmpeg'))

    def test_bounded_native_input_result_and_serial_match(self):
        inputs,outputs=checker.check_trace(trace())
        self.assertEqual(inputs,[3019,15099]*16)
        self.assertEqual(outputs,bytes((0x92,0xb8))*16)
        checker.contract(*images())

    def test_wrong_domain_serial_value_missing_rows_and_bad_context_fail(self):
        good=trace()
        for bad in (good.replace('flags=0dca','flags=0d8a'),
                    good.replace('value=00000dca','value=00000d8a'),
                    good.replace('input=80','input=0'),
                    good.replace('a=0000000bcb','a=0000000bcc'),
                    good.replace('address=358a','address=3589'),
                    good.replace('pc=00009436','pc=00009435'),
                    good.replace('t=86.140050000','t=86.160050000'),
                    good+'\nnsm1_compand_fetch: malformed'):
            with self.subTest(tail=bad[-80:]),self.assertRaises(ValueError):
                checker.check_trace(bad)

    def test_code_hook_and_sign_masks_are_pinned(self):
        for which,address in ((0,0x9435),(0,0x24d5),(0,0x3585),(1,0xb3d7),(0,0xa49a),(0,0xa53b)):
            values=images()
            values[which][2*address]^=1
            with self.subTest(address=address),self.assertRaises(ValueError):
                checker.contract(*values)


if __name__=='__main__':
    unittest.main()

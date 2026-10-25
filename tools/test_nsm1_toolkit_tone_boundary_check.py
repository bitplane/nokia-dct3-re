import hashlib
import struct
import unittest
from unittest.mock import patch

from tools import nsm1_toolkit_tone_boundary_check as checker


def trace():
    early='\n'.join(f'nsm1_tone_boundary_early: address=00010000 value=00000001 mask=ffff0000 t=0.{i+1} observer_writes=0' for i in range(8))
    compiler='\n'.join(f'nsm1_tone_boundary_compiler: field={field:08x} value=00000002 commit=00000001 caller=002a2bf5 t=0.{i+1} observer_writes=0' for i,field in enumerate((8,9,0x28,0x2d,0x2e,0x2d,0x2e)))
    io='\n'.join(f'nsm1_tone_boundary_io: address=0021 value={value} mask=ffff pc=47a1 t=87.000000000 observer_writes=0'
                 for _ in range(1408) for value in ('0e82','0682'))
    return '\n'.join((early,compiler,io,'nsm1_tone_boundary_summary: start=85 end=91.5 mcu_early=70514 mcu=0 dsp=0 io=2816 compiler_early=7 compiler=0 observer_writes=0'))


def image():
    data=bytearray(0xa5000)
    for field in range(47):
        struct.pack_into('>I',data,0xa4aa0+4*field,0x2a4d16)
    for field,target,pool,value in (
            (0x20,0x2a4bc6,0x2a4f0c,0x100ae),(0x21,0x2a4bc2,0x2a4f08,0x100b0),
            (0x22,0x2a4bbe,0x2a4f04,0x100b2),(0x23,0x2a4bb6,0x2a4f00,0x100b4),
            (0x24,0x2a4bb2,0x2a4efc,0x100b6),(0x25,0x2a4bae,0x2a4ef8,0x100ac)):
        struct.pack_into('>I',data,0xa4aa0+4*field,target)
        struct.pack_into('>H',data,target-0x200000,0x4a00|((pool-((target+4)&~3))//4))
        struct.pack_into('>I',data,pool-0x200000,value)
    struct.pack_into('>H',data,0xa4a80,0x4800|((0x2a4d04-0x2a4a84)//4))
    struct.pack_into('>I',data,0xa4d04,0x10000)
    return data


class ToneBoundaryTest(unittest.TestCase):
    def test_scoped_absence_needs_both_early_positives_and_late_native_activity(self):
        good=trace()
        checker.check_trace(good)
        for bad in (good.replace('mcu_early=70514','mcu_early=0'),
                    good.replace('compiler_early=7','compiler_early=0'),
                    good.replace('mcu=0','mcu=1'),good.replace('dsp=0','dsp=1'),
                    good.replace('compiler=0','compiler=1'),good.replace('value=0682','value=0000'),
                    good.replace('address=00010000','address=00100000'),
                    good.replace('field=00000028','field=00000020'),
                    good.replace('t=87.000000000','t=92.000000000'),good.split('\n',1)[1],
                    good+'\nnsm1_tone_boundary_mcu: unexpected',
                    good+'\nnsm1_tone_boundary_io: malformed'):
            with self.subTest(tail=bad[-80:]),self.assertRaises(ValueError):
                checker.check_trace(bad)

    def test_amplitude_is_field24_not_field25_and_candidates_are_not_closed_callers(self):
        data=image()
        callers=[0x2404f2,0x240e4c,0x240e56,0x2a2bf0,0x2a2c24,0x2a2c4c,0x2a4d5a]
        with patch.object(checker,'SHA1',hashlib.sha1(data).hexdigest()),patch.object(checker,'direct_call_candidates',return_value=callers):
            result=checker.contract(data)
            self.assertEqual(result['fields'][0x24],0x100b6)
            self.assertEqual(result['fields'][0x25],0x100ac)
            self.assertFalse(result['indirect_callers_closed'])
            with self.assertRaises(ValueError):
                checker.contract(data[:-1])
        struct.pack_into('>I',data,0xa4efc,0x100ac)
        with patch.object(checker,'SHA1',hashlib.sha1(data).hexdigest()),patch.object(checker,'direct_call_candidates',return_value=callers):
            with self.assertRaises(ValueError):
                checker.contract(data)


if __name__=='__main__':
    unittest.main()

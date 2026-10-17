from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch

from tools.mu4_overlap_check import TARGETS, exchange, verify
from tools.mu4_transform_check import fnv


def fixture(directory):
    words={a:0x7777 for a in (0x24a60,0x24a61,0x24a63,0x24a64,0x24af8,0x24b13,0x24b3e,0x24b59,0x24955,0x2496e)}
    lines=[]
    for ordinal,mode in enumerate(range(4),1):
        size=128 if mode==2 else 1024
        count=size//2
        values=[(i*1234567)%0x100000000-0x80000000 for i in range(size)]
        history=[(i*7919)&0xffff for i in range(count)]
        output,saved=exchange(values,history,mode)
        lines.extend((f'mu4_scale_snapshot: invocation={ordinal} stage=1 t={ordinal+0.1} observer_writes=0',
                      f'mu4_window_snapshot: invocation={ordinal} stage=0 t={ordinal+0.9} observer_writes=0'))
        for site,target in enumerate(TARGETS[mode]):
            for phase,name in enumerate(('call','entry','return')):
                address=(0x24a60 if site==0 else 0x24a63) if phase==0 else target if phase==1 else (0x24a61 if site==0 else 0x24a64)
                time=ordinal+0.2+site*0.3+phase*0.01
                lines.append(f'mu4_window_callback: invocation={ordinal} call={ordinal} site={site} phase={name} '
                             f'address={address:06x} opcode=7777 target={target:06x} mode={mode:04x} '
                             f'base=7000 history=a000 parameter={size:04x} t={time:.3f} '
                             'identity=Execute_entry context_only=1 observer_writes=0 audio_acceptance=0')
        for stage,data in enumerate((values,history,output,saved)):
            blob=struct.pack(f'>{len(data)}'+('H' if stage%2 else 'i'),*data)
            name=f'mu4_history_{ordinal:03d}_stage{stage}.be16'
            (directory/name).write_bytes(blob)
            address=0x24a60 if stage<2 else 0x24a61
            time=ordinal+0.2+(0.02 if stage>=2 else 0)
            lines.append(f'mu4_history_snapshot: invocation={ordinal} call={ordinal} stage={stage} '
                         f'address={address:06x} opcode=7777 base={0xa000 if stage%2 else 0x7000:04x} '
                         f'words={len(blob)//2} parameter={size:04x} hash={fnv(blob):08x} file={name} '
                         f't={time:.3f} identity=Execute_entry context_only=1 observer_writes=0 audio_acceptance=0')
    (directory/'error.log').write_text('\n'.join(lines))
    return words


class OverlapTests(unittest.TestCase):
    def test_exchange_rounding_bounds_reversal_and_untouched_regions(self):
        for mode in range(4):
            size=128 if mode==2 else 1024
            count=size//2
            values=([-0x80000000,-32769,-32768,-1,0,32767,32768,0x7fffffff]*(size//8))
            history=[((i*7919)^0x8000)&0xffff for i in range(count)]
            signed=lambda word:struct.unpack('>h',struct.pack('>H',word))[0]
            raw,saved=exchange(values,history,mode)
            for i,value in enumerate(values[:count]):
                expected=((value//32768+1)//2)&0xffff
                self.assertEqual(saved[count-1-i],expected)
                self.assertEqual(raw[i],signed(history[i if mode==3 else count-1-i])<<16)
            if mode==3:
                self.assertEqual(raw[896:960],[signed(x)<<16 for x in history[-1:-65:-1]])
                self.assertEqual(raw[512:896],values[512:896])
                self.assertEqual(raw[960:],values[960:])
            else:
                self.assertEqual(raw[count:],values[count:])
        with self.assertRaises(ValueError):
            exchange([0],[0],0)

    def test_dispatch_entry_return_and_rehashed_buffer_corruption(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory=Path(temporary)
            words=fixture(directory)
            prerequisite=lambda *_:{'windows':[{'call':i} for i in range(1,5)],'source_words_pinned':1042,
                                     'window_products_validated':True,'transform_chain_validated':True,'scaling_validated':True,'native_audio_validated':False}
            with patch('tools.mu4_overlap_check.verify_scale',side_effect=prerequisite),patch('tools.mu4_overlap_check.source_contract',return_value=words),patch('tools.mu4_overlap_check.pin_block'):
                result=verify(directory,b'source')
                self.assertTrue(result['history_exchange_validated'])
                self.assertEqual(result['source_words_pinned'],1347)
                self.assertFalse(result['native_audio_validated'])
                text=(directory/'error.log').read_text()
                for before,after in [('target=024af8','target=024b13'),('phase=entry','phase=call'),
                                     ('mode=0000','mode=0001'),('identity=Execute_entry','identity=live_pc')]:
                    (directory/'error.log').write_text(text.replace(before,after,1))
                    with self.subTest(before=before),self.assertRaises(ValueError):
                        verify(directory,b'source')
                path=directory/'mu4_history_001_stage3.be16'
                blob=path.read_bytes()
                changed=bytes([blob[0]^1])+blob[1:]
                path.write_bytes(changed)
                (directory/'error.log').write_text(text)
                with self.assertRaises(ValueError):
                    verify(directory,b'source')
                (directory/'error.log').write_text(text.replace(f'hash={fnv(blob):08x} file={path.name}',f'hash={fnv(changed):08x} file={path.name}'))
                self.assertFalse(verify(directory,b'source')['history_exchange_validated'])

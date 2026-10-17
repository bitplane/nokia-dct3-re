from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch

from tools.mu4_overlap_check import TARGETS, exchange, verify, final_tail, history_continuity, verify_tail, conversion_input_binding
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
    def test_conversion_input_binding_accepts_zero_and_sign_extension(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory=Path(temporary)
            for i in range(1,17):
                (directory/f'mu4_tail_{i:03d}_stage3.be16').write_bytes(struct.pack('>1024i',*([-1]*1024)))
            lines=[]
            for ordinal in range(16384):
                frame,within=divmod(ordinal,2048)
                sample,channel=divmod(within,2)
                raw=0xffffffffff if ordinal%2 else 0x00ffffffff
                lines.append(f'mu4_conversion_value: call={frame+1} sample={sample} channel={channel} raw={raw:010x} '
                             'identity=Execute_entry observer_writes=0')
            text='\n'.join(lines)
            report=conversion_input_binding(directory,text)
            self.assertTrue(report['conversion_input_binding_validated'])
            self.assertEqual((report['components'],report['blocks']),(16384,8))
            self.assertEqual(conversion_input_binding(directory,'\n'.join(lines[:4096]))['blocks'],2)
            self.assertFalse(conversion_input_binding(directory,text.replace('raw=00ffffffff','raw=00fffffffe',1))['conversion_input_binding_validated'])
            self.assertFalse(conversion_input_binding(directory,'raw=fffffffffe'.join(text.rsplit('raw=ffffffffff',1)))['conversion_input_binding_validated'])
            with self.assertRaises(ValueError):
                conversion_input_binding(directory,text.replace('raw=00ffffffff','raw=01ffffffff',1))

    def test_final_tail_against_ordered_buffer_moves(self):
        for mode in range(4):
            raw=[i*12345-1000000 for i in range(1024)]
            history=[(i*7919)&0xffff for i in range(512)]
            saved=[-i*54321 for i in range(128)]
            r,h,s=raw[:],history[:],saved[:]
            if mode==2:
                s[:]=r[:128]
                for i in range(448):
                    word=h[i]
                    r[i]=(word-65536 if word&32768 else word)<<16
                    h[i]=((r[i+576]//32768+1)//2)&0xffff
                    r[i+576]=r[i+128]
                r[448:576]=s
            elif mode==3:
                for i in range(128):
                    r[896+i],s[i]=s[i],r[896+i]
                for i in reversed(range(448)):
                    r[576+i]=r[512+i]
                for i in range(224):
                    r[576+i],r[1023-i]=r[1023-i],r[576+i]
                r[448:576]=s
            self.assertEqual(final_tail(raw,history,saved,mode),(r,h,s))
        with self.assertRaises(ValueError):
            final_tail([],[],[],0)

    def test_observed_history_continuity_detects_intervening_change(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory=Path(temporary)
            lines=[]
            for ordinal,(marker,stage,values) in enumerate((('mu4_history',1,[0,1]),('mu4_history',3,[2,3]),
                    ('mu4_tail',1,[2,3]),('mu4_tail',4,[4,5]),('mu4_history',1,[4,5]))):
                name=f'event{ordinal}.be16'
                (directory/name).write_bytes(struct.pack('>2H',*values))
                lines.append(f'{marker}_snapshot: stage={stage} file={name} base=a000 t={ordinal} observer_writes=0')
            report=history_continuity(directory,'\n'.join(lines))
            self.assertEqual((report['initial_reads'],report['known_reads'],report['mismatched_words']),(2,4,0))
            self.assertTrue(report['history_continuity_validated'])
            (directory/'event4.be16').write_bytes(struct.pack('>2H',4,6))
            report=history_continuity(directory,'\n'.join(lines))
            self.assertEqual(report['mismatched_words'],1)
            self.assertFalse(report['history_continuity_validated'])

    def test_full_tail_capture_and_rehashed_wrong_output(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory=Path(temporary)
            lines=[]
            callbacks=[]
            ordinal=0
            for call,mode in enumerate(range(4),1):
                history=[(i*7919)&0xffff for i in range(512)]
                for channel in range(2):
                    ordinal+=1
                    callbacks.append({'invocation':ordinal,'call':call,'mode':mode})
                    start=call+0.2+channel*0.4
                    lines.append(f'mu4_window_snapshot: invocation={ordinal} stage=2 t={start-0.1} observer_writes=0')
                    lines.append(f'mu4_tail_arguments: tail={ordinal} invocation={ordinal} call={call} mode={mode:04x} '
                                 f'base=7000 history={0xa000+call*512:04x} saved=c000 t={start:.3f} '
                                 'identity=Execute_entry context_only=1 observer_writes=0 audio_acceptance=0')
                    raw=[i*12345-1000000 for i in range(1024)]
                    saved=[-i*54321 for i in range(128)]
                    output=final_tail(raw,history,saved,mode)
                    for stage,data in enumerate((raw,history,saved,*output)):
                        name=f'mu4_tail_{ordinal:03d}_stage{stage}.be16'
                        kind=stage%3
                        blob=struct.pack(f'>{len(data)}'+('H' if kind==1 else 'i'),*data)
                        (directory/name).write_bytes(blob)
                        base=(0x7000,0xa000+call*512,0xc000)[kind]
                        address=0x24a7e if stage<3 else 0x24af3
                        lines.append(f'mu4_tail_snapshot: invocation={ordinal} call={call} stage={stage} '
                                     f'address={address:06x} opcode=7777 base={base:04x} words={len(blob)//2} '
                                     f'parameter=0400 file={name} hash={fnv(blob):08x} t={start+(0.1 if stage>=3 else 0):.3f} '
                                     'identity=Execute_entry context_only=1 observer_writes=0 audio_acceptance=0')
                    history=output[1]
                lines.append(f'mu4_processing_return: call={call} t={call+0.99} observer_writes=0')
            text='\n'.join(lines)
            (directory/'error.log').write_text(text)
            prerequisite=lambda *_:{'callbacks':callbacks,'source_words_pinned':1347}
            with patch('tools.mu4_overlap_check.verify',side_effect=prerequisite),patch('tools.mu4_overlap_check.source_contract',return_value={0x24a7e:0x7777,0x24af3:0x7777}),patch('tools.mu4_overlap_check.pin_block'):
                report=verify_tail(directory,b'source')
                self.assertTrue(report['final_tails_validated'])
                self.assertTrue(report['continuity']['history_continuity_validated'])
                for before,after in [('tail=1 invocation=1','tail=1 invocation=0'),('words=2048','words=256'),
                                     ('stage=5','stage=4')]:
                    (directory/'error.log').write_text(text.replace(before,after,1))
                    with self.subTest(before=before),self.assertRaises(ValueError):
                        verify_tail(directory,b'source')
                path=directory/'mu4_tail_005_stage3.be16'
                blob=path.read_bytes()
                changed=bytes([blob[0]^1])+blob[1:]
                path.write_bytes(changed)
                (directory/'error.log').write_text(text)
                with self.assertRaises(ValueError):
                    verify_tail(directory,b'source')
                (directory/'error.log').write_text(text.replace(f'file={path.name} hash={fnv(blob):08x}',f'file={path.name} hash={fnv(changed):08x}'))
                self.assertFalse(verify_tail(directory,b'source')['final_tails_validated'])

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

import math
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch

from tools.mu4_coefficient_check import decode_table, updates, verify
from tools.mu4_transform_check import fnv


def recurrence_fixture(address,packed):
    n,k=128,3 if packed else 2
    words={address:n,address+1:k,address+8:0x2000,address+9:0x1000}
    def pair(at,value):
        words[at],words[at+1]=(value>>16)&65535,value&65535
    prefix=[10+10*i for i in range(k)]
    for i,value in enumerate(prefix):
        pair(0x2000+2*i,value)
    if packed:
        words[address+2],words[address+3]=2,3
        pair(address+4,4)
    else:
        pair(address+2,2)
        pair(address+4,3)
    pair(address+6,5)
    corrections=[(1,-1,2,-2)[i%4] for i in range(n-k-1)]
    for i,value in enumerate(corrections):
        at=0x1000+(i//4 if packed else i)
        words[at]=(words.get(at,0)|((value&15)<<(12-4*(i%4)))) if packed else value&65535
    return words,prefix,corrections


class CoefficientTests(unittest.TestCase):
    def test_finite_differences_against_binomial_integration(self):
        for address,packed in ((0xbbe6,True),(0xbc72,False)):
            words,prefix,corrections=recurrence_fixture(address,packed)
            output=decode_table(words,address)
            expected=prefix+[5]
            for r in range(1,len(corrections)+1):
                if packed:
                    value=5+4*r+3*math.comb(r+1,2)+2*math.comb(r+2,3)
                    value+=sum(math.comb(r-i+3,3)*delta for i,delta in enumerate(corrections[:r],1))
                else:
                    value=5+3*r+2*math.comb(r+1,2)
                    value+=sum(math.comb(r-i+2,2)*delta for i,delta in enumerate(corrections[:r],1))
                expected.append(value)
            self.assertEqual(output,tuple(expected))
            changed=dict(words)
            changed[address]=127
            with self.assertRaises(ValueError):
                decode_table(changed,address)

    def test_source_selector_truth_table(self):
        for a in (0,1,65535):
            for b in (0,1,65535):
                self.assertEqual(updates(0x24955,a,b),[(0xf000,0xc4e8 if a else 0xbbe6)])
                expected=[]
                if a or b: expected.append((0xf100,0xc572))
                if not a or not b: expected.append((0xf000,0xbc72))
                self.assertEqual(updates(0x2496e,a,b),expected)
        with self.assertRaises(ValueError):
            updates(0x24955,65536,0)

    def test_cache_preservation_and_consumer_continuity(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory=Path(temporary)
            tables={a:tuple(range(1024 if a in (0xbbe6,0xc4e8) else 128)) for a in (0xbbe6,0xc4e8,0xbc72,0xc572)}
            selections=((0x24955,0,0),(0x24955,1,0),(0x2496e,0,0),(0x2496e,1,1),(0x2496e,0,1))
            lines=[]
            for ordinal,(target,a,b) in enumerate(selections,1):
                parameter=1024 if target==0x24955 else 128
                start,end=ordinal+0.1,ordinal+0.2
                lines.append(f'mu4_coefficient_selection: invocation={ordinal} call={ordinal} target={target:06x} '
                             f'base=b33b flag0={a:04x} flag1={b:04x} t={start} identity=Execute_entry '
                             'context_only=1 observer_writes=0 audio_acceptance=0')
                before=bytes.fromhex('5555')*2048
                after=bytearray(before)
                for destination,descriptor in updates(target,a,b):
                    data=struct.pack(f'>{len(tables[descriptor])}i',*tables[descriptor])
                    offset=2*(destination-0xf000)
                    after[offset:offset+len(data)]=data
                for stage,blob in enumerate((before,after)):
                    name=f'mu4_coefficients_{ordinal:03d}_stage{stage}.be16'
                    (directory/name).write_bytes(blob)
                    address=0x24a63 if stage==0 else 0x24a64
                    time=start if stage==0 else end
                    lines.append(f'mu4_window_callback: invocation={ordinal} call={ordinal} site=1 '
                                 f'phase={"call" if stage==0 else "return"} address={address:06x} opcode=7777 '
                                 f'target={target:06x} base=b33b parameter={parameter:04x} t={time} observer_writes=0')
                    lines.append(f'mu4_coefficients_snapshot: invocation={ordinal} call={ordinal} stage={stage} '
                                 f'address={address:06x} opcode=7777 base=f000 words=2048 parameter={parameter:04x} '
                                 f'file={name} hash={fnv(blob):08x} t={time} identity=Execute_entry '
                                 'context_only=1 observer_writes=0 audio_acceptance=0')
                base=0xf100 if target==0x2496e and a and b else 0xf000
                offset=2*(base-0xf000)
                name=f'window{ordinal}.be16'
                (directory/name).write_bytes(after[offset:offset+4*parameter])
                lines.append(f'mu4_window_snapshot: invocation={ordinal} stage=1 base={base:04x} '
                             f'words={2*parameter} file={name} observer_writes=0')
            text='\n'.join(lines)
            (directory/'error.log').write_text(text)
            prerequisite=lambda *_:{'callbacks':[{} for _ in selections],'source_words_pinned':1469,'native_audio_validated':False}
            with patch('tools.mu4_coefficient_check.verify_tail',side_effect=prerequisite),patch('tools.mu4_coefficient_check.source_contract',return_value={}),patch('tools.mu4_coefficient_check.pin_block'),patch('tools.mu4_coefficient_check.decode_table',side_effect=lambda words,address:tables[address]):
                report=verify(directory,b'source')
                self.assertTrue(report['coefficient_publication_validated'])
                self.assertEqual(report['source_words_pinned'],1750)
                for before,after in [('flag0=0000','flag0=10000'),('words=2048','words=2047'),
                                     ('identity=Execute_entry','identity=live_pc')]:
                    (directory/'error.log').write_text(text.replace(before,after,1))
                    with self.subTest(before=before),self.assertRaises(ValueError):
                        verify(directory,b'source')
                path=directory/'mu4_coefficients_003_stage1.be16'
                blob=path.read_bytes()
                changed=blob[:1000]+bytes([blob[1000]^1])+blob[1001:]
                path.write_bytes(changed)
                (directory/'error.log').write_text(text)
                with self.assertRaises(ValueError):
                    verify(directory,b'source')
                (directory/'error.log').write_text(text.replace(f'file={path.name} hash={fnv(blob):08x}',f'file={path.name} hash={fnv(changed):08x}'))
                self.assertFalse(verify(directory,b'source')['coefficient_publication_validated'])

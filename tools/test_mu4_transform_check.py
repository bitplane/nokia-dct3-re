import hashlib
import math
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch

from tools.mu4_transform_check import SITES, fnv, pin_block, radix4, stage3, weighted_stage, verify


def coefficients():
    return [round(math.cos((i+1)*math.pi/256)*(1<<29)) for i in range(512)]


def fixture(directory):
    words={address:(0xf982 if address>=0x24900 else 0x1106 if stage in (1,2) else 0xee02)
           for stage,address in SITES.items()}
    words.update({0x117a:0x16a0,0x117b:0x9e66})
    lines=['mu4_processing_entry: call=2 t=0.9 observer_writes=0',
           'mu4_processing_return: call=2 t=1.2 observer_writes=0']
    for channel in (0,1):
        initial=[((i*1103515245+12345)>>5)%80000-40000 for i in range(128)]
        vectors={0:initial}
        vectors[1]=radix4(initial)
        vectors[2]=stage3(vectors[1],0x16a09e66)
        for stage,width in ((3,16),(4,32),(5,64)):
            vectors[stage]=weighted_stage(vectors[stage-1],coefficients(),width)
        vectors[6]=vectors[5]
        for ordinal,stage in enumerate((0,7,8,1,2,3,4,5,6)):
            blob=(struct.pack('>512i',*coefficients()) if stage==7 else bytes.fromhex('16a09e66') if stage==8
                  else struct.pack('>128i',*vectors[stage]))
            name=f'mu4_fft_ch{channel}_stage{stage}.be16'
            (directory/name).write_bytes(blob)
            base=0xf400 if stage==7 else 0x117a if stage==8 else (0x7800,0xe800)[channel]
            lines.append(f'mu4_fft_snapshot: channel={channel} stage={stage} address={SITES[stage]:06x} '
                         f'opcode={words[SITES[stage]]:04x} base={base:04x} words={len(blob)//2} parameter=0080 '
                         f'hash={fnv(blob):08x} file={name} t={1+channel*0.1+ordinal*0.001:.3f} '
                         'identity=Execute_entry context_only=1 observer_writes=0 audio_acceptance=0')
    (directory/'error.log').write_text('\n'.join(lines))
    return words


class TransformTests(unittest.TestCase):
    def test_hashed_source_presence_and_each_word_mutation(self):
        words={0x100+i:i for i in range(64)}
        digest=hashlib.sha256(struct.pack('>64H',*range(64))).hexdigest()
        pin_block(words,0x100,64,digest)
        for address in words:
            changed=dict(words)
            changed[address]^=1
            with self.subTest(address=address),self.assertRaises(ValueError):
                pin_block(changed,0x100,64,digest)
        with self.assertRaises(ValueError):
            pin_block({},0x100,64,digest)

    def test_radix4_against_inverse_dft_basis(self):
        for component in range(8):
            data=[0]*8
            data[component]=4096
            original=[complex(*data[i:i+2]) for i in range(0,8,2)]
            natural=[original[i] for i in (0,2,1,3)]
            expected=[]
            for k in range(4):
                value=sum(x*(1j**(j*k)) for j,x in enumerate(natural))/4
                expected.extend((round(value.real),round(value.imag)))
            self.assertEqual(radix4(data),expected)
        with self.assertRaises(ValueError):
            radix4([0])

    def test_complete_model_against_independent_inverse_dft_impulses(self):
        # A numerical model sanity check, not a native/codec error tolerance.
        for impulse in (0,1,7,31):
            reverse=sum(((impulse>>b)&1)<<(5-b) for b in range(6))
            data=[0]*128
            data[2*reverse]=1<<20
            result=stage3(radix4(data),0x16a09e66)
            for width in (16,32,64):
                result=weighted_stage(result,coefficients(),width)
            for k in range(64):
                angle=2*math.pi*k*impulse/64
                self.assertLessEqual(abs(result[2*k]-(1<<14)*math.cos(angle)),32)
                self.assertLessEqual(abs(result[2*k+1]-(1<<14)*math.sin(angle)),32)

    def test_capture_and_semantic_mutation_guards(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory=Path(temporary)
            words=fixture(directory)
            with patch('tools.mu4_transform_check.source_contract',return_value=words):
                report=verify(directory,b'source')
                self.assertTrue(report['selected_transform_validated'])
                self.assertFalse(report['native_audio_validated'])
                text=(directory/'error.log').read_text()
                for before,after in [('identity=Execute_entry','identity=live_pc'),('words=256','words=255'),
                                     ('parameter=0080','parameter=0400'),('base=117a','base=117b'),
                                     ('observer_writes=0','observer_writes=1'),('stage=8','stage=7')]:
                    (directory/'error.log').write_text(text.replace(before,after,1))
                    with self.subTest(before=before),self.assertRaises(ValueError):
                        verify(directory,b'source')
                (directory/'error.log').write_text(text)
                path=directory/'mu4_fft_ch0_stage2.be16'
                blob=path.read_bytes()
                changed=bytes([blob[0]^1])+blob[1:]
                path.write_bytes(changed)
                with self.assertRaises(ValueError):
                    verify(directory,b'source')
                # A consistently rehashed wrong vector must still fail arithmetic.
                (directory/'error.log').write_text(text.replace(f'hash={fnv(blob):08x}',f'hash={fnv(changed):08x}',1))
                self.assertFalse(verify(directory,b'source')['selected_transform_validated'])

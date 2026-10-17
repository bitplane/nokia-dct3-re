import hashlib
import math
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch

from tools.mu4_transform_check import SITES, fnv, pin_block, radix4, stage3, weighted_stage, verify, invocation_inventory, posttransform, verify_post, pretransform, bit_reverse_complex, verify_pre


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


def complete_fixture(directory):
    words={site:0xf982 for site in (0x24919,0x24944,0x2491e,0x24946,0x2494e,0x24950,
           0x2400a,0x2401e,0x24036,0x24051,0x2406c,0x2408b,0x240a6,0x240be)}
    words.update({0x117a:0x16a0,0x117b:0x9e66})
    lines=[]
    sites={1:0x2400a,2:0x2401e,3:0x24036,4:0x24051,5:0x2406c,
           6:0x2408b,7:0x240a6,8:0x240be}
    for call in range(1,9):
        parameter=0x80 if call%2 else 0x400
        address=0x24919 if parameter==0x80 else 0x24944
        final=5 if parameter==0x80 else 8
        lines.extend((f'mu4_processing_entry: call={call} t={call} observer_writes=0',
                      f'mu4_processing_return: call={call} t={call+0.9} observer_writes=0',
                      f'mu4_fft_invocation: call={call} address={address:06x} opcode=f982 '
                      f'parameter={parameter:04x} base=7000 t={call+0.1} identity=Execute_entry '
                      'context_only=1 observer_writes=0 audio_acceptance=0'))
        vectors={0:[((i*1103515245+12345)>>5)%80000-40000 for i in range(parameter)]}
        vectors[1]=radix4(vectors[0])
        vectors[2]=stage3(vectors[1],0x16a09e66)
        for stage in range(3,final+1):
            vectors[stage]=weighted_stage(vectors[stage-1],coefficients(),1<<(stage+1))
        vectors[9]=vectors[final]
        for ordinal,stage in enumerate([0,10,11]+list(range(1,final+1))+[9]):
            blob=(struct.pack('>512i',*coefficients()) if stage==10 else bytes.fromhex('16a09e66') if stage==11
                  else struct.pack(f'>{parameter}i',*vectors[stage]))
            name=f'mu4_fft_{call:03d}_stage{stage}.be16'
            (directory/name).write_bytes(blob)
            site=address if stage in (0,10,11) else (0x2491e if parameter==0x80 else 0x24946) if stage==9 else sites[stage]
            base=0xf400 if stage==10 else 0x117a if stage==11 else 0x7000
            lines.append(f'mu4_fft_vector_snapshot: invocation={call} call={call} stage={stage} '
                         f'address={site:06x} opcode=f982 base={base:04x} words={len(blob)//2} '
                         f'parameter={parameter:04x} hash={fnv(blob):08x} file={name} '
                         f't={call+0.1+ordinal*0.001:.3f} identity=Execute_entry context_only=1 '
                         'observer_writes=0 audio_acceptance=0')
    (directory/'error.log').write_text('\n'.join(lines))
    return words


class TransformTests(unittest.TestCase):
    def test_pre_half_shift_and_bit_reversal_against_independent_bounds(self):
        for size in (128,1024):
            values=[((i*1103515245+12345)>>5)%80000-40000 for i in range(size)]
            cs=[round(math.cos(i*math.pi/size)*(1<<29)) for i in range(size)]
            output=pretransform(values,cs)
            count=size//2
            for i in range(count):
                x,y=values[2*i],values[-1-2*i]
                c,d=cs[i],cs[count+i]
                real=((-((x*c)>>32))>>1)-(((y*d)>>32)>>1)
                imag=(((y*c)>>32)>>1)-(((x*d)>>32)>>1)
                self.assertIn(output[2*i]-(real<<3),(0,8,16))
                self.assertIn(output[2*i+1]-(imag<<3),(-8,0,8))
            self.assertEqual(bit_reverse_complex(bit_reverse_complex(values)),values)
            reverse=bit_reverse_complex(list(range(size)))
            self.assertEqual(reverse[2:4],[size//2,size//2+1])
        with self.assertRaises(ValueError):
            pretransform([0],[0])
        with self.assertRaises(ValueError):
            bit_reverse_complex([0])

    def test_pre_capture_continuity_and_rehashed_corruption(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory=Path(temporary)
            words={site:0xf982 for site in (0x2490f,0x24934,0x24914,0x24939)}
            lines=[]
            for call,parameter in ((1,128),(2,1024)):
                entry=0x2490f if parameter==128 else 0x24934
                returned=0x24914 if parameter==128 else 0x24939
                lines.extend((f'mu4_processing_entry: call={call} t={call} observer_writes=0',
                              f'mu4_processing_return: call={call} t={call+0.9} observer_writes=0',
                              f'mu4_fft_invocation: call={call} parameter={parameter:04x} base=7000 t={call+0.5} observer_writes=0',
                              'mu4_fft_vector_snapshot: observer_writes=0'))
                values=[((i*1103515245+12345)>>5)%80000-40000 for i in range(parameter)]
                cs=[round(math.cos(i*math.pi/parameter)*(1<<29)) for i in range(parameter)]
                output=pretransform(values,cs)
                (directory/f'mu4_fft_{call:03d}_stage0.be16').write_bytes(struct.pack(f'>{parameter}i',*bit_reverse_complex(output)))
                for stage,data in enumerate((values,cs,output)):
                    blob=struct.pack(f'>{parameter}i',*data)
                    name=f'mu4_pre_{call:03d}_stage{stage}.be16'
                    (directory/name).write_bytes(blob)
                    base=(0xf200 if parameter==128 else 0xf000) if stage==1 else 0x7000
                    lines.append(f'mu4_pre_snapshot: invocation={call} call={call} stage={stage} '
                                 f'address={returned if stage==2 else entry:06x} opcode=f982 base={base:04x} '
                                 f'words={2*parameter} parameter={parameter:04x} hash={fnv(blob):08x} '
                                 f'file={name} t={call+0.1+stage*0.01} identity=Execute_entry '
                                 'context_only=1 observer_writes=0 audio_acceptance=0')
            text='\n'.join(lines)
            (directory/'error.log').write_text(text)
            with patch('tools.mu4_transform_check.source_contract',return_value=words),patch('tools.mu4_transform_check.pin_block'),patch('tools.mu4_transform_check.verify_post',return_value={'selected_transform_validated':True,'post_transform_validated':True}):
                self.assertTrue(verify_pre(directory,b'source')['pre_transform_validated'])
                path=directory/'mu4_fft_001_stage0.be16'
                original=path.read_bytes()
                path.write_bytes(bytes([original[0]^1])+original[1:])
                report=verify_pre(directory,b'source')
                self.assertFalse(report['pre_transform_validated'])
                self.assertEqual(report['pre_transforms'][0]['bit_reversal_mismatches'],1)
                path.write_bytes(original)
                path=directory/'mu4_pre_001_stage2.be16'
                blob=path.read_bytes()
                changed=bytes([blob[0]^1])+blob[1:]
                path.write_bytes(changed)
                with self.assertRaises(ValueError):
                    verify_pre(directory,b'source')
                (directory/'error.log').write_text(text.replace(f'hash={fnv(blob):08x}',f'hash={fnv(changed):08x}',1))
                self.assertFalse(verify_pre(directory,b'source')['pre_transform_validated'])

    def test_post_rotation_against_full_precision_product_bound(self):
        for size in (128,1024):
            values=[((i*1103515245+12345)>>5)%80000-40000 for i in range(size)]
            cs=[round(math.cos(i*math.pi/size)*(1<<29)) for i in range(size)]
            output=posttransform(values,cs)
            count=size//2
            imaginary=[]
            for i in range(count):
                x,y=values[2*i:2*i+2]
                c,d=cs[i],cs[count+i]
                # Each source product omits low*low; its scaled floor differs
                # from a full signed product by zero or eight raw units.
                real=((x*c>>32)-(y*d>>32))<<3
                imag=((x*d>>32)+(y*c>>32))<<3
                self.assertIn(output[2*i]-real,(-8,0,8))
                imaginary.append(imag)
            for i,expected in enumerate(reversed(imaginary)):
                self.assertIn(output[2*i+1]-expected,(-16,-8,0))
        with self.assertRaises(ValueError):
            posttransform([0],[0])

    def test_complete_post_capture_and_corruption(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory=Path(temporary)
            words=complete_fixture(directory)
            text=(directory/'error.log').read_text()
            lines=[]
            for call in range(1,9):
                parameter=0x80 if call%2 else 0x400
                input_blob=(directory/f'mu4_fft_{call:03d}_stage9.be16').read_bytes()
                values=struct.unpack(f'>{parameter}i',input_blob)
                cs=[round(math.cos(i*math.pi/parameter)*(1<<29)) for i in range(parameter)]
                blobs=[input_blob,struct.pack(f'>{parameter}i',*cs),struct.pack(f'>{parameter}i',*posttransform(values,cs))]
                for stage,blob in enumerate(blobs):
                    name=f'mu4_post_{call:03d}_stage{stage}.be16'
                    (directory/name).write_bytes(blob)
                    base=(0xf200 if parameter==128 else 0xf000) if stage==1 else 0x7000
                    address=0x24950 if stage==2 else 0x2491e if parameter==128 else 0x2494e
                    lines.append(f'mu4_post_snapshot: invocation={call} call={call} stage={stage} '
                                 f'address={address:06x} opcode=f982 base={base:04x} words={parameter*2} '
                                 f'parameter={parameter:04x} hash={fnv(blob):08x} file={name} t={call+0.5+0.01*stage} '
                                 'identity=Execute_entry context_only=1 observer_writes=0 audio_acceptance=0')
            text+='\n'+'\n'.join(lines)
            (directory/'error.log').write_text(text)
            with patch('tools.mu4_transform_check.source_contract',return_value=words),patch('tools.mu4_transform_check.pin_block'):
                self.assertTrue(verify_post(directory,b'source')['post_transform_validated'])
                for before,after in [('mu4_post_snapshot: invocation=1','mu4_post_snapshot: invocation=2'),
                                     ('stage=2 address=024950','stage=2 address=02491e'),
                                     ('file=mu4_post_001_stage0.be16','file=../bad')]:
                    (directory/'error.log').write_text(text.replace(before,after,1))
                    with self.subTest(before=before),self.assertRaises(ValueError):
                        verify_post(directory,b'source')
                path=directory/'mu4_post_001_stage2.be16'
                blob=path.read_bytes()
                changed=bytes([blob[0]^1])+blob[1:]
                path.write_bytes(changed)
                (directory/'error.log').write_text(text)
                with self.assertRaises(ValueError):
                    verify_post(directory,b'source')
                (directory/'error.log').write_text(text.replace(f'hash={fnv(blob):08x}',f'hash={fnv(changed):08x}',1))
                self.assertFalse(verify_post(directory,b'source')['post_transform_validated'])

    def test_complete_short_and_long_snapshot_contract(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory=Path(temporary)
            words=complete_fixture(directory)
            with patch('tools.mu4_transform_check.source_contract',return_value=words):
                report=verify(directory,b'source')
                self.assertEqual(report['invocations'],8)
                self.assertTrue(report['selected_transform_validated'])
                self.assertFalse(report['native_audio_validated'])
                text=(directory/'error.log').read_text()
                for before,after in [('invocation=1','invocation=2'),('words=2048','words=256'),
                                     ('stage=11','stage=10'),('base=117a','base=117b'),
                                     ('file=mu4_fft_001_stage0.be16','file=../bad'),
                                     ('identity=Execute_entry','identity=live_pc')]:
                    (directory/'error.log').write_text(text.replace(before,after,1))
                    with self.subTest(before=before),self.assertRaises(ValueError):
                        verify(directory,b'source')
                path=directory/'mu4_fft_002_stage6.be16'
                blob=path.read_bytes()
                changed=bytes([blob[0]^1])+blob[1:]
                path.write_bytes(changed)
                (directory/'error.log').write_text(text)
                with self.assertRaises(ValueError):
                    verify(directory,b'source')
                (directory/'error.log').write_text(text.replace(f'hash={fnv(blob):08x}',f'hash={fnv(changed):08x}',1))
                self.assertFalse(verify(directory,b'source')['selected_transform_validated'])

    def test_invocation_inventory_bounds_and_identity(self):
        words={0x24919:0xf982,0x24944:0xf982}
        lines=[]
        for call in range(1,9):
            lines.extend((f'mu4_processing_entry: call={call} t={call} observer_writes=0',
                          f'mu4_processing_return: call={call} t={call+0.9} observer_writes=0'))
            for index,(address,parameter,base) in enumerate(((0x24919,0x80,0x7800),(0x24944,0x400,0xe800))):
                lines.append(f'mu4_fft_invocation: call={call} address={address:06x} opcode=f982 '
                             f'parameter={parameter:04x} base={base:04x} t={call+0.1+index*0.1} '
                             'identity=Execute_entry context_only=1 observer_writes=0 audio_acceptance=0')
        text='\n'.join(lines)
        report=invocation_inventory(text,words)
        self.assertEqual(report['invocations'],16)
        self.assertEqual(report['processing_calls'][0]['vectors'][1]['complex_points'],512)
        self.assertFalse(report['native_audio_validated'])
        for before,after in [('call=1 address','call=9 address'),('parameter=0400','parameter=0080'),
                             ('base=e800','base=ff00'),('opcode=f982','opcode=0000'),
                             ('identity=Execute_entry','identity=live_pc'),('observer_writes=0','observer_writes=1'),
                             ('t=1.1','t=0.1'),('t=1.2','t=1.05')]:
            with self.subTest(before=before),self.assertRaises(ValueError):
                invocation_inventory(text.replace(before,after,1),words)

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
        for size in (64,512):
            bits=size.bit_length()-1
            for impulse in (0,1,7,size//2-1):
                reverse=sum(((impulse>>b)&1)<<(bits-1-b) for b in range(bits))
                data=[0]*(2*size)
                data[2*reverse]=1<<20
                result=stage3(radix4(data),0x16a09e66)
                for power in range(4,bits+1):
                    result=weighted_stage(result,coefficients(),1<<power)
                for k in range(size):
                    angle=2*math.pi*k*impulse/size
                    amplitude=(1<<20)/size
                    self.assertLessEqual(abs(result[2*k]-amplitude*math.cos(angle)),32)
                    self.assertLessEqual(abs(result[2*k+1]-amplitude*math.sin(angle)),32)

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

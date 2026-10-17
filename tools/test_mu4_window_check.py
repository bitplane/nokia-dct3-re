import math
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch

from tools.mu4_transform_check import fnv
from tools.mu4_window_check import window_products, verify


def fixture(directory):
    lines=[]
    for ordinal,parameter in ((1,128),(2,1024),(3,128)):
        fft_parameter=1024 if ordinal==3 else parameter
        lines.extend((f'mu4_fft_invocation: call={ordinal} parameter={fft_parameter:04x} t={ordinal} observer_writes=0',
                      f'mu4_post_snapshot: invocation={ordinal} stage=2 t={ordinal+0.1} observer_writes=0',
                      f'mu4_processing_return: call={ordinal} t={ordinal+0.9} observer_writes=0'))
        values=[((i*1103515245+12345)>>5)%80000-40000 for i in range(parameter)]
        coefficients=[round(math.cos(i*math.pi/parameter)*(1<<29)) for i in range(parameter)]
        output=window_products(values,coefficients)
        for stage,data in enumerate((values,coefficients,output)):
            blob=struct.pack(f'>{parameter}i',*data)
            name=f'mu4_window_{ordinal:03d}_stage{stage}.be16'
            (directory/name).write_bytes(blob)
            address=0x24a72 if stage==2 else 0x24a70
            lines.append(f'mu4_window_snapshot: invocation={ordinal} call={ordinal} stage={stage} '
                         f'address={address:06x} opcode=f982 base={0x8000 if stage==1 else 0x7000:04x} '
                         f'words={2*parameter} parameter={parameter:04x} file={name} hash={fnv(blob):08x} '
                         f't={ordinal+0.2+stage*0.01} identity=Execute_entry context_only=1 '
                         'observer_writes=0 audio_acceptance=0')
    (directory/'error.log').write_text('\n'.join(lines))


class WindowTests(unittest.TestCase):
    def test_mirrored_products_and_reversal_against_full_precision_bounds(self):
        for size in (128,1024):
            values=[((i*1103515245+12345)>>5)%80000-40000 for i in range(size)]
            coefficients=[round(math.cos(i*math.pi/size)*(1<<29)) for i in range(size)]
            output=window_products(values,coefficients)
            count=size//2
            for i in range(count):
                left,right=values[count-1-i],values[count+i]
                c,d=coefficients[i],coefficients[-1-i]
                difference=(((left*d)>>32)-((right*c)>>32))<<3
                total=(((right*d)>>32)+((left*c)>>32))<<3
                self.assertIn(output[i]-difference,(-8,0,8))
                self.assertIn(output[size-1-i]-total,(-16,-8,0))
        with self.assertRaises(ValueError):
            window_products([0],[0])

    def test_capture_metadata_and_consistently_rehashed_wrong_output(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory=Path(temporary)
            fixture(directory)
            with patch('tools.mu4_window_check.source_contract',return_value={0x24a70:0xf982,0x24a72:0xf982}),patch('tools.mu4_window_check.pin_block'),patch('tools.mu4_window_check.verify_pre',return_value={'pre_transform_validated':True,'selected_transform_validated':True,'post_transform_validated':True,'source_words_pinned':837}):
                report=verify(directory,b'source')
                self.assertTrue(report['window_products_validated'])
                self.assertEqual((report['windows'][2]['fft_parameter'],report['windows'][2]['components']),('0400',128))
                self.assertEqual(report['source_words_pinned'],955)
                self.assertFalse(report['native_audio_validated'])
                text=(directory/'error.log').read_text()
                for before,after in [('invocation=1 call=1 stage=0','invocation=2 call=1 stage=0'),
                                     ('words=2048','words=256'),('base=7000','base=ff00'),
                                     ('identity=Execute_entry','identity=live_pc'),
                                     ('file=mu4_window_001_stage0.be16','file=../bad')]:
                    (directory/'error.log').write_text(text.replace(before,after,1))
                    with self.subTest(before=before),self.assertRaises(ValueError):
                        verify(directory,b'source')
                path=directory/'mu4_window_001_stage2.be16'
                blob=path.read_bytes()
                changed=bytes([blob[0]^1])+blob[1:]
                path.write_bytes(changed)
                (directory/'error.log').write_text(text)
                with self.assertRaises(ValueError):
                    verify(directory,b'source')
                (directory/'error.log').write_text(text.replace(f'hash={fnv(blob):08x}',f'hash={fnv(changed):08x}',1))
                self.assertFalse(verify(directory,b'source')['window_products_validated'])

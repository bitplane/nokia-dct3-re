import struct
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from tools.mu4_processing_check import match_packets, signed40, verify
from tools.mu4_playback_frame_check import rows


class ProcessingTests(unittest.TestCase):
    @staticmethod
    def packet(length=16):
        return bytes([255,249,80,128|(length>>11),(length>>3)&255,((length&7)<<5)|31,252])+bytes(range(1,length-6))

    def fixture(self, length=16):
        packet = self.packet(length)
        entry = {'call':'1','index':'0001','remaining':'0010','base':'0200','reservoir':'0000fff9','t':'1.0'}
        returned = {'call':'1','index':f'{(length+1)//2:04x}','remaining':'0008' if length&1 else '0000','t':'2.0'}
        reads = [{'address':f'{0x200+i//2:04x}','word':(packet+b'\xaa')[i:i+2].hex(),'t':str(1.1+i*.01)}
                 for i in range(2,len(packet),2)]
        return packet,entry,returned,reads

    def test_packet_consumption_requires_every_actual_byte(self):
        packet,entry,returned,reads = self.fixture()
        result = match_packets([entry],[returned],reads,packet)
        self.assertEqual(result[0]['covered_bytes'],16)
        self.assertEqual(result[0]['unvalidated_unconsumed_lookahead_bytes'],0)
        bad = [dict(row) for row in reads]
        bad[0]['word']='ffff'
        for e,r,rr in ((entry,returned,bad),(entry,returned,reads[:-1]),
                       (dict(entry,reservoir='0000fff8'),returned,reads),
                       (entry,dict(returned,index='0009'),reads),
                       (dict(entry,remaining='000f'),returned,reads)):
            with self.assertRaises(ValueError):
                match_packets([e],[r],rr,packet)

    def test_unconsumed_lookahead_is_not_counted_as_reference_data(self):
        packet,entry,returned,reads = self.fixture(15)
        result = match_packets([entry],[returned],reads,packet)
        self.assertEqual(result[0]['covered_bytes'],15)
        self.assertEqual(result[0]['unvalidated_unconsumed_lookahead_bytes'],1)
        with self.assertRaises(ValueError):
            match_packets([entry],[returned],reads,packet+b'\0')

    def test_accumulator_width_is_enforced(self):
        self.assertEqual(signed40((1<<40)-1),-1)
        self.assertEqual(signed40(1<<39),-(1<<39))
        for value in (-1,1<<40):
            with self.assertRaises(ValueError):
                signed40(value)

    def test_processing_wrapper_rejects_order_scope_and_stored_value_drift(self):
        def line(marker,fields):
            return marker+': '+' '.join(f'{key}={value}' for key,value in fields.items())+' observer_writes=0 identity=Execute_entry'
        packet = self.packet()
        text=[]
        snapshots=[[],[]]
        with tempfile.TemporaryDirectory() as name:
            directory=Path(name)
            for frame in range(8):
                aac,entry,returned,reads=self.fixture()
                start=3*frame+1
                entry.update(call=str(frame+1),t=str(start),sp='0ffe',caller_sp='1000',a='1006',st1='4000',context_only='1')
                destination=0x7000+(frame%2)*0x800
                returned.update(call=str(frame+1),t=str(start+.5),sp='1000',result='0',destination=f'{destination:04x}',input0='f000',input1='e800')
                text.append(line('mu4_processing_entry',entry))
                text.append(line('mu4_processing_return',returned))
                text.append(line('mu4_conversion_block',{'call':frame+1,'frame':frame,'samples':1024,'destination':f'{destination:04x}','bad':0,'t':start+1}))
                for read in reads:
                    read['t']=str(start+float(read['t'])-1)
                    text.append(line('mu4_frame_bit_read',read))
                for channel in (0,1):
                    filename=f'{frame}_{channel}.be16'
                    (directory/filename).write_bytes(bytes(2048))
                    snapshots[channel].append({'file':filename,'base':destination+1024*channel,'time':str(start+1.1)})
                if frame<8:
                    for sample in range(1024):
                        for channel in (0,1):
                            text.append(line('mu4_conversion_value',{'call':frame+1,'frame':frame,'sample':sample,'channel':channel,
                                'argument':14,'raw':'0000000000','shifted':'0000000000','result':'0000000000',
                                'retirement':'unclaimed','destination':f'{destination+sample+1024*channel:04x}',
                                't':start+.501+(2*sample+channel)*.0001}))
            original='\n'.join(text)
            (directory/'error.log').write_text(original)
            with patch('tools.mu4_processing_check.verify_frames',return_value={'source_blocks':snapshots}),patch('tools.mu4_processing_check.contract',return_value={'pcm_conversion':{'pinned_words':152}}):
                result=verify(directory,b'source',packet*8,bytes(32768),require_eight_blocks=True)
                self.assertTrue(result['packet_to_output_call_binding_validated'])
                self.assertEqual((result['conversion_words'],result['conversion_blocks']),(16384,8))
                self.assertEqual(result['reference_comparison_diagnostic']['samples'],16384)
                self.assertFalse(result['reference_comparison_diagnostic']['codec_tolerance_established'])
                self.assertFalse(result['native_audio_validated'])
                legacy='\n'.join(line for line in text if not rows(line,'mu4_conversion_value') or int(rows(line,'mu4_conversion_value')[0]['call'])<=2)
                (directory/'error.log').write_text(legacy)
                self.assertEqual(verify(directory,b'source',packet*8,bytes(32768))['conversion_blocks'],2)
                with self.assertRaises(ValueError):
                    verify(directory,b'source',packet*8,bytes(32768),require_eight_blocks=True)
                for bad in (original.replace('bad=0','bad=1',1),original.replace('argument=14','argument=15',1),
                            original.replace('raw=0000000000','raw=0000004000',1),
                            'raw=0000004000'.join(original.rsplit('raw=0000000000',1)),
                            original.replace('identity=Execute_entry','identity=GENPC',1),
                            original.replace('caller_sp=1000','caller_sp=1001',1)):
                    (directory/'error.log').write_text(bad)
                    with self.assertRaises(ValueError):
                        verify(directory,b'source',packet*8,bytes(32768))
                (directory/'error.log').write_text(original)
                (directory/snapshots[1][7]['file']).write_bytes(b'\0\1'+bytes(2046))
                with self.assertRaises(ValueError):
                    verify(directory,b'source',packet*8,bytes(32768))


if __name__=='__main__':
    unittest.main()

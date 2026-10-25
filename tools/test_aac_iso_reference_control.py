from pathlib import Path
import tempfile
import unittest
import wave

from tools.aac_iso_reference_control import cases,read_result,require_hash,write_pcm24
from tools.aac_reference_metrics import compare


class OriginalControlTest(unittest.TestCase):
    def test_exact_original_verdict_and_exit(self):
        for verdict,code,expected in (('OK',0,True),('FAIL',2,False)):
            text='version 1.8\n===  Test on RMS and abs. max. difference sample  : '+verdict+'\n'
            self.assertEqual(read_result(text,code),expected)
            for bad,exitcode in ((text,1),(text+text,code),
                                 (text.replace('1.8','1.80'),code),
                                 (text.replace(verdict,'UNKNOWN'),code),
                                 (text+'Test on RMS and abs. max. difference sample malformed',code)):
                with self.subTest(verdict=verdict,bad=bad),self.assertRaises(ValueError):
                    read_result(bad,exitcode)

    def test_pinned_archive_required(self):
        with self.assertRaisesRegex(ValueError,'identity differs'):
            require_hash(b'not original','0'*64)

    def test_synthetic_criteria_and_legacy_boundary_are_distinct(self):
        rows=cases()
        self.assertEqual(len(rows),6)
        differences=[]
        for name,channels,reference,native,expected,original in rows:
            result=compare([v/(1<<23) for v in native],
                           [v/(1<<23) for v in reference],channels)
            self.assertEqual(all(r['criterion_met'] for r in result['channels']),expected,name)
            if expected!=original:
                differences.append(name)
        self.assertEqual(differences,['peak-equality'])

    def test_stereo_does_not_average_away_bad_channel(self):
        _,channels,reference,native,_,_=cases()[-1]
        result=compare([v/(1<<23) for v in native],[v/(1<<23) for v in reference],channels)
        self.assertEqual([r['criterion_met'] for r in result['channels']],[True,False])

    def test_exact_pcm24_writer(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'control.wav'
            samples=[-(1<<23),-1,0,(1<<23)-1]
            write_pcm24(path,samples,2)
            with wave.open(str(path),'rb') as source:
                self.assertEqual((source.getnchannels(),source.getsampwidth(),source.getframerate(),source.getnframes()),(2,3,44100,2))
                raw=source.readframes(2)
            self.assertEqual([int.from_bytes(raw[i:i+3],'little',signed=True) for i in range(0,len(raw),3)],samples)
            for values,channels in (([],1),([0],0),([0],2),([1<<23],1)):
                with self.assertRaises(ValueError):
                    write_pcm24(path,values,channels)


if __name__=='__main__':
    unittest.main()

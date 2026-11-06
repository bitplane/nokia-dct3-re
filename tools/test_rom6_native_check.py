import hashlib
import unittest
import tempfile
from pathlib import Path
from unittest.mock import patch
from tools import rom6_native_check as check


GOOD='\n'.join([
    'rom6_native_composition: hle=0 staged=0 observer_writes=0',
    'rom6_native_entry: kind=reset', 'rom6_native_entry: kind=upload',
    'rom6_native_verifier: result0=0000 result1=0006 observer_writes=0',
    'rom6_native_entry: kind=reset', 'rom6_native_entry: kind=upload',
    'rom6_native_upload: region=loader2 words=623 entry=0a00 observer_writes=0',
    'rom6_native_upload: region=installed words=422 entry=2c75 observer_writes=0',
    'rom6_native_entry: kind=resident',
    'rom6_native_selftest: command=0d faults=00 flag=c4 observer_writes=0',
    'rom6_native_state: kind=terminal t=30.016667 pc=0097bc illegal=0 idle=0 pmst=07ac st1=6903 imr=035f ifr=0225 hle=0 observer_writes=0',
    'rom6_native_coverage: missing_program_reads=0 observer_writes=0',
])


class NativeCheckTest(unittest.TestCase):
    def test_configured_clock_and_native_backend(self):
        xml='<mame><machine name="nsm3r6"><chip tag="dsp_c54x:cpu" clock="45500000"/><device_ref name="nokia_dsp_c54x"/></machine></mame>'
        check.check_composition(xml)
        for old,new in (('45500000','52000000'),('nokia_dsp_c54x','nokia_dsp_hle'),('nsm3r6','other')):
            with self.subTest(old=old),self.assertRaises(ValueError):
                check.check_composition(xml.replace(old,new))

    def test_valid_trace(self):
        check.check_trace(GOOD)

    def test_missing_or_reordered_initialization(self):
        for token in ('rom6_native_verifier:', 'kind=resident', 'words=623', 'flag=c4','missing_program_reads=0'):
            with self.subTest(token=token),self.assertRaises(ValueError):
                check.check_trace(GOOD.replace(token,'missing'))

    def test_rejects_bad_execution_and_incomplete_observation(self):
        for suffix in ('unsupported form','[LUA ERROR] callback','native_suspended=1',
                       'runtime_hle_handoff','nsm3r6_state: FAIL incomplete'):
            with self.subTest(suffix=suffix),self.assertRaises(ValueError):
                check.check_trace(GOOD+'\n'+suffix)
        for old,new in (('illegal=0','illegal=1'),('hle=0','hle=1'),('30.016667','3.016667')):
            with self.subTest(old=old),self.assertRaises(ValueError):
                check.check_trace(GOOD.replace(old,new))

    def test_rejects_an_extra_reset(self):
        with self.assertRaises(ValueError):
            check.check_trace(GOOD+'\nrom6_native_entry: kind=reset')

    def test_exact_own_uploads(self):
        image=bytearray(0x11ac40+623*2)
        loader=bytes(i%256 for i in range(623*2))
        installed=bytes((i*7)%256 for i in range(422*2))
        image[0x11ac40:]=loader
        image[0x115128:0x115128+len(installed)]=installed
        with patch.object(check,'MCU_SHA1',hashlib.sha1(image).hexdigest()):
            check.check_uploads(image,loader,installed)
            for candidate in (loader[:-1],loader[:-1]+bytes([loader[-1]^1])):
                with self.assertRaises(ValueError): check.check_uploads(image,candidate,installed)
            with self.assertRaises(ValueError): check.check_uploads(image,loader,installed[:-1])
        with self.assertRaises(ValueError): check.check_uploads(image,loader,installed)

    def test_blank_is_not_a_menu(self):
        from PIL import Image
        for size in ((84,48),(84,49)):
            with self.subTest(size=size),self.assertRaises(ValueError):
                check.check_menu(Image.new('L',size,255))

    def test_replay_requires_complete_matching_endpoints_and_active_io(self):
        from PIL import Image
        frame=Image.new('L',(84,48),255)
        def state(time):
            arm=['00000000']*37
            arm[15]='00200000'; arm[13]='00110000'
            return ('t='+time+' pc=00200000 sp=00110000 ram=01234567 cpu='+','.join(arm)+
                    ' dsp='+','.join(name+':0' for name in sorted(check.DSP_REGISTERS))+' data=12345678 program=87654321')
        text='\n'.join('nsm3r6_state: phase='+label+' '+state(time) for label,time in (
            ('saved','24.000000000'),('reference_end','26.700000000'),
            ('restored','24.000000000'),('restored_end','26.700000000')))
        text+='\nnsm3r6_state_replay: reference=42 restored=42 window=2.7 streamed=1 endpoint=1\n'
        text+='\n'.join('nsm3r6_state_contact: phase='+phase+' down='+down for phase,down in (
            ('reference','1'),('reference','0'),('restored','1'),('restored','0')))
        text+='\nnsm3r6_state: completed=1'
        with tempfile.TemporaryDirectory() as directory,patch.object(check,'check_menu'):
            reference=Path(directory)/'reference.io'
            restored=Path(directory)/'restored.io'
            records=b'0.001000000:0000:0001:ffff\n'*42
            reference.write_bytes(records); restored.write_bytes(records)
            check.check_state(text,frame,frame,reference,restored)
            for old,new in (('endpoint=1',''),('reference=42 restored=42','reference=0 restored=0'),
                            ('restored_end','missing_end'),('AR3:0','AR3:1'),
                            ('cpu=00000000,','cpu='),('data=12345678','data=bad'),('ILLEGAL:0','ILLEGAL:1')):
                with self.subTest(old=old),self.assertRaises(ValueError):
                    check.check_state(text.replace(old,new,1),frame,frame,reference,restored)
            for broken in (records[:-1],records.replace(b':0001:',b':0002:',1),records+records):
                restored.write_bytes(broken)
                with self.assertRaises(ValueError): check.check_state(text,frame,frame,reference,restored)


if __name__=='__main__': unittest.main()

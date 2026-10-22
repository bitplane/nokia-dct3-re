from functools import reduce
from operator import xor
from pathlib import Path
import tempfile
import unittest
from tools import mu4_playback_serial_restore_check as gate


def frame(index,selector,token):
    values=[0x1e,5,0xaa,1,selector,1,0,0,token]
    values += [reduce(xor,values,0),0x55]
    return (f'mu4_native_recorder_response: index={index} selector={selector:02x} words=11 token={token:02x} class=1\n'+
            ''.join(f'mu4_native_recorder_response_word: index={i} value={v:04x}\n' for i,v in enumerate(values)))


def fixture():
    prefix='''mu4_original_resident_program: PASS checked_words=48261 mismatch_words=0 excluded_data_words=3660 source_words=51921 before_resident_execution=1 physical_mapping=unvalidated full_boot=0
mu4_original_resident_data: PASS checked_words=3656 mismatch_words=0 excluded_register_words=4 source_data_words=3660 before_resident_execution=1 physical_mapping=unvalidated full_boot=0
mu4_playback_control_input: phase=status49
'''+''.join(frame(i,0x58+i,i) for i in range(3))
    cpu=','.join(f'{name}:0000000000' for name in gate.REGISTERS)
    snapshot=f't=1.050000000 cpu={cpu} ram=12345678 program=abcdef01 data_words=65408 program_bytes=16777216 observer_writes=0\n'
    continuation=frame(3,0x71,3)+'''mu4_playback_control_input: phase=control30_04
mu4_native_recorder_control_ack: token=03 pin_output=1
mu4_play_control: kind=send16 pc=020727 pending=0003
mu4_play_control: kind=resident_idle3 pc=028922 opcode=f5e1
'''
    def events(phase):
        return ''.join(f'mu4_control_replay_event: phase={phase} index={i} t={1.08+i*0.001:.9f} address=0033 value={v:04x} mask=ffff observer_writes=0\n'
                       for i,v in enumerate([0x1e,5,0xaa,1,0x71,1,0,0,3,0xc3,0x55,0x7f,3,0x55]))
    return (prefix+'mu4_control_state: phase=saved '+snapshot+events('reference')+continuation+
            'mu4_control_state: phase=restored '+snapshot+events('restored')+continuation+
            '''mu4_control_replay: result=pass reference_events=14 restored_events=14 window=0.150000000 cap=4096 observer_writes=0 audio_acceptance=0
mu4_playback_capture_terminal: closed=1 illegal=0
mu4_playback_control_terminal: status_reply=1 input_cursor=29 decoded_calls=5 idle=1 pc=028923
mu4_control_restore_terminal: complete=1 observer_writes=0 audio_acceptance=0
''')


def resident_fixture():
    text = fixture()
    prefix = text[:text.index('mu4_control_state: phase=saved')]
    prefix += ''.join(frame(i,0x60+i,i) for i in range(3,6))
    prefix += 'mu4_playback_control_input: phase=resident_status49\n'
    prefix += ''.join(f'mu4_native_command_input: cursor={i} total=37\n' for i in range(30,35))
    cpu = ','.join(f'{name}:{(1 if name == "IDLE" else 0):010x}' for name in gate.REGISTERS)
    snapshot = f't=11.205000000 cpu={cpu} ram=12345678 program=abcdef01 data_words=65408 program_bytes=16777216 observer_writes=0\n'
    values = [0x7f,4,0x55,0x1e,5,0xaa,1,0x71,1,0,0,2,0xc2,0x55]
    text = prefix
    for phase in ('reference', 'restored'):
        text += f'mu4_control_state: phase={"saved" if phase == "reference" else "restored"} '+snapshot
        text += ''.join(f'mu4_control_replay_event: phase={phase} index={i} t={11.22+i*0.001:.9f} address=0033 value={v:04x} mask=ffff observer_writes=0\n'
                        for i,v in enumerate(values))
        text += 'mu4_native_recorder_control_ack: token=04 pin_output=1\n'+frame(6,0x71,2)
    return text+'''mu4_control_replay: result=pass reference_events=14 restored_events=14 window=0.150000000 cap=4096 observer_writes=0 audio_acceptance=0
mu4_playback_capture_terminal: closed=1 illegal=0
mu4_playback_control_terminal: status_reply=1 input_cursor=37 decoded_calls=5 idle=1 pc=028923
mu4_control_restore_terminal: complete=1 observer_writes=0 audio_acceptance=0
'''


class NativeControlRestoreTest(unittest.TestCase):
    def verify(self,text):return gate.check(text,'Fatal error: '+gate.COMPLETION+'\n')

    def test_native_replay_is_not_audio_completion(self):
        result=self.verify(fixture())
        self.assertEqual(result['registers'],36)
        self.assertEqual(result['tx_events'],14)
        self.assertFalse(result['audio_completion'])

    def test_resident_status_replay(self):
        result=gate.check(resident_fixture(),'Fatal error: '+gate.COMPLETION+'\n',True)
        self.assertEqual(result['tx_events'],14)
        self.assertFalse(result['audio_completion'])

    def test_resident_status_pending_identity_and_replay_are_strict(self):
        original=resident_fixture()
        for old,new in [('phase=resident_status49','phase=other'), ('t=11.205000000','t=11.305000000'),
                        ('token=02 class=1','token=03 class=1'), ('token=04 pin_output=1','token=05 pin_output=1'),
                        ('input_cursor=37','input_cursor=29'), ('idle=1 pc=028923','idle=1 pc=0298ea')]:
            with self.subTest(old=old),self.assertRaises(ValueError):
                gate.check(original.replace(old,new),'Fatal error: '+gate.COMPLETION+'\n',True)
        with self.assertRaisesRegex(ValueError,'five of eight'):
            gate.check(original.replace('cursor=34 total=37','cursor=35 total=37'),
                       'Fatal error: '+gate.COMPLETION+'\n',True)
        with self.assertRaisesRegex(ValueError,'pending'):
            gate.check('mu4_native_recorder_control_ack: token=04 pin_output=1\n'+original,
                       'Fatal error: '+gate.COMPLETION+'\n',True)

    def test_rejects_state_and_scope_changes(self):
        for old,new in [('ram=12345678','ram=00000000'),('program=abcdef01','program=00000000'),
                        ('t=1.050000000','t=1.060000000'),('PC:0000000000','PC:0000000001')]:
            text=fixture()
            begin=text.index('mu4_control_state: phase=restored')
            with self.subTest(old=old),self.assertRaises(ValueError):
                self.verify(text[:begin]+text[begin:].replace(old,new,1))
        for old,new in [('observer_writes=0','observer_writes=1'),('data_words=65408','data_words=64000'),
                        ('IDLE:0000000000','IDLE:0000000001'),(',CURPC:0000000000',''),
                        ('ram=12345678','ram='),('program=abcdef01','program='),
                        ('complete=1','complete=0')]:
            with self.subTest(old=old),self.assertRaises(ValueError):self.verify(fixture().replace(old,new))

    def test_rejects_event_replay_drift_and_false_terminal(self):
        for old,new in [('index=0 t=1.080000000','index=0 t=1.081000000'),('value=001e','value=001f'),
                        ('index=0','index=1'),('address=0033','address=0034')]:
            text=fixture()
            begin=text.index('mu4_control_state: phase=restored')
            with self.subTest(old=old),self.assertRaises(ValueError):
                self.verify(text[:begin]+text[begin:].replace(old,new,1))
        for old,new in [('reference_events=14','reference_events=13'),('cap=4096','cap=4095'),
                        ('window=0.150000000','window=0.100000000'),('result=pass','result=fail')]:
            with self.subTest(old=old),self.assertRaises(ValueError):self.verify(fixture().replace(old,new))

    def test_save_must_precede_status_response_and_control(self):
        for extra in ('mu4_native_recorder_response: selector=71 \n','mu4_playback_control_input: phase=control30_04\n'):
            with self.subTest(extra=extra),self.assertRaises(ValueError):self.verify(extra+fixture())

    def test_equal_but_wrong_replay_cannot_replace_native_wire_frames(self):
        with self.assertRaisesRegex(ValueError,'status frame/control ACK'):
            self.verify(fixture().replace('address=0033 value=001e','address=0033 value=001f'))

    def test_strict_health_cannot_be_hidden(self):
        with self.assertRaises(RuntimeError):
            gate.check(fixture(),'Fatal error: '+gate.COMPLETION+'\n[LUA ERROR] failed\n')

    def test_preserved_capture_prefixes_are_byte_exact(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parents[1]/'tmp') as directory:
            run=Path(directory)
            text=''
            for index,name in enumerate(gate.CAPTURES):
                (run/(name+'.before-load-1')).write_bytes(b'prefix'+bytes([index]))
                (run/name).write_bytes(b'prefix'+bytes([index])+b'continued')
                text+=f'mu4_playback_capture_rewind: index={index} bytes=3 reference={name}.before-load-1 same_process=1 firmware_state_forcing=0 observer_writes=0 audio_acceptance=0\n'
                text+='capture: save_restore=same_process_rewind observer_writes=0\n'
            self.assertEqual(gate.check_captures(run,text),dict(reference_bytes=63,exact_prefixes=9))
            path=run/gate.CAPTURES[0]
            path.write_bytes(b'wrong prefix')
            with self.assertRaisesRegex(ValueError,'restored prefix'):gate.check_captures(run,text)
            path.write_bytes(b'prefix\x00continued')
            for old,new in [('index=0','index=1'),('bytes=3','bytes=300'),
                            ('same_process=1','same_process=0'),('before-load-1','../unexpected')]:
                with self.subTest(old=old),self.assertRaises(ValueError):gate.check_captures(run,text.replace(old,new,1))

    def test_all_captures_must_match_independent_cold_control(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parents[1]/'tmp') as directory:
            root=Path(directory)
            cold,restored=root/'cold',root/'restored'
            cold.mkdir();restored.mkdir()
            for name in gate.CAPTURES:
                (cold/name).write_bytes(b'complete')
                (restored/name).write_bytes(b'complete')
            self.assertEqual(gate.check_independent_captures(restored,cold),dict(cold_capture_bytes=72,exact_cold_files=9))
            for name in gate.CAPTURES:
                (restored/name).write_bytes(b'wrong')
                with self.subTest(name=name),self.assertRaises(ValueError):gate.check_independent_captures(restored,cold)
                (restored/name).write_bytes(b'complete')


if __name__=='__main__':unittest.main()

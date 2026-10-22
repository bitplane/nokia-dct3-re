from functools import reduce
from operator import xor
import unittest
from pathlib import Path
import re
from tools import mu4_playback_serial_control_check as gate


def response(selector=0x71,token=3):
    data=[0x1e,3,0xaa,1,selector,0,token]
    data += [reduce(xor,data,0),0x55]
    return (f'mu4_native_recorder_response: index=0 selector={selector:02x} words={len(data)} token={token:02x} class=1\n'+
            ''.join(f'mu4_native_recorder_response_word: index={index} value={value:04x}\n'
                    for index,value in enumerate(data)))


def fixture():
    text='''mu4_original_bootstrap_upload: records=7 words=10760 program_only=1
mu4_original_resident_program: PASS checked_words=48261 mismatch_words=0 excluded_data_words=3660 source_words=51921 before_resident_execution=1 physical_mapping=unvalidated full_boot=0
mu4_original_resident_data: PASS checked_words=3656 mismatch_words=0 excluded_register_words=4 source_data_words=3660 before_resident_execution=1 physical_mapping=unvalidated full_boot=0
mu4_playback_control_input: phase=play30_02 decoded_calls=0 replies=0 t=0.3 firmware_state_forcing=0 audio_acceptance=0
mu4_native_playback_overlay: PASS marker=aa99 records=3127 words=49843 checked_ram_words=49839 excluded_register_words=4 before_execution=1
mu4_playback_control_input: phase=status49 decoded_calls=1 replies=3 t=1.0 firmware_state_forcing=0 audio_acceptance=0
mu4_native_recorder_control_ack: token=02 pin_output=1
'''+response()+'''mu4_playback_control_input: phase=control30_04 decoded_calls=2 replies=4 t=1.1 firmware_state_forcing=0 audio_acceptance=0
mu4_native_recorder_control_ack: token=03 pin_output=1
'''
    for ordinal,kind in enumerate(('serial30','control30_04','build03','input03','pending3','send16','resident_idle3'),1):
        pc,opcode=gate.SITES[kind]
        text+=(f'mu4_play_control: ordinal={ordinal} kind={kind} pc={pc:06x} opcode={opcode:04x} '
               f'cycles={ordinal*100} sp=1000 f2=0003 f3=0000 f4=0000 f5=0001 pending=0003 '
               'active=0001 state=0000 lifecycle=0004 parameter=0004 identity=Execute_entry operands=context_only observer_writes=0\n')
    text+=''.join(f'mu4_native_command_input: cursor={index} total=29\n' for index in range(1,30))
    return text+'''mu4_playback_capture_terminal: closed=1 illegal=0 t=20.000000000 observer_writes=0 audio_acceptance=0
mu4_playback_control_terminal: status_reply=1 input_cursor=29 decoded_calls=5 idle=1 pc=028923 t=20.000000000 firmware_state_forcing=0 audio_acceptance=0
mu4_play_control_terminal: events=7 cap=1024 observer_writes=0 audio_acceptance=0
'''


class NativeSerialControlTest(unittest.TestCase):
    def test_debugger_hex_limits_match_reported_decimal_cap(self):
        source=Path(__file__).with_name('mu4_playback_serial_control_observe.lua').read_text()
        limits=re.findall(r'temp0<([0-9a-f]+)',source)
        self.assertEqual(len(limits),2)
        self.assertTrue(all(int(limit,16)==1024 for limit in limits))

    def verify(self,text):return gate.check(text,'Fatal error: '+gate.COMPLETION+'\n')

    def test_complete_explicit_control_is_not_audio_acceptance(self):
        result=self.verify(fixture())
        self.assertFalse(result['audio_completion'])
        self.assertFalse(result['receive16_observed'])

    def test_unclassified_input_total_is_not_ignored(self):
        with self.assertRaises(ValueError):
            self.verify(fixture()+'\nmu4_native_command_input: cursor=30 total=99\n')

    def test_rejects_early_control_even_when_reply_counter_is_high(self):
        text=fixture().replace('replies=4','replies=400')
        start=text.index('mu4_playback_control_input: phase=control30_04')
        end=text.index('\n',start)+1
        early=text[start:end]+text[:start]+text[end:]
        with self.assertRaises(ValueError):self.verify(early)

    def test_response_is_byte_and_token_bound(self):
        for old,new in [('token=03 class=1','token=02 class=1'),
                        ('index=5 value=0000','index=5 value=0001'),
                        ('index=0 value=001e','index=1 value=001e')]:
            with self.subTest(old=old),self.assertRaises(ValueError):
                self.verify(fixture().replace(old,new))

    def test_illegal_scope_opcode_cap_and_terminal_fail(self):
        for old,new in [('illegal=0','illegal=1'),('opcode=f5e1','opcode=f4e1'),
                        ('events=7','events=1024'),('status_reply=1','status_reply=0'),
                        ('input_cursor=29','input_cursor=20'),('f3=0000','f3=0001')]:
            with self.subTest(old=old),self.assertRaises(ValueError):
                self.verify(fixture().replace(old,new))

    def test_unknown_console_failure_cannot_be_hidden(self):
        with self.assertRaises(RuntimeError):
            gate.check(fixture(),'Fatal error: '+gate.COMPLETION+'\n[LUA ERROR] failed\n')

    def test_observer_scope_must_remain_read_only_and_non_audio(self):
        for old,new in [('observer_writes=0','observer_writes=1'),
                        ('audio_acceptance=0','audio_acceptance=1'),
                        ('operands=context_only','operands=committed')]:
            with self.subTest(old=old),self.assertRaises(ValueError):
                self.verify(fixture().replace(old,new))


if __name__=='__main__':unittest.main()

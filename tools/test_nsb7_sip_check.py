import io
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import AsyncMock,patch
from tools.nsb7_sip_check import check,check_restore,check_incoming_restore,check_sip_frame
from tools.test_nsb7_host_outgoing_call_check import fixture
from tools.run_sip_handset_gate import main,verify_success,verify_restore,verify_handset_health


def restore_fixture():
    cpu=','.join(f'{value:08x}' for value in range(37))
    rows=[]
    for key,time in [('Keypad '+key,46+index) for index,key in enumerate('5551234')]+[('Send',54)]:
        code='0e' if key=='Send' else f'{int(key[-1]):02x}'
        rows.extend([f'nsb7_outgoing_call_input: key={key} state=1 t={time}.000000000',
                     f'nsb7_key_decode: key={code} observer_writes=0',
                     f'nsb7_outgoing_call_input: key={key} state=0 t={time}.200000000'])
    rows.extend(['gsm_call_adapter: request id=1 epoch=1 digits=5551234',
                 'gsm_call_adapter: state id=1 epoch=1 phase=connected'])
    for event in ('saved','restored'):
        rows.extend([f'nsb7_sip_state: event={event} pc=00200000 sp=00110000 ram=12345678 cpu={cpu} t=60.000000000',
                     f'sip_state: {event}'])
    rows.extend(['state_roundtrip: result=pass scenario=nsb7_sip_outgoing',
                 'gsm_call_adapter: request id=1 epoch=2 digits=5551234',
                 'gsm_call_adapter: state id=1 epoch=2 phase=connected',
                 'termination id=1 cause=41 result=accepted',
                 'GSM service downlink kind=13 sapi=0 pd=03 message=25',
                 'GSM service uplink sapi=0 pd=03 message=2d',
                 'GSM service downlink kind=26 sapi=0 pd=03 message=2a',
                 'LAPDm service Channel Release acknowledged',
                 'gsm_call_adapter: state id=1 epoch=2 phase=media_closed',
                 'gsm_call_adapter: state id=1 epoch=2 phase=ended'])
    for key,time,prefix,code in [('End',66,'nsb7_outgoing_call_input','0f'),
                               ('Left Softkey / Menu',70,'nsb7_sip_cleanup_input','19'),
                               ('End',73,'nsb7_sip_cleanup_input','0f')]:
        rows.extend([f'{prefix}: key={key} state=1 t={time}.000000000',
                     f'nsb7_key_decode: key={code} observer_writes=0',
                     f'{prefix}: key={key} state=0 t={time}.200000000'])
    rows.append('nsb7_outgoing_call_done: t=76.000000000')
    return '\n'.join(rows)


def incoming_restore_fixture():
    from tools.test_nsb7_host_incoming_call_check import fixture as incoming_fixture
    text=incoming_fixture().replace('length=19','length=17').replace(
        '4d030504046002008134015c0781447700091032',
        '45030504046002008134015c0581551532f4')
    architecture=restore_fixture().split('nsb7_sip_state: event=saved',1)[1].split(
        'gsm_call_adapter: request id=1 epoch=2',1)[0]
    architecture=('nsb7_sip_state: event=saved'+architecture).replace('60.000000000','55.000000000').replace(
        'scenario=nsb7_sip_outgoing','scenario=nsb7_sip_incoming')
    end='nsb7_call_input: key=End state=1 t=60.000000000'
    text=text.replace(end,architecture+'\ngsm_call_adapter: incoming state id=1 epoch=2 phase=connected\n'+end)
    text=text.replace('GSM service uplink sapi=0 pd=03 message=25 length=5 data=836502e090',
        'termination id=1 cause=41 result=accepted\nGSM service downlink kind=13 sapi=0 pd=03 message=25')
    text=text.replace('gsm_call_adapter: incoming state id=1 epoch=1 phase=media_closed',
        'LAPDm service Channel Release acknowledged\ngsm_call_adapter: incoming state id=1 epoch=2 phase=media_closed')
    text=text.replace('epoch=1 phase=ended','epoch=2 phase=ended')
    for key,time,code in [('Left Softkey / Menu',62,'19'),('End',65,'0f')]:
        text+=f'\nnsb7_sip_cleanup_input: key={key} state=1 t={time}.000000000\n'
        text+=f'nsb7_key_decode: key={code} observer_writes=0\n'
        text+=f'nsb7_sip_cleanup_input: key={key} state=0 t={time}.200000000\n'
    return text+'\nnsb7_sip_done: direction=incoming t=68.000000000'


class SipTest(unittest.TestCase):
    @patch('tools.nsb7_sip_check.check_authentication')
    def test_physical_call_without_probe_only_decisions(self,authentication):
        text=fixture()
        text='\n'.join(row for row in text.splitlines() if
                       'decision id=2' not in row and 'outcome=0 result=rejected' not in row)
        check(text)
        for bad in (text.replace('epoch=1 phase=ended','epoch=2 phase=ended'),
                    text.replace('key=0e observer_writes=0','key=0f observer_writes=0'),
                    text+'\ndsp_hle: speech tick',
                    text+'\nmedia_uplink_sequence=1',
                    text+'\ngsm_call_adapter: decision id=1 outcome=0 result=accepted'):
            with self.subTest(bad=bad),self.assertRaises(ValueError): check(bad)

    def test_cli_excludes_unproved_scenarios(self):
        base=['gate','--pjsua','unused','--run-dir','unused','--product','8290']
        for options in ([],['--incoming'],['--record-media'],['--restore-call'],
                        ['--restore-outgoing','--sip-response','180'],
                        ['--incoming','--restore-outgoing'],['--restore-idle'],['--calls','2'],
                        ['--sip-response','486']):
            flags=[] if not options else ['--signaling-only']
            with self.subTest(options=options),patch('sys.argv',base+flags+options+['--','true']), \
                    patch('sys.stderr',new_callable=io.StringIO),self.assertRaises(SystemExit) as error:
                main()
            self.assertEqual(error.exception.code,2)
        with patch('sys.argv',base+['--signaling-only','--','true']), \
                patch('tools.run_sip_handset_gate.run',new_callable=AsyncMock) as run:
            main()
        self.assertEqual(run.call_args.args[0].product,'8290')
        with patch('sys.argv',base+['--signaling-only','--restore-outgoing','--','true']), \
                patch('tools.run_sip_handset_gate.run',new_callable=AsyncMock) as run:
            main()
        self.assertTrue(run.call_args.args[0].restore_outgoing)
        with tempfile.TemporaryDirectory() as directory:
            with patch('sys.argv',base+['--signaling-only','--incoming',
                    '--ready-file',str(Path(directory)/'ready'),'--','true']), \
                    patch('tools.run_sip_handset_gate.run',new_callable=AsyncMock) as run:
                main()
            self.assertTrue(run.call_args.args[0].incoming)
            with patch('sys.argv',base+['--signaling-only','--incoming','--restore-call',
                    '--ready-file',str(Path(directory)/'ready'),'--','true']), \
                    patch('tools.run_sip_handset_gate.run',new_callable=AsyncMock) as run:
                main()
            self.assertTrue(run.call_args.args[0].restore_call)

    @patch('tools.nsb7_sip_check.check_authentication')
    def test_incoming_restore_requires_own_caller_answer_and_epoch(self,authentication):
        text=incoming_restore_fixture()
        check_incoming_restore(text)
        for bad in (text.replace('81551532f4','81551532f5',1),
                    text.replace('key=0e observer_writes=0','key=0f observer_writes=0'),
                    text.replace('epoch=2 phase=connected','epoch=1 phase=connected'),
                    text.replace('55.000000000','54.000000000',1),
                    text.replace('key=19 observer_writes=0','key=18 observer_writes=0'),
                    text.replace('pc=002645ec','pc=002645ee'),
                    text+'\ngsm_call_adapter: incoming id=2 result=accepted',
                    text+'\ngsm_call_adapter: media direction=uplink id=1 good=1'):
            with self.subTest(bad=bad[-100:]),self.assertRaises(ValueError): check_incoming_restore(bad)

    @patch('tools.nsb7_sip_check.check_frame')
    def test_only_reviewed_ringing_animation_gets_alternate_frame(self,frame):
        path=Path('nsb7_call_ringing.png')
        frame.side_effect=[ValueError('phase'),None]
        check_sip_frame(path,'primary',incoming=True)
        self.assertEqual(frame.call_count,2)
        frame.side_effect=ValueError('wrong pixels')
        with self.assertRaises(ValueError): check_sip_frame(path,'primary',incoming=True)
        frame.reset_mock()
        with self.assertRaises(ValueError): check_sip_frame(Path('final.png'),'primary',incoming=True)
        self.assertEqual(frame.call_count,1)

    @patch('tools.nsb7_sip_check.check_authentication')
    def test_incoming_sip_restore_requires_bye_one_clear_and_no_speech(self,authentication):
        text=incoming_restore_fixture()
        bridge='\n'.join(['SIP incoming identity=(1, 1) caller=5551234',
            'SIP physical answer identity=(1, 1)','SIP confirmed status=200 identity=(1, 1)',
            'SIP epoch changed old=1 new=2','SIP restored call cleared identity=(2, 1)',
            'SIP epoch media discarded {"uplink":0}'])
        remote='state changed to CONFIRMED\nRequest msg BYE/'
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            (root/'error.log').write_text(text)
            (root/'sip-bridge.log').write_text(bridge)
            verify_restore(root,remote,'connected','8290',signaling_only=True)
            result=json.loads((root/'sip-result.json').read_text())
            self.assertFalse(result['speech_tested'])
            self.assertFalse(result['native_dsp_complete'])
            for bad_bridge,bad_log,bad_remote in (
                    (bridge.replace('"uplink":0','"uplink":1'),text,remote),
                    (bridge+'\nSIP incoming identity=(2, 1)',text,remote),
                    (bridge,text+'\ntermination id=1 cause=41 result=accepted',remote),
                    (bridge,text,remote.replace('Request msg BYE/',''))):
                (root/'error.log').write_text(bad_log)
                (root/'sip-bridge.log').write_text(bad_bridge)
                with self.assertRaises((ValueError,RuntimeError)):
                    verify_restore(root,bad_remote,'connected','8290',signaling_only=True)

    @patch('tools.nsb7_sip_check.check_authentication')
    def test_connected_restore_requires_architecture_epoch_and_physical_recovery(self,authentication):
        text=restore_fixture()
        check_restore(text)
        check_restore(text+'\ngsm_call_adapter: media direction=uplink id=1 good=0\nmedia_uplink_sequence=178')
        for bad in (text.replace('event=restored pc=00200000','event=restored pc=00200002'),
                    text.replace('event=restored pc=00200000 sp=00110000 ram=12345678',
                                 'event=restored pc=00200000 sp=00110000 ram=12345679'),
                    text.replace(',00000024 t=', ' t='),
                    text.replace('epoch=2 digits','epoch=1 digits'),
                    text.replace('cause=41 result=accepted','cause=16 result=accepted'),
                    text.replace('key=19 observer_writes=0','key=18 observer_writes=0'),
                    text.replace('state=1 t=70.','state=1 t=71.'),
                    text+'\ngsm_call_adapter: media direction=uplink id=1 good=1',
                    text+'\ndsp_hle: speech tick',text+'\n[LUA ERROR] in resume'):
            with self.subTest(bad=bad[-100:]),self.assertRaises(ValueError):
                check_restore(bad)

    def test_handset_console_failures_cannot_pass_sip_gate(self):
        verify_handset_health('Driver nsb7hle: 0 errors, 1 warnings\nROM NEEDS REDUMP\n')
        for marker in ('[LUA ERROR] in resume: assertion failed!',
                       'Error running autoboot script', 'Fatal error: stopped',
                       'Disk quota exceeded', 'Error writing NVRAM file','Error generating PNG',
                       'Driver nsb7hle: 1 error, 0 warnings',
                       'Driver nsb7hle: 12 errors, 0 warnings'):
            with self.subTest(marker=marker),self.assertRaises(RuntimeError):
                verify_handset_health(marker)

    def test_incoming_identity_and_physical_answer(self):
        from tools.test_nsb7_host_incoming_call_check import fixture as incoming_fixture
        text=incoming_fixture().replace('length=19','length=17').replace(
            '4d030504046002008134015c0781447700091032',
            '45030504046002008134015c0581551532f4')
        with patch('tools.nsb7_host_incoming_call_check.check_authentication'):
            check(text,incoming=True)
            for bad in (text.replace('key=0e observer_writes=0','key=0f observer_writes=0'),
                        text.replace('81551532f4','81551532f5'),text+'\nmedia_uplink_sequence=1'):
                with self.subTest(bad=bad),self.assertRaises(ValueError):
                    check(bad,incoming=True)

    def test_real_sip_result_requires_explicit_no_speech_scope(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            bridge=('digits=5551234\nSIP confirmed status=200 identity=(1, 1)\n'
                    'SIP bridge ended {"uplink":0,"downlink":20}\n')
            (root/'sip-bridge.log').write_text(bridge)
            (root/'error.log').write_text('physical call')
            args=SimpleNamespace(product='8290',incoming=False,signaling_only=True)
            remote='state changed to CONFIRMED\nDISCONNECTED [reason=200 (Normal call clearing)]'
            with patch('tools.nsb7_sip_check.check') as product:
                verify_success(root,remote,args)
                product.assert_called_once_with('physical call')
            result=json.loads((root/'sip-result.json').read_text())
            self.assertFalse(result['speech_tested'])
            self.assertFalse(result['native_dsp_complete'])
            self.assertEqual(result['dialed_digits'],'5551234')
            (root/'sip-bridge.log').write_text(bridge.replace('"uplink":0','"uplink":1'))
            with patch('tools.nsb7_sip_check.check'),self.assertRaises(RuntimeError):
                verify_success(root,remote,args)

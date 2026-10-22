import unittest
import io
import json
import tempfile
from pathlib import Path
from unittest.mock import patch
from tools.noki6250_pin_retry_check import registered_card
from tools.run_noki6250_pin2_cold import check_cold_input
from tools import run_noki6250_pin2_cold as runner
from tools.run_noki6250_pin_retry_cold import validate_retained_launch


class ColdPin2InputTest(unittest.TestCase):
    def test_complete_puk2_retry_range_requires_exact_predecessor(self):
        for remaining in range(11):
            card=bytearray(registered_card())
            card[3492:3500]=b'8765\xff\xff\xff\xff'
            card[3517]=0
            card[3519]=remaining
            with self.subTest(remaining=remaining):
                check_cold_input(bytes(card),bytes(card),retries=0,puk2=remaining)
                with self.assertRaises(ValueError):
                    check_cold_input(bytes(card),bytes(card),retries=0,puk2=(remaining+1)%11)
        for invalid in (-1,11,False,'1'):
            with self.subTest(invalid=invalid),self.assertRaises(ValueError):
                check_cold_input(bytes(card),bytes(card),retries=0,puk2=invalid)

    def test_puk2_campaign_rejects_impossible_or_ambiguous_plans(self):
        for options in (['--puk2-rejections','1'],
                        ['--permanent-recheck'],
                        ['--exhaust-pin2','--puk2-rejections','11'],
                        ['--exhaust-pin2','--puk2-rejections','-1'],
                        ['--exhaust-pin2','--recover-puk2','--puk2-rejections','10'],
                        ['--exhaust-pin2','--recover-puk2','--wrong-puk2-first','--puk2-rejections','1']):
            with self.subTest(options=options),patch('sys.argv',['runner','unused',*options]), \
                    patch('sys.stderr',new_callable=io.StringIO),self.assertRaises(SystemExit) as error:
                runner.main()
            self.assertEqual(error.exception.code,2)

    def test_phase_command_binds_model_storage_roms_script_and_duration(self):
        command=runner.mame_command(Path('/mame'),Path('/repo'),Path('/capture/puk2'),Path('/repo/puk.lua'),90)
        self.assertEqual(command[:2],['/mame','nhm3hle'])
        for option,value in (('-rompath','/capture/puk2/roms;/repo/roms'),
                             ('-nvram_directory','nvram'),('-cfg_directory','cfg'),
                             ('-autoboot_script','/repo/puk.lua'),('-seconds_to_run','90')):
            self.assertEqual(command[command.index(option)+1],value)

    def test_retained_installation_binds_shared_fixture_and_actual_command(self):
        recorded={'phase':'install','command':['python','install'],
                  'invocation_id':'4511042c-2632-4b03-94a0-c80104462fd4',
                  'mame_sha256':'binary','fixture_base_sha256':'fixture',
                  'mame_command':['mame','nhm3hle']}
        fields=('phase','command','mame_sha256','fixture_base_sha256','mame_command')
        validate_retained_launch(recorded,recorded,None,set(),fields=fields)
        for key,value in (('fixture_base_sha256','other'),('mame_command',['mame','other'])):
            with self.subTest(key=key),self.assertRaises(ValueError):
                validate_retained_launch({**recorded,key:value},recorded,None,set(),fields=fields)

    def test_exit_evidence_is_required_for_unfinished_capture(self):
        identity='4511042c-2632-4b03-94a0-c80104462fd4'
        with tempfile.TemporaryDirectory() as directory:
            phase=Path(directory)
            with self.assertRaises(ValueError):
                runner.verify_process_completion(phase,identity,set())
            runner.verify_process_completion(phase,identity,{identity})
            runner.record_process_completion(phase,identity)
            runner.verify_process_completion(phase,identity,set())
            for code in (1,False,'0',None):
                (phase/'process_exit.json').write_text(json.dumps({'invocation_id':identity,'returncode':code}))
                with self.subTest(code=code),self.assertRaises(ValueError):
                    runner.verify_process_completion(phase,identity,{identity})
            (phase/'process_exit.json').write_text(json.dumps({'invocation_id':'other','returncode':0}))
            with self.assertRaises(ValueError):
                runner.verify_process_completion(phase,identity,{identity})

    def test_legacy_completion_requires_whole_unique_accepted_chain(self):
        identity='4511042c-2632-4b03-94a0-c80104462fd4'
        with tempfile.TemporaryDirectory() as directory:
            run=Path(directory)
            self.assertEqual(runner.completed_legacy_launches(run),set())
            manifest={'independent_processes':1,'phases':[{'invocation_id':identity}]}
            (run/'acceptance.json').write_text(json.dumps(manifest))
            self.assertEqual(runner.completed_legacy_launches(run),{identity})
            for broken in ([],{**manifest,'independent_processes':True},
                           {**manifest,'independent_processes':2},
                           {'independent_processes':2,'phases':manifest['phases']*2}):
                (run/'acceptance.json').write_text(json.dumps(broken))
                with self.subTest(broken=broken),self.assertRaises(ValueError):
                    runner.completed_legacy_launches(run)

    def test_wrong_puk2_requires_its_exhaustion_and_recovery_plan(self):
        with patch('sys.argv',['runner','unused','--wrong-puk2-first']), \
                patch('sys.stderr',new_callable=io.StringIO) as output:
            with self.assertRaises(SystemExit) as error:
                runner.main()
        self.assertEqual(error.exception.code,2)
        self.assertIn('--wrong-puk2-first requires --recover-puk2',output.getvalue())

    def test_wrong_puk2_allowance_is_not_regenerated_on_cold_input(self):
        card=bytearray(self.card())
        card[3517]=0
        card[3519]=9
        check_cold_input(bytes(card),bytes(card),retries=0,puk2=9)
        with self.assertRaises(ValueError):
            check_cold_input(bytes(card),bytes(card),retries=0)

    def test_puk2_recovery_requires_physical_exhaustion_plan(self):
        with patch('sys.argv',['runner','unused','--recover-puk2']), \
                patch('sys.stderr',new_callable=io.StringIO) as output:
            with self.assertRaises(SystemExit) as error:
                runner.main()
        self.assertEqual(error.exception.code,2)
        self.assertIn('--recover-puk2 requires --exhaust-pin2',output.getvalue())

    def test_blocked_and_recovered_inputs_retain_their_actual_credentials(self):
        blocked=bytearray(self.card())
        blocked[3517]=0
        check_cold_input(bytes(blocked),bytes(blocked),retries=0)
        recovered=bytearray(self.card())
        recovered[3492:3500]=b'2468\xff\xff\xff\xff'
        check_cold_input(bytes(recovered),bytes(recovered),pin2='2468')
        with self.assertRaises(ValueError):
            check_cold_input(bytes(recovered),bytes(recovered))

    def test_incompatible_phase_plans_are_rejected_before_launch(self):
        with patch('sys.argv',['runner','unused','--reject-first','--exhaust-pin2']), \
                patch('sys.stderr',new_callable=io.StringIO) as output:
            with self.assertRaises(SystemExit) as error:
                runner.main()
        self.assertEqual(error.exception.code,2)
        self.assertIn('select different phase plans',output.getvalue())

    def card(self):
        card=bytearray(registered_card())
        card[3492:3500]=b'8765\xff\xff\xff\xff'
        return bytes(card)

    def test_entire_installed_card_is_the_cold_input(self):
        check_cold_input(self.card(),self.card())

    def test_failed_allowance_is_retained_not_repaired(self):
        failed=bytearray(self.card())
        failed[3517]=2
        check_cold_input(bytes(failed),bytes(failed),retries=2)
        with self.assertRaises(ValueError):
            check_cold_input(bytes(failed),bytes(failed))
        with self.assertRaises(ValueError):
            check_cold_input(self.card(),bytes(failed),retries=2)

    def test_default_or_repaired_card_is_not_a_cold_installation(self):
        with self.assertRaises(ValueError):
            check_cold_input(registered_card(),self.card())
        with self.assertRaises(ValueError):
            check_cold_input(self.card(),registered_card())

    def test_equal_but_unrelated_mutations_are_not_accepted(self):
        for offset in (0,1604,3484,3492,3516,3517,len(self.card())-1):
            changed=bytearray(self.card())
            changed[offset]^=1
            with self.subTest(offset=offset),self.assertRaises(ValueError):
                check_cold_input(bytes(changed),bytes(changed))


if __name__=='__main__':unittest.main()

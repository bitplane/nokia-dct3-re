import unittest
import subprocess
import sys
from pathlib import Path

from tools.run_npe3_native import (check_execution, check_input_start, expected_menu_digest,
    ADC_P09_MENU_SHA256, ADC_BROADCAST_MENU_SHA256, ADC_SCH_MENU_SHA256,ADC_LU_MENU_SHA256)


class Native6210EvidenceTest(unittest.TestCase):
    def test_addressed_page_requires_bounded_explicit_raw_profile(self):
        script=Path(__file__).with_name('run_npe3_native.py')
        for args in (['--page-at','18'],['--raw-agch','--page-at','15'],
                     ['--raw-agch','--page-at','18','--seconds','60'],
                     ['--raw-agch','--page-at','23','--seconds','24']):
            result=subprocess.run([sys.executable,str(script),*args],capture_output=True,text=True)
            self.assertEqual(result.returncode,2)
            self.assertNotIn('Native6210 evidence:',result.stdout)
    def test_lu_oracle_is_explicit_and_other_compositions_remain_unchanged(self):
        self.assertEqual(expected_menu_digest('fcch-sch-broadcast','p09',True),ADC_LU_MENU_SHA256)
        for source,capture in (('silence','p09'),('fcch-sch','p09'),('fcch-sch-broadcast','immediate')):
            with self.assertRaises(ValueError):expected_menu_digest(source,capture,True)

    def test_record_fixture_requires_explicit_raw_agch_profile(self):
        script=Path(__file__).with_name('run_npe3_native.py')
        result=subprocess.run([sys.executable,str(script),'--provision-records'],capture_output=True,text=True)
        self.assertEqual(result.returncode,2)
        self.assertNotIn('Native6210 evidence:',result.stdout)

    def test_raw_agch_requires_explicit_timed_transmitter(self):
        script=Path(__file__).with_name('run_npe3_native.py')
        result=subprocess.run([sys.executable,str(script),'--raw-agch'],capture_output=True,text=True)
        self.assertEqual(result.returncode,2)
        self.assertNotIn('Native6210 evidence:',result.stdout)

    def test_transmit_candidate_requires_explicit_full_si_reference_composition(self):
        script=Path(__file__).with_name('run_npe3_native.py')
        for args in ([],['--adc-source','silence'],
                     ['--adc-source','fcch-sch-broadcast'],
                     ['--adc-source','fcch-sch-broadcast','--capture-start','p09']):
            result=subprocess.run([sys.executable,str(script),'--tx-release','p05',*args],capture_output=True,text=True)
            self.assertEqual(result.returncode,2)
            self.assertNotIn('Native6210 evidence:',result.stdout)

    def test_full_broadcast_oracle_does_not_replace_other_profiles(self):
        from tools.run_noki6210_acceptance import MENU_SHA256
        self.assertNotEqual(ADC_BROADCAST_MENU_SHA256, ADC_P09_MENU_SHA256)
        self.assertEqual(expected_menu_digest('fcch-sch-broadcast', 'p09'), ADC_BROADCAST_MENU_SHA256)
        for source in ('fcch-sch', 'fcch-sch-bcch'):
            self.assertEqual(expected_menu_digest(source, 'p09'), ADC_P09_MENU_SHA256)
        self.assertEqual(expected_menu_digest('fcch-sch', 'immediate'), ADC_SCH_MENU_SHA256)
        for source in (None, 'silence'):
            self.assertEqual(expected_menu_digest(source, 'p09'), MENU_SHA256)

    def test_physical_start_requires_actual_requested_time(self):
        marker = '6210_phonebook_physical: action=names'
        check_input_start(marker + ' t=60.000000', marker, 60)
        for text in (marker + ' t=20.000000', marker, (marker + ' t=60\n') * 2):
            with self.assertRaisesRegex(ValueError, 'horizon'):
                check_input_start(text, marker, 60)

    def setUp(self):
        self.flash = bytearray(0x26000)
        self.flash[0x24b7c:0x24b7c + 1258] = bytes([0xa5]) * 1258
        self.flash[0x1eac4:0x1eac4 + 844] = bytes([0x3c]) * 844
        self.loader = self.flash[0x24b7c:0x24b7c + 1258]
        self.program = bytearray(0x20000)
        self.program[0x590 * 2:0x736 * 2] = self.flash[0x1eac4:0x1eac4 + 844]
        self.text = '\n'.join((
            'npe3_native_composition: hle=0 staged=0 clock_policy=13000000 fitted_mask_claim=0 observer_writes=0',
            'npe3_native_control_contract: status=10 release=04 execution=01 readback=05 observer_writes=0',
            'npe3_native_verifier: result0=0000 result1=0006 observer_writes=0',
            'npe3_native_entry: kind=reset ordinal=1 observer_writes=0',
            'npe3_native_entry: kind=reset ordinal=2 observer_writes=0',
            'npe3_native_entry: kind=loader ordinal=1 observer_writes=0',
            'npe3_native_entry: kind=loader ordinal=2 observer_writes=0',
            'npe3_native_entry: kind=resident ordinal=1 observer_writes=0',
            'npe3_native_selftest: command=0d faults=00 flag=c4 observer_writes=0',
            'npe3_native_terminal: t=30.016667 arm=005078a8 dsp=003999 illegal=0 idle=1 pmst=07ac observer_writes=0',
            'native_dsp_terminal_timing: released=1 clock=13000000',
            'npe3_native_coverage: missing_program_reads=0 observer_writes=0',
        ))

    def check(self, text=None, loader=None, program=None):
        check_execution(self.text if text is None else text,
                        self.loader if loader is None else loader,
                        self.program if program is None else program, self.flash)

    def test_complete_own_uploads(self):
        self.check()

    def test_reference_clock_requires_explicit_experiment_identity(self):
        text = self.text.replace('clock_policy=13000000', 'clock_policy=45500000').replace(
            'released=1 clock=13000000', 'released=1 clock=45500000')
        with self.assertRaises(ValueError):
            check_execution(text, self.loader, self.program, self.flash, reference=True)
        text += '\nnpe3_native_reference_clock: source=8210_documented_rate fitted_clock_claim=0 experiment=1 observer_writes=0'
        check_execution(text, self.loader, self.program, self.flash, reference=True)
        with self.assertRaises(ValueError):
            self.check(text=text)

    def test_horizon_and_actual_terminal_clock_are_required(self):
        with self.assertRaisesRegex(ValueError, 'horizon'):
            check_execution(self.text, self.loader, self.program, self.flash, minimum_seconds=65)
        with self.assertRaisesRegex(ValueError, 'clock'):
            self.check(text=self.text.replace('released=1 clock=13000000', 'released=1 clock=45500000'))

    def test_restarts_missing_terminal_and_failed_contracts_reject(self):
        cases = [self.text + '\nnpe3_native_entry: kind=reset ordinal=3',
                 self.text + '\n' + self.text,
                 self.text.replace('illegal=0', 'illegal=1'),
                 self.text.replace('faults=00', 'faults=01'),
                 self.text.replace('missing_program_reads=0', 'missing_program_reads=1'),
                 self.text.replace('hle=0', 'hle=1'),
                 self.text.replace('readback=05', 'readback=00'),
                 self.text.replace('npe3_native_terminal:', 'missing_terminal:'),
                 self.text + '\nLua error: observer failed']
        for text in cases:
            with self.subTest(text=text), self.assertRaises(ValueError):
                self.check(text=text)

    def test_foreign_or_incomplete_uploads_reject(self):
        with self.assertRaises(ValueError):
            self.check(loader=self.loader[:-2])
        for index in (0x590 * 2, 0x736 * 2 - 1):
            changed = self.program.copy()
            changed[index] ^= 1
            with self.subTest(index=index), self.assertRaises(ValueError):
                self.check(program=changed)


if __name__ == '__main__':
    unittest.main()

import unittest
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path

from tools.run_npe3_native import (check_execution, check_input_start, expected_menu_digest,
    ADC_P09_MENU_SHA256, ADC_BROADCAST_MENU_SHA256, ADC_SCH_MENU_SHA256,ADC_LU_MENU_SHA256,
    configure_cell_power,check_cell_power)


class Native6210EvidenceTest(unittest.TestCase):
    def test_frame_pages_are_separate_bounded_external_input(self):
        script=Path(__file__).with_name('run_npe3_native.py')
        result=subprocess.run([sys.executable,str(script),'--frame-pages'],capture_output=True,text=True)
        self.assertEqual(result.returncode,2)
        self.assertNotIn('Native6210 evidence:',result.stdout)
        with tempfile.TemporaryDirectory() as tmp:
            directory=Path(tmp)
            configure_cell_power(directory,'npe3ap5','handset33',frame_pages=True)
            ports=ET.parse(directory/'cfg/npe3ap5.cfg').findall('./system/input/port')
            self.assertEqual([(p.get('mask'),p.get('value')) for p in ports],[('2','2'),('1024','1024')])

    def test_outgoing_scout_is_bounded_and_excludes_incoming_inputs(self):
        script=Path(__file__).with_name('run_npe3_native.py')
        for args in ([],['--seconds','36'],['--seconds','36','--cell-power-policy','handset33'],
                     ['--seconds','36','--cell-power-policy','handset33','--sacch','--incoming-call']):
            result=subprocess.run([sys.executable,str(script),'--scenario','outgoing',*args],capture_output=True,text=True)
            self.assertEqual(result.returncode,2)
            self.assertNotIn('Native6210 evidence:',result.stdout)
        result=subprocess.run([sys.executable,str(script),'--accept-outgoing-service'],capture_output=True,text=True)
        self.assertEqual(result.returncode,2)
        result=subprocess.run([sys.executable,str(script),'--proceed-outgoing-call'],capture_output=True,text=True)
        self.assertEqual(result.returncode,2)
        with tempfile.TemporaryDirectory() as tmp:
            directory=Path(tmp)
            configure_cell_power(directory,'npe3ap5','handset33',sacch=True,accept_outgoing_service=True,proceed_outgoing_call=True,traffic_assignment=True)
            ports=ET.parse(directory/'cfg/npe3ap5.cfg').findall('./system/input/port')
            self.assertEqual([(p.get('mask'),p.get('value')) for p in ports],[('2','2'),('8','8'),('64','64'),('256','256'),('512','512')])

    def test_call_input_is_separate_and_requires_positive_paging_fixture(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory=Path(tmp)
            configure_cell_power(directory,'npe3ap5','handset33',incoming_call=True)
            ports=ET.parse(directory/'cfg/npe3ap5.cfg').findall('./system/input/port')
            self.assertEqual([(p.get('mask'),p.get('value')) for p in ports],[('2','2'),('4','4')])
        with tempfile.TemporaryDirectory() as tmp:
            directory=Path(tmp)
            configure_cell_power(directory,'npe3ap5','handset33',incoming_call=True,traffic_assignment=True)
            ports=ET.parse(directory/'cfg/npe3ap5.cfg').findall('./system/input/port')
            self.assertEqual([(p.get('mask'),p.get('value')) for p in ports],[('2','2'),('4','4'),('8','8')])
        script=Path(__file__).with_name('run_npe3_native.py')
        for args in ([],['--cell-power-policy','handset33'],['--page-at','18']):
            result=subprocess.run([sys.executable,str(script),'--incoming-call',*args],capture_output=True,text=True)
            self.assertEqual(result.returncode,2)
            self.assertNotIn('Native6210 evidence:',result.stdout)
        result=subprocess.run([sys.executable,str(script),'--traffic-assignment'],capture_output=True,text=True)
        self.assertEqual(result.returncode,2)
        self.assertNotIn('Native6210 evidence:',result.stdout)
        for args in (['--reject-call'], ['--reject-call','--incoming-call','--traffic-assignment'],
                     ['--ack-call-control'],['--ack-call-control','--incoming-call','--traffic-assignment'],
                     ['--clear-rejected-call'],['--clear-rejected-call','--reject-call','--incoming-call'],
                     ['--sacch'],['--sacch','--raw-agch'],['--answer-call'],
                     ['--answer-call','--reject-call','--incoming-call'],
                     ['--assignment-after-connect'],['--assignment-after-connect','--traffic-assignment','--incoming-call']):
            result=subprocess.run([sys.executable,str(script),*args],capture_output=True,text=True)
            self.assertEqual(result.returncode,2)
            self.assertNotIn('Native6210 evidence:',result.stdout)
        with tempfile.TemporaryDirectory() as tmp:
            directory=Path(tmp)
            configure_cell_power(directory,'npe3ap5','handset33',incoming_call=True,call_ack=True,clear_call=True,sacch=True)
            ports=ET.parse(directory/'cfg/npe3ap5.cfg').findall('./system/input/port')
            self.assertEqual([(p.get('mask'),p.get('value')) for p in ports],[('2','2'),('4','4'),('16','16'),('32','32'),('64','64')])
        with tempfile.TemporaryDirectory() as tmp:
            directory=Path(tmp)
            configure_cell_power(directory,'npe3ap5','handset33',incoming_call=True,
                                 traffic_assignment=True,call_ack=True,sacch=True,assignment_after_connect=True)
            ports=ET.parse(directory/'cfg/npe3ap5.cfg').findall('./system/input/port')
            self.assertEqual([(p.get('mask'),p.get('value')) for p in ports],
                             [('2','2'),('4','4'),('8','8'),('16','16'),('64','64'),('128','128')])

    def test_cell_power_experiment_is_explicit_and_broadcast_in_both_messages(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory=Path(tmp)
            configure_cell_power(directory,'npe3ap5','legacy39')
            self.assertFalse((directory/'cfg').exists())
            configure_cell_power(directory,'npe3ap5','handset33')
            port=ET.parse(directory/'cfg/npe3ap5.cfg').find('./system/input/port')
            self.assertEqual(port.attrib,dict(tag=':LAB_BTS',type='CONFIG',mask='2',defvalue='0',value='2'))
        lines=['npe3_bts_initial_ms_power: level=5 gsm900_dbm=33 external_cell_policy=1 firmware_writes=0']
        for index,offset,message in ((2,14,0x1b),(3,8,0x1c)):
            octets=bytearray(23);octets[2]=message;octets[offset]=5
            lines.append(f'rf_adc_broadcast_si: index={index} arfcn=35 octets={octets.hex()} physical_claim=0')
        text='\n'.join(lines)
        self.assertTrue(check_cell_power(text,'handset33')['external_network_input'])
        with self.assertRaises(ValueError):check_cell_power(text,'legacy39')
        with self.assertRaises(ValueError):check_cell_power('\n'.join(lines[:-1]),'handset33')
        with self.assertRaises(ValueError):check_cell_power(text.replace('gsm900_dbm=33','gsm900_dbm=39'),'handset33')

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
                 self.text+'\n[:dsp_c54x:cpu] unmapped program memory read from 01A50D',
                 self.text+'\nnpe3_native_program_escape: address=01a50d',
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

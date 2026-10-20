import hashlib
import copy
import json
from pathlib import Path
import tempfile
import unittest
import xml.etree.ElementTree as ET
from unittest.mock import patch

from tools.nsk1_rom4_compare import prepare,summarize,check_nominal_startup,check_card_startup


class Nsk1CompareTest(unittest.TestCase):
    def test_card_gate_requires_dcs_reads_complete_scan_and_prompt(self):
        from PIL import Image
        with tempfile.TemporaryDirectory() as directory:
            snapshots = Path(directory)
            frame = Image.new('RGB',(84,48),'white')
            digest = hashlib.sha256(frame.tobytes()).hexdigest()
            for seconds in (10,20):
                frame.save(snapshots/f'nsk1_native_{seconds}s.png')
            lines = ['sim_device: body ins=a4 length=2 requested=7f21 selected=7f21 result=ok']
            for fid,length in (('2fe2','0a'),('6fae','01'),('6f38','08'),('6f07','09')):
                lines.append(f'sim_device: header cla=a0 ins=b0 p1=00 p2=00 p3={length} selected={fid}')
            lines.extend(f'sim_device: header cla=a0 ins=b2 p1={index:02x} p2=04 p3=20 selected=6f3a'
                         for index in range(1,51))
            trace = '\n'.join(lines)
            with patch('tools.nsk1_rom4_compare.check_nominal_readiness') as readiness, \
                    patch('tools.nsk1_rom4_compare.SECURITY_PROMPT_RGB_SHA256',digest):
                self.assertEqual(50,check_card_startup({},trace,snapshots)['adn_records_read'])
                readiness.assert_called_once_with({})
                for changed in (trace.replace('selected=6f07','selected=6f05'),
                                trace.replace('p1=32','p1=31'),trace+'\nsim_device: header malformed',
                                trace+'\nSIM status ins=b2 sw=9402',trace.replace('result=ok','result=not-found')):
                    with self.assertRaises(ValueError):
                        check_card_startup({},changed,snapshots)
                Image.new('RGB',(84,48),'black').save(snapshots/'nsk1_native_20s.png')
                with self.assertRaisesRegex(ValueError,'pixels'):
                    check_card_startup({},trace,snapshots)

    def test_nominal_graphical_gate_requires_pixels_reports_and_physical_keys(self):
        from PIL import Image
        with tempfile.TemporaryDirectory() as directory:
            snapshots = Path(directory)
            frame = Image.new('RGB',(84,48),'white')
            digest = hashlib.sha256(frame.tobytes()).hexdigest()
            for seconds in (10,20):
                frame.save(snapshots/f'nsk1_native_{seconds}s.png')
            report = {'adc2_source':554,'native_illegal_observed':False,
                      'physical_input_observation':{'contacts':[{'checklist':15,'irq_mask':0xca,'simi_reads':12}]*6,
                       'task1_scalars':[{'value':0x14},{'value':0x15}], 'report14_countdowns':[1,0],
                       'forwarded_keys':[0x19,0x17,0x1a], 'power_windows':[{'sample':3581}]}}
            with patch('tools.nsk1_rom4_compare.EMPTY_SOCKET_RGB_SHA256',digest):
                self.assertIn('not fitted',check_nominal_startup(report,snapshots)['scope'])
                for key,value in (('task1_scalars',[{'value':0x15}]),('forwarded_keys',[]),
                                  ('report14_countdowns',[1]),('power_windows',[{'sample':6614}]),
                                  ('contacts',[{'checklist':14,'irq_mask':0xce,'simi_reads':0}]*6)):
                    changed = copy.deepcopy(report)
                    changed['physical_input_observation'][key] = value
                    with self.subTest(key=key),self.assertRaises(ValueError):
                        check_nominal_startup(changed,snapshots)
                changed = copy.deepcopy(report)
                changed['native_illegal_observed'] = True
                with self.assertRaises(ValueError):
                    check_nominal_startup(changed,snapshots)
                Image.new('RGB',(84,48),'black').save(snapshots/'nsk1_native_20s.png')
                with self.assertRaisesRegex(ValueError,'pixels'):
                    check_nominal_startup(report,snapshots)

    def test_observation_checks_own_payload_and_does_not_claim_boot(self):
        image = bytearray(0x100000)
        payload = bytes(range(186))*2
        image[0x9c2f0:0x9c2f0+372] = payload
        image[0x9728c:0x97296] = bytes.fromhex('fd00ff80024405000078')
        trace = '\n'.join(f'[luaengine] nsk1_boot: t={time} pc=00289406 dsp_pc=408d illegal=0'
                          for time in (0.1,0.5,1,2,5))
        trace += '\n'+'\n'.join(f'nsk1_presentation: t={time} commands=20 data=504 nonzero=504 startup_reason=0a idle_flag=01 observer_writes=0'
                                for time in (0.1,0.5,1,2,5))
        release = ('[luaengine] nsk1_release: index=1 command=01 pc=00273504 '
                   f'args=0100030000008000000100010200 program={payload.hex()}')
        trace += '\n'+release
        report = summarize(trace,image,'standard')
        self.assertEqual(1,report['releases'])
        self.assertTrue(report['own_slot25_payload_observed'])
        self.assertFalse(report['descriptor0_release_observed'])
        self.assertFalse(report['graphical_or_fitted_hardware_proved'])
        self.assertEqual(504,report['lcd_nonzero_bytes'])
        decision = '\nnsk1_record_decision: pointer=00101bc8 state=00000002 valid=00000000 observer_writes=0'
        self.assertEqual([{'pointer':'00101bc8','state':2,'valid':0}],
                         summarize(trace+decision,image,'standard')['record_decisions'])
        accepted = '\nnsk1_record_accept: context=0010ba34 valid=00000001 copied='+('00'*24)+' observer_writes=0'
        self.assertEqual([{'context':'0010ba34','copied':'00'*24}],
                         summarize(trace+accepted,image,'standard')['record_accepts'])
        late = '\nnsk1_late: t=20.000000000 pc=00289408 dsp_pc=408d illegal=1 startup_reason=0a idle_flag=01 commands=68 data=2016 nonzero=504 observer_writes=0'
        self.assertTrue(summarize(trace+late,image,'standard')['native_illegal_observed'])
        self.assertEqual(20.0,summarize(trace+late,image,'standard')['late_samples'][0]['time'])
        second = '\nnsk1_release: index=2 command=01 pc=00273786 args=fd00ff8002440500007800000000 program=00'
        self.assertTrue(summarize(trace+second,image,'standard')['descriptor0_release_observed'])
        for bad in (trace.replace('00273504','00273506'),
                    trace.replace(payload.hex(),'00'),
                    trace.replace('t=5 ','t=4 '),
                    trace+'\n[LUA ERROR]',
                    trace.replace('nsk1_presentation: t=5','nsk1_presentation: t=6'),
                    trace+second.replace('fd00ff80','fd01ff80')):
            with self.assertRaises(ValueError):
                summarize(bad,image,'standard')
        self.assertTrue(summarize(trace.replace('illegal=0','illegal=1'),image,'standard')['native_illegal_observed'])

    def test_pinned_inputs_only_and_fresh_storage(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            sources = root / 'sources'
            sources.mkdir()
            inputs = []
            for index in range(4):
                data = bytes([index])*16
                name = f'input{index}.bin'
                (sources/name).write_bytes(data)
                inputs.append((name,hashlib.sha1(data).hexdigest()))
            run = root/'run'
            with patch('tools.nsk1_rom4_compare.INPUTS',inputs), \
                    patch('tools.nsk1_rom4_compare.inspect') as inspect:
                prepare(sources,run)
                self.assertEqual(['standard','2915'],[call.args[1] for call in inspect.call_args_list])
                self.assertFalse((run/'nvram').exists())
                manifest = json.loads((run/'input_manifest.json').read_text())
                self.assertFalse(manifest['peer_completions'])
                self.assertEqual(4,len(list((run/'inputs/nsk1r4t').iterdir())))
                with self.assertRaisesRegex(ValueError,'fresh'):
                    prepare(sources,run)
                profile = bytes(2048)
                with patch('tools.nsk1_rom4_compare.build_from_capture',return_value=(profile,{'classification':'diagnostic'})):
                    probe = root/'probe'
                    self.assertEqual(profile,prepare(sources,probe,run))
                    self.assertEqual(profile,(probe/'nvram/nsk1r4t/eeprom').read_bytes())
                    self.assertEqual({'classification':'diagnostic'},json.loads((probe/'input_manifest.json').read_text())['record_profile'])
                    checksum_probe = root/'checksum'
                    repaired = prepare(sources,checksum_probe,run,True,True)
                    self.assertEqual(profile[:0x11e],repaired[:0x11e])
                    self.assertEqual(profile[0x120:],repaired[0x120:])
                    metadata = json.loads((checksum_probe/'input_manifest.json').read_text())['security_checksum']
                    self.assertEqual('011e..011f',metadata['written_range'])
                    self.assertEqual(hashlib.sha256(repaired).hexdigest(),metadata['eeprom_sha256'])
                    nominal = root/'nominal'
                    self.assertEqual(repaired,prepare(sources,nominal,run,True,True,True))
                    port = ET.parse(nominal/'cfg/nsk1r4t.cfg').find('./system/input/port')
                    self.assertEqual(':RESEARCH_ADC2',port.get('tag'))
                    self.assertEqual('554',port.get('value'))
                    self.assertEqual(554,json.loads((nominal/'input_manifest.json').read_text())['adc2_source'])
                    inserted = root/'inserted'
                    prepare(sources,inserted,run,True,True,True,True)
                    ports = ET.parse(inserted/'cfg/nsk1r4t.cfg').findall('./system/input/port')
                    socket = next(port for port in ports if port.get('tag') == ':SIM_REMOVED')
                    self.assertEqual('0',socket.get('value'))
                    self.assertTrue(json.loads((inserted/'input_manifest.json').read_text())['inserted_synthetic_sim'])
                with self.assertRaisesRegex(ValueError,'qualified record'):
                    prepare(sources,root/'invalid-checksum',security_checksum=True)
                with self.assertRaisesRegex(ValueError,'checksum control'):
                    prepare(sources,root/'invalid-adc',nominal_adc2=True)
                with self.assertRaisesRegex(ValueError,'nominal-input control'):
                    prepare(sources,root/'invalid-card',insert_sim=True)
                (sources/'input3.bin').write_bytes(b'wrong mask')
                with self.assertRaisesRegex(ValueError,'hash'):
                    prepare(sources,root/'rejected')
                self.assertFalse((root/'rejected').exists())


if __name__ == '__main__':
    unittest.main()

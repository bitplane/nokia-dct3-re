import hashlib
import copy
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.dct3_rom4_record_codec import decode_record,retained_record_transform,LOCK_TABLE_82,ENCODE_SCHEDULE
from tools.nsk1_record_profile import build_profile,build_from_capture,check_observation,EXAMPLE_RECORDS,repair_security_checksum,initialize_phone_retry


class Nsk1RecordProfileTest(unittest.TestCase):
    def test_retry_initialization_only_changes_decoded_field(self):
        original = bytes([255])*2048
        with patch('tools.nsk1_record_profile.phonecode_contract',return_value={'retry_eeprom':'0x2a4'}):
            result = initialize_phone_retry(original,b'own flash')
        self.assertEqual([0x2a4],[index for index,(before,after) in enumerate(zip(original,result)) if before != after])
        self.assertEqual(0,result[0x2a4])
        with self.assertRaises(ValueError):
            initialize_phone_retry(original[:-1],b'')
    def test_security_checksum_changes_only_checksum_and_excludes_word_74(self):
        image = bytes([255])*2048
        repaired = repair_security_checksum(image)
        self.assertEqual(bytes.fromhex('db24'),repaired[0x11e:0x120])
        self.assertEqual(image[:0x11e],repaired[:0x11e])
        self.assertEqual(image[0x120:],repaired[0x120:])
        changed = bytearray(image)
        changed[0x74:0x76] = bytes(2)
        self.assertEqual(repaired[0x11e:0x120],repair_security_checksum(changed)[0x11e:0x120])
        changed[0x40] = 0
        self.assertEqual(0xda25,int.from_bytes(repair_security_checksum(changed)[0x11e:0x120],'big'))
        self.assertEqual(repaired,repair_security_checksum(repaired))
        with self.assertRaises(ValueError):
            repair_security_checksum(image[:-1])

    def test_acceptance_requires_copy_native_execution_and_unchanged_low_storage(self):
        expected = b''.join(EXAMPLE_RECORDS[start:start+10]+bytes(2) for start in (0,12)).hex()
        report = {'record_accepts':[{'copied':expected}], 'startup_reasons':['0a']*5,
                  'releases':2,'native_illegal_observed':False,
                  'record_arithmetic':{'exchanges':[{'decoded':expected,'private_markers':['54c2']*2}]}}
        image = build_profile(bytes.fromhex('00160010'))
        check_observation(report,image,image)
        for key,value in (('record_accepts',[]),('startup_reasons',['04']*5),('releases',3),
                          ('native_illegal_observed',True),('record_arithmetic',{'exchanges':[]})):
            changed = copy.deepcopy(report)
            changed[key] = value
            with self.assertRaises(ValueError):
                check_observation(changed,image,image)
        for final in (image[:-1],bytes(2048)):
            with self.assertRaises(ValueError):
                check_observation(report,image,final)

    def test_only_declared_external_records_and_checksum_change(self):
        chip = bytes.fromhex('00160010')
        image = build_profile(chip)
        self.assertEqual(2048,len(image))
        allowed = set(range(0x20,0x38)) | set(range(0x3c,0x40))
        self.assertTrue(all(value == 255 or index in allowed for index,value in enumerate(image)))
        self.assertEqual(sum(image[:0x3c]) & 0xffff,int.from_bytes(image[0x3c:0x40],'big'))
        staged = retained_record_transform(image[0x20:0x38],image[:12])
        self.assertEqual(EXAMPLE_RECORDS,b''.join(decode_record(staged[start:start+12],chip) for start in (0,12)))
        with self.assertRaises(ValueError):
            build_profile(bytes(3))

    def source(self, root):
        inputs = root/'inputs/nsk1r4t'
        inputs.mkdir(parents=True)
        drom = bytearray(0x20000)
        drom[0xb6df*2:0xb6df*2+12] = LOCK_TABLE_82
        drom[0xb6f7*2:0xb6f7*2+24] = b''.join(word.to_bytes(2,'big') for word in ENCODE_SCHEDULE)
        drom[0xb703*2:0xb703*2+2] = bytes.fromhex('54c2')
        expected = []
        for name,data in (('own.flash',b'own MCU'),('nse1_rom4_dsp_data.bin',drom)):
            (inputs/name).write_bytes(data)
            expected.append((name,hashlib.sha1(data).hexdigest()))
        manifest = {'inputs':[{'source':name,'sha1':digest} for name,digest in expected],
                    'peer_completions':False,'physical_keys':False,
                    'external_eeprom':'own erased 2048 bytes; no donor provisioning'}
        (root/'input_manifest.json').write_text(json.dumps(manifest))
        (root/'summary.json').write_text(json.dumps({'variant':'standard','unchanged_flash':True,
            'own_slot25_payload_observed':True,'native_illegal_observed':False}))
        storage = root/'nvram/nsk1r4t'
        storage.mkdir(parents=True)
        (storage/'flash').write_bytes(b'own MCU')
        (storage/'eeprom').write_bytes(bytes([255])*2048)
        (root/'error.log').write_text('captured own exchange')
        return expected,manifest

    def test_capture_provenance_and_stable_chip_are_required(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory)
            expected,manifest = self.source(source)
            with patch('tools.nsk1_record_profile.check',return_value={'exchanges':[{'chip_field':'00160010'}]*2}) as check:
                image,metadata = build_from_capture(source,expected)
                self.assertEqual(build_profile(bytes.fromhex('00160010')),image)
                self.assertEqual('00160010',metadata['chip_field'])
                self.assertEqual(bytes([255])*2048,check.call_args.kwargs['initial_eeprom'])
                manifest['peer_completions'] = True
                (source/'input_manifest.json').write_text(json.dumps(manifest))
                with self.assertRaisesRegex(ValueError,'qualified'):
                    build_from_capture(source,expected)
                manifest['peer_completions'] = False
                (source/'input_manifest.json').write_text(json.dumps(manifest))
                manifest['physical_keys'] = True
                (source/'input_manifest.json').write_text(json.dumps(manifest))
                with self.assertRaisesRegex(ValueError,'qualified'):
                    build_from_capture(source,expected)
                manifest['physical_keys'] = False
                (source/'input_manifest.json').write_text(json.dumps(manifest))
                (source/'summary.json').write_text(json.dumps({'variant':'2915','unchanged_flash':True}))
                with self.assertRaisesRegex(ValueError,'standard-family'):
                    build_from_capture(source,expected)
                (source/'summary.json').write_text(json.dumps({'variant':'standard','unchanged_flash':True,
                    'own_slot25_payload_observed':True,'native_illegal_observed':False}))
                check.return_value = {'exchanges':[{'chip_field':'00160010'},{'chip_field':'00000000'}]}
                with self.assertRaisesRegex(ValueError,'stable'):
                    build_from_capture(source,expected)
                (source/'inputs/nsk1r4t/own.flash').write_bytes(b'different MCU')
                with self.assertRaisesRegex(ValueError,'changed'):
                    build_from_capture(source,expected)


if __name__ == '__main__':
    unittest.main()

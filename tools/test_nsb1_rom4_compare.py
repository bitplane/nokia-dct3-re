from pathlib import Path
import struct
import hashlib
import json
import tempfile
import unittest
from unittest.mock import patch
import xml.etree.ElementTree as ET

from tools.nsb1_rom4_compare import checksum_control, check_checksum_control, check_late_presentation, check_lcd, check_trace, check_security_gate, check_menu, check_native_execution, check_receive_activity, prepare


class NativeCompareTest(unittest.TestCase):
    def test_ppm_selection_pins_own_variant_and_preserves_resident_inputs(self):
        from tools.nsb1_rom4_compare import INPUTS, inputs_for_ppm, storage_for_ppm
        self.assertEqual(INPUTS, inputs_for_ppm('A'))
        variant = inputs_for_ppm('B')
        self.assertEqual(('noki5190/5190f671b.fls',
                          '49987333223932f13dd4fb8e71a9645497894f73'), variant[0])
        self.assertEqual(INPUTS[1:], variant[1:])
        self.assertEqual('nsb1r4t', storage_for_ppm('A'))
        self.assertEqual('nsb1r4t_1', storage_for_ppm('B'))
        with self.assertRaisesRegex(ValueError, 'selection'):
            inputs_for_ppm('C')

    def test_b_checksum_source_and_storage_are_variant_specific(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            image = b'own B test image'
            (root / 'own-b.bin').write_bytes(image)
            inputs = (('own-b.bin', hashlib.sha1(image).hexdigest()),)
            source = root / 'source'
            storage = source / 'nvram/nsb1r4t_1'
            storage.mkdir(parents=True)
            (storage / 'flash').write_bytes(image)
            (storage / 'eeprom').write_bytes(b'\xff' * 8192)
            manifest = {'inputs': [{'source': inputs[0][0], 'sha1': inputs[0][1]}],
                        'external_eeprom': 'erased 8 KiB; no donor storage'}
            (source / 'input_manifest.json').write_text(json.dumps(manifest))
            with patch('tools.nsb1_rom4_compare.inputs_for_ppm', return_value=inputs):
                run = root / 'accepted'
                prepare(root, run, source, True, True, ppm='B')
                self.assertTrue((run / 'nvram/nsb1r4t_1/eeprom').exists())
                self.assertFalse((run / 'nvram/nsb1r4t/eeprom').exists())
                dictionary = root / 'dictionary'
                prepare(root, dictionary, source, True, True, ppm='B', english_dictionary=True)
                before = (run / 'nvram/nsb1r4t_1/eeprom').read_bytes()
                after = (dictionary / 'nvram/nsb1r4t_1/eeprom').read_bytes()
                self.assertEqual([0x4c7], [i for i, pair in enumerate(zip(before, after)) if pair[0] != pair[1]])
                self.assertEqual(1, after[0x4c7])
                self.assertEqual(1, json.loads((dictionary / 'input_manifest.json').read_text())['synthetic_dictionary_language'])
                manifest['inputs'][0]['sha1'] = 'wrong variant'
                (source / 'input_manifest.json').write_text(json.dumps(manifest))
                with self.assertRaisesRegex(ValueError, 'qualified own'):
                    prepare(root, root / 'rejected', source, True, True, ppm='B')
                self.assertFalse((root / 'rejected').exists())

    def test_dictionary_fixture_requires_qualified_variant_and_retry_control(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for ppm, retry in (('A', True), ('B', False)):
                with self.assertRaisesRegex(ValueError, 'dictionary fixture'):
                    prepare(root, root / 'run', initialized_retry=retry, ppm=ppm, english_dictionary=True)
                self.assertFalse((root / 'run').exists())

    def test_native_execution_is_separate_from_bootstrap(self):
        sample = 'nsb1_native_execution: t=5.0 pc=31a5 illegal=0 idle=1 tx=0041/0041 rx=00a6/00a6 observer_writes=0'
        check_native_execution(sample)
        with self.assertRaisesRegex(ValueError, 'observations'):
            check_native_execution(sample, minimum_time=34)
        late = sample + '\n' + sample.replace('t=5.0', 't=34.0')
        activity = '\nnsb1_activity_check: count=0001 pc=002aff04 t=31.0 observer_writes=0'
        check_native_execution(late, minimum_time=34)
        check_receive_activity(late + activity, minimum_time=34)
        for trace in (late, late + activity.replace('count=0001', 'count=0000')):
            with self.assertRaisesRegex(ValueError, 'receive activity'):
                check_receive_activity(trace, minimum_time=34)
        long = late + '\n' + sample.replace('t=5.0', 't=89.0')
        with self.assertRaisesRegex(ValueError, 'receive activity'):
            check_receive_activity(long + activity, minimum_time=89)
        check_receive_activity(long + activity + activity.replace('t=31.0', 't=62.0'), minimum_time=89)
        for trace in ('', sample.replace('5.0', '0.5'), sample.replace('illegal=0', 'illegal=1')):
            with self.subTest(trace=trace), self.assertRaises(ValueError):
                check_native_execution(trace)

    program3 = b'\x12\x34' * 186
    program4 = b'\x56\x78' * 186

    def trace(self):
        lines = ['Soft reset']
        for index, program in enumerate((self.program3, self.program4), 1):
            lines.append('nsb1_dsp_release: command=01 arguments=0100030000008000000100010200 pc=0028f9c8')
            lines.append(f'nsb1_release_program: release={index} words=186 data={program.hex()} observer_writes=0')
            for data, mask, pc in (('00040000', 'ffff0000', '0028f9dc'),
                                   ('00040000', 'ffff0000', '0028fa08'),
                                   (f'{index + 2:08x}', '0000ffff', '0028fa0c')):
                lines.append(f'nsb1_dsp_result_read: address=00010004 data={data} mask={mask} pc={pc}')
        lines.append('nsb1_dsp_release: command=01 arguments=fd00ff8002440500007800000000 pc=0028fc50')
        for data in ('0000ffff', '00000004'):
            lines.append(f'nsb1_dsp_result_read: address=00010000 data={data} mask=0000ffff pc=0028fa96')
        for time in (0.5, 1, 2, 5):
            lines.append(f'nsb1_dsp_state: t={time:.9f} words=0a051eff00d00003 '
                         'flags=04ec/1074 released=1 init_success=01 saved_version=0004 '
                         'handoffs=63 releases=3 observer_writes=0')
        lines += ['rom4_port_write: port=02 data=ffff pc=0d8f',
                  'rom4_port_write: port=36 data=0080 pc=8d98']
        return '\n'.join(lines)

    def test_native_continuation(self):
        check_trace(self.trace(), self.program3, self.program4, 0)

    def test_wrong_own_program(self):
        with self.assertRaisesRegex(ValueError, 'own uploads'):
            check_trace(self.trace(), self.program4, self.program3, 0)

    def test_wrong_version_sequence(self):
        with self.assertRaisesRegex(ValueError, 'mismatch'):
            check_trace(self.trace().replace('data=00000003', 'data=00000004'), self.program3, self.program4, 0)

    def test_no_block_completion(self):
        with self.assertRaisesRegex(ValueError, '63-handoff'):
            check_trace(self.trace().replace('handoffs=63', 'handoffs=62'), self.program3, self.program4, 0)

    def test_native_execution_required(self):
        with self.assertRaisesRegex(ValueError, 'execution'):
            check_trace(self.trace().replace('pc=8d98', 'pc=8d99'), self.program3, self.program4, 0)

    def test_final_publication_required(self):
        with self.assertRaisesRegex(ValueError, 'final-result'):
            check_trace(self.trace().replace('data=00000004 mask=0000ffff pc=0028fa96',
                                            'data=0000ffff mask=0000ffff pc=0028fa96'), self.program3, self.program4, 0)

    def test_crash_and_reset_rejected(self):
        for trace, code in ((self.trace(), 139), (self.trace() + '\nSoft reset', 0),
                            (self.trace() + '\nnsb1_reset_reason: reason=00000068', 0)):
            with self.subTest(code=code), self.assertRaises(ValueError):
                check_trace(trace, self.program3, self.program4, code)

    def test_prior_storage_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaisesRegex(ValueError, 'prior EEPROM'):
                prepare(Path(directory), Path(directory))

    def test_card_selection_is_explicit_and_empty_by_default(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / 'own.bin'
            source.write_bytes(b'qualified test input')
            inputs = [('own.bin', hashlib.sha1(source.read_bytes()).hexdigest())]
            with patch('tools.nsb1_rom4_compare.INPUTS', inputs):
                for inserted in (False, True):
                    run = root / str(inserted)
                    prepare(root, run, insert_sim=inserted)
                    manifest = json.loads((run / 'input_manifest.json').read_text())
                    self.assertEqual(manifest['synthetic_sim_inserted'], inserted)
                    self.assertFalse(manifest['boot_claim'])
                    self.assertEqual(('A', '671a'), (manifest['ppm'], manifest['bios']))
                    config = run / 'cfg/nsb1r4t.cfg'
                    self.assertEqual(config.exists(), inserted)
                    if inserted:
                        system = ET.parse(config).getroot().find('system')
                        self.assertEqual(system.get('name'), 'nsb1r4t')
                        port = system.find('input/port')
                        self.assertEqual(port.attrib, dict(tag=':SIM_REMOVED', type='OTHER',
                                                          mask='1', defvalue='1', value='0'))

    def test_unqualified_inputs_leave_no_run(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'noki5190').mkdir()
            (root / 'noki5190/5190f671a.fls').write_bytes(b'not the own image')
            with self.assertRaisesRegex(ValueError, 'unqualified'):
                prepare(root, root / 'run')
            self.assertFalse((root / 'run').exists())


class LcdCompareTest(unittest.TestCase):
    def trace(self):
        commands = '24 40 80 41 80 42 80 43 80 44 80 45 80 20 21 05 14 ce 20 0c'.split()
        lines = [f'nsb1_lcd_command: byte={value} pc=002ae138' for value in commands]
        for time, command, data, nonzero in ((0.1, 0, 0, 0), (0.5, 32, 1008, 504),
                                            (1, 32, 1008, 504), (2, 68, 2016, 504),
                                            (5, 68, 2016, 504)):
            lines.append(f'nsb1_lcd_summary: t={time} commands={command} data_bytes={data} '
                         f'nonzero={nonzero} observer_writes=0')
        return '\n'.join(lines)

    def test_bus_and_both_pixel_states(self):
        from PIL import Image
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            Image.new('RGB', (84, 48), 'black').save(root / 'nsb1_native_1s.png')
            Image.new('RGB', (84, 48), 'white').save(root / 'nsb1_native_5s.png')
            check_lcd(self.trace(), root)
            Image.new('RGB', (84, 48), 'white').save(root / 'nsb1_native_1s.png')
            with self.assertRaisesRegex(ValueError, 'pixels'):
                check_lcd(self.trace(), root)

    def test_command_drift(self):
        with self.assertRaisesRegex(ValueError, 'initialization'):
            check_lcd(self.trace().replace('byte=24', 'byte=25'), Path('.'))

    def test_missing_nonzero_transfer(self):
        with self.assertRaisesRegex(ValueError, 'traffic'):
            check_lcd(self.trace().replace('nonzero=504', 'nonzero=0'), Path('.'))


class ChecksumControlTest(unittest.TestCase):
    def test_menu_requires_contacts_and_nonblank_exact_pixels(self):
        from PIL import Image
        with self.assertRaisesRegex(ValueError, 'Menu/Select'):
            check_menu('', Path('.'))
        trace = '\n'.join(f'nsb1_menu_contact: index={index} down={down} t=26.0 physical_only=1'
                          for index in (1, 2) for down in (1, 0))
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            Image.new('RGB', (84, 48), 'white').save(root / 'nsb1_menu_2.png')
            with self.assertRaisesRegex(ValueError, 'pixels'):
                check_menu(trace, root)

    def test_physical_security_gate_controls(self):
        contacts = '\n'.join(f'nsb1_phonecode_contact: index={index} down={down} t=12.0 physical_only=1'
                             for index in range(1, 7) for down in (1, 0))
        for initialized, count in ((False, 'ff'), (True, '00')):
            trace = contacts + f'\nnsb1_security_gate: retry={count} match=00000001 t=14.0 observer_writes=0'
            check_security_gate(trace, initialized)
            for old, new in (('match=00000001', 'match=00000000'), ('index=6', 'index=5'),
                             ('observer_writes=0', 'observer_writes=1')):
                with self.subTest(change=old), self.assertRaises(ValueError):
                    check_security_gate(trace.replace(old, new), initialized)
            with self.assertRaises(ValueError):
                check_security_gate(trace, not initialized)

    def test_retry_fixture_changes_only_own_counter_beyond_integrity(self):
        source = bytes([0xff]) * 8192
        integrity = checksum_control(source)
        initialized = checksum_control(source, initialized_retry=True)
        self.assertEqual([i for i, (a, b) in enumerate(zip(integrity, initialized)) if a != b], [0x4ac])
        self.assertEqual(initialized[0x4ac], 0)
        self.assertEqual(initialized[0x4a8:0x4ac], source[0x4a8:0x4ac])

    def test_retry_requires_explicit_inserted_checksum_experiment(self):
        for inserted in (False, True):
            with self.subTest(inserted=inserted), self.assertRaisesRegex(ValueError, 'initialized retry requires'):
                prepare(Path('.'), Path('unused'), insert_sim=inserted, initialized_retry=True)

    def test_late_presentation_rejects_early_or_blank(self):
        from PIL import Image
        with self.assertRaisesRegex(ValueError, 'readiness'):
            check_late_presentation('', Path('.'))
        trace = 'nsb1_late_ui: t=10.000000000 phase=03 reports=0f column_mask=20 observer_writes=0'
        with self.assertRaisesRegex(ValueError, 'readiness'):
            check_late_presentation(trace.replace('10.000', '5.000'), Path('.'))
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            Image.new('RGB', (84, 48), 'white').save(root / 'nsb1_native_10s.png')
            with self.assertRaisesRegex(ValueError, 'pixels'):
                check_late_presentation(trace, root)

    def test_runtime_bit_retention(self):
        trace = '\n'.join(f'nsb1_presentation: t={time} selftest_flags={value} observer_writes=0'
                          for time, value in zip((0.1, 0.5, 1, 2, 5), ('00', 'c0', 'c0', '40', '40')))
        trace += '\n' + '\n'.join(f'nsb1_simi_summary: t={time} reads={reads} writes={writes} card_inserted=0 observer_writes=0'
                                    for time, reads, writes in ((0.1, 0, 0), (0.5, 0, 0), (1, 0, 18), (2, 3, 21), (5, 9, 45)))
        trace += '\nnsb1_simi: direction=W address=00020038 data=ff pc=0029cdfc'
        trace += '\nnsb1_simi: direction=W address=0002003a data=03 pc=0029ce26'
        trace += '\nnsb1_simi: direction=W address=0002003d data=18 pc=0029ce3c'
        trace += '\nnsb1_startup_mode: value=02 pc=002a438e t=0.128846692 observer_writes=0'
        trace += '\n' + '\n'.join(f'nsb1_input_readiness: t={time} startup_reason=0a phase=03 reports=0f column_mask=20 observer_writes=0'
                                    for time in (0.1, 0.5, 1, 2, 5))
        check_checksum_control(trace)
        with self.assertRaises(ValueError):
            check_checksum_control(trace.replace('selftest_flags=40', 'selftest_flags=00'))
        with self.assertRaises(ValueError):
            check_checksum_control(trace + '\nnsb1_selftest_flags: value=84 pc=0023a018 t=0.4')
        with self.assertRaisesRegex(ValueError, 'SIMI initialization'):
            check_checksum_control(trace.replace('pc=0029cdfc', 'pc=0029cdfd'))
        with self.assertRaisesRegex(ValueError, 'empty-socket'):
            check_checksum_control(trace.replace('writes=18', 'writes=0'))
        for old, new in (('reports=0f', 'reports=0e'), ('column_mask=20', 'column_mask=3f'),
                         ('startup_reason=0a', 'startup_reason=00'), ('phase=03', 'phase=00'),
                         ('nsb1_input_readiness:', 'missing_readiness:')):
            with self.subTest(change=old), self.assertRaisesRegex(ValueError, 'input-readiness'):
                check_checksum_control(trace.replace(old, new))

    def test_only_six_integrity_bytes_change(self):
        original = bytearray(b'\xff' * 8192)
        original[0x154:0x156] = b'\x12\x34'
        before = bytes(original)
        result = checksum_control(original)
        self.assertEqual(before, bytes(original))
        allowed = {*range(0x11c, 0x120), 0x1fe, 0x1ff}
        self.assertTrue({i for i, (a, b) in enumerate(zip(before, result)) if a != b} <= allowed)
        self.assertEqual((284 * 255) & 0xffff, struct.unpack_from('>I', result, 0x11c)[0])
        # Exclude each adjustment byte, not the numerical 0x1234 word.
        self.assertEqual((220 * 255) & 0xffff, struct.unpack_from('>H', result, 0x1fe)[0])
        self.assertEqual(result, checksum_control(result))

    def test_wrong_geometry_refused(self):
        for size in (2048, 4096, 16384):
            with self.subTest(size=size), self.assertRaises(ValueError):
                checksum_control(bytes(size))

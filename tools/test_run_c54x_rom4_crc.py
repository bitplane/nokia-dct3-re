import unittest
import contextlib
import hashlib
import io
import json
import tempfile
import re
import subprocess
import sys
from pathlib import Path
from unittest.mock import patch

from tools import run_c54x_rom4_crc as runner
from tools.run_c54x_rom4_crc import COMPLETE, PASS, PACKED_COMPLETE, PACKED_PASS, FR_COMPLETE, FR_PASS, check_result
from tools.run_c54x_rom4_crc import DECODER_PASS, DECODER_COMPLETE, DECODER_COMPARISON
from tools.run_c54x_rom4_crc import ENCODER_PASS, ENCODER_COMPLETE, ENCODER_COMPARISON
from tools.run_c54x_rom4_crc import PCM_MUTE_PASS
from tools.run_c54x_rom4_crc import DECODER_REPLAY, ENCODER_REPLAY
from tools.run_c54x_rom4_crc import file_identity
from tools.run_c54x_rom4_crc import EFR_CRC_PASS, EFR_CRC_COMPLETE
from tools.run_c54x_rom4_crc import EFR_PACKING_PASS, EFR_PACKING_COMPLETE
from tools.run_c54x_rom4_crc import EFR_DECODER_PASS, EFR_DECODER_COMPLETE
from tools.run_c54x_rom4_crc import DELAYED_REPEAT_PASS
from tools.run_c54x_rom4_crc import EFR_DECODER_REPLAY
from tools.run_c54x_rom4_crc import EFR_ENCODER_PASS, EFR_ENCODER_COMPLETE, EFR_ENCODER_REPLAY
from tools.run_c54x_rom4_crc import EFR_DTX_DECODER_PASS, EFR_DTX_DECODER_COMPLETE, EFR_DTX_DECODER_REPLAY
from tools.run_c54x_rom4_crc import EFR_DTX_ENCODER_PASS, EFR_DTX_ENCODER_COMPLETE, EFR_DTX_ENCODER_REPLAY
from tools.run_c54x_rom4_crc import EFR_ERASURE_PASS, EFR_ERASURE_COMPLETE, EFR_ERASURE_REPLAY
from tools.run_c54x_rom4_crc import EFR_HOMING_PASS, EFR_HOMING_COMPLETE, EFR_HOMING_REPLAY
from tools.run_c54x_rom4_crc import EFR_ENCODER_HOMING_PASS, EFR_ENCODER_HOMING_COMPLETE, EFR_ENCODER_HOMING_REPLAY
from tools.run_c54x_rom4_crc import EFR_INVALID_SID_PASS, EFR_INVALID_SID_COMPLETE, EFR_INVALID_SID_REPLAY
from tools.run_c54x_rom4_crc import check_rf_bitmap, check_rf_pairs


class Rom4HelperResultTest(unittest.TestCase):
    def test_run_records_all_modular_bench_source_identities(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            binary = root / 'mame'
            binary.write_bytes(b'test executable identity')
            output = root / 'runs'
            output.mkdir()

            def execute(command, **kwargs):
                kwargs['stdout'].write(PASS + '\n' + COMPLETE + '\n')
                return subprocess.CompletedProcess(command, 3)

            argv = ['run_c54x_rom4_crc', '--mame', str(binary),
                    '--rompath', str(root), '--output-root', str(output)]
            with patch('sys.argv', argv), patch.object(runner.subprocess, 'run', execute), \
                 contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(runner.main(), 0)
            evidence = json.loads(next(output.glob('*/evidence.json')).read_text())
            identities = {Path(item['path']): item for item in evidence['inputs']}
            sources = runner.harness_sources()
            self.assertGreater(len(sources), 10)
            self.assertTrue(any(path.suffix == '.h' for path in sources))
            for path in sources:
                self.assertEqual(identities[path]['sha256'], hashlib.sha256(path.read_bytes()).hexdigest())

    def test_lost_sid_requires_every_frame_replay_and_frontend(self):
        frames = '\n'.join(f'ROM4 EFR lost SID decoder comparison: frame={i} mismatches=0' for i in range(518))
        replay = 'TMS320C54x ROM4 EFR lost SID decoder replay: PASS frames=518 ram_words=65440 checkpoints=frame_input handset_audio_claim=0'
        marker = 'TMS320C54x ROM4 EFR lost SID decoder: PASS frames=518 samples=82880 lost_sid=2 mismatches=0 handset_audio_claim=0'
        complete = 'Fatal error: TMS320C54x ROM4 EFR lost SID decoder complete'
        text = '\n'.join((frames, replay, marker, complete))
        check_result(text, 3, efr_lost_sid=True)
        staged = 'ROM4 EFR lost SID decoder frontend: staged handset_audio_claim=0\n' + text
        check_result(staged, 3, efr_lost_sid=True, staged=True)
        for bad in (text.replace(replay, ''), text.replace('frame=97 mismatches=0', 'frame=97 mismatches=1'),
                    text.replace('frame=517 mismatches=0', 'frame=516 mismatches=0'),
                    text.replace('lost_sid=2', 'lost_sid=0'), text.replace(complete, '')):
            with self.assertRaises(ValueError):
                check_result(bad, 3, efr_lost_sid=True)
        with self.assertRaises(ValueError):
            check_result(text, 0, efr_lost_sid=True)
        with self.assertRaises(ValueError):
            check_result(text, 3, efr_lost_sid=True, staged=True)

    def test_native_rf_pair_math_and_history_require_both_tc_paths(self):
        marker='TMS320C54x ROM4 RF pairs: PASS cases=32 vectors=8 tc=2 frct=2 replay=32 ram_words=65440 dot_cross_both_tc=1 history_copy_tc_only=1 rf_acquisition_claim=0'
        complete='Fatal error: TMS320C54x ROM4 RF pairs complete'
        text=marker+'\n'+complete+'\n'
        check_rf_pairs(text,3)
        staged='ROM4 RF pairs frontend: staged handset_audio_claim=0\n'+text
        check_rf_pairs(staged,3,staged=True)
        for field in marker.split():
            if '=' in field:
                with self.subTest(field=field),self.assertRaises(ValueError):
                    check_rf_pairs(text.replace(field,field.split('=')[0]+'=invalid'),3)
        for bad in ('c54x_failure: phase=711000\n'+text, 'NOT FOUND\n'+text, complete+'\n'+text,
                    text+marker+'\n', 'Fatal error: other\n'+text, staged):
            with self.assertRaises(ValueError):check_rf_pairs(bad,3)
        with self.assertRaises(ValueError):check_rf_pairs(text,0)
        with self.assertRaises(ValueError):check_rf_pairs(text,3,staged=True)

    def test_native_rf_bitmap_basis_and_replay(self):
        marker='TMS320C54x ROM4 RF bitmap: PASS cases=515 basis=512 controls=empty,full,alternating replay=515 ram_words=65440 rf_acquisition_claim=0'
        complete='Fatal error: TMS320C54x ROM4 RF bitmap complete'
        text=marker+'\n'+complete+'\n'
        check_rf_bitmap(text,3)
        staged='ROM4 RF bitmap frontend: staged handset_audio_claim=0\n'+text
        check_rf_bitmap(staged,3,staged=True)
        for bad in (text.replace('basis=512','basis=511'),
                    text.replace('replay=515','replay=514'),
                    text.replace('ram_words=65440','ram_words=1'),
                    text+marker+'\n',text+'Fatal error: other\n',
                    'Driver tms54rom4: 1 errors, 0 warnings\n'+text,
                    'NOT FOUND\n'+text):
            with self.subTest(bad=bad),self.assertRaises(ValueError):
                check_rf_bitmap(bad,3)
        with self.assertRaises(ValueError): check_rf_bitmap(text,0)
        with self.assertRaises(ValueError): check_rf_bitmap(text,3,staged=True)

    def test_efr_lifecycle_frontends_load_the_same_vectors(self):
        source = (Path(__file__).resolve().parents[1] / 'driver/tms320c54x_test.cpp').read_text()
        body = source.split('ROM_START(tms54rom4)', 1)[1].split('ROM_END', 1)[0]
        bios = dict((name, int(number)) for number, name in
                    re.findall(r'ROM_SYSTEM_BIOS\((\d+), "([^"]+)"', body))
        cases = (
            ('efr-dtx-encoder', 'efr-dtx-enc-pipe', (('efr_dtx_encoder_pcm', 'dtx.inp'), ('efr_dtx_encoder_serial', 'dtx.cod'))),
            ('efr-dtx-decoder', 'efr-dtx-dec-pipe', (('efr_dtx_input', 'dtx.dec'), ('efr_dtx_pcm', 'dtx.out'))),
            ('efr-enc-homing', 'efr-ehome-pipe', (('efr_homing_encoder_pcm', 'homing_enc.inp'), ('efr_homing_encoder_serial', 'homing_enc.cod'))),
            ('efr-homing', 'efr-homing-pipe', (('efr_homing_input', 'homing.dec'), ('efr_homing_pcm', 'homing.out'))),
            ('efr-invalid-sid', 'efr-isid-pipe', (('efr_invalid_sid_input', 'invalid_sid.dec'), ('efr_invalid_sid_pcm', 'invalid_sid.out'))),
            ('efr-lost-sid', 'efr-lsid-pipe', (('efr_lost_sid_input', 'lost_sid.dec'), ('efr_lost_sid_pcm', 'lost_sid.out'))),
            ('efr-lsid-observe', 'efr-lso-pipe', (('efr_lost_sid_input', 'lost_sid.dec'), ('efr_lost_sid_pcm', 'lost_sid.out'))),
            ('efr-erasure', 'efr-erasure-pipe', (('efr_erasure_input', 'erasure.dec'), ('efr_erasure_pcm', 'erasure.out'))),
            ('efr-loss-observe', 'efr-loss-pipe', (('efr_erasure_input', 'erasure.dec'), ('efr_erasure_pcm', 'erasure.out'))),
        )
        self.assertTrue(all(len(name) <= 16 for name in bios))
        for name, staged, regions in cases:
            for region, filename in regions:
                with self.subTest(profile=name, region=region):
                    content = body.split(f'"{region}", 0)', 1)[1].split('ROM_REGION', 1)[0]
                    loads = re.findall(r'ROMX_LOAD\("' + re.escape(filename) +
                                       r'"(.*), ROM_BIOS\((\d+)\)\)', content)
                    by_bios = {int(number): payload for payload, number in loads}
                    self.assertEqual(len(loads), 4 if region.startswith(('efr_erasure_', 'efr_lost_sid_')) else 2)
                    self.assertEqual(by_bios[bios[name]], by_bios[bios[staged]])

    def test_staged_efr_lifecycle_requires_its_own_frontend_and_full_result(self):
        cases = (
            ('EFR DTX encoder', dict(efr_dtx_encoder=True), 518, EFR_DTX_ENCODER_REPLAY, EFR_DTX_ENCODER_PASS, EFR_DTX_ENCODER_COMPLETE),
            ('EFR DTX decoder', dict(efr_dtx_decoder=True), 518, EFR_DTX_DECODER_REPLAY, EFR_DTX_DECODER_PASS, EFR_DTX_DECODER_COMPLETE),
            ('EFR homing encoder', dict(efr_homing_encoder=True), 46, EFR_ENCODER_HOMING_REPLAY, EFR_ENCODER_HOMING_PASS, EFR_ENCODER_HOMING_COMPLETE),
            ('EFR homing decoder', dict(efr_homing_decoder=True), 46, EFR_HOMING_REPLAY, EFR_HOMING_PASS, EFR_HOMING_COMPLETE),
            ('EFR invalid SID decoder', dict(efr_invalid_sid=True), 518, EFR_INVALID_SID_REPLAY, EFR_INVALID_SID_PASS, EFR_INVALID_SID_COMPLETE),
            ('EFR erasure decoder', dict(efr_erasure_decoder=True), 163, EFR_ERASURE_REPLAY, EFR_ERASURE_PASS, EFR_ERASURE_COMPLETE),
        )
        for kind, mode, count, replay, marker, complete in cases:
            frontend = f'ROM4 {kind} frontend: staged handset_audio_claim=0'
            frames = '\n'.join(f'ROM4 {kind} comparison: frame={i} mismatches=0' for i in range(count))
            result = '\n'.join((frames, replay, marker, complete))
            output = frontend + '\n' + result
            with self.subTest(kind=kind):
                check_result(output, 3, staged=True, **mode)
                for bad in (result, frontend + '\n' + output,
                            'Driver tms54rom4 (file tms320c54x_test.cpp): 3 errors, 0 warnings\n' + output,
                            output.replace(frontend, 'ROM4 EFR encoder frontend: staged handset_audio_claim=0'),
                            output.replace(replay, ''),
                            output.replace(f'frame={count-1} mismatches=0', f'frame={count-1} mismatches=1'),
                            output + '\ntrailer'):
                    with self.assertRaises(ValueError):
                        check_result(bad, 3, staged=True, **mode)

    def test_efr_lifecycle_frontends_load_the_same_roms(self):
        source = (Path(__file__).resolve().parents[1] / 'driver/tms320c54x_test.cpp').read_text()
        body = source.split('ROM_START(tms54rom4)', 1)[1].split('ROM_END', 1)[0]
        bios = dict((name, int(number)) for number, name in
                    re.findall(r'ROM_SYSTEM_BIOS\((\d+), "([^"]+)"', body))
        for region, filename in (('dspprg', 'dsp_full.bin'),
                                 ('dspdata', 'dsp_cold_data.bin')):
            content = body.split(f'"{region}", 0)', 1)[1].split('ROM_REGION', 1)[0]
            loads = re.findall(r'ROMX_LOAD\("' + re.escape(filename) +
                               r'"(.*?)ROM_BIOS\((\d+)\)\)', content, re.S)
            by_bios = {int(number): re.sub(r'\s+', ' ', payload).strip()
                       for payload, number in loads}
            for atomic, staged in (('efr-invalid-sid', 'efr-isid-pipe'),
                                   ('efr-erasure', 'efr-erasure-pipe'),
                                   ('efr-loss-observe', 'efr-loss-pipe')):
                with self.subTest(region=region, atomic=atomic):
                    self.assertEqual(by_bios[bios[atomic]], by_bios[bios[staged]], region)

    def test_efr_decoder_frontends_load_the_same_vectors(self):
        source = (Path(__file__).resolve().parents[1] / 'driver/tms320c54x_test.cpp').read_text()
        body = source.split('ROM_START(tms54rom4)', 1)[1].split('ROM_END', 1)[0]
        bios = dict((name, int(number)) for number, name in
                    re.findall(r'ROM_SYSTEM_BIOS\((\d+), "([^"]+)"', body))
        for region, filename in (('efr_reference_input', 'spch_dos.dec'),
                                 ('efr_reference_pcm', 'spch_dos.out')):
            content = body.split(f'"{region}", 0)', 1)[1].split('ROM_REGION', 1)[0]
            loads = re.findall(r'ROMX_LOAD\("' + re.escape(filename) +
                               r'"(.*), ROM_BIOS\((\d+)\)\)', content)
            by_bios = {int(number): payload for payload, number in loads}
            self.assertEqual(len(loads), 2, region)
            self.assertEqual(by_bios[bios['efr-decoder']],
                             by_bios[bios['efr-decoder-pipe']], region)

    def test_efr_encoder_frontends_load_the_same_vectors(self):
        source = (Path(__file__).resolve().parents[1] / 'driver/tms320c54x_test.cpp').read_text()
        body = source.split('ROM_START(tms54rom4)', 1)[1].split('ROM_END', 1)[0]
        bios = dict((name, int(number)) for number, name in
                    re.findall(r'ROM_SYSTEM_BIOS\((\d+), "([^"]+)"', body))
        for region, filename in (('efr_encoder_pcm', 'spch_dos.inp'),
                                 ('efr_encoder_serial', 'spch_dos.cod')):
            content = body.split(f'"{region}", 0)', 1)[1].split('ROM_REGION', 1)[0]
            loads = re.findall(r'ROMX_LOAD\("' + re.escape(filename) +
                               r'"(.*), ROM_BIOS\((\d+)\)\)', content)
            by_bios = {int(number): payload for payload, number in loads}
            self.assertEqual(len(loads), 2, region)
            self.assertEqual(by_bios[bios['efr-encoder']],
                             by_bios[bios['efr-encoder-pipe']], region)

    def test_every_rom4_bios_loads_program_and_data(self):
        source = (Path(__file__).resolve().parents[1] / 'driver/tms320c54x_test.cpp').read_text()
        body = source.split('ROM_START(tms54rom4)', 1)[1].split('ROM_END', 1)[0]
        declared = set(map(int, re.findall(r'ROM_SYSTEM_BIOS\((\d+),', body)))
        for region in ('dspprg', 'dspdata'):
            content = body.split(f'"{region}", 0)', 1)[1].split('ROM_REGION', 1)[0]
            loaded = list(map(int, re.findall(r'ROM_BIOS\((\d+)\)', content)))
            self.assertEqual(set(loaded), declared, region)
            self.assertEqual(len(loaded), len(declared), region)

    def test_efr_encoder_homing_requires_every_result_and_replay(self):
        frames = "\n".join(f"ROM4 EFR homing encoder comparison: frame={frame} mismatches=0" for frame in range(46))
        text = "\n".join((frames, EFR_ENCODER_HOMING_REPLAY, EFR_ENCODER_HOMING_PASS, EFR_ENCODER_HOMING_COMPLETE))
        check_result(text, 3, efr_homing_encoder=True)
        check_result(text + "\nAverage speed: 1234.56% (6 seconds)", 3, efr_homing_encoder=True)
        for invalid in (text.replace("frame=45 mismatches=0", "frame=45 mismatches=1"),
                        text.replace("frame=0 mismatches=0", "frame=1 mismatches=0"),
                        text.replace(EFR_ENCODER_HOMING_REPLAY, ""), text.replace("homing=6", "homing=0"),
                        text.replace("parameters=2622", "parameters=0"),
                        text.replace("packed_words=782", "packed_words=0"),
                        text.replace("repeated_speech=20", "repeated_speech=0"),
                        text.replace("ram_words=65440", "ram_words=160"),
                        text + "\nunexpected trailer"):
            with self.subTest(text=invalid), self.assertRaises(ValueError):
                check_result(invalid, 3, efr_homing_encoder=True)

    def test_efr_homing_decoder_requires_every_frame_and_replay(self):
        frames = "\n".join(f"ROM4 EFR homing decoder comparison: frame={frame} mismatches=0" for frame in range(46))
        text = "\n".join((frames, EFR_HOMING_REPLAY, EFR_HOMING_PASS, EFR_HOMING_COMPLETE))
        check_result(text, 3, efr_homing_decoder=True)
        check_result(text + "\nAverage speed: 1234.56% (6 seconds)", 3, efr_homing_decoder=True)
        for invalid in (text.replace("frame=45 mismatches=0", "frame=45 mismatches=1"),
                        text.replace("frame=0 mismatches=0", "frame=1 mismatches=0"),
                        text.replace(EFR_HOMING_REPLAY, ""), text.replace("homing=6", "homing=0"),
                        text.replace("steady_ehf=4", "steady_ehf=0"),
                        text.replace("repeated_speech=20", "repeated_speech=0"),
                        text.replace("ram_words=65440", "ram_words=160"),
                        text + "\nunexpected trailer"):
            with self.subTest(text=invalid), self.assertRaises(ValueError):
                check_result(invalid, 3, efr_homing_decoder=True)

    def test_efr_invalid_sid_requires_every_frame_control_scope_and_replay(self):
        frames = '\n'.join(f'ROM4 EFR invalid SID decoder comparison: frame={frame} mismatches=0' for frame in range(518))
        text = '\n'.join((frames, EFR_INVALID_SID_REPLAY, EFR_INVALID_SID_PASS, EFR_INVALID_SID_COMPLETE))
        check_result(text, 3, efr_invalid_sid=True)
        check_result(text + '\nAverage speed: 1234.56% (20 seconds)', 3, efr_invalid_sid=True)
        for bad in (text.replace('frame=517 mismatches=0', 'frame=517 mismatches=1'),
                    text.replace('frame=517 mismatches=0', 'frame=0 mismatches=0'),
                    text.replace('control_checked=518', 'control_checked=0'),
                    text.replace('invalid_codewords=4', 'invalid_codewords=0'),
                    text.replace('bad_valid_codewords=1', 'bad_valid_codewords=0'),
                    text.replace('ram_words=65440', 'ram_words=160'),
                    text.replace(EFR_INVALID_SID_REPLAY, ''), text + '\ntrailer'):
            with self.subTest(bad=bad[-200:]), self.assertRaises(ValueError):
                check_result(bad, 3, efr_invalid_sid=True)

    def test_efr_erasure_decoder_completion(self):
        frames = "\n".join(f"ROM4 EFR erasure decoder comparison: frame={frame} mismatches=0" for frame in range(163))
        text = "\n".join((frames, EFR_ERASURE_REPLAY, EFR_ERASURE_PASS, EFR_ERASURE_COMPLETE))
        check_result(text, 3, efr_erasure_decoder=True)
        check_result(text + "\nAverage speed: 1234.56% (6 seconds)", 3, efr_erasure_decoder=True)
        for invalid in (text.replace("frame=9 mismatches=0", "frame=9 mismatches=127"),
                        text.replace("frame=162 mismatches=0", "frame=0 mismatches=0"),
                        text.replace(EFR_ERASURE_REPLAY, ""), text.replace("erased=28", "erased=0"),
                        text.replace("bursts=1,3,8,16", "bursts=1"),
                        text.replace("ram_words=65440", "ram_words=160"),
                        text + "\nunexpected trailer"):
            with self.subTest(text=invalid), self.assertRaises(ValueError):
                check_result(invalid, 3, efr_erasure_decoder=True)

    def test_efr_dtx_encoder_completion(self):
        frames = "\n".join(f"ROM4 EFR DTX encoder comparison: frame={frame} mismatches=0" for frame in range(518))
        text = "\n".join((frames, EFR_DTX_ENCODER_REPLAY, EFR_DTX_ENCODER_PASS, EFR_DTX_ENCODER_COMPLETE))
        check_result(text, 3, efr_dtx_encoder=True)
        for invalid in (text.replace("frame=517 mismatches=0", "frame=517 mismatches=1"),
                        text.replace("frame=0 mismatches=0", "frame=1 mismatches=0"),
                        text.replace("flags=1036", "flags=0"), text.replace("packed_words=8806", "packed_words=0"),
                        text.replace(EFR_DTX_ENCODER_REPLAY, ""),
                        text.replace("ram_words=65440", "ram_words=160"), text + "\nunexpected trailer"):
            with self.subTest(text=invalid), self.assertRaises(ValueError):
                check_result(invalid, 3, efr_dtx_encoder=True)

    def test_efr_dtx_decoder_completion(self):
        frames = "\n".join(f"ROM4 EFR DTX decoder comparison: frame={frame} mismatches=0" for frame in range(518))
        text = "\n".join((frames, EFR_DTX_DECODER_REPLAY, EFR_DTX_DECODER_PASS, EFR_DTX_DECODER_COMPLETE))
        check_result(text, 3, efr_dtx_decoder=True)
        check_result(text + "\nAverage speed: 1234.56% (20 seconds)", 3, efr_dtx_decoder=True)
        for invalid in (text.replace("frame=517 mismatches=0", "frame=517 mismatches=1"),
                        text.replace("frame=0 mismatches=0", "frame=1 mismatches=0"),
                        text.replace("erased=135", "erased=0"), text.replace("sid=8", "sid=0"),
                        text.replace("nonzero_cni_frames=46", "nonzero_cni_frames=0"),
                        text.replace(EFR_DTX_DECODER_REPLAY, ""),
                        text.replace("ram_words=65440", "ram_words=160"),
                        text + "\nunexpected trailer"):
            with self.subTest(text=invalid), self.assertRaises(ValueError):
                check_result(invalid, 3, efr_dtx_decoder=True)

    def test_efr_encoder_completion(self):
        frames = "\n".join(f"ROM4 EFR encoder comparison: frame={frame} mismatches=0" for frame in range(163))
        text = "\n".join((frames, EFR_ENCODER_REPLAY, EFR_ENCODER_PASS, EFR_ENCODER_COMPLETE))
        check_result(text, 3, efr_encoder=True)
        frontend = 'ROM4 EFR encoder frontend: staged handset_audio_claim=0'
        staged = frontend + '\n' + text
        check_result(staged, 3, efr_encoder=True, staged=True)
        teardown = '\nEnter sdlwindow_exit\nLeave sdlwindow_exit'
        check_result(staged + teardown, 3, efr_encoder=True, staged=True, verbose_teardown=True)
        with self.assertRaises(ValueError):
            check_result(staged + teardown, 3, efr_encoder=True, staged=True)
        with self.assertRaises(ValueError):
            check_result(staged + teardown + '\nunexpected', 3, efr_encoder=True, staged=True, verbose_teardown=True)
        for bad in (text, staged.replace('EFR encoder frontend', 'FR encoder frontend'),
                    frontend + '\n' + staged,
                    staged.replace('frame=162 mismatches=0', 'frame=162 mismatches=1'),
                    staged.replace(EFR_ENCODER_REPLAY, '')):
            with self.subTest(staged=bad[-200:]), self.assertRaises(ValueError):
                check_result(bad, 3, efr_encoder=True, staged=True)
        check_result(text + "\nAverage speed: 1234.56% (6 seconds)", 3, efr_encoder=True)
        for invalid in (text.replace("frame=162 mismatches=0", "frame=162 mismatches=1"),
                        text.replace("frame=0 mismatches=0", "frame=1 mismatches=0"),
                        text.replace(EFR_ENCODER_REPLAY, ""),
                        text.replace(EFR_ENCODER_REPLAY, EFR_ENCODER_REPLAY + "\n" + EFR_ENCODER_REPLAY),
                        text.replace("ram_words=65440", "ram_words=160"),
                        text.replace("flags=326", "flags=0"),
                        text.replace("parameters=9291", "parameters=0"),
                        text.replace("packed_words=2771", "packed_words=0"),
                        text.replace(EFR_ENCODER_PASS, EFR_DECODER_PASS),
                        text + "\nunexpected trailer"):
            with self.subTest(text=invalid), self.assertRaises(ValueError):
                check_result(invalid, 3, efr_encoder=True)
        with self.assertRaises(ValueError):
            check_result(text, 0, efr_encoder=True)

    def test_efr_decoder_completion(self):
        frames = "\n".join(f"ROM4 EFR decoder comparison: frame={frame} mismatches=0" for frame in range(163))
        text = DELAYED_REPEAT_PASS + "\n" + frames + "\n" + EFR_DECODER_REPLAY + "\n" + EFR_DECODER_PASS + "\n" + EFR_DECODER_COMPLETE
        check_result(text, 3, efr_decoder=True)

        frontend = 'ROM4 EFR decoder frontend: staged handset_audio_claim=0'
        staged = frontend + '\n' + text
        check_result(staged, 3, efr_decoder=True, staged=True)
        for invalid in (text, staged.replace('EFR decoder frontend', 'EFR encoder frontend'),
                        frontend + '\n' + staged,
                        staged.replace('frame=162 mismatches=0', 'frame=162 mismatches=1'),
                        staged.replace(EFR_DECODER_REPLAY, ''),
                        staged.replace(DELAYED_REPEAT_PASS, '')):
            with self.subTest(staged=invalid[-200:]), self.assertRaises(ValueError):
                check_result(invalid, 3, efr_decoder=True, staged=True)
        check_result(text + "\nAverage speed: 1234.56% (6 seconds)", 3, efr_decoder=True)
        for invalid in (text.replace("frame=162 mismatches=0", "frame=162 mismatches=1"),
                        text.replace("frame=0 mismatches=0", "frame=1 mismatches=0"),
                        text.replace(EFR_DECODER_PASS, EFR_PACKING_PASS),
                        text.replace(DELAYED_REPEAT_PASS, ""),
                        text.replace(EFR_DECODER_REPLAY, ""),
                        text + "\nunexpected trailer",
                        text + "\nAverage speed: 1234.56% (6 seconds)\nunexpected trailer"):
            with self.subTest(text=invalid), self.assertRaises(ValueError):
                check_result(invalid, 3, efr_decoder=True)

    def test_efr_packing_completion(self):
        text = EFR_PACKING_PASS + "\n" + EFR_PACKING_COMPLETE
        check_result(text, 3, efr_packing=True)
        for invalid in (text.replace("corrupt_frames=277", "corrupt_frames=0"),
                        text.replace(EFR_PACKING_PASS, EFR_CRC_PASS),
                        text.replace(EFR_PACKING_PASS, EFR_PACKING_PASS + "\n" + EFR_PACKING_PASS)):
            with self.subTest(text=invalid), self.assertRaises(ValueError):
                check_result(invalid, 3, efr_packing=True)

    def test_efr_crc_completion(self):
        text = EFR_CRC_PASS + "\n" + EFR_CRC_COMPLETE
        check_result(text, 3, efr_crc=True)
        for invalid in (text.replace("cases=554", "cases=0"),
                        text.replace(EFR_CRC_PASS, EFR_CRC_PASS + "\n" + EFR_CRC_PASS),
                        text.replace(EFR_CRC_PASS, FR_PASS)):
            with self.subTest(text=invalid), self.assertRaises(ValueError):
                check_result(invalid, 3, efr_crc=True)

    def test_file_identity_tracks_actual_bytes(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "input.bin"
            path.write_bytes(b"abc")
            identity = file_identity(path)
            self.assertEqual(identity["path"], str(path.resolve()))
            self.assertEqual(identity["bytes"], 3)
            self.assertEqual(identity["sha256"],
                             "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")
            path.write_bytes(b"abcd")
            self.assertNotEqual(file_identity(path)["sha256"], identity["sha256"])

    def test_encoder_completion(self):
        text = "\n".join((ENCODER_COMPARISON, ENCODER_REPLAY, ENCODER_PASS, PCM_MUTE_PASS, ENCODER_COMPLETE))
        check_result(text, 3, fr_encoder=True)
        for invalid in (text.replace("mismatches=0", "mismatches=1"),
                        text.replace(ENCODER_COMPARISON, ""),
                        text.replace(ENCODER_REPLAY, ""),
                        text.replace(ENCODER_REPLAY, ENCODER_REPLAY + "\n" + ENCODER_REPLAY),
                        text.replace("ram_words=65440", "ram_words=160"),
                        text.replace("reference_advance=once", "reference_advance=twice"),
                        text.replace(PCM_MUTE_PASS, ""),
                        text.replace(PCM_MUTE_PASS, PCM_MUTE_PASS + "\n" + PCM_MUTE_PASS),
                        text.replace(ENCODER_PASS, DECODER_PASS),
                        text.replace(ENCODER_PASS, ENCODER_PASS + "\n" + ENCODER_PASS)):
            with self.subTest(text=invalid), self.assertRaises(ValueError):
                check_result(invalid, 3, fr_encoder=True)

    def test_decoder_completion(self):
        text = "\n".join((DECODER_COMPARISON, DECODER_REPLAY, DECODER_PASS, DECODER_COMPLETE))
        check_result(text, 3, fr_decoder=True)
        for invalid in (text.replace("mismatches=0", "mismatches=1"),
                        text.replace(DECODER_COMPARISON, ""),
                        text.replace(DECODER_REPLAY, ""),
                        text.replace(DECODER_REPLAY, DECODER_REPLAY + "\n" + DECODER_REPLAY),
                        text.replace("frames=32 ram_words", "frames=0 ram_words"),
                        text.replace("reference_advance=once", "reference_advance=twice"),
                        text.replace(DECODER_PASS, FR_PASS),
                        text.replace(DECODER_PASS, DECODER_PASS + "\n" + DECODER_PASS)):
            with self.subTest(text=invalid), self.assertRaises(ValueError):
                check_result(invalid, 3, fr_decoder=True)

    def test_staged_decoder_requires_frontend_and_full_comparison(self):
        marker = 'ROM4 FR decoder frontend: staged handset_audio_claim=0'
        result = '\n'.join((DECODER_COMPARISON, DECODER_REPLAY, DECODER_PASS, DECODER_COMPLETE))
        text = marker + '\n' + result
        check_result(text, 3, fr_decoder=True, staged=True)
        for invalid in (result, marker + '\n' + text,
                        text.replace('mismatches=0', 'mismatches=1'),
                        text.replace(DECODER_REPLAY, '')):
            with self.subTest(text=invalid), self.assertRaises(ValueError):
                check_result(invalid, 3, fr_decoder=True, staged=True)
        with self.assertRaises(ValueError):
            check_result(text, 3, staged=True)

    def test_staged_encoder_requires_selection_parameters_and_replay(self):
        marker = 'ROM4 FR encoder frontend: staged handset_audio_claim=0'
        result = '\n'.join((ENCODER_COMPARISON, ENCODER_REPLAY, ENCODER_PASS, PCM_MUTE_PASS, ENCODER_COMPLETE))
        text = marker + '\n' + result
        check_result(text, 3, fr_encoder=True, staged=True)
        for invalid in (result, marker + '\n' + text,
                        text.replace('FR encoder frontend', 'FR decoder frontend'),
                        text.replace('mismatches=0', 'mismatches=1'),
                        text.replace(ENCODER_REPLAY, ''), text.replace(PCM_MUTE_PASS, '')):
            with self.subTest(text=invalid), self.assertRaises(ValueError):
                check_result(invalid, 3, fr_encoder=True, staged=True)

    def test_encoder_verbose_footer_does_not_waive_parameters_or_replay(self):
        result = '\n'.join((ENCODER_COMPARISON, ENCODER_REPLAY, ENCODER_PASS, PCM_MUTE_PASS, ENCODER_COMPLETE))
        text = result + '\nEnter sdlwindow_exit\nLeave sdlwindow_exit\n'
        check_result(text, 3, fr_encoder=True, verbose_teardown=True)
        for invalid in (text + 'unexpected\n', text.replace('mismatches=0', 'mismatches=1'),
                        text.replace(ENCODER_REPLAY, ''), text.replace(PCM_MUTE_PASS, '')):
            with self.subTest(text=invalid), self.assertRaises(ValueError):
                check_result(invalid, 3, fr_encoder=True, verbose_teardown=True)
        with self.assertRaises(ValueError):
            check_result(text, 3, fr_encoder=True)

    def test_decoder_verbose_footer_is_explicit_and_does_not_waive_pcm(self):
        result = '\n'.join((DECODER_COMPARISON, DECODER_REPLAY, DECODER_PASS, DECODER_COMPLETE))
        text = result + '\nEnter sdlwindow_exit\nLeave sdlwindow_exit\n'
        check_result(text, 3, fr_decoder=True, verbose_teardown=True)
        for invalid in (text + 'unexpected\n', text.replace('mismatches=0', 'mismatches=1')):
            with self.subTest(text=invalid), self.assertRaises(ValueError):
                check_result(invalid, 3, fr_decoder=True, verbose_teardown=True)
        with self.assertRaises(ValueError):
            check_result(text, 3, fr_decoder=True)

    def test_efr_trace_aliases_require_encoder(self):
        script = Path(__file__).with_name('run_c54x_rom4_crc.py')
        for option in ('--trace-efr', '--trace-efr-reset'):
            result = subprocess.run([sys.executable, str(script), '--mame', 'unused',
                                     '--rompath', 'unused', '--fr-decoder', option],
                                    capture_output=True, text=True)
            with self.subTest(option=option):
                self.assertEqual(result.returncode, 2)
                self.assertIn('--trace-efr requires --efr-encoder', result.stderr)

    def test_completion(self):
        check_result(PASS + "\n" + COMPLETE + "\n", 3)

    def test_packed_completion(self):
        check_result(PACKED_PASS + "\n" + PACKED_COMPLETE + "\n", 3, packed=True)
        with self.assertRaises(ValueError):
            check_result(PASS + "\n" + COMPLETE + "\n", 3, packed=True)
        with self.assertRaises(ValueError):
            check_result(PACKED_PASS + "\n" + PACKED_COMPLETE + "\n", 3)

    def test_fr_packing_completion(self):
        check_result(FR_PASS + "\n" + FR_COMPLETE + "\n", 3, fr_packing=True)
        with self.assertRaises(ValueError):
            check_result(PACKED_PASS + "\n" + PACKED_COMPLETE + "\n", 3, fr_packing=True)
        with self.assertRaises(ValueError):
            check_result(FR_PASS + "\n" + FR_COMPLETE + "\n", 3, packed=True)

    def test_reject_incomplete_or_ambiguous_results(self):
        for text, status in [(PASS, 3), (COMPLETE, 3),
                             (PASS + "\n" + COMPLETE, 0),
                             (PASS + "\n" + PASS + "\n" + COMPLETE, 3),
                             (PASS + "\n" + COMPLETE + "\nextra", 3),
                             ("WRONG CHECKSUMS\n" + PASS + "\n" + COMPLETE, 3),
                             ("Fatal error: failure\n" + PASS + "\n" + COMPLETE, 3)]:
            with self.subTest(text=text, status=status):
                with self.assertRaises(ValueError):
                    check_result(text, status)


if __name__ == "__main__":
    unittest.main()

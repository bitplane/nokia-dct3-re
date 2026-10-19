import struct
import tempfile
import unittest
from pathlib import Path
import wave

from tools.mu4_dac_output_check import check_frames, check_wav, check_full_capture


def fixture(extra=None,levels=(0.5,-0.5),volume=0x79):
    registers = [0x97,0x97,0x79,0x79,0x0a,0x08,0x07,0x01,0x20,0]
    fields = {'power':6,'analog':4,'digital':5,'format':7,'rate':8,'active':9,'hp_left':2,'hp_right':3}
    lines = []
    commands = [0x0810,0x0a00,0x0e53,0x1023,0x1201]
    if extra is not None: commands.append(extra)
    for index,word in enumerate(commands):
        address,data = word >> 9,word & 0x1ff
        registers[address] = data
        if address in (2,3) and data & 0x100:
            registers[address^1] = (registers[address^1] & ~0x7f) | (data & 0x7f)
        snapshot = ' '.join(f'{field}={registers[address]:03x}' for field,address in fields.items())
        lines.append(f'mu4_codec_control: word={word:04x} ns={1_000_000_000+index*10000} {snapshot} observer_writes=0 audio_acceptance=0')
    lines += [f'mu4_dac_frame: ordinal=1 ns=1070001334 left_code=4000 right_code=c000 left_volume={volume:02x} right_volume={volume:02x} left_level={levels[0]:.12f} right_level={levels[1]:.12f} observer_writes=0 analog_filter=0 audio_acceptance=0',
              'mu4_dac_terminal: frames=1 first=1.060000000 last=1.100000000 capped=0 t=2.016666667 observer_writes=0 analog_filter=0 audio_acceptance=0']
    return '\n'.join(lines),[0x4000,0xc000],struct.pack('<QBQB',1_070_000_000,0,1_070_001_334,1)


class DacOutputTest(unittest.TestCase):
    def test_full_frame_values_are_bound_to_every_din_pair(self):
        text,din,slots = fixture()
        data = struct.pack('<QhhBBHdd',1_070_001_334,0x4000,-0x4000,0x79,0x79,0,0.5,-0.5)
        self.assertEqual(check_frames(text,din,slots,data)['frames'],1)
        extended_slots = slots + struct.pack('<QBQB',1_070_022_675,0,1_070_024_009,1)
        with self.assertRaises(ValueError):
            check_frames(text,din+din,extended_slots,data)
        checksum = 2166136261
        for value in data:
            checksum = ((checksum^value)*16777619)&0xffffffff
        terminal = (f'mu4_dac_capture: frames=1 bytes=32 fnv1a={checksum:08x} capped=0 '
            'encoding=le64ns_i16left_i16right_u8lvol_u8rvol_u16reserved_f64left_f64right '
            'file=mu4_headphone.frames save_restore=unsupported observer_writes=0 analog_filter=0 audio_acceptance=0')
        self.assertEqual(check_full_capture(terminal,data)['frames'],1)
        for changed in (terminal.replace('capped=0','capped=1'),terminal.replace('frames=1','frames=2')):
            with self.assertRaises(ValueError):
                check_full_capture(changed,data)
        for level,reserved in ((0.4,0),(float('nan'),0),(0.5,1)):
            changed = struct.pack('<QhhBBHdd',1_070_001_334,0x4000,-0x4000,0x79,0x79,reserved,level,-0.5)
            with self.assertRaises(ValueError):
                check_frames(text,din,slots,changed)

    def test_independent_stereo_codes_and_nominal_gain(self):
        result = check_frames(*fixture())
        self.assertEqual(result['frames'],1)
        self.assertEqual(result['nonzero_channel_samples'],2)
        self.assertFalse(result['native_audio_validated'])
        half = 0.2505936168136361
        result = check_frames(*fixture(extra=0x0573,levels=(half,-half),volume=0x73))
        self.assertLess(result['max_normalized_level_error'],6e-12)

    def test_dac_mute_power_and_selected_path_gate_output(self):
        for command in (0x0a08,0x0c0f,0x0c17,0x0800):
            with self.subTest(command=command):
                result = check_frames(*fixture(extra=command,levels=(0,0)))
                self.assertEqual(result['nonzero_channel_samples'],0)
                if command == 0x0a08:
                    self.assertTrue(result['digital_mute_epochs'][0]['digital_mute'])
                    self.assertEqual(result['digital_mute_epochs'][0]['nonzero_channel_samples'],0)

    def test_changed_values_volume_scope_and_missing_pair_fail(self):
        text,din,slots = fixture()
        for old,new in [('left_level=0.500000000000','left_level=0.400000000000'),
                        ('left_code=4000','left_code=4001'),('left_volume=79','left_volume=70'),
                        ('analog_filter=0','analog_filter=1'),('capped=0','capped=1')]:
            with self.subTest(old=old),self.assertRaises(ValueError):
                check_frames(text.replace(old,new),din,slots)
        with self.assertRaises(ValueError):
            check_frames(text,din,struct.pack('<QBQB',1_070_000_000,0,1_070_002_000,1))

    def test_pending_volume_waits_for_known_crossing(self):
        text,_,_ = fixture()
        terminal = text.splitlines()[-1]
        extra = [
            'mu4_codec_control: word=0580 ns=1070005000 power=007 analog=010 digital=000 format=053 rate=023 active=001 hp_left=180 hp_right=000 observer_writes=0 audio_acceptance=0',
            'mu4_dac_frame: ordinal=2 ns=1070024009 left_code=4000 right_code=c000 left_volume=79 right_volume=00 left_level=0.500000000000 right_level=0.000000000000 observer_writes=0 analog_filter=0 audio_acceptance=0',
            'mu4_dac_frame: ordinal=3 ns=1070046685 left_code=c000 right_code=4000 left_volume=00 right_volume=00 left_level=0.000000000000 right_level=0.000000000000 observer_writes=0 analog_filter=0 audio_acceptance=0',
        ]
        text = text.replace(terminal,'\n'.join(extra+[terminal.replace('frames=1','frames=3')]))
        slots = b''.join(struct.pack('<QB',time,slot) for time,slot in [
            (1070000000,0),(1070001334,1),(1070022675,0),(1070024009,1),
            (1070045351,0),(1070046685,1)])
        self.assertEqual(check_frames(text,[0x4000,0xc000,0x4000,0xc000,0xc000,0x4000],slots)['nonzero_channel_samples'],3)

    def test_missing_early_input_does_not_invent_a_zero_crossing(self):
        with self.assertRaisesRegex(ValueError,'cannot be established'):
            check_frames(*fixture(extra=0x0580,levels=(0,0),volume=0))

    def test_rendered_wav_requires_both_channels(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/'sample.wav'
            def write(words):
                with wave.open(str(path),'wb') as output:
                    output.setnchannels(2); output.setsampwidth(2); output.setframerate(48000)
                    output.writeframes(struct.pack('<'+'h'*len(words),*words))
            write([100,-200,200,-100])
            self.assertEqual(check_wav(path)['peak'],[200,200])
            write([100,0,200,0])
            with self.assertRaises(ValueError):
                check_wav(path)


if __name__ == '__main__':
    unittest.main()

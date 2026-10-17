import unittest
import tempfile
from pathlib import Path
from PIL import Image

from tools.c54x_rom4_tone_levels_check import check_level, check_frame, check_host_writers, check_private_shadows
from tools import test_c54x_rom4_codec_contract as codec_fixtures


class NativeToneLevelsTest(unittest.TestCase):
    def shadow_trace(self):
        return '\n'.join([
            'rom4_tone_access: owner=codec_selector8_shadow direction=write address=10a9e4 value=8426 mask=ffff pc=271fd8 t=1.0',
            'rom4_codec_parameter_publish: phase=boot field=0008 wire=8426 r5=00000001',
            'rom4_tone_access: owner=codec_selector9_shadow direction=write address=10a9e8 value=92090000 mask=ffff0000 pc=271fd8 t=1.1',
            'rom4_codec_parameter_publish: phase=boot field=0009 wire=9209 r5=00000000',
            'rom4_tone_access: owner=codec_selector9_shadow direction=write address=10a9e8 value=920c0000 mask=ffff0000 pc=271f14 t=9.0',
            'rom4_codec_parameter_publish: phase=interactive field=001a wire=920c r5=00000001',
        ])

    def test_private_shadow_join_preserves_lanes_and_subfield_ownership(self):
        result = check_private_shadows(self.shadow_trace())
        self.assertEqual(result['selector8_publications'], 1)
        self.assertEqual(result['selector9_publications'], 2)
        self.assertEqual(result['observed_subfields'], [0x1a])
        self.assertFalse(result['hardware_request_latch_claim'])
        changed = self.shadow_trace().replace(
            'value=920c0000 mask=ffff0000 pc=271f14 t=9.0',
            'value=92000000 mask=ff000000 pc=271f14 t=9.0\n'
            'rom4_tone_access: owner=codec_selector9_shadow direction=write address=10a9e8 value=c0000 mask=ff0000 pc=271f14 t=9.1')
        self.assertEqual(check_private_shadows(changed), result)

    def test_private_shadow_join_rejects_missing_wrong_or_stale_writes(self):
        original = self.shadow_trace()
        for text in ('', '\n'.join(original.splitlines()[1:]),
                     original.replace('address=10a9e8', 'address=10a9e4'),
                     original.replace('mask=ffff0000', 'mask=ffff'),
                     original.replace('wire=920c', 'wire=9209'),
                     original + '\n' + original.splitlines()[-1],
                     original.replace('field=001a', 'field=bad')):
            with self.subTest(text=text), self.assertRaises(ValueError):
                check_private_shadows(text)

    def test_adjacent_halfword_is_not_a_fresh_shadow_write(self):
        text = self.shadow_trace() + '\n' + '\n'.join([
            'rom4_tone_access: owner=codec_selector9_shadow direction=write address=10a9e8 value=1234 mask=ffff pc=271fd8 t=10.0',
            'rom4_codec_parameter_publish: phase=interactive field=001a wire=920c r5=00000001',
        ])
        with self.assertRaisesRegex(ValueError, 'fresh private-shadow'):
            check_private_shadows(text)

    def test_writer_join_preserves_thumb_vs_bus_context_pc(self):
        report = {'accesses': [{'pc': 0x2721d2, 'kind': 'write', 'width': 2, 'address': 0x30000}]}
        text = 'rom4_tone_access: owner=codec_dspif_mcu direction=write address=030000 value=40000 mask=ffff0000 pc=2721d0 t=0.13'
        check_host_writers(text, report)
        check_host_writers(text.replace('pc=2721d0', 'pc=2721d2'), report)
        for old, new in (('value=40000', 'value=80000'), ('mask=ffff0000', 'mask=ffff'),
                         ('pc=2721d0', 'pc=2721d4'), ('pc=2721d0', 'pc=2721d1'),
                         ('direction=write', 'direction=read')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_host_writers(text.replace(old, new), report)
    def trace(self, option):
        lines = [codec_fixtures.CodecTraceTests().trace()]
        for index, key in enumerate(('menu', '9', '5', str(option), 'clear', 'clear', 'clear', '2')):
            time = 8 + index * .42
            lines.extend((f'input-press: t={time:.6f} name={key} port=1f',
                          f'input-release: t={time + .22:.6f} name={key} port=1d'))
        lines.extend((
            'rom4_codec_control_call: hit=1 a=0000000626 b=0000000008 sp=1ea0 return=a4bb t=8.06',
            'rom4_tone_access: owner=dsp direction=write address=0000fe value=00e1 mask=ffff pc=00a5de t=8.05'))
        count = 1
        copies = 1
        if option in (3, 4):
            word = '920c' if option == 3 else '9209'
            value = '000000020c' if option == 3 else '0000000209'
            lines.extend((
                'rom4_tone_access: owner=codec_shared_mcu direction=write address=0100a8 '
                f'value={word}0000 mask=ffff0000 pc=272034 t=10.98',
                'rom4_tone_access: owner=codec_shared_dsp direction=read address=000854 '
                f'value={word} mask=ffff pc=0027f2 t=10.99',
                f'rom4_codec_control_call: hit=2 a={value} b=0000000009 sp=1ea7 return=a598 t=10.991'))
            count += 1
        if option != 1:
            count += 1
            copies += 1
            lines.extend((
                f'rom4_codec_control_call: hit={count} a=0000000626 b=0000000008 sp=1ea0 return=a4bb t=11.02',
                'rom4_tone_access: owner=dsp direction=write address=0000fe value=00e1 mask=ffff pc=00a5de t=11.0'))
        lines.extend((f'rom4_codec_control_summary: calls={count} t=15.0',
                      f'rom4_tone_summary: tx_words=2 rx_reads=2 tone_reads=40 tone_copies={copies}'))
        return '\n'.join(lines)

    def test_all_settings(self):
        for option in range(1, 5):
            with self.subTest(option=option):
                check_level(self.trace(option), option)

    def test_wrong_sequences_and_observation_bounds(self):
        for before, after in (
                ('name=clear', 'name=menu'), ('input-release:', 'lost-release:'),
                ('t=15.0', 't=11.2'), ('calls=3', 'calls=4'),
                ('tone_copies=2', 'tone_copies=3'), ('tx_words=2', 'tx_words=3'),
                ('t=8.05', 't=7.05'), ('hit=2', 'hit=1')):
            with self.subTest(before=before), self.assertRaises(ValueError):
                check_level(self.trace(3).replace(before, after), 3)

    def test_enabled_requires_tone_and_activation(self):
        for before, after in (('value=00e1 mask=ffff pc=00a5de t=11.0',
                               'value=0001 mask=ffff pc=00a5de t=11.0'),
                              ('return=a4bb t=11.02', 'return=a598 t=11.02')):
            with self.subTest(before=before), self.assertRaises(ValueError):
                check_level(self.trace(2).replace(before, after), 2)

    def test_off_rejects_late_activation_and_tone(self):
        text = self.trace(1)
        for line in (
                'rom4_codec_control_call: hit=2 a=0000000626 b=0000000008 sp=1ea0 return=a4bb t=12.0',
                'rom4_tone_access: owner=dsp direction=write address=0000fe value=00e1 mask=ffff pc=00a5de t=12.0'):
            changed = text + '\n' + line
            changed = changed.replace('calls=1', 'calls=2') if 'hit=2' in line else changed.replace('tone_copies=1', 'tone_copies=2')
            with self.subTest(line=line), self.assertRaisesRegex(ValueError, 'Off still emits'):
                check_level(changed, 1)

    def test_level_controls_require_correct_lane_and_order(self):
        for before, after in (('920c0000', '92090000'), ('mask=ffff0000', 'mask=ffff'),
                              ('t=10.98', 't=11.03')):
            with self.subTest(before=before), self.assertRaises(ValueError):
                check_level(self.trace(3).replace(before, after), 3)

    def test_packed_control_must_survive_native_consumption(self):
        for before, after in (('value=920c mask=ffff', 'value=0000 mask=ffff'),
                              ('pc=0027f2', 'pc=0027f4'),
                              ('a=000000020c', 'a=0000000000'),
                              ('b=0000000009', 'b=0000000000'),
                              ('t=10.991', 't=11.01')):
            with self.subTest(before=before), self.assertRaises(ValueError):
                check_level(self.trace(3).replace(before, after), 3)

    def test_unknown_setting(self):
        with self.assertRaises(ValueError):
            check_level(self.trace(1), 0)

    def test_blank_and_wrong_size_frames_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'frame.png'
            for size in ((84, 48), (48, 84)):
                with self.subTest(size=size):
                    Image.new('L', size, 255).save(path)
                    with self.assertRaisesRegex(ValueError, 'digit-2 frame'):
                        check_frame(path)


if __name__ == '__main__':
    unittest.main()

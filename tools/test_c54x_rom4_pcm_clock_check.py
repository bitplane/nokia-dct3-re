import functools
import math
import unittest

from tools.c54x_rom4_pcm_clock_check import check, check_wire, check_restore, check_clocked_boot, control_timeline, demand_transitions, tone_fraction, WORDS, reject_native_faults


class NativeFaultTests(unittest.TestCase):
    def test_clean(self):
        reject_native_faults('native_pcm_fixture_echo_summary: words=42 untimed=0 handset_clock_claim=0')

    def test_reject_faults(self):
        for text in ('unsupported staged C54x form 7721 at 000e61',
                     'UNIMPLEMENTED instruction', 'rom4_reset_request: reason=4',
                     'TMS320C54x illegal opcode'):
            with self.subTest(text=text), self.assertRaises(ValueError):
                reject_native_faults(text)


KEYS = 'input-press: t=8.000000 name=2 port=1f\ninput-release: t=8.220000 name=2 port=0f\n'
CONTROL = ('rom4_tone_access: owner=mcu direction=write address=0100ac value=e10000 mask=ffff0000 pc=272034 t=8.100000\n'
           'rom4_tone_access: owner=mcu direction=write address=0100ac value=14e0 mask=ffff pc=272034 t=8.100100\n'
           'rom4_tone_access: owner=mcu_frequency direction=write address=0100b0 value=ae40000 mask=ffff0000 pc=272034 t=8.100200\n')
HELPERS = ('rom4_codec_control_call: hit=1 a=0000000626 b=0000000008 sp=1ea0 return=a4bb t=7.000000000\n'
           'rom4_codec_control_call: hit=2 a=0000000626 b=0000000008 sp=1ea0 return=a4bb t=8.100000000\n'
           'rom4_codec_control_summary: calls=2 t=12.000000000\n')


@functools.lru_cache(maxsize=2)
def clock_log(frequency=697):
    lines = [KEYS, CONTROL, HELPERS, 'rom4_tone_summary: tx_words=60000 rx_reads=60000 tone_reads=20 tone_copies=8 t=12.0']
    for n in range(52001):
        time = 2.5000625 + n / 8000
        value = round(1800 * (math.sin(2 * math.pi * frequency * time) + math.sin(2 * math.pi * 1336 * time))) if 8.11 <= time < 8.22 else 0
        lines.append(f'native_pcm_fixture_word: value={value & 0xffff:04x} t={time:.9f}')
    return '\n'.join(lines)


def restore_log():
    fields = ['t=8.150000000', 'mcu_PC=0025432c', 'mcu_CPSR=0000003f']
    for name in ('PC', 'SP', 'ST0', 'ST1', 'A', 'B', 'T', *(f'AR{i}' for i in range(8)), 'IFR', 'IMR'):
        fields.append(f'dsp_{name}=0000000000')
    fields.extend(f'mcu_R{i}=00000000' for i in range(16))
    fields.extend(f'data_{address:04x}=0000' for address in (0x21, 0x22, 0x23, *range(0xf0, 0xff)))
    snapshot = ' '.join(fields)
    lines = clock_log().splitlines()
    begin = next(i for i, line in enumerate(lines) if WORDS.fullmatch(line) and float(WORDS.fullmatch(line)[2]) >= 8.15)
    end = next(i for i, line in enumerate(lines) if WORDS.fullmatch(line) and float(WORDS.fullmatch(line)[2]) >= 8.2)
    suffix = lines[begin:end]
    return '\n'.join([*lines[:begin], 'native_pcm_restore: phase=saved ' + snapshot,
                      *suffix, 'native_pcm_restore: phase=restored ' + snapshot,
                      *suffix, 'native_pcm_restore: phase=complete t=8.200000000 physical_release_replayed=0',
                      *lines[end:]])


class NativeClockTests(unittest.TestCase):
    def test_clocked_boot_requires_real_delayed_self_test_consumption(self):
        text = ('rom4_serial_audit: space=data direction=write address=0021 value=0aaa mask=ffff pc=0e31 t=0.100000000\n'
                'native_pcm_fixture_echo_word: value=0aaa t=0.100016000\n'
                'rom4_codec_boot_echo: value=0aaa pc=0e5d t=0.100017000\n'
                'native_pcm_fixture_echo_summary: words=1 untimed=0 handset_clock_claim=0\n')
        result = check_clocked_boot(text)
        self.assertEqual(result['clocked_echo_words'], 1)
        self.assertEqual(result['untimed_loopbacks'], 0)
        self.assertAlmostEqual(result['first_echo_latency_us'], 17)
        staged = text.replace('pc=0e5d', 'pc=0e60')
        self.assertEqual(check_clocked_boot(staged, staged=True), result)
        with self.assertRaises(ValueError):
            check_clocked_boot(staged)
        with self.assertRaises(ValueError):
            check_clocked_boot(text, staged=True)
        for bad in ('', text + text, text.replace('untimed=0', 'untimed=1'),
                    text.replace('words=1', 'words=2'), text.replace('value=0aaa', 'value=0555'),
                    text.replace('pc=0e5d', 'pc=3464'), text.replace('t=0.100017000', 't=0.100015000'),
                    text.replace('t=0.100016000', 't=0.100016500')):
            with self.subTest(text=bad), self.assertRaises(ValueError):
                check_clocked_boot(bad)

    def test_wire_phase_is_not_only_a_sample_rate_check(self):
        text = WORDS.sub(lambda match: f'native_pcm_fixture_word: value={match[1]} t={round((float(match[2]) - 0.000016) * 8000) / 8000 + 0.000016:.9f}', clock_log())
        self.assertEqual(check_wire(text)['frame_clocks'], 125)
        shifted = WORDS.sub(lambda match: f'native_pcm_fixture_word: value={match[1]} t={float(match[2]) + 0.0000005:.9f}', text)
        self.assertTrue(check(shifted, clock=True)['clock'])
        with self.assertRaisesRegex(ValueError, 'phase'):
            check_wire(shifted)
        with self.assertRaisesRegex(ValueError, 'partial word'):
            check_restore(restore_log(), wire=True)
        first = next(WORDS.finditer(text))
        bad_word = text[:first.start()] + f'native_pcm_fixture_word: value=1000 t={first[2]}' + text[first.end():]
        self.assertTrue(check(bad_word, clock=True)['clock'])
        with self.assertRaisesRegex(ValueError, '13-bit'):
            check_wire(bad_word)

    def test_demand_bitmap_acquire_release_and_complete_commits(self):
        text = ('rom4_codec_demand_entry: hit=1 operation=acquire mask=0001 previous=0002 t=1.0\n'
                'rom4_tone_access: owner=codec_demand direction=write address=0000ef value=0003 mask=ffff pc=00a4c4 t=1.1\n'
                'rom4_codec_demand_entry: hit=2 operation=release mask=0001 previous=0003 t=1.2\n'
                'rom4_tone_access: owner=codec_demand direction=write address=0000ef value=0002 mask=ffff pc=00a4e0 t=1.3\n'
                'rom4_codec_demand_summary: entries=2 t=2.0\n')
        self.assertEqual(demand_transitions(text)['transitions'], 2)
        staged = text.replace('pc=00a4c4', 'pc=00a4c3').replace('pc=00a4e0', 'pc=00a4df')
        self.assertEqual(demand_transitions(staged, staged=True), demand_transitions(text))
        with self.assertRaises(ValueError):
            demand_transitions(staged)
        for bad in (text.replace('value=0003', 'value=0001'),
                    text.replace('value=0002', 'value=0000'),
                    text.replace('pc=00a4e0', 'pc=00a4c4'),
                    text.replace('entries=2', 'entries=3'),
                    text.replace('hit=2', 'hit=3'),
                    text.replace('t=1.3', 't=1.0')):
            with self.assertRaises(ValueError):
                demand_transitions(bad)

    def test_complete_control_timeline_without_bit_semantics(self):
        result = control_timeline(HELPERS)
        self.assertEqual(result['register8_values'], dict(before=[0x626], active=[0x626], after=[0x626]))
        self.assertFalse(result['physical_clock_enable_claim'])
        for bad in (HELPERS.replace('calls=2', 'calls=3'),
                    HELPERS.replace('hit=2', 'hit=1'),
                    HELPERS.replace('t=12.000000000', 't=8.000000000'),
                    HELPERS.replace('b=0000000008', 'b=0000000009')):
            with self.assertRaises(ValueError):
                control_timeline(bad)
        changed = HELPERS.replace('hit=2 a=0000000626', 'hit=2 a=0000000026')
        self.assertEqual(control_timeline(changed)['register8_values']['active'], [0x26])

    def test_exact_active_restore_and_snapshot_required(self):
        text = restore_log()
        result = check_restore(text)
        self.assertEqual(result['replayed_words'], 400)
        self.assertTrue(result['snapshot_equal'])
        self.assertTrue(result['absolute_frame_times_equal'])
        self.assertEqual(result['captured_words'], result['canonical_captured_words'] + 400)
        for bad in (text.replace('mcu_R0=00000000 ', ''),
                    text.replace('phase=restored t=8.150000000', 'phase=restored t=8.150000001'),
                    text.replace('phase=complete t=8.200000000', 'phase=complete t=8.230000000'),
                    text.replace('physical_release_replayed=0', 'physical_release_replayed=1'),
                    text.replace('input-release: t=8.220000', 'input-release: t=8.150000'),
                    text.replace('mcu_CPSR=0000003f', 'mcu_CPSR=bad'),
                    text.replace('phase=restored', 'phase=saved')):
            with self.subTest(bad=bad[-100:]), self.assertRaises(ValueError):
                check_restore(bad)

    def test_replayed_words_and_absolute_times_cannot_drift(self):
        text = restore_log()
        position = text.index('native_pcm_restore: phase=restored')
        head, tail = text[:position], text[position:]
        record = next(WORDS.finditer(tail))
        for replacement in (f'native_pcm_fixture_word: value=1234 t={record[2]}',
                            f'native_pcm_fixture_word: value={record[1]} t=8.150062600', ''):
            changed = head + tail[:record.start()] + replacement + tail[record.end():]
            with self.assertRaisesRegex(ValueError, 'words or absolute'):
                check_restore(changed)

    def test_native_waveform_and_unclocked_control(self):
        result = check(clock_log(), clock=True)
        self.assertEqual(result['active_samples'], 480)
        self.assertGreater(sum(result['sinusoidal_energy_fractions']), 0.99)
        self.assertFalse(result['handset_clock_claim'])
        self.assertFalse(result['native_call_speech_claim'])
        control = KEYS + CONTROL + 'rom4_tone_summary: tx_words=2 rx_reads=2 tone_reads=20 tone_copies=8 t=12.0'
        check(control, clock=False)
        with self.assertRaises(ValueError):
            check(control.replace('tx_words=2', 'tx_words=3'), clock=False)

    def test_wrong_frequency_and_non_sinusoidal_output(self):
        with self.assertRaisesRegex(ValueError, 'DTMF'):
            check(clock_log(450), clock=True)
        samples = [2000 if math.sin(2 * math.pi * 900 * n / 8000) >= 0 else -2000 for n in range(480)]
        self.assertLess(tone_fraction(samples), 0.9)
        self.assertEqual(tone_fraction([0] * 480), 0)

    def test_missing_or_retimed_samples_cannot_pass(self):
        for bad in (clock_log().replace('t=8.150062500', 't=8.150062600'),
                    clock_log().replace('native_pcm_fixture_word: value=0000 t=7.500062500', ''),
                    clock_log().replace('t=8.150062500', 't=8.150187500')):
            with self.assertRaises(ValueError):
                check(bad, clock=True)

    def test_key_and_silence_controls_required(self):
        for bad in (clock_log().replace('name=2', 'name=3'),
                    clock_log().replace('value=14e0', 'value=14e1'),
                    clock_log().replace('t=8.100100', 't=8.099900'),
                    clock_log().replace('tx_words=60000', 'tx_words=2'),
                    clock_log().replace('tone_copies=8', 'tone_copies=0'),
                    clock_log().replace('value=0000 t=7.500062500', 'value=0001 t=7.500062500'),
                    clock_log().replace('value=0000 t=8.600062500', 'value=0001 t=8.600062500')):
            with self.assertRaises(ValueError):
                check(bad, clock=True)


if __name__ == '__main__':
    unittest.main()

from pathlib import Path
from fractions import Fraction
import struct
import unittest

from tools.mu4_clock_contract import contract, decode_clkmd, check_trace, check_clock_rates, transition_seconds, WRITES
from tools.noki5510_a00_inventory import uploaded_word_image


class ClockContractTest(unittest.TestCase):
    def test_selected_rates_keep_external_codec_clock(self):
        rates = [5_997_600, 95_961_600, 5_997_600, 119_952_000]
        delays = [0, transition_seconds(0x77fb, entering_pll=True),
                  transition_seconds(0x77fb, entering_pll=False), transition_seconds(0x97fb, entering_pll=True)]
        text = self.trace()
        for index, (rate, delay) in enumerate(zip(rates, delays)):
            text += (f'\nmu4_system_clock_selected: cpu={rate} dma={rate} mcbsp0={rate} '
                     f'mcbsp1={rate} mcbsp2={rate} codec=11995200 t={float(index + 1 + delay):.9f}')
        self.assertTrue(check_clock_rates(text)['execution_clock_propagated'])
        reset = text.replace('codec=11995200 t=1.000000000', 'codec=11995200 t=0.000000000')
        marker = '\nmu4_clock_reset_profile: pins=111 clkmd=0000 divider=2 family=C5416_compatible exact_da150_identity=0'
        self.assertTrue(check_clock_rates(reset + marker)['execution_clock_propagated'])
        for wrong in (text.replace('codec=11995200', 'codec=119952000'),
                      text.replace('dma=95961600', 'dma=13000000'),
                      text.replace('t=2.000340673', 't=2.000000000'),
                      text + '\n' + text.splitlines()[-1], self.trace(), reset,
                      reset + marker.replace('pins=111', 'pins=000')):
            with self.assertRaises(ValueError):
                check_clock_rates(wrong)

    def test_pll_transition_delay(self):
        # Original x8/x10 requests: 255*16 lock ticks, six input ticks,
        # then 3.5 ticks of the destination PLL clock.
        for value, multiplier in ((0x77fb, 8), (0x97fb, 10)):
            expected = Fraction(4086, 11_995_200) + Fraction(7, 2 * multiplier * 11_995_200)
            self.assertEqual(transition_seconds(value, entering_pll=True), expected)
            locked = transition_seconds(value, entering_pll=True, locked=True)
            self.assertEqual(expected - locked, Fraction(4080, 11_995_200))
            self.assertEqual(transition_seconds(value, entering_pll=False), locked)

    def test_bypass_exit_and_fractional_pll_delay(self):
        self.assertEqual(transition_seconds(0xf002, entering_pll=False, input_hz=1), Fraction(31, 2))
        self.assertEqual(transition_seconds(0xf002, entering_pll=True, input_hz=1), Fraction(19, 2))
        self.assertEqual(transition_seconds(0x3802, entering_pll=True, input_hz=1), Fraction(32, 3))
        with self.assertRaises(ValueError):
            transition_seconds(0, entering_pll=True, input_hz=0)

    def test_requested_rates(self):
        self.assertEqual([decode_clkmd(value)['requested_hz'] for value in (0x6004, 0x77fb, 0x97fb)],
                         [5_997_600, 95_961_600, 119_952_000])
        self.assertEqual(decode_clkmd(0x97fb)['lock_count_input_cycles'], 4080)
        self.assertFalse(decode_clkmd(0x97fb)['physical_system_clock_validated'])

    def test_divider_and_fractional_modes(self):
        self.assertEqual(decode_clkmd(0xf000)['ratio'], [1, 4])
        self.assertEqual(decode_clkmd(0xf002)['ratio'], [1, 1])
        self.assertEqual(decode_clkmd(0x2802)['ratio'], [3, 2])
        self.assertEqual(decode_clkmd(0x3802)['ratio'], [3, 4])
        self.assertEqual(decode_clkmd(0x2802, 1)['requested_hz_fraction'], [3, 2])
        self.assertIsNone(decode_clkmd(0x2802, 1)['requested_hz'])
        for value, clock in ((-1, 1), (0x10000, 1), (0, 0)):
            with self.assertRaises(ValueError):
                decode_clkmd(value, clock)

    def test_all_clock_table_selections(self):
        fractional = [(1, 2), (1, 4), (3, 2), (3, 4), (5, 2), (5, 4), (7, 2), (7, 4),
                      (9, 2), (9, 4), (11, 2), (11, 4), (13, 2), (13, 4), (15, 2), (15, 4)]
        for multiplier in range(16):
            for divider in range(2):
                value = multiplier << 12 | divider << 11
                self.assertEqual(decode_clkmd(value)['ratio'], [1, 4 if multiplier == 15 else 2])
                expected = list(fractional[multiplier]) if divider else [multiplier + 1 if multiplier < 15 else 1, 1]
                self.assertEqual(decode_clkmd(value | 2)['ratio'], expected)

    def test_serial_boot_map_skips_compatibility_header(self):
        payload = struct.pack('>12H', 0x08aa, 0x18, 3, 0x800, 0x10, 0, 0x900,
                              1, 0, 0x900, 0x1234, 0)
        checksum = 0
        for byte in payload:
            checksum ^= byte
        image = struct.pack('>HI', 0xaa55, len(payload)) + payload + struct.pack('>HH', checksum, 0x8888)
        self.assertEqual(uploaded_word_image(image, 'aa55'), {0x900: 0x1234})
        # An overlay count can equal a serial signature; its marker owns the grammar.
        overlay = struct.pack('>3H', 0x08aa, 2, 0x8000) + b'\x12\x34' * 0x08aa + b'\0\0'
        checksum = 0
        for byte in overlay:
            checksum ^= byte
        image += struct.pack('>HI', 0xaa22, len(overlay)) + overlay + struct.pack('>HH', checksum, 0x8888)
        mapped = uploaded_word_image(image, 'aa22')
        self.assertEqual(len(mapped), 0x08aa)
        self.assertEqual(mapped[0x28000], 0x1234)

    def trace(self):
        lines = ['mu4_clock_snapshot: phase=initial resets=0 clkmd=0000 t=0.000000000']
        previous = ['0000', '6004', '77fb', '77f8']
        lines += [f'mu4_clock_write: pc={address + 4:06x} value={value:04x} mask=ffff previous={previous[index]} t={index + 1:.9f}'
                  for index, (_, address, value) in enumerate(WRITES)]
        lines += ['mu4_clock_summary: resets=0 writes=4 clkmd=97fb t=20.000000000',
                  'mu4_original_bootstrap_frontier: entry_count=1 resident_count=1 pc=023d86',
                  'mu4_native_status_transaction: PASS complete=1', 'mu4_native_receive_pins: PASS complete=1']
        return '\n'.join(lines)

    def test_runtime_requires_full_observation(self):
        text = self.trace()
        self.assertEqual(check_trace(text)['writes'], 4)
        for wrong in (text.replace('value=97fb', 'value=77fb'), text.replace('mask=ffff', 'mask=00ff'),
                      text.replace('t=0.000000000', 't=2.000000000'), text.replace('writes=4', 'writes=0'),
                      text.replace('previous=77f8', 'previous=0000'), text.replace('t=4.000000000', 't=1.000000000'),
                      text.replace('mu4_native_receive_pins: PASS', 'mu4_native_receive_pins: FAIL'),
                      text + '\n' + text):
            with self.assertRaises(ValueError):
                check_trace(wrong)

    def test_original_source(self):
        path = Path('roms/5510-mu4-reference/InitData_R060.a00')
        if not path.exists():
            self.skipTest('local original R060 source unavailable')
        self.assertEqual(contract(path.read_bytes())['writers'][-1]['requested_hz'], 119_952_000)
        with self.assertRaises(ValueError):
            contract(b'not the acquired source')


if __name__ == '__main__':
    unittest.main()

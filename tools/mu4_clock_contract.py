"""Read-only R060 CLKMD contract; requested rates are not physical clock measurements."""
import argparse
from fractions import Fraction
import hashlib
import json
from pathlib import Path
import re

if __package__:
    from .noki5510_a00_inventory import uploaded_word_image
else:
    from noki5510_a00_inventory import uploaded_word_image

SOURCE_SHA256 = '65118e00dfd8d78726a45e781e2c7c96714228ec025bc5ccb141d445d85db094'
INPUT_HZ = 11_995_200
WRITES = (('aa55', 0x923, 0x6004), ('aa55', 0x92b, 0x77fb),
          ('aa22', 0x2905d, 0x6004), ('aa22', 0x29065, 0x97fb))


def decode_clkmd(value, input_hz=INPUT_HZ):
    """SPRU131G tables 8-18/19: requested selection, not PLL lock status."""
    if not 0 <= value <= 0xffff or input_hz <= 0:
        raise ValueError('invalid CLKMD value or input clock')
    multiplier = value >> 12
    pll = bool(value & 2)
    if not pll:
        ratio = Fraction(1, 4 if multiplier == 15 else 2)
    elif not value & 0x800:
        ratio = Fraction(1 if multiplier == 15 else multiplier + 1)
    else:
        ratio = Fraction(multiplier, 4) if multiplier & 1 else Fraction(multiplier + 1, 2)
    rate = input_hz * ratio
    return {'clkmd': f'{value:04x}', 'requested_mode': 'PLL' if pll else 'DIV',
            'ratio': [ratio.numerator, ratio.denominator],
            'requested_hz': rate.numerator if rate.denominator == 1 else None,
            'requested_hz_fraction': [rate.numerator, rate.denominator],
            'lock_count_input_cycles': ((value >> 3) & 0xff) * 16,
            'physical_system_clock_validated': False}


def transition_seconds(value, *, entering_pll, locked=False, input_hz=INPUT_HZ):
    """SPRU131G pp. 8-32/33; value supplies the PLL configuration on either exit.

    For PLL -> DIV use the old PLL fields, not the protected write's fields.
    Reset straps and IDLE3 wake timing are separate contracts.
    """
    pll = decode_clkmd(value | 2, input_hz)
    rate = Fraction(*pll['requested_hz_fraction'])
    input_cycles = 6 if entering_pll or value >> 12 != 15 else 12
    if entering_pll and not locked:
        input_cycles += pll['lock_count_input_cycles']
    return Fraction(input_cycles, input_hz) + Fraction(7, 2) / rate


def contract(source):
    if hashlib.sha256(source).hexdigest() != SOURCE_SHA256:
        raise ValueError('expected original R060 InitData source')
    image = bytes.fromhex(source.decode('ascii'))
    words = {marker: uploaded_word_image(image, marker) for marker in ('aa55', 'aa22')}
    rows = []
    for marker, address, value in WRITES:
        # STM #0058,AR1; STM #value,*AR1. Do not scan arbitrary data as instructions.
        expected = (0x7711, 0x0058, 0x7681, value)
        actual = tuple(words[marker].get(address + offset) for offset in range(4))
        if actual != expected:
            raise ValueError(f'CLKMD writer changed at {marker}:{address:06x}')
        rows.append({'segment': marker, 'setup_word_address': f'{address:06x}',
                     'observed_next_pc': f'{address + 4:06x}', **decode_clkmd(value)})
    delays = []
    for value in (0x77fb, 0x97fb):
        duration = transition_seconds(value, entering_pll=True)
        delays.append({'clkmd': f'{value:04x}',
                       'unlocked_div_to_pll_seconds': [duration.numerator, duration.denominator],
                       'unlocked_div_to_pll_us': float(duration * 1_000_000)})
    return {'source_sha256': SOURCE_SHA256, 'schematic_input_hz': INPUT_HZ,
            'reset_contract': {'clock_pins': [1, 1, 1], 'pullup_ohms': 100_000,
                               'u101_pins': [77, 78, 79], 'r101_pairs': [[7, 2], [6, 3], [8, 1]],
                               'compatible_family': 'C5416', 'clkmd': '0000',
                               'requested_hz': INPUT_HZ // 2, 'exact_da150_identity': False},
            'writers': rows,
            'ti_family_transition_predictions': delays,
            'scope': 'four pinned original writers and TI-family clock selection; no complete writer census or physical DA150 reset/PLL clock measurements'}


def check_trace(text):
    pattern = r'mu4_clock_write: pc=([0-9a-f]{6}) value=([0-9a-f]{4}) mask=([0-9a-f]{4}) previous=([0-9a-f]{4}) t=([0-9.]+)$'
    rows = re.findall(pattern, text, re.MULTILINE)
    expected = [(f'{address + 4:06x}', f'{value:04x}', 'ffff') for _, address, value in WRITES]
    if text.count('mu4_clock_write:') != len(expected) or [row[:3] for row in rows] != expected:
        raise ValueError('original four-write CLKMD sequence missing or changed')
    if [row[3] for row in rows] != ['0000', '6004', '77fb', '77f8']:
        raise ValueError('previous CLKMD values differ from the observed modeled sequence')
    times = [float(row[4]) for row in rows]
    if not 0 < times[0] < times[1] < times[2] < times[3] < 20:
        raise ValueError('CLKMD write timestamps must be ordered within the observation window')
    initial = re.findall(r'mu4_clock_snapshot: phase=initial resets=0 clkmd=0000 t=0\.000000000$', text, re.MULTILINE)
    summary = re.findall(r'mu4_clock_summary: resets=0 writes=4 clkmd=97fb t=20\.000000000$', text, re.MULTILINE)
    if len(initial) != 1 or len(summary) != 1:
        raise ValueError('clock observation must cover time zero through the full 20-second endpoint')
    for marker in ('mu4_original_bootstrap_frontier: entry_count=1 resident_count=1 ',
                   'mu4_native_status_transaction: PASS ', 'mu4_native_receive_pins: PASS '):
        if text.count(marker) != 1:
            raise ValueError(f'missing or duplicated native startup boundary: {marker}')
    return {'writes': len(rows), 'final_clkmd': '97fb', 'native_status_transaction': True,
            'physical_system_clock_validated': False}


def check_clock_rates(text):
    """Original write -> modeled source switch; never a physical frequency oracle."""
    check_trace(text)
    pattern = (r'mu4_system_clock_selected: cpu=(\d+) dma=(\d+) mcbsp0=(\d+) '
               r'mcbsp1=(\d+) mcbsp2=(\d+) codec=(\d+) t=([0-9.]+)$')
    raw = re.findall(pattern, text, re.MULTILINE)
    if text.count('mu4_system_clock_selected:') != len(raw):
        raise ValueError('malformed system-clock publication')
    rows = []
    for row in raw:
        rates = [int(value) for value in row[:6]]
        time = Fraction(row[6])
        if rates[:5] != [rates[0]] * 5 or rates[5] != INPUT_HZ:
            raise ValueError('system-clock consumers diverged or external codec clock changed')
        if rates[0] == 13_000_000 and time == 0:
            continue  # Explicitly provisional reset rate, not CLKMD-derived.
        rows.append((rates[0], time))
    expected = [5_997_600, 95_961_600, 5_997_600, 119_952_000]
    if [rate for rate, _ in rows] != expected:
        raise ValueError('missing or unexpected original CLKMD system-clock sequence')
    writes = re.findall(r'mu4_clock_write: .* t=([0-9.]+)$', text, re.MULTILINE)
    delays = [Fraction(0), transition_seconds(0x77fb, entering_pll=True),
              transition_seconds(0x77fb, entering_pll=False),
              transition_seconds(0x97fb, entering_pll=True)]
    reset_marker = 'mu4_clock_reset_profile: pins=111 clkmd=0000 divider=2 family=C5416_compatible exact_da150_identity=0'
    for index, ((_, time), write, delay) in enumerate(zip(rows, writes, delays)):
        if index == 0 and time == 0:
            if text.count(reset_marker) != 1:
                raise ValueError('reset-derived divider rate requires the explicit compatibility profile')
            continue
        if abs(time - Fraction(write) - delay) > Fraction(3, 1_000_000_000):
            raise ValueError('source switch differs from input-derived transition deadline')
    return {'selected_hz': expected, 'external_codec_hz': INPUT_HZ,
            'execution_clock_propagated': True, 'physical_system_clock_validated': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('--trace', type=Path)
    parser.add_argument('--clock-rates', action='store_true', help='require selected-rate publication in --trace')
    args = parser.parse_args()
    try:
        report = contract(args.image.read_bytes())
        if args.trace:
            trace = args.trace.read_text()
            report['runtime'] = check_trace(trace)
            if args.clock_rates:
                report['clock_rates'] = check_clock_rates(trace)
        elif args.clock_rates:
            raise ValueError('--clock-rates requires --trace')
        print(json.dumps(report, indent=2))
    except (OSError, ValueError, UnicodeError) as error:
        parser.exit(1, f'MU4 clock contract: {error}\n')


if __name__ == '__main__':
    main()

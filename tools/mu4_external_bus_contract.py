"""Pinned MU4 external-bus requests under a C5416 compatibility hypothesis."""
import argparse
import hashlib
import json
from pathlib import Path

if __package__:
    from .mu4_clock_contract import SOURCE_SHA256
    from .noki5510_a00_inventory import uploaded_word_image
else:
    from mu4_clock_contract import SOURCE_SHA256
    from noki5510_a00_inventory import uploaded_word_image


def word(value):
    if not isinstance(value, int) or not 0 <= value <= 0xffff:
        raise ValueError('expected a 16-bit control value')
    return value


def wait_states(swwsr, swcr, space, address, *, external):
    """SPRS095P table 3-3/3-4; wait component only, not instruction duration."""
    word(swwsr)
    word(swcr)
    if space not in ('program', 'data', 'io'):
        raise ValueError('unknown address space')
    limit = 0x7fffff if space == 'program' else 0xffff
    if not isinstance(address, int) or not 0 <= address <= limit:
        raise ValueError('address outside the selected space')
    if not isinstance(external, bool):
        raise ValueError('external/internal ownership must be explicit')
    if space == 'io' and not external:
        raise ValueError('C5416 I/O transactions use the external IS bus')
    if not external:
        return 0
    if space == 'io':
        shift = 12
    elif space == 'data':
        shift = 9 if address & 0x8000 else 6
    else:
        upper = bool(address & (0x400000 if swwsr & 0x8000 else 0x8000))
        shift = 3 if upper else 0
    return ((swwsr >> shift) & 7) * (1 + (swcr & 1))


def decode_bscr(value):
    """SPRS095P table 3-5, not SPRU131G's older BNKCMP/PS-DS dialect."""
    word(value)
    return {'written_value': f'{value:04x}', 'writable_value': f'{value & 0xf006:04x}',
            'reserved_written_bits': f'{value & ~0xf006:04x}',
            'consecutive_external_reads': bool(value & 0x8000),
            'external_read_framing_cycles_when_consecutive': 3 if value & 0x8000 else None,
            'clkout_divisor': 1 + ((value >> 13) & 3),
            'iack_output_disabled': bool(value & 0x1000),
            'hpi_bus_holder': bool(value & 4), 'data_bus_holder': bool(value & 2),
            'nonconsecutive_bank_boundary_words': 0x8000}


def contract(source):
    if hashlib.sha256(source).hexdigest() != SOURCE_SHA256:
        raise ValueError('expected original R060 InitData source')
    words = uploaded_word_image(bytes.fromhex(source.decode('ascii')), 'aa22')
    expected = (0xf6be, 0xf495, 0xf495, 0x771d, 0x202c, 0x7728, 0x7fff, 0x7729, 0x8006)
    if tuple(words.get(0x26d00 + index) for index in range(len(expected))) != expected:
        raise ValueError('original pinned resident bus-control prefix changed')
    return {
        'source_sha256': SOURCE_SHA256,
        'profile': 'C5416_compatible', 'exact_da150_identity': False,
        'primary_reference': 'TI SPRS095P sections 3.6.1-3.6.3, tables 3-3/3-4/3-5',
        'reset_hypothesis': {'swwsr': '7fff', 'swcr': '0000', 'bscr': 'f000'},
        'writers': [
            {'segment': 'aa22', 'word_address': '026d05', 'register': 'SWWSR',
             'register_address': '0028', 'value': '7fff', 'xpa': False},
            {'segment': 'aa22', 'word_address': '026d07', 'register': 'BSCR',
             'register_address': '0029', **decode_bscr(0x8006)},
        ],
        'wait_prediction_assuming_reset_swcr': {
            space: wait_states(0x7fff, 0, space, address, external=True)
            for space, address in (('program', 0x26d05), ('data', 0xc281), ('io', 0x4000))
        },
        'external_io_evidence': 'C5416 section 3.7: PORTR/PORTW assert IS for off-chip I/O; MU4 NAND uses I/O 4000..7fff',
        'open_contracts': [
            'DA150 physical internal/external program/data bank ownership',
            'SWCR value before these pinned writes; no complete writer census',
            'READY wiring and external memory timing',
            'CLKOUT and IACK pin consumers and bus-holder electrical behavior',
        ],
        'scope': 'two pinned original writers and C5416-compatible field predictions; no bus-cycle measurement, native boot or implemented bus behavior',
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(contract(args.image.read_bytes()), indent=2))
    except (OSError, ValueError, UnicodeError) as error:
        parser.exit(1, f'MU4 external-bus contract: {error}\n')


if __name__ == '__main__':
    main()

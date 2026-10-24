"""Verify acquired service-loader identities and bounded bootstrap checksums."""
import argparse
import hashlib
import json
from pathlib import Path
import struct

KNOWN = {
    'DCT3BT2.TIA': (956, '45054e2be9d227a6ad7096277eb25f8e4e450546f438a19782cbc3296ef1d7ad'),
    'WD1BT2.TIA': (844, '1e76600ccc762089d6d5883c68419cab6a9871e673ec9e3996ee8690df8acb39'),
    'I28F008.TIA': (4676, '3433fd08b76b6492fd389fa655fb612fc7dc89960c4770696327a260aa31b263'),
    'I28F160.TIA': (6092, 'e925e1778855de3c57b8c289f5d84a5821dcef626381a8d426fa2cb58ff04d3f'),
    'M28W320C.TIA': (5996, '1c7952d6f048d230f75e1de5b7936aba681f01793f88a4f1e0e080d15057cef0'),
}
BOOTSTRAP_SITES = {
    'DCT3BT2.TIA': (0x7a, 0x82, 0x140, 0x37c),
    'WD1BT2.TIA': (0x7c, 0x84, 0x142, 0x30c),
}


def pc_sub_table(data, site, register):
    if site < 0 or site + 6 > len(data) or register not in (1, 2):
        raise ValueError('bounded bootstrap table site required')
    move, subtract, load = struct.unpack_from('>3H', data, site)
    if (move != 0x4678 + register or subtract & 0xff00 != 0x3800 + (register << 8)
            or load != (0x5809 if register == 1 else 0x5a10)):
        raise ValueError('bootstrap PC/subtract/indexed-load sequence differs')
    # High-register MOV uses PC+4, not LDR-literal's word-aligned PC.
    address = site + 4 - (subtract & 255)
    if address < 0 or address + (16 if register == 1 else 8) > len(data):
        raise ValueError('bootstrap table exceeds image')
    return address


def bootstrap_setup(data, sites):
    stack_site, config_site, serial_site, receive_site = sites
    stack_table = pc_sub_table(data, stack_site, 1)
    config_table = pc_sub_table(data, config_site, 1)
    serial_table = pc_sub_table(data, serial_site, 2)
    stacks = struct.unpack_from('>4I', data, stack_table)
    configs = struct.unpack_from('>4I', data, config_table)
    serial = struct.unpack_from('>4H', data, serial_table)
    if data[0x58:0x66] != bytes.fromhex('220204177838468006800f804681'):
        raise ValueError('bootstrap ID read/selector construction differs')
    if data[receive_site:receive_site + 10] != bytes.fromhex('78f90989d3fc793846f7'):
        raise ValueError('bootstrap receive handshake differs')
    transmit = receive_site + 10
    if data[transmit:transmit + 32] != bytes.fromhex(
            'b501182d220878f909c9d2fc01c170f9084078f909c9d3fc3a01d1f4793abd01'):
        raise ValueError('bootstrap eight-bit transmit handshake differs')
    return dict(id_read_address=0x20000, selector_bits='5:4',
                table_offsets=[stack_table, config_table, serial_table],
                branches=[dict(selector=i, stack=stacks[i], configuration_base=configs[i],
                               serial_base=0x20000 + serial[i]) for i in range(4)],
                receive_site=receive_site, transmit_site=transmit,
                status_offset=3, receive_offset=4, receive_ready_mask=0x20,
                clock_sample_mask=0x40, transmit_bit_mask=0x80,
                transmit_bits=8, transmit_order='lsb_first',
                controller_identity_proved=False, physical_timing_proved=False)


def checksum_words(data, span):
    if span <= 0 or span & 1 or span > len(data):
        raise ValueError('checksum span must contain bounded complete halfwords')
    return sum(word[0] for word in struct.iter_unpack('>H', data[:span])) & 0xffff


def bootstrap_span(data):
    if len(data) < 0x1a:
        raise ValueError('bootstrap prologue is truncated')
    low, high, shift, combine = struct.unpack_from('>4H', data, 0x12)
    if low & 0xff00 != 0x2000 or high & 0xff00 != 0x2200 or shift != 0x0212 or combine != 0x4310:
        raise ValueError('bootstrap length construction differs')
    return (low & 255) | ((high & 255) << 8)


def audit(directory):
    result = []
    for name, (size, expected) in KNOWN.items():
        data = (directory / name).read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        if len(data) != size or digest != expected:
            raise ValueError(f'{name}: acquired member identity differs')
        row = dict(name=name, bytes=len(data), sha256=digest,
                   runtime_execution_proved=False, fitted_product_proved=False)
        if name in ('DCT3BT2.TIA', 'WD1BT2.TIA'):
            span = bootstrap_span(data)
            if data[:6] != b'\xe0\x05DCT3' or span != len(data) - 2 or checksum_words(data, span):
                raise ValueError(f'{name}: bootstrap header/checksum differs')
            row.update(checksum_span=span, checksum=0, excluded_suffix=data[span:].hex(),
                       thumb_relative_entry=14, setup=bootstrap_setup(data, BOOTSTRAP_SITES[name]))
        result.append(row)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    try:
        result = audit(args.directory)
    except (OSError, ValueError) as error:
        parser.exit(1, f'MAD2 service-loader audit FAIL: {error}\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()

"""Qualify acquired NSE-2 v5.53 MCU/PPM and loader inputs, not handset boot."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import struct
import sys
import zipfile

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.dct3_eeprom_descriptor import eeprom_geometry

ARCHIVES = {
    '9027b6beda4f55c2b72242f9d1cc4640fd9302068c5c46e7846fb27f7b7dee75': 'zip',
    '055f1655c135adcdf0f3d0d03f56f23b42e4bf9db0b8d42b4c23b8f3c7bd5a8c': 'sfx',
}
MEMBERS = {
    '5110i MCU-05.53.fls': (0x190000, '21084d1546648364ff9d5ee38176ddca31c7b0862e46d4e14dd15c1aca13d628'),
    '5110i PPM-05.53A.fls': (0x70000, 'a6502decf1a1e6dc3278530b745ce7088c865adcbf25c5a5a08df83d4e1020de'),
}
FLASH_SHA256 = '00ab7e062de9c89718eb81031528a646a79317ba0bad5a272a6d4d3b09c77781'
VERSION = b'V 05.53\n11-01-01\nNSE-2\n(c) NMP.\0'
BASE = 0x200000
TABLE = 0x29bba0
PERIPHERAL_FRAGMENTS = {
    'timer_queue_selector': (0x254c7c, 'b5304ca4792279c3401a0892d3011c23e00122181913691d'),
    'analog_source7': (0x2644e6, '2007f033f82d'),
    'analog_sample_scale': (0x264508, '489a434821e8f035ff4d04080c008028'),
    'analog_sample_range': (0x26458e, '2000882922e100d24291db0b4adb4291dc08882122e100d24291db034ad74291dc002001bd70'),
    'analog_average_count': (0x2645b4, 'b500b081466846691c89f7ffff8f2800d00f49677a882800d0011e407288466888008d0a18808508466888408c0a18808408b001bd00'),
    'startup_completion_gate': (0x25c7d2, '783007000f002806d1a748c4780007000f00280fd1a1'),
    'startup_report14': (0x2969a6, 'b50020012114f7beff9abd00'),
    'startup_state_table': (0x25c59c, '0025c5fa0025c5e60025ca4e0025c97c0025cb6c0025bb0c0025c1b80025c1c20025cd660025cd060025c8560025cdac0025cb040025cb4e0025cb280025cbda0025c732'),
    'dsp_upload_copy': (0x29acb4, '477846c0e92d5ff0e2522001e8b01ff8e8a11ff8e25220015afffffbe8bd5ff0e12fff1e'),
    'eeprom_record_sum': (0x23315c, 'b5f0b0811c0c1c0642a6da3a1ba004000c041c20f023f9341c0725002f00d11342a6da231ba41c3046692201f060fe0346687800194004000c051c7004000c063c012c00d1efe0111c301c391c22f060fdf2e0061e6004000c045de0194004000c052c00d1f61c38f022ffe0f04dfa560a011a6904090c0d06000e001a2804000c051c28b001bdf0'),
    'eeprom_record_consumer': (0x2333ce, '200901804970f7fffec24682486ea9012202f060fcd720290100210644692202f060fcd04668888046514281d137466888c043082800d032464946407308'),
    'eeprom_record_exclusion': (0x280678, 'b500b081209d008046692202f013fb8546688800b001bd00'),
    'dsp_loader_startup_poll': (0x2781fa, '88a049a24288d11220004682200af022fd4846502814dc081c4004000c00468288a1489a4281d0f1e0014640704788a14640810188e04281d14f464078402800d04b'),
    'dsp_loader_failure': (0x2782d4, '464070477007'),
    'eeprom_selftest': (0x27dc02, 'b500f7dbf96a2801d00121fde000210048cc74817c80bd00'),
    'eeprom_checksum_sum': (0x28a0c4, '1c0220002900d00d1e4904090c097813181804000c0032011c0b1e5904090c092b00d1f446f7'),
    'eeprom_checksum_validator': (0x258edc, 'b530b08120090180f7fdfb641c0420001c212214f03aff4f201446692204f03aff4a25001c202114f031f8de43c198004281d100250120181c21220cf03aff3b202446692204f03aff361c20210cf031f8cb43c198004281d000250020281c212218f03aff28204046692204f03aff231c202118f031f8b843c198004281d000250020441c2122ff32e1f03aff142089008046692204f03aff0e1c2021ff31e1f031f8a243c198004281d0002500204500c01c212214f03afefe208f008046692204f03afef81c202114f031f88d43c198004281d00025001c20f7fdf8e71c28b001bd30'),
    'eeprom_input': (0x29574e, '25802601270446a6200002ab3320791a781c43b2711a'),
    'eeprom_start': (0x29581a, '22012380029b33207819431170197919431171192004781943017019'),
    'eeprom_stop': (0x29588c, '22012380029b33207819439170197919431171192004781943017019'),
    'eeprom_block_read': (0x293d92, 'b5701c161c0c1c052005f7c1fa781c282100f7ffff2e2800d1012400e00f2e02db081e752001f001fcc5702034013d012d00d1f72000f001fcbd70202401f001fd592005f7c1fabf1c20bd70'),
    'ccont_read': (0x296704, '4835222821225411222a21044321541121295c0908c9d3fb212d5c08'),
    'lcd_command': (0x296e46, '4a7621295c890849d3fb212c548846f7'),
    'lcd_data': (0x296eac, '4a5c21295c890849d3fb212b548846f7'),
    'keypad_base': (0x298078, '4c6120335d0006800e80'),
    'keypad_scan': (0x2980a0, '2031231f5503202f26e0550620305d0006c00ec006000e0125ff291f'),
    'keypad_index': (0x2980f4, '008818084665182806000e05'),
    'keypad_decode': (0x299010, 'b500f7fff82b28ffd101203ebd0009c12900d00949167809008a188918404913180838807800bd0021194a1178124351184049105c08bd00'),
    'simi_init': (0x28652e, '4c60213820ff550820c07220'),
}
NORMAL_KEYS = bytes.fromhex('3e3e3e3e3e3e1a0102033e19040506183e0708093e170c0a0b')


def peripheral_contract(mcu):
    for name, (address, encoded) in PERIPHERAL_FRAGMENTS.items():
        expected = bytes.fromhex(encoded)
        offset = address - BASE
        if mcu[offset:offset + len(expected)] != expected:
            raise ValueError('NSE-2 peripheral instruction differs: ' + name)
    for address in (0x2967dc, 0x297020, 0x298200, 0x2866b0):
        if mcu[address - BASE:address - BASE + 4] != bytes.fromhex('00020000'):
            raise ValueError('NSE-2 peripheral base literal differs')
    if mcu[0x27df44 - BASE:0x27df48 - BASE] != bytes.fromhex('0010fca0'):
        raise ValueError('NSE-2 EEPROM self-test result base differs')
    for address, expected in ((0x233594, 0x31e), (0x278488, 0xffff), (0x27848c, 0x10ba78),
                              (0x2782f4, 0x10b9f4), (0x28dabc, 0x10b9f4),
                              (0x25c99c, 0x10c5cc), (0x25c800, 0x10ffac),
                              (0x25caf0, 0x10c5c9), (0x264908, 5500), (0x264774, 1500),
                              (0x2975f0, 0x2b3850), (0x22d6e4, 0x10bb0c),
                              (0x264764, 0x10a558), (0x22db34, 0x10a562)):
        if struct.unpack_from('>I', mcu, address - BASE)[0] != expected:
            raise ValueError('NSE-2 NV/loader contract literal differs')
    if mcu[0x2b3850 - BASE:0x2b385a - BASE] != bytes.fromhex('00040506070301020201'):
        raise ValueError('NSE-2 ADC source channel map differs')
    if mcu[0x29907c - BASE:0x299088 - BASE] != bytes.fromhex('002b3848 0010c6b6 002b382c'):
        raise ValueError('NSE-2 keypad table/selector literals differ')
    if (mcu[0x2b382c - BASE:0x2b382c - BASE + 25] != NORMAL_KEYS
            or mcu[0x2b3848 - BASE:0x2b3848 - BASE + 5] != bytes.fromhex('3e3e3e3e0d')):
        raise ValueError('NSE-2 own key tables differ')
    return dict(instruction_sites={name: hex(address) for name, (address, _) in PERIPHERAL_FRAGMENTS.items()},
                eeprom=dict(signal='0x20020', direction='0x20024', sda_bit=0, scl_bit=2,
                            start='0x295814', stop='0x295886', block_reader='0x293d92',
                            block_arguments=['address', 'destination', 'length'],
                            byte_reader='0x295746', intermediate_ack_argument=1,
                            final_nack_argument=0),
                gensio=dict(select='0x28', ccont_select='0x22', ccont_tx='0x2a',
                            ccont_rx='0x2d', status='0x29', ccont_ready_bit=2,
                            lcd_command='0x2c', lcd_data='0x2b', lcd_ready_bit=0),
                keypad=dict(rows=5, row_signal='0x31', column_input='0x30',
                            column_irq_mask='0x33', row_direction='0x2f',
                            normal_table='0x2b382c', special_table='0x2b3848',
                            selector='0x10c6b6', raw_formula='row * 5 + column',
                            normal_keys=list(NORMAL_KEYS), power_column=4),
                simi=dict(iir='0x38', initialization_contract_only=True),
                analog_readiness=dict(source=7, ccont_channel=2, scale_numerator=1500,
                                      scale_denominator=232, accepted_sample_min=1800,
                                      accepted_sample_max=5500, remaining='0x10a562',
                                      report14_engine='0x10bb0c', physical_calibration_proved=False),
                electrical_timing_or_runtime_delivery_proved=False)


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def unpack_package(source):
    digest = sha256(source)
    if digest not in ARCHIVES:
        raise ValueError('requires the acquired hash-qualified NSE-2 ZIP or SFX')
    # ZipFile handles the SFX prefix; no installer or extracted program runs.
    with zipfile.ZipFile(io.BytesIO(source)) as archive:
        if sorted(archive.namelist()) != sorted(MEMBERS):
            raise ValueError('NSE-2 archive members differ or are duplicated')
        contents = {}
        for name, (size, expected) in MEMBERS.items():
            info = archive.getinfo(name)
            if info.file_size != size:
                raise ValueError('NSE-2 member extent differs: ' + name)
            data = archive.read(info)
            if len(data) != size or sha256(data) != expected:
                raise ValueError('NSE-2 member hash differs: ' + name)
            contents[name] = data
    return contents, {'archive_sha256': digest, 'container': ARCHIVES[digest]}


def loader_contract(mcu):
    offset = TABLE - BASE
    if len(mcu) < offset + 112:
        raise ValueError('NSE-2 loader table truncated')
    if struct.unpack_from('>II', mcu, offset - 8) != (112, 0x10ba14):
        raise ValueError('NSE-2 loader initializer differs')
    pointers = struct.unpack_from('>28I', mcu, offset)
    if [i for i, value in enumerate(pointers) if not value] != [27]:
        raise ValueError('NSE-2 loader null slots differ')
    entries = []
    for slot, pointer in enumerate(pointers):
        if not pointer:
            continue
        start = pointer - BASE
        if pointer & 1 or start < 0 or start + 12 > len(mcu):
            raise ValueError('NSE-2 loader descriptor outside MCU')
        header = struct.unpack_from('>6H', mcu, start)
        end = start + 12 + 2 * header[2]
        if end > len(mcu) or header[0] + header[2] > 0x10000:
            raise ValueError('NSE-2 loader payload extent differs')
        entries.append(dict(slot=slot, descriptor=hex(pointer), header=list(header),
                            payload_sha256=sha256(mcu[start + 12:end])))
    verifier = entries[25]
    if verifier['descriptor'] != '0x2a0f6c' or verifier['header'] != [0xf00, 0, 186, 0x700, 180, 0]:
        raise ValueError('NSE-2 verifier descriptor differs')
    payload = int(verifier['descriptor'], 16) - BASE + 12
    if mcu[payload + 24:payload + 30] != bytes.fromhex('76f8 0803 0004'):
        raise ValueError('NSE-2 verifier identity store differs')
    calls = []
    for relative in (0x44, 0x7c, 0xf4):
        if mcu[payload + relative:payload + relative + 2] != bytes.fromhex('f074'):
            raise ValueError('NSE-2 verifier resident CALL opcode differs')
        calls.append(int.from_bytes(mcu[payload + relative + 2:payload + relative + 4], 'big'))
    if calls != [0x8023, 0x8023, 0x802b]:
        raise ValueError('NSE-2 verifier resident CALL targets differ')
    return dict(table=hex(TABLE), initializer_destination='0x10ba14', slots=28,
                null_slots=[27], entries=entries, verifier_identity_store=4,
                resident_call_targets=[hex(value) for value in calls],
                fitted_mask_or_execution_proved=False)


def inspect(source):
    contents, report = unpack_package(source)
    mcu, ppm = (contents[name] for name in MEMBERS)
    if (mcu.count(VERSION) != 1 or mcu[0x9b5ec:0x9b5ec + len(VERSION)] != VERSION
            or ppm[:4] != b'PPM\0' or ppm[4:4 + len(VERSION)] != VERSION
            or ppm[0x28:0x2a] != b'A\0'):
        raise ValueError('NSE-2 MCU/PPM identity differs')
    if (mcu[0x40:0x44] != bytes.fromhex('e3a01602')
            or mcu[0x68:0x6c] != bytes.fromhex('e59f10fc')
            or mcu[0xe4:0xec] != bytes.fromhex('e28f0001e12fff10')
            or mcu[0x16c:0x170] != bytes.fromhex('0010fb98')):
        raise ValueError('NSE-2 reset/interworking contract differs')
    flash = mcu + ppm
    if sha256(flash) != FLASH_SHA256:
        raise ValueError('NSE-2 combined flash differs')
    report.update(product='NSE-2', version='05.53', ppm='A',
                  members={name: dict(bytes=len(data), sha256=sha256(data))
                           for name, data in contents.items()},
                  flash_bytes=len(flash), flash_sha256=FLASH_SHA256,
                  mcu_base='0x200000', ppm_base='0x390000',
                  reset_entry='0x200040', thumb_entry='0x2000ec',
                  stack_literal='0x10fb98', eeprom=eeprom_geometry(mcu[5]),
                  loader=loader_contract(mcu), peripherals=peripheral_contract(mcu), eeprom_supplied=False,
                  boot_or_peripheral_wiring_proved=False)
    return flash, report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('package', type=Path)
    parser.add_argument('--compare-package', type=Path, help='independently qualify and compare another wrapper')
    parser.add_argument('--output', type=Path, help='write qualified raw flash and manifest')
    args = parser.parse_args()
    try:
        flash, report = inspect(args.package.read_bytes())
        if args.compare_package:
            other, other_report = inspect(args.compare_package.read_bytes())
            if flash != other:
                raise ValueError('qualified NSE-2 wrapper payloads differ')
            report['comparison'] = dict(archive_sha256=other_report['archive_sha256'],
                                        container=other_report['container'], byte_identical=True)
        if args.output:
            args.output.mkdir(parents=True, exist_ok=True)
            (args.output / '5110i_5.53a.fls').write_bytes(flash)
            (args.output / 'manifest.json').write_text(json.dumps(report, indent=2) + '\n')
        print(json.dumps(report, indent=2))
    except (OSError, ValueError, zipfile.BadZipFile) as error:
        parser.exit(1, f'NSE-2 package FAIL: {error}\n')


if __name__ == '__main__':
    main()

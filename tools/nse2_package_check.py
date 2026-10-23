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
                  loader=loader_contract(mcu), eeprom_supplied=False,
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
            (args.output / '5110i_5.53A.fls').write_bytes(flash)
            (args.output / 'manifest.json').write_text(json.dumps(report, indent=2) + '\n')
        print(json.dumps(report, indent=2))
    except (OSError, ValueError, zipfile.BadZipFile) as error:
        parser.exit(1, f'NSE-2 package FAIL: {error}\n')


if __name__ == '__main__':
    main()

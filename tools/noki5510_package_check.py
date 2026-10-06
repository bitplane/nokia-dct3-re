"""Normalize the pinned NPM-5 package without executing its installer."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
import zipfile

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.extract_dct3_wintesla import decode_records
from tools.dct3_startup_records import decode_startup_records, initialized_bytes

PACKAGE_SHA256 = '4f13d4aab02e970a86e83c15b903aed18590584dc5c383ed126cf632763d8d60'
FLASH_SHA256 = 'a320d808229adf1e471446547d3ec4643d597a1f67f4b0975cc9beada7c3d15a'
UPLOADS = (
    (0x1e9240, (0xff80, 0xff80, 104, 0x200, 0x8c, 0), '440bf49f1eba4cadb12f7f7581c992b0025807d6'),
    (0x1e936c, (0xa00, 0x1000, 629, 0x200, 0x3e8, 0), 'ab334f30a6c564ba0d60a1adb64e95f22620111a'),
    (0x1ea428, (0xf00, 0, 223, 0xf00, 0xdc, 0), '6646da3c5be9c70deda7e0b5b9f257d5d2ace815'),
)


def assess_flash(flash):
    if len(flash) != 0x350000 or hashlib.sha256(flash).hexdigest() != FLASH_SHA256:
        raise ValueError('not the pinned NPM-5 v3.53 PPM C image')
    if not flash[0x290000:].startswith(b'PPM\x00V 03.53\n15-04-02\nNPM-5\n'):
        raise ValueError('product/version identifier mismatch')
    uploads = []
    for offset, fields, digest in UPLOADS:
        if struct.unpack_from('>6H', flash, offset) != fields:
            raise ValueError(f'descriptor mismatch at {offset + 0x200000:06x}')
        payload = flash[offset + 12:offset + 12 + fields[2] * 2]
        if hashlib.sha1(payload).hexdigest() != digest:
            raise ValueError('upload extent/hash mismatch')
        uploads.append({'descriptor': f'{offset + 0x200000:06x}',
                        'words': fields[2], 'sha1': digest,
                        'scope': 'static descriptor candidate, not executed ownership proof'})
    records, end = decode_startup_records(flash, 0x200000, 0x3b0430)
    if (len(records), end, sum(len(payload) for _, _, payload in records)) != (2090, 0x3bb8d8, 26487):
        raise ValueError('startup initializer coverage mismatch')
    for address, descriptor in ((0x1237c0, 0x3e9240), (0x1237d4, 0x3e936c),
                                (0x1237f0, 0x3ea428)):
        if initialized_bytes(records, address, 4) != struct.pack('>I', descriptor):
            raise ValueError('initialized upload pointer mismatch')
    return {'product': 'NPM-5', 'version': '3.53', 'ppm': 'C',
            'startup_copy': {'records': len(records), 'payload_bytes': 26487,
                             'terminator_end': f'{end:06x}',
                             'verifier_pointer': '1237f0', 'consumer': '319032'},
            'flash_size': len(flash), 'flash_sha256': FLASH_SHA256,
            'uploads': uploads, 'pmm_present': False,
            'graphical_boot_validated': False, 'native_dsp_complete': False}


def normalize(package):
    if hashlib.sha256(package.read_bytes()).hexdigest() != PACKAGE_SHA256:
        raise ValueError('service-package SHA-256 mismatch')
    with zipfile.ZipFile(package) as archive:
        members = {}
        for name, size in (('npm5nx03.530', 2417759), ('npm5nx03.53c', 787296),
                           ('npm-5.ini', 8273)):
            if archive.getinfo(name).file_size != size:
                raise ValueError('unexpected member extent: ' + name)
            members[name] = archive.read(name)  # ZIP CRC is checked here.
    mcu_base, mcu = decode_records(members['npm5nx03.530'], 'NPM-5 MCU')
    ppm_base, ppm = decode_records(members['npm5nx03.53c'], 'NPM-5 PPM C')
    if (mcu_base, len(mcu), ppm_base, len(ppm)) != (0x200000, 0x24da00, 0x490000, 0xc0000):
        raise ValueError('unexpected MCU/PPM record coverage')
    flash = mcu + b'\xff' * (ppm_base - mcu_base - len(mcu)) + ppm
    report = assess_flash(flash)
    report['package_sha256'] = PACKAGE_SHA256
    report['unprogrammed_gap'] = {'start': '44da00', 'end': '490000', 'fill': 'ff'}
    return members, flash, report


def write_derived(path, data):
    if path.exists() and path.read_bytes() != data:
        raise ValueError('refusing to replace different existing artifact: ' + str(path))
    path.write_bytes(data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('package', type=Path)
    parser.add_argument('--output-dir', type=Path)
    args = parser.parse_args()
    try:
        members, flash, report = normalize(args.package)
        if args.output_dir:
            args.output_dir.mkdir(parents=True, exist_ok=True)
            for name, data in members.items():
                write_derived(args.output_dir / name, data)
            write_derived(args.output_dir / '5510f353c.fls', flash)
            # Keep the immutable normalization manifest independent of
            # expanding static-analysis results reported on stdout.
            manifest = {key: value for key, value in report.items() if key != 'startup_copy'}
            write_derived(args.output_dir / 'normalization.json',
                          (json.dumps(manifest, indent=2) + '\n').encode())
    except (OSError, ValueError, KeyError, zipfile.BadZipFile) as error:
        parser.exit(1, f'NPM-5 package validation FAIL: {error}\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()

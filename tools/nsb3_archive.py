"""Qualify acquired NSB-3 v6.13 service regions without executing the installer."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import sys
import zipfile
import configparser

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.extract_dct3_wintesla import decode_records

INSTALLER_SHA256 = 'd2e684dbef4dc74fd0233ed96c3a3c1f1bdbd9b0615f338aff28e18a20c63d85'
REGIONS = {
    'nsb3___6.130': (880844, 0x200000, 0xd6d00, '4ae57f0eed01125efbe0f4e63abf5412b44539069b424f5d6dcf030659a8ebda'),
    'nsb3___6.13a': (131216, 0x2e0000, 0x20000, 'b4c76f9e02d7c74d4a3920badaae0dc58bb40dc978ad660097d33f38341786ae'),
    'nsb3___6.13b': (131216, 0x2e0000, 0x20000, '8df785599c6139afe37379e9d151132981d2a31aff7b3c426fb950beff629dac'),
    'nsb3___6.13c': (131216, 0x2e0000, 0x20000, '337ffe264342ef0e4b4674e071d4e3f94a79b0453f3330740c3bd9479c98d0bd'),
}
OTHER_MEMBERS = {'nsb3.pp', 'NSB3.CLI', 'NSB3.WUG', 'NSB3FIDO.PP',
                 'NSB3FIDO.WUG', 'NSB3POWE.WUG', 'NSB3VSTR.WUG', 'nsb-3.ini'}


def inspect(source):
    if hashlib.sha256(source).hexdigest() != INSTALLER_SHA256:
        raise ValueError('requires acquired NSB-3 v6.13 installer')
    regions, rows = {}, []
    with zipfile.ZipFile(io.BytesIO(source)) as archive:
        entries = archive.infolist()
        expected = set(REGIONS) | OTHER_MEMBERS
        if len(entries) != len(expected) or {e.filename for e in entries} != expected:
            raise ValueError('unexpected or duplicate NSB-3 members')
        settings = configparser.ConfigParser()
        settings.read_string(archive.read('nsb-3.ini').decode('ascii'))
        restore = settings['NSB-3_RESTOREFILES']
        if (restore.get('EraseDefinedArea'), restore.get('LowEraseLimit'),
                restore.get('HighEraseLimit')) != ('Yes', '200000', '2FFFFF'):
            raise ValueError('NSB-3 declared flash erase range differs')
        for entry in entries:
            if entry.filename not in REGIONS:
                continue
            raw_size, start, length, digest = REGIONS[entry.filename]
            if entry.file_size != raw_size:
                raise ValueError('NSB-3 uncompressed member size differs')
            raw = archive.read(entry)
            address, payload = decode_records(raw, entry.filename)
            if (address, len(payload)) != (start, length) or hashlib.sha256(payload).hexdigest() != digest:
                raise ValueError('NSB-3 normalized region identity differs')
            regions[entry.filename] = payload
            rows.append({'member': entry.filename, 'start': hex(start),
                         'end_exclusive': hex(start + length), 'bytes': length,
                         'sha256': digest, 'source_sha256': hashlib.sha256(raw).hexdigest()})
    return {'installer_sha256': INSTALLER_SHA256, 'regions': rows,
            'installer_erase_range': ['0x200000', '0x300000'],
            'unprovided_mcu_ppm_gap': ['0x2d6d00', '0x2e0000'],
            'scope': 'Service regions only; no EEPROM, resident DSP mask or handset support established'}, regions


def image_from_regions(regions, pack):
    if pack not in ('a', 'b', 'c'):
        raise ValueError('requires PPM a, b or c')
    mcu, ppm = regions['nsb3___6.130'], regions['nsb3___6.13' + pack]
    if len(mcu) != 0xd6d00 or len(ppm) != 0x20000:
        raise ValueError('NSB-3 image region sizes differ')
    # The own installer explicitly erases the whole 200000..2fffff range.
    return mcu + b'\xff' * 0x9300 + ppm


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('installer', type=Path)
    parser.add_argument('--output', type=Path, help='fresh directory; no gap or tail synthesized')
    parser.add_argument('--image', type=Path, help='fresh 1 MiB image with installer-declared erased gap')
    parser.add_argument('--ppm', choices=list('abc'), default='a')
    args = parser.parse_args()
    try:
        report, regions = inspect(args.installer.read_bytes())
        if args.output:
            args.output.mkdir(parents=True, exist_ok=False)
            for name, payload in regions.items():
                (args.output / (name + '.bin')).write_bytes(payload)
            (args.output / 'manifest.json').write_text(json.dumps(report, indent=2) + '\n')
        if args.image:
            image = image_from_regions(regions, args.ppm)
            args.image.parent.mkdir(parents=True, exist_ok=True)
            with args.image.open('xb') as stream:
                stream.write(image)
            report['image_sha256'] = hashlib.sha256(image).hexdigest()
    except (OSError, ValueError, zipfile.BadZipFile) as exc:
        parser.exit(1, f'NSB-3 archive: {exc}\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()

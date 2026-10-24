"""Qualify the acquired NSM-4 v5.18 service archive, not handset support."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import zipfile

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.extract_dct3_wintesla import decode_records

ARCHIVE_SHA256 = '8ef5e4bf68b0b190ca31a51790a80fe13e3fc2aee0dd4fed45975149d9c5807d'
MCU = 'nsm4_5.180'
MEMBERS = {MCU: (2689928, 0x200000, 0x290000)}
MEMBERS.update({f'nsm4_5.18{pack}': (984120, 0x490000, 0xf0000)
                for pack in 'pqrstu'})


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def decode_archive(source):
    if sha256(source) != ARCHIVE_SHA256:
        raise ValueError('requires the acquired NSM-4 v5.18 archive')
    import io
    decoded = {}
    rows = []
    with zipfile.ZipFile(io.BytesIO(source)) as archive:
        entries = archive.infolist()
        if len(entries) != len(MEMBERS) or {e.filename for e in entries} != set(MEMBERS):
            raise ValueError('unexpected or duplicate NSM-4 archive members')
        for entry in entries:
            size, start, length = MEMBERS[entry.filename]
            if entry.file_size != size:
                raise ValueError(f'{entry.filename}: unexpected uncompressed size')
            raw = archive.read(entry)
            address, payload = decode_records(raw, entry.filename)
            if address != start or len(payload) != length:
                raise ValueError(f'{entry.filename}: unexpected decoded extent')
            decoded[entry.filename] = payload
            rows.append({'member': entry.filename, 'source_sha256': sha256(raw),
                         'start': hex(address), 'end_exclusive': hex(address + length),
                         'bytes': len(payload), 'decoded_sha256': sha256(payload)})
    report = {'archive_sha256': ARCHIVE_SHA256, 'members': sorted(rows, key=lambda r: r['member']),
              'boundary': 'MCU/PPM only; no PMM, resident DSP mask or hardware profile established'}
    return report, decoded


def extract(source, output):
    report, decoded = decode_archive(source)
    # Refuse reuse: a failed or unrelated earlier extraction must not become evidence.
    output.mkdir(parents=True, exist_ok=False)
    for name, payload in decoded.items():
        (output / (name + '.bin')).write_bytes(payload)
    (output / 'manifest.json').write_text(json.dumps(report, indent=2) + '\n')
    return report


def image_prefix(source, pack):
    if pack not in 'pqrstu' or len(pack) != 1:
        raise ValueError('requires one acquired PPM pack p through u')
    _, decoded = decode_archive(source)
    return decoded[MCU] + decoded[f'nsm4_5.18{pack}']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=Path)
    parser.add_argument('--output', type=Path, help='fresh directory for normalized regions')
    parser.add_argument('--image', type=Path, help='fresh MCU+PPM prefix file, no tail padding')
    parser.add_argument('--ppm', choices=list('pqrstu'), default='p')
    args = parser.parse_args()
    try:
        source = args.archive.read_bytes()
        report = extract(source, args.output) if args.output else decode_archive(source)[0]
        if args.image:
            payload = image_prefix(source, args.ppm)
            args.image.parent.mkdir(parents=True, exist_ok=True)
            with args.image.open('xb') as stream:
                stream.write(payload)
            report['image_prefix'] = {'ppm': args.ppm, 'bytes': len(payload),
                                      'sha256': sha256(payload)}
    except (OSError, ValueError, zipfile.BadZipFile) as exc:
        parser.exit(1, f'NSM-4 archive: {exc}\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()

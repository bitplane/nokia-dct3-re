"""Author external test media in the original MU4 bench layout, never firmware state."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import struct
import subprocess
import sys

if __package__:
    from .mu4_recorder_media_inventory import adts_inventory, fixture_volume
else:
    from mu4_recorder_media_inventory import adts_inventory, fixture_volume


def titled_track(payload):
    frames = b''
    for tag, value in ((b'TIT2', b'MAME'), (b'TPE1', b'TEST')):
        text = b'\0' + value
        frames += tag + struct.pack('>I', len(text)) + b'\0\0' + text
    body = frames.ljust(100, b'\0')
    return b'ID3\x03\0\0\0\0\0\x64' + body + payload


def embed_fixture_volume(image, volume):
    metadata, original = fixture_volume(image)
    if len(volume) != len(original) or volume[:512] != original[:512]:
        raise ValueError('authored volume changed the fixture extent or boot sector')
    result = bytearray(image)
    changed = 0
    for sector in range(metadata['volume_sectors']):
        start = sector * 512
        block = volume[start:start + 512]
        if block != original[start:start + 512]:
            changed += 1
        page = metadata['first_physical_page'] + sector
        result[page * 528:page * 528 + 512] = block
    output = bytes(result)
    if fixture_volume(output)[1] != volume:
        raise ValueError('authored NAND did not preserve the checked volume mapping')
    return output, {'changed_data_pages': changed, 'spare_bytes_preserved': True,
                    'outside_partition_preserved': True, 'mapping': metadata}


def pcm_tone_report(pcm):
    if len(pcm) % 4 or len(pcm) < 44100 * 3 * 4:
        raise ValueError('host reference is not at least three seconds of stereo s16 PCM')
    frames = list(struct.iter_unpack('<hh', pcm))
    window = frames[44100:88200]
    amplitudes = []
    for channel, frequency in enumerate((440, 660)):
        def amplitude(hz):
            real = sum(frame[channel] * math.cos(2 * math.pi * hz * i / 44100) for i, frame in enumerate(window))
            imaginary = sum(frame[channel] * math.sin(2 * math.pi * hz * i / 44100) for i, frame in enumerate(window))
            return 2 * math.hypot(real, imaginary) / len(window)
        wanted, other = amplitude(frequency), amplitude(660 if channel == 0 else 440)
        if wanted < 500 or wanted < 10 * other:
            raise ValueError('host reference does not retain the independently requested stereo tones')
        amplitudes.append({'channel': channel, 'frequency_hz': frequency,
                           'amplitude_s16': wanted, 'other_tone_amplitude_s16': other})
    return {'stereo_frames': len(frames), 'sample_rate': 44100,
            'pcm_sha256': hashlib.sha256(pcm).hexdigest(), 'tones': amplitudes,
            'scope': 'host-decoded generated media only; not native DSP output or speech'}


def run(command, environment):
    completed = subprocess.run([str(value) for value in command], check=False,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               env=environment, timeout=120)
    if completed.returncode:
        raise ValueError(f'{command[0]} failed ({completed.returncode}): {completed.stderr.decode(errors="replace")}')
    return completed


def validate_generated_headers(headers):
    chain = headers['first_candidate_chain']
    if not chain or chain[0] != 0 or headers['chain_stop_reason'] != 'exact_eof':
        raise ValueError('generated AAC does not have a complete declared-length header chain')
    candidates = {row['offset']: row for row in headers['candidates']}
    for offset in chain:
        row = candidates[offset]
        if (row['mpeg_version'], row['profile_bits'], row['sampling_index'], row['channel_configuration'], row['raw_data_blocks_minus_one']) != (2, 1, 4, 2, 0):
            raise ValueError('generated AAC chain differs from the requested control configuration')


def fixture_authoring_policy(volume):
    if len(volume) < 512:
        raise ValueError('incomplete fixture boot sector')
    sector_bytes, reserved = struct.unpack_from('<H', volume, 11)[0], struct.unpack_from('<H', volume, 14)[0]
    cluster, fats = volume[13], volume[16]
    roots, fat_sectors = struct.unpack_from('<H', volume, 17)[0], struct.unpack_from('<H', volume, 22)[0]
    sectors = struct.unpack_from('<H', volume, 19)[0] or struct.unpack_from('<I', volume, 32)[0]
    first_fat = reserved * sector_bytes
    if sector_bytes != 512 or not cluster or first_fat + 3 > len(volume):
        raise ValueError('unsupported fixture FAT geometry')
    if volume[first_fat] == volume[21]:
        return {'skip_check': False, 'reason': 'matching BPB/FAT media bytes'}
    clusters = (sectors - reserved - fats * fat_sectors - (roots * 32 + 511) // 512) // cluster
    if not 0 < clusters < 4085 or volume[21] != 0xf8 or volume[first_fat:first_fat + 3] != b'\xff\xff\xff':
        raise ValueError('unrecognized fixture media-byte mismatch; refusing broad compatibility override')
    return {'skip_check': True, 'reason': 'checked legacy FAT12 fixture: reserved media ff, BPB media f8',
            'data_clusters': clusters}


def generate(source, output, *, mcopy='mcopy', ffmpeg='ffmpeg', ffprobe='ffprobe', sevenzip='7z'):
    image = source.read_bytes()
    source_hash = hashlib.sha256(image).hexdigest()
    _, volume = fixture_volume(image)
    output.mkdir(parents=True, exist_ok=False)
    environment = dict(os.environ, TZ='UTC', LC_ALL='C', SOURCE_DATE_EPOCH='946684800', MTOOLSRC=os.devnull)
    policy = fixture_authoring_policy(volume)
    environment.pop('MTOOLS_SKIP_CHECK', None)
    if policy['skip_check']:
        environment['MTOOLS_SKIP_CHECK'] = '1'
    encoded, decoded = output / 'control.aac', output / 'host-reference.s16'
    command = [ffmpeg, '-hide_banner', '-loglevel', 'error', '-f', 'lavfi', '-i',
               'aevalsrc=0.125*sin(2*PI*440*t)|0.0625*sin(2*PI*660*t):s=44100:d=4',
               '-threads', '1', '-c:a', 'aac', '-profile:a', 'mpeg2_aac_low', '-ar', '44100',
               '-ac', '2', '-b:a', '128k', '-write_mpeg2', '1', '-f', 'adts', str(encoded)]
    run(command, environment)
    headers = adts_inventory(encoded.read_bytes())
    validate_generated_headers(headers)
    streams = json.loads(run([ffprobe, '-v', 'error', '-show_streams', '-of', 'json', encoded], environment).stdout)['streams']
    if len(streams) != 1 or streams[0].get('codec_name') != 'aac' or streams[0].get('profile') != 'LC' or streams[0].get('sample_rate') != '44100' or streams[0].get('channels') != 2:
        raise ValueError('host probe did not identify the requested AAC-LC stereo control')
    decoded_result = run([ffmpeg, '-hide_banner', '-loglevel', 'error', '-i', encoded, '-f', 's16le', '-acodec', 'pcm_s16le', decoded], environment)
    if decoded_result.stderr.strip():
        raise ValueError('host reference decoder reported errors despite returning success')
    reference = pcm_tone_report(decoded.read_bytes())
    track = output / 'TEST'
    track.write_bytes(titled_track(encoded.read_bytes()))
    os.utime(track, (946684800, 946684800))
    volume_path = output / 'volume.img'
    volume_path.write_bytes(volume)
    # The fixture's long name is TEST, but its existing short name is TEST.REL.
    run([mcopy, '-o', '-m', '-i', volume_path, track, '::TEST.REL'], environment)
    readback = run([sevenzip, 'e', '-so', volume_path, 'TEST*'], environment).stdout
    if readback != track.read_bytes():
        raise ValueError('independent FAT readback differs from generated track')
    (output / 'readback_TEST').write_bytes(readback)
    nand, changes = embed_fixture_volume(image, volume_path.read_bytes())
    target = output / 'control.nand'
    target.write_bytes(nand)
    if hashlib.sha256(source.read_bytes()).hexdigest() != source_hash:
        raise ValueError('source NAND changed during fixture authoring')
    versions = {}
    for name, version_command in (('ffmpeg', [ffmpeg, '-version']), ('ffprobe', [ffprobe, '-version']),
                                  ('mcopy', [mcopy, '-V']), ('7z', [sevenzip, 'i'])):
        result = run(version_command, environment)
        lines = (result.stdout + result.stderr).decode(errors='replace').splitlines()
        versions[name] = next((line for line in lines if line.strip()), 'unreported')
    report = {'source_nand_sha256': source_hash, 'nand_sha256': hashlib.sha256(nand).hexdigest(),
              'track_sha256': hashlib.sha256(readback).hexdigest(), 'track_bytes': len(readback),
              'host_reference': reference, 'encoded_headers': headers, 'media_changes': changes,
              'authoring_compatibility': policy,
              'short_destination': 'TEST.REL', 'independent_readback_selector': 'TEST*',
              'generation_command': command, 'source_date_epoch': 946684800,
              'aac_profile': 'mpeg2_aac_low',
              'tool_versions': versions,
              'scope': 'externally authored test media in own fixture; not native recording, native PCM or phone boot acceptance'}
    (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    volume_path.unlink()
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source_nand', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--mcopy', default='mcopy')
    args = parser.parse_args()
    try:
        report = generate(args.source_nand, args.output, mcopy=args.mcopy)
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(f'MU4 generated media: {error}', file=sys.stderr)
        return 1
    print(json.dumps({key: report[key] for key in ('nand_sha256', 'track_sha256', 'track_bytes', 'host_reference')}, indent=2))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

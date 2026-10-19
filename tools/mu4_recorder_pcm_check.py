"""Validate native ingested PCM; optional projected-AAC correlation is diagnostic."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

if __package__:
    from .mu4_recorder_read_census import check, decode_ingress_loop
    from .mu4_playback_frame_check import rows
    from .mu4_transform_check import fnv
    from .mu4_clock_contract import SOURCE_SHA256
    from .noki5510_a00_inventory import uploaded_word_image
    from .mu4_recorder_media_inventory import adts_header
    from .mu4_recorder_media_inventory import fixture_volume
    from .mu4_recorder_transform_check import project_complete_windows
    from .mu4_context_contract import contract as recording_contract
else:
    from mu4_recorder_read_census import check, decode_ingress_loop
    from mu4_playback_frame_check import rows
    from mu4_transform_check import fnv
    from mu4_clock_contract import SOURCE_SHA256
    from noki5510_a00_inventory import uploaded_word_image
    from mu4_recorder_media_inventory import adts_header
    from mu4_recorder_media_inventory import fixture_volume
    from mu4_recorder_transform_check import project_complete_windows
    from mu4_context_contract import contract as recording_contract


def validate_capture(text, raw, consumer):
    ends = rows(text, 'mu4_recorder_input_pcm')
    if len(ends) != 1 or consumer is None:
        raise ValueError('PCM capture needs one terminal and validated native consumer')
    end = ends[0]
    fixed = dict(file='mu4_recorder_input.pcm', bad_order='0', dropped='0', cap='1048576',
                 channels='2', format='s16le_interleaved', source='staged_operand_read',
                 save_restore='unsupported', observer_writes='0', audio_acceptance='0')
    if any(end[key] != value for key, value in fixed.items()):
        raise ValueError('PCM capture ordering, cap or scope failed')
    words = int(end['words'])
    if (not 0 < words <= 1048576 or len(raw) != 2*words or words != consumer['frames']*2048
            or any(consumer[key] != words for key in ('origins', 'known_reads', 'read_epochs'))
            or fnv(raw) != int(end['fnv1a'], 16)):
        raise ValueError('PCM file does not match the complete native consumed-word capture')
    return dict(words=words, stereo_frames=words//2, bytes=len(raw),
                sha256=hashlib.sha256(raw).hexdigest(), channels=2, format='s16le_interleaved',
                scope='Actual ordered native operand reads; not measured ADC rate or encoder output')


def correlation_matrix(reference_raw, decoded_raw, *, margin=4096, max_lag=4096):
    import numpy as np
    if len(reference_raw) % 4 or len(decoded_raw) % 4 or not 0 <= max_lag <= margin:
        raise ValueError('correlation needs complete stereo s16 frames and a bounded lag')
    reference = np.frombuffer(reference_raw, dtype='<i2').reshape(-1, 2).astype(np.float64)
    decoded = np.frombuffer(decoded_raw, dtype='<i2').reshape(-1, 2).astype(np.float64)
    n = min(len(reference), len(decoded))
    length = n-2*margin
    if length <= 0:
        raise ValueError('insufficient overlap for the fixed interior comparison')
    size = 1 << (n+length-2).bit_length()
    starts = np.arange(margin-max_lag, margin+max_lag+1)
    matrix = []
    for source in range(2):
        x = reference[:n, source]
        prefix = np.concatenate(([0.0], np.cumsum(x)))
        square = np.concatenate(([0.0], np.cumsum(x*x)))
        sums = prefix[starts+length]-prefix[starts]
        variances = np.maximum(0.0, square[starts+length]-square[starts]-sums*sums/length)
        x_fft = np.fft.rfft(x, size)
        for output in range(2):
            y = decoded[margin:n-margin, output]
            mean = float(y.mean())
            y_var = float(np.dot(y-mean, y-mean))
            valid = variances > 0
            row = dict(input_channel=source, output_channel=output)
            if not y_var or not valid.any():
                matrix.append(dict(row, assessable=False, reason='zero_signal_variance'))
                continue
            convolution = np.fft.irfft(x_fft*np.fft.rfft(y[::-1], size), size)
            covariance = convolution[length-1+starts]-sums*mean
            scores = np.zeros(len(starts))
            scores[valid] = covariance[valid]/np.sqrt(variances[valid]*y_var)
            candidates = np.flatnonzero(valid)
            best = int(candidates[np.argmax(np.abs(scores[candidates]))])
            start = int(starts[best])
            score = float(np.clip(scores[best], -1.0, 1.0))
            zero = int(np.flatnonzero(starts == margin)[0])
            gain = float(covariance[best]/variances[best])
            offset = mean-gain*float(sums[best])/length
            residual = y-(gain*x[start:start+length]+offset)
            error = float(np.dot(residual, residual))
            matrix.append(dict(row, assessable=True, correlation=score,
                               fitted_delay_samples=margin-start, affine_gain=gain,
                               zero_lag_correlation=float(np.clip(scores[zero], -1.0, 1.0)) if valid[zero] else None,
                               fitted_at_search_boundary=abs(margin-start) == max_lag,
                               affine_offset=offset,
                               affine_fit_snr_db=float(10*np.log10(y_var/error)) if error else None,
                               perfect_affine_fit=not bool(error),
                               near_peak_offsets=int(np.count_nonzero(valid & (np.abs(scores) >= abs(score)*0.999)))))
    return dict(input_stereo_frames=len(reference), decoded_stereo_frames=len(decoded),
                input_rms=[float(value) for value in np.sqrt(np.mean(reference*reference, axis=0))],
                decoded_rms=[float(value) for value in np.sqrt(np.mean(decoded*decoded, axis=0))],
                compared_frames_per_pair=length, fixed_margin=margin, maximum_lag=max_lag,
                near_peak_ratio=.999, numpy_version=np.__version__,
                matrix=matrix, alignment_fitted=True, alignment_validated=False,
                codec_conformance_validated=False, tail_completion_validated=False,
                scope='Fixed interior, bounded-lag best-absolute affine correlation; periodic input cannot prove unique latency')


def decode_projection(path, *, ffmpeg='ffmpeg', ffprobe='ffprobe'):
    data = path.read_bytes()
    cursor, configurations, packets = 0, set(), 0
    while cursor < len(data):
        header = adts_header(data, cursor)
        if header is None or not header['frame_within_file']:
            raise ValueError('projected AAC must be an exact complete ADTS chain')
        if header['profile_bits'] != 1 or header['channel_configuration'] != 2 or header['raw_data_blocks_minus_one']:
            raise ValueError('comparison currently qualifies AAC-LC stereo single-block packets only')
        configurations.add((header['mpeg_version'], header['sampling_index']))
        packets += 1
        if packets > 512:
            raise ValueError('projected AAC exceeds the bounded native capture horizon')
        cursor += header['frame_bytes']
    if not packets or len(configurations) != 1:
        raise ValueError('missing or changing projected AAC configuration')
    probe = subprocess.run([ffprobe, '-v', 'error', '-show_entries',
                            'stream=codec_name,profile,sample_rate,channels', '-of', 'json', str(path)],
                           check=True, capture_output=True, timeout=60)
    streams = json.loads(probe.stdout)['streams']
    if (len(streams) != 1 or streams[0]['codec_name'] != 'aac' or streams[0]['profile'] != 'LC'
            or streams[0]['channels'] != 2 or int(streams[0]['sample_rate']) <= 0):
        raise ValueError('external decoder configuration differs from the ADTS comparison scope')
    decoded = subprocess.run([ffmpeg, '-nostdin', '-hide_banner', '-loglevel', 'error', '-i', str(path),
                              '-frames:a', str(packets), '-f', 's16le', '-acodec', 'pcm_s16le', 'pipe:1'],
                             check=True, capture_output=True, timeout=60)
    if decoded.stderr or len(decoded.stdout) != packets*1024*4:
        raise ValueError('external AAC decode has errors or incomplete sample coverage: '+decoded.stderr.decode(errors='replace')[:2048])
    version = subprocess.run([ffmpeg, '-version'], check=True, capture_output=True, timeout=10).stdout.decode().splitlines()[0]
    return decoded.stdout, dict(sha256=hashlib.sha256(data).hexdigest(), packets=packets,
                                declared_sample_rate=int(streams[0]['sample_rate']),
                                decoder=version, host_projection_derivation_validated=False,
                                native_projection_beyond_prefix_validated=False,
                                scope='Externally supplied host projection; extraction and inverse-transform provenance require separate qualification')


def qualify_projection(endpoint, volume, trace, projected, source, *, sevenzip='7z'):
    recording_contract(source)
    metadata, raw_volume = fixture_volume(endpoint.read_bytes())
    del raw_volume
    if hashlib.sha256(volume.read_bytes()).hexdigest() != metadata['volume_sha256']:
        raise ValueError('projection volume does not derive from this recording endpoint')
    extracted = subprocess.run([sevenzip, 'e', '-so', '-bd', str(volume.resolve()), 'TEST'],
                               check=True, capture_output=True, timeout=60).stdout
    proof, payload = project_complete_windows(trace.read_text(), extracted)
    if payload != projected.read_bytes():
        raise ValueError('projected AAC differs from independent endpoint extraction and inverse projection')
    return dict(host_projection_derivation_validated=True,
                native_projection_beyond_prefix_validated=proof['native_beyond_prefix_validated'],
                fixture_volume_sha256=metadata['volume_sha256'],
                stored_file_sha256=hashlib.sha256(extracted).hexdigest(),
                native_prefix_bytes=proof['captured_prefix_bytes'],
                projected_sha256=hashlib.sha256(payload).hexdigest(),
                scope='Same-endpoint fixture export, independent TEST extraction and bounded-prefix-qualified host inverse projection; later native state remains unvalidated')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run', type=Path)
    parser.add_argument('--source-a00', type=Path, required=True)
    parser.add_argument('--projected-aac', type=Path, help='optional diagnostic content comparison, not codec acceptance')
    parser.add_argument('--ffmpeg', default='ffmpeg')
    parser.add_argument('--ffprobe', default='ffprobe')
    parser.add_argument('--fixture-volume', type=Path, help='optionally qualify projection against this run endpoint')
    parser.add_argument('--projection-trace', type=Path, help='bounded native transform trace for projection qualification')
    parser.add_argument('--sevenzip', default='7z')
    args = parser.parse_args()
    if bool(args.fixture_volume) != bool(args.projection_trace) or (args.fixture_volume and not args.projected_aac):
        parser.error('projection qualification needs --projected-aac, --fixture-volume and --projection-trace together')
    try:
        source = args.source_a00.read_bytes()
        if hashlib.sha256(source).hexdigest() != SOURCE_SHA256:
            raise ValueError('expected original R060 InitData source')
        contract = decode_ingress_loop(uploaded_word_image(bytes.fromhex(source.decode('ascii')), 'aa88'))
        text = (args.run/'error.log').read_text()
        proof = check(text, True)
        if len(re.findall(r'mu4_native_recorder_metadata: PASS native_reply=1 success=1 playback=0 firmware_state_forcing=0$', text, re.M)) != 1:
            raise ValueError('capture lacks original native metadata completion')
        raw = (args.run/'mu4_recorder_input.pcm').read_bytes()
        report = dict(capture=validate_capture(text, raw, proof['stack_consumer']), source_contract=contract,
                      endpoint_sha256=hashlib.sha256((args.run/'mu4_recorder_endpoint.nand').read_bytes()).hexdigest())
        if args.projected_aac:
            decoded, projection = decode_projection(args.projected_aac, ffmpeg=args.ffmpeg, ffprobe=args.ffprobe)
            if args.fixture_volume:
                projection.update(qualify_projection(args.run/'mu4_recorder_endpoint.nand', args.fixture_volume,
                                                     args.projection_trace, args.projected_aac, source, sevenzip=args.sevenzip))
            report['projected_aac'] = projection
            report['content_comparison'] = correlation_matrix(raw, decoded)
        print(json.dumps(report, indent=2, allow_nan=False))
    except (ValueError, KeyError, IndexError, TypeError, OSError, ArithmeticError, ImportError, subprocess.SubprocessError) as error:
        parser.exit(1, f'MU4 recorder PCM check FAIL: {error}\n')


if __name__ == '__main__':
    main()

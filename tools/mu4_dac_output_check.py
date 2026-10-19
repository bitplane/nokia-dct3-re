"""Check ideal DAC/headphone contribution from actual DIN and control epochs."""
import argparse
from array import array
import json
import math
from pathlib import Path
import struct
import sys
import wave

if __package__:
    from .mu4_playback_frame_check import rows
    from .mu4_serial_epoch_check import codec_control_history, verify as verify_serial
else:
    from mu4_playback_frame_check import rows
    from mu4_serial_epoch_check import codec_control_history, verify as verify_serial


def active(registers):
    return bool(registers[9]&1 and not registers[6]&0x88 and registers[4]&0x10 and not registers[5]&8)


def signed(word):
    return word-65536 if word & 0x8000 else word


def check_frames(text,din,slots):
    frames,ends = rows(text,'mu4_dac_frame'),rows(text,'mu4_dac_terminal')
    if (len(ends) != 1 or not frames or not len(frames) == int(ends[0]['frames']) <= 4096
            or ends[0]['first'] != '1.060000000' or ends[0]['last'] != '1.100000000'
            or ends[0]['capped'] != '0' or ends[0]['analog_filter'] != '0'
            or ends[0]['audio_acceptance'] != '0' or float(ends[0]['t']) < 1.10):
        raise ValueError('incomplete or changed ideal DAC capture')
    captured = {int(row['ns']):row for row in frames}
    if len(captured) != len(frames) or any(int(row['ordinal']) != i for i,row in enumerate(frames,1)):
        raise ValueError('duplicate/discontinuous ideal DAC frames')
    metadata = list(struct.iter_unpack('<QB',slots))
    if len(metadata) != len(din):
        raise ValueError('DAC input metadata extent differs from DIN')
    input_frames = []
    for index,(time,slot) in enumerate(metadata):
        if slot == 1 and index:
            if metadata[index-1][1] != 0:
                raise ValueError('DAC input lacks its preceding first slot')
            input_frames.append((time,(din[index-1],din[index])))
    expected_times = {time for time,words in input_frames if 1_060_000_000 <= time <= 1_100_000_000}
    if set(captured) != expected_times:
        raise ValueError('DAC frames do not cover every actual stereo DIN pair in the window')
    controls = rows(text,'mu4_codec_control')
    history = codec_control_history(text)
    events = [(time,0,(row,registers)) for row,(time,registers) in zip(controls,history)]
    events += [(time,1,words) for time,words in input_frames if time <= 1_100_000_000]
    events.sort(key=lambda value:(value[0],value[1]))
    registers = (0x97,0x97,0x79,0x79,0x0a,0x08,0x07,0x01,0x20,0)
    effective,pending,previous_input = [0x79,0x79],[False,False],[None,None]
    previous_time,previous_kind,checked,nonzero,max_error = None,None,0,0,0.0
    for time,kind,value in events:
        if time == previous_time and kind != previous_kind:
            raise ValueError('control/frame ordering is ambiguous at nanosecond precision')
        previous_time,previous_kind = time,kind
        if kind == 0:
            control,registers = value
            word = int(control['word'],16)
            address,data = word >> 9,word & 0x1ff
            if address == 15 and not data:
                effective,pending,previous_input = [0x79,0x79],[False,False],[0,0]
            if address in (2,3):
                affected = [address-2]
                if data & 0x100: affected.append((address-2)^1)
                for channel in affected:
                    target = registers[channel+2] & 0x7f
                    if not registers[channel+2]&0x80 or not active(registers) or previous_input[channel] == 0:
                        effective[channel],pending[channel] = target,False
                    else:
                        # Uncaptured early input must not establish a guessed crossing.
                        if previous_input[channel] is None: effective[channel] = None
                        pending[channel] = True
            for channel in range(2):
                if pending[channel] and (not active(registers) or previous_input[channel] == 0):
                    effective[channel],pending[channel] = registers[channel+2]&0x7f,False
            continue
        words = value
        mapped = [signed(words[channel ^ bool(registers[7]&0x20)]) for channel in range(2)]
        for channel in range(2):
            old,new = previous_input[channel],mapped[channel]
            crossing = new == 0 or (old is not None and (old == 0 or ((old < 0) != (new < 0))))
            if pending[channel] and (not active(registers) or crossing):
                effective[channel],pending[channel] = registers[channel+2]&0x7f,False
        previous_input = mapped
        if time not in captured:
            continue
        record = captured[time]
        if record['analog_filter'] != '0' or record['audio_acceptance'] != '0':
            raise ValueError('ideal DAC observation scope changed')
        for channel,name in enumerate(('left','right')):
            if effective[channel] is None:
                raise ValueError('effective headphone gain cannot be established from captured input/control')
            level = (mapped[channel]/32768.0*10**((effective[channel]-121)/20)
                     if active(registers) and not registers[6]&0x10 and effective[channel] >= 0x30 else 0.0)
            observed = float(record[name+'_level'])
            if (int(record[name+'_code'],16) != mapped[channel]&0xffff
                    or int(record[name+'_volume'],16) != effective[channel]
                    or not math.isfinite(observed) or abs(observed-level) > 6e-12):
                raise ValueError('ideal DAC code/gain/level differs from independent DIN/control model')
            max_error = max(max_error,abs(observed-level))
            nonzero += level != 0
        checked += 1
    if checked != len(frames):
        raise ValueError('unconsumed DAC observations')
    return dict(frames=checked,nonzero_channel_samples=nonzero,max_normalized_level_error=max_error,
                ideal_dac_control_path_validated=True,native_audio_validated=False,
                scope='Ideal stereo latch, control gates, nominal gain and digital zero/sign-cross policy; '
                      'not converter filters, measured analog threshold, electrical routing or whole playback')


def check_wav(path):
    with wave.open(str(path),'rb') as source:
        if source.getnchannels() != 2 or source.getsampwidth() != 2 or source.getframerate() != 48000 or source.getcomptype() != 'NONE':
            raise ValueError('rendered WAV must be 48-kHz stereo signed 16-bit PCM')
        frames = source.getnframes()
        words = array('h')
        words.frombytes(source.readframes(frames))
    if sys.byteorder != 'little': words.byteswap()
    if not frames or len(words) != frames*2:
        raise ValueError('empty/truncated rendered WAV')
    nonzero = [sum(value != 0 for value in words[channel::2]) for channel in range(2)]
    if not all(nonzero):
        raise ValueError('rendered headphone audio is silent in a channel')
    return dict(frames=frames,rate=48000,nonzero=nonzero,
                peak=[max(abs(value) for value in words[channel::2]) for channel in range(2)],
                host_render_nonzero=True,analog_waveform_validated=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory',type=Path)
    parser.add_argument('--wav',type=Path)
    args = parser.parse_args()
    try:
        serial = verify_serial(args.directory)
        din = array('H')
        din.frombytes((args.directory/'mu4_playback_din.le16').read_bytes())
        if sys.byteorder != 'little': din.byteswap()
        result = check_frames((args.directory/'error.log').read_text(),din,(args.directory/'mu4_playback_din.slots').read_bytes())
        result['source_words_delivered_to_din'] = serial['final_words_at_din']
        if args.wav: result['rendered_wav'] = check_wav(args.wav)
        print(json.dumps(result,indent=2))
    except (ValueError,KeyError,OSError,ArithmeticError,wave.Error) as error:
        print(f'FAIL: {error}',file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

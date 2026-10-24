#!/usr/bin/env python3
"""Build the external ETSI EFR reference and verify its installation vectors.

No reference implementation or speech vectors are imported into MAME sources.
"""

import argparse
import hashlib
import json
import re
from pathlib import Path
import subprocess
import struct
import sys
import tempfile
import zipfile


ARCHIVE_SHA256 = "54f4bcba0fbcc3fe1e42e0fb8de2c7f8bb3ac811cf08f1db9b6e40d38c91894d"
SPECIFICATION_URL = "https://www.etsi.org/deliver/etsi_ts/146000_146099/146053/09.00.00_60/ts_146053v090000p.pdf"
MIRROR_URL = "https://www.freecalypso.org/pub/GSM/codecs/ts_146053v080000p0.zip"
CFLAGS = "-std=gnu89 -O2 -fwrapv -DWMOPS=0"
EXPECTED = {
    "cod": (80196, "506f476855c71e91e7180804d030c1bfbf146d762582e0de72f331c6629872cd"),
    "dec": (80522, "c3abf186ad28c28d8aa71737cf10a1d54fb2148b8c1ff1f2739cb9b92c139e68"),
    "out": (52160, "738af1ead354b8960504bd7d9656d53c1a11880051d01e79b97c276f739c187a"),
}
DTX_FRAMES = 518
DTX_SID_FRAMES = (51, 71, 95, 259, 263, 287, 311, 335)
INVALID_SID_FRAMES = (51, 95, 259, 311)
BAD_SID_FRAMES = (95, 263)
# TS 46.062 table 1: LTP LAG 1 bits b0/b1. Five LSF fields occupy
# 38 bits; the following nine-bit lag ends at serial bit 46 (zero based).
INVALID_SID_BITS = (45, 46)
INVALID_SID_EXPECTED = {
    'dec': 'ea9ecb8fa25d734bdcd5744362264a0a51d489682ad5bd12c16f5e9bfa45cc69',
    'out': '272757031568500197fe395934096dfd2f53a87f0af935a66de14e7ff7cc7d8d',
}
ERASURE_BURSTS = ((8, 1), (32, 3), (80, 8), (120, 16))
ERASURE_EXPECTED = {
    "dec": "ec37dbbd0f498b5e19b6e549b80687acafe03859c68ff5f337940a25dfb370d5",
    "out": "1c05e9951ffbe91fb25e2ddd4b349f4e563a021ebd5ad7d1e3115881ce326fc9",
}
HOMING_FRAMES = 46
HOMING_ERROR_FRAMES = 49
BAD_HOMING_FRAMES = (0, 23, 46)
STEADY_ERROR_HOMING_FRAMES = (2, 25, 48)
HOMING_ERROR_EXPECTED = {
    'dec': '4e0479de662a10c8e0f6ea9bcd491b6f3143bec22584655bf614b16ef841f7d4',
    'out': 'd742f052436adb03ccec6940eca0584c2d0c08095f7ad3b35b6de025b3b436e3',
}
HOMING_EXPECTED = {
    "dec": "0ca6dbc6caff2cf556aa6f7458dc4d8c3044750af1ad977a2af6edc6920d1fc1",
    "out": "7dad66375d871bb518cd00bac53e57b698a6fee1e1b3555cea0e06109a022205",
}
ENCODER_HOMING_EXPECTED = {
    "inp": "a443d4341b71c17f42610d5e873016e3da89b9b02c2bb01874840a144c35ca55",
    "cod": "b10eeee611bf9e8b77f8a8dd3f184d02731433d494ffec03017ede4b49a7bafd",
}


def encoder_homing_input(speech):
    if len(speech) != 52352:
        raise ValueError("encoder homing requires the complete installation input")
    pattern = struct.pack('<320h', *([8] * 320))
    segment = speech[:20 * 320]
    return pattern + segment + pattern + segment + pattern


def inspect_encoder_homing(coded):
    if len(coded) != HOMING_FRAMES * 492:
        raise ValueError("encoder homing frame count differs")
    for frame in struct.iter_unpack('<246H', coded):
        if any(bit > 1 for bit in frame) or frame[245] != 1:
            raise ValueError("encoder homing requires binary active-speech output")
    canonical = coded[492:984]
    if any(coded[frame * 492:(frame + 1) * 492] != canonical for frame in (0, 23, 45)):
        raise ValueError("steady encoder homing differs from reset output")
    first, second = coded[2 * 492:22 * 492], coded[24 * 492:44 * 492]
    if first == canonical * 20 or first != second:
        raise ValueError("encoder speech after homing does not reproduce from reset")
    return {"frames": HOMING_FRAMES, "homing_frames": [0, 1, 22, 23, 44, 45],
            "repeated_speech_frames": 20}


def generate_encoder_homing(source, commands, log):
    (source / "homing_enc.inp").write_bytes(encoder_homing_input((source / "spch_dos.inp").read_bytes()))
    for name in ("homing_enc.cod", "homing_enc_replay.cod"):
        command = ["./coder", "homing_enc.inp", name]
        commands.append(command)
        subprocess.run(command, cwd=source, stdout=log, stderr=subprocess.STDOUT, check=True)
    coded = (source / "homing_enc.cod").read_bytes()
    if coded != (source / "homing_enc_replay.cod").read_bytes():
        raise ValueError("encoder homing differs across fresh processes")
    outputs = {}
    for suffix in ("inp", "cod"):
        data = (source / f"homing_enc.{suffix}").read_bytes()
        if digest(data) != ENCODER_HOMING_EXPECTED[suffix]:
            raise ValueError("encoder homing reference differs from its local reproducibility pin")
        outputs[suffix] = {"bytes": len(data), "sha256": digest(data)}
    return {"coverage": inspect_encoder_homing(coded), "outputs": outputs,
            "fresh_process_replay": True, "published_expected_vector_claim": False}
DTX_EXPECTED = {
    "inp": "f7c76955e1f79777b6a67eab3c6cc346d13351974b40817a6765acc89739ba1d",
    "cod": "b06b7dd60d251b6fa48931ecb3e313f7b03a754bb0a0b3807be1781cd73e590b",
    "dec": "6d21b356fdeb8ff24309659a1caa8b5c6ad3f10ec49b998133fc628ffa9cb409",
    "out": "1d8a85ae33572c9e9035512fd35b59269e76a5914303b429380ebf57b3c6a385",
}


def dtx_input(speech):
    """Two complete supplied-speech segments separated by low-level noise."""
    if len(speech) != 52352:
        raise ValueError("DTX source requires the complete supplied installation input")
    complete = speech[:163 * 320]
    noise = bytearray()
    state = 0x51d
    for _ in range(192 * 160):
        state = (state * 1664525 + 1013904223) & 0xffffffff
        noise.extend(struct.pack('<h', (((state >> 24) & 15) - 8) * 8))
    return bytes(noise[:96 * 320]) + complete + bytes(noise[96 * 320:]) + complete


def inspect_dtx(coded, decoder, pcm):
    if (len(coded), len(decoder), len(pcm)) != (DTX_FRAMES * 492, DTX_FRAMES * 494, DTX_FRAMES * 320):
        raise ValueError("DTX reference frame counts differ")
    flags = []
    for frame in struct.iter_unpack("<246H", coded):
        if any(bit > 1 for bit in frame):
            raise ValueError("DTX encoder contains a nonbinary serial bit or flag")
        flags.append(frame[245])
    sid, bfi = {}, {}
    for frame in struct.iter_unpack("<247H", decoder):
        if frame[0] > 1 or frame[245] > 2 or frame[246] > 1 or any(bit > 1 for bit in frame[1:245]):
            raise ValueError("DTX decoder input contains an invalid bit or flag")
        sid[frame[245]] = sid.get(frame[245], 0) + 1
        bfi[frame[0]] = bfi.get(frame[0], 0) + 1
    resumptions = sum(a == 0 and b == 1 for a, b in zip(flags, flags[1:]))
    nonzero_cni = sum(not flag and any(pcm[i * 320:(i + 1) * 320]) for i, flag in enumerate(flags))
    if not (0 in flags and 1 in flags and resumptions >= 2 and sid.get(2, 0) and bfi.get(1, 0) and nonzero_cni):
        raise ValueError("DTX conversation did not exercise silence, SID, unusable frames and speech resumption")
    return {"frames": DTX_FRAMES, "speech_frames": sum(flags),
            "inactive_frames": flags.count(0), "speech_resumptions": resumptions,
            "sid": sid, "bfi": bfi, "nonzero_cni_frames": nonzero_cni}


def generate_dtx(source, commands, log):
    (source / "dtx.inp").write_bytes(dtx_input((source / "spch_dos.inp").read_bytes()))
    for prefix in ("dtx", "dtx_replay"):
        batch = [["./coder", "dtx.inp", f"{prefix}.cod", "dtx"],
                 ["./ed_iface", f"{prefix}.cod", f"{prefix}.dec"],
                 ["./decoder", f"{prefix}.dec", f"{prefix}.out"]]
        commands.extend(batch)
        for command in batch:
            subprocess.run(command, cwd=source, stdout=log, stderr=subprocess.STDOUT, check=True)
    outputs = {}
    for suffix in ("inp", "cod", "dec", "out"):
        data = (source / f"dtx.{suffix}").read_bytes()
        if suffix != "inp" and data != (source / f"dtx_replay.{suffix}").read_bytes():
            raise ValueError(f"DTX reference {suffix} is not deterministic across fresh processes")
        if digest(data) != DTX_EXPECTED[suffix]:
            raise ValueError(f"DTX generated {suffix} differs from its local reproducibility pin")
        outputs[suffix] = {"bytes": len(data), "sha256": digest(data)}
    coverage = inspect_dtx(*((source / f"dtx.{suffix}").read_bytes() for suffix in ("cod", "dec", "out")))
    return {"profile": "noise96-speech163-noise96-speech163",
            "outputs": outputs, "coverage": coverage, "fresh_process_replay": True,
            "published_expected_vector_claim": False}


def invalid_sid_input(decoder):
    if len(decoder) != DTX_FRAMES * 494:
        raise ValueError('invalid-SID source requires the complete DTX conversation')
    frames = list(struct.iter_unpack('<247H', decoder))
    if tuple(i for i, frame in enumerate(frames) if frame[245] == 2) != DTX_SID_FRAMES:
        raise ValueError('invalid-SID source differs from the known valid-SID schedule')
    for frame in frames:
        if frame[0] > 1 or frame[245] not in (0, 2) or frame[246] > 1 or any(bit > 1 for bit in frame[1:245]):
            raise ValueError('invalid-SID source contains invalid flags or speech bits')
    result = bytearray(decoder)
    for index in INVALID_SID_FRAMES:
        if frames[index][0] or any(frames[index][1 + bit] != 1 for bit in INVALID_SID_BITS):
            raise ValueError('invalid-SID source must have clean one-valued selected codeword bits')
        for bit in INVALID_SID_BITS:
            struct.pack_into('<H', result, index * 494 + 2 * (1 + bit), 0)
        struct.pack_into('<H', result, index * 494 + 490, 1)
    for index in BAD_SID_FRAMES:
        if frames[index][0]:
            raise ValueError('selected bad SID is already marked bad')
        struct.pack_into('<H', result, index * 494, 1)
    return bytes(result)


def lost_sid_input(decoder):
    """Lose two scheduled SID updates using decoder-entry channel flags only."""
    if digest(decoder) != DTX_EXPECTED['dec']:
        raise ValueError('lost-SID source must be the verified DTX conversation')
    result = bytearray(decoder)
    for index in (71, 95):
        frame = struct.unpack_from('<247H', decoder, index * 494)
        if (frame[0], frame[245], frame[246]) != (0, 2, 1):
            raise ValueError('lost-SID source must have a clean scheduled SID update')
        struct.pack_into('<H', result, index * 494, 1)
        struct.pack_into('<H', result, index * 494 + 490, 0)
    return bytes(result)


def check_sid_classes(text, decoder):
    if len(decoder) != DTX_FRAMES * 494:
        raise ValueError('SID classifier requires all 518 complete decoder frames')
    frames = list(struct.iter_unpack('<247H', decoder))
    rows = text.splitlines()
    if len(rows) != len(frames):
        raise ValueError('SID classifier did not cover every input frame')
    for index, (row, frame) in enumerate(zip(rows, frames)):
        match = re.fullmatch(r'EFRSID frame=(\d+) class=([012])', row)
        if not match or int(match[1]) != index or int(match[2]) != frame[245]:
            raise ValueError('payload-derived SID class disagrees with the decoder input')


def generate_invalid_sid(source, commands, log):
    decoder = invalid_sid_input((source / 'dtx.dec').read_bytes())
    (source / 'invalid_sid.dec').write_bytes(decoder)
    tools = Path(__file__).resolve().parent
    observer, rules = tools / 'etsi_efr_sid_observer.c', tools / 'etsi_efr_state_observer.mk'
    command = ['make', '-f', 'makefile.gcc', '-f', str(rules), f'CFLAGS={CFLAGS}',
               f'SID_OBSERVER_SOURCE={observer}', 'sid_observed']
    commands.append(command)
    subprocess.run(command, cwd=source, stdout=log, stderr=subprocess.STDOUT, check=True)
    command = ['./sid_observed', 'invalid_sid.dec']
    commands.append(command)
    with (source / 'invalid_sid_classes.log').open('w') as classes:
        subprocess.run(command, cwd=source, stdout=classes, stderr=log, check=True)
    check_sid_classes((source / 'invalid_sid_classes.log').read_text(), decoder)
    for name in ('invalid_sid.out', 'invalid_sid_replay.out'):
        command = ['./decoder', 'invalid_sid.dec', name]
        commands.append(command)
        subprocess.run(command, cwd=source, stdout=log, stderr=subprocess.STDOUT, check=True)
    pcm = (source / 'invalid_sid.out').read_bytes()
    if len(pcm) != DTX_FRAMES * 320 or pcm != (source / 'invalid_sid_replay.out').read_bytes():
        raise ValueError('invalid-SID decoder lacks complete fresh-process replay')
    frames = list(struct.iter_unpack('<247H', decoder))
    if sum(frame[245] == 1 for frame in frames) != 4 or sum(bool(frame[0]) and frame[245] == 2 for frame in frames) != 1:
        raise ValueError('invalid-SID classification coverage differs')
    if any(digest(data) != INVALID_SID_EXPECTED[suffix] for suffix, data in (('dec', decoder), ('out', pcm))):
        raise ValueError('invalid-SID reference differs from its local reproducibility pin')
    baseline = (source / 'dtx.out').read_bytes()
    changed = sum(pcm[i:i + 320] != baseline[i:i + 320] for i in range(0, len(pcm), 320))
    if not changed:
        raise ValueError('invalid-SID conversation has no output effect relative to clean DTX')
    return dict(frames=DTX_FRAMES, invalid_codewords=4, bad_valid_codewords=1,
                classifier_matches_input=True, fresh_process_replay=True,
                changed_frames_vs_clean_dtx=changed,
                published_expected_vector_claim=False, observer_sha256=digest(observer.read_bytes()),
                classifier_sha256=digest((source / 'sid_observed').read_bytes()),
                classifier_records_sha256=digest((source / 'invalid_sid_classes.log').read_bytes()),
                rules_sha256=digest(rules.read_bytes()),
                outputs={suffix: dict(bytes=len(data), sha256=digest(data))
                         for suffix, data in (('dec', decoder), ('out', pcm))})


def digest(data):
    return hashlib.sha256(data).hexdigest()


def observe_erasures(source, commands, log):
    """Link read-only wrappers without changing the verified decoder binary."""
    tools = Path(__file__).resolve().parent
    wrapper, rules = tools / 'etsi_efr_state_observer.c', tools / 'etsi_efr_state_observer.mk'
    original_decoder = digest((source / 'decoder').read_bytes())
    command = ['make', '-f', 'makefile.gcc', '-f', str(rules),
               f'CFLAGS={CFLAGS}', f'OBSERVER_SOURCE={wrapper}', 'decoder_observed']
    commands.append(command)
    subprocess.run(command, cwd=source, stdout=log, stderr=subprocess.STDOUT, check=True)
    if digest((source / 'decoder').read_bytes()) != original_decoder:
        raise ValueError('EFR observer build replaced the verified decoder')
    for suffix in ('', '_replay'):
        command = ['./decoder_observed', 'erasure.dec', f'erasure_observed{suffix}.out']
        commands.append(command)
        with (source / f'erasure_state{suffix}.log').open('w') as state_log:
            subprocess.run(command, cwd=source, stdout=log, stderr=state_log, check=True)
        if (source / f'erasure_observed{suffix}.out').read_bytes() != (source / 'erasure.out').read_bytes():
            raise ValueError('read-only EFR observer changed reference PCM')
    try:
        from tools.c54x_rom4_efr_state_check import parse, parse_policy
    except ModuleNotFoundError:
        from c54x_rom4_efr_state_check import parse, parse_policy
    records = []
    for suffix in ('', '_replay'):
        text = (source / f'erasure_state{suffix}.log').read_text()
        parse(text)
        parse_policy(text)
        records.append(('\n'.join(line for line in text.splitlines()
                                  if line.startswith(('EFRSTATE ', 'EFRGAIN ', 'EFRPOLICY '))) + '\n').encode())
    state = records[0]
    if state != records[1]:
        raise ValueError('EFR state observations differ across fresh processes')
    (source / 'erasure_state.records').write_bytes(state)
    return dict(reference_pcm_unchanged=True, fresh_process_replay=True,
                original_decoder_sha256=original_decoder,
                state_records_sha256=digest(state), wrapper_sha256=digest(wrapper.read_bytes()),
                rules_sha256=digest(rules.read_bytes()),
                executable_sha256=digest((source / 'decoder_observed').read_bytes()))


def erasure_input(decoder):
    """Retain the installation payload/TAF and mark fixed active-speech losses."""
    if len(decoder) != 163 * 494:
        raise ValueError("erasure source requires 163 complete decoder frames")
    for frame in struct.iter_unpack('<247H', decoder):
        if frame[0] or frame[245] or frame[246] > 1 or any(bit > 1 for bit in frame[1:245]):
            raise ValueError("erasure source requires clean active speech")
    result = bytearray(decoder)
    for start, count in ERASURE_BURSTS:
        for frame in range(start, start + count):
            struct.pack_into('<H', result, frame * 494, 1)
    return bytes(result)


def inspect_erasures(decoder, pcm, clean_pcm):
    if len(pcm) != 163 * 320 or len(clean_pcm) != len(pcm):
        raise ValueError("erasure PCM requires 163 complete frames")
    if len(decoder) != 163 * 494:
        raise ValueError("erasure input requires 163 complete frames")
    expected = {f for start, count in ERASURE_BURSTS for f in range(start, start + count)}
    actual = set()
    for i, frame in enumerate(struct.iter_unpack('<247H', decoder)):
        if frame[0] > 1 or frame[245] or frame[246] > 1 or any(bit > 1 for bit in frame[1:245]):
            raise ValueError("erasure input has invalid active-speech flags or bits")
        if frame[0]:
            actual.add(i)
    if actual != expected:
        raise ValueError("erasure schedule differs")
    for start, count in ERASURE_BURSTS:
        begin, end = start * 320, (start + count) * 320
        if pcm[begin:end] == clean_pcm[begin:end] or not any(pcm[begin:end]):
            raise ValueError("erasure burst lacks nonzero concealment differing from clean speech")
    return {"frames": 163, "erased_frames": len(actual),
            "bursts": [{"start": start, "frames": count} for start, count in ERASURE_BURSTS],
            "changed_frames": sum(pcm[i:i + 320] != clean_pcm[i:i + 320]
                                  for i in range(0, len(pcm), 320))}


def generate_erasures(source, commands, log):
    (source / "erasure.dec").write_bytes(erasure_input((source / "verified.dec").read_bytes()))
    for name in ("erasure.out", "erasure_replay.out"):
        command = ["./decoder", "erasure.dec", name]
        commands.append(command)
        subprocess.run(command, cwd=source, stdout=log, stderr=subprocess.STDOUT, check=True)
    pcm = (source / "erasure.out").read_bytes()
    if pcm != (source / "erasure_replay.out").read_bytes():
        raise ValueError("erasure decoding differs across fresh processes")
    decoder = (source / "erasure.dec").read_bytes()
    coverage = inspect_erasures(decoder, pcm, (source / "verified.out").read_bytes())
    if any(digest(data) != ERASURE_EXPECTED[suffix] for suffix, data in (("dec", decoder), ("out", pcm))):
        raise ValueError("erasure reference differs from its local reproducibility pin")
    return {"coverage": coverage, "fresh_process_replay": True,
            "outputs": {suffix: {"bytes": len(data), "sha256": digest(data)}
                        for suffix, data in (("dec", decoder), ("out", pcm))},
            "published_expected_vector_claim": False}


def homing_conversation(canonical, speech, boundary_frames=2):
    """Canonical DHF boundaries surrounding two identical speech segments."""
    if len(canonical) != 492 or len(speech) != 80196:
        raise ValueError("homing source requires one encoded frame and verified installation speech")
    if any(word > 1 for (word,) in struct.iter_unpack('<H', canonical)):
        raise ValueError("homing source contains a nonbinary bit or flag")
    if boundary_frames not in (2, 3):
        raise ValueError('homing boundaries require two or three frames')
    segment = speech[:20 * 492]
    boundary = canonical * boundary_frames
    return boundary + segment + boundary + segment + boundary


def homing_error_input(decoder):
    if len(decoder) != HOMING_ERROR_FRAMES * 494:
        raise ValueError('homing/error source requires all 49 frames')
    frames = list(struct.iter_unpack('<247H', decoder))
    if any(row[0] or row[245] or row[246] > 1 or any(bit > 1 for bit in row[1:245]) for row in frames):
        raise ValueError('homing/error source requires clean active-speech input')
    for start in BAD_HOMING_FRAMES:
        if any(frames[index][1:245] != frames[0][1:245] for index in range(start, start + 3)):
            raise ValueError('homing/error boundaries must retain the same canonical payload')
    result = bytearray(decoder)
    for frame in BAD_HOMING_FRAMES:
        struct.pack_into('<H', result, frame * 494, 1)
    return bytes(result)


def inspect_homing_errors(decoder, pcm):
    if len(decoder) != HOMING_ERROR_FRAMES * 494 or len(pcm) != HOMING_ERROR_FRAMES * 320:
        raise ValueError('homing/error conversation frame counts differ')
    frames = list(struct.iter_unpack('<247H', decoder))
    for index, row in enumerate(frames):
        if row[0] != int(index in BAD_HOMING_FRAMES) or row[245] or row[246] > 1 or any(bit > 1 for bit in row[1:245]):
            raise ValueError('homing/error flags or speech bits differ')
    pattern = b'\x08\x00' * 160
    if any(pcm[index * 320:(index + 1) * 320] != pattern for index in STEADY_ERROR_HOMING_FRAMES):
        raise ValueError('steady homing after a bad frame did not produce EHF')
    if any(pcm[index * 320:(index + 1) * 320] == pattern for index in BAD_HOMING_FRAMES):
        raise ValueError('bad homing fixture unexpectedly produces the EHF mask')
    first, second = pcm[3 * 320:23 * 320], pcm[26 * 320:46 * 320]
    if not any(first) or first == pattern * 20 or first != second:
        raise ValueError('speech after bad/clean homing does not reproduce from reset')
    return dict(frames=HOMING_ERROR_FRAMES, bad_homing_frames=list(BAD_HOMING_FRAMES),
                steady_ehf_frames=list(STEADY_ERROR_HOMING_FRAMES), repeated_speech_frames=20)


def generate_homing_errors(source, commands, log):
    canonical = (source / 'homing_probe.cod').read_bytes()[492:]
    (source / 'homing_error.cod').write_bytes(homing_conversation(canonical, (source / 'verified.cod').read_bytes(), 3))
    command = ['./ed_iface', 'homing_error.cod', 'homing_error_clean.dec']
    commands.append(command)
    subprocess.run(command, cwd=source, stdout=log, stderr=subprocess.STDOUT, check=True)
    decoder = homing_error_input((source / 'homing_error_clean.dec').read_bytes())
    (source / 'homing_error.dec').write_bytes(decoder)
    for name in ('homing_error.out', 'homing_error_replay.out'):
        command = ['./decoder', 'homing_error.dec', name]
        commands.append(command)
        subprocess.run(command, cwd=source, stdout=log, stderr=subprocess.STDOUT, check=True)
    pcm = (source / 'homing_error.out').read_bytes()
    if pcm != (source / 'homing_error_replay.out').read_bytes():
        raise ValueError('homing/error reference differs across fresh processes')
    if any(digest(data) != HOMING_ERROR_EXPECTED[suffix] for suffix, data in (('dec', decoder), ('out', pcm))):
        raise ValueError('homing/error reference differs from its local reproducibility pin')
    return dict(coverage=inspect_homing_errors(decoder, pcm), fresh_process_replay=True,
                published_expected_vector_claim=False,
                outputs={suffix: dict(bytes=len(data), sha256=digest(data))
                         for suffix, data in (('dec', decoder), ('out', pcm))})


def inspect_homing(decoder, pcm):
    if len(decoder) != HOMING_FRAMES * 494 or len(pcm) != HOMING_FRAMES * 320:
        raise ValueError("homing conversation frame count differs")
    for frame in struct.iter_unpack('<247H', decoder):
        if frame[0] or frame[245] or frame[246] > 1 or any(bit > 1 for bit in frame[1:245]):
            raise ValueError("homing conversation requires clean active-speech frames")
    pattern = struct.pack('<160h', *([8] * 160))
    for frame in (0, 1, 23, 45):
        if pcm[frame * 320:(frame + 1) * 320] != pattern:
            raise ValueError("steady homing output is not the EHF pattern")
    first, second = pcm[2 * 320:22 * 320], pcm[24 * 320:44 * 320]
    if not any(first) or first == pattern * 20 or first != second:
        raise ValueError("speech after homing does not reproduce from reset")
    return {"frames": HOMING_FRAMES, "homing_frames": [0, 1, 22, 23, 44, 45],
            "steady_ehf_frames": [0, 1, 23, 45], "repeated_speech_frames": 20}


def generate_homing(source, commands, log):
    (source / "homing_probe.inp").write_bytes(struct.pack('<320h', *([8] * 320)))
    command = ["./coder", "homing_probe.inp", "homing_probe.cod"]
    commands.append(command)
    subprocess.run(command, cwd=source, stdout=log, stderr=subprocess.STDOUT, check=True)
    probe = (source / "homing_probe.cod").read_bytes()
    if len(probe) != 2 * 492:
        raise ValueError("homing encoder did not produce two complete frames")
    canonical = probe[492:]
    (source / "homing.cod").write_bytes(homing_conversation(canonical, (source / "verified.cod").read_bytes()))
    for prefix in ("homing", "homing_replay"):
        for command in (["./ed_iface", "homing.cod", f"{prefix}.dec"],
                        ["./decoder", f"{prefix}.dec", f"{prefix}.out"]):
            commands.append(command)
            subprocess.run(command, cwd=source, stdout=log, stderr=subprocess.STDOUT, check=True)
    outputs = {}
    for suffix in ("dec", "out"):
        data = (source / f"homing.{suffix}").read_bytes()
        if data != (source / f"homing_replay.{suffix}").read_bytes():
            raise ValueError("homing reference differs across fresh processes")
        if digest(data) != HOMING_EXPECTED[suffix]:
            raise ValueError("homing reference differs from its local reproducibility pin")
        outputs[suffix] = {"bytes": len(data), "sha256": digest(data)}
    coverage = inspect_homing((source / "homing.dec").read_bytes(), (source / "homing.out").read_bytes())
    return {"coverage": coverage, "outputs": outputs, "canonical_sha256": digest(canonical),
            "fresh_process_replay": True, "published_expected_vector_claim": False}


def validate_members(names):
    """This specific archive is flat; reject paths and lowercase collisions."""
    seen = set()
    for name in names:
        if not name or "/" in name or "\\" in name or name in (".", ".."):
            raise ValueError("reference archive requires flat filenames")
        lower = name.lower()
        if lower in seen:
            raise ValueError("reference archive has case-colliding filenames")
        seen.add(lower)


def check_vectors(directory):
    directory = Path(directory)
    results = {}
    for suffix, (length, expected_hash) in EXPECTED.items():
        expected = (directory / f"spch_dos.{suffix}").read_bytes()
        actual = (directory / f"verified.{suffix}").read_bytes()
        if len(expected) != length or digest(expected) != expected_hash:
            raise ValueError(f"reference {suffix} verification input differs")
        if actual != expected:
            raise ValueError(f"reference {suffix} output is not bit-exact")
        results[suffix] = {"bytes": len(actual), "sha256": digest(actual)}
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    parser.add_argument("--output-root", type=Path,
                        default=Path(__file__).resolve().parents[1] / "tmp")
    parser.add_argument("--fixture-dir", type=Path,
                        help="export verified codec inputs/outputs to isolated MAME fixture ROM storage")
    parser.add_argument("--dtx-fixture", action="store_true",
                        help="generate a deterministic DTX conversation after installation verification")
    parser.add_argument("--erasure-fixture", action="store_true",
                        help="generate fixed active-speech BFI bursts after installation verification")
    parser.add_argument("--homing-fixture", action="store_true",
                        help="generate a repeated-speech decoder homing conversation")
    parser.add_argument("--encoder-homing-fixture", action="store_true",
                        help="generate repeated speech and EHF inputs for encoder reset coverage")
    parser.add_argument('--erasure-state-observer', action='store_true',
                        help='also link output-preserving erasure-state observers; requires --erasure-fixture')
    parser.add_argument('--invalid-sid-fixture', action='store_true',
                        help='derive a payload-classified invalid-SID DTX conversation')
    parser.add_argument('--homing-error-fixture', action='store_true',
                        help='derive bad-DHF/clean-DHF recovery and repeated speech')
    args = parser.parse_args()
    if args.erasure_state_observer and not args.erasure_fixture:
        parser.error('--erasure-state-observer requires --erasure-fixture')
    evidence_path = None
    evidence = {}
    try:
        archive = args.archive.read_bytes()
        if digest(archive) != ARCHIVE_SHA256:
            raise ValueError("ETSI reference archive SHA-256 differs")
        args.output_root.mkdir(parents=True, exist_ok=True)
        run = Path(tempfile.mkdtemp(prefix="run_etsi_efr_", dir=args.output_root)).resolve()
        source = run / "source"
        source.mkdir()
        with zipfile.ZipFile(args.archive) as zipped:
            validate_members(zipped.namelist())
            for member in zipped.infolist():
                (source / member.filename.lower()).write_bytes(zipped.read(member))
        commands = [
            ["make", "-f", "makefile.gcc", f"CFLAGS={CFLAGS}", "-j4"],
            ["./coder", "spch_dos.inp", "verified.cod"],
            ["./ed_iface", "verified.cod", "verified.dec"],
            ["./decoder", "verified.dec", "verified.out"],
        ]
        evidence = {"schema": 1, "archive": str(args.archive.resolve()),
                    "archive_sha256": ARCHIVE_SHA256, "specification_url": SPECIFICATION_URL,
                    "mirror_url": MIRROR_URL, "commands": commands,
                    "native_dsp_execution_claim": False, "status": "running"}
        evidence_path = run / "evidence.json"
        evidence_path.write_text(json.dumps(evidence, indent=2) + "\n")
        print(f"ETSI EFR reference evidence: {run}", flush=True)
        with (run / "build-and-run.log").open("w") as log:
            for command in commands:
                subprocess.run(command, cwd=source, stdout=log,
                               stderr=subprocess.STDOUT, check=True)
        evidence["outputs"] = check_vectors(source)
        if args.dtx_fixture or args.invalid_sid_fixture:
            with (run / "build-and-run.log").open("a") as log:
                evidence["dtx"] = generate_dtx(source, commands, log)
        if args.invalid_sid_fixture:
            with (run / 'build-and-run.log').open('a') as log:
                evidence['invalid_sid'] = generate_invalid_sid(source, commands, log)
        if args.erasure_fixture:
            with (run / "build-and-run.log").open("a") as log:
                evidence["erasures"] = generate_erasures(source, commands, log)
                if args.erasure_state_observer:
                    evidence['erasure_state_observer'] = observe_erasures(source, commands, log)
        if args.homing_fixture or args.homing_error_fixture:
            with (run / "build-and-run.log").open("a") as log:
                evidence["homing"] = generate_homing(source, commands, log)
        if args.homing_error_fixture:
            with (run / 'build-and-run.log').open('a') as log:
                evidence['homing_errors'] = generate_homing_errors(source, commands, log)
        if args.encoder_homing_fixture:
            with (run / "build-and-run.log").open("a") as log:
                evidence["encoder_homing"] = generate_encoder_homing(source, commands, log)
        evidence["frames"] = evidence["outputs"]["out"]["bytes"] // 320
        evidence["unused_input_samples"] = len((source / "spch_dos.inp").read_bytes()) // 2 - evidence["frames"] * 160
        evidence["executables"] = {name: digest((source / name).read_bytes())
                                   for name in ("coder", "ed_iface", "decoder")}
        evidence["status"] = "pass"
        if args.fixture_dir is not None:
            args.fixture_dir.mkdir(parents=True, exist_ok=True)
            for suffix in ("inp", "cod", "dec", "out"):
                name = f"spch_dos.{suffix}"
                (args.fixture_dir / name).write_bytes((source / name).read_bytes())
                if args.dtx_fixture:
                    name = f"dtx.{suffix}"
                    (args.fixture_dir / name).write_bytes((source / name).read_bytes())
            evidence["fixture_directory"] = str(args.fixture_dir.resolve())
            if args.erasure_fixture:
                for suffix in ("dec", "out"):
                    name = f"erasure.{suffix}"
                    (args.fixture_dir / name).write_bytes((source / name).read_bytes())
            if args.invalid_sid_fixture:
                for suffix in ('dec', 'out'):
                    name = f'invalid_sid.{suffix}'
                    (args.fixture_dir / name).write_bytes((source / name).read_bytes())
            if args.homing_fixture:
                for suffix in ("dec", "out"):
                    name = f"homing.{suffix}"
                    (args.fixture_dir / name).write_bytes((source / name).read_bytes())
            if args.homing_error_fixture:
                for suffix in ('dec', 'out'):
                    name = f'homing_error.{suffix}'
                    (args.fixture_dir / name).write_bytes((source / name).read_bytes())
            if args.encoder_homing_fixture:
                for suffix in ("inp", "cod"):
                    name = f"homing_enc.{suffix}"
                    (args.fixture_dir / name).write_bytes((source / name).read_bytes())
        evidence_path.write_text(json.dumps(evidence, indent=2) + "\n")
    except (OSError, ValueError, zipfile.BadZipFile, subprocess.CalledProcessError) as error:
        if evidence_path is not None:
            evidence.update(status="failed", error=str(error))
            evidence_path.write_text(json.dumps(evidence, indent=2) + "\n")
        print(f"ETSI EFR reference: FAIL: {error}", file=sys.stderr)
        return 1
    print("ETSI EFR reference: PASS frames=163 encoder=exact interface=exact decoder=exact native_dsp_claim=0")
    if args.dtx_fixture:
        print("ETSI EFR DTX reference: PASS frames=518 fresh_process_replay=1 published_vector_claim=0 native_dsp_claim=0")
    if args.erasure_fixture:
        print("ETSI EFR erasure reference: PASS frames=163 erased=28 bursts=1,3,8,16 fresh_process_replay=1 published_vector_claim=0 native_dsp_claim=0")
    if args.invalid_sid_fixture:
        print('ETSI EFR invalid-SID reference: PASS frames=518 invalid_codewords=4 bad_valid_codewords=1 payload_classification=1 fresh_process_replay=1 published_vector_claim=0 native_dsp_claim=0')
    if args.homing_fixture:
        print("ETSI EFR homing reference: PASS frames=46 homing=6 repeated_speech=20 fresh_process_replay=1 published_vector_claim=0 native_dsp_claim=0")
    if args.homing_error_fixture:
        print('ETSI EFR homing/error reference: PASS frames=49 bad_homing=3 steady_ehf=3 repeated_speech=20 fresh_process_replay=1 published_vector_claim=0 native_dsp_claim=0')
    if args.encoder_homing_fixture:
        print("ETSI EFR encoder homing reference: PASS frames=46 homing=6 repeated_speech=20 fresh_process_replay=1 published_vector_claim=0 native_dsp_claim=0")
    return 0


if __name__ == "__main__":
    sys.exit(main())

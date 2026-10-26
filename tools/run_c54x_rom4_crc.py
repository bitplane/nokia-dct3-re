#!/usr/bin/env python3
"""Run isolated acquired ROM4 algorithms, not handset audio delivery."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile

try:
    from tools.c54x_profiles import harness_sources
except ModuleNotFoundError:
    from c54x_profiles import harness_sources

PASS = "TMS320C54x ROM4 CRC helper: PASS cases=512 gsm_packing_claim=0 speech_claim=0"
COMPLETE = "Fatal error: TMS320C54x ROM4 CRC helper complete"
PACKED_PASS = "TMS320C54x ROM4 packed CRC: PASS cases=516 basis_bits=192 gsm_crc_claim=1 speech_claim=0"
PACKED_COMPLETE = "Fatal error: TMS320C54x ROM4 packed CRC complete"
FR_PASS = "TMS320C54x ROM4 FR packing: PASS vectors=293 basis_bits=260 protected_bits=50 speech_claim=0"
FR_COMPLETE = "Fatal error: TMS320C54x ROM4 FR packing complete"
DECODER_PASS = "TMS320C54x ROM4 FR decoder: PASS frames=32 samples=5120 handset_audio_claim=0"
DECODER_COMPLETE = "Fatal error: TMS320C54x ROM4 FR decoder complete"
DECODER_COMPARISON = "TMS320C54x ROM4 FR decoder comparison: frames=32 samples=5120 mismatches=0"
DECODER_REPLAY = "TMS320C54x ROM4 FR decoder replay: PASS frames=32 ram_words=65440 checkpoints=frame_input reference_advance=once handset_audio_claim=0"
ENCODER_PASS = "TMS320C54x ROM4 FR encoder: PASS frames=32 parameters=2432 handset_audio_claim=0"
ENCODER_COMPLETE = "Fatal error: TMS320C54x ROM4 FR encoder complete"
ENCODER_COMPARISON = "ROM4 FR encoder aggregate: frames=32 parameters=2432 mismatches=0"
ENCODER_REPLAY = "TMS320C54x ROM4 FR encoder replay: PASS frames=32 ram_words=65440 checkpoints=before_preprocessing reference_advance=once handset_audio_claim=0"
PCM_MUTE_PASS = "TMS320C54x ROM4 PCM mute: PASS cases=8 samples=1280 guards=16 handset_audio_claim=0"
EFR_CRC_PASS = "TMS320C54x ROM4 EFR CRC: PASS cases=554 basis_bits=244 protected_bits=65 handset_audio_claim=0"
EFR_CRC_COMPLETE = "Fatal error: TMS320C54x ROM4 EFR CRC complete"
EFR_PACKING_PASS = "TMS320C54x ROM4 EFR packing: PASS vectors=277 basis_bits=244 corrupt_frames=277 handset_audio_claim=0"
EFR_PACKING_COMPLETE = "Fatal error: TMS320C54x ROM4 EFR packing complete"
EFR_DECODER_PASS = "TMS320C54x ROM4 EFR decoder: PASS frames=163 samples=26080 mismatches=0 handset_audio_claim=0"
EFR_DECODER_COMPLETE = "Fatal error: TMS320C54x ROM4 EFR decoder complete"
DELAYED_REPEAT_PASS = "TMS320C54x ROM4 delayed repeat: PASS cases=10 replay=10 controls=BD,RETD counts=0,1,2,9,39"
EFR_DECODER_REPLAY = "TMS320C54x ROM4 EFR decoder replay: PASS frames=163 ram_words=65440 checkpoints=frame_input handset_audio_claim=0"
EFR_ENCODER_PASS = "TMS320C54x ROM4 EFR encoder: PASS frames=163 parameters=9291 flags=326 packed_words=2771 mismatches=0 handset_audio_claim=0"
EFR_ENCODER_COMPLETE = "Fatal error: TMS320C54x ROM4 EFR encoder complete"
EFR_ENCODER_REPLAY = "TMS320C54x ROM4 EFR encoder replay: PASS frames=163 ram_words=65440 checkpoints=before_preprocessing handset_audio_claim=0"
EFR_DTX_DECODER_PASS = "TMS320C54x ROM4 EFR DTX decoder: PASS frames=518 samples=82880 sid=8 erased=135 nonzero_cni_frames=46 mismatches=0 handset_audio_claim=0"
EFR_DTX_DECODER_COMPLETE = "Fatal error: TMS320C54x ROM4 EFR DTX decoder complete"
EFR_DTX_DECODER_REPLAY = "TMS320C54x ROM4 EFR DTX decoder replay: PASS frames=518 ram_words=65440 checkpoints=frame_input handset_audio_claim=0"
EFR_INVALID_SID_PASS = "TMS320C54x ROM4 EFR invalid SID decoder: PASS frames=518 samples=82880 invalid_codewords=4 bad_valid_codewords=1 control_checked=518 mismatches=0 handset_audio_claim=0"
EFR_INVALID_SID_REPLAY = "TMS320C54x ROM4 EFR invalid SID decoder replay: PASS frames=518 ram_words=65440 checkpoints=frame_input handset_audio_claim=0"
EFR_INVALID_SID_COMPLETE = "Fatal error: TMS320C54x ROM4 EFR invalid SID decoder complete"
EFR_DTX_ENCODER_PASS = "TMS320C54x ROM4 EFR DTX encoder: PASS frames=518 parameters=29526 flags=1036 packed_words=8806 mismatches=0 handset_audio_claim=0"
EFR_DTX_ENCODER_COMPLETE = "Fatal error: TMS320C54x ROM4 EFR DTX encoder complete"
EFR_DTX_ENCODER_REPLAY = "TMS320C54x ROM4 EFR DTX encoder replay: PASS frames=518 ram_words=65440 checkpoints=before_preprocessing handset_audio_claim=0"
EFR_ERASURE_PASS = "TMS320C54x ROM4 EFR erasure decoder: PASS frames=163 samples=26080 erased=28 bursts=1,3,8,16 mismatches=0 handset_audio_claim=0"
EFR_ERASURE_COMPLETE = "Fatal error: TMS320C54x ROM4 EFR erasure decoder complete"
EFR_ERASURE_REPLAY = "TMS320C54x ROM4 EFR erasure decoder replay: PASS frames=163 ram_words=65440 checkpoints=frame_input handset_audio_claim=0"
EFR_HOMING_PASS = "TMS320C54x ROM4 EFR homing decoder: PASS frames=46 samples=7360 homing=6 steady_ehf=4 repeated_speech=20 mismatches=0 handset_audio_claim=0"
EFR_HOMING_COMPLETE = "Fatal error: TMS320C54x ROM4 EFR homing decoder complete"
EFR_HOMING_REPLAY = "TMS320C54x ROM4 EFR homing decoder replay: PASS frames=46 ram_words=65440 checkpoints=frame_input handset_audio_claim=0"
EFR_ENCODER_HOMING_PASS = "TMS320C54x ROM4 EFR homing encoder: PASS frames=46 parameters=2622 flags=92 packed_words=782 homing=6 repeated_speech=20 mismatches=0 handset_audio_claim=0"
EFR_ENCODER_HOMING_COMPLETE = "Fatal error: TMS320C54x ROM4 EFR homing encoder complete"
EFR_ENCODER_HOMING_REPLAY = "TMS320C54x ROM4 EFR homing encoder replay: PASS frames=46 ram_words=65440 checkpoints=before_preprocessing handset_audio_claim=0"


def check_staged_frontend(text, kind):
    if kind is None or text.splitlines().count(f'ROM4 {kind} frontend: staged handset_audio_claim=0') != 1:
        raise ValueError('missing unique staged codec frontend selection')


def file_identity(path):
    path = Path(path).resolve()
    with path.open("rb") as source:
        digest = hashlib.file_digest(source, "sha256").hexdigest()
    return {"path": str(path), "bytes": path.stat().st_size, "sha256": digest}


def check_result(text, returncode, packed=False, fr_packing=False, fr_decoder=False, fr_encoder=False, efr_crc=False, efr_packing=False, efr_decoder=False, efr_encoder=False, efr_dtx_decoder=False, efr_dtx_encoder=False, efr_erasure_decoder=False, efr_homing_decoder=False, efr_homing_encoder=False, efr_invalid_sid=False, staged=False, verbose_teardown=False, efr_lost_sid=False):
    if re.search(r'^Driver .+: [1-9]\d* errors?,', text, re.MULTILINE):
        raise ValueError('MAME driver validity errors invalidate codec acceptance')
    lines = text.splitlines()
    if verbose_teardown and lines[-2:] == ['Enter sdlwindow_exit', 'Leave sdlwindow_exit']:
        lines = lines[:-2]
    if staged:
        kind = ('FR decoder' if fr_decoder else 'FR encoder' if fr_encoder else
                'EFR encoder' if efr_encoder else 'EFR decoder' if efr_decoder else
                'EFR DTX encoder' if efr_dtx_encoder else 'EFR DTX decoder' if efr_dtx_decoder else
                'EFR homing encoder' if efr_homing_encoder else 'EFR homing decoder' if efr_homing_decoder else
                'EFR lost SID decoder' if efr_lost_sid else 'EFR invalid SID decoder' if efr_invalid_sid else 'EFR erasure decoder' if efr_erasure_decoder else None)
        check_staged_frontend(text, kind)
    if (efr_decoder or efr_encoder or efr_dtx_decoder or efr_dtx_encoder or efr_erasure_decoder or efr_homing_decoder or efr_homing_encoder or efr_invalid_sid or efr_lost_sid) and lines and re.fullmatch(r"Average speed: \d+(?:\.\d+)?% \(\d+ seconds\)", lines[-1]):
        lines.pop()
    marker, complete = (PACKED_PASS, PACKED_COMPLETE) if packed else (PASS, COMPLETE)
    if efr_lost_sid:
        marker = 'TMS320C54x ROM4 EFR lost SID decoder: PASS frames=518 samples=82880 lost_sid=2 mismatches=0 handset_audio_claim=0'
        complete = 'Fatal error: TMS320C54x ROM4 EFR lost SID decoder complete'
        replay = 'TMS320C54x ROM4 EFR lost SID decoder replay: PASS frames=518 ram_words=65440 checkpoints=frame_input handset_audio_claim=0'
        if lines.count(replay) != 1:
            raise ValueError('missing unique native lost-SID frame/history replay')
        for frame in range(518):
            if lines.count(f'ROM4 EFR lost SID decoder comparison: frame={frame} mismatches=0') != 1:
                raise ValueError('missing unique bit-exact lost-SID frame comparison')
    if efr_crc:
        marker, complete = EFR_CRC_PASS, EFR_CRC_COMPLETE
    if efr_packing:
        marker, complete = EFR_PACKING_PASS, EFR_PACKING_COMPLETE
    if efr_decoder:
        marker, complete = EFR_DECODER_PASS, EFR_DECODER_COMPLETE
        if lines.count(DELAYED_REPEAT_PASS) != 1:
            raise ValueError("missing unique delayed-repeat regression and replay result")
        if lines.count(EFR_DECODER_REPLAY) != 1:
            raise ValueError("missing unique native EFR frame/history replay")
        for frame in range(163):
            if lines.count(f"ROM4 EFR decoder comparison: frame={frame} mismatches=0") != 1:
                raise ValueError("missing unique bit-exact EFR frame comparison")
    if efr_encoder:
        marker, complete = EFR_ENCODER_PASS, EFR_ENCODER_COMPLETE
        if lines.count(EFR_ENCODER_REPLAY) != 1:
            raise ValueError("missing unique native EFR encoder frame/history replay")
        for frame in range(163):
            if lines.count(f"ROM4 EFR encoder comparison: frame={frame} mismatches=0") != 1:
                raise ValueError("missing unique bit-exact EFR encoder frame comparison")
    if efr_dtx_decoder:
        marker, complete = EFR_DTX_DECODER_PASS, EFR_DTX_DECODER_COMPLETE
        if lines.count(EFR_DTX_DECODER_REPLAY) != 1:
            raise ValueError("missing unique native EFR DTX decoder frame/history replay")
        for frame in range(518):
            if lines.count(f"ROM4 EFR DTX decoder comparison: frame={frame} mismatches=0") != 1:
                raise ValueError("missing unique bit-exact EFR DTX decoder frame comparison")
    if efr_invalid_sid:
        marker, complete = EFR_INVALID_SID_PASS, EFR_INVALID_SID_COMPLETE
        if lines.count(EFR_INVALID_SID_REPLAY) != 1:
            raise ValueError('missing unique native invalid-SID frame/history replay')
        for frame in range(518):
            if lines.count(f'ROM4 EFR invalid SID decoder comparison: frame={frame} mismatches=0') != 1:
                raise ValueError('missing unique bit-exact invalid-SID frame comparison')
    if efr_dtx_encoder:
        marker, complete = EFR_DTX_ENCODER_PASS, EFR_DTX_ENCODER_COMPLETE
        if lines.count(EFR_DTX_ENCODER_REPLAY) != 1:
            raise ValueError("missing unique native EFR DTX encoder frame/history replay")
        for frame in range(518):
            if lines.count(f"ROM4 EFR DTX encoder comparison: frame={frame} mismatches=0") != 1:
                raise ValueError("missing unique bit-exact EFR DTX encoder frame comparison")
    if efr_erasure_decoder:
        marker, complete = EFR_ERASURE_PASS, EFR_ERASURE_COMPLETE
        if lines.count(EFR_ERASURE_REPLAY) != 1:
            raise ValueError("missing unique native EFR erasure decoder frame/history replay")
        for frame in range(163):
            if lines.count(f"ROM4 EFR erasure decoder comparison: frame={frame} mismatches=0") != 1:
                raise ValueError("missing unique bit-exact EFR erasure decoder frame comparison")
    if efr_homing_decoder:
        marker, complete = EFR_HOMING_PASS, EFR_HOMING_COMPLETE
        if lines.count(EFR_HOMING_REPLAY) != 1:
            raise ValueError("missing unique native EFR homing frame/history replay")
        for frame in range(46):
            if lines.count(f"ROM4 EFR homing decoder comparison: frame={frame} mismatches=0") != 1:
                raise ValueError("missing unique bit-exact EFR homing decoder frame comparison")
    if efr_homing_encoder:
        marker, complete = EFR_ENCODER_HOMING_PASS, EFR_ENCODER_HOMING_COMPLETE
        if lines.count(EFR_ENCODER_HOMING_REPLAY) != 1:
            raise ValueError("missing unique native EFR homing encoder frame/history replay")
        for frame in range(46):
            if lines.count(f"ROM4 EFR homing encoder comparison: frame={frame} mismatches=0") != 1:
                raise ValueError("missing unique bit-exact EFR homing encoder frame comparison")
    if fr_packing:
        marker, complete = FR_PASS, FR_COMPLETE
    if fr_decoder:
        marker, complete = DECODER_PASS, DECODER_COMPLETE
        if lines.count(DECODER_COMPARISON) != 1:
            raise ValueError("missing unique bit-exact PCM comparison")
        if lines.count(DECODER_REPLAY) != 1:
            raise ValueError("missing unique native decoder frame/history replay")
    if fr_encoder:
        marker, complete = ENCODER_PASS, ENCODER_COMPLETE
        if lines.count(ENCODER_COMPARISON) != 1:
            raise ValueError("missing unique bit-exact parameter comparison")
        if lines.count(ENCODER_REPLAY) != 1:
            raise ValueError("missing unique native encoder frame/history replay")
        if lines.count(PCM_MUTE_PASS) != 1:
            raise ValueError("missing unique native PCM mute buffer-boundary result")
    if returncode != 3:
        raise ValueError(f"unexpected MAME exit {returncode}; expected fixture completion 3")
    if lines.count(marker) != 1 or not lines or lines[-1] != complete:
        raise ValueError("missing unique result or terminal fixture completion")
    if any(line.startswith("Fatal error:") and line != complete for line in lines):
        raise ValueError("unexpected fatal error")
    if "WRONG CHECKSUMS" in text or "NOT FOUND" in text:
        raise ValueError("acquired ROM input failed validation")


def check_rf_bitmap(text,returncode,staged=False):
    marker='TMS320C54x ROM4 RF bitmap: PASS cases=515 basis=512 controls=empty,full,alternating replay=515 ram_words=65440 rf_acquisition_claim=0'
    complete='Fatal error: TMS320C54x ROM4 RF bitmap complete'
    check_rf_helper(text, returncode, marker, complete, 'RF bitmap', staged)


def check_rf_pairs(text, returncode, staged=False):
    marker='TMS320C54x ROM4 RF pairs: PASS cases=32 vectors=8 tc=2 frct=2 replay=32 ram_words=65440 dot_cross_both_tc=1 history_copy_tc_only=1 rf_acquisition_claim=0'
    complete='Fatal error: TMS320C54x ROM4 RF pairs complete'
    check_rf_helper(text, returncode, marker, complete, 'RF pairs', staged)


def check_rf_helper(text, returncode, marker, complete, kind, staged):
    lines=text.splitlines()
    if lines and re.fullmatch(r'Average speed: \d+(?:\.\d+)?% \(\d+ seconds\)',lines[-1]): lines.pop()
    if returncode!=3 or lines[-2:]!=[marker,complete] or lines.count(marker)!=1 or lines.count(complete)!=1:
        raise ValueError(f'incomplete native {kind} basis/replay')
    if staged: check_staged_frontend(text,kind)
    elif f'ROM4 {kind} frontend:' in text:
        raise ValueError('unexpected staged frontend in atomic RF fixture')
    if 'c54x_failure:' in text:
        raise ValueError('native RF fixture assertion failed')
    if any(token in text for token in ('NOT FOUND','WRONG CHECKSUMS','Errors:')) or re.search(r'^Driver .+: [1-9]\d* errors?,',text,re.MULTILINE):
        raise ValueError(f'invalid native {kind} inputs or machine')
    if any(line.startswith('Fatal error:') and line!=complete for line in lines):
        raise ValueError(f'unexpected native {kind} failure')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mame", type=Path, required=True)
    parser.add_argument("--rompath", type=Path, required=True)
    parser.add_argument("--output-root", type=Path,
                        default=Path(__file__).resolve().parents[1] / 'tmp')
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--rf-bitmap',action='store_true',help='execute the original 512-bit search-list decoder and replay controls')
    mode.add_argument('--rf-pairs',action='store_true',help='execute original adjacent-pair arithmetic and conditional history copy')
    mode.add_argument("--packed", action="store_true", help="validate the outer packed-input CRC routine")
    mode.add_argument("--fr-packing", action="store_true", help="validate FR parameter packing, CRC and unpacking")
    mode.add_argument("--fr-decoder", action="store_true", help="compare native FR decoding against independent PCM")
    mode.add_argument("--fr-encoder", action="store_true", help="compare native FR encoding against independent parameters")
    mode.add_argument("--efr-crc", action="store_true", help="compare native EFR CRC against normative polynomial division")
    mode.add_argument("--efr-packing", action="store_true", help="validate native EFR packing, unpacking and CRC rejection")
    mode.add_argument("--efr-decoder", action="store_true", help="compare native EFR decoder with verified ETSI PCM")
    mode.add_argument("--efr-encoder", action="store_true", help="compare native EFR encoder with verified ETSI parameters and VAD/SP")
    mode.add_argument("--efr-dtx-decoder", action="store_true", help="compare native EFR silence/SID/unusable-frame decoding with the external reference")
    mode.add_argument('--efr-invalid-sid', action='store_true', help='compare payload-classified invalid and bad accepted SID frames against the reference')
    mode.add_argument('--efr-lost-sid', action='store_true', help='compare lost-SID muting and first-speech recovery against the reference')
    mode.add_argument('--efr-lost-sid-observe', action='store_true', help='characterize the complete lost-SID conversation without claiming reference conformance')
    mode.add_argument("--efr-dtx-encoder", action="store_true", help="compare native EFR VAD/DTX encoding with the external reference")
    mode.add_argument("--efr-erasure-decoder", action="store_true", help="compare native EFR active-speech erasure concealment with the external reference")
    mode.add_argument("--efr-erasure-observe", action="store_true", help="characterize all native EFR loss frames without claiming reference conformance")
    mode.add_argument("--efr-homing-decoder", action="store_true", help="compare native EFR homing and repeated speech with the external reference")
    mode.add_argument('--efr-homing-errors', action='store_true', help='compare the full native homing/error PCM and reset sequence against the reference')
    mode.add_argument("--efr-homing-encoder", action="store_true", help="compare native EFR encoder homing and repeated speech with the external reference")
    parser.add_argument('--observe-homing-errors', action='store_true', help='capture all homing/error differences without claiming conformance')
    parser.add_argument('--observe-dtx-history', action='store_true', help='observe original DTX first-speech history while retaining strict PCM acceptance')
    parser.add_argument('--observe-lost-sid-state', action='store_true', help='capture lost-SID LSF/gain boundaries without relaxing strict PCM acceptance')
    parser.add_argument('--staged', action='store_true', help='run supported codec or RF helper fixtures on the staged CPU, not a handset speech claim')
    parser.add_argument('--trace-fr', action='store_true', help='retain decoder frame 8 or encoder frame 0 encoding instructions, entry RAM and actual data accesses')
    parser.add_argument('--trace-efr', '--trace-efr-reset', dest='trace_efr_reset', action='store_true', help='observe at most 8192 EFR reset and 200000 first-frame instructions, without changing acceptance')
    parser.add_argument('--trace-efr-loss', action='store_true', help='capture loss-frame-8 entry RAM and bounded execution/data traces without conformance promotion')
    args = parser.parse_args()
    if args.staged and not (args.fr_decoder or args.fr_encoder or args.efr_encoder or args.efr_decoder or
                           args.efr_dtx_encoder or args.efr_dtx_decoder or args.efr_homing_encoder or args.efr_homing_decoder or args.efr_invalid_sid or
                           args.efr_erasure_decoder or args.efr_erasure_observe or args.efr_homing_errors or args.efr_lost_sid or args.efr_lost_sid_observe or args.rf_bitmap or args.rf_pairs):
        parser.error('--staged requires a supported codec or RF helper mode')
    if args.trace_fr and not (args.fr_decoder or args.fr_encoder):
        parser.error('--trace-fr requires --fr-decoder or --fr-encoder')
    if args.trace_efr_reset and not args.efr_encoder:
        parser.error('--trace-efr requires --efr-encoder')
    if args.trace_efr_loss and not args.efr_erasure_observe:
        parser.error('--trace-efr-loss requires --efr-erasure-observe')
    if args.observe_homing_errors and not args.efr_homing_errors:
        parser.error('--observe-homing-errors requires --efr-homing-errors')
    if args.observe_dtx_history and not (args.efr_dtx_decoder or args.efr_invalid_sid or args.efr_lost_sid or args.efr_lost_sid_observe):
        parser.error('--observe-dtx-history requires a DTX decoder fixture')
    if args.observe_lost_sid_state and (not (args.efr_lost_sid or args.efr_lost_sid_observe) or args.observe_dtx_history):
        parser.error('--observe-lost-sid-state requires a lost-SID fixture without --observe-dtx-history')
    args.output_root.mkdir(parents=True, exist_ok=True)
    run = Path(tempfile.mkdtemp(prefix="run_c54x_rom4_crc_", dir=args.output_root))
    bios = "fr-encoder" if args.fr_encoder else "fr-decoder" if args.fr_decoder else "fr-packing" if args.fr_packing else "crc-packed" if args.packed else "crc-helper"
    if args.rf_bitmap: bios='rf-bitmap'
    if args.rf_pairs: bios='rf-pairs'
    if args.efr_crc:
        bios = "efr-crc"
    if args.efr_packing:
        bios = "efr-packing"
    if args.efr_decoder:
        bios = "efr-decoder"
    if args.efr_encoder:
        bios = "efr-encoder"
    if args.efr_dtx_decoder:
        bios = "efr-dtx-decoder"
    if args.efr_invalid_sid:
        bios = 'efr-invalid-sid'
    if args.efr_lost_sid:
        bios = 'efr-lost-sid'
    if args.efr_lost_sid_observe:
        bios = 'efr-lsid-observe'
    if args.efr_dtx_encoder:
        bios = "efr-dtx-encoder"
    if args.efr_erasure_decoder:
        bios = "efr-erasure"
    if args.efr_erasure_observe:
        bios = "efr-loss-observe"
    if args.efr_homing_decoder:
        bios = "efr-homing"
    if args.efr_homing_errors:
        bios = 'efr-home-errors'
    if args.efr_homing_encoder:
        bios = "efr-enc-homing"
    if args.staged:
        bios = {'efr-dtx-encoder': 'efr-dtx-enc-pipe',
                'efr-dtx-decoder': 'efr-dtx-dec-pipe',
                'efr-enc-homing': 'efr-ehome-pipe',
                'efr-invalid-sid': 'efr-isid-pipe',
                'efr-lost-sid': 'efr-lsid-pipe',
                'efr-lsid-observe': 'efr-lso-pipe',
                'efr-home-errors': 'efr-herr-pipe',
                'efr-loss-observe': 'efr-loss-pipe'}.get(bios, bios + '-pipe')
    command = [str(args.mame.resolve()), "tms54rom4", "-bios", bios,
               "-rompath", str(args.rompath.resolve()), "-video", "none",
               "-sound", "none", "-nothrottle", "-noreadconfig", "-log",
               "-seconds_to_run", "30" if args.efr_dtx_decoder or args.efr_dtx_encoder or args.efr_invalid_sid or args.efr_lost_sid or args.efr_lost_sid_observe else "10" if args.efr_decoder or args.efr_encoder or args.efr_erasure_decoder or args.efr_erasure_observe or args.efr_homing_decoder or args.efr_homing_encoder or args.efr_homing_errors else "2"]
    if args.trace_fr or args.trace_efr_reset or args.trace_efr_loss:
        command.append('-verbose')
    root = Path(__file__).resolve().parents[1]
    if args.observe_dtx_history:
        command.extend(['-debug', '-debugger', 'none', '-autoboot_delay', '0',
                        '-autoboot_script', str(root / 'tools/c54x_rom4_efr_dtx_history_observe.lua')])
    if args.observe_lost_sid_state:
        command.extend(['-debug', '-debugger', 'none', '-autoboot_delay', '0',
                        '-autoboot_script', str(root / 'tools/c54x_rom4_efr_lost_sid_state_observe.lua')])
    inputs = [args.mame, Path(__file__), *harness_sources(root)]
    if args.observe_dtx_history:
        inputs.extend(root / name for name in ('tools/c54x_rom4_efr_dtx_history_observe.lua',
                                               'tools/c54x_rom4_efr_dtx_history_check.py'))
    if args.observe_lost_sid_state:
        inputs.extend(root / name for name in ('tools/c54x_rom4_efr_lost_sid_state_observe.lua',
                                               'tools/c54x_rom4_efr_lost_sid_state_check.py'))
    inputs.extend(root / name for name in ("lib/util/gsmfr.cpp", "lib/util/gsmfr.h",
                                           "driver/gsm_tch_f_l1.cpp", "driver/gsm_tch_f_l1.h",
                                           "cpu/tms320c54x/tms320c54x.cpp", "cpu/tms320c54x/tms320c54x.h"))
    inputs.extend(sorted((args.rompath / "tms54rom4").glob("*.bin")))
    if args.efr_decoder:
        for name in ("spch_dos.dec", "spch_dos.out"):
            inputs.append(args.rompath / "tms54rom4" / name)
    if args.efr_encoder:
        for name in ("spch_dos.inp", "spch_dos.cod"):
            inputs.append(args.rompath / "tms54rom4" / name)
    if args.efr_dtx_decoder:
        for name in ("dtx.dec", "dtx.out"):
            inputs.append(args.rompath / "tms54rom4" / name)
    if args.efr_invalid_sid:
        for name in ('invalid_sid.dec', 'invalid_sid.out'):
            inputs.append(args.rompath / 'tms54rom4' / name)
    if args.efr_lost_sid or args.efr_lost_sid_observe:
        for name in ('lost_sid.dec', 'lost_sid.out'):
            inputs.append(args.rompath / 'tms54rom4' / name)
    if args.efr_dtx_encoder:
        for name in ("dtx.inp", "dtx.cod"):
            inputs.append(args.rompath / "tms54rom4" / name)
    if args.efr_erasure_decoder or args.efr_erasure_observe:
        for name in ("erasure.dec", "erasure.out"):
            inputs.append(args.rompath / "tms54rom4" / name)
    if args.efr_homing_decoder:
        for name in ("homing.dec", "homing.out"):
            inputs.append(args.rompath / "tms54rom4" / name)
    if args.efr_homing_errors:
        for name in ('homing_error.dec', 'homing_error.out'):
            inputs.append(args.rompath / 'tms54rom4' / name)
        inputs.append(root / 'tools/c54x_rom4_efr_homing_error_check.py')
    if args.efr_homing_encoder:
        for name in ("homing_enc.inp", "homing_enc.cod"):
            inputs.append(args.rompath / "tms54rom4" / name)
    evidence = {"command": command, "bios": bios,
                "inputs": [file_identity(path) for path in inputs],
                "handset_audio_claim": False}
    if args.efr_erasure_observe or args.efr_lost_sid_observe:
        evidence['comparison_scope'] = 'characterization'
        evidence['frontend'] = 'staged' if args.staged else 'atomic'
        if args.trace_efr_loss:
            evidence['trace_scope'] = 'loss_frame8_entry_and_execution'
    if args.trace_fr:
        evidence['trace_scope'] = 'encoder_frame0_encoding' if args.fr_encoder else 'decoder_frame8'
    if args.efr_homing_errors:
        evidence['comparison_scope'] = 'characterization' if args.observe_homing_errors else 'whole_sequence'
    (run / "evidence.json").write_text(json.dumps(evidence, indent=2) + "\n")
    print(f"ROM4 helper evidence: {run}", flush=True)
    with (run / "console.log").open("w") as output:
        result = subprocess.run(command, cwd=run, stdout=output, stderr=subprocess.STDOUT)
    text = (run / "console.log").read_text()
    print(text, end="")
    try:
        if args.efr_erasure_observe or args.efr_lost_sid_observe:
            if args.staged and not args.efr_lost_sid_observe:
                check_staged_frontend(text, 'EFR erasure decoder')
            try:
                from tools.c54x_rom4_efr_loss_check import characterize
            except ModuleNotFoundError:
                from c54x_rom4_efr_loss_check import characterize
            fixture_name = 'lost_sid' if args.efr_lost_sid_observe else 'erasure'
            report, pcm = characterize(text, result.returncode,
                (args.rompath / f'tms54rom4/{fixture_name}.out').read_bytes(),
                (args.rompath / f'tms54rom4/{fixture_name}.dec').read_bytes(),
                verbose_teardown=args.trace_efr_loss, lost_sid=args.efr_lost_sid_observe, staged=args.staged)
            (run / 'characterization.json').write_text(json.dumps(report, indent=2) + '\n')
            (run / 'native.pcm').write_bytes(pcm)
            label = 'lost SID' if args.efr_lost_sid_observe else 'loss'
            print(f"ROM4 EFR {label} characterized: frames={report['frames']} differing_frames={report['differing_frames']} mismatches={report['mismatches']} conformance_claim=0")
        elif args.efr_homing_errors:
            try:
                from tools.c54x_rom4_efr_homing_error_check import check as check_homing_errors
            except ModuleNotFoundError:
                from c54x_rom4_efr_homing_error_check import check as check_homing_errors
            reference = (args.rompath / 'tms54rom4/homing_error.out').read_bytes()
            report = check_homing_errors(text, result.returncode, reference, staged=args.staged)
            report['strict_reference_required'] = not args.observe_homing_errors
            (run / 'comparison.json').write_text(json.dumps(report, indent=2) + '\n')
            if not args.observe_homing_errors:
                check_homing_errors(text, result.returncode, reference, strict=True, staged=args.staged)
            print(f"ROM4 EFR homing/error characterized: total_mismatches={report['total_mismatches']} reset_mismatch_frames={report['reset_mismatch_frames']} conformance_claim=0")
        elif args.rf_bitmap:
            check_rf_bitmap(text,result.returncode,staged=args.staged)
        elif args.rf_pairs:
            check_rf_pairs(text,result.returncode,staged=args.staged)
        else:
            check_result(text, result.returncode, args.packed, args.fr_packing, args.fr_decoder, args.fr_encoder, args.efr_crc, args.efr_packing, args.efr_decoder, args.efr_encoder, args.efr_dtx_decoder, args.efr_dtx_encoder, args.efr_erasure_decoder, args.efr_homing_decoder, args.efr_homing_encoder, efr_invalid_sid=args.efr_invalid_sid, staged=args.staged, verbose_teardown=args.trace_fr or args.trace_efr_reset, efr_lost_sid=args.efr_lost_sid)
        if args.observe_dtx_history:
            try:
                from tools.c54x_rom4_efr_dtx_history_check import check_trace
            except ModuleNotFoundError:
                from c54x_rom4_efr_dtx_history_check import check_trace
            writes = check_trace((run / 'error.log').read_text(), require_previous_muting=args.efr_lost_sid)
            if writes != 2:
                raise ValueError('expected two original first-speech transitions')
            print(f'ROM4 EFR first-speech history: PASS transitions={writes} previous_muting_required={int(args.efr_lost_sid)}')
    except ValueError as error:
        print(f"ROM4 helper: FAIL: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

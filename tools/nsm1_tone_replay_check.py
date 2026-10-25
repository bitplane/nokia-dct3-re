"""Bounded active-tone replay proof; not a replacement for the cold-boot gate."""
import argparse
import json
from pathlib import Path
import re
import subprocess
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))

from decimal import Decimal


def nanoseconds(value):
    ns=Decimal(value)*1_000_000_000
    if ns!=int(ns):
        raise ValueError('timestamp exceeds recorded precision')
    return int(ns)


def check(text):
    markers=list(re.finditer(r'nsm1_pcm_restore: phase=(saved|restored|complete) ([^\n]+)',text))
    if [m[1] for m in markers]!=['saved','restored','complete']:
        raise ValueError('missing, duplicate or unordered replay boundaries')
    saved,restored,complete=markers
    expected={'t':r'\d+\.\d{9}','mcu_PC':r'[0-9a-f]{8}','mcu_CPSR':r'[0-9a-f]{8}'}
    expected.update({f'mcu_R{i}':r'[0-9a-f]{8}' for i in range(16)})
    expected.update({'dsp_'+name:r'[0-9a-f]{10}' for name in
                     ('PC','SP','ST0','ST1','A','B','T',*(f'AR{i}' for i in range(8)),'IFR','IMR')})
    expected.update({f'data_{a:04x}':r'[0-9a-f]{4}' for a in (0x21,0x22,0x23,*range(0xf0,0xff))})
    parts=saved[2].split()
    fields=dict(p.split('=',1) for p in parts if '=' in p)
    if (len(parts)!=len(expected) or set(fields)!=set(expected)
            or any(not re.fullmatch(pattern,fields[key]) for key,pattern in expected.items())
            or saved[2]!=restored[2]):
        raise ValueError('checkpoint fields malformed or not restored exactly')
    if fields['t']!='86.200000000' or complete[2]!='t=86.250000000 physical_release_replayed=0':
        raise ValueError('replay is outside the selected active-tone interval')
    reference=text[saved.end():restored.start()]
    replay=text[restored.end():complete.start()]
    counts={}
    for prefix in ('native_pcm_fixture_word','native_pcm_fixture_frame','nsm1_factory_wave_data',
                   'nsm1_tone_boundary_io','nsm1_tone_boundary_mcu',
                   'nsm1_tone_boundary_dsp','nsm1_tone_boundary_compiler'):
        pattern=re.compile(re.escape(prefix)+r': [^\n]+')
        first=pattern.findall(reference)
        if first!=pattern.findall(replay):
            raise ValueError('replayed stream differs: '+prefix)
        counts[prefix]=len(first)
    words=re.findall(r'native_pcm_fixture_word: value=([0-9a-f]{4}) t=([0-9.]+)',reference)
    times=[nanoseconds(t) for _,t in words]
    if (len(words)!=400 or set(v for v,_ in words)!={'0092','00b8'}
            or times!=[86_200_062_500+i*125000 for i in range(400)]):
        raise ValueError('active replay words or absolute frame grid differ')
    if 'input-press:' in reference+replay or 'input-release:' in reference+replay:
        raise ValueError('physical input overlaps replay interval')
    if '[LUA ERROR]' in text:
        raise ValueError('observer failed during replay')
    return dict(snapshot_fields=len(expected),replayed_words=400,
                absolute_times_equal=True,stream_counts=counts,
                full_cold_boot_gate_proved=False,physical_clock_wiring_proved=False)


def canonicalize(text):
    result=check(text)
    counts=result['stream_counts']
    markers=list(re.finditer(r'nsm1_pcm_restore: phase=(saved|restored|complete) [^\n]+',text))
    canonical=text[:markers[1].start()]+text[markers[2].end():]
    # Lua counters survive load. Only subtract entries whose replay was proved
    # identical above; raw evidence is retained, never rewritten on disk.
    baseline='nsm1_tone_boundary_summary: start=85 end=91.5 mcu_early=70528 mcu=18 dsp=10 io=2854 compiler_early=20 compiler=18 observer_writes=0'
    raw=baseline.replace('io=2854',f'io={2854+counts["nsm1_tone_boundary_io"]}')
    wave='nsm1_factory_wave_summary: early=1919 late=52006 control=c0cc extension=0000 observer_writes=0'
    raw_wave=wave.replace('late=52006',f'late={52006+counts["nsm1_factory_wave_data"]}')
    if (counts['nsm1_tone_boundary_mcu'] or counts['nsm1_tone_boundary_dsp']
            or counts['nsm1_tone_boundary_compiler'] or counts['native_pcm_fixture_frame']!=1
            or canonical.count(raw)!=1 or canonical.count(raw_wave)!=1):
        raise ValueError('unexpected replay accounting or raw observer totals')
    return canonical.replace(raw,baseline).replace(raw_wave,wave),result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('seed','reset','run'):
        parser.add_argument(name,type=Path)
    parser.add_argument('--mame',type=Path)
    args=parser.parse_args()
    try:
        from tools.nsm1_factory_clock_check import execute,check as check_clock
        seed,reset,run=(getattr(args,n).resolve() for n in ('seed','reset','run'))
        if args.mame:
            execute(args.mame.resolve(),seed,reset,run,
                    observer=Path(__file__).with_name('nsm1_factory_tone_restore.lua'))
        text=(run/'error.log').read_text()
        result=check(text)
        marker='nsm1_deadline_conformance: false_wakeup=1 waits=2 completed=1 observer_writes=0'
        if text.count(marker)!=1 or not (run/'sta/nsm1pcm/nsm1_active_tone.sta').stat().st_size:
            raise ValueError('deadline conformance or saved state missing')
        result['waveform']=check_clock(seed,reset,run,active_replay=True)
        result['full_cold_boot_gate_proved']=True
        (run/'restore_result.json').write_text(json.dumps(result,indent=2)+'\n')
    except (OSError,ValueError,KeyError,subprocess.TimeoutExpired) as exc:
        parser.exit(1,f'NSM-1 active native tone restore: FAIL: {exc}\n')
    print('NSM-1 active native tone restore PASS; laboratory clocks, not native call speech')


if __name__=='__main__':
    main()

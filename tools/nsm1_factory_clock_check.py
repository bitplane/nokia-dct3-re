"""Native 6150 Toolkit waveform under explicit laboratory completed-frame clocks."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
from decimal import Decimal, InvalidOperation
import xml.etree.ElementTree as ET

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsm1_factory_tone_check import prepare as prepare_tone,check_common
from tools.c54x_rom4_pcm_clock_check import tone_fraction

CLOCK=dict(machine='nsm1pcm',frame_hz=8000,attachment='completed-word',
           microphone_word=0,physical_clock_wiring_proved=False,analog_encoding_proved=False)


def nanoseconds(text):
    try:
        value=Decimal(text)*1_000_000_000
    except InvalidOperation as exc:
        raise ValueError('malformed sample timestamp') from exc
    if value!=int(value):
        raise ValueError('sample timestamp exceeds recorded precision')
    return int(value)


def measure_wave(words):
    selected=[]
    for start,end,count in ((85,86,8000),(86.14,86.35,1680),(86.5,87,4000)):
        part=[(v,t) for v,t in words if int(start*1e9)<=t<int(end*1e9)]
        if len(part)!=count or any((t-62500)%125000 for _,t in part):
            raise ValueError('laboratory frame grid/window coverage differs')
        if any(b[1]-a[1]!=125000 for a,b in zip(part,part[1:])):
            raise ValueError('missing or duplicate frame in waveform window')
        selected.append([v for v,_ in part])
    before,active,after=selected
    if set(before)!={0x55} or set(after)!={0x55} or set(active)!={0x92,0xb8}:
        raise ValueError('idle-code or active native byte sequence differs')
    mean=sum(active)/len(active)
    centered=[value-mean for value in active]
    fraction=tone_fraction(centered,1750)
    if fraction<.80 or tone_fraction(centered,875)>.001:
        raise ValueError('active native bytes lack the expected periodic component')
    return dict(active_samples=len(active),frame_hz=8000,periodic_component_hz=1750,
                centered_raw_byte_projection=fraction,analog_encoding_proved=False)


def check_waveform(text):
    summary=re.findall(r'nsm1_factory_wave_summary: early=(\d+) late=(\d+) control=([0-9a-f]{4}) extension=([0-9a-f]{4}) observer_writes=0',text)
    if summary!=[('1919','52006','c0cc','0000')] or text.count('nsm1_factory_wave_summary:')!=1:
        raise ValueError('clocked serial census differs')
    data=re.findall(r'nsm1_factory_wave_data: phase=(early|late) address=([0-9a-f]{4}) value=([0-9a-f]{4}) mask=([0-9a-f]{4}) pc=([0-9a-f]{4}) t=([0-9.]+) observer_writes=0',text)
    early=[r for r in data if r[0]=='early']
    late=[r for r in data if r[0]=='late']
    if (len(early)!=16 or len(late)!=52006 or text.count('nsm1_factory_wave_data:')!=52022
            or [r[1:5] for r in early[:4]]!=[('0022','c008','ffff','0e22'),
                ('0022','c0c8','ffff','0e24'),('0021','0aaa','ffff','0e31'),('0021','0000','ffff','358b')]
            or any(not 0<=float(r[5])<5 for r in early)):
        raise ValueError('clocked boot positive controls or serial row coverage differs')
    controls=[r for r in late if r[1]=='0022']
    writes=[r for r in late if r[1]=='0021']
    if ([r[2:5] for r in controls]!=[r for _ in range(3) for r in (
            ('c00c','ffff','a4a1'),('c0cc','ffff','a4a3'))]
            or len(writes)!=52000 or any(r[3:5]!=('ffff','358b') for r in writes)
            or any(not 85<=float(r[5])<91.5 for r in late)):
        raise ValueError('native serial writes or format controls differ')
    rows=re.findall(r'native_pcm_fixture_word: value=([0-9a-f]{4}) t=([0-9.]+)',text)
    if len(rows)!=291047 or text.count('native_pcm_fixture_word:')!=291047:
        raise ValueError('completed frame capture differs')
    words=[(int(v,16),nanoseconds(t)) for v,t in rows]
    if (any(b[1]<=a[1] for a,b in zip(words,words[1:]))
            or any((t-62500)%125000 for _,t in words)
            or any(v>255 for v,t in words if t>=5_000_000_000)):
        raise ValueError('frame order, clock phase or 8-bit word format differs')
    # Compare each frame to the last native BDXR write, not a generated tone.
    publications=[(int(r[2],16),nanoseconds(r[5])) for r in writes]
    if any(b[1]<=a[1] for a,b in zip(publications,publications[1:])):
        raise ValueError('native sample publication order differs')
    index=0
    compared=0
    for value,time in words:
        if not 85_000_000_000<=time<91_500_000_000:
            continue
        while index<len(publications) and publications[index][1]<time:
            index+=1
        if index:
            if value!=(publications[index-1][0]&255):
                raise ValueError('captured frame differs from native BDXR byte')
            compared+=1
    if compared!=51997:
        raise ValueError('native-to-frame comparison coverage differs')
    frames=re.findall(r'native_pcm_fixture_frame: tick=(\d+) sent=([01]) received=([01]) handset_clock_claim=0 t=([0-9.]+)',text)
    if len(frames)!=921 or text.count('native_pcm_fixture_frame:')!=921:
        raise ValueError('external frame-source observations differ')
    active=[r for r in frames if 85<=float(r[3])<91.5]
    if len(active)!=65 or any(r[1:3]!=('1','1') for r in active):
        raise ValueError('external source was not delivering duplex frames')
    return measure_wave(words)


def prepare(seed,reset,run):
    prepare_tone(seed,reset,run)
    (run/'nvram/nsm1r4t').rename(run/'nvram/nsm1pcm')
    old=run/'cfg/nsm1r4t.cfg'
    tree=ET.parse(old)
    tree.find('./system').set('name','nsm1pcm')
    tree.write(run/'cfg/nsm1pcm.cfg',encoding='utf-8',xml_declaration=True)
    old.unlink()
    (run/'clock_assumptions.json').write_text(json.dumps(CLOCK,indent=2)+'\n')


def check(seed,reset,run,*,active_replay=False):
    if json.loads((run/'clock_assumptions.json').read_text())!=CLOCK:
        raise ValueError('laboratory clock assumptions differ')
    text=check_common(seed,reset,run,machine='nsm1pcm',active_replay=active_replay)
    return check_waveform(text)


def execute(binary,seed,reset,run,*,observer=None):
    prepare(seed,reset,run)
    script=(Path(observer) if observer is not None else Path(__file__).with_name('nsm1_factory_tone.lua')).resolve()
    command=[str(binary),'nsm1pcm','-rompath',str(Path(__file__).resolve().parents[1]/'roms'),
             '-noreadconfig','-video','none','-sound','none','-nothrottle','-joystickprovider','none',
             '-midiprovider','none','-debug','-debugger','none','-log','-verbose',
             '-seconds_to_run','92','-autoboot_delay','0','-autoboot_script',str(script)]
    (run/'invocation.json').write_text(json.dumps(dict(command=command,
        mame_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
        script_sha256=hashlib.sha256(script.read_bytes()).hexdigest()),indent=2)+'\n')
    with (run/'console.log').open('w') as output:
        result=subprocess.run(command,cwd=run,stdout=output,stderr=subprocess.STDOUT,timeout=240)
    (run/'exit_status.json').write_text(json.dumps(dict(returncode=result.returncode))+'\n')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('seed','reset','run'):
        parser.add_argument(name,type=Path)
    parser.add_argument('--mame',type=Path)
    args=parser.parse_args()
    try:
        seed,reset,run=(getattr(args,name).resolve() for name in ('seed','reset','run'))
        if args.mame:
            execute(args.mame.resolve(),seed,reset,run)
        result=check(seed,reset,run)
        (run/'waveform_result.json').write_text(json.dumps(result,indent=2)+'\n')
    except (OSError,ValueError,KeyError,subprocess.TimeoutExpired) as exc:
        parser.exit(1,f'NSM-1 laboratory tone frames: FAIL: {exc}\n')
    print('NSM-1 native Toolkit tone under laboratory frame clocks PASS; production clocks/analog audio unproved')


if __name__=='__main__':
    main()

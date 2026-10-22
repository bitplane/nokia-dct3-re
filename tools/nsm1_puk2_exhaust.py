"""Physical NSM-1 PUK2 exhaustion with complete retained-card cold links."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsm1_puk2_check import recovery
from tools.nsm1_pin2_check import check_change,PREFIX
from tools.nsm1_pin_check import check_authenticated
from tools.nsm1_published_record_check import records
from tools.nsm1_v523_static_check import SHA1
from tools.nsk3_puk2_exhaust import trace_text,compress_trace
from tools.sim_security_trace_check import ATTEMPTS_OFFSET,AOC_NVRAM_SIZE,statuses
from tools.nsk1_phonebook_roundtrip import frame_check

ROOT=Path(__file__).resolve().parents[1]
MACHINE='nsm1r4t'
PIN2_ITEM='e3aa8fdedd0634e762c3b42e469b3b57e50f5870952e37518e9b63df4f1d4926'
BARRING_ITEM='052dcbc2adb5dfd2acfc763260ab9dcb46a44ffc5ff1e57028e6146384008f95'


def stored(run,name):
    return (run/'nvram'/MACHINE/name).read_bytes()


def prepare(previous,run):
    if run.exists():raise ValueError('requires a fresh cold directory')
    shutil.copytree(previous/'nvram',run/'nvram')
    shutil.copytree(previous/'cfg',run/'cfg')
    for name in ('sim_card','eeprom'):
        (run/f'initial_{name}.bin').write_bytes(stored(previous,name))
    (run/'cold_input.json').write_text(json.dumps(dict(source=str(previous.resolve()),
        source_sha256={name:hashlib.sha256(stored(previous,name)).hexdigest()
                       for name in ('flash','eeprom','sim_card')},
        copied_domains=['nvram','cfg'],ram_or_save_state_copied=False),indent=2)+'\n')


def check_link(run,previous):
    metadata=json.loads((run/'cold_input.json').read_text())
    expected={name:hashlib.sha256(stored(previous,name)).hexdigest() for name in ('flash','eeprom','sim_card')}
    if metadata!=dict(source=str(previous.resolve()),source_sha256=expected,
                      copied_domains=['nvram','cfg'],ram_or_save_state_copied=False):
        raise ValueError('cold source identity or copied domains differ')
    for name in ('sim_card','eeprom'):
        if (run/f'initial_{name}.bin').read_bytes()!=stored(previous,name):
            raise ValueError('cold card/EEPROM differs from predecessor')
    check_run(run,require_console=True)


def check_run(run,*,require_console=False):
    if hashlib.sha1(stored(run,'flash')).hexdigest()!=SHA1:
        raise ValueError('own executable flash differs')
    if len(stored(run,'eeprom'))!=0x4000:raise ValueError('own EEPROM extent differs')
    trace=trace_text(run)
    if re.search(r'illegal|unsupported|unhandled|fatal|\[LUA ERROR\]',trace,re.I):
        raise ValueError('execution/observer error')
    records(trace,True)
    if require_console:
        from tools.run_sip_handset_gate import verify_handset_health
        verify_handset_health((run/'console.log').read_text())


def execute(run,script='nsm1_puk2_wrong.lua'):
    command=[sys.executable,str(ROOT/'tools/run_mame_isolated.py'),
        '--mame-dir',str(ROOT/'mame'),'--run-dir',str(run),'--',MACHINE,
        '-rompath','../roms','-video','none','-sound','none','-nothrottle',
        '-seconds_to_run','52','-log','-verbose','-debug','-debugger','none',
        '-autoboot_delay','0','-autoboot_script','../tools/'+script]
    with (run/'console.log').open('w') as console:
        subprocess.run(command,cwd=ROOT,stdout=console,stderr=subprocess.STDOUT,check=True)


def check_permanent(run,previous):
    check_link(run,previous)
    before=stored(previous,'sim_card')
    if before[ATTEMPTS_OFFSET:ATTEMPTS_OFFSET+4]!=bytes((3,0,10,0)) or stored(run,'sim_card')!=before:
        raise ValueError('cold permanent card changed or wrong security domain')
    trace=trace_text(run)
    check_authenticated(trace,PREFIX)
    if any(statuses(trace,ins) for ins in (4,0x24,0x26,0x28,0x2c,0xd6,0xdc)):
        raise ValueError('permanent PIN2 refusal admitted credential/storage mutation')
    edges=re.findall(r'nsm1_puk2_permanent_contact: key=18 down=([01]) t=([0-9.]+) firmware_writes=0',trace)
    if len(edges)!=trace.count('nsm1_puk2_permanent_contact:') or [edge for edge,_ in edges]!=['1','0']:
        raise ValueError('permanent menu physical Down differs')
    if not 34<=float(edges[0][1])<34.1 or not 34.2<=float(edges[1][1])<34.3:
        raise ValueError('permanent menu Down timing differs')
    for second,digest in ((32,PIN2_ITEM),(38,BARRING_ITEM),(47,BARRING_ITEM)):
        frame_check(run/f'snap/nsm1_puk2_permanent_{second}.png',digest)


def chain(pin_runs,wrong,output,*,check_only=False,resume=False):
    if len(pin_runs)!=3:raise ValueError('requires three physical PIN2 rejections')
    previous=None
    for count,run in zip((3,2,1),pin_runs):
        before=(run/'initial_sim_card.bin').read_bytes()
        if len(before)!=AOC_NVRAM_SIZE or before[ATTEMPTS_OFFSET+1]!=count:
            raise ValueError('PIN2 rejection card extent or preceding retry differs')
        if previous is not None and (before!=stored(previous,'sim_card') or
                (run/'initial_eeprom.bin').read_bytes()!=stored(previous,'eeprom')):
            raise ValueError('PIN2 cold predecessor differs')
        check_change(trace_text(run),before,stored(run,'sim_card'),'5675','8765',False)
        check_run(run)
        previous=run
    if (wrong/'initial_sim_card.bin').read_bytes()!=stored(previous,'sim_card') or \
            (wrong/'initial_eeprom.bin').read_bytes()!=stored(previous,'eeprom'):
        raise ValueError('first wrong PUK2 predecessor differs')
    recovery(trace_text(wrong),stored(previous,'sim_card'),stored(wrong,'sim_card'),False)
    check_run(wrong)
    if stored(wrong,'sim_card')[ATTEMPTS_OFFSET:ATTEMPTS_OFFSET+4]!=bytes((3,0,10,9)):
        raise ValueError('first wrong PUK2 state differs')
    if output.exists() and not (check_only or resume):raise ValueError('requires fresh output, --resume or --check-only')
    previous=wrong
    for remaining in range(8,-1,-1):
        run=output/f'remaining-{remaining}'
        if not check_only and not (resume and run.exists()):
            prepare(previous,run)
            execute(run)
        check_link(run,previous)
        recovery(trace_text(run),stored(previous,'sim_card'),stored(run,'sim_card'),False)
        if stored(run,'sim_card')[ATTEMPTS_OFFSET+3]!=remaining:raise ValueError('PUK2 exhaustion skipped a retry')
        if not check_only:compress_trace(run)
        print(f'PASS: NSM-1 physical PUK2 rejection, remaining={remaining}, complete cold card checked',flush=True)
        previous=run
    run=output/'permanent-menu'
    if not check_only and not (resume and run.exists()):
        prepare(previous,run)
        execute(run,'nsm1_puk2_permanent_menu.lua')
    check_permanent(run,previous)
    if not check_only:compress_trace(run)
    print('PASS: NSM-1 permanent PUK2 cold refusal, physical PIN1/menu and unchanged full card',flush=True)
    return run


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('first','second','blocked','wrong','output'):parser.add_argument(name,type=Path)
    parser.add_argument('--check-only',action='store_true')
    parser.add_argument('--resume',action='store_true')
    args=parser.parse_args()
    try:
        chain([args.first.resolve(),args.second.resolve(),args.blocked.resolve()],args.wrong.resolve(),
              args.output.resolve(),check_only=args.check_only,resume=args.resume)
    except (OSError,EOFError,ValueError,RuntimeError,KeyError,subprocess.SubprocessError) as error:
        parser.exit(1,f'NSM-1 PUK2 exhaustion: FAIL: {error}\n')


if __name__=='__main__':main()

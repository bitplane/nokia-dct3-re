"""Qualify the own 6150 physical settings reset and complete saved EEPROM."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsm1_toolkit_check import prepare, check_configuration
from tools.nsm1_native_state_check import check_storage
from tools.nsm1_security_check import check_trace as check_phone
from tools.nsk3_pin_check import check_trace as check_pin
from tools.sim_toolkit_display_check import check_observer_console
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.nsm1_tone_policy_contract import contract

EEPROM = '003f7ddd0966db0a90ae7fb418819ad3bb49e0d0ef272d6868bf3084100355ba'
RECORDS = (
    '0009012f010401010000ffff', '0006042fffff00000000ffff',
    '0006022f00ff00030000ffff', '000a011b020401020000ffff',
    '000a042f010401040000ffff', '0108012f010400010000ffff',
    '0009012f010401010000ffff',
)
FRAMES = {
    'menu': '51a20a16acead610184225c78632724f9f20025565bd29a93beffec0c271d3a3',
    'settings': '7299e6d3447ef1723aa5547cc1cb6a4eac68f7d930e737fb61c9653fbf7800d5',
    'settings_open': '1912cbc31af7c0e79deab7de1a3a6858c8b5500e0ce6e5ccb223915a1030cdf5',
    'restore': '94d874c585c6ff4ac1f6de512a543a8cce0688a0edb6ebd9784ca09ccafa9356',
    'prompt': 'e94f0df7b133ca79ff33a26746025b0885c8190b3d8794d46693736d994785cc',
    'code': '1ce05c3a4a780c6d29f30fc41c1ec9dad06c45e347a9002f508cc1b3f6a1e152',
    'result': 'c8d68f46ef0b142bb04e5f6fa61ae62b26be36df1d8e59b6e6c4c56a0bc08c93',
}


def check_trace(text):
    check_observer_console(text)
    check_pin(text, True, prefix='nsm1', tx_limit=4096)
    check_phone(text, True, (), extra_codes=('313233343500ffff',))
    rows = re.findall(r'nsm1_factory_contact: index=(\d+) key=([0-9a-f]{2}) column=(\d+) mask=([0-9a-f]{2}) down=([01]) t=([0-9.]+) physical_only=1', text)
    contacts = ((0x19,1,2),(4,2,4),(0x19,1,2),(6,4,4),(0x19,1,2),
                (1,2,2),(2,3,2),(3,4,2),(4,2,4),(5,3,4),(0x19,1,2))
    expected = [(str(i), f'{key:02x}', str(column), f'{mask:02x}', str(down))
                for i,(key,column,mask) in enumerate(contacts,1) for down in (1,0)]
    if [r[:5] for r in rows] != expected or text.count('nsm1_factory_contact:') != 22:
        raise ValueError('physical reset contacts differ')
    times = [float(r[5]) for r in rows]
    if times != sorted(times) or not 25 <= times[0] < times[-1] < 40:
        raise ValueError('physical reset contact order/horizon differs')
    calls = re.findall(r'nsm1_factory_initializer: caller=([0-9a-f]+) cycles=([0-9A-Fa-f]{16}) observer_writes=0', text)
    if [c[0] for c in calls] != ['29d457','29dc5f'] or text.count('nsm1_factory_initializer:') != 2:
        raise ValueError('own initializer lifecycle differs')
    if text.rfind('nsm1_security_accepted:') > text.index('nsm1_factory_initializer:'):
        raise ValueError('initializer preceded physical authorization')


def check(seed, run):
    before = check_storage(seed)
    after = check_storage(run, eeprom_sha256=EEPROM)
    contract(after['flash'])
    if after['flash'] != before['flash'] or after['sim_card'] != before['sim_card']:
        raise ValueError('reset altered card or executable')
    if sum(a != b for a,b in zip(before['eeprom'],after['eeprom'])) != 190:
        raise ValueError('complete reset transition differs')
    for i, raw in enumerate(RECORDS):
        if after['eeprom'][0x3f9c+12*i:0x3fa8+12*i] != bytes.fromhex(raw):
            raise ValueError('firmware-generated settings record differs')
    for name in ('eeprom','sim_card'):
        if (run/f'initial_{name}.bin').read_bytes() != before[name]:
            raise ValueError('reset initial input differs')
    expected = dict(source=str(seed.resolve()), source_sha256={
        name: hashlib.sha256(data).hexdigest() for name,data in before.items()},
        copied_directories=['nvram','cfg'], ram_or_save_state_copied=False)
    if json.loads((run/'cold_input.json').read_text()) != expected:
        raise ValueError('reset cold provenance differs')
    check_configuration(seed, run, control=True)
    if json.loads((run/'exit_status.json').read_text()) != {'returncode':0}:
        raise ValueError('reset run did not complete')
    check_observer_console((run/'console.log').read_text())
    check_trace((run/'error.log').read_text())
    for name,digest in FRAMES.items():
        frame_check(run/f'snap/factory_{name}.png',digest)


def execute(binary, seed, run):
    prepare(seed, run, control=True)
    script = Path(__file__).with_name('nsm1_factory_reset.lua').resolve()
    command = [str(binary),'nsm1r4t','-rompath',str(Path(__file__).resolve().parents[1]/'roms'),
               '-noreadconfig','-video','none','-sound','none','-nothrottle',
               '-joystickprovider','none','-midiprovider','none','-debug','-debugger','none',
               '-log','-verbose','-seconds_to_run','92','-autoboot_delay','0','-autoboot_script',str(script)]
    (run/'invocation.json').write_text(json.dumps(dict(command=command,
        mame_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
        script_sha256=hashlib.sha256(script.read_bytes()).hexdigest()),indent=2)+'\n')
    with (run/'console.log').open('w') as output:
        result = subprocess.run(command,cwd=run,stdout=output,stderr=subprocess.STDOUT,timeout=180)
    (run/'exit_status.json').write_text(json.dumps(dict(returncode=result.returncode))+'\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('seed',type=Path); parser.add_argument('run',type=Path)
    parser.add_argument('--mame',type=Path)
    args = parser.parse_args()
    try:
        seed,run = args.seed.resolve(),args.run.resolve()
        if args.mame:
            execute(args.mame.resolve(),seed,run)
        check(seed,run)
    except (OSError,ValueError,KeyError,subprocess.TimeoutExpired) as exc:
        parser.exit(1,f'NSM-1 factory reset: FAIL: {exc}\n')
    print('NSM-1 physical factory reset and exact complete settings bank PASS; cold replay/audio unproved')


if __name__ == '__main__':
    main()

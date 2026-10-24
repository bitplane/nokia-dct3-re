"""Audit intact public NSE-5 backups; optionally observe a fresh native trial.

Whole code/storage inputs remain paired as acquired. A filename or coherent
journal is not proof of original silicon pairing, native RF or a usable UI.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import zipfile

from PIL import Image

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nse5_pmm_journal import replay

IMAGES = {
    '501d-full': ('FuBu7110-v501-PPM-D.fls', 'da259e776feea05d905399a644f3e4f9ed2862f591e8f3cd8dbc8d05884a5deb'),
    '500-full': ('My 7110 v5.00.fls', '4e0660c14e8e335380f963ae200bf2404d50e826b15f50355a68c9da0a78aecd'),
}
ROM_LABELS = {'501d-full': '7110f501d_full.fls', '500-full': '7110f500_full.fls'}
ARCHIVE = '4391f36226314808a6ffce4d38509e2879a51eff0839b75670b168b25e9bda77'
BASELINE = '53af8324919f455ba8199d2c05f7a921cfb811d5'
MASKS = {
    'nse1_rom4_dsp_program.bin': 'a05a1e96a8c36ec5a47e1ea059d15afa54ca5739',
    'nse1_rom4_dsp_data.bin': '024c7f970f4ef754d3e90471de48a167515f930d',
}


def capture_rom_label(bios, metadata):
    # Old retained captures predate normalized labels. Do not rewrite evidence
    # or accept arbitrary paths under the guise of backwards compatibility.
    if 'rom_label' not in metadata:
        return IMAGES[bios][0]
    if metadata['rom_label'] != ROM_LABELS[bios]:
        raise ValueError('capture ROM label differs from the selected BIOS')
    return ROM_LABELS[bios]


def describe(image, baseline):
    if len(image) != 0x400000 or len(baseline) != 0x390000:
        raise ValueError('requires intact4MiB backup and own MCU/PPM baseline')
    sectors = []
    for offset in range(0x390000, 0x400000, 0x2000):
        sector = image[offset:offset+0x2000]
        if sector[6:12] != b'EEPROM':
            continue
        cache, writes, end = replay(sector)
        sectors.append(dict(address=hex(0x200000+offset), writes=len(writes),
                            journal_end=hex(end), cache_sha256=hashlib.sha256(cache).hexdigest()))
    if not sectors:
        raise ValueError('no complete write-journal sector in own product-state area')
    return dict(size=len(image), sha256=hashlib.sha256(image).hexdigest(),
                mcu_prefix_bytes=0x310000,
                matches_acquired_v501_mcu=image[:0x310000] == baseline[:0x310000],
                non_ff_upper_region_bytes=sum(byte != 0xff for byte in image[0x390000:]),
                eeprom_sectors=sectors, silicon_pairing_proved=False,
                native_registration_verified=False)


def audit(directory, roms):
    baseline = (roms / 'noki7110/7110f501_ppmc.fls').read_bytes()
    if hashlib.sha1(baseline).hexdigest() != BASELINE:
        raise ValueError('own existing baseline changed')
    archive = directory / 'My-7110-original-FuBu.zip'
    if hashlib.sha256(archive.read_bytes()).hexdigest() != ARCHIVE:
        raise ValueError('original-backup archive differs')
    with zipfile.ZipFile(archive) as package:
        if package.namelist() != ['My 7110 v5.00.fls']:
            raise ValueError('archive member inventory differs')
        if package.read('My 7110 v5.00.fls') != (directory / IMAGES['500-full'][0]).read_bytes():
            raise ValueError('extracted complete backup differs from archive')
    result = {}
    for bios, (name, digest) in IMAGES.items():
        image = (directory / name).read_bytes()
        if hashlib.sha256(image).hexdigest() != digest:
            raise ValueError('whole backup input differs: ' + name)
        result[bios] = describe(image, baseline)
    return result


def observation(text, returncode, times=(1,2,4,8,16,24,36,48)):
    if returncode or 'LUA ERROR' in text or 'Error in' in text:
        raise ValueError('native trial or observer failed')
    rows = re.findall(r'nse5_full_sample: index=(\d+) t=([0-9.]+) arm=([0-9a-f]{8}) sp=([0-9a-f]{8}) dsp=([0-9a-f]{6}) stopped=([01]) idle=([01]) observer_writes=0', text)
    if (len(rows) != len(times) or text.count('nse5_full_sample:') != len(times)
            or [int(row[0]) for row in rows] != list(range(1, len(times)+1))
            or any(abs(float(row[1])-time) > .1 for row, time in zip(rows, times))):
        raise ValueError('bounded native observations incomplete')
    # A stopped core is a reportable negative boundary, never accepted boot.
    return dict(samples=[dict(time=float(row[1]), arm=row[2], sp=row[3],
                              dsp=row[4], stopped=bool(int(row[5])), idle=bool(int(row[6])))
                         for row in rows], graphical_boot_verified=False,
                native_registration_verified=False)


def check_v501_cache(image, text):
    rows = re.findall(r'nse5_full_cache: index=(\d+) source=([0-9a-f]{8}) bytes=([0-9a-f]+) observer_writes=0', text)
    sectors = re.findall(r'nse5_full_sector: index=(\d+) bytes=([0-9a-f]+) observer_writes=0', text)
    if (not 1 <= len(rows) <= 4 or len(rows) != len(sectors)
            or text.count('nse5_full_cache:') != len(rows)
            or text.count('nse5_full_sector:') != len(sectors)):
        raise ValueError('own journal/cache observations incomplete')
    hashes = []
    for index, (row, sector) in enumerate(zip(rows, sectors), 1):
        if row[:2] != (str(index), '005fc000') or sector[0] != str(index):
            raise ValueError('journal source or snapshot order differs')
        raw = bytes.fromhex(sector[1])
        if len(raw) != 0x2000 or index == 1 and raw != image[0x3fc000:0x3fe000]:
            raise ValueError('first journal is not the intact acquired input')
        cache, _, _ = replay(raw)
        if bytes.fromhex(row[2]) != cache:
            raise ValueError('live cache differs from independent journal replay')
        hashes.append(hashlib.sha256(cache).hexdigest())
    return dict(journal_address='0x5fc000', exact_cache_replays=len(hashes), cache_sha256=hashes)


def check_v501_boundary(text):
    sites = dict(service_init='003bc2a8', identity_check='003af540',
                 identity_invalid='003af64e', context_result='003ae594',
                 subsystem_release='0049fddc', subsystem_skip='0049fe36',
                 selftest_reply='0030e7a6')
    rows = re.findall(r'nse5_full_boundary: name=(\w+) pc=([0-9a-f]{8}) index=(\d+) t=[0-9.]+ observer_writes=0', text)
    counts = Counter()
    for name, pc, index in rows:
        counts[name] += 1
        if sites.get(name) != pc or int(index) != counts[name]:
            raise ValueError('own boundary site or sequence differs')
    if (text.count('nse5_full_boundary:') != len(rows)
            or counts != dict(service_init=2, identity_check=2, context_result=1,
                              subsystem_skip=1, selftest_reply=1)):
        raise ValueError('whole-backup startup boundary changed')
    failed = re.findall(r'nse5_full_failed_item: item=([0-9a-f]{2}) value=([0-9a-f]{2}) t=[0-9.]+ observer_writes=0', text)
    if failed != [('12', '12')]*2 or text.count('nse5_full_failed_item:') != 2:
        raise ValueError('startup failed-item census differs')
    payload = re.findall(r'nse5_full_selftest: index=(\d+) bytes=([0-9a-f]+) observer_writes=0', text)
    if payload != [('1', '00000074000401000d000000')] or text.count('nse5_full_selftest:') != 1:
        raise ValueError('self-test consumer did not receive the actual0d00 reply')
    flags = re.findall(r'nse5_full_flags: index=(\d+) data=([0-9a-f]{2}) pc=([0-9a-f]{8}) t=[0-9.]+ observer_writes=0', text)
    if (len(flags) != 15 or text.count('nse5_full_flags:') != 15
            or [int(row[0]) for row in flags] != list(range(1,16))
            or [(flags[i][1], flags[i][2]) for i in (6,12)] != [('8c','0030e63e')]*2):
        raise ValueError('release-flag write observations differ')
    return dict(identity_check_entries=2, structural_invalid_entries=0,
                selftest_reply_entries=1, failed_item='0x12', failed_item_value='0x12',
                second_task_batch_released=False)


def execute(directory, roms, binary, run, bios, serial_window=False):
    if serial_window and bios != '500-full':
        raise ValueError('serial window requires own v5.00 input')
    admission = audit(directory, roms)
    if run.exists():
        raise ValueError('fresh isolated trial required')
    target = run / 'roms/nse5r4t'
    target.mkdir(parents=True)
    name = IMAGES[bios][0]
    shutil.copyfile(directory / name, target / ROM_LABELS[bios])
    for name, digest in MASKS.items():
        data = (roms / 'noki5110' / name).read_bytes()
        if hashlib.sha1(data).hexdigest() != digest:
            raise ValueError('declared ROM4 compatibility mask changed')
        (target / name).write_bytes(data)
    script_name = ('nse5_v500_serial_window.lua' if serial_window else
                   'nse5_fullbackup_v501_observe.lua' if bios == '501d-full' else
                   'nse5_fullbackup_observe.lua')
    script = Path(__file__).with_name(script_name).resolve()
    command = [str(binary), 'nse5r4t', '-bios', bios, '-rompath', str(run / 'roms'),
               '-noreadconfig', '-video', 'none', '-sound', 'none', '-nothrottle',
               '-log', '-verbose', '-seconds_to_run', '6' if serial_window else '50',
               '-joystickprovider', 'none', '-midiprovider', 'none',
               '-autoboot_delay', '0', '-autoboot_script', str(script)]
    metadata = dict(command=command, input=admission[bios], rom_label=ROM_LABELS[bios],
                    mame_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                    script_sha256=hashlib.sha256(script.read_bytes()).hexdigest(),
                    copied_guest_ram=False, donor_storage=False,
                    fitted_mask_proved=False, saved_state_copied=False, serial_window=serial_window)
    (run / 'input.json').write_text(json.dumps(metadata, indent=2)+'\n')
    with (run / 'console.log').open('w') as output:
        process = subprocess.run(command, cwd=run, stdout=output, stderr=subprocess.STDOUT, timeout=180)
    (run / 'exit_status.json').write_text(json.dumps(dict(returncode=process.returncode))+'\n')
    text = (run / 'error.log').read_text()
    result = observation(text, process.returncode, (1,2,4,5) if serial_window else (1,2,4,8,16,24,36,48))
    if bios == '501d-full':
        result['journal'] = check_v501_cache((directory / IMAGES[bios][0]).read_bytes(), text)
        result['boundary'] = check_v501_boundary(text)
    (run / 'observation.json').write_text(json.dumps(result, indent=2)+'\n')
    return result


def check_run(directory, roms, run, bios, serial_window=False):
    if serial_window and bios != '500-full':
        raise ValueError('serial window requires own v5.00 input')
    admission = audit(directory, roms)
    metadata = json.loads((run / 'input.json').read_text())
    if (metadata.get('input') != admission[bios] or metadata.get('copied_guest_ram') is not False
            or metadata.get('donor_storage') is not False or metadata.get('fitted_mask_proved') is not False
            or metadata.get('saved_state_copied') is not False
            or metadata.get('serial_window', False) != serial_window):
        raise ValueError('whole-backup trial provenance differs')
    command = metadata['command']
    if (command[1] != 'nse5r4t' or command[command.index('-bios')+1] != bios
            or command[command.index('-seconds_to_run')+1] != ('6' if serial_window else '50')):
        raise ValueError('trial selected a different composition or BIOS')
    inputs = run / 'roms/nse5r4t'
    image = (directory / IMAGES[bios][0]).read_bytes()
    if (inputs / capture_rom_label(bios, metadata)).read_bytes() != image:
        raise ValueError('trial did not load the complete acquired image')
    for name, digest in MASKS.items():
        if hashlib.sha1((inputs / name).read_bytes()).hexdigest() != digest:
            raise ValueError('trial resident compatibility input changed')
    text = (run / 'error.log').read_text()
    times = (1,2,4,5) if serial_window else (1,2,4,8,16,24,36,48)
    result = observation(text, json.loads((run / 'exit_status.json').read_text())['returncode'], times)
    if bios == '501d-full':
        result['journal'] = check_v501_cache(image, text)
        result['boundary'] = check_v501_boundary(text)
    result['frames'] = {}
    for time in times:
        with Image.open(run / f'snap/nse5_full_{time:02}.png') as frame:
            if frame.size != (96,65):
                raise ValueError('whole-backup display geometry differs')
            result['frames'][str(time)] = hashlib.sha256(frame.convert('RGB').tobytes()).hexdigest()
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--roms', type=Path, default=Path(__file__).resolve().parents[1] / 'roms')
    parser.add_argument('--mame', type=Path)
    parser.add_argument('--run', type=Path)
    parser.add_argument('--bios', choices=IMAGES)
    parser.add_argument('--serial-window', action='store_true', help='six-second v5.00 passive serial census')
    args = parser.parse_args()
    try:
        result = audit(args.directory, args.roms)
        if any((args.mame, args.run, args.bios, args.serial_window)):
            if not all((args.run, args.bios)):
                raise ValueError('trial requires --run and --bios together')
            if args.mame:
                execute(args.directory.resolve(), args.roms.resolve(), args.mame.resolve(),
                        args.run.resolve(), args.bios, args.serial_window)
            result = check_run(args.directory.resolve(), args.roms.resolve(), args.run.resolve(), args.bios,
                               args.serial_window)
    except (OSError, ValueError, KeyError, zipfile.BadZipFile, subprocess.TimeoutExpired) as error:
        parser.exit(1, f'NSE-5 whole-backup audit FAIL: {error}\n')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()

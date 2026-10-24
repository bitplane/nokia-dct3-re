"""Bounded own-checksum consistency fixture, never an authentic backup repair."""
import argparse
from collections import Counter
import hashlib
import json
import re
from pathlib import Path
import shutil
import subprocess
import sys

from PIL import Image

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nse5_fullbackup_check import (
    IMAGES, MASKS, ROM_LABELS, audit, observation, check_v501_cache, capture_rom_label,
)
from tools.nse5_pmm_journal import replay

SECTOR = 0x3fc000


def derive(image):
    if hashlib.sha256(image).hexdigest() != IMAGES['501d-full'][1]:
        raise ValueError('requires unchanged acquired v5.01 whole backup')
    cache, writes, _ = replay(image[SECTOR:SECTOR+0x2000])
    computed, stored = sum(cache[:0x3c]) & 0xffff, int.from_bytes(cache[0x3c:0x40], 'big')
    if (computed, stored) != (0x18e0, 0x18ed):
        raise ValueError('own measured checksum premise differs')
    result = bytearray(image)
    sources = []
    for address, value in enumerate(computed.to_bytes(4, 'big'), 0x3c):
        owners = [row for row in writes if row['destination'] <= address < row['destination']+row['length']]
        if not owners:
            raise ValueError('checksum byte has no qualified journal writer')
        row = owners[-1]
        source = SECTOR+row['source']+address-row['destination']
        sources.append(source)
        result[source] = value
    changed = [i for i, (before, after) in enumerate(zip(image, result)) if before != after]
    if sources != list(range(0x3fcdb0, 0x3fcdb4)) or changed != [0x3fcdb3]:
        raise ValueError('fixture is not the single qualified checksum-byte change')
    expected = bytearray(cache)
    expected[0x3c:0x40] = computed.to_bytes(4, 'big')
    if replay(result[SECTOR:SECTOR+0x2000])[0] != expected:
        raise ValueError('fixture altered logical content outside the checksum')
    return bytes(result), dict(computed='18e0', acquired_stored='18ed',
        changed_file_offsets=['3fcdb3'], changed_logical_offsets=['3f'],
        original_sha256=hashlib.sha256(image).hexdigest(),
        derived_sha256=hashlib.sha256(result).hexdigest(), authentic_provisioning=False,
        donor_storage=False, firmware_code_changed=False)


def execute(directory, roms, binary, run):
    audit(directory, roms)
    image = (directory / IMAGES['501d-full'][0]).read_bytes()
    fixture, metadata = derive(image)
    if run.exists():
        raise ValueError('fresh isolated output required')
    target = run / 'roms/nse5r4t'
    target.mkdir(parents=True)
    shutil.copyfile(directory / IMAGES['501d-full'][0], target / ROM_LABELS['501d-full'])
    for name in MASKS:
        shutil.copyfile(roms / 'noki5110' / name, target / name)
    storage = run / 'nvram/nse5r4t_1'
    storage.mkdir(parents=True)
    (storage / 'flash').write_bytes(fixture)
    (run / 'initial_flash.bin').write_bytes(fixture)
    script = Path(__file__).with_suffix('.lua').resolve()
    command = [str(binary), 'nse5r4t', '-bios', '501d-full', '-rompath', str(run / 'roms'),
               '-noreadconfig', '-video', 'none', '-sound', 'none', '-nothrottle',
               '-joystickprovider', 'none', '-midiprovider', 'none',
               '-debug', '-debugger', 'none', '-log', '-verbose', '-seconds_to_run', '50',
               '-autoboot_delay', '0', '-autoboot_script', str(script)]
    metadata.update(command=command, rom_label=ROM_LABELS['501d-full'],
                    mame_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
                    script_sha256=hashlib.sha256(script.read_bytes()).hexdigest())
    (run / 'checksum_fixture.json').write_text(json.dumps(metadata, indent=2)+'\n')
    with (run / 'console.log').open('w') as output:
        result = subprocess.run(command, cwd=run, stdout=output,
                                stderr=subprocess.STDOUT, timeout=180)
    (run / 'exit_status.json').write_text(json.dumps(dict(returncode=result.returncode))+'\n')
    return observation((run / 'error.log').read_text(), result.returncode)


def check_trace(text):
    comparisons = re.findall(r'nse5_checksum_compare: computed=([0-9a-f]{8}) stored=([0-9a-f]{8}) observer_writes=0', text)
    results = re.findall(r'nse5_checksum_result: accepted=([0-9a-f]{8}) observer_writes=0', text)
    if (comparisons != [('000018e0', '000018e0')]*2 or results != ['00000001']*2
            or text.count('nse5_checksum_compare:') != 2 or text.count('nse5_checksum_result:') != 2):
        raise ValueError('live own checksum comparison/result differs')
    boundaries = re.findall(r'nse5_full_boundary: name=(\w+) pc=([0-9a-f]{8}) index=(\d+) ', text)
    expected = dict(service_init=('003bc2a8', 2), identity_check=('003af540', 2),
                    identity_invalid=('003af64e', 2), context_result=('003ae594', 1),
                    selftest_reply=('0030e7a6', 1), subsystem_release=('0049fddc', 1))
    counts = Counter()
    for name, pc, index in boundaries:
        counts[name] += 1
        if name not in expected or pc != expected[name][0] or int(index) != counts[name]:
            raise ValueError('own boundary ownership/sequence differs')
    if counts != {name: count for name, (_, count) in expected.items()} or len(boundaries) != text.count('nse5_full_boundary:'):
        raise ValueError('own checksum-fixture lifecycle differs')
    if ('nse5_full_failed_item:' in text or
            text.count('nse5_full_boundary_state: name=subsystem_release index=1 flags=c8 ') != 1):
        raise ValueError('self-test release predicate differs')
    if re.search(r'illegal|unsupported|unhandled|fatal|LUA ERROR|Error in', text, re.I):
        raise ValueError('native execution or observer error')
    return dict(checksum_accepted=True, subsystem_release_observed=True,
                identity_invalid_entries=2, authentic_provisioning=False,
                graphical_boot_verified=False, native_registration_verified=False)


def check(directory, roms, run):
    audit(directory, roms)
    image = (directory / IMAGES['501d-full'][0]).read_bytes()
    fixture, metadata = derive(image)
    actual = json.loads((run / 'checksum_fixture.json').read_text())
    if any(actual.get(key) != value for key, value in metadata.items()):
        raise ValueError('external checksum-fixture provenance differs')
    if (run / 'initial_flash.bin').read_bytes() != fixture:
        raise ValueError('initial flash differs outside the qualified checksum change')
    if (run / 'roms/nse5r4t' / capture_rom_label('501d-full', actual)).read_bytes() != image:
        raise ValueError('acquired executable ROM changed')
    for name, digest in MASKS.items():
        if hashlib.sha1((run / 'roms/nse5r4t' / name).read_bytes()).hexdigest() != digest:
            raise ValueError('resident compatibility input differs')
    command = actual['command']
    if (command[1] != 'nse5r4t' or command[command.index('-bios')+1] != '501d-full'
            or command[command.index('-seconds_to_run')+1] != '50'):
        raise ValueError('wrong BIOS or observation horizon')
    text = (run / 'error.log').read_text()
    status = json.loads((run / 'exit_status.json').read_text())['returncode']
    result = observation(text, status)
    if any(row['stopped'] for row in result['samples']):
        raise ValueError('native core stopped')
    if any(row['dsp'] not in ('00913e', '009140') for row in result['samples'][2:]):
        raise ValueError('late native boundary differs')
    result.update(check_trace(text))
    result['journal'] = check_v501_cache(fixture, text)
    saved = (run / 'nvram/nse5r4t_1/flash').read_bytes()
    saved_hash = hashlib.sha256(saved).hexdigest()
    if (len(saved) != len(image) or saved[:0x310000] != image[:0x310000]
            or saved_hash != '8683161a65476e694a9f55ff4734d676fb58741b5786a95637a5ece92b8f6188'):
        raise ValueError('complete firmware-owned saved flash transition differs')
    result['saved_flash_sha256'] = saved_hash
    with Image.open(run / 'snap/nse5_full_48.png') as frame:
        if frame.size != (96, 65) or frame.convert('RGB').getextrema() != ((255,255),)*3:
            raise ValueError('qualified blank final frame changed')
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('run', type=Path)
    parser.add_argument('--mame', type=Path)
    args = parser.parse_args()
    try:
        directory, run = args.directory.resolve(), args.run.resolve()
        roms = Path(__file__).resolve().parents[1] / 'roms'
        if args.mame:
            execute(directory, roms, args.mame.resolve(), run)
        result = check(directory, roms, run)
    except (OSError, ValueError, subprocess.TimeoutExpired) as error:
        parser.exit(1, f'NSE-5 checksum fixture FAIL: {error}\n')
    print(json.dumps(result, indent=2))

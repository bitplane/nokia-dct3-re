"""Check own NSB-1 MCU upload and erased-storage research boundary, not boot."""

import argparse
import hashlib
from pathlib import Path
import re

FLASH_SHA1 = '27b8f0cb8640e5b423960b206e9aee065f32d726'


def check_trace(trace, program):
    if len(program) != 372:
        raise ValueError('own program must contain exactly 186 words')
    if '[LUA ERROR]' in trace or trace.count('Soft reset') != 1:
        raise ValueError('observer error or unexpected reset')
    uploads = re.findall(r'nsb1_program: words=186 data=([0-9a-f]+) observer_writes=0', trace)
    if len(uploads) != 1 or bytes.fromhex(uploads[0]) != program:
        raise ValueError('installed program differs from own ROM4 catalogue')
    releases = re.findall(r'nsb1_dsp_release: command=01 arguments=([0-9a-f]+) pc=0028f9c8 t=([0-9.]+) observer_writes=0', trace)
    if (len(releases) != 2 or any(arguments != '0100030000008000000100010200' or not 0 < float(time) < 0.1
                                  for arguments, time in releases)
            or float(releases[0][1]) >= float(releases[1][1])):
        raise ValueError('missing own DSP release arguments')
    if not re.search(r'nsb1_dsp_result_read: address=00010004 data=ffff0000 mask=ffff0000 pc=0028f9dc t=', trace):
        raise ValueError('missing firmware-owned initial version read')
    samples = re.findall(
        r'nsb1_boot: t=([0-9.]+) pc=([0-9a-f]+) sp=([0-9a-f]+) irqmask=([0-9a-f]+) '
        r'eeprom_mode=01 page=20 capacity=00002000 selector=00 dsp_family=0004 '
        r'catalogue0=002b35a4/002b94e4 shared_writes=(\d+) observer_writes=0', trace)
    if len(samples) != 5:
        raise ValueError('missing own EEPROM/catalogue/family observations')
    for sample, expected in zip(samples, (0.1, 0.5, 1, 2, 5)):
        time, pc, sp, control, writes = sample
        if abs(float(time) - expected) > 0.02 or not 0x100000 <= int(sp, 16) < 0x110000:
            raise ValueError('unexpected observation ordering or stack extent')
        if expected >= 0.5 and (int(pc, 16) not in (0x2a4672, 0x2a4674, 0x2a4676, 0x2a4678) or int(writes) != 246):
            raise ValueError('MCU no longer reproduces the measured supervisor park')
    clears = re.findall(r'nsb1_supervisor_flag: value=00 pc=002acaca t=([0-9.]+) observer_writes=0', trace)
    if len(clears) < 3 or any(float(a) >= float(b) for a, b in zip(clears, clears[1:])):
        raise ValueError('missing repeated organic FIQ supervisor wakeups')
    interrupts = re.findall(r'nsb1_interrupts: t=([0-9.]+) cpsr=([0-9a-f]+) ctrl=([0-9a-f]+)', trace)
    if len(interrupts) != 5:
        raise ValueError('missing actual global interrupt-control observations')
    for time, cpsr, control in interrupts[1:]:
        if int(control, 16) != 5 or int(cpsr, 16) & 0xc0:
            raise ValueError('global interrupt delivery disabled after startup')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('flash', type=Path)
    parser.add_argument('run', type=Path)
    args = parser.parse_args()
    try:
        image = args.flash.read_bytes()
        if hashlib.sha1(image).hexdigest() != FLASH_SHA1:
            raise ValueError('requires own normalized NSB-1 v6.71 A')
        check_trace((args.run / 'error.log').read_text(), image[0xbe570:0xbe570 + 372])
        storage = args.run / 'nvram/noki5190'
        if (storage / 'flash').read_bytes() != image:
            raise ValueError('source executable flash changed')
        if len((storage / 'eeprom').read_bytes()) != 8192:
            raise ValueError('persisted EEPROM capacity differs from own descriptor')
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSB-1 MCU boundary failed: {error}\n')
    print('NSB-1 MCU boundary PASS: own 186-word upload, EEPROM geometry and supervisor park; no phone-boot claim')


if __name__ == '__main__':
    main()

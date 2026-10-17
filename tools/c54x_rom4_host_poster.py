#!/usr/bin/env python3
"""Execute the NSE-1 field poster in isolation; not a handset/hardware model."""
import argparse
import hashlib
import json
from pathlib import Path

ENTRY = 0x271d70
RETURN = 0x400000
LOCK_CALLS = (0x254780, 0x254848)


def check_case(case):
    """Check recovered software relationships, not DSP electrical behavior."""
    field, payload, notify = case['field'], case['payload'], case['notify']
    if case.get('busy', 0) or field > 0x2e:
        if case['result'] != 0 or case['writes']:
            raise ValueError('busy or invalid field must reject without external stores')
        return
    if case['result'] != 1:
        raise ValueError('poster did not report success')
    writes = [(item['address'], item['size'], item['value']) for item in case['writes']]
    notification = [(0x100e0, 2, 1), (0x30000, 2, 4), (0x20008, 1, 2)]
    expected = notification if notify or field == 0x2e else []
    actual = [item for item in writes if item[0] in (0x100e0, 0x30000, 0x20008)]
    if actual != expected or (expected and writes[-3:] != expected):
        raise ValueError('poster notification ordering or ownership differs')
    parameters = writes[:-3] if expected else writes
    if any(address < 0x100a0 or address >= 0x100e0 or size != 2
           for address, size, value in parameters):
        raise ValueError('unclassified external parameter write')
    if field <= 0xf and parameters != [(0x100a8, 2, (field << 12) | (payload & 0xfff))]:
        raise ValueError('packed field selector/payload differs')
    if field == 0x1b and parameters != [(0x100aa, 2, payload)]:
        raise ValueError('field 1b shared-word store differs')
    if field == 0x2e and parameters:
        raise ValueError('trigger-only field unexpectedly stores a parameter')


def execute(image, field, payload, notify, retained=0xa55a, busy=0):
    """Original BE Thumb routine, supplied RAM inputs, only lock calls stubbed."""
    from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_BIG_ENDIAN
    from unicorn import UC_HOOK_CODE, UC_HOOK_MEM_WRITE
    from unicorn.arm_const import (UC_ARM_REG_R0, UC_ARM_REG_R1,
                                   UC_ARM_REG_R2, UC_ARM_REG_SP,
                                   UC_ARM_REG_LR, UC_ARM_REG_PC)
    if not 0 <= field <= 0xffff or not 0 <= payload <= 0xffff or notify not in (0, 1) or not 0 <= retained <= 0xffff or not 0 <= busy <= 0xffff:
        raise ValueError('field, payload or notification outside audited input domain')
    if len(image) != 0x100000:
        raise ValueError('expected original one-megabyte NSE-1 MCU image')
    cpu = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_BIG_ENDIAN)
    cpu.mem_map(0x200000, 0x200000)
    cpu.mem_write(0x200000, image)
    cpu.mem_map(0x10000, 0x30000)
    cpu.mem_map(0x100000, 0x100000)
    for address in range(0x100a0, 0x100e0, 2):
        cpu.mem_write(address, retained.to_bytes(2, 'big'))
    cpu.mem_write(0x100e0, busy.to_bytes(2, 'big'))
    writes, calls = [], []
    steps = 0

    def instruction(machine, address, size, _):
        nonlocal steps
        steps += 1
        if address in LOCK_CALLS:
            calls.append(address)
            machine.reg_write(UC_ARM_REG_PC, machine.reg_read(UC_ARM_REG_LR))

    def store(machine, access, address, size, value, _):
        if address < 0x100000:
            writes.append(dict(pc=machine.reg_read(UC_ARM_REG_PC),
                               address=address, size=size, value=value))

    cpu.hook_add(UC_HOOK_CODE, instruction)
    cpu.hook_add(UC_HOOK_MEM_WRITE, store)
    cpu.reg_write(UC_ARM_REG_SP, 0x180000)
    cpu.reg_write(UC_ARM_REG_LR, RETURN | 1)
    cpu.reg_write(UC_ARM_REG_R0, field)
    cpu.reg_write(UC_ARM_REG_R1, payload)
    cpu.reg_write(UC_ARM_REG_R2, notify)
    cpu.emu_start(ENTRY | 1, RETURN, count=256)
    if cpu.reg_read(UC_ARM_REG_PC) != RETURN or calls != list(LOCK_CALLS):
        raise ValueError('poster failed bounded return or lock-call pairing')
    return dict(field=field, payload=payload, notify=notify, retained=retained, busy=busy,
                steps=steps, result=cpu.reg_read(UC_ARM_REG_R0), writes=writes)


def audit(image):
    # Reject another firmware before executing address-specific instructions.
    from tools.c54x_rom4_codec_contract import check_mcu_speech_field
    check_mcu_speech_field(image)
    fields = [*range(47), 0x2f, 0xffff]
    cases = [execute(image, field, payload, notify, retained, busy)
             for field in fields for payload in (0, 1, 0x9209, 0xffff)
             for notify in (0, 1) for retained in (0, 0xa55a)
             for busy in (0, 1, 0xffff)]
    for case in cases:
        check_case(case)
    import unicorn
    return dict(schema=2, image_sha256=hashlib.sha256(image).hexdigest(),
                engine='Unicorn', engine_version=unicorn.__version__,
                scope='isolated original MCU poster; lock calls stubbed; no hardware semantics inferred',
                fields=47, payloads=[0, 1, 0x9209, 0xffff], retained=[0, 0xa55a],
                rejected_fields=[0x2f, 0xffff], busy_values=[0, 1, 0xffff],
                cases=cases, hardware_request_latch_claim=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    report = audit(args.image.read_bytes())
    args.output.write_text(json.dumps(report, indent=2) + '\n')
    print(f"NSE-1 isolated field poster: fields={report['fields']} cases={len(report['cases'])} hardware_request_latch_claim=0")


if __name__ == '__main__':
    main()

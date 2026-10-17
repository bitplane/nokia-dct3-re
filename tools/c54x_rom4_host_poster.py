#!/usr/bin/env python3
"""Execute the NSE-1 field poster in isolation; not a handset/hardware model."""
import argparse
import hashlib
import json
from pathlib import Path

ENTRY = 0x271d70
RETURN = 0x400000
LOCK_CALLS = (0x254780, 0x254848)
SHADOWS = (0x10a9e6, 0x10a9e8)
SHADOW_PATTERNS = ((0xffff, 0xffff), (0xa55a, 0x5aa5),
                   (0x1234, 0xabcd), (0x8426, 0x9209))


def parameter_contract(field, payload, retained, shadows=(0, 0)):
    """Independent expression of the original selector/mask/store branches.

    The two private shadows are not the retained HPI words. Register names
    and electrical request sources are deliberately not inferred here.
    """
    if not 0 <= field <= 0x2e:
        raise ValueError('field outside the decoded poster contract')
    if field <= 0xf:
        word = (field << 12) | (payload & 0xfff)
        private = [(SHADOWS[field - 8], word)] if field in (8, 9) else []
        return [(0x100a8, word)], private
    if 0x10 <= field <= 0x16:
        mask = (0x400, 0x200, 0x20, 0x10, 8, 4, 1)[field - 0x10]
        word = (shadows[0] & ~mask) | ((payload & 1) * mask)
        return [(0x100a8, word)], [(SHADOWS[0], word)]
    if 0x17 <= field <= 0x1a:
        shift, mask = ((10, 3), (8, 3), (4, 15), (0, 15))[field - 0x17]
        word = (shadows[1] & ~(mask << shift)) | ((payload & mask) << shift)
        return [(0x100a8, word)], [(SHADOWS[1], word)]
    direct = {0x1b: 0x100aa, 0x24: 0x100b6, 0x25: 0x100ac,
              0x28: 0x100bc, 0x29: 0x100b8, 0x2d: 0x100be}
    if field in direct:
        return [(direct[field], payload)], []
    if 0x1c <= field <= 0x1f:
        shift, mask, keep = ((5, 7, 0x1f), (0, 1, 0xfe),
                             (1, 1, 0xfd), (2, 7, 0xe3))[field - 0x1c]
        return [(0x100ac, (retained & keep) | ((payload & mask) << shift))], []
    if 0x20 <= field <= 0x23:
        return [(0x100ae + (field - 0x20) * 2, payload & 0x3fff)], []
    if field == 0x26:
        return [(0x100be, (retained & 0xff) | ((payload & 0xff) << 8))], []
    if field == 0x27:
        return [(0x100be, (retained & 0xff00) | (payload & 0xff))], []
    if field in (0x2a, 0x2b, 0x2c):
        shift, mask = {0x2a: (15, 1), 0x2b: (14, 1), 0x2c: (0, 15)}[field]
        return [(0x100ba, (retained & ~(mask << shift)) | ((payload & mask) << shift))], []
    if field == 0x2e:
        return [], []
    raise ValueError('field outside the decoded poster contract')


def check_case(case):
    """Check recovered software relationships, not DSP electrical behavior."""
    field, payload, notify = case['field'], case['payload'], case['notify']
    if case.get('busy', 0) or field > 0x2e:
        if case['result'] != 0 or case['writes'] or case.get('shadow_writes', []):
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
    expected_parameters, expected_shadow = parameter_contract(
        field, payload, case.get('retained', 0), case.get('shadows', (0, 0)))
    if [(address, value) for address, size, value in parameters] != expected_parameters:
        raise ValueError('decoded parameter transformation differs')
    if case.get('shadow_writes', []) != [dict(address=address, size=2, value=value)
                                         for address, value in expected_shadow]:
        raise ValueError('private shadow ownership or preservation differs')


def execute(image, field, payload, notify, retained=0xa55a, busy=0, shadows=(0, 0)):
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
    if len(shadows) != 2 or any(not 0 <= value <= 0xffff for value in shadows):
        raise ValueError('private shadows require two halfwords')
    cpu = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_BIG_ENDIAN)
    cpu.mem_map(0x200000, 0x200000)
    cpu.mem_write(0x200000, image)
    cpu.mem_map(0x10000, 0x30000)
    cpu.mem_map(0x100000, 0x100000)
    for address in range(0x100a0, 0x100e0, 2):
        cpu.mem_write(address, retained.to_bytes(2, 'big'))
    cpu.mem_write(0x100e0, busy.to_bytes(2, 'big'))
    for address, value in zip(SHADOWS, shadows):
        cpu.mem_write(address, value.to_bytes(2, 'big'))
    writes, calls, shadow_writes = [], [], []
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
        elif 0x10a9e4 <= address < 0x10a9ec:
            shadow_writes.append(dict(address=address, size=size, value=value))

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
                shadows=list(shadows), shadow_writes=shadow_writes,
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
    shadow_cases = [execute(image, field, payload, notify, retained, shadows=shadows)
                    for field in range(0x10, 0x1b)
                    for payload in (0, 0xffff, *(1 << bit for bit in range(16)))
                    for notify in (0, 1) for retained in (0, 0xa55a)
                    for shadows in SHADOW_PATTERNS]
    cases.extend(shadow_cases)
    for case in cases:
        check_case(case)
    import unicorn
    return dict(schema=3, image_sha256=hashlib.sha256(image).hexdigest(),
                engine='Unicorn', engine_version=unicorn.__version__,
                scope='isolated original MCU poster; lock calls stubbed; no hardware semantics inferred',
                fields=47, payloads=[0, 1, 0x9209, 0xffff], retained=[0, 0xa55a],
                rejected_fields=[0x2f, 0xffff], busy_values=[0, 1, 0xffff],
                shadow_cases=len(shadow_cases), shadow_addresses=list(SHADOWS),
                shadow_patterns=[list(pair) for pair in SHADOW_PATTERNS],
                shadow_payloads=[0, 0xffff, *(1 << bit for bit in range(16))],
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

"""Execute NPE-3's type-88 field decoder; not a live RF response fixture."""

from unicorn import Uc, UC_ARCH_ARM, UC_HOOK_CODE, UC_MODE_BIG_ENDIAN, UC_MODE_THUMB
from unicorn.arm_const import (
    UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_R0, UC_ARM_REG_R1,
    UC_ARM_REG_R2, UC_ARM_REG_R4, UC_ARM_REG_R5, UC_ARM_REG_SP,
)

from tools.noki6210_radio_contract import verify


def check_fields(image):
    """Compare real Thumb execution with the recovered wire arithmetic."""
    verify(image)
    cpu = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_BIG_ENDIAN)
    cpu.mem_map(0x200000, (len(image) + 0xfff) & ~0xfff)
    cpu.mem_write(0x200000, image)
    cpu.mem_map(0x100000, 0x80000)
    obj, output, stop = 0x101000, 0x102000, 0x200000
    cases, requests = 0, 0
    for frame in (0, 49, 50, 51, 65535, 0xffffff):
        for phase in (0, 1, 4999, 5000, 5001, 9999, 10000, 65535):
            # Four-byte MCU object prefix precedes the RX payload.
            payload = bytes([0]) + frame.to_bytes(3, 'big')
            payload += (35).to_bytes(2, 'big') + phase.to_bytes(2, 'big')
            # Public vocabulary lists ten bytes; this decoder uses the first
            # eight. Nonzero trailing bytes avoid implying a complete encoder.
            payload += bytes([0x5a, 0xc3])
            cpu.mem_write(obj, bytes(4) + payload)
            cpu.mem_write(output, bytes([0xa5]) * 4)
            cpu.reg_write(UC_ARM_REG_SP, 0x10f000)
            cpu.reg_write(UC_ARM_REG_R0, obj)
            cpu.reg_write(UC_ARM_REG_R1, output)
            cpu.reg_write(UC_ARM_REG_R2, output + 2)
            cpu.reg_write(UC_ARM_REG_LR, stop | 1)
            cpu.emu_start(0x38af67, stop, count=1000)
            if cpu.reg_read(UC_ARM_REG_PC) != stop:
                raise ValueError('type-88 field decoder did not return')
            actual = bytes(cpu.mem_read(output, 4))
            expected = bytes([(frame + phase // 5000 + 52) % 51, 0xa5])
            expected += (phase % 5000).to_bytes(2, 'big')
            if actual != expected:
                raise ValueError(
                    f'type-88 fields differ: frame={frame} phase={phase} '
                    f'actual={actual.hex()} expected={expected.hex()}')
            cases += 1
            for flags, arfcn in ((0, 35), (0x20, 512)):
                args, request = 0x104000, 0x105000
                fields = bytearray(12)
                fields[0] = 2
                fields[2:4] = expected[2:4]
                fields[4:6] = arfcn.to_bytes(2, 'big')
                fields[6:8] = bytes([0, expected[0]])
                fields[8] = flags
                cpu.mem_write(args, bytes(fields))
                cpu.mem_write(request, bytes(16))
                cpu.reg_write(UC_ARM_REG_R4, request)
                cpu.reg_write(UC_ARM_REG_R5, args)
                # Execute only the constructor's field-copy block, after
                # allocation and before logging/posting; no scheduler stub.
                cpu.emu_start(0x3f2515, 0x3f2588, count=200)
                if cpu.reg_read(UC_ARM_REG_PC) != 0x3f2588:
                    raise ValueError('mode-2 constructor did not reach post boundary')
                wire = bytes.fromhex('00020c110200') + expected[2:4]
                wire += bytes([0, expected[0]]) + arfcn.to_bytes(2, 'big')
                wire += bytes([0, 6 if flags else 1, 0, 0])
                actual = bytes(cpu.mem_read(request, 16))
                if actual != wire:
                    raise ValueError(
                        f'mode-2 field copy differs: {actual.hex()} != {wire.hex()}')
                requests += 1
    transitions = 0
    record = 0x103000
    cpu.mem_write(0x1711d4, record.to_bytes(4, 'big'))
    for status in (0, 1, 255):
        for retries in (0, 254, 255):
            payload = bytes([status]) + (50).to_bytes(3, 'big')
            payload += (35).to_bytes(2, 'big') + (5001).to_bytes(2, 'big')
            payload += bytes([0x5a, 0xc3])
            cpu.mem_write(obj, bytes(4) + payload)
            initial = bytearray([0xa5] * 16)
            initial[11], initial[13] = retries, 0xa2
            cpu.mem_write(record, bytes(initial))
            cpu.reg_write(UC_ARM_REG_SP, 0x10f000)
            cpu.reg_write(UC_ARM_REG_R0, obj)
            cpu.reg_write(UC_ARM_REG_LR, stop | 1)
            cpu.emu_start(0x38afa9, stop, count=1000)
            if cpu.reg_read(UC_ARM_REG_PC) != stop:
                raise ValueError('type-88 record updater did not return')
            expected = initial.copy()
            if status == 0:
                expected[4:6] = (1).to_bytes(2, 'big')
                expected[8], expected[11], expected[13] = 1, 0, 0xa3
            else:
                expected[11] = (retries + 1) & 0xff
                if expected[11]:
                    expected[13] = 0xa1
            actual = bytes(cpu.mem_read(record, 16))
            if actual != expected:
                raise ValueError(
                    f'type-88 record transition differs: status={status} '
                    f'retries={retries} actual={actual.hex()} '
                    f'expected={expected.hex()}')
            transitions += 1
    selected = []
    targets = {0x38ae5c, 0x38ae22, 0x3f2500}
    cpu.mem_write(0x1711d8, record.to_bytes(4, 'big'))

    def stop_at_mode(_cpu, address, _size, _data):
        if address in targets:
            selected.append(address)
            _cpu.emu_stop()

    hook = cpu.hook_add(UC_HOOK_CODE, stop_at_mode)
    try:
        for nibble in range(16):
            selected.clear()
            initial = bytearray(16)
            initial[6:8] = (35).to_bytes(2, 'big')
            initial[13] = 0xa0 | nibble
            cpu.mem_write(record, bytes(initial))
            cpu.reg_write(UC_ARM_REG_SP, 0x10f000)
            cpu.reg_write(UC_ARM_REG_R0, 1)
            cpu.reg_write(UC_ARM_REG_LR, stop | 1)
            cpu.emu_start(0x38ae9b, stop, count=200)
            expected_target = {2: 0x3f2500, 3: 0x38ae5c, 4: 0x38ae22}.get(nibble)
            if selected != ([] if expected_target is None else [expected_target]):
                raise ValueError(f'record nibble {nibble} selected {selected}')
            if expected_target is None and cpu.reg_read(UC_ARM_REG_PC) != stop:
                raise ValueError('unselected record did not return')
            if nibble == 2:
                args = cpu.reg_read(UC_ARM_REG_R0)
                if bytes(cpu.mem_read(args, 1)) != bytes([4]):
                    raise ValueError('nibble-2 direct constructor did not select mode 4')
    finally:
        cpu.hook_del(hook)
    return {'executed_cases': cases, 'record_transition_cases': transitions,
            'request_copy_cases': requests,
            'record_mode_cases': 16,
            'entry': '38af66',
            'phase_modulus': 5000, 'frame_modulus': 51,
            'decoded_prefix_octets': 8,
            'scope': 'isolated MCU decode, not physical DSP encoding proof'}

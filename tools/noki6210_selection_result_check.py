"""Execute NPE-3's type-88 field decoder; not a live RF response fixture."""

from unicorn import Uc, UC_ARCH_ARM, UC_MODE_BIG_ENDIAN, UC_MODE_THUMB
from unicorn.arm_const import (
    UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_R0, UC_ARM_REG_R1,
    UC_ARM_REG_R2, UC_ARM_REG_SP,
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
    cases = 0
    for frame in (0, 49, 50, 51, 65535, 0xffffff):
        for phase in (0, 1, 4999, 5000, 5001, 9999, 10000, 65535):
            # Four-byte MCU object prefix precedes the eight-byte RX payload.
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
    return {'executed_cases': cases, 'record_transition_cases': transitions,
            'entry': '38af66',
            'phase_modulus': 5000, 'frame_modulus': 51,
            'decoded_prefix_octets': 8,
            'scope': 'isolated MCU decode, not physical DSP encoding proof'}

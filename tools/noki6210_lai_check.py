"""Execute the own NPE-3 mandatory LAI parser, not a handset fixture."""

from unicorn import Uc, UC_ARCH_ARM, UC_MODE_BIG_ENDIAN, UC_MODE_THUMB
from unicorn.arm_const import UC_ARM_REG_LR, UC_ARM_REG_PC, UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R2, UC_ARM_REG_SP

from tools.noki6210_radio_contract import verify


def check_lai(image):
    verify(image)
    cpu = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_BIG_ENDIAN)
    cpu.mem_map(0x200000, (len(image) + 0xfff) & ~0xfff)
    cpu.mem_write(0x200000, image)
    cpu.mem_map(0x100000, 0x80000)
    source, stop = 0x101000, 0x200000
    pool = 0x2a0b74 + 4 + 0x15c
    owner = int.from_bytes(image[pool - 0x200000:pool - 0x200000 + 4], 'big')
    cases = 0
    for nibble in range(16):
        for length in range(7):
            # Sweep the encoded field in isolated instruction execution,
            # never ADC/card inputs in a running phone.
            cpu.mem_write(source, bytes([0x13, nibble << 4, 0x62, 0, 1]))
            cpu.mem_write(owner + 0xe, b'\xa5')
            cpu.reg_write(UC_ARM_REG_SP, 0x10f000)
            cpu.reg_write(UC_ARM_REG_R0, 0x13)
            cpu.reg_write(UC_ARM_REG_R1, source)
            cpu.reg_write(UC_ARM_REG_R2, length)
            cpu.reg_write(UC_ARM_REG_LR, stop | 1)
            cpu.emu_start(0x2a0b71, stop, count=10000)
            accepted = length >= 5 and nibble == 15
            if cpu.reg_read(UC_ARM_REG_PC) != stop:
                raise ValueError('mandatory LAI parser did not return')
            consumed = cpu.reg_read(UC_ARM_REG_R0)
            status = bytes(cpu.mem_read(owner + 0xe, 1))[0]
            if consumed != (5 if accepted else 0) or status != (1 if accepted else 3):
                raise ValueError(f'LAI parser differs for nibble={nibble:x} length={length}')
            cases += 1
    return {'cases': cases, 'accepted_mnc_filler': 15, 'minimum_length': 5}

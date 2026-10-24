import hashlib
import unittest
from unittest.mock import patch

from tools import nsm4_boot_check as subject


class Nsm4BootTest(unittest.TestCase):
    def test_exact_samples_prefix_and_tail_are_required(self):
        source = bytes(0x380000)
        flash = bytearray(source + b'\xff' * 0x80000)
        flash[0x390006:0x390009] = b'PMM'
        flash[0x3e0006:0x3e000c] = b'EEPROM'
        trace = '\n'.join(f'nsm4_boot: second={n} pc=0033e2e0 lr=0033e2d9 '
                          'catalogue0=003fae04 catalogue27=004019d8 read_callback=003e0499 '
                          'program_callback=0015a6b1 dsp_control=01 shared0=0000 shared2=ffff observer_writes=0'
                          for n in range(1, 6))
        with patch.object(subject, 'inspect'), patch.object(subject, 'FLASH_SHA256', hashlib.sha256(flash).hexdigest()):
            subject.check(source, trace, flash)
            for wrong in (trace.replace('shared2=ffff', 'shared2=0001'), trace + '\n' + trace,
                          trace.replace('second=5', 'second=6')):
                with self.assertRaises(ValueError):
                    subject.check(source, wrong, flash)
            changed = bytearray(flash)
            changed[0] ^= 1
            with self.assertRaises(ValueError):
                subject.check(source, trace, changed)
            changed = bytearray(flash)
            changed[-1] ^= 1
            with self.assertRaises(ValueError):
                subject.check(source, trace, changed)


if __name__ == '__main__':
    unittest.main()

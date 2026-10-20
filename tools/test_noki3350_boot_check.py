import unittest

from tools.noki3350_boot_check import ARGUMENTS, check


PROGRAM = b'\x12\x34' * 247


def capture():
    lines = [
        'nhm9_boot_initial_storage: first=00530000 bytes=851968 non_erased_words=0 observer_writes=0',
        f'nhm9_boot_program: words={PROGRAM.hex()} observer_writes=0',
        f'nhm9_boot_release: arguments={ARGUMENTS} pc=0031dc3a t=0.127802846 observer_writes=0',
        'nhm9_boot_catalogue: address=00120f8c entries=28 observer_writes=0',
        'nhm9_boot_catalogue_summary: matches=1 aligned_ram_bytes=262144 observer_writes=0',
        'nhm9_boot_flash_summary: writes=200 observer_writes=0',
    ]
    for time in (0.1, 0.5, 1, 2, 5, 10):
        lines.append(f'nhm9_boot_sample: t={time:.9f} pc=0031dc84 sp=0012bca8 '
                     'shared=0000ffff0000000000ea00ea000000f0 writes=543 ctrl=05 dsp_reset=01 observer_writes=0')
    return '\n'.join(lines)


class Nhm9BootCheckTest(unittest.TestCase):
    def test_complete_capture(self):
        check(capture(), PROGRAM)

    def test_old_223_word_program_rejected(self):
        with self.assertRaisesRegex(ValueError, '247-word'):
            check(capture(), PROGRAM[:446])

    def test_installed_program_differs(self):
        with self.assertRaisesRegex(ValueError, 'installed'):
            check(capture().replace(PROGRAM.hex(), '0000' * 247), PROGRAM)

    def test_sibling_geometry_rejected(self):
        with self.assertRaisesRegex(ValueError, 'arguments'):
            check(capture().replace(ARGUMENTS, '010003000000e800000100010200'), PROGRAM)

    def test_non_erased_storage_rejected(self):
        with self.assertRaisesRegex(ValueError, 'not erased'):
            check(capture().replace('non_erased_words=0', 'non_erased_words=1'), PROGRAM)

    def test_missing_late_sample_rejected(self):
        with self.assertRaisesRegex(ValueError, 'interval'):
            check('\n'.join(line for line in capture().splitlines() if 't=10.' not in line), PROGRAM)

    def test_changed_wait_rejected(self):
        with self.assertRaisesRegex(ValueError, 'ownership wait'):
            check(capture().replace('shared=0000ffff', 'shared=0001ffff'), PROGRAM)

    def test_observer_failure_rejected(self):
        with self.assertRaisesRegex(ValueError, 'observer'):
            check(capture() + '\n[LUA ERROR]', PROGRAM)


if __name__ == '__main__':
    unittest.main()

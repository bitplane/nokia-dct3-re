"""Native research evidence defaults to disk, independent of system TMPDIR."""
from pathlib import Path
import unittest
from unittest.mock import patch

from tools import run_c54x_rom4_crc, run_c54x_rom4_rf_delivery, run_c54x_rom4_rf_impulse


class ScratchTests(unittest.TestCase):
    def test_default_and_explicit_output_roots(self):
        modules = (run_c54x_rom4_crc, run_c54x_rom4_rf_delivery, run_c54x_rom4_rf_impulse)
        default = Path(__file__).resolve().parents[1] / 'tmp'
        for module in modules:
            for explicit in (False, True):
                expected = Path('custom-evidence') if explicit else default
                argv = ['runner', '--mame', 'unused', '--rompath', 'unused']
                if explicit:
                    argv.extend(['--output-root', str(expected)])
                with self.subTest(module=module.__name__, explicit=explicit), \
                        patch('sys.argv', argv), patch.object(Path, 'mkdir') as mkdir, \
                        patch.object(module.tempfile, 'mkdtemp',
                                     side_effect=RuntimeError('allocation reached')) as allocate:
                    with self.assertRaisesRegex(RuntimeError, 'allocation reached'):
                        module.main()
                    mkdir.assert_called_once_with(parents=True, exist_ok=True)
                    self.assertEqual(allocate.call_args.kwargs['dir'], expected)


if __name__ == '__main__':
    unittest.main()

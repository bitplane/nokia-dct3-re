import hashlib
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools import run_rom6_native as runner


class NativeRom6PreparationTest(unittest.TestCase):
    def test_lab_and_native_use_run_local_alias_without_copying_roms(self):
        mcu,pmm=b'mcu',b'pmm'
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            pack=root/'roms/nsm3r6'
            pack.mkdir(parents=True)
            (pack/'program').write_bytes(b'acquired')
            for machine in ('nsm3r6','nsm3afs'):
                run=root/machine
                with patch.object(runner,'ROOT',root), \
                     patch.object(runner,'MCU_SHA1',hashlib.sha1(mcu).hexdigest()), \
                     patch.object(runner,'PMM_SHA256',hashlib.sha256(pmm).hexdigest()), \
                     patch.object(runner,'base_record_fixture',return_value=b'base'):
                    runner.prepare(run,mcu,pmm,machine=machine)
                alias=run/'roms'/machine
                self.assertTrue(alias.is_symlink())
                self.assertEqual(alias.resolve(),pack)
                self.assertEqual((alias/'program').read_bytes(),b'acquired')
                self.assertEqual((run/'nvram'/machine/'flash').read_bytes(),b'mcubase')

    def test_invalid_inputs_do_not_create_storage_or_rom_alias(self):
        with tempfile.TemporaryDirectory() as directory:
            run=Path(directory)/'run'
            with self.assertRaises(ValueError):
                runner.prepare(run,b'not the ROM',b'not the PMM')
            self.assertFalse(run.exists())

import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from tools.nsb1_rom4_compare import check_cold_link


class Nsb1ColdLinkTests(unittest.TestCase):
    def test_actual_executable_and_saved_input_hashes_are_required(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            run, source = root / 'run', root / 'source'
            storage = source / 'nvram/nsb1r4t'
            storage.mkdir(parents=True)
            for name in ('flash', 'eeprom', 'sim_card'):
                (storage / name).write_bytes(name.encode())
            inputs = (('fixture.bin', hashlib.sha1(b'own').hexdigest()),)
            executable = run / 'inputs/nsb1r4t/fixture.bin'
            executable.parent.mkdir(parents=True)
            executable.write_bytes(b'own')
            manifest = {'inputs': [{'source': name, 'sha1': digest} for name, digest in inputs],
                        'synthetic_sim_inserted': True, 'synthetic_retry_initialized': True,
                        'external_eeprom': 'diagnostic checksum/retry control; calibration/identity not validated',
                        'cold_storage_sha256': {name: hashlib.sha256((storage / name).read_bytes()).hexdigest()
                                               for name in ('flash', 'eeprom', 'sim_card')}}
            (run / 'input_manifest.json').write_text(json.dumps(manifest))
            check_cold_link(run, source, inputs)
            executable.write_bytes(b'different')
            with self.assertRaisesRegex(ValueError, 'input changed'):
                check_cold_link(run, source, inputs)
            executable.write_bytes(b'own')
            (storage / 'sim_card').write_bytes(b'changed')
            with self.assertRaisesRegex(ValueError, 'hashes differ'):
                check_cold_link(run, source, inputs)


if __name__ == '__main__':
    unittest.main()

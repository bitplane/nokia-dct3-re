import gzip
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.nsk3_puk2_exhaust import (check_link, check_permanent, compress_trace,
                                     trace_text)
from tools.sim_security_trace_check import ATTEMPTS_OFFSET, AOC_NVRAM_SIZE


class PermanentPuk2Test(unittest.TestCase):
    def test_cold_provenance_and_initial_bytes_are_required(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            previous, run = root/'previous', root/'run'
            for directory in (previous, run):
                storage = directory/'nvram/nsk3r4t'
                storage.mkdir(parents=True)
                for name, value in (('flash', b'own'), ('eeprom', b'settings'), ('sim_card', b'card')):
                    (storage/name).write_bytes(value)
            (run/'initial_eeprom.bin').write_bytes(b'settings')
            (run/'initial_sim_card.bin').write_bytes(b'card')
            (run/'error.log').write_text('native upload')
            metadata = dict(source=str(previous.resolve()), copied_directories=['nvram', 'cfg'],
                ram_or_save_state_copied=False, source_sha256={name: hashlib.sha256(value).hexdigest()
                    for name, value in (('flash', b'own'), ('eeprom', b'settings'), ('sim_card', b'card'))})
            record = run/'cold_input.json'
            record.write_text(json.dumps(metadata))
            with patch('tools.nsk3_puk2_exhaust.check_upload_handoffs') as uploads:
                check_link(run, previous, b'own')
                uploads.assert_called_once_with('native upload', 1)
                for field, value in (('source', str(run)), ('ram_or_save_state_copied', True),
                                     ('fdn_fixture', True), ('source_sha256', {})):
                    record.write_text(json.dumps(dict(metadata, **{field: value})))
                    with self.subTest(field=field), self.assertRaises(ValueError):
                        check_link(run, previous, b'own')
                record.write_text(json.dumps(metadata))
                (run/'initial_eeprom.bin').write_bytes(b'changed')
                with self.assertRaises(ValueError):
                    check_link(run, previous, b'own')

    def test_lossless_trace_archive_and_conflicting_archive(self):
        with tempfile.TemporaryDirectory() as temporary:
            run = Path(temporary)
            raw = run/'error.log'
            source = 'native trace\n'*100
            raw.write_text(source)
            compress_trace(run)
            self.assertFalse(raw.exists())
            self.assertEqual(trace_text(run), source)
            compress_trace(run)
            raw.write_text('different evidence')
            with self.assertRaises(ValueError):
                compress_trace(run)
            self.assertTrue(raw.exists())
            self.assertEqual(gzip.decompress((run/'error.log.gz').read_bytes()).decode(), source)

    def permanent(self, trace, mutate=False, navigate=True):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            previous, run = root/'previous', root/'run'
            card = bytearray(AOC_NVRAM_SIZE)
            card[ATTEMPTS_OFFSET:ATTEMPTS_OFFSET+4] = bytes((3, 0, 10, 0))
            for directory in (previous, run):
                storage = directory/'nvram/nsk3r4t'
                storage.mkdir(parents=True)
                (storage/'sim_card').write_bytes(card)
            if mutate:
                card[0] ^= 1
                (run/'nvram/nsk3r4t/sim_card').write_bytes(card)
            (run/'error.log').write_text(trace)
            with patch('tools.nsk3_puk2_exhaust.check_link'), \
                 patch('tools.nsk3_puk2_exhaust.check_phone'), \
                 patch('tools.nsk3_puk2_exhaust.check_pin') as pin, \
                 patch('tools.nsk3_puk2_exhaust.frame_check'):
                check_permanent(run, previous, b'own', navigate)
                pin.assert_called_once_with(trace, True)

    def test_permanent_block_preserves_pin1_and_forbids_mutations(self):
        trace = 'SIM status ins=20 sw=9000 chv=3/0 puk=10/0 enabled=1\n'
        self.permanent(trace, navigate=False)
        with self.assertRaises(ValueError):
            self.permanent(trace, mutate=True, navigate=False)
        for changed in (trace.replace('sw=9000', 'sw=9804'), trace+'SIM status ins=2c sw=9840 chv=3/0 puk=10/0 enabled=1\n'):
            with self.assertRaises(ValueError):
                self.permanent(changed, navigate=False)

    def test_physical_navigation_requires_decode_between_edges(self):
        trace = ('SIM status ins=20 sw=9000 chv=3/0 puk=10/0 enabled=1\n'
                 'nsk3_puk2_permanent_contact: key=18 down=1 t=34.000000 firmware_writes=0\n'
                 'nsk3_logical_key: site=002ac2fc key=18 cycles=100 observer_writes=0\n'
                 'nsk3_puk2_permanent_contact: key=18 down=0 t=34.216667 firmware_writes=0\n')
        self.permanent(trace)
        for changed in (trace.replace('key=18 cycles', 'key=17 cycles'),
                        trace.replace('down=0', 'down=1'), trace.replace('34.216667', '35.000000')):
            with self.assertRaises(ValueError):
                self.permanent(changed)


if __name__ == '__main__':
    unittest.main()

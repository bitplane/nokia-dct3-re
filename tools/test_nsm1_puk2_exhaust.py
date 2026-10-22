import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.nsm1_puk2_exhaust import prepare,check_link,check_permanent,chain,check_run,SHA1
from tools.sim_security_trace_check import ATTEMPTS_OFFSET,AOC_NVRAM_SIZE


class ColdPuk2Test(unittest.TestCase):
    def test_new_cold_runs_reject_console_lua_errors(self):
        from types import SimpleNamespace
        with patch('tools.nsm1_puk2_exhaust.stored',side_effect=[b'flash',bytes(0x4000)]), \
             patch('tools.nsm1_puk2_exhaust.hashlib.sha1',return_value=SimpleNamespace(hexdigest=lambda:SHA1)), \
             patch('tools.nsm1_puk2_exhaust.trace_text',return_value='clean trace'), \
             patch('tools.nsm1_puk2_exhaust.records'), \
             patch.object(Path,'read_text',return_value='[LUA ERROR] passive fixture failed'):
            with self.assertRaises(RuntimeError):check_run(Path('run'),require_console=True)

    def test_permanent_menu_requires_full_card_and_timed_physical_input(self):
        card=bytearray(AOC_NVRAM_SIZE)
        card[ATTEMPTS_OFFSET:ATTEMPTS_OFFSET+4]=bytes((3,0,10,0))
        trace=('nsm1_puk2_permanent_contact: key=18 down=1 t=34.000000 firmware_writes=0\n'
               'nsm1_puk2_permanent_contact: key=18 down=0 t=34.200000 firmware_writes=0')
        with patch('tools.nsm1_puk2_exhaust.check_link'), \
             patch('tools.nsm1_puk2_exhaust.stored',return_value=bytes(card)) as stored, \
             patch('tools.nsm1_puk2_exhaust.trace_text',return_value=trace) as text, \
             patch('tools.nsm1_puk2_exhaust.check_authenticated'), \
             patch('tools.nsm1_puk2_exhaust.frame_check') as frames:
            check_permanent(Path('run'),Path('previous'))
            self.assertEqual(frames.call_count,3)
            for bad in (trace.replace('34.200000','34.100000'),trace.replace('key=18','key=19'),
                        trace+'\nSIM status ins=2c sw=9840 chv=3/3 puk=10/10 enabled=1'):
                text.return_value=bad
                with self.assertRaises(ValueError):check_permanent(Path('run'),Path('previous'))
            text.return_value=trace
            changed=bytearray(card);changed[-1]^=1
            stored.side_effect=[bytes(card),bytes(changed)]
            with self.assertRaises(ValueError):check_permanent(Path('run'),Path('previous'))

    def test_cold_input_domains_hashes_and_bytes_are_required(self):
        with patch.object(Path,'read_bytes',return_value=b''):
            with self.assertRaisesRegex(ValueError,'extent'):
                chain([Path('a'),Path('b'),Path('c')],Path('wrong'),Path('output'),check_only=True)
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            previous=root/'previous'
            storage=previous/'nvram/nsm1r4t'
            storage.mkdir(parents=True)
            for name,data in (('flash',b'own image'),('sim_card',b'card'),('eeprom',b'eeprom')):
                (storage/name).write_bytes(data)
            (previous/'cfg').mkdir()
            (previous/'cfg/nsm1r4t.cfg').write_text('own config')
            (previous/'sta').mkdir()
            (previous/'sta/forbidden.sta').write_bytes(b'not a cold input')
            run=root/'cold'
            prepare(previous,run)
            self.assertFalse((run/'sta').exists())
            with patch('tools.nsm1_puk2_exhaust.check_run'):
                check_link(run,previous)
                metadata=json.loads((run/'cold_input.json').read_text())
                for key,value in (('source','other'),('ram_or_save_state_copied',True),
                                  ('source_sha256',{}),('copied_domains',['nvram','cfg','sta'])):
                    bad={**metadata,key:value}
                    (run/'cold_input.json').write_text(json.dumps(bad))
                    with self.assertRaises(ValueError):check_link(run,previous)
                (run/'cold_input.json').write_text(json.dumps(metadata))
                (run/'initial_sim_card.bin').write_bytes(b'other card')
                with self.assertRaises(ValueError):check_link(run,previous)
            with self.assertRaises(ValueError):prepare(previous,run)


if __name__=='__main__':unittest.main()

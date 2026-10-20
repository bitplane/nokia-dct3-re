from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.make_sim_card_profile import CHV1,CHV2,PUK1,PUK2
from tools.nsk1_puk_roundtrip import check_run,NEW_PIN
from tools.sim_security_trace_check import CHV_OFFSET,ATTEMPTS_OFFSET,ENABLED_OFFSET,AOC_NVRAM_SIZE


class Nsk1PukTest(unittest.TestCase):
    def test_exact_recovery_and_cold_storage_contract(self):
        blocked = bytearray(AOC_NVRAM_SIZE)
        blocked[CHV_OFFSET:CHV_OFFSET+32] = CHV1+CHV2+PUK1+PUK2
        blocked[ATTEMPTS_OFFSET:ATTEMPTS_OFFSET+4] = bytes((0,3,10,10))
        blocked[ENABLED_OFFSET] = 1
        recovered = bytearray(blocked)
        recovered[CHV_OFFSET:CHV_OFFSET+8] = NEW_PIN
        recovered[ATTEMPTS_OFFSET] = 3
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            storage = run/'nvram/nsk1r4t'
            storage.mkdir(parents=True)
            (run/'initial_eeprom.bin').write_bytes(bytes(2048))
            path = storage/'sim_card'
            path.write_bytes(recovered)
            with patch('tools.nsk1_puk_roundtrip.check_contacts'),patch('tools.nsk1_puk_roundtrip.frame_check'):
                for phase,ins,previous in (('unblock','2c',blocked),('cold','20',recovered)):
                    trace = f'SIM status ins={ins} sw=9000 chv=3/3 puk=10/10 enabled=1'
                    with patch('tools.nsk1_puk_roundtrip.check_phase',return_value=({},trace)):
                        self.assertEqual(phase,check_run(run,phase,previous)[0]['physical_sim_security_phase'])
                        with self.assertRaises(ValueError):
                            check_run(run,phase)
                        changed = bytearray(recovered)
                        changed[0] ^= 1
                        path.write_bytes(changed)
                        with self.assertRaises(ValueError):
                            check_run(run,phase,previous)
                        path.write_bytes(recovered)
                    for invalid in (trace.replace('sw=9000','sw=9804'),trace+'\nSIM status ins=26 sw=9000 chv=3/3 puk=10/10 enabled=0'):
                        with patch('tools.nsk1_puk_roundtrip.check_phase',return_value=({},invalid)):
                            with self.assertRaises(ValueError):
                                check_run(run,phase,previous)
                path.write_bytes(blocked)
                trace = '\n'.join(f'SIM status ins=20 sw={sw} chv={retry}/3 puk=10/10 enabled=1'
                                  for sw,retry in (('9804',2),('9804',1),('9840',0)))
                with patch('tools.nsk1_puk_roundtrip.check_phase',return_value=({},trace)):
                    self.assertEqual('block',check_run(run,'block')[0]['physical_sim_security_phase'])
                with patch('tools.nsk1_puk_roundtrip.check_phase',return_value=({},trace.replace('chv=1/3','chv=2/3'))):
                    with self.assertRaises(ValueError):
                        check_run(run,'block')

    def test_failed_puk_changes_only_one_retry_and_recovery_restores_it(self):
        blocked = bytearray(AOC_NVRAM_SIZE)
        blocked[CHV_OFFSET:CHV_OFFSET+32] = CHV1+CHV2+PUK1+PUK2
        blocked[ATTEMPTS_OFFSET:ATTEMPTS_OFFSET+4] = bytes((0,3,10,10))
        blocked[ENABLED_OFFSET] = 1
        failed = bytearray(blocked)
        failed[ATTEMPTS_OFFSET+2] = 9
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            path = run/'nvram/nsk1r4t/sim_card'
            path.parent.mkdir(parents=True)
            path.write_bytes(failed)
            (run/'initial_eeprom.bin').write_bytes(bytes(2048))
            trace = 'SIM status ins=2c sw=9804 chv=0/3 puk=9/10 enabled=1'
            with patch('tools.nsk1_puk_roundtrip.check_contacts'),patch('tools.nsk1_puk_roundtrip.frame_check'):
                with patch('tools.nsk1_puk_roundtrip.check_phase',return_value=({},trace)):
                    self.assertEqual('wrong',check_run(run,'wrong',blocked)[0]['physical_sim_security_phase'])
                    for invalid in (blocked[:-1],failed):
                        with self.assertRaises(ValueError):
                            check_run(run,'wrong',invalid)
                    mutated = bytearray(failed)
                    mutated[CHV_OFFSET] ^= 1
                    path.write_bytes(mutated)
                    with self.assertRaises(ValueError):
                        check_run(run,'wrong',blocked)
                recovered = bytearray(failed)
                recovered[CHV_OFFSET:CHV_OFFSET+8] = NEW_PIN
                recovered[ATTEMPTS_OFFSET:ATTEMPTS_OFFSET+4] = bytes((3,3,10,10))
                path.write_bytes(recovered)
                with patch('tools.nsk1_puk_roundtrip.check_phase',return_value=({},'SIM status ins=2c sw=9000 chv=3/3 puk=10/10 enabled=1')):
                    self.assertEqual('unblock',check_run(run,'unblock',failed)[0]['physical_sim_security_phase'])


if __name__ == '__main__':
    unittest.main()

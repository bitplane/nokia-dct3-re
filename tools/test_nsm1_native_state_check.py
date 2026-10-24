import hashlib
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools import nsm1_native_state_check as checker
from tools.nse2_native_state_check import check_state
from tools.test_nse2_native_state_check import trace as architecture
from tools import test_nsm1_pin_check as pin_fixture
from tools import test_nsm1_published_record_check as record_fixture


def trace():
    phone = '\n'.join(f'nsm1_security_contact: key={key:02x} down={down} t=15 firmware_writes=0'
                      for key in (1, 2, 3, 4, 5, 0x19) for down in (1, 0))
    phone += '\nnsm1_security_submit: text=3132333435000000 caller=0029faf3'
    phone += '\nnsm1_security_gate: match=00000001 retry_pointer=00112824 retry=00'
    phone += '\nnsm1_security_accepted: result=00000001'
    state = architecture().replace('nse2_', 'nsm1_').replace(
        '18.500000000', '40.000000000').replace('0010d584', '0011d584')
    return '\n'.join((record_fixture.RecordLifecycleTest().fixture(True),
                      pin_fixture.PinTest().pin_trace(), phone, state))


class Nsm1NativeStateTest(unittest.TestCase):
    def test_explicit_backed_ram_bound_preserves_default_and_checks_stack(self):
        text = architecture().replace('0010d584', '0011d584')
        card, pixels = bytes(3570), b'Messages'
        options = dict(menu_frame=hashlib.sha256(pixels).hexdigest())
        with self.assertRaises(ValueError):
            check_state(text, card, card, pixels, pixels, **options)
        check_state(text, card, card, pixels, pixels, ram_limit=0x120000, **options)
        for limit in (0x100000, 0x120001, 0x180004):
            with self.subTest(limit=limit), self.assertRaises(ValueError):
                check_state(text, card, card, pixels, pixels, ram_limit=limit, **options)
        with self.assertRaises(ValueError):
            check_state(text.replace('0011d584', '00120000'), card, card,
                        pixels, pixels, ram_limit=0x120000, **options)

    def test_own_authentication_architecture_and_physical_replay(self):
        card, pixels = bytes(3570), b'Messages'
        with patch.object(checker, 'MENU', hashlib.sha256(pixels).hexdigest()):
            for prefix in ('', '[luaengine] '):
                text = '\n'.join(prefix + line for line in trace().splitlines())
                result = checker.check_trace(text, card, card, pixels, pixels)
                self.assertEqual(result['io_replay_events'], 8180)
                self.assertFalse(result['native_registration_verified'])
            for bad in (trace().replace('ILLEGAL:0', 'ILLEGAL:1'),
                        trace().replace('restored=8180', 'restored=8179'),
                        trace().replace('retry=00', 'retry=2b'),
                        trace().replace('r5=00000001', 'r5=00000000'),
                        trace().replace('ins=20 sw=9000', 'ins=20 sw=9804'),
                        trace() + '\nillegal opcode',
                        trace() + '\nnsm1_restart_request: reason=00000068 caller=002c2a02',
                        trace() + '\nnsm1_state: phase=saved malformed'):
                with self.subTest(tail=bad[-100:]), self.assertRaises(ValueError):
                    checker.check_trace(bad, card, card, pixels, pixels)
            with self.assertRaises(ValueError):
                checker.check_trace(trace(), card, card, pixels, b'blank')

    def test_full_persistent_inputs_not_ranges_and_no_ram_copy(self):
        with tempfile.TemporaryDirectory() as temporary:
            seed, run = (Path(temporary) / name for name in ('seed', 'run'))
            storage = seed / 'nvram/nsm1r4t'
            storage.mkdir(parents=True)
            (seed / 'cfg').mkdir()
            (seed / 'sta').mkdir()
            (seed / 'sta/old.sta').write_bytes(b'not a cold input')
            files = dict(flash=b'own image', eeprom=bytes(0x4000),
                         sim_card=checker.make_current_profile(True))
            for name, data in files.items():
                (storage / name).write_bytes(data)
            (seed / 'cfg/nsm1r4t.cfg').write_text(
                '<mameconfig><system name="nsm1r4t"><input><port tag=":A" value="0"/></input></system></mameconfig>')
            with patch.object(checker, 'SHA1', hashlib.sha1(files['flash']).hexdigest()), \
                    patch.object(checker, 'SEED_EEPROM', hashlib.sha256(files['eeprom']).hexdigest()):
                checker.prepare(seed, run)
                self.assertFalse((run / 'sta').exists())
                for name, data in files.items():
                    self.assertEqual((run / 'nvram/nsm1r4t' / name).read_bytes(), data)
                    damaged = bytes((data[0] ^ 1,)) + data[1:]
                    (storage / name).write_bytes(damaged)
                    with self.subTest(name=name), self.assertRaises(ValueError):
                        checker.check_storage(seed)
                    (storage / name).write_bytes(data)
                with self.assertRaises(ValueError):
                    checker.prepare(seed, run)


if __name__ == '__main__':
    unittest.main()

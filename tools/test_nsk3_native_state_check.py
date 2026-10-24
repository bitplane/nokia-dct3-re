import hashlib
import unittest
from unittest.mock import patch

from tools import nsk3_native_state_check as checker
from tools.test_nse2_native_state_check import trace as state_trace
from tools import test_nsk3_pin_check as pin_fixture


def trace():
    lines = [pin_fixture.PinCheckTest().trace(True)]
    lines += [f'nsk3_security_contact: key={key:02x} down=1 t=15'
              for key in (1, 2, 3, 4, 5, 0x19)]
    lines += [
        'nsk3_security_submit: pointer=10c000 text=3132333435000000 caller=0028a1d7',
        'nsk3_security_gate: match=00000001 retry_pointer=0010c520 retry=00',
        'nsk3_security_accepted: result=00000001',
    ]
    for i in range(1, 65):
        odd = i % 2 == 1
        pc = '0028cef6' if i == 64 else '0028cea8' if odd else '0028ceb8'
        lines.append(f"nsk3_loader_handoff: index={i} offset={'000100fc' if odd else '00010100'} value=00000000 mask={'0000ffff' if odd else 'ffff0000'} pc={pc} t={i/1000:.6f} observer_writes=0")
    for pc, offset, value, mask, time in (
            ('0028ce70', '0010b840', '00040000', 'ffff0000', 0),
            ('0028cf0a', '0010b844', '00000004', '0000ffff', .08),
            ('0028cf0e', '0010b844', '00000000', 'ffff0000', .081)):
        lines.append(f'nsk3_loader_state: offset={offset} value={value} mask={mask} pc={pc} t={time:.6f} observer_writes=0')
    lines.append(state_trace().replace('nse2_', 'nsk3_').replace(
        '18.500000000', '40.000000000'))
    return '\n'.join(lines)


class Nsk3NativeStateTest(unittest.TestCase):
    def setUp(self):
        self.card = bytes(3570)
        self.pixels = b'own Messages pixels'
        self.frames = patch.object(checker, 'FRAMES', (hashlib.sha256(self.pixels).hexdigest(),))
        self.frames.start()
        self.addCleanup(self.frames.stop)

    def check(self, text):
        return checker.check_trace(text, self.card, self.card, self.pixels, self.pixels)

    def test_product_restore_and_structured_register_names(self):
        text = trace()
        for prefix in ('', '[luaengine] '):
            result = self.check('\n'.join(prefix + line for line in text.splitlines()))
            self.assertEqual(result['io_replay_events'], 8180)
            self.assertFalse(result['native_registration_verified'])

    def test_architecture_replay_input_and_pixels_remain_strict(self):
        text = trace()
        for bad in (text.replace('ILLEGAL:0', 'ILLEGAL:1'),
                    text.replace('restored=8180', 'restored=8179'),
                    text.replace('40.000000000', '18.500000000'),
                    text.replace('nsk3_state: completed=1', ''),
                    text.replace('phase=restored down=0', 'phase=restored down=1'),
                    text + '\nnsk3_state: phase=saved malformed'):
            with self.subTest(tail=bad[-100:]), self.assertRaises(ValueError):
                self.check(bad)
        with self.assertRaises(ValueError):
            checker.check_trace(text, self.card, self.card, self.pixels, b'other pixels')
        with self.assertRaises(ValueError):
            checker.check_trace(text, self.card, self.card[:-1] + b'\x01', self.pixels, self.pixels)

    def test_protocol_errors_and_resets_are_not_hidden_by_architecture_parsing(self):
        text = trace()
        for bad in (text + '\nillegal opcode', text + '\nunsupported instruction',
                    text + '\n[LUA ERROR] failed', text + '\nnsk3_reset_request: command=05',
                    text.replace('key=19 down=1', 'key=17 down=1'),
                    text.replace('ins=20 sw=9000', 'ins=20 sw=9804'),
                    text.replace('result=00000001', 'result=00000000'),
                    text.replace('index=64 ', 'index=63 ')):
            with self.subTest(tail=bad[-100:]), self.assertRaises(ValueError):
                self.check(bad)


if __name__ == '__main__':
    unittest.main()

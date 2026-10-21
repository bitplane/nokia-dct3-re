import unittest

from tools.nsb7_sms_delete_check import check_delete
from tools.radio_sms_acceptance_common import SMS_NVRAM_OFFSET, sms_record
from tools.test_nsb7_sms_check import card


def fixture(cancel=False):
    source = bytearray(card())
    source[SMS_NVRAM_OFFSET] = 1
    saved = bytearray(source)
    if not cancel:
        saved[SMS_NVRAM_OFFSET] = 0
    rows = []
    for time in (28, 30, 32):
        key = 'Back' if cancel and time == 32 else 'Menu'
        rows.extend([f'nsb7_sms_delete_input: key={key} state=1 t={time}.000000000',
                     f'nsb7_sms_delete_input: key={key} state=0 t={time}.200000000'])
    times = [19.146, 25.482] + ([] if cancel else [32.4])
    rows.extend(f'sim_device: update fid=6f3c record=1 length=176 t={time}' for time in times)
    if not cancel:
        wire = bytes.fromhex('a0dc0104b0') + sms_record(saved)
        rows.extend(f'nsb7_sim_tx: index={i} value={value:02x}' for i, value in enumerate(wire, 1))
    return '\n'.join(rows), bytes(saved), bytes(source)


class DeleteTests(unittest.TestCase):
    def test_confirm(self):
        check_delete(*fixture())

    def test_cancel(self):
        check_delete(*fixture(True), cancel=True)

    def test_cancelling_cannot_delete(self):
        text, saved, source = fixture(True)
        changed = bytearray(saved)
        changed[SMS_NVRAM_OFFSET] = 0
        with self.assertRaises(ValueError):
            check_delete(text, bytes(changed), source, cancel=True)

    def test_no_secure_erase_claim(self):
        text, saved, source = fixture()
        changed = bytearray(saved)
        changed[SMS_NVRAM_OFFSET+1:SMS_NVRAM_OFFSET+176] = b'\xff'*175
        with self.assertRaises(ValueError):
            check_delete(text, bytes(changed), source)

    def test_confirmation_required(self):
        text, saved, source = fixture()
        with self.assertRaises(ValueError):
            check_delete(text.replace('key=Menu state=1 t=32.', 'key=Back state=1 t=32.'), saved, source)

    def test_write_before_confirmation(self):
        text, saved, source = fixture()
        with self.assertRaises(ValueError):
            check_delete(text.replace('t=32.4', 't=31.4'), saved, source)

    def test_cancel_cannot_hide_a_delete_write(self):
        text, saved, source = fixture(True)
        deleted = bytearray(sms_record(source))
        deleted[0] = 0
        wire = bytes.fromhex('a0dc0104b0') + bytes(deleted)
        text += '\n' + '\n'.join(f'nsb7_sim_tx: index={i} value={value:02x}'
                                  for i, value in enumerate(wire, 1))
        with self.assertRaises(ValueError):
            check_delete(text, saved, source, cancel=True)


if __name__ == '__main__':
    unittest.main()

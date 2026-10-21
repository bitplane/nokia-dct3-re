import re
import unittest

from tools.nsb7_sms_check import check_delivery, check_cold
from tools.radio_sms_acceptance_common import FIRST_SMS_DELIVER_BODY, SMS_NVRAM_OFFSET
from tools.test_nsb7_location_update_check import trace as location_trace


def card():
    record = b'\x03' + FIRST_SMS_DELIVER_BODY
    record += b'\xff'*(176-len(record))
    prefix = bytearray(SMS_NVRAM_OFFSET)
    prefix[1600:1611] = bytes.fromhex('ffffffff00f1100001ff00')
    return bytes(prefix) + record + (b'\x00' + b'\xff'*175)*9


def delivery():
    original = location_trace()
    rows = [row for row in original.splitlines() if not row.startswith('nsb7_radio_tx:')]
    data = {int(i): (kind, payload) for i, kind, payload in re.findall(
        r'nsb7_radio_tx: index=(\d+) type=(\w+) length=\d+ data=(\w+)', original)}
    for index, prefix in {65: '00800f7301', 66: '00800f2101', 67: '00800f4101',
                          70: '00800d40098904', 71: '00800d42158901020240',
                          73: '0080036101'}.items():
        data[index] = ('1b', prefix + '2b'*(25-len(bytes.fromhex(prefix))) + '000000')
    data[74] = ('02', '041202000000001a600002580000000f00000000')
    data[78] = ('57', '03050000')
    for index in range(1, 84):
        kind, payload = data.get(index, ('00', ''))
        rows.append(f'nsb7_radio_tx: index={index} type={kind} length={len(payload)//2} data={payload} cursor=00 producer=01 pc=002cb0ec t=20.0 observer_writes=0')
    record = card()[SMS_NVRAM_OFFSET:SMS_NVRAM_OFFSET+176]
    wire = bytes.fromhex('a0dc0104b0') + record
    rows.extend(f'nsb7_sim_tx: index={i} value={value:02x}' for i, value in enumerate(wire, 1))
    rows.append('GSM service downlink kind=17 sapi=3 pd=09 message=04 length=2 t=20')
    return '\n'.join(rows)


def cold():
    wire = bytes.fromhex('a0b20104b0')
    rows = [f'nsb7_sim_tx: index={i} value={value:02x}' for i, value in enumerate(wire, 1)]
    rows.extend(f'nsb7_simi: kind=read offset=37 value={value:02x} pc=002eee80'
                for value in card()[SMS_NVRAM_OFFSET:SMS_NVRAM_OFFSET+176])
    return '\n'.join(rows)


class SmsTests(unittest.TestCase):
    def test_delivery(self):
        check_delivery(delivery(), card())

    def test_ack_missing(self):
        with self.assertRaises(ValueError):
            check_delivery(delivery().replace('00800d42158901020240', '00800d42158901020241'), card())

    def test_stored_text_changed(self):
        value = bytearray(card())
        value[SMS_NVRAM_OFFSET+10] ^= 1
        with self.assertRaises(ValueError):
            check_delivery(delivery(), bytes(value))

    def test_cold_read(self):
        check_cold(cold(), card(), card())

    def test_cold_missing_received_record(self):
        with self.assertRaises(ValueError):
            check_cold(cold().replace('value=03 ', 'value=00 ', 1), card(), card())

    def test_cold_card_modified(self):
        with self.assertRaises(ValueError):
            check_cold(cold(), card()[:-1], card())


if __name__ == '__main__':
    unittest.main()

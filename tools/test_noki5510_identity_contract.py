import unittest

from tools.noki5510_identity_contract import (wire_identity, display_identity,
    check_formatting, identity_contract, record_reply_entry_eligible, check_record_capture,
    record_reply_nibble_filter, record_reply_slot_binding, record_scan_tail,
    factory_request_word_source, phone_code_from_nv, check_phone_code_capture)


def decode_mobile_identity(data):
    digits = [data[0] >> 4]
    for byte in data[1:]:
        digits.extend((byte & 15,byte >> 4))
    if not data[0] & 8:
        assert digits.pop() == 15
    assert all(digit <= 9 for digit in digits)
    return data[0] & 7, ''.join(map(str,digits))


def fixture():
    return '\n'.join([
        '5510_identity_scope: entry=002a05cc cap=16 identity=debugger_entry operands=context_only observer_writes=0',
        '5510_nv_read: ordinal=42 offset=0000000c destination=0012eadc bytes=00000008 observer_writes=0',
        '5510_identity_entry: ordinal=1 selector=1 sp=001279c0 source=0012eadc destination=0012ea6d packed=ffffffffffffffff observer_writes=0',
        '5510_identity_return: ordinal=1 selector=1 address=002a060a sp=001279b0 source=0012eadc destination=0012ea6d output=faffffffffffff0f observer_writes=0',
        '5510_identity_terminal: entries=1 cap=16 observer_writes=0'])


class IdentityContractTests(unittest.TestCase):
    def test_phone_code_capture_requires_owned_read_and_exact_return(self):
        text = '\n'.join([
            '5510_phone_code_entry: ordinal=1 destination=001292c0 sp=001292b0 observer_writes=0',
            '5510_nv_read: offset=00000110 bytes=00000003 owner=0029f88f observer_writes=0',
            '5510_phone_code_return: ordinal=1 destination=001292c0 sp=0012929c stored_valid=0 output=3132333435 terminator=00 observer_writes=0'])
        self.assertFalse(check_phone_code_capture(text,b'\xff'*0x3800)['stored_digits_valid'])
        for old,new in [('owner=0029f88f','owner=0029f88d'),('terminator=00','terminator=ff'),
                        ('sp=0012929c','sp=001292a0'),('output=3132333435','output=0000000000'),
                        ('stored_valid=0','stored_valid=1'),('offset=00000110','offset=0000000c')]:
            with self.assertRaises(ValueError):
                check_phone_code_capture(text.replace(old,new),b'\xff'*0x3800)

    def test_phone_code_fallback_and_unchecked_sixth_nibble(self):
        self.assertEqual(phone_code_from_nv(b'\xff'*3),
                         {'code':'12345','stored_digits_valid':False})
        for last in range(16):
            self.assertEqual(phone_code_from_nv(bytes([0x12,0x34,0x50|last])),
                             {'code':'12345','stored_digits_valid':True})
        for digit in range(5):
            for value in range(16):
                nibbles = [9]*6
                nibbles[digit] = value
                packed = bytes((nibbles[i]<<4)|nibbles[i+1] for i in range(0,6,2))
                result = phone_code_from_nv(packed)
                self.assertEqual(result['stored_digits_valid'],value < 10)
                if value >= 10:
                    self.assertEqual(result['code'],'12345')
        with self.assertRaises(ValueError):
            phone_code_from_nv(bytes(2))

    def test_factory_word_selection_is_ppm_probe_not_chip_id(self):
        flash = bytearray(b'\xff'*0x350000)
        flash[0x24d34c:0x24d350] = (0x490000).to_bytes(4,'big')
        flash[0x38:0x3c] = bytes.fromhex('12345678')
        flash[-4:] = bytes.fromhex('abcdef01')
        self.assertEqual(factory_request_word_source(flash)['selected_word'],'12345678')
        for offset in (0,35):
            changed = bytearray(flash)
            changed[0x290004+offset] = 0
            self.assertEqual(factory_request_word_source(changed)['selected_word'],'abcdef01')
        flash[0x290004+36] = 0
        self.assertEqual(factory_request_word_source(flash)['selected_word'],'12345678')
        flash[0x24d34c:0x24d350] = bytes(4)
        with self.assertRaises(ValueError):
            factory_request_word_source(flash)

    def test_bounded_record_scan_tail_and_terminal_sentinel(self):
        state = {'stored_index':0,'armed':1}
        requests = []
        for _ in range(11):
            state = record_scan_tail(state['stored_index'],state['armed'],0)
            if state['request_index'] is not None:
                requests.append(state['request_index'])
        self.assertEqual(requests,list(range(1,10))+[0])
        self.assertEqual(state,{'request_index':None,'stored_index':10,'armed':0})
        for index in range(10):
            self.assertIsNone(record_scan_tail(index,1,1)['request_index'])
            self.assertEqual(record_scan_tail(index,0,0)['armed'],0)
        for args in [(-1,1,0),(0,256,0),(0,1,256)]:
            with self.assertRaises(ValueError):
                record_scan_tail(*args)

    def test_record_reply_binding_uses_nv_slot_not_identity_offset(self):
        cache = bytearray(b'\xff'*0x3800)
        for slot in range(10):
            cache[0x20+24*slot:0x38+24*slot] = bytes(range(slot,slot+24))
        for slot in range(10):
            message = bytearray(0x3e)
            message[0x24:0x3c] = cache[0x20+24*slot:0x38+24*slot]
            self.assertTrue(record_reply_slot_binding(message,cache,slot))
            message[0x0c:0x24] = b'\xab'*24
            self.assertTrue(record_reply_slot_binding(message,cache,slot))
            for index in range(0x24,0x3c):
                changed = bytearray(message)
                changed[index] ^= 1
                self.assertFalse(record_reply_slot_binding(changed,cache,slot))
            self.assertFalse(record_reply_slot_binding(message,cache,(slot+1)%10))
        for message,nv,slot in [(b'',cache,0),(bytes(62),cache[:-1],0),
                                (bytes(62),cache,-1),(bytes(62),cache,10)]:
            with self.assertRaises(ValueError):
                record_reply_slot_binding(message,nv,slot)

    def test_record_nibble_filter_is_not_decimal_identity_validation(self):
        allowed = set(range(12)) | {15}
        self.assertTrue(record_reply_nibble_filter(bytes.fromhex('abffffffffffffff')))
        self.assertTrue(record_reply_nibble_filter(b'\xff'*8))
        for position in range(8):
            for value in range(256):
                packed = bytearray(b'\xff'*8)
                packed[position] = value
                expected = value >> 4 in allowed and (position == 7 or value & 15 in allowed)
                self.assertEqual(record_reply_nibble_filter(packed),expected,(position,value))
        for size in (0,7,9):
            with self.assertRaises(ValueError):
                record_reply_nibble_filter(bytes(size))

    def test_factory_capture_evidence(self):
        lines = ['5510_records_scope: reply34=0029fdc4 reply35=0029ffe8 state=001204dc cap=16 observer_writes=0']
        for index,(header,count) in enumerate([('1304',4),('140c',12),('1514',20),('1618',24)]):
            data = header + ('12345678' if index == 0 else 'ff'*count)
            body = f'type=70 payload={count+2} words={(count+4)//2} '
            lines.append('dsp_hle: TX packet ' + body + f'radio_phase=inactive data={data} t=1.{index}00000')
            lines.append('dspif_transport: TX consume ' + body + f'consumer=01 data={data} t=1.{index}00000')
        lines.append('5510_records_terminal: reply34=0 reply35=0 state=02 index=00 receipt34=00 cap=16 observer_writes=0')
        text = '\n'.join(lines)
        self.assertEqual(len(check_record_capture(text)['requests']),4)
        word_text = text + '\n5510_records_word: value=12345678 flash_header=00000000 flash_tail=12345678 observer_writes=0'
        self.assertEqual(len(check_record_capture(word_text,'12345678')['requests']),4)
        for changed in [text,word_text.replace('flash_tail=12345678','flash_tail=87654321'),
                        word_text.replace('value=12345678','value=87654321')]:
            with self.assertRaises(ValueError):
                check_record_capture(changed,'12345678')
        for changed in [text.replace('receipt34=00','receipt34=01'),
                        text.replace('words=4','words=1'),
                        text.replace('reply35=0','reply35=16'),
                        text.replace('140cffff','140c00ff'),
                        text.replace('t=1.200000','t=1.000000'),
                        '\n'.join(lines[:3]+lines[4:]), text+'\n'+lines[1],
                        text+'\n5510_records_reply: command=34 ordinal=1']:
            with self.assertRaises(ValueError):
                check_record_capture(changed)

    def test_record_reply_entry_checksum_and_bypasses(self):
        message = bytearray(0x3e)
        message[9] = 0x34
        message[10:60] = bytes(range(50))
        total = sum(int.from_bytes(message[i:i+2], 'big') for i in range(10,60,2)) & 65535
        message[60:62] = (total ^ 65535).to_bytes(2,'big')
        self.assertTrue(record_reply_entry_eligible(message,9,2))
        message[60] ^= 1
        self.assertFalse(record_reply_entry_eligible(message,9,2))
        self.assertTrue(record_reply_entry_eligible(message,9,1))
        message[9] = 0x32
        self.assertTrue(record_reply_entry_eligible(message,9,2))
        message[9] = 0x33
        self.assertFalse(record_reply_entry_eligible(message,9,2))
        self.assertTrue(record_reply_entry_eligible(message,9,1))
        for state in (0,1,2,255):
            self.assertFalse(record_reply_entry_eligible(message,10,state))
        with self.assertRaises(ValueError):
            record_reply_entry_eligible(message[:12],0,0)

    def test_independent_identity_value_decode(self):
        packed = bytes.fromhex('12345678901234ff')
        self.assertEqual(wire_identity(packed,1).hex(),'1a32547698103204')
        self.assertEqual(wire_identity(packed,2).hex(),'1332547698103204f6')
        self.assertEqual(decode_mobile_identity(wire_identity(packed,1)),(2,'123456789012340'))
        for svn in range(100):
            self.assertEqual(decode_mobile_identity(wire_identity(packed,2,svn)),
                             (3,'12345678901234'+f'{svn:02d}'))
        for last in range(256):
            self.assertEqual(wire_identity(packed[:7]+bytes([last]),1),wire_identity(packed,1))

    def test_qualified_display_forms(self):
        packed = bytes.fromhex('12345678901234ff')
        self.assertEqual(display_identity(packed,1),'12345678901234')
        self.assertEqual(display_identity(packed,3),'123456789012347')
        self.assertEqual(display_identity(packed,5),'123456789012340')
        self.assertEqual(display_identity(packed,9),'1234567890123406')
        for flags in (0,2,15):
            with self.assertRaises(ValueError):
                display_identity(packed,flags)
        with self.assertRaises(ValueError):
            display_identity(b'\xff'*8,3)

    def test_erased_runtime_is_negative_identity_not_provisioning(self):
        result = check_formatting(fixture(),{'software_version_decimal':6})
        self.assertFalse(result['calls'][0]['decimal_domain_valid'])
        self.assertEqual(result['calls'][0]['wire'],'faffffffffffff0f')

    def test_reject_changed_frames_scope_or_bytes(self):
        for before,after in [('cap=16','cap=1'),('entries=1','entries=2'),
                             ('output=faff','output=0aff'),('sp=001279b0','sp=001279b2'),
                             ('address=002a060a','address=002a063e'),
                             ('bytes=00000008','bytes=00000007'),
                             ('offset=0000000c','offset=00000014'),
                             ('destination=0012ea6d','destination=0022ea6d'),
                             ('observer_writes=0','observer_writes=1')]:
            with self.subTest(before=before),self.assertRaises(ValueError):
                check_formatting(fixture().replace(before,after),{'software_version_decimal':6})

    def test_reject_unsupported_reference_or_source(self):
        with self.assertRaises(ValueError):
            wire_identity(bytes(7),1)
        with self.assertRaises(ValueError):
            wire_identity(bytes(8),2,100)
        with self.assertRaises(ValueError):
            identity_contract(bytes(0x350000))


if __name__ == '__main__':
    unittest.main()

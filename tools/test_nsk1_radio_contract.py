import unittest

from tools.nsk1_radio_contract import (literal_references,analyze,native_completion_contract,
    live_publisher_census,serial_publisher_contract,tuning_upload_contract,
    PUBLISHER_TYPE_LOADS,publisher_type_inventory)


class Nsk1RadioContractTest(unittest.TestCase):
    def publisher_image(self):
        image = bytearray(0x20000)
        for call, (load, packet_type) in PUBLISHER_TYPE_LOADS.items():
            image[call*2:call*2+4] = bytes.fromhex('f27437ce')
            image[load*2:load*2+4] = bytes((0x77,0x14,0,packet_type))
        image[0x9404*2:0x9404*2+4] = bytes.fromhex('f27437ce')
        return image

    def test_publisher_types_leave_inherited_and_reachability_open(self):
        result = publisher_type_inventory(self.publisher_image())
        self.assertEqual(result['direct_candidates'],19)
        self.assertEqual(len(result['local_type_loads']),18)
        self.assertEqual(result['inherited_type_site'],'9404')
        self.assertFalse(result['unconditional_heartbeat_proved'])
        self.assertEqual(sum(row['packet_type']=='74' for row in result['local_type_loads']),8)

    def test_changed_type_and_extra_or_missing_call_fail_inventory(self):
        for offset in (0x314f*2+3,0x4f20*2,0x4810*2):
            image = self.publisher_image()
            image[offset] ^= 1
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                publisher_type_inventory(image)
        image = self.publisher_image()
        image[0x20:0x24] = bytes.fromhex('f27437ce')
        with self.assertRaises(ValueError):
            publisher_type_inventory(image)

    def test_own_tuning_table_does_not_inherit_nse1_addends(self):
        image = bytearray(0x100000)
        image[0x9821c:0x9822e] = bytes.fromhex('216a1e80011c013000780000000417bf1797')
        self.assertEqual(tuning_upload_contract(image)['native_fields']['0x216b'],'0x17bf')
        image[0x9822a:0x9822c] = bytes.fromhex('13a6')
        with self.assertRaises(ValueError):
            tuning_upload_contract(image)

    def test_serial_publisher_has_typed_calls_and_a_queue_gate(self):
        image = bytearray(0x20000)
        for address,words in {0x8dd4:'721206c010f806b6fa458dfb',0x8ded:'f274938b7714009b',0x8df5:'f274938b7714009a',0x8ed8:'74f8000800358813',0x8fb5:'6bf806b60001',0x8fc8:'6bf806b60001',0x938d:'771306b61083fc47',0x9404:'f27437ce77131200'}.items():
            data = bytes.fromhex(words)
            image[address*2:address*2+len(data)] = data
        result = serial_publisher_contract(image)
        self.assertEqual([row['packet_type'] for row in result['direct_callers']],['0x9b','0x9a'])
        self.assertFalse(result['unconditional_heartbeat_proved'])
        self.assertEqual(result['input_port'],'0x35')
        self.assertEqual(result['checked_queue_increment_sites'],['0x8fb5','0x8fc8'])
        self.assertIn('0x8fb6',result['raw_queue_word_occurrences'])
        image[0x20:0x24] = bytes.fromhex('f074938b')
        with self.assertRaisesRegex(ValueError,'caller census'):
            serial_publisher_contract(image)
        image[0x20:0x24] = bytes(4)
        image[0x938d*2] = 0
        with self.assertRaisesRegex(ValueError,'sequence'):
            serial_publisher_contract(image)

    def test_live_publisher_census_is_word_ordered_and_bounded(self):
        image = bytearray(0x20000)
        image[0x20:0x28] = bytes.fromhex('f27437cef07337ce')
        result = live_publisher_census(image)
        self.assertEqual(result['words_scanned'],65536)
        self.assertEqual(result['adjacent_pairs_scanned'],65535)
        self.assertEqual(result['direct_call_candidates'],[{'address':'0x10','delayed':True}])
        self.assertEqual(result['direct_tail_branch_candidates'],['0x12'])
        self.assertIn('indirect',result['scope'])
        image[0x20:0x28] = bytes.fromhex('74f2ce3773f0ce37')
        self.assertFalse(live_publisher_census(image)['direct_call_candidates'])
        with self.assertRaises(ValueError):
            live_publisher_census(image[:-2])

    def test_big_endian_aligned_literal_candidates(self):
        image = bytearray(64)
        image[0:2] = bytes.fromhex('4803')
        image[2:4] = bytes.fromhex('4903')
        image[16:20] = (0x107158).to_bytes(4,'big')
        result = literal_references(image,{0x107158})
        self.assertEqual([{'address':'0x200010','value':'0x107158'}],result['aligned_pools'])
        self.assertEqual(['0x200000','0x200002'],[row['address'] for row in result['thumb16_literal_load_candidates']])
        self.assertEqual(31,result['aligned_halfwords_scanned'])
        image[16:20] = (0x107158).to_bytes(4,'little')
        self.assertFalse(literal_references(image,{0x107158})['aligned_pools'])

    def test_requires_own_exact_image(self):
        with self.assertRaisesRegex(ValueError,'own standard'):
            analyze(bytes(1024))

    def test_native_publisher_rejects_unqualified_program(self):
        with self.assertRaisesRegex(ValueError, 'acquired NSE-1'):
            native_completion_contract(bytes(1024))


if __name__ == '__main__':
    unittest.main()

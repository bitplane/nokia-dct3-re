import unittest
from unittest.mock import patch

from tools.npe3_sch_receive_check import check, decode_information, check_program


class NativeSchResultTest(unittest.TestCase):
    text = ('npe3_sch_crc: sequence=1 t=3.251000000 status=0000 remainder=03ff words=00a8,e40a,0002 read_owner=004f5d class=0040 observer_writes=0\n'
            'npe3_capture_due: target=1201 counter=1201 t=3.248953000 physical_claim=0')
    source = dict(source_first_tick=42236400, source_last_tick=42247908)
    packet = dict(type=0x80, payload='40000000002900230000a8000ae4', time='3.251147560')

    def checked(self, text=None, source=None, packet=None):
        with patch('tools.npe3_sch_receive_check.check_rx_publications', return_value=dict(packets=[packet or self.packet])), \
                patch('tools.npe3_sch_receive_check.check_program'):
            return check(text or self.text, source or self.source, b'', b'', b'', b'')

    def test_code_and_data_provenance_is_required(self):
        with self.assertRaises(ValueError):
            check_program(bytes(131072), bytes(131072), bytes(131072), bytes(131072))

    def test_information_is_independently_decoded_and_parity_checked(self):
        self.assertEqual(decode_information([0x00a8, 0xe40a, 2])[:2], (704, 42))
        report = self.checked()
        self.assertTrue(report['native_sch_crc_qualified'])
        self.assertFalse(report['physical_capture_qualified'])
        self.assertFalse(report['camping_qualified'])

    def test_later_reacquisition_requires_opt_in_and_its_own_parity_publication(self):
        text=self.text+'\nnpe3_sch_crc: sequence=2 t=10.694439033 status=0000 remainder=03ff words=00a8,9c89,0001 read_owner=004f5d class=0040 observer_writes=0'
        packet=dict(self.packet,payload='40000000002900230000a800899c',time='10.694500000')
        with patch('tools.npe3_sch_receive_check.check_rx_publications',return_value=dict(packets=[self.packet,packet])), \
                patch('tools.npe3_sch_receive_check.check_program'):
            with self.assertRaises(ValueError):
                check(text,self.source,b'',b'',b'',b'')
            result=check(text,self.source,b'',b'',b'',b'',allow_reacquisition=True)
            self.assertEqual(result['native_sch_results_checked'],2)
            for bad in (text.replace('9c89','9c88'),text.replace('sequence=2','sequence=3')):
                with self.assertRaises(ValueError):
                    check(bad,self.source,b'',b'',b'',b'',allow_reacquisition=True)

    def test_reject_status_parity_source_and_packet_mismatches(self):
        for old, new in [('status=0000', 'status=0001'), ('remainder=03ff', 'remainder=03fe'),
                         ('e40a', 'e60a'), ('00a8', '00ac'), ('read_owner=004f5d', 'read_owner=004f5c'),
                         ('counter=1201', 'counter=1200')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                self.checked(text=self.text.replace(old, new))
        for source in [dict(source_first_tick=42240012, source_last_tick=42247908),
                       dict(source_first_tick=42236400, source_last_tick=42247000)]:
            with self.assertRaises(ValueError):
                self.checked(source=source)
        for packet in [dict(self.packet, payload='40000100002900230000a8000ae4'),
                       dict(self.packet, payload='40000000002900230000a8000be4'),
                       dict(self.packet, time='3.250000000')]:
            with self.assertRaises(ValueError):
                self.checked(packet=packet)

    def test_failed_reacquisition_must_have_matching_failure_publication(self):
        failure='\nnpe3_sch_crc: sequence=2 t=4.000000000 status=0001 remainder=0219 words=176d,663e,0002 read_owner=004f5d class=0040 observer_writes=0'
        failed_packet=dict(self.packet,payload='400001000029002300006d173e66',time='4.000010000')
        with patch('tools.npe3_sch_receive_check.check_rx_publications',return_value=dict(packets=[self.packet,failed_packet])), \
                patch('tools.npe3_sch_receive_check.check_program'):
            result=check(self.text+failure,self.source,b'',b'',b'',b'',allow_reacquisition=True)
            self.assertEqual(result['native_sch_results_checked'],1)
            self.assertEqual(result['native_sch_failed_attempts_checked'],1)
            for bad in (failure.replace('status=0001','status=0000'),
                        failure.replace('0219','03ff'),failure.replace('176d','176c'),
                        failure.replace('sequence=2','sequence=3')):
                with self.assertRaises(ValueError):
                    check(self.text+bad,self.source,b'',b'',b'',b'',allow_reacquisition=True)
            with self.assertRaises(ValueError):check(self.text+failure,self.source,b'',b'',b'',b'')


if __name__ == '__main__':
    unittest.main()

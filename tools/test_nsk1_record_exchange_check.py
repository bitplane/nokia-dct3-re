import unittest

from tools.nsk1_record_exchange_check import check


class Nsk1RecordExchangeTest(unittest.TestCase):
    def transcript(self):
        lines = []
        for encoded,decoded in (
                ('7f80'*12,'b2ade0d999085170cbb40000'*2),
                ('ff'*24,'5d3e6f9a3cf547fbb1f00000'*2)):
            for command,body in ((0x13,'e688a8f2'),(0x14,'ff'*12),(0x15,'ff'*20),(0x16,encoded)):
                size = len(bytes.fromhex(body))
                packet = f'0002{size+2:02x}70{command:02x}{size:02x}'+body
                lines.append(f'nsk1_record_request: command={command:02x} bytes={packet} caller=0025a313 t=0.5 observer_writes=0')
            lines.append('nsk1_record_reply: consumer=0025a620 envelope=0000007400120100 payload=340e00829f623628a1e763cc898cb419 t=0.54 observer_writes=0')
            lines.append(f'nsk1_record_reply: consumer=0025a814 envelope=0000007400360100 payload=35320000{decoded}{encoded} t=0.55 observer_writes=0')
        return '\n'.join(lines)

    def test_both_own_exchanges_match_without_acceptance_claim(self):
        report = check(self.transcript(),initial_eeprom=bytes([255])*2048)
        self.assertEqual(['retained transform','raw'],[row['storage_source'] for row in report['exchanges']])
        self.assertEqual(['00160010']*2,[row['chip_field'] for row in report['exchanges']])
        self.assertEqual(['b35b']*2,report['exchanges'][0]['private_markers'])
        self.assertIn('not fitted-mask',report['scope'])
        self.assertEqual('not checked',check(self.transcript())['exchanges'][0]['storage_source'])

    def test_changed_payload_echo_checksum_and_envelope_fail(self):
        text = self.transcript()
        for before,after in (('b2ade0d9','b2ade0d8'),('35320000','35320001'),
                             ('0000007400360100','0000007400350100'),
                             ('00120100','00120101'),('1304e688a8f2','1304e688a8f3'),
                             ('340e0082','340e0083'),('command=13','command=14'),
                             ('16187f80','16187f81')):
            with self.subTest(before=before),self.assertRaises(ValueError):
                check(text.replace(before,after,1))

    def test_incomplete_reordered_and_malformed_traces_fail(self):
        lines = self.transcript().splitlines()
        for text in ('', '\n'.join(lines[:-1]), '\n'.join(lines[1:]),
                     '\n'.join(lines[:1]+lines), '\n'.join(lines[:4]+lines[5:6]+lines[4:5]+lines[6:]),
                     self.transcript()+'\nnsk1_record_reply: unknown'):
            with self.assertRaises(ValueError):
                check(text)

    def test_initial_storage_is_not_assumed_valid_or_ignored(self):
        for image in (bytes(2047),bytes(2048)):
            with self.assertRaises(ValueError):
                check(self.transcript(),initial_eeprom=image)
        with self.assertRaises(ValueError):
            check(self.transcript(),minimum=0)

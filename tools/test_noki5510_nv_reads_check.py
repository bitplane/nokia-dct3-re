import unittest

from tools.noki5510_nv_reads_check import check_reads, records, source_contract


def fixture():
    requests = ((0,0x120,0x2a05a5),(0x120,0x136,0x24c671),
                (0x154,2,0x3368b3),(0x256,2,0x24c82f),(0x170,2,0x24c83d))
    lines = ['5510_nv_reads_scope: reader=002fd578 wrapper=003ac638 cap=1024 '
             'identity=debugger_entry operands=context_only observer_writes=0']
    for ordinal, (offset,count,owner) in enumerate(requests,1):
        lines.append(f'5510_nv_read: ordinal={ordinal} offset={offset:08x} '
                     f'destination=00120000 bytes={count:08x} link=003ac651 owner={owner:08x} '
                     'identity=debugger_entry operands=context_only observer_writes=0')
    lines += ['5510_nv_reads_terminal: events=5 cap=1024 observer_writes=0',
              '5510_nv_reads_stop: t=8.016666667 observer_writes=0',
              'pc=0024c7dc address=0013fbf2 data=00000012',
              'pc=0024c85a address=0013fbec data=0000000c',
              '5510_selftest_return: command=0d flags=88 records=ffff00ff/00ffff00/00ffff00/0cff0000/00001200']
    contract = {'encoded_reference_census': {'direct_calls':
                {0x3ac638: [(owner & ~1)-4 for _,_,owner in requests]}}}
    return '\n'.join(lines), b'\xff'*0x3800, contract


class NvReadInventoryTests(unittest.TestCase):
    def test_ordered_checksum_reads_and_coverage(self):
        result = check_reads(*fixture())
        self.assertEqual(result['requests'], 5)
        self.assertEqual(result['unique_requested_bytes'], 0x258)
        self.assertFalse(result['owners_outside_direct_bl_census'])

    def test_unresolved_owner_is_reported_not_absence(self):
        text, cache, contract = fixture()
        contract['encoded_reference_census']['direct_calls'][0x3ac638] = []
        result = check_reads(text, cache, contract)
        self.assertEqual(len(result['owners_outside_direct_bl_census']), 5)

    def test_reject_incomplete_or_changed_request_domain(self):
        text, cache, contract = fixture()
        for before, after in [('events=5','events=4'),('cap=1024','cap=5'),
                              ('ordinal=2','ordinal=1'),('bytes=00000120','bytes=00003801'),
                              ('bytes=00000120','bytes=00000000'),
                              ('destination=00120000','destination=0017ffff'),
                              ('link=003ac651','link=003ac64f'),('owner=002a05a5','owner=002a05a4'),
                              ('owner=002a05a5','owner=006a05a5'),
                              ('operands=context_only','operands=retired'),
                              ('t=8.016666667','t=NaN'),('24c85a','24c858'),
                              ('observer_writes=0','observer_writes=1')]:
            with self.subTest(before=before), self.assertRaises(ValueError):
                check_reads(text.replace(before,after), cache, contract)

    def test_duplicate_fields_and_wrong_source_rejected(self):
        with self.assertRaises(ValueError):
            records('x: a=1 a=2 observer_writes=0','x')
        with self.assertRaises(ValueError):
            source_contract(bytes(0x350000))


if __name__ == '__main__':
    unittest.main()

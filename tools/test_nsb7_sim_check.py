import unittest
from tools.nsb7_sim_check import check_trace


def trace():
    tx = bytes.fromhex('ff00ffa0a40000023f00a0a40000022fe2a0b000000a'
                       'a0a40000026faea0a40000026f07a0b0000009'
                       'a0a40000026f3aa0c000000f')
    tx += b''.join(bytes((0xa0, 0xb2, record, 4, 32)) for record in range(1, 51))
    tx += bytes.fromhex('a0f2000016')
    rows = ['nsb7_profile: machine=nsb7hle observer_writes=0',
            'nsb7_card_fixture: present=1 physical_contact=1',
            'runtime_hle_handoff pc=2c75 native_suspended=1',
            'nsb7_simi: kind=read offset=38 value=40 pc=002eeefe',
            'nsb7_simi: kind=write offset=39 value=b3 pc=002eeb9c',
            'nsb7_key_decode: key=01 observer_writes=0']
    rows += [f'nsb7_sim_tx: index={i} value={value:02x}' for i, value in enumerate(tx, 1)]
    rows += [f'nsb7_simi: kind=read offset=37 value={value:02x} pc=002eee80'
             for value in bytes.fromhex('3b1005ff00ff')]
    return '\n'.join(rows)


class SimTransportTests(unittest.TestCase):
    def test_complete(self):
        check_trace(trace())

    def test_reject_missing_boundary(self):
        for line in trace().splitlines()[:6]:
            with self.subTest(line=line), self.assertRaises(ValueError):
                check_trace(trace().replace(line, ''))

    def test_reject_broken_wire_capture(self):
        for old, new in [('index=2 ', 'index=3 '), ('offset=37 value=3b', 'offset=37 value=00'),
                         ('machine=nsb7hle', 'machine=noki8290')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                check_trace(trace().replace(old, new))
        with self.assertRaises(ValueError):
            check_trace(trace() + '\nLUA ERROR')


if __name__ == '__main__':
    unittest.main()

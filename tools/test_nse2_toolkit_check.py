import unittest

from tools.nse2_toolkit_check import DISPLAY, RESPONSE, check_trace, check_storage


def capture(proactive=True):
    tx = bytes.fromhex('a0a40000026f3aa0dc010420') + bytes(32)
    if proactive:
        tx += bytes.fromhex('a01000000403018000a012000016a01400000c') + RESPONSE
    rows = [f'nse2_toolkit_wire: direction=TX data={value:02x} t=1 observer_writes=0'
            for value in tx]
    if proactive:
        rows += [f'nse2_toolkit_wire: direction=RX data={value:02x} t=2 observer_writes=0'
                 for value in DISPLAY + b'\x90\x00']
    rows += [f'nse2_phonebook_contact: event={i} down={i % 2} t=1 physical_only=1'
             for i in range(1, 17)]
    rows += [f'SIM status ins=dc sw={"9116" if proactive else "9000"} chv=3/3 puk=10/10 enabled=0']
    if proactive:
        rows += ['SIM status ins=14 sw=9000 chv=3/3 puk=10/10 enabled=0']
    return '\n'.join(rows)


class ToolkitCheckTests(unittest.TestCase):
    def test_refusal_and_control(self):
        self.assertTrue(check_trace(capture(), True)['capability_refusal'])
        self.assertFalse(check_trace(capture(False), False)['fetched'])

    def test_rejects_missing_or_changed_evidence(self):
        text = capture()
        for changed in (text.replace('data=30 ', 'data=00 '),
                        text.replace('data=d0 ', 'data=d1 '),
                        text.replace('sw=9116', 'sw=9000'),
                        text.replace('sw=9000', 'sw=6f00'),
                        text.replace('event=16 down=0', 'event=16 down=1'),
                        text + '\nLUA ERROR', text + '\nnse2_reset_entry:'):
            with self.subTest(changed=changed[-80:]), self.assertRaises(ValueError):
                check_trace(changed, True)

    def test_control_rejects_proactive_traffic(self):
        with self.assertRaises(ValueError):
            check_trace(capture(), False)

    def test_storage_rejects_unqualified_endpoint(self):
        base = dict(flash=b'flash', sim_card=b'card', eeprom=bytes(8192))
        for field in ('flash', 'sim_card', 'eeprom'):
            changed = dict(base, **{field: b'changed'})
            with self.subTest(field=field), self.assertRaises(ValueError):
                check_storage(base, changed)


if __name__ == '__main__':
    unittest.main()

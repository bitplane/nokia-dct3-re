import unittest

from tools.nsb7_radio_check import check_location_update, check_receiver_return


def trace():
    rows = []
    for block in (
            '01734905087000f000fffe200809101010325476982b2b2b',
            '030045050200f1100001170809101010325476982b2b2b2b',
            '03020d060d002b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b2b'):
        for pc, task in [('002e49e6', 4), ('002645ec', 14)]:
            rows.append(f'nsb7_lapdm_downlink: pc={pc} task={task} selector=80 block={block} t=11')
    for index, prefix in [(43, '0080032101'), (44, '0080034101')]:
        rows.append(f'nsb7_radio_tx: index={index} type=1b length=28 data={prefix}' + '2b'*20 + '000000 t=11')
    rows.append('nsb7_radio_tx: index=45 type=02 length=20 data=041202000000001a600002580000000f00000000 t=12')
    for state, pc in [(4, '002665c0'), (3, '0026644a'), (0, '00265674')]:
        rows.append(f'nsb7_lapdm_state: context=0012a6dc state={state} pc={pc} task=14 t=11')
    return '\n'.join(rows)


class LocationUpdateCheckTests(unittest.TestCase):
    def test_acknowledged_exchange(self):
        check_location_update(trace())

    def test_missing_consumer(self):
        with self.assertRaises(ValueError):
            check_location_update(trace().replace('pc=002645ec', 'pc=002645ee', 1))

    def test_wrong_acknowledgement(self):
        with self.assertRaises(ValueError):
            check_location_update(trace().replace('0080034101', '0080032101'))

    def test_unreleased_link(self):
        with self.assertRaises(ValueError):
            check_location_update(trace().replace('state=0', 'state=4'))

    def release_fixture(self):
        rows = [f'nsb7_radio_tx: index={i} type=00 length=0 data= cursor=00 producer=01 pc=002cb0ec t=1.0 observer_writes=0'
                for i in range(1, 43)]
        text = trace().replace(' t=11', ' cursor=00 producer=01 pc=002cb0ec t=11.0 observer_writes=0').replace(
            ' t=12', ' cursor=00 producer=01 pc=002cb0ec t=12.0 observer_writes=0')
        rows.append(text)
        for index, kind, data in [
                (46, '46', '3210321000010000'),
                (47, '02', '0412020200000016600002580000000001297000'),
                (48, '02', '0412020200000016600002580000000001297000'),
                (49, '57', '03050000')]:
            rows.append(f'nsb7_radio_tx: index={index} type={kind} length={len(data)//2} data={data} cursor=00 producer=01 pc=002cb0ec t=15.0 observer_writes=0')
        for index in range(26, 29):
            rows.append(f'nsb7_radio_rx: index={index} type=89 length=8 t=15')
        rows.extend(['live_channel=0258 live_mode=1a ', 'live_channel=0258 live_mode=16 '])
        wire = bytes.fromhex('a0a40000026f7ea0d600040500f1100001a0d6000a0100')
        rows.extend(f'nsb7_sim_tx: index={i} value={value:02x}' for i, value in enumerate(wire, 1))
        card = bytes(1600) + bytes.fromhex('ffffffff00f1100001ff00')
        return '\n'.join(rows), card

    def test_receiver_return(self):
        check_receiver_return(*self.release_fixture())

    def test_card_not_updated(self):
        text, card = self.release_fixture()
        with self.assertRaises(ValueError):
            check_receiver_return(text, card[:-1] + b'\x01')

    def test_missing_wire_update(self):
        text, card = self.release_fixture()
        with self.assertRaises(ValueError):
            check_receiver_return(text.replace('nsb7_sim_tx:', 'absent:'), card)


if __name__ == '__main__':
    unittest.main()

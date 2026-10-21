import unittest

from tools.nsb7_radio_check import check_location_update, check_receiver_return, check_paging, check_addressed_page, check_page_response, check_incoming_call_setup, check_physical_call


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

    def paging_fixture(self):
        block = '1506210001f0' + '2b'*18
        rows = []
        for index in range(8):
            rows.extend([
                f'nsb7_agch_forward: index={index} task=4 mode=60 error=00 frame={3453+index*102} block={block} t=15',
                f'nsb7_agch_consumer: index={index} task=14 tag=0623 mode=60 block={block} t=15',
                f'nsb7_rr_decoder_result: index={index} task=14 code=061a block={block} t=15'])
        return '\n'.join(rows)

    def test_repeated_paging(self):
        check_paging(self.paging_fixture())

    def test_paging_wrong_frame(self):
        with self.assertRaises(ValueError):
            check_paging(self.paging_fixture().replace('frame=3555', 'frame=3556'))

    def test_paging_not_decoded(self):
        with self.assertRaises(ValueError):
            check_paging(self.paging_fixture().replace('code=061a', 'code=0000', 1))

    def test_explicitly_permitted_missed_opportunity(self):
        # The qualification is group consistency, not complete L1 timing.
        text = self.paging_fixture()
        for index in reversed(range(1, 8)):
            text = text.replace(f'frame={3453+index*102} ',
                                f'frame={3453+(index+1)*102} ')
        check_paging(text)

    def addressed_fixture(self):
        text, _ = self.release_fixture()
        rows = [text,
                'nsb7_agch_consumer: index=6 task=14 tag=0623 mode=60 block=31062110080910101032547698' + '2b'*11 + ' t=16',
                'nsb7_page_post: index=10 source=14 target=17 lr=00264591 data=05eb t=16']
        for data in [
                'pc=00274124 r0=00000017 r2=00000009 data=080910101032547698 ',
                'pc=002741dc r0=00000001 r2=00000000 data=080910101032547698 stack=00102168080910101032547698 ',
                'pc=002741f6 r0=00000000 ',
                'pc=00274e82 r0=00000001 ',
                'pc=002b3130 r0=0000083e ']:
            rows.append('nsb7_page_identity: ' + data + 't=16')
        rows.extend([
            'nsb7_page_event_return: event=083e caller=00260c2b record=00130660 t=16',
            'nsb7_page_dispatch: pc=00260c4c event=083e record=0012c97c flag0b=00 saved=001069bc flag03=00 flag06=01 api1=01 t=16',
            'nsb7_page_dispatch: pc=00260c16 event=083e record=0012c97c flag0b=01 saved=00000000 flag03=00 flag06=01 api1=01 t=16',
            'nsb7_page_post: index=42 source=18 target=16 lr=0022e323 data=09c8 t=14',
            'nsb7_page_post: index=46 source=16 target=17 lr=00200e39 data=07d4 t=14',
            'nsb7_rr_flag06_trigger: event=07d4 report=03ec t=14',
            'nsb7_rr_flag06: value=01 pc=00260a48 task=17 t=14'])
        rows.extend([
            'nsb7_9c8_source: caller=00232057 task=18 t=6',
            'nsb7_9c8_source: caller=00232057 task=18 t=14',
            'nsb7_9c8_completion: event=1584 record=0012f2e8 t=10',
            'nsb7_9c8_completion: event=1599 record=0012f2e8 t=20',
            'nsb7_service_completion: pending=00102168 tag=03ec t=10',
            'nsb7_page_post: index=21 source=17 target=13 lr=002b29a1 data=03ec t=6',
            'nsb7_page_post: index=76 source=17 target=13 lr=002b29a1 data=03ec t=14'])
        return '\n'.join(rows)

    def test_addressed_imsi_match(self):
        check_addressed_page(self.addressed_fixture())

    def test_addressed_identity_mismatch(self):
        with self.assertRaises(ValueError):
            check_addressed_page(self.addressed_fixture().replace('pc=002741f6 r0=00000000', 'pc=002741f6 r0=00000001'))

    def test_addressed_result_missing(self):
        with self.assertRaises(ValueError):
            check_addressed_page(self.addressed_fixture().replace('r0=0000083e', 'r0=00000000'))

    def test_addressed_saved_record_not_released(self):
        with self.assertRaises(ValueError):
            check_addressed_page(self.addressed_fixture().replace('saved=00000000', 'saved=001069bc'))

    def test_addressed_readiness_not_assumed(self):
        with self.assertRaises(ValueError):
            check_addressed_page(self.addressed_fixture().replace('flag06=01', 'flag06=00'))

    def test_post_release_timeout_not_completion(self):
        with self.assertRaises(ValueError):
            check_addressed_page(self.addressed_fixture().replace('event=1599', 'event=1584'))

    def response_fixture(self):
        text, card = self.release_fixture()
        rows = [text, 'nsb7_radio_rx: index=29 type=8b length=166 t=15',
                'nsb7_service_completion: pending=00102138 tag=03ec caller=00220deb t=17',
                'nsb7_9c8_completion: event=1584 record=0012f2e8 t=17']
        for index in range(50, 70):
            kind, data = '00', ''
            if index == 57:
                kind, data = '0c', '0000186c00000000'
            if index == 61:
                kind, data = '1b', '0080013f41062707032058810809101010325476982b2b2b2b000000'
            rows.append(f'nsb7_radio_tx: index={index} type={kind} length={len(data)//2} data={data} cursor=00 producer=01 pc=002cb0ec t=18.0 observer_writes=0')
        ua = '01734106270703205881080910101032547698' + '2b'*5
        for pc, task in [('002e49e6', 4), ('002645ec', 14)]:
            rows.append(f'nsb7_lapdm_downlink: pc={pc} task={task} selector=80 block={ua} t=18')
        return '\n'.join(rows), card

    def test_own_paging_response(self):
        check_page_response(*self.response_fixture())

    def test_paging_response_wrong_identity(self):
        text, card = self.response_fixture()
        with self.assertRaises(ValueError):
            check_page_response(text.replace('06270703205881080910101032547698', '06270703205881080910101032547699'), card)

    def test_paging_response_no_background_completion(self):
        text, card = self.response_fixture()
        with self.assertRaises(ValueError):
            check_page_response(text.replace('index=29 type=8b', 'index=29 type=83'), card)

    def test_paging_response_retained_location(self):
        text, card = self.response_fixture()
        with self.assertRaises(ValueError):
            check_page_response(text, card[:-1] + b'\x01')

    def call_fixture(self):
        text, card = self.response_fixture()
        rows = text.splitlines()
        payloads = {
            64: '00800140090632' + '2b'*18 + '000000',
            67: '008001622d8308040460020180150101' + '2b'*9 + '000000',
            68: '00800184098341' + '2b'*18 + '000000',
        }
        for index, payload in payloads.items():
            replacement = f'nsb7_radio_tx: index={index} type=1b length=28 data={payload} cursor=00 producer=01 pc=002cb0ec t=19.0 observer_writes=0'
            rows = [replacement if row.startswith(f'nsb7_radio_tx: index={index} ') else row for row in rows]
        rows.append('nsb7_radio_tx: index=70 type=02 length=20 data=041202000271012fc10002580000000400000000 cursor=00 producer=01 pc=002cb0ec t=19.0 observer_writes=0')
        block = '032445030504046002008134015c0581551532f4' + '2b'*4
        for pc, task in [('002e49e6', 4), ('002645ec', 14)]:
            rows.append(f'nsb7_lapdm_downlink: pc={pc} task={task} selector=80 block={block} t=19')
        rows.append('nsb7_channel_pending: address=00102168 tag=0402 bit=00 armed=01 t=19')
        return '\n'.join(rows), card

    def test_incoming_call_confirmed_and_alerting(self):
        check_incoming_call_setup(*self.call_fixture())

    def test_call_without_alerting_fails(self):
        text, card = self.call_fixture()
        with self.assertRaises(ValueError):
            check_incoming_call_setup(text.replace('00800184098341', '00800184098342'), card)

    def test_call_wrong_traffic_descriptor(self):
        text, card = self.call_fixture()
        with self.assertRaises(ValueError):
            check_incoming_call_setup(text.replace('bit=00 armed=01', 'bit=01 armed=01'), card)

    def test_call_packet_order_is_not_a_set(self):
        text, card = self.call_fixture()
        with self.assertRaises(ValueError):
            check_incoming_call_setup(text.replace('index=67 type=1b', 'index=68 type=1b'), card)

    def physical_call_fixture(self):
        text, card = self.call_fixture()
        rows = text.splitlines()
        def block(prefix):
            return prefix + '2b'*(25-len(bytes.fromhex(prefix))) + '000000'
        payloads = {71: block('00b0013f01'), 72: block('00b001000d062900'),
                    75: block('00b00104098307'), 78: block('00b0012615836502e090'),
                    80: block('00b0014809832a'),
                    83: '041202001117001a600002580000001400000001',
                    86: '0412020200000016600002580000000001297000', 87: '03050000'}
        for index in range(71, 93):
            key = 'Send' if index == 75 else 'End' if index == 78 else None
            if key:
                when = 22 if key == 'Send' else 27
                rows.append(f'nsb7_call_input: key={key} state=1 t={when}.000000000')
            data = payloads.get(index, '')
            kind = '02' if index in (83, 86) else '57' if index == 87 else '1b'
            time = 19.14 if index < 75 else 22.04 if index < 78 else 27.04 if index < 84 else 30.0
            rows.append(f'nsb7_radio_tx: index={index} type={kind} length={len(data)//2} data={data} cursor=00 producer=01 pc=002cb0ec t={time} observer_writes=0')
            if key:
                rows.append(f'nsb7_call_input: key={key} state=0 t={when}.200000000')
        for prefix in ('017301', '036009030f'):
            data = prefix + '2b'*(24-len(bytes.fromhex(prefix)))
            for pc, task in [('002e49e6', 4), ('002645ec', 14)]:
                rows.append(f'nsb7_lapdm_downlink: pc={pc} task={task} selector=b0 block={data} t=28')
        rows.append('nsb7_dedicated_armed: address=00102168 value=00 pc=002e5086 task=4 t=19.13')
        return '\n'.join(rows), card

    def test_physical_answer_end_signalling(self):
        check_physical_call(*self.physical_call_fixture())

    def test_physical_connect_missing(self):
        text, card = self.physical_call_fixture()
        with self.assertRaises(ValueError):
            check_physical_call(text.replace('00b00104098307', '00b00104098308'), card)

    def test_physical_answer_required(self):
        text, card = self.physical_call_fixture()
        with self.assertRaises(ValueError):
            check_physical_call(text.replace('key=Send state=1', 'key=End state=1'), card)

    def test_physical_release_parameter(self):
        text, card = self.physical_call_fixture()
        with self.assertRaises(ValueError):
            check_physical_call(text.replace('600002580000001400000001', '600002580000000f00000001'), card)


if __name__ == '__main__':
    unittest.main()

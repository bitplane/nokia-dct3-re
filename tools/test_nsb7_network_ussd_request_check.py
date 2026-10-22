import unittest
import re
from unittest.mock import patch

from tools.nsb7_network_ussd_request_check import KEYS, check, check_state


def fixture():
    lines = [
        'PCH IMSI page transmitted channel=60 fn=1',
        'GSM service establish sapi=0 pd=06 message=27 length=16',
        'gsm_ss: network_initiated operation=request transaction=0b invoke=1 dcs=0f',
        'GSM service downlink kind=32 sapi=0 pd=0b message=3b length=29 t=22',
    ]
    for second, key in KEYS:
        lines.extend([f'nsb7_ussd_input: key={key} state=1 t={second}.000000000',
                      f'nsb7_ussd_input: key={key} state=0 t={second}.200000000'])
        if second == 32:
            lines.extend([
                'gsm_ss: network_initiated handset_response message=3a length=21 data=8b3a12a210020101300b02013c300604010f040132 t=32.2',
                'gsm_ss: network_result transaction=8b invoke=1 dcs=0f packed_length=1 empty=0 t=32.2',
                'GSM service downlink kind=27 sapi=0 pd=0b message=2a length=2 t=32.3',
                'LAPDm service Channel Release acknowledged',
            ])
    lines.append('nsb7_ussd: complete')
    return '\n'.join(lines)


def host_fixture():
    text=fixture()
    def shift(match):
        second=int(match[1])
        return f't={second if second==24 else second+4}.{match[2]}'
    text=re.sub(r't=(\d+)\.(000000000|200000000)',shift,text)
    prefix=('gsm_call_adapter: incoming ussd id=1 result=accepted t=24.010000\n'
            'gsm_call_adapter: incoming ussd state id=1 epoch=1 phase=queued\n')
    return prefix+text.replace('LAPDm service Channel Release acknowledged\n',
        'LAPDm service Channel Release acknowledged\n'
        'gsm_call_adapter: incoming ussd reply id=1 epoch=1 dcs=0f data=32 empty=0\n'
        'gsm_call_adapter: incoming ussd state id=1 epoch=1 phase=answered\n')


class NetworkUssdTests(unittest.TestCase):
    def verify(self, text, final=b'card', host=False, empty_reply=False):
        with patch('tools.nsb7_network_ussd_request_check.check_source'), \
                patch('tools.nsb7_network_ussd_request_check.check_authentication'):
            check(text, b'card', final, host, empty_reply)

    def test_empty_host_result_preserves_absent_parameters(self):
        text=host_fixture()
        text=text.replace('nsb7_ussd_input: key=Keypad 2 state=1 t=34.000000000',
                          'nsb7_ussd_input: key=End state=1 t=34.000000000')
        text=text.replace('nsb7_ussd_input: key=Keypad 2 state=0 t=34.200000000',
                          'nsb7_ussd_input: key=End state=0 t=34.200000000')
        text=re.sub(r'nsb7_ussd_input: key=Left Softkey / Menu state=[01] t=36\.[0-9]+\n','',text)
        text=text.replace('length=21 data=8b3a12a210020101300b02013c300604010f040132',
                          'length=8 data=8b3a05a203020101')
        text=text.replace('dcs=0f packed_length=1 empty=0','dcs=00 packed_length=0 empty=1')
        text=text.replace('dcs=0f data=32 empty=0','dcs=00 data= empty=1')
        self.verify(text,host=True,empty_reply=True)
        for old,new in [('data= empty=1','data=32 empty=1'),
                        ('data=8b3a05a203020101','data=8b3a05a203020102'),
                        ('packed_length=0 empty=1','packed_length=1 empty=0')]:
            with self.subTest(old=old),self.assertRaises(ValueError):
                self.verify(text.replace(old,new),host=True,empty_reply=True)

    def test_host_response_follows_release(self):
        self.verify(host_fixture(),host=True)
        for old,new in [('reply id=1 epoch=1','reply id=1 epoch=2'),
                        ('data=32 empty=0','data=33 empty=0'),
                        ('phase=answered','phase=delivered')]:
            with self.subTest(old=old),self.assertRaises(ValueError):
                self.verify(host_fixture().replace(old,new),host=True)

    def test_physical_reply(self):
        self.verify(fixture())

    def test_mutations_fail(self):
        original = fixture()
        for old, new in [('length=29', 'length=32'), ('data=8b3a', 'data=1b3a'),
                         ('transaction=8b invoke=1', 'transaction=8b invoke=2'),
                         ('packed_length=1', 'packed_length=2'),
                         ('t=32.000000000', 't=33.000000000'),
                         ('LAPDm service Channel Release acknowledged', 'missing'),
                         ('nsb7_ussd: complete', 'missing')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                self.verify(original.replace(old, new))

    def test_card_change_and_duplicate_fail(self):
        with self.assertRaises(ValueError):
            self.verify(fixture(), b'changed')
        with self.assertRaises(ValueError):
            self.verify(fixture() + '\ngsm_ss: network_result duplicate')


def state_fixture(host=False):
    text = 'gsm_ss: network_initiated operation=request transaction=0b\nnsb7_state: event=saved\n'
    if host:
        text='gsm_call_adapter: incoming ussd state id=1 epoch=1 phase=queued\n'+text
    for phase in ('reference', 'restored'):
        text += f'state_replay: phase={phase} event=begin\n'
        epoch=1 if phase=='reference' else 2
        if host and phase=='restored':
            text+='gsm_call_adapter: incoming ussd state id=1 epoch=2 phase=queued\n'
        for key, second in (('Keypad 2', '29'), ('Left Softkey / Menu', '30')):
            for state, fraction in ((1, '750000000'), (0, '950000000')):
                text += f'nsb7_ussd_replay: phase={phase} key={key} state={state} t={int(second)+(4 if host else 0)}.{fraction}\n'
        text += ('gsm_ss: network_initiated handset_response message=3a length=21 data=8b3a12a210020101300b02013c300604010f040132 t=31\n'
                 'gsm_ss: network_result transaction=8b invoke=1 dcs=0f packed_length=1 empty=0 t=31\n'
                 'GSM service downlink kind=27 sapi=0 pd=0b message=2a length=2 t=31\n'
                 'LAPDm service Channel Release acknowledged\n')
        if host:
            text+=f'gsm_call_adapter: incoming ussd reply id=1 epoch={epoch} dcs=0f data=32 empty=0\n'
            text+=f'gsm_call_adapter: incoming ussd state id=1 epoch={epoch} phase=answered\n'
        text += f'state_replay: phase={phase} event=end\n'
    return text + f'nsb7_ussd_state_input: key=End state=1 t={40 if host else 36}.000000000\n'


class NetworkUssdStateTests(unittest.TestCase):
    def verify(self, text, final=b'card', host=False):
        with patch('tools.nsb7_network_ussd_request_check.check_source'), \
                patch('tools.nsb7_network_ussd_request_check.check_authentication'), \
                patch('tools.nsb7_network_ussd_request_check.check_architecture'):
            check_state(text, b'card', final, host)

    def test_replayed_reply(self):
        self.verify(state_fixture())

    def test_host_replay_epochs_and_reply_inventory(self):
        self.verify(state_fixture(True),host=True)
        for old,new in [('reply id=1 epoch=2','reply id=1 epoch=1'),
                        ('epoch=2 phase=queued','epoch=1 phase=queued'),
                        ('data=32 empty=0','data=33 empty=0')]:
            with self.subTest(old=old),self.assertRaises(ValueError):
                self.verify(state_fixture(True).replace(old,new),host=True)
        with self.assertRaises(ValueError):
            self.verify(state_fixture(True)+'gsm_call_adapter: incoming ussd reply id=1 epoch=2\n',host=True)

    def test_missing_reply_contact_release_or_recovery_fails(self):
        for old, new in [('key=Keypad 2', 'key=Keypad 3'), ('transaction=8b invoke=1', 'transaction=8b invoke=2'),
                         ('Channel Release acknowledged', 'missing'), ('t=36.000000000', 't=37.000000000')]:
            with self.subTest(old=old), self.assertRaises(ValueError):
                self.verify(state_fixture().replace(old, new))

    def test_duplicate_request_and_card_change_fail(self):
        with self.assertRaises(ValueError):
            self.verify(state_fixture() + 'gsm_ss: network_initiated operation=request duplicate')
        with self.assertRaises(ValueError):
            self.verify(state_fixture(), b'changed')


if __name__ == '__main__':
    unittest.main()

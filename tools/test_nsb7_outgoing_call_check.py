"""Positive and negative NSB-7 outgoing signalling fixtures."""
import unittest
import re

from tools.nsb7_outgoing_call_check import check, check_inputs


def fixture():
    steps = [(20, 'Keypad 5'), (21, 'Keypad 5'), (22, 'Keypad 5'),
             (23, 'Keypad 1'), (24, 'Keypad 2'), (25, 'Keypad 3'),
             (26, 'Keypad 4'), (28, 'Send'), (40, 'End')]
    lines = [f'nsb7_outgoing_call_input: key={key} state={state} t={second}.{fraction}'
             for second, key in steps[:-1]
             for state, fraction in [('1', '000000000'), ('0', '200000000')]]
    lines += ['GSM service establish sapi=0 pd=05 message=24 length=16 data=05247103205881080910101032547698',
              'GSM service downlink kind=5 sapi=0 pd=05 message=21 length=2',
              'GSM service uplink sapi=0 pd=03 message=05 length=18 data=03450404600201805e0581551532f4150101',
              'GSM service downlink kind=10 sapi=0 pd=03 message=02 length=2',
              'GSM service downlink kind=14 sapi=0 pd=06 message=2e length=8',
              'GSM service uplink sapi=0 pd=06 message=29 length=3',
              'GSM service downlink kind=11 sapi=0 pd=03 message=01 length=2',
              'GSM service downlink kind=12 sapi=0 pd=03 message=07 length=2',
              'GSM service uplink sapi=0 pd=03 message=0f length=2',
              'nsb7_outgoing_call_input: key=End state=1 t=40.000000000',
              'GSM service uplink sapi=0 pd=03 message=25 length=5',
              'GSM service downlink kind=25 sapi=0 pd=03 message=2d length=2',
              'GSM service uplink sapi=0 pd=03 message=2a length=2',
              'LAPDm service Channel Release acknowledged',
              'nsb7_outgoing_call_input: key=End state=0 t=40.200000000',
              'nsb7_radio_tx: index=86 type=02 length=20 data=0412020200000016600002580000000001297000',
              'nsb7_radio_tx: index=88 type=57 length=4 data=03050000',
              'nsb7_outgoing_call_done: t=50.000000000']
    return '\n'.join(lines)


class OutgoingCallTest(unittest.TestCase):
    def test_explicit_setup_transfer_requires_channel_and_request_identity(self):
        text=fixture()
        setup=next(line for line in text.splitlines() if 'message=05' in line)
        body='03450404600201805e0581551532f4150101'
        request='GSM outgoing request id=1 digits=5551234'
        text=text.replace(setup,'nsb7_radio_tx: index=70 type=1b length=28 data=0080013049'+body+'2b2b000000\n'+setup+'\n'+request,1)
        connect='GSM service downlink kind=12 sapi=0 pd=03 message=07 length=2'
        text=text.replace(connect,'nsb7_radio_tx: index=78 type=1b length=28 data=00b0012249'+body+'2b2b000000\n'+setup+'\n'+request+'\n'+connect)
        check(text,setup_transfer=True)
        with self.assertRaises(ValueError):
            check(text)
        with self.assertRaises(ValueError):
            check(text.replace('data=00b0012249','data=0080012249'),setup_transfer=True)
        last=text.rfind(request)
        with self.assertRaises(ValueError):
            check(text[:last]+text[last:].replace('id=1','id=2',1),setup_transfer=True)

    def test_explicit_five_digit_fixture(self):
        steps=[(20+i,'Keypad '+digit) for i,digit in enumerate('12346')]+[(28,'Send')]
        prefix='\n'.join(f'nsb7_outgoing_call_input: key={key} state={edge} t={second}.{fraction}'
                         for second,key in steps for edge,fraction in [('1','000000000'),('0','200000000')])
        text=prefix+'\nGSM service establish'+fixture().split('GSM service establish',1)[1]
        text=text.replace('length=18 data=03450404600201805e0581551532f4150101',
                          'length=17 data=03450404600201805e04812143f6150101')
        check(text,digits='12346')
        with self.assertRaises(ValueError):
            check(text,digits='12347')

    def test_dial_fixture_domain(self):
        for digits in ('','12345678','12*34','１２３'):
            with self.subTest(digits=digits),self.assertRaises(ValueError):
                check_inputs(fixture(),digits=digits)

    def test_explicit_physical_delay(self):
        text = re.sub(r't=(\d+)\.', lambda m: f't={int(m[1])+8}.', fixture())
        check(text, delay=8)
        with self.assertRaises(ValueError):
            check(text)

    def test_signalling_without_speech(self):
        check(fixture())

    def test_missing_checkpoints(self):
        for token in ['key=Send', 'message=24', 'message=21', 'message=05',
                      'message=02', 'message=2e', 'message=29', 'message=01',
                      'message=07', 'message=0f', 'key=End', 'message=25',
                      'message=2d', 'message=2a', 'Release acknowledged',
                      'type=02', 'type=57', 't=50.']:
            with self.subTest(token=token), self.assertRaises(ValueError):
                check(fixture().replace(token, 'incorrect'))

    def test_wrong_called_number(self):
        with self.assertRaises(ValueError):
            check(fixture().replace('5e0581551532f4', '5e0581551532f5'))

    def test_duplicate_setup(self):
        setup = next(line for line in fixture().splitlines() if 'message=05' in line)
        with self.assertRaises(ValueError):
            check(fixture() + '\n' + setup)

    def test_incoming_page_is_not_outgoing_evidence(self):
        with self.assertRaises(ValueError):
            check(fixture() + '\nPCH IMSI page transmitted')


if __name__ == '__main__':
    unittest.main()

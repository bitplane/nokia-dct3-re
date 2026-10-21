"""Qualify NSB-7 physical outgoing call signalling, without a speech claim."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb7_radio_check import contract
from tools.radio_call_lifecycle_common import require_ordered
from tools import radio_outgoing_call_trace_check as call


def check_inputs(text, delay=0, digits='5551234'):
    if not isinstance(delay, int) or delay < 0:
        raise ValueError('physical input delay must be a nonnegative integer')
    if '[LUA ERROR]' in text:
        raise ValueError('physical fixture failed')
    if not re.fullmatch(r'[0-9]{1,7}', digits):
        raise ValueError('physical dial digits must be one to seven ASCII digits')
    steps = [(20+i, 'Keypad '+digit) for i,digit in enumerate(digits)] + [(28, 'Send'), (40, 'End')]
    rows = list(re.finditer(r'nsb7_outgoing_call_input: key=(.*?) state=([01]) t=([0-9.]+)', text))
    expected = [(key, state, f'{second+delay}.{fraction}') for second, key in steps
                for state, fraction in [('1', '000000000'), ('0', '200000000')]]
    if [row.groups() for row in rows] != expected:
        raise ValueError('physical dial/Send/End sequence differs')
    return rows


def check(text, delay=0, digits='5551234', setup_transfer=False):
    check_inputs(text, delay, digits)
    require_ordered(text, [
        ('physical Send', rf'key=Send state=1 t={28+delay}\.'),
        ('CM Service Request', call.CM_SERVICE_REQUEST),
        ('CM Service Accept', call.CM_SERVICE_ACCEPT),
        ('SETUP', call.SETUP), ('Call Proceeding', call.CALL_PROCEEDING),
        ('traffic assignment', call.TRAFFIC_ASSIGNMENT),
        ('Assignment Complete', call.ASSIGNMENT_COMPLETE),
        ('Alerting', call.ALERTING), ('Connect', call.CONNECT),
        ('Connect Acknowledge', call.CONNECT_ACKNOWLEDGE),
        ('physical End', rf'key=End state=1 t={40+delay}\.'),
        ('Disconnect', call.DISCONNECT), ('Release', call.RELEASE),
        ('Release Complete', call.RELEASE_COMPLETE),
        ('RR release', call.RR_RELEASE),
        ('common control', r'nsb7_radio_tx: .*type=02 length=20 data=0412020200000016600002580000000001297000'),
        ('background measurement', r'nsb7_radio_tx: .*type=57 length=4 data=03050000'),
        ('observation complete', rf'nsb7_outgoing_call_done: t={50+delay}\.'),
    ], 'NSB-7 mobile-originated signalling')
    setups = list(call.SETUP.finditer(text))
    if len(setups) != (2 if setup_transfer else 1) or len(call.TRAFFIC_ASSIGNMENT.findall(text)) != 1:
        raise ValueError('unexpected SETUP or traffic-assignment inventory')
    setup = call.SETUP.search(text)
    body = bytes.fromhex(setup.group('data'))
    if len(body) != int(setup.group('length')) or call.decode_called_digits(body) != digits:
        raise ValueError('SETUP length or called number differs')
    if setup_transfer:
        if setups[1].group('data') != setups[0].group('data') or setups[1].group('length') != setups[0].group('length'):
            raise ValueError('traffic-channel SETUP differs from the original')
        require_ordered(text,[
            ('SDCCH SETUP packet',r'nsb7_radio_tx: [^\n]*type=1b length=28 data=0080[0-9a-f]{6}'+body.hex()),
            ('original SETUP',call.SETUP), ('traffic assignment',call.TRAFFIC_ASSIGNMENT),
            ('Assignment Complete',call.ASSIGNMENT_COMPLETE),
            ('TCH SETUP packet',r'nsb7_radio_tx: [^\n]*type=1b length=28 data=00b0[0-9a-f]{6}'+body.hex()),
            ('transferred SETUP',call.SETUP), ('Connect',call.CONNECT),
        ],'NSB-7 SETUP transfer across assignment')
        requests = re.findall(r'GSM outgoing request id=(\d+) digits='+re.escape(digits)+r'\b',text)
        if len(requests)!=2 or requests[0]!=requests[1]:
            raise ValueError('SETUP transfer changed the network request identity')
    service = call.CM_SERVICE_REQUEST.search(text)
    if len(bytes.fromhex(service.group('data'))) != int(service.group('length')):
        raise ValueError('CM Service Request length differs')
    if 'PCH IMSI page transmitted' in text:
        raise ValueError('outgoing fixture must not be initiated by an incoming page')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('run', type=Path)
    parser.add_argument('--delay', type=int, default=0, help='physical input delay, not emulation timing')
    args = parser.parse_args()
    try:
        contract(args.image.read_bytes())
        check((args.run / 'error.log').read_text(), delay=args.delay)
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSB-7 outgoing call: FAIL: {error}\n')
    print('PASS: physical dial/Send, outgoing call signalling, physical End and common-control return; speech unproven')


if __name__ == '__main__':
    main()

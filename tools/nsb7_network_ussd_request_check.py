"""Physical network-originated laboratory USSD reply on research-HLE NSB-7."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb7_pin_services_check import check_source, check_authentication
from tools.nsb7_pin_check import check_frame
from tools.nsb7_radio_check import contract
from tools.nsb7_storage_check import inspect
from tools.radio_call_lifecycle_common import require_ordered
from tools.run_sip_handset_gate import verify_handset_health
from tools.nsb7_state_check import check_architecture

FRAMES = {
    25: 'bc5b245d9301869ea5539cfaa5b2e9762f42429781752c38466f47bff7bf0262',
    29: '4dd885abe2b46c973d8c660c62a63fb27606de0a4a68c0382ff4ac404b1fb850',
    31: 'b2d19830b1228edddf5ad118e2007fc0a07cf5aec2c21443da07489f104be229',
    33: '2ce2ca567aae64073fb72d9bce0b6cbe486313f8db217dffb31efa7220f813bf',
    37: '32c41625e02a87a0ec97ec98a250040fc87ad8c5722c78200c54b27f7de2d560',
    39: '76728306449ad0f2ae8cbdbd1fc5e880d3dcff054c955e8b4a2aabc87db8e7e4',
}
KEYS = [(24, 'End'), (26, 'Left Softkey / Menu'), (28, 'Left Softkey / Menu'),
        (30, 'Keypad 2'), (32, 'Left Softkey / Menu'), (36, 'End'),
        (38, 'Left Softkey / Menu')]


def check(text, initial, final):
    check_source(initial)
    check_authentication(text.split('gsm_ss: network_initiated ', 1)[0])
    if initial != final:
        raise ValueError('network USSD changed persistent card contents')
    for state, fraction in ((1, '000000000'), (0, '200000000')):
        observed = re.findall(rf'nsb7_ussd_input: key=(.*?) state={state} t=(\d+)\.{fraction}', text)
        if observed != [(key, str(second)) for second, key in KEYS]:
            raise ValueError('network USSD physical contact sequence differs')
    require_ordered(text, [
        ('page', r'PCH IMSI page transmitted channel=60 fn='),
        ('Paging Response', r'GSM service establish sapi=0 pd=06 message=27 length=16'),
        ('request', r'gsm_ss: network_initiated operation=request transaction=0b invoke=1 dcs=0f'),
        ('REGISTER', r'GSM service downlink kind=32 sapi=0 pd=0b message=3b length=29 '),
        ('physical Send', r'nsb7_ussd_input: key=Left Softkey / Menu state=1 t=32\.'),
        ('reply', r'handset_response message=3a length=21 data=8b3a12a210020101300b02013c300604010f040132 '),
        ('correlation', r'network_result transaction=8b invoke=1 dcs=0f packed_length=1 empty=0 '),
        ('release', r'GSM service downlink kind=27 sapi=0 pd=0b message=2a length=2 '),
        ('RR release', r'LAPDm service Channel Release acknowledged'),
        ('idle recovery', r'nsb7_ussd_input: key=End state=1 t=36\.'),
        ('Menu', r'nsb7_ussd_input: key=Left Softkey / Menu state=1 t=38\.'),
        ('complete', r'nsb7_ussd: complete'),
    ], 'NSB-7 network USSD')
    if text.count('gsm_ss: network_initiated operation=request ') != 1 or text.count('gsm_ss: network_result ') != 1:
        raise ValueError('network USSD transaction duplicated')
    if 'sim_device: update ' in text:
        raise ValueError('network USSD unexpectedly writes card')


def check_state(text, initial, final):
    check_source(initial)
    check_authentication(text.split('nsb7_state: event=saved', 1)[0])
    if initial != final or 'sim_device: update ' in text:
        raise ValueError('restored network USSD changed persistent card')
    check_architecture(text, 'ussd', ('TX packet', 'RX enqueue', 'GSM service',
                                    'sim_device:', 'ccont_rtc:', 'ccont_analog:', 'gsm_ss:'))
    if text.count('gsm_ss: network_initiated operation=request ') != 1:
        raise ValueError('network request duplicated across restoration')
    if 'network_result ' in text.split('nsb7_state: event=saved', 1)[0]:
        raise ValueError('USSD must remain pending at save')
    for phase in ('reference', 'restored'):
        begin = text.index(f'state_replay: phase={phase} event=begin')
        end = text.index(f'state_replay: phase={phase} event=end', begin)
        interval = text[begin:end]
        require_ordered(interval, [
            ('digit', rf'nsb7_ussd_replay: phase={phase} key=Keypad 2 state=1 t=29\.750000000'),
            ('digit release', rf'nsb7_ussd_replay: phase={phase} key=Keypad 2 state=0 t=29\.950000000'),
            ('Send', rf'nsb7_ussd_replay: phase={phase} key=Left Softkey / Menu state=1 t=30\.750000000'),
            ('Send release', rf'nsb7_ussd_replay: phase={phase} key=Left Softkey / Menu state=0 t=30\.950000000'),
            ('reply', r'handset_response message=3a length=21 data=8b3a12a210020101300b02013c300604010f040132 '),
            ('correlation', r'network_result transaction=8b invoke=1 dcs=0f packed_length=1 empty=0 '),
            ('release', r'GSM service downlink kind=27 sapi=0 pd=0b message=2a length=2 '),
            ('RR release', r'LAPDm service Channel Release acknowledged'),
        ], f'NSB-7 USSD {phase} replay')
        if interval.count('gsm_ss: network_result ') != 1:
            raise ValueError('restored USSD completion duplicated')
    if 'nsb7_ussd_state_input: key=End state=1 t=36.000000000' not in text:
        raise ValueError('missing restored physical idle recovery')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('run', type=Path)
    parser.add_argument('--state', action='store_true', help='pending reply editor save/load replay')
    args = parser.parse_args()
    try:
        contract(args.image.read_bytes())
        verify_handset_health(Path(str(args.run) + '-launch.log').read_text())
        storage = args.run / 'nvram/nsb7hle'
        inspect((storage / 'flash').read_bytes())
        (check_state if args.state else check)((args.run / 'error.log').read_text(), (args.run / 'initial_sim_card.bin').read_bytes(),
              (storage / 'sim_card').read_bytes())
        if args.state:
            for phase in ('reference', 'restored'):
                check_frame(args.run / f'snap/nsb7_state_ussd_{phase}.png', FRAMES[33])
            check_frame(args.run / 'snap/nsb7_state_ussd_final.png', FRAMES[37])
            for second in (25, 29):
                check_frame(args.run / f'snap/nsb7_ussd_response_{second}.png', FRAMES[second])
        else:
            for second, digest in FRAMES.items():
                check_frame(args.run / f'snap/nsb7_ussd_response_{second}.png', digest)
    except (OSError, ValueError, RuntimeError) as error:
        parser.exit(1, f'NSB-7 network USSD: FAIL: {error}\n')
    print('PASS: network USSD physical reply, correlated release, exact idle/Menu and unchanged card; research HLE only')


if __name__ == '__main__':
    main()

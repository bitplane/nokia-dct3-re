"""Coherent PIN-authenticated outgoing call and incoming/read/reply SMS."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb7_radio_check import contract, check_physical_call
from tools.nsb7_pin_registration_check import check_registered, LOCI, request
from tools.nsb7_outgoing_call_check import check as check_call
from tools.nsb7_outgoing_sms_check import check as check_submit, check_frame as sent_frame
from tools.nsb7_sms_check import check_read, check_read_frame, tx_bytes
from tools.nsb7_netmonitor_check import frame, IDLE_HASH
from tools.nsb7_pin_check import serialized_profile
from tools.nsb7_storage_check import inspect
from tools.radio_sms_acceptance_common import FIRST_SMS_DELIVER_BODY, SMS_NVRAM_OFFSET
from tools.sim_security_trace_check import CURRENT_NVRAM_SIZE

SMSS_OFFSET = CURRENT_NVRAM_SIZE-2


def check_authentication(text):
    end = re.search(r'nsb7_security_input: key=Left Softkey / Menu state=0 t=21\.200000000', text)
    if not end:
        raise ValueError('complete physical phone-code input absent')
    startup = text.split('nsb7_radio_tx: index=58 ', 1)[0]
    # Authentication completes before service file writes, while the first
    # service packet may precede the last phone-code key. Keep both scopes raw.
    check_registered(startup, cold=True, require_complete=False, authentication_text=text[:end.end()])


def check_source(card):
    expected = bytearray(serialized_profile())
    expected[1600:1611] = LOCI
    if card != bytes(expected):
        raise ValueError('service input must be the own PIN-enabled registered card')


def check_incoming(text, card):
    check_source(card)
    check_authentication(text)
    check_physical_call(text, card, startup_offset=3, request=request(True), armed_second=21)
    for key, code in [('Send', 0x0e), ('End', 0x0f)]:
        press = text.index(f'nsb7_call_input: key={key} state=1 ')
        release = text.index(f'nsb7_call_input: key={key} state=0 ')
        if f'nsb7_key_decode: key={code:02x} observer_writes=0' not in text[press:release]:
            raise ValueError('incoming Answer/End contact did not decode')


def check_sms(text, initial, final):
    check_submit(text, delay=4)
    check_sms_storage(text,initial,final)


def check_sms_storage(text, initial, final, *, update_reference=True):
    check_source(initial)
    delivered = bytearray(initial)
    record = b'\x03'+FIRST_SMS_DELIVER_BODY
    record += b'\xff'*(176-len(record))
    delivered[SMS_NVRAM_OFFSET:SMS_NVRAM_OFFSET+176] = record
    # Validate the read independently of the sender's success-only EF_SMSS
    # update, checked separately below. This is an artifact projection, not a
    # modification of the card or guest state.
    read_view = bytearray(final)
    read_view[SMSS_OFFSET:SMSS_OFFSET+2] = initial[SMSS_OFFSET:SMSS_OFFSET+2]
    check_read(text, bytes(read_view), bytes(delivered), read_times=(23,25,27,29))
    expected = bytearray(delivered)
    expected[SMS_NVRAM_OFFSET] = 1
    if update_reference: expected[SMSS_OFFSET] = 0
    if final != bytes(expected):
        raise ValueError('authenticated reply changed unexpected card contents')
    wire = tx_bytes(text)
    updates = re.findall(r'sim_device: update-binary fid=6f43 offset=0 length=2 t=([0-9.]+)', text)
    if not update_reference:
        if re.search(r'sim_device: update-binary fid=6f43\b',text) or bytes.fromhex('a0d600000200ff') in wire:
            raise ValueError('failed submission must not update EF_SMSS')
        return
    if wire.count(bytes.fromhex('a0d600000200ff')) != 1:
        raise ValueError('expected one exact EF_SMSS update')
    submit = re.search(r'gsm_sms_submit: [^\n]* t=([0-9.]+)', text)
    if len(updates) != 1 or not submit or float(updates[0]) <= float(submit[1]):
        raise ValueError('EF_SMSS update must follow the accepted submission')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('image', 'call', 'sms'):
        parser.add_argument(name, type=Path)
    parser.add_argument('--incoming', type=Path)
    args = parser.parse_args()
    try:
        contract(args.image.read_bytes())
        for run in (args.call, args.sms):
            inspect((run / 'nvram/nsb7hle/flash').read_bytes())
            check_source((run / 'initial_sim_card.bin').read_bytes())
            check_authentication((run / 'error.log').read_text())
        call = (args.call / 'error.log').read_text()
        check_call(call, delay=8)
        if (args.call / 'initial_sim_card.bin').read_bytes() != (args.call / 'nvram/nsb7hle/sim_card').read_bytes():
            raise ValueError('call changed persistent SIM data')
        frame(args.call / 'snap/nsb7_outgoing_call_34.png', '223ffbdc6b19f01641f293b0a168a65b8715c37b71a97ecc5bb972b8d15baea1')
        frame(args.call / 'snap/nsb7_outgoing_call_36.png', '067a6a8ee85b4989a4295780ab87d1fe2eccbab47271107e8550dacc02883091')
        frame(args.call / 'snap/nsb7_outgoing_call_final.png', IDLE_HASH)
        check_sms((args.sms / 'error.log').read_text(), (args.sms / 'initial_sim_card.bin').read_bytes(),
                  (args.sms / 'nvram/nsb7hle/sim_card').read_bytes())
        check_read_frame(args.sms / 'snap/nsb7_sms_read_29.png')
        sent_frame(args.sms / 'snap/nsb7_sms_reply_final_48.3.png')
        if args.incoming:
            initial = (args.incoming / 'initial_sim_card.bin').read_bytes()
            final = (args.incoming / 'nvram/nsb7hle/sim_card').read_bytes()
            inspect((args.incoming / 'nvram/nsb7hle/flash').read_bytes())
            check_source(initial)
            if final != initial:
                raise ValueError('incoming call changed persistent SIM data')
            check_incoming((args.incoming / 'error.log').read_text(), final)
            frame(args.incoming / 'snap/nsb7_call_final.png', IDLE_HASH)
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSB-7 PIN services: FAIL: {error}\n')
    print('PASS: physical PIN/cold registration precede outgoing call and SMS read/reply; exact card/pixel outcomes')


if __name__ == '__main__':
    main()

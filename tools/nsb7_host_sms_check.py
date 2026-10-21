"""Authenticated NSB-7 host SMS in both directions, with exact card/UI checks."""
import argparse
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb7_pin_services_check import check_source, check_authentication, check_sms
from tools.nsb7_sms_check import check_read, check_read_frame
from tools.nsb7_pin_check import check_frame
from tools.nsb7_radio_check import contract
from tools.nsb7_storage_check import inspect
from tools.radio_sms_acceptance_common import FIRST_SMS_DELIVER_BODY, SMS_NVRAM_OFFSET
from tools.radio_incoming_host_sms_trace_check import verify as incoming_host
from tools.radio_outgoing_host_sms_trace_check import verify as outgoing_host

COMPOSER_HASH='efd0548fdc2d839b45677831e346245eab1a74a3c511eaa76ce79353517b8d31'


def check_incoming(text, initial, final):
    check_source(initial)
    check_authentication(text.split('nsb7_sms_input:',1)[0])
    incoming_host(text,arfcn=600)
    if text.count('GSM service downlink kind=16 sapi=3 pd=09 message=01 ') != 1:
        raise ValueError('expected exactly one host-originated SMS delivery')
    delivered=bytearray(initial)
    record=b'\x03'+FIRST_SMS_DELIVER_BODY
    record+=b'\xff'*(176-len(record))
    delivered[SMS_NVRAM_OFFSET:SMS_NVRAM_OFFSET+176]=record
    check_read(text,final,bytes(delivered),read_times=(23,25,27,29))


def check_outgoing(text, initial, final):
    check_sms(text,initial,final)
    outgoing_host(text,octets=1)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('image','incoming','outgoing'):
        parser.add_argument(name,type=Path)
    args=parser.parse_args()
    try:
        contract(args.image.read_bytes())
        for run,check in ((args.incoming,check_incoming),(args.outgoing,check_outgoing)):
            storage=run/'nvram/nsb7hle'
            inspect((storage/'flash').read_bytes())
            check((run/'error.log').read_text(),(run/'initial_sim_card.bin').read_bytes(),
                  (storage/'sim_card').read_bytes())
            check_read_frame(run/'snap/nsb7_sms_read_29.png')
        check_frame(args.outgoing/'snap/nsb7_sms_reply_final_74.png',COMPOSER_HASH)
    except (OSError,ValueError) as error:
        parser.exit(1,f'NSB-7 host SMS: FAIL: {error}\n')
    print('PASS: physical PIN/registration, host-originated hello/read and host-decided A/5551234 Reply; exact storage/UI')


if __name__=='__main__':
    main()

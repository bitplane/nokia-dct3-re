"""PIN-gated fresh/cold lab registration, with a long wrong-PIN control."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.nsb7_radio_check import contract, PACKET, check_location_update
from tools.nsb7_pin_check import check_trace as check_pin, serialized_profile
from tools.nsb7_security_check import check_trace as check_phone
from tools.nsb7_netmonitor_check import frame, IDLE_HASH
from tools.nsb7_sms_check import tx_bytes
from tools.make_sim_card_profile import make_profile
from tools.sim_security_trace_check import ATTEMPTS_OFFSET
from tools.nsb7_storage_check import inspect

LOCI = bytes.fromhex('ffffffff00f1100001ff00')


def request(cold):
    return bytes.fromhex('05087200f110000120080910101032547698' if cold else
                         '05087000f000fffe20080910101032547698')


def check_registered(text, cold=False, require_complete=True, authentication_text=None):
    # Service fixtures scope radio startup separately from later file writes.
    authentication = authentication_text if authentication_text is not None else text
    check_pin(authentication, True)
    check_phone(authentication)
    if require_complete and 'nsb7_pin_registration: complete' not in text:
        raise ValueError('authenticated observation incomplete')
    rows = PACKET.findall(text)
    if [int(row[0]) for row in rows] != list(range(1, 58)):
        raise ValueError('authenticated TX inventory differs')
    packets = {int(row[0]): (row[1], bytes.fromhex(row[3])) for row in rows}
    expected = request(cold)
    if packets[45] != ('1b', bytes.fromhex('0080013f49') + expected + bytes.fromhex('2b2b000000')):
        raise ValueError('fresh/retained-location request differs')
    marker = 'GSM service establish sapi=0 pd=05 message=08 length=18 data='+expected.hex()
    if text.count('GSM service establish sapi=0 pd=05 message=08') != 1 or marker not in text:
        raise ValueError('expected one own Location Updating Request')
    if text.index('SIM status ins=20 sw=9000') >= text.index(marker):
        raise ValueError('network request preceded successful physical PIN verification')
    # PIN-gated startup adds three earlier packets; payload/consumer checks
    # remain identical. Scope ends at the first post-release receiver return.
    check_location_update(text.split('nsb7_radio_tx: index=49 ', 1)[0], first_ack=46, request=expected)
    common = bytes.fromhex('0412020200000016600002580000000001297000')
    if [packets[i] for i in range(49, 53)] != [
            ('46', bytes.fromhex('3210321000010000')), ('02', common), ('02', common),
            ('57', bytes.fromhex('03050000'))]:
        raise ValueError('post-registration common-control/background sequence differs')
    wire = tx_bytes(text)
    if cold and b'\xa0\xd6' in wire:
        raise ValueError('retained-location fixture unexpectedly rewrote SIM files')
    if not cold and (bytes.fromhex('a0d600040500f1100001') not in wire or bytes.fromhex('a0d6000a0100') not in wire):
        raise ValueError('registered LOCI was not written over SIMI')


def check_denied(text):
    check_pin(text, False)
    if not re.search(r'ccont_rtc: [^\n]* t=45\.000000000', text):
        raise ValueError('wrong-PIN control must run through 45 seconds')
    if 'GSM service establish sapi=0 pd=05 message=08' in text or 'Location Updating Accept acknowledged' in text:
        raise ValueError('wrong PIN must not complete subscriber registration')
    if b'\xa0\xd6' in tx_bytes(text):
        raise ValueError('wrong-PIN control must not update SIM files')


def check_cards(initial_wrong, wrong, initial_fresh, fresh, initial_cold, cold):
    expected_wrong = bytearray(serialized_profile())
    expected_wrong[ATTEMPTS_OFFSET] = 2
    expected_registered = bytearray(serialized_profile())
    expected_registered[1600:1611] = LOCI
    if initial_wrong != make_profile(True) or initial_fresh != initial_wrong:
        raise ValueError('fresh positive/negative cards differ from declared PIN profile')
    if wrong != bytes(expected_wrong) or fresh != bytes(expected_registered) or initial_cold != fresh or cold != fresh:
        raise ValueError('retry/location/cold full-card state differs')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('image', 'wrong', 'fresh', 'cold'):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    try:
        contract(args.image.read_bytes())
        check_denied((args.wrong / 'error.log').read_text())
        check_registered((args.fresh / 'error.log').read_text())
        check_registered((args.cold / 'error.log').read_text(), cold=True)
        values = []
        for run in (args.wrong, args.fresh, args.cold):
            inspect((run / 'nvram/nsb7hle/flash').read_bytes())
            values += [(run / 'initial_sim_card.bin').read_bytes(),
                       (run / 'nvram/nsb7hle/sim_card').read_bytes()]
        check_cards(*values)
        for run in (args.fresh, args.cold):
            frame(run / 'snap/nsb7_pin_registered.png', IDLE_HASH)
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSB-7 PIN registration: FAIL: {error}\n')
    print('PASS: physical PIN gates fresh/cold lab registration; exact LOCI/idle; wrong PIN remains unregistered')


if __name__ == '__main__':
    main()

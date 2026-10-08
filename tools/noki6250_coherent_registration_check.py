"""Own NHM-3 uploads, declared PMM comparison and coherent lab carrier 19."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.noki6250_staged_check import check as check_uploads, check_initial_fixture
from tools.radio_registration_trace_check import verify as check_registration


def verify(text, storage, *, preserved=False, require_host=True):
    check_uploads(text, runtime=True)
    check_initial_fixture(text)
    check_registration(text, 'nhm3', preserved=preserved, configured_carrier=True)
    if require_host and not re.search(r'gsm_call_adapter: network registered=1 arfcn=19\b', text):
        raise ValueError('NHM-3 host and handset do not share carrier 19')
    if len(storage) < 1611 or storage[1604:1609] != bytes.fromhex('00f1100001') or storage[1610] != 0:
        raise ValueError('NHM-3 persistent SIM location is not laboratory-updated')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('storage', type=Path)
    parser.add_argument('--preserved', action='store_true',
                        help='require the persisted-location cold-start contract')
    args = parser.parse_args()
    try:
        with args.log.open(errors='replace') as stream:
            text = ''.join(line for line in stream if not line.startswith('[opcov]'))
        verify(text, args.storage.read_bytes(), preserved=args.preserved)
    except (OSError, ValueError) as error:
        parser.exit(1, f'6250 coherent registration FAIL: {error}\n')
    print('6250 own uploads/HLE, coherent ARFCN19 and persistent registration PASS; native speech unproved')


if __name__ == '__main__':
    main()

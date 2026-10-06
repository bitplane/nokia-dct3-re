"""Check NSM-3 physical save and cold SIM readback evidence."""

import argparse
from pathlib import Path

try:
    from tools.sim_phonebook_check import validate_phonebook_storage
except ModuleNotFoundError:
    from sim_phonebook_check import validate_phonebook_storage


def check(write_trace, read_trace, storage):
    cursor = 0
    for event in ('8210_phonebook_physical: action=save',
                  'header cla=a0 ins=dc p1=01 p2=04 p3=20 selected=6f3a',
                  'body ins=dc length=32 selected=6f3a',
                  'SIM status ins=dc sw=9000'):
        cursor = write_trace.find(event, cursor)
        if cursor < 0:
            raise ValueError(f'missing ordered save evidence: {event}')
        cursor += len(event)
    if 'header cla=a0 ins=b2 p1=01 p2=04 p3=20 selected=6f3a' not in read_trace:
        raise ValueError('cold process did not read EF_ADN record 1')
    if '8210_phonebook_read_physical: action=contact' not in read_trace:
        raise ValueError('missing physical cold contact selection')
    if 'ins=dc' in read_trace:
        raise ValueError('cold readback process unexpectedly wrote SIM storage')
    validate_phonebook_storage(storage, b'A')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('write_trace', type=Path)
    parser.add_argument('read_trace', type=Path)
    parser.add_argument('sim_nvram', type=Path)
    args = parser.parse_args()
    try:
        def events(path):
            with path.open(errors='replace') as stream:
                return ''.join(line for line in stream if
                               'phonebook_' in line or 'sim_device:' in line or
                               'SIM status' in line)
        check(events(args.write_trace), events(args.read_trace), args.sim_nvram.read_bytes())
    except (OSError, ValueError) as error:
        parser.exit(1, f'8210 phonebook FAIL: {error}\n')
    print('8210 physical SIM save and cold record readback PASS; inspect UI captures separately')


if __name__ == '__main__':
    main()

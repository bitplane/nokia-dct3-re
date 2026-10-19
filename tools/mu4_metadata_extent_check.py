"""Bind metadata EOF accounting to actual RAM writes and independent file readbacks."""
import argparse
import json
import math
from pathlib import Path
import sys

if __package__:
    from .mu4_context_contract import contract, reference_metadata_size_updates
    from .mu4_playback_frame_check import rows
    from .mu4_recorder_media_inventory import id3v23_inventory, file_inventory
    from .noki5510_bootstrap_check import verify_resident_memory
else:
    from mu4_context_contract import contract, reference_metadata_size_updates
    from mu4_playback_frame_check import rows
    from mu4_recorder_media_inventory import id3v23_inventory, file_inventory
    from noki5510_bootstrap_check import verify_resident_memory


def check(text, before, after):
    verify_resident_memory(text)
    scope = dict(first='14.9',last='20',first_address='1456',last_address='145a',cap='4096',
                 boundary='data_write',context_pc='not_owner',observer_writes='0',audio_acceptance='0')
    events, ends = rows(text,'mu4_metadata_extent_write'), rows(text,'mu4_metadata_extent_terminal')
    success = 'mu4_native_recorder_metadata: PASS native_reply=1 success=1 playback=0 firmware_state_forcing=0'
    if rows(text,'mu4_metadata_extent_scope') != [scope] or len(ends) != 1 or not events or text.count(success) != 1:
        raise ValueError('missing metadata extent scope, terminal or native success')
    if '\0' in text or 'LUA ERROR' in text:
        raise ValueError('failed or corrupt metadata observation')
    old_tag, new_tag = id3v23_inventory(before), id3v23_inventory(after)
    if old_tag['tag_payload_bytes'] != new_tag['tag_payload_bytes'] or old_tag['tag_payload_bytes'] != 100:
        raise ValueError('observation covers the original 100-byte ID3 payload only')
    expected = reference_metadata_size_updates(len(before),old_tag['tag_payload_bytes'])
    delta = file_inventory(before,after)
    if (len(after) != expected[-1]['extent'] or delta['removed_bytes'] or
            any(last > old_tag['tag_end'] for _,last in delta['changed_common_ranges_end_exclusive'])):
        raise ValueError('final file extent or common payload differs from the source model')
    try:
        end = ends[0]
        stop = float(end['t'])
        if (not len(events) == int(end['events']) < 4096 or end['capped'] != '0' or end['illegal'] != '0'
                or end['observer_writes'] != '0' or end['audio_acceptance'] != '0'
                or not math.isfinite(stop) or stop < 20):
            raise ValueError('capped, faulted or incomplete metadata observation')
        selected, previous = [],14.9
        for ordinal,event in enumerate(events,1):
            when = float(event['t'])
            address,value,pc = (int(event[key],16) for key in ('address','value','context_pc'))
            if (int(event['ordinal']) != ordinal or not previous <= when <= 20 or not math.isfinite(when)
                    or not 0x1456 <= address <= 0x145a or not 0 <= value <= 0xffff
                    or not 0 <= pc <= 0x7fffff or event['mask'] != 'ffff' or event['observer_writes'] != '0'
                    or any(not 0 <= int(event[key],16) <= 0xffff for key in ('ar1','ar7'))):
                raise ValueError('changed metadata write domain/order')
            previous = when
            if address in (0x1456,0x1457) and 0x3c3f6 <= pc <= 0x3c404:
                selected.append(event)
        if len(selected) != 2*len(expected):
            raise ValueError('extent write pairs differ from the complete metadata write plan')
        for index,step in enumerate(expected):
            pair = selected[2*index:2*index+2]
            values = {int(event['address'],16): int(event['value'],16) for event in pair}
            if set(values) != {0x1456,0x1457} or values[0x1456]*65536+values[0x1457] != step['extent']:
                raise ValueError('actual extent write differs from the unfitted source progression')
        if text.index('mu4_metadata_extent_terminal:') < text.rindex('mu4_metadata_extent_write:'):
            raise ValueError('metadata terminal precedes writes')
    except (KeyError,TypeError,OverflowError) as error:
        raise ValueError('incomplete metadata extent fields') from error
    return dict(observed_writes=len(events),accounting_updates=len(expected),
                initial_extent=len(before),final_extent=len(after),growth_bytes=len(after)-len(before),
                clear_phase_extent=expected[-3]['extent'],header_phase_extent=expected[-2]['extent'],
                file_delta=delta,first_update_t=selected[0]['t'],last_update_t=selected[-1]['t'],
                scope='Source-model agreement with actual RAM extent writes and independent file deltas; '
                      'PCs/AR values are context only; not intended padding, file-flush, retired-instruction or audio acceptance')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log',type=Path)
    parser.add_argument('before_file',type=Path)
    parser.add_argument('after_file',type=Path)
    parser.add_argument('--source-a00',type=Path,required=True)
    args = parser.parse_args()
    try:
        contract(args.source_a00.read_bytes())
        report = check(args.log.read_text(),args.before_file.read_bytes(),args.after_file.read_bytes())
    except (ValueError,OSError) as error:
        parser.exit(1,f'MU4 metadata extent: {error}\n')
    print(json.dumps(report,indent=2))


if __name__ == '__main__':
    main()

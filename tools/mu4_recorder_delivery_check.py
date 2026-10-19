"""Check original recording buffer offers and consumer acknowledgements."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

if __package__:
    from .mu4_recorder_tail_check import source_contract
    from .mu4_recorder_full_transform_check import records, verify_file
    from .noki5510_a00_inventory import uploaded_word_image
else:
    from mu4_recorder_tail_check import source_contract
    from mu4_recorder_full_transform_check import records, verify_file
    from noki5510_a00_inventory import uploaded_word_image


DELIVERY_RANGES = (
    (0x38a31,0x38ad1,'13806faef60fe3a3b8339a7443c005ab4dd706025ba2a8597e21e26d7a185163'),
    (0x38b43,0x38b55,'5952df7dda6576a17321aedc6f05622d1f881bfa30c1c114b9027f4b06a2446d'),
    (0x38b74,0x38b8b,'df0dcb3f42e02e074f6a747aea30d23c2625be907ad3be92e07d130fc6ee238e'),
    (0x38bbf,0x38bd7,'81153e02f7ba4af8a88fcb6f39a61673927d1af6a48c78234723051de16b9e84'),
    (0x55b4,0x55b7,'218eb671e2367c3dc65d0c36d52758b09437d69916b884f0f9a69530a3309ad9'),
    (0x3eb4d,0x3eb85,'7e3215e57027bf09ec51713ba82d040ce72c97a47480cf94b8d9e0ae9e55d690'),
)


def source_delivery(source):
    report = source_contract(source)
    words = uploaded_word_image(bytes.fromhex(source.decode('ascii')), 'aa88')
    for start,end,expected in DELIVERY_RANGES:
        payload = b''.join(words[address].to_bytes(2,'big') for address in range(start,end+1))
        if hashlib.sha256(payload).hexdigest() != expected:
            raise ValueError('original delivery source changed')
    report['delivery_pinned_words'] = sum(end-start+1 for start,end,_ in DELIVERY_RANGES)
    report['buffer_callback'] = dict(slot='55b4..55b7', target='03eb4d',
                                   selected_call='03eb82 -> 03ee2b',
                                   scope='In-place transform callback, not a file-write acknowledgement')
    return report


def offer_words(unread):
    if not 0 <= unread <= 0xffff:
        raise ValueError('unread count outside word domain')
    signed = unread if unread < 0x8000 else unread-0x10000
    return 0 if signed <= 256 else 512 if signed > 512 else 256


def verify(text, *, persisted_bytes, native_bytes=None):
    terminal = records(text, 'mu4_recorder_delivery_terminal')
    posts = records(text, 'mu4_recorder_delivery_post')
    consumes = records(text, 'mu4_recorder_delivery_consume')
    if len(terminal) != 1 or not posts or not consumes:
        raise ValueError('missing delivery capture or terminal')
    for name,rows in (('posts',posts),('consumes',consumes)):
        if int(terminal[0][name]) != len(rows) or len(rows) >= int(terminal[0]['cap']):
            raise ValueError('incomplete or capped delivery capture')
        if [int(row['ordinal']) for row in rows] != list(range(1,len(rows)+1)):
            raise ValueError('missing or reordered delivery events')
    events = sorted([(int(row['cycles'],16),'post',row) for row in posts]+
                    [(int(row['cycles'],16),'consume',row) for row in consumes],key=lambda event:event[0])
    previous, pending, data_posts, words, controls = -1, None, 0, 0, 0
    sizes = {}
    for cycle,kind,row in events:
        if cycle <= previous:
            raise ValueError('ambiguous delivery chronology')
        previous = cycle
        if kind == 'post':
            selector, length = int(row['selector'],16), int(row['length'],16)
            if selector == 0x16 and length == 2:
                controls += 1
                continue
            if selector != 0x21 or length != 5 or pending is not None:
                raise ValueError('unexpected or overlapping buffer offer')
            ed,ee,count = (int(row[key],16) for key in ('ed','ee','words'))
            if not 0 <= ee <= ed < 0x8000 or count != offer_words(ee) or not count:
                raise ValueError('offer differs from original threshold selection')
            if row['ea'] != '0001' or row['f0'] != '0001' or row['f3'] != '0000':
                raise ValueError('offer lifecycle flags differ')
            if int(row['source'],16) != 0x61c+ed-ee:
                raise ValueError('offer does not name original unread buffer')
            pending = count
            sizes[count] = sizes.get(count,0)+1
            data_posts += 1
        else:
            before,amount,after = (int(row[key],16) for key in ('before','amount','after'))
            if (row['instruction'] not in ('038b50','038b87') or row['f0'] != '0001'
                    or pending != amount or amount > before or after != before-amount):
                raise ValueError('consumer does not acknowledge the pending word count')
            words += amount
            pending = None
    if pending is not None or words*2 != persisted_bytes:
        raise ValueError('acknowledged extent differs from persisted native prefix')
    stops = [row for row in posts if row['selector'] == '0016' and row.get('source') == '0000']
    if len(stops) != 1 or stops[0] is not posts[-1]:
        raise ValueError('missing final stop-control publication')
    stop = stops[0]
    unread = int(stop['ee'],16)
    if (stop['ea'] != '0000' or stop['f0'] != '0000' or stop['f1'] != '0001'
            or stop['ed'] != stop['ee'] or not offer_words(unread)):
        raise ValueError('stop boundary does not establish disabled delivery with unread data')
    if native_bytes is not None and native_bytes-persisted_bytes != 2*unread:
        raise ValueError('stop unread count differs from native unpersisted suffix')
    return dict(data_offers=data_posts, control_posts=controls, consumer_acknowledgements=len(consumes),
                acknowledged_words=words, acknowledged_bytes=words*2,
                offered_word_counts={str(size):count for size,count in sorted(sizes.items())},
                stop_unread_words=unread, stop_inflight_transfer=False,
                threshold_eligible_words=offer_words(unread),
                persisted_prefix_count_matches=True, per_offer_payload_bytes_validated=False,
                file_flush_validated=False,
                scope='Message construction and consumer arithmetic/count correlation; not per-byte NAND transport ownership')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('trace',type=Path)
    parser.add_argument('stored_file',type=Path)
    parser.add_argument('--source-a00',required=True,type=Path)
    args = parser.parse_args()
    try:
        source = source_delivery(args.source_a00.read_bytes())
        text = args.trace.read_text()
        if 'mu4_native_recorder_metadata: PASS native_reply=1 success=1 playback=0 firmware_state_forcing=0' not in text:
            raise ValueError('recording did not complete its original metadata lifecycle')
        transform,_ = verify_file(text,args.stored_file.read_bytes(),inventory_divergence=True)
        report = verify(text,persisted_bytes=transform['persisted_native_prefix_bytes'],
                        native_bytes=transform['native_window_bytes'])
        print(json.dumps(dict(source=source,transform=transform,delivery=report),indent=2))
    except (KeyError,OSError,ValueError) as error:
        print(f'Recorder delivery: FAIL: {error}',file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

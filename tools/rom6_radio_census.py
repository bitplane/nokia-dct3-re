"""Summarize passive native ROM6 I/O; no RF semantics or absence proof."""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import re


PORT=re.compile(r'rom6_radio_port: direction=([RW]) address=([0-9a-f]{4}) count=(\d+) first=([0-9.]+) last=([0-9.]+) values=([0-9a-f,]+) values_capped=([01]) cpu_pcs=([0-9a-f,]+) pcs_capped=([01])')
WRITE=re.compile(rb'([0-9]+\.[0-9]{9})\t([0-9a-f]{6})\t([0-9a-f]{4})\t([0-9a-f]{4})\t([0-9a-f]{4})\n')


def find_output_source(image, overlay):
    """Join the bounded first-output block to a unique own-MCU source."""
    if len(overlay) != 0x4000:
        raise ValueError('first-output overlay size differs')
    block = overlay[0x0b20 * 2:0x0b70 * 2]
    offset = image.find(block)
    if offset < 0 or image.find(block, offset + 1) >= 0:
        raise ValueError('first radio block does not uniquely match own MCU bytes')
    return dict(mcu_offset=f'{offset:x}', bytes=len(block),
                sha256=hashlib.sha256(block).hexdigest())


def check_output_overlay(image, overlay):
    """Pin the observed uploaded writer, not a later overlay at the same PC."""
    if len(overlay)!=0x2000*2:
        raise ValueError('first-output overlay size differs')
    block=overlay[0x0b20*2:0x0b70*2]
    if block!=image[0x117070:0x117070+len(block)]:
        raise ValueError('observed serial-output writer differs from own MCU upload')
    if overlay[0x0b54*2:0x0b56*2]!=bytes.fromhex('75920031') or overlay[0x0b58*2:0x0b5a*2]!=bytes.fromhex('75820032'):
        raise ValueError('observed port-pair instruction boundary differs')
    return dict(dsp_words='0b20..0b6f',own_mcu_byte_offset='117070',
                bytes=len(block),sha256=hashlib.sha256(block).hexdigest())


def census(text, stream):
    footers=re.findall(r'rom6_radio_capture: writes=(\d+) overflow=([01]) observer_writes=0',text)
    # Raw 6210 observations can cover120s (five24s capture budgets).
    # Completeness still requires overflow=0 and every footer-counted row.
    if len(footers)!=1 or footers[0][1]!='0' or not 0<int(footers[0][0])<=5*2097152:
        raise ValueError('missing, empty, duplicate or truncated radio capture')
    ports={}
    for match in PORT.finditer(text):
        direction,address,count,first,last,values,value_cap,pcs,pc_cap=match.groups()
        key=f'{direction}:{address}'
        if key in ports or int(count)<=0 or not 0<=float(first)<=float(last):
            raise ValueError('duplicate or malformed port summary')
        ports[key]=dict(count=int(count),first=float(first),last=float(last),
                        values=values.split(','),values_capped=value_cap=='1',
                        bus_time_cpu_pcs=pcs.split(','),cpu_pcs_capped=pc_cap=='1')
    if len(ports)!=text.count('rom6_radio_port:'):
        raise ValueError('unparsed or missing per-port summary fields')
    counts=Counter()
    contexts=defaultdict(Counter)
    digest=hashlib.sha256()
    tuples={}
    latched={0x30:0,0x31:0,0x32:0}
    seen=set()
    records,last=0,-1.0
    for line in stream:
        match=WRITE.fullmatch(line)
        if not match:
            raise ValueError('malformed complete write stream')
        digest.update(line)
        time,pc,address,value,mask=match.groups()
        now=float(time)
        if now<last:
            raise ValueError('write timestamps are not monotonic')
        last=now; records+=1
        port,data=int(address,16),int(value,16)
        counts[f'W:{port:04x}']+=1
        contexts[f'W:{port:04x}'][pc.decode()]+=1
        if port in latched:
            latched[port]=data; seen.add(port)
        elif port==4:
            # Raw issued tuples only: this is not proof of timed delivery,
            # serial shift, tuning, reception or peripheral completion.
            key=tuple(latched[p] for p in (0x30,0x31,0x32))+(data,)
            row=tuples.setdefault(key,dict(count=0,first=now,last=now,fresh_payloads=0))
            row['count']+=1; row['last']=now
            row['fresh_payloads']+=int(all(p in seen for p in latched))
            seen.clear()
    if records!=int(footers[0][0]):
        raise ValueError('write stream length differs from capture footer')
    expected={key:value['count'] for key,value in ports.items() if key.startswith('W:')}
    if counts!=expected:
        raise ValueError('write counts differ from independent per-port summaries')
    return dict(write_records=records,write_sha256=digest.hexdigest(),ports=ports,
        write_cpu_contexts={key:dict(sorted(values.items())) for key,values in sorted(contexts.items())},
        issued_p30_p31_p32_p04=[dict(words=[f'{word:04x}' for word in key],**value)
                              for key,value in sorted(tuples.items())],
        limits=['One pinned product/input schedule; not all reachable paths.',
                'Read values and bus-time CPU contexts are explicitly capped.',
                'Issued P04 tuples are not proof of delivery, RF meaning or reception.'])


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run',type=Path)
    parser.add_argument('--mcu',type=Path,help='also compare the captured first writer to its own MCU upload')
    args=parser.parse_args()
    try:
        with (args.run/'rom6_radio_writes.tsv').open('rb') as stream:
            report=census((args.run/'error.log').read_text(),stream)
        if args.mcu:
            report['own_upload_writer']=check_output_overlay(args.mcu.read_bytes(),
                (args.run/'rom6_radio_first_output.be16').read_bytes())
    except (OSError,ValueError) as error:
        parser.exit(1,f'ROM6 radio census FAIL: {error}\n')
    print(json.dumps(report,indent=2))


if __name__=='__main__': main()

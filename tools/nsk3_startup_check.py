"""Own NSK-3 checksum controls and still-invalid native record boundary."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsk3_boot_check import check_run as check_control,check_native_trace,FRAME
from tools.nsk3_static_check import inspect
from tools.make_nsk3_checksum_profile import repair
from tools.nsk1_phonebook_roundtrip import frame_check
from tools.dct3_msid_codec import decode_msid
from tools.dct3_rom4_record_codec import decode_record

BLANK='5fdab89cdada4b6ac821d32facb589dfb541e9856fe3b569e8da49646fc3d249'
RESULTS='ff ff 00 ff 00 ff 00 00 00 ff ff 00 00 ff 00 ff ff ff 00 00 00 00 ff ff'.split()


def check_record_math(trace):
    pending,chip,count={},None,0
    records=[]
    rows=re.findall(r'nsk3_record_(post|reply): .*?data=([0-9a-f]+) cycles=',trace)
    if len(rows)!=trace.count('nsk3_record_post: ')+trace.count('nsk3_record_reply: '):
        raise ValueError('malformed native record packet observation')
    for kind,raw in rows:
        packet=bytes.fromhex(raw)
        if len(packet) not in (60,64):raise ValueError('native record packet capture truncated')
        if kind=='post':
            packet=packet[:packet[2]+4]
            command,size=packet[4:6]
            if (packet[3]!=0x70 or command!=0x13+len(pending)
                    or size!={0x13:4,0x14:12,0x15:20,0x16:24}.get(command)
                    or len(packet)!=6+size):
                raise ValueError('native record request order/shape differs')
            pending[command]=packet[6:]
        else:
            packet=packet[:int.from_bytes(packet[4:6],'big')+6]
            payload=packet[8:]
            if packet[:4]!=bytes.fromhex('00000074') or packet[6:8]!=bytes.fromhex('0100') or len(pending)!=4:
                raise ValueError('native record reply lacks complete own requests')
            if payload[:4]==bytes.fromhex('340e0082') and len(payload)==16:
                identity=decode_msid(payload[3:])
                if chip is not None or identity[:4]!=pending[0x13]:
                    raise ValueError('native MSID does not reproduce own request')
                chip=identity[4:8]
            elif payload[:4]==bytes.fromhex('35320000') and len(payload)==52 and chip is not None:
                encoded=pending[0x16]
                blocks=[decode_record(encoded[i:i+12],chip) for i in (0,12)]
                decoded=b''.join(block[:10]+bytes(2) for block in blocks)
                if payload[4:28]!=decoded or payload[28:]!=encoded:
                    raise ValueError('native record arithmetic/echo differs')
                records.append({'chip_field':chip.hex(),'flash_field':pending[0x13].hex(),
                                'request':encoded.hex(),'decoded':decoded.hex()})
                count+=1
                pending,chip={},None
            else:raise ValueError('unclassified native record reply')
    if count!=2 or pending:raise ValueError('native record conversation incomplete')
    return records


def check_startup_trace(trace,low_block):
    sums=re.findall(r'nsk3_security_checksum: stored=([0-9a-f]+) computed=([0-9a-f]+)',trace)
    if sums != [('db24','db24'),('da54','da54')]:
        raise ValueError('own checksum comparison/firmware recalculation differs')
    rows=re.findall(r'nsk3_startup_result: index=([0-9a-f]+) code=([0-9a-f]+) pointer=([0-9a-f]+)',trace)
    expected=[]
    for _ in range(2):
        expected.extend((f'{i:02x}','12' if i==0x12 and not low_block else code,f'{0x10fc80+i:08x}') for i,code in enumerate(RESULTS))
    if rows!=expected or len(rows)!=trace.count('nsk3_startup_result: '):
        raise ValueError('complete own startup-result sweep differs')
    formats=re.findall(r'nsk3_record_format: valid=([0-9a-f]+) pointer=([0-9a-f]+)',trace)
    if formats!=[('00000000','00101c4c')]*2:
        raise ValueError('native record-format failure changed')
    calls=re.findall(r'nsk3_restart_entry: reason=([0-9a-f]+) caller=([0-9a-f]+)',trace)
    if calls!=[('00000004','0026f335')]:
        raise ValueError('record-owned early restart differs')
    marker=re.findall(r'nsk3_startup_marker: offset=0010fdc8 value=([0-9a-f]+) mask=0000ff00 pc=([0-9a-f]+)',trace)
    if [(value,pc) for value,pc in marker if pc!='002adfb6'] != [
            ('00005a00','0023ab7e'),('0000a500','0026f308'),('00005500','0026f2fa')]:
        raise ValueError('own record failure/retry marker ordering differs')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('source','control','native_roms','run'):
        parser.add_argument(name,type=Path)
    parser.add_argument('--low-block',action='store_true')
    args=parser.parse_args()
    try:
        check_control(args.source,args.control,'boot4',args.native_roms)
        _,image=inspect(args.source,'boot4')
        trace=(args.run/'error.log').read_text()
        check_native_trace(trace)
        check_startup_trace(trace,args.low_block)
        check_record_math(trace)
        storage=args.run/'nvram/nsk3r4t'
        if (storage/'flash').read_bytes()!=image:raise ValueError('own flash changed')
        initial=repair((args.control/'nvram/nsk3r4t/eeprom').read_bytes(),args.low_block)
        expected=bytearray(initial)
        expected[0x73]=0xe0
        expected[0x7a]=0x4e
        expected[0x11e:0x120]=bytes.fromhex('da54')
        if (storage/'eeprom').read_bytes()!=expected:
            raise ValueError('unexpected own EEPROM change beyond observed firmware writes')
        for seconds in (5,15):frame_check(args.run/f'snap/nsk3_boot_{seconds}.png',BLANK if args.low_block else FRAME)
    except (ValueError,OSError) as error:
        print(f'NSK-3 startup comparison failed: {error}',file=sys.stderr)
        return 1
    print('NSK-3 checksum control PASS; invalid record/reason04 retained; usable boot unproved')
    return 0


if __name__=='__main__':raise SystemExit(main())

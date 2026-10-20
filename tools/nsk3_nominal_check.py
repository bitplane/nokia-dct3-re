"""NSK-3 nominal-input graphical startup and raw matrix-scan acceptance only."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsk3_record_check import check,replies
from tools.nsk3_startup_check import check_record_math
from tools.nsk3_static_check import inspect
from tools.nsk1_phonebook_roundtrip import frame_check

FRAMES={5:'b3e40355a86a287082270923fb337d76aa2b6bcdacf9066d6689082bbae04cf7',
        15:'a879c658a532b6526d3343287b42848f8a16bf10b7215086c1e215db45ee67cc'}


def check_trace(trace):
    if re.findall(r'ccont_adc2_config: raw=([0-9a-f]+) external_input=1',trace)!=['22a']:
        raise ValueError('nominal external ADC2 input not applied exactly once')
    samples=re.findall(r'nsk3_ui_analog_sample: accepted=([0-9a-f]+) first=([0-9a-f]+) second=([0-9a-f]+) gain=([0-9a-f]+) offset=([0-9a-f]+)',trace)
    if not samples or any(row!=('01','0dfd','0dfd','3f800000','00000000') for row in samples):
        raise ValueError('nominal converted samples/coefficient contract differs')
    if 'nsk3_ui_startup_report: value=0014 ' not in trace or 'nsk3_ui_second_group: bits=0f ' not in trace:
        raise ValueError('organic readiness completion missing')
    rows=re.findall(r'nsk3_ui_sample: t=(\d+) lcd=(\d+) scan=(\d+) rows=(\d+) .*?columnmask=([0-9a-f]+)',trace)
    if [int(row[0]) for row in rows]!=list(range(1,17)):
        raise ValueError('UI sample interval incomplete')
    before,after=rows[6],rows[8]
    if not (int(after[2])>int(before[2]) and int(after[3])>int(before[3]) and after[4]=='20'):
        raise ValueError('physical contact did not cause matrix rescan')
    contacts=re.findall(r'nsk3_ui_contact: down=(\d) column=1 mask=04 firmware_writes=0',trace)
    if contacts!=['1','0']:raise ValueError('physical contact fixture incomplete')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('source','control','native_roms','initial','cold','run'):
        parser.add_argument(name,type=Path)
    args=parser.parse_args()
    try:
        check(args.source,args.control,args.native_roms,args.initial,args.cold)
        records=check_record_math((args.control/'error.log').read_text())
        chip=bytes.fromhex(records[0]['chip_field'])
        trace=(args.run/'error.log').read_text()
        replies(trace,chip,True)
        check_trace(trace)
        _,image=inspect(args.source,'boot4')
        if (args.run/'nvram/nsk3r4t/flash').read_bytes()!=image:
            raise ValueError('own executable flash changed')
        if (args.run/'nvram/nsk3r4t/eeprom').read_bytes()!=(args.cold/'nvram/nsk3r4t/eeprom').read_bytes():
            raise ValueError('persistent record/identity/calibration changed')
        for time,digest in FRAMES.items():frame_check(args.run/f'snap/nsk3_boot_{time}.png',digest)
    except (ValueError,OSError) as error:
        print(f'NSK-3 nominal startup failed: {error}',file=sys.stderr)
        return 1
    print('NSK-3 nominal hypothesis PASS: Insert SIM card and physical matrix rescan; SIM/radio/fitted calibration unproved')
    return 0


if __name__=='__main__':raise SystemExit(main())

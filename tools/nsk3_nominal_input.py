"""Prepare an opt-in nominal ADC2 comparison from qualified own NSK-3 storage."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import sys
import xml.etree.ElementTree as ET

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsk3_record_check import check
from tools.nsk3_analog_contract import inspect
from tools.nsk3_static_check import inspect as inspect_source
from tools.nsk3_security_contract import inspect as inspect_security
from tools.make_sim_card_profile import make_profile
from tools.sim_security_trace_check import ATTEMPTS_OFFSET


def config(insert_sim=False):
    root=ET.Element('mameconfig',version='10')
    system=ET.SubElement(root,'system',name='nsk3r4t')
    inputs=ET.SubElement(system,'input')
    ET.SubElement(inputs,'port',tag=':RESEARCH_ADC2',type='CONFIG',
                  mask='1023',defvalue='1023',value='554')
    if insert_sim:
        ET.SubElement(inputs,'port',tag=':SIM_REMOVED',type='OTHER',
                      mask='1',defvalue='1',value='0')
    return ET.ElementTree(root)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('source','control','native_roms','initial','cold','output'):
        parser.add_argument(name,type=Path)
    parser.add_argument('--insert-sim',action='store_true',help='cold physical insertion of the explicit synthetic card')
    parser.add_argument('--initialize-phone-retry',action='store_true',help='external one-byte retry fixture derived from own initializer, not factory data')
    parser.add_argument('--pin-enabled',action='store_true',help='use the explicit shared laboratory PIN-enabled synthetic card')
    parser.add_argument('--pin-blocked',action='store_true',help='use an explicit laboratory card with CHV1 attempts exhausted')
    args=parser.parse_args()
    try:
        if args.output.exists():raise ValueError('comparison requires a fresh output directory')
        if (args.pin_enabled or args.pin_blocked) and not args.insert_sim:
            raise ValueError('PIN-card comparison requires explicit insertion')
        check(args.source,args.control,args.native_roms,args.initial,args.cold)
        _,image=inspect_source(args.source,'boot4')
        contract=inspect(image)
        storage=args.cold/'nvram/nsk3r4t'
        shutil.copytree(storage,args.output/'nvram/nsk3r4t')
        security=None
        if args.initialize_phone_retry:
            security=inspect_security(image)
            eeprom=args.output/'nvram/nsk3r4t/eeprom'
            data=bytearray(eeprom.read_bytes())
            if data[security['retry_eeprom']]!=0xff:
                raise ValueError('retry comparison requires own erased retry byte')
            data[security['retry_eeprom']]=0
            eeprom.write_bytes(data)
        card=None
        if args.pin_enabled or args.pin_blocked:
            card=bytearray(make_profile(True))
            if args.pin_blocked:card[ATTEMPTS_OFFSET]=0
            (args.output/'nvram/nsk3r4t/sim_card').write_bytes(card)
        (args.output/'cfg').mkdir()
        config(args.insert_sim).write(args.output/'cfg/nsk3r4t.cfg',encoding='utf-8',xml_declaration=True)
        metadata={'classification':'nominal external-input hypothesis, not fitted calibration',
                  'raw_adc2':554,'basis':'nominal 3.6 V / typical 6.5 mV per count',
                  'inserted_synthetic_sim':args.insert_sim,
                  'phone_retry_fixture':security,
                  'synthetic_pin_card_sha256':hashlib.sha256(card).hexdigest() if card else None,
                  'synthetic_pin_blocked':args.pin_blocked,
                  'own_static_contract':contract,
                  'seed':str(args.cold.resolve()),
                  'seed_eeprom_sha256':hashlib.sha256((storage/'eeprom').read_bytes()).hexdigest(),
                  'guest_state_writes':False,'donor_provisioning':False}
        (args.output/'nominal_input.json').write_text(json.dumps(metadata,indent=2)+'\n')
    except (ValueError,OSError) as error:
        print(f'NSK-3 nominal input preparation failed: {error}',file=sys.stderr)
        return 1
    print(f'prepared {args.output}; physical ADC2=554 hypothesis, own persistent record unchanged')
    return 0


if __name__=='__main__':raise SystemExit(main())

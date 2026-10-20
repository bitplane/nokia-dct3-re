"""Prepare a validated-record NSM-1 nominal ADC2 comparison, not factory data."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsm1_published_record_check import records
from tools.nsm1_analog_contract import inspect
from tools.nsm1_v523_static_check import SHA1
from tools.dct3_rom4_record_codec import retained_record_transform
from tools.nsm1_security_contract import inspect as inspect_security
from tools.sim_phonebook_check import CURRENT_NVRAM_LENGTH
from tools.sim_fdn_profile import extend as extend_fdn,EXTENDED_LENGTH
from tools.sim_security_trace_check import CHV_OFFSET,ATTEMPTS_OFFSET,ENABLED_OFFSET,padded_pin


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('cold',type=Path);parser.add_argument('output',type=Path)
    parser.add_argument('--initialize-phone-retry',action='store_true')
    card_mode=parser.add_mutually_exclusive_group()
    card_mode.add_argument('--preserve-sim',action='store_true')
    card_mode.add_argument('--pin-enabled',action='store_true')
    parser.add_argument('--fdn',action='store_true',help='append explicit FDN laboratory card profile')
    parser.add_argument('--fdn-adn-access',action='store_true',help='allow ADN access while invalidated in the new FDN profile')
    args=parser.parse_args()
    try:
        if args.output.exists():raise ValueError('fresh output required')
        encoded=records((args.cold/'error.log').read_text(),True)
        storage=args.cold/'nvram/nsm1r4t'
        flash=(storage/'flash').read_bytes()
        if hashlib.sha1(flash).hexdigest()!=SHA1:raise ValueError('own flash differs')
        inspect(flash)
        eeprom=(storage/'eeprom').read_bytes()
        if len(eeprom)!=0x4000:raise ValueError('EEPROM extent differs')
        if retained_record_transform(eeprom[0x20:0x38],eeprom[:12])!=encoded:
            raise ValueError('saved record differs from qualified cold echo')
        if int.from_bytes(eeprom[0x3c:0x40],'big')!=sum(eeprom[:0x3c])&65535:
            raise ValueError('saved low-block checksum differs')
        if args.initialize_phone_retry:
            retry=inspect_security(flash)
            eeprom=bytearray(eeprom);eeprom[retry['eeprom_offset']]=retry['initial_value'];eeprom=bytes(eeprom)
        card=None
        if args.preserve_sim or args.pin_enabled:
            card=(storage/'sim_card').read_bytes()
            if len(card) not in (CURRENT_NVRAM_LENGTH,EXTENDED_LENGTH):raise ValueError('current synthetic card extent required')
            if args.pin_enabled:
                if (card[CHV_OFFSET:CHV_OFFSET+8]!=padded_pin('1234')
                        or card[ATTEMPTS_OFFSET:ATTEMPTS_OFFSET+4]!=bytes((3,3,10,10))
                        or card[ENABLED_OFFSET]!=0):raise ValueError('fresh default-credential lab card required')
                card=bytearray(card);card[ENABLED_OFFSET]=1;card=bytes(card)
        if args.fdn:
            if card is None:raise ValueError('FDN requires an explicit preserved or PIN-enabled card')
            card=extend_fdn(card,args.fdn_adn_access)
        elif args.fdn_adn_access:raise ValueError('FDN ADN-access option requires --fdn')
        target=args.output/'nvram/nsm1r4t/eeprom'
        target.parent.mkdir(parents=True);target.write_bytes(eeprom)
        (args.output/'initial_eeprom.bin').write_bytes(eeprom)
        if card is not None:
            (target.parent/'sim_card').write_bytes(card)
            (args.output/'initial_sim_card.bin').write_bytes(card)
        (args.output/'input.json').write_text(json.dumps({'adc2':554,'physical_units_proved':False,
            'phone_retry_initialized':args.initialize_phone_retry,'sim_preserved':args.preserve_sim,
            'pin_enabled_fixture':args.pin_enabled,
            'fdn_fixture':args.fdn,
            'fdn_adn_access':args.fdn_adn_access,
            'donor_provisioning':False},indent=2)+'\n')
        root=ET.Element('mameconfig',version='10')
        inputs=ET.SubElement(ET.SubElement(root,'system',name='nsm1r4t'),'input')
        ET.SubElement(inputs,'port',tag=':RESEARCH_ADC2',type='CONFIG',mask='1023',defvalue='1023',value='554')
        cfg=args.output/'cfg/nsm1r4t.cfg';cfg.parent.mkdir()
        ET.ElementTree(root).write(cfg,encoding='utf-8',xml_declaration=True)
    except (ValueError,OSError) as error:
        print(f'NSM-1 nominal comparison failed: {error}',file=sys.stderr);return 1
    return 0


if __name__=='__main__':raise SystemExit(main())

"""Prepare a validated-record NSM-1 nominal ADC2 comparison, not factory data."""
import argparse
import hashlib
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsm1_published_record_check import records
from tools.nsm1_analog_contract import inspect
from tools.nsm1_v523_static_check import SHA1
from tools.dct3_rom4_record_codec import retained_record_transform


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('cold',type=Path);parser.add_argument('output',type=Path)
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
        target=args.output/'nvram/nsm1r4t/eeprom'
        target.parent.mkdir(parents=True);target.write_bytes(eeprom)
        (args.output/'initial_eeprom.bin').write_bytes(eeprom)
        root=ET.Element('mameconfig',version='10')
        inputs=ET.SubElement(ET.SubElement(root,'system',name='nsm1r4t'),'input')
        ET.SubElement(inputs,'port',tag=':RESEARCH_ADC2',type='CONFIG',mask='1023',defvalue='1023',value='554')
        cfg=args.output/'cfg/nsm1r4t.cfg';cfg.parent.mkdir()
        ET.ElementTree(root).write(cfg,encoding='utf-8',xml_declaration=True)
    except (ValueError,OSError) as error:
        print(f'NSM-1 nominal comparison failed: {error}',file=sys.stderr);return 1
    return 0


if __name__=='__main__':raise SystemExit(main())

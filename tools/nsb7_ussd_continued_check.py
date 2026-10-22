"""Physical continued USSD reply and ordinary recovery on research-HLE NSB-7."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None,''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsb7_pin_services_check import check_source,check_authentication
from tools.nsb7_pin_check import check_frame
from tools.nsb7_radio_check import contract
from tools.nsb7_storage_check import inspect
from tools.radio_ussd_trace_check import verify
from tools.run_sip_handset_gate import verify_handset_health

FRAMES={
    35:'95f19dea1279b6851b27f77cfa99ddfd8f2e524c69ebc5c599fb4e1686a38483',
    37:'4dd885abe2b46c973d8c660c62a63fb27606de0a4a68c0382ff4ac404b1fb850',
    39:'b2d19830b1228edddf5ad118e2007fc0a07cf5aec2c21443da07489f104be229',
    41:'c0f40af21daacee07ecaa75592cb8eaf6c78e261d8b3156e6c0c2abd2c0fb566',
    45:'32c41625e02a87a0ec97ec98a250040fc87ad8c5722c78200c54b27f7de2d560',
    47:'76728306449ad0f2ae8cbdbd1fc5e880d3dcff054c955e8b4a2aabc87db8e7e4',
}
KEYS=[(26,'Keypad *'),(27,'Keypad 1'),(28,'Keypad 2'),(29,'Keypad 3'),
      (30,'Keypad #'),(32,'Send'),(36,'Left Softkey / Menu'),(38,'Keypad 2'),
      (40,'Left Softkey / Menu'),(44,'End'),(46,'Left Softkey / Menu')]


def check(text,initial,final):
    check_source(initial)
    check_authentication(text.split('nsb7_ussd_input:',1)[0])
    if initial!=final:raise ValueError('continued USSD changed persistent card contents')
    verify(text,Path('.'),'continued-reply',require_frame=False)
    for state,fraction in ((1,'000000000'),(0,'200000000')):
        observed=re.findall(rf'nsb7_ussd_input: key=(.*?) state={state} t=(\d+)\.{fraction}',text)
        if observed!=[(key,str(second)) for second,key in KEYS]:
            raise ValueError('continued USSD physical input/release sequence differs')
    reply=text.index('data=1b3a12a210020102300b02013c300604010f040132 ')
    if reply<text.index('nsb7_ussd_input: key=Left Softkey / Menu state=1 t=40.'):
        raise ValueError('USSD reply preceded physical Send')
    if text.count('gsm_ss: request=ussd ')!=1 or text.count('gsm_ss: continued_result ')!=1:
        raise ValueError('continued USSD transaction was duplicated')
    if 'sim_device: update ' in text or text.count('nsb7_ussd: complete')!=1:
        raise ValueError('continued USSD incomplete or unexpected card write')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image',type=Path)
    parser.add_argument('run',type=Path)
    args=parser.parse_args()
    try:
        contract(args.image.read_bytes())
        verify_handset_health(Path(str(args.run)+'-launch.log').read_text())
        storage=args.run/'nvram/nsb7hle'
        inspect((storage/'flash').read_bytes())
        check((args.run/'error.log').read_text(),(args.run/'initial_sim_card.bin').read_bytes(),
              (storage/'sim_card').read_bytes())
        for second,digest in FRAMES.items():
            check_frame(args.run/f'snap/nsb7_ussd_response_{second}.png',digest)
    except (OSError,ValueError,RuntimeError) as error:
        parser.exit(1,f'NSB-7 continued USSD: FAIL: {error}\n')
    print('PASS: physical continued USSD reply, correlated final result, RR release, idle/menu recovery and unchanged card; research HLE only')


if __name__=='__main__':main()

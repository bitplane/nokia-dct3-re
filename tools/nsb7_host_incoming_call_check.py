"""Host-originated NSB-7 incoming signaling from reviewed settled idle."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.nsb7_pin_services_check import check_source,check_authentication
from tools.nsb7_radio_check import contract
from tools.nsb7_storage_check import inspect
from tools.nsb7_pin_check import check_frame
from tools.nsb7_host_outgoing_call_check import check_clock
from tools.radio_call_lifecycle_common import require_ordered,laboratory_incoming_setup

FRAMES={
    # Same retained 13:47:45 source as the outgoing fixture, without reset.
    'nsb7_call_ready.png':'32c41625e02a87a0ec97ec98a250040fc87ad8c5722c78200c54b27f7de2d560',
    'nsb7_call_ringing.png':'cf0be19d9695137a07460b23c9fa590c21e6427d684e28e2bc1051f588cac110',
    'nsb7_call_Send.png':'9cabd1e512ce5e4467808054cbabf5c5f1306e7c6ab681e3fa193661cc441c7e',
    'nsb7_call_final.png':'32c41625e02a87a0ec97ec98a250040fc87ad8c5722c78200c54b27f7de2d560',
}


def check(text, restored=False, *, caller='447700900123'):
    setup=laboratory_incoming_setup(caller)
    check_authentication(text.split('nsb7_call_ready:',1)[0])
    if '[LUA ERROR]' in text:
        raise ValueError('host incoming physical fixture failed')
    rows=list(re.finditer(r'nsb7_call_input: key=(Send|End) state=([01]) t=([0-9.]+)',text))
    expected=[(key,edge,f'{second}.{fraction}') for key,second in [('Send',52),('End',60)]
              for edge,fraction in [('1','000000000'),('0','200000000')]]
    if [row.groups() for row in rows]!=expected:
        raise ValueError('host incoming Answer/End sequence differs')
    for index,code in enumerate((0x0e,0x0f)):
        if f'nsb7_key_decode: key={code:02x} observer_writes=0' not in text[rows[2*index].end():rows[2*index+1].start()]:
            raise ValueError('host incoming physical contact did not decode')
    states=re.findall(r'gsm_call_adapter: incoming state id=1 epoch=(\d+) phase=(\w+)',text)
    expected_states=[('1',phase) for phase in ('queued','paging','alerting','connected')]
    if restored:
        expected_states.append(('2','connected'))
    expected_states += [(str(2 if restored else 1),phase) for phase in ('media_closed','ended')]
    if states!=expected_states:
        raise ValueError('host incoming identity/epoch/lifecycle differs')
    require_ordered(text,[
        ('registered PCS',r'gsm_call_adapter: network registered=1 arfcn=600\b'),
        ('settled readiness',r'nsb7_call_ready: t=45\.000000000'),
        ('host acceptance',r'gsm_call_adapter: incoming id=1 result=accepted'),
        ('host queued',r'incoming state id=1 epoch=1 phase=queued'),
        ('host paging',r'incoming state id=1 epoch=1 phase=paging'),
        ('incoming SETUP',rf'GSM service downlink kind=9 sapi=0 pd=03 message=05 length={len(setup)}(?:\s|$)'),
        ('task4 calling number',rf'nsb7_lapdm_downlink: pc=002e49e6 task=4 selector=80 block=03[0-9a-f]{{2}}{len(setup)*4+1:02x}{setup.hex()}[0-9a-f]* '),
        ('task14 calling number',rf'nsb7_lapdm_downlink: pc=002645ec task=14 selector=80 block=03[0-9a-f]{{2}}{len(setup)*4+1:02x}{setup.hex()}[0-9a-f]* '),
        ('Call Confirmed',r'GSM service uplink sapi=0 pd=03 message=08 length=11 data=8308040460020180150101'),
        ('Alerting',r'GSM service uplink sapi=0 pd=03 message=01 length=2 data=8341'),
        ('host alerting',r'incoming state id=1 epoch=1 phase=alerting'),
        ('physical Answer',r'nsb7_call_input: key=Send state=1 t=52\.'),
        ('Connect',r'GSM service uplink sapi=0 pd=03 message=07 length=2 data=8307'),
        ('Connect ACK',r'GSM service downlink kind=20 sapi=0 pd=03 message=0f length=2'),
        ('host connected',r'incoming state id=1 epoch=1 phase=connected'),
        ('physical End',r'nsb7_call_input: key=End state=1 t=60\.'),
        ('Disconnect',r'GSM service uplink sapi=0 pd=03 message=25 length=5 data=836502e090'),
        ('Release',r'GSM service downlink kind=25 sapi=0 pd=03 message=2d length=2'),
        ('Release Complete',r'GSM service uplink sapi=0 pd=03 message=2a length=2 data=832a'),
        ('host ended',rf'incoming state id=1 epoch={2 if restored else 1} phase=ended'),
        ('common control',r'nsb7_radio_tx: [^\n]*type=02 length=20 data=0412020200000016600002580000000001297000'),
        ('background measurement',r'nsb7_radio_tx: [^\n]*type=57 length=4 data=03050000'),
    ],'NSB-7 host incoming signaling')
    if restored:
        require_ordered(text,[
            ('connected predecessor',r'incoming state id=1 epoch=1 phase=connected'),
            ('save',r'nsb7_state: event=saved'),('restore',r'nsb7_state: event=restored'),
            ('new epoch',r'incoming state id=1 epoch=2 phase=connected'),
            ('stale termination rejection',r'gsm_call_adapter: stale epoch=1 current=2 type=termination id=1 result=rejected'),
            ('physical End',r'nsb7_call_input: key=End state=1 t=60\.'),
        ],'NSB-7 incoming host restoration')
    if len(re.findall(r'gsm_call_adapter: incoming id=\d+ result=accepted',text))!=1 or \
            'GSM service establish sapi=0 pd=05 message=24 ' in text:
        raise ValueError('expected one externally originated call, not physical outgoing establishment')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image',type=Path)
    parser.add_argument('run',type=Path)
    args=parser.parse_args()
    try:
        contract(args.image.read_bytes())
        text=(args.run/'error.log').read_text()
        check(text)
        check_clock(text,finish=68)
        storage=args.run/'nvram/nsb7hle'
        initial=(args.run/'initial_sim_card.bin').read_bytes()
        check_source(initial)
        if (storage/'sim_card').read_bytes()!=initial:
            raise ValueError('host incoming call changed persistent card data')
        inspect((storage/'flash').read_bytes())
        for name,digest in FRAMES.items():
            check_frame(args.run/'snap'/name,digest)
    except (OSError,ValueError) as error:
        parser.exit(1,f'NSB-7 host incoming: FAIL: {error}\n')
    print('PASS: host call from authenticated settled idle, physical Answer/End and correlated signaling release; speech unproved')


if __name__=='__main__':
    main()

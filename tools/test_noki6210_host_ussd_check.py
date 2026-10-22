import unittest
import json
from tools.noki6210_host_ussd_check import check_notification,check_request
from tools.noki6210_puk_check import profile


def fixture():
    lines=[]
    items=[(8+i,'Keypad '+d) for i,d in enumerate('1234')]
    items += [(12,'Left Softkey / Menu'),(26,'Right Softkey / C'),(30,'Left Softkey / Menu')]
    for second,key in items:
        if second==26:
            lines += [
                'gsm_call_adapter: incoming ussd id=1 result=accepted t=20.010000',
                'gsm_call_adapter: incoming ussd state id=1 epoch=1 phase=queued',
                'gsm_ss: network_initiated operation=notify transaction=0b invoke=1 dcs=0f',
                'GSM service downlink kind=32 sapi=0 pd=0b message=3b length=29',
                'GSM service uplink sapi=0 pd=0b message=3a length=8 data=8b3a05a203020101',
                'GSM service downlink kind=27 sapi=0 pd=0b message=2a length=2',
                'LAPDm service Channel Release acknowledged nr=6',
                'gsm_call_adapter: incoming ussd state id=1 epoch=1 phase=delivered',
            ]
        lines += [f'6210_host_ussd_input: key={key} state=1 t={second}.000000000',
                  f'6210_host_ussd_input: key={key} state=0 t={second}.150000000']
        if second==12:
            lines += ['header cla=a0 ins=20 p1=00 p2=01 p3=08',
                      'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1',
                      'gsm_call_adapter: network registered=1 arfcn=35']
    lines.append('6210_host_ussd: complete=1 firmware_state_forcing=0')
    return '\n'.join(lines),profile(3,True)


def request_fixture():
    lines=[]
    items=[(8+i,'Keypad '+d) for i,d in enumerate('1234')]
    items += [(12,'Left Softkey / Menu'),(26,'Left Softkey / Menu'),(30,'Keypad 2'),
              (32,'Left Softkey / Menu'),(36,'End'),(38,'Left Softkey / Menu')]
    for second,key in items:
        if second==26:
            lines += [
                'gsm_call_adapter: incoming ussd id=1 result=accepted t=20.010000',
                'gsm_call_adapter: incoming ussd state id=1 epoch=1 phase=queued',
                'gsm_ss: network_initiated operation=request transaction=0b invoke=1 dcs=0f',
                'GSM service downlink kind=32 sapi=0 pd=0b message=3b length=26',
            ]
        lines += [f'6210_host_ussd_input: key={key} state=1 t={second}.000000000',
                  f'6210_host_ussd_input: key={key} state=0 t={second}.150000000']
        if second==12:
            lines += ['header cla=a0 ins=20 p1=00 p2=01 p3=08',
                      'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1',
                      'gsm_call_adapter: network registered=1 arfcn=35']
        if second==32:
            lines += [
                'GSM service uplink sapi=0 pd=0b message=3a length=21 data=8b3a12a210020101300b02013c300604010f040132',
                'GSM service downlink kind=27 sapi=0 pd=0b message=2a length=2',
                'LAPDm service Channel Release acknowledged nr=6',
                'gsm_call_adapter: incoming ussd reply id=1 epoch=1 dcs=0f data=32 empty=0',
                'gsm_call_adapter: incoming ussd state id=1 epoch=1 phase=answered',
            ]
    lines.append('6210_host_ussd: complete=1 firmware_state_forcing=0')
    reply={'type':'incoming_ussd_response','epoch':1,'request_id':1,'dcs':15,'data':'32','empty':False}
    return '\n'.join(lines),profile(3,True),'host_ussd_reply: '+json.dumps(reply)


class HostNotificationTests(unittest.TestCase):
    def test_pending_editor_restores_under_new_host_epoch(self):
        text,card,console=request_fixture()
        text=text.replace('epoch=1 phase=answered','epoch=2 phase=answered').replace(
            'reply id=1 epoch=1','reply id=1 epoch=2')
        marker='6210_host_ussd_input: key=Keypad 2 state=1 t=30.'
        boundary=('6210_ussd_restore: saved t=28.000000000 pc=0000001c ram=02f11025\n'
                  '6210_ussd_restore: architecture=pass\n'
                  'incoming ussd state id=1 epoch=2 phase=queued\n')
        text=text.replace(marker,boundary+marker)
        console=console.replace('"epoch": 1','"epoch": 2')
        check_request(text,card,console,restored=True)
        for old,new in (('architecture=pass','architecture=fail'),
                        ('epoch=2 phase=queued','epoch=1 phase=queued'),
                        ('reply id=1 epoch=2','reply id=1 epoch=1'),
                        ('saved t=28.000000000','saved t=31.000000000')):
            with self.subTest(old=old),self.assertRaises(ValueError):
                check_request(text.replace(old,new),card,console,restored=True)
        with self.assertRaises(ValueError):
            check_request(text,card,console.replace('"epoch": 2','"epoch": 1'),restored=True)

    def test_ordinary_request_cannot_stand_in_for_restoration(self):
        with self.assertRaises(ValueError):
            check_request(*request_fixture(),restored=True)

    def test_interactive_request_returns_exact_host_reply(self):
        check_request(*request_fixture())

    def test_reply_requires_exact_component_identity_payload_and_physical_keys(self):
        text,card,console=request_fixture()
        for old,new in (('operation=request','operation=notify'),
                        ('key=Keypad 2 state=1 t=30.','key=Keypad 3 state=1 t=30.'),
                        ('epoch=1 phase=answered','epoch=2 phase=answered'),
                        ('dcs=0f data=32 empty=0','dcs=0f data=33 empty=0'),
                        ('300604010f040132','300604010f040133'),
                        ('p2=01 p3=08','p2=02 p3=08')):
            with self.subTest(old=old),self.assertRaises(ValueError):
                check_request(text.replace(old,new),card,console)

    def test_host_received_json_is_independent_required_evidence(self):
        text,card,console=request_fixture()
        reply=json.loads(console.split(': ',1)[1])
        for key,value in (('request_id',2),('epoch',2),('data','33'),('dcs',0),
                          ('empty',True),('empty',0),('epoch',True)):
            changed={**reply,key:value}
            with self.subTest(key=key,value=value),self.assertRaises(ValueError):
                check_request(text,card,'host_ussd_reply: '+json.dumps(changed))
        with self.assertRaises(ValueError):
            check_request(text,card,'')
        with self.assertRaises(ValueError):
            check_request(text,card,console+'\n'+console)

    def test_reply_cannot_precede_rr_release(self):
        text,card,console=request_fixture()
        line='gsm_call_adapter: incoming ussd reply id=1 epoch=1 dcs=0f data=32 empty=0'
        text=text.replace(line+'\n','')
        text=text.replace('LAPDm service Channel Release acknowledged nr=6',
                          line+'\nLAPDm service Channel Release acknowledged nr=6')
        with self.assertRaises(ValueError):
            check_request(text,card,console)

    def test_notification_and_physical_recovery(self):
        check_notification(*fixture())

    def test_wrong_epoch_message_operation_input_or_order_fails(self):
        text,card=fixture()
        for old,new in (('epoch=1 phase=delivered','epoch=2 phase=delivered'),
                        ('phase=delivered','phase=queued'),('operation=notify','operation=request'),
                        ('arfcn=35','arfcn=1'),('t=20.010000','t=19.010000'),
                        ('length=29','length=28'),('data=8b3a05a203020101','data=8b3a05a203020102'),
                        ('key=Right Softkey / C','key=End'),('complete=1','complete=0')):
            with self.subTest(old=old),self.assertRaises(ValueError):
                check_notification(text.replace(old,new),card)

    def test_duplicate_admission_or_notification_fails(self):
        text,card=fixture()
        for line in ('gsm_call_adapter: incoming ussd id=1 result=accepted t=20.010000',
                     'gsm_ss: network_initiated operation=notify transaction=0b invoke=1 dcs=0f'):
            with self.subTest(line=line),self.assertRaises(ValueError):
                check_notification(text+'\n'+line,card)

    def test_whole_card_and_no_credential_or_record_writes(self):
        text,card=fixture()
        for index in (0,1604,3484,3516,3520,3569):
            changed=bytearray(card)
            changed[index]^=1
            with self.subTest(index=index),self.assertRaises(ValueError):
                check_notification(text,bytes(changed))
        for ins in (4,0x24,0x26,0x28,0x2c,0x44,0xdc):
            with self.subTest(ins=ins),self.assertRaises(ValueError):
                check_notification(text+f'\nSIM status ins={ins:02x} sw=9000 chv=3/3 puk=10/10 enabled=1',card)


if __name__=='__main__':
    unittest.main()

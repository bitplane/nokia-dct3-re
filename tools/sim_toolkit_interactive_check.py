"""Wire-level lab-card DISPLAY TEXT / GET INKEY / GET INPUT contract.

Product wrappers own authentication, storage, configuration and reviewed pixels.
"""
import re
from tools.sim_toolkit_display_check import DISPLAY, RESPONSE, wire
from tools.sim_fdn_protocol_check import apdus
from tools.sim_security_trace_check import statuses

INKEY = bytes.fromhex('d0138103022200820281828d0804') + b'Press 5'
INPUT = bytes.fromhex('d0188103032300820281828d0904') + b'Enter 42' + bytes.fromhex('91020102')
INKEY_RESPONSE = bytes.fromhex('8103022200020282810301000d020435')
INPUT_RESPONSE = bytes.fromhex('8103032300020282810301000d03043432')
MENU = bytes.fromhex('d0268103042500820281828509') + b'DCT3 menu' + bytes.fromhex('8f0901') + b'Continue' + bytes.fromhex('8f0502') + b'Exit'
MENU_RESPONSE = bytes.fromhex('810304250002028281030100')
SELECT_ITEM = bytes.fromhex('d0208103072400820281828506') + b'Choose' + bytes.fromhex('8f0601') + b'Alpha' + bytes.fromhex('8f0502') + b'Beta'
ITEM_RESPONSE = bytes.fromhex('810307240002028281030100100102')
REFRESH = bytes.fromhex('d009810309010082028182')
REFRESH_RESPONSE = bytes.fromhex('810309010002028281030100')
PLAY_TONE = bytes.fromhex('d01a81030c2000820281038508')+b'SAT tone'+bytes.fromhex('8e011084020102')
TONE_CANCEL_RESPONSE = bytes.fromhex('81030c200002028281030111')
TONE_COMPLETE_RESPONSE = bytes.fromhex('81030c200002028281030100')


def check_interactive(text, *, prefix, terminal_profile, reset_markers=(), menu_item=None, select_item=False, refresh=False, tone=None):
    if tone is not None:
        if tone not in ('cancel','hold','late') or select_item or refresh or menu_item not in (None,1):
            raise ValueError('PLAY TONE requires distinct Continue profile')
        menu_item=1
    if refresh:
        if select_item or menu_item not in (None,1):
            raise ValueError('REFRESH requires distinct Continue profile')
        menu_item=1
    if select_item:
        if menu_item not in (None,1):
            raise ValueError('follow-up item requires Continue')
        menu_item=1
    if menu_item not in (None,1,2):
        raise ValueError('unknown installed-menu item')
    if any(marker in text for marker in (*reset_markers,'LUA ERROR','Error in')):
        raise ValueError('reset or failed observer')
    if re.search(r'illegal instruction|unimplemented|unsupported|unhandled|fatal',text,re.I):
        raise ValueError('execution failed')
    tx,rx = (wire(text,direction,prefix,limit=16384 if refresh else 8192)
             for direction in ('TX','RX'))
    commands = list(apdus(tx))
    allowed=(0xa4,0x20,0xb0,0xb2,0xc0,0xf2,0x10,0x12,0x14)
    if any(row[0] not in allowed+((0xc2,) if menu_item is not None else ()) for row in commands):
        raise ValueError('unrelated SIM operation')
    expected = [(0x10,0,0,len(terminal_profile),terminal_profile)]
    exchanges=[(DISPLAY,RESPONSE),(INKEY,INKEY_RESPONSE),(INPUT,INPUT_RESPONSE)]
    if menu_item is not None:
        exchanges.append((MENU,MENU_RESPONSE))
    for payload,response in exchanges:
        expected += [(0x12,0,0,len(payload),b''),(0x14,0,0,len(response),response)]
        if rx.count(payload+b'\x90\x00') != 1:
            raise ValueError('missing unique card command payload')
    if menu_item is not None:
        expected.append((0xc2,0,0,9,bytes.fromhex('d307020201811001')+bytes((menu_item,))))
    if select_item:
        expected += [(0x12,0,0,len(SELECT_ITEM),b''),(0x14,0,0,len(ITEM_RESPONSE),ITEM_RESPONSE)]
        if rx.count(SELECT_ITEM+b'\x90\x00') != 1:
            raise ValueError('missing unique follow-up item payload')
    if refresh:
        expected += [(0x12,0,0,len(REFRESH),b''),
                     (0x10,0,0,len(terminal_profile),terminal_profile),
                     (0x14,0,0,len(REFRESH_RESPONSE),REFRESH_RESPONSE)]
        if rx.count(REFRESH+b'\x90\x00') != 1:
            raise ValueError('missing unique REFRESH payload')
    if tone is not None:
        expected.append((0x12,0,0,len(PLAY_TONE),b''))
        if tone!='hold':
            response=TONE_CANCEL_RESPONSE if tone=='cancel' else TONE_COMPLETE_RESPONSE
            expected.append((0x14,0,0,len(response),response))
        if rx.count(PLAY_TONE+b'\x90\x00') != 1:
            raise ValueError('missing unique PLAY TONE payload')
    if [row[:5] for row in commands if row[0] in (0x10,0x12,0x14,0xc2)] != expected:
        raise ValueError('interactive profile/FETCH/response sequence differs')
    if 'read-binary fid=6fae offset=0 length=1 first=03' not in text or text.count('SIM completion ins=f2 sw=9116') != 1:
        raise ValueError('Phase-2+ pending command differs')
    results=[(0x9115,3,10),(0x911a,3,10),(0x9000,3,10)]
    if menu_item is not None:
        results[-1:]=[(0x9128,3,10),(0x9000,3,10)]
    if select_item or refresh or tone in ('cancel','late'):
        results.append((0x9000,3,10))
    if (statuses(text,0x10) != [(0x9000,3,10)]*(2 if refresh else 1) or statuses(text,0x14) != results
            or statuses(text,0xc2) != ([(0x911c if tone is not None else 0x910b if refresh else 0x9122 if select_item else 0x9000,3,10)] if menu_item is not None else [])):
        raise ValueError('interactive response status differs')
    marker = prefix+'_toolkit_physical: action=dismiss'
    if re.findall(re.escape(marker)+r' down=([01]) t=([0-9.]+)',text) != [('1','68.000000000'),('0','68.200000000')]:
        raise ValueError('physical display dismissal differs')
    actions = ('inkey_5','inkey_confirm','input_4','input_2','input_confirm')
    action_marker = prefix+'_toolkit_input:'
    if re.findall(re.escape(action_marker)+r' action=(\w+) down=([01])',text) != [(action,down) for action in actions for down in ('1','0')]:
        raise ValueError('physical input sequence differs')
    if text.count(action_marker) != 10 or text.count(prefix+'_toolkit_physical:') != 2:
        raise ValueError('extra or malformed physical actions')
    for contact,count,payload in (
            (marker+' down=1',0,DISPLAY),
            (action_marker+' action=inkey_5 down=1',1,INKEY),
            (action_marker+' action=inkey_confirm down=1',1,INKEY),
            (action_marker+' action=input_4 down=1',2,INPUT),
            (action_marker+' action=input_2 down=1',2,INPUT),
            (action_marker+' action=input_confirm down=1',2,INPUT)):
        before=text.split(contact,1)[0]
        if sum(row[0]==0x14 for row in apdus(wire(before,'TX',prefix))) != count:
            raise ValueError('response precedes physical input/confirmation')
        if payload+b'\x90\x00' not in wire(before,'RX',prefix):
            raise ValueError('physical input precedes card command')
    if menu_item is not None:
        selection=prefix+'_toolkit_menu: action=select down=1'
        if text.count(selection) != 1:
            raise ValueError('missing unique physical menu selection')
        before=text.split(selection,1)[0]
        if any(row[0]==0xc2 for row in apdus(wire(before,'TX',prefix))):
            raise ValueError('menu envelope precedes physical selection')
        if MENU+b'\x90\x00' not in wire(before,'RX',prefix) or len(statuses(before,0x14)) != 4:
            raise ValueError('menu installation incomplete before selection')
    if select_item:
        marker=prefix+'_toolkit_select_item:'
        if (re.findall(re.escape(marker)+r' action=(\w+) down=([01])',text) !=
                [(action,down) for action in ('next','confirm') for down in ('1','0')]
                or text.count(marker) != 4):
            raise ValueError('physical follow-up selection differs')
        before=text.split(marker+' action=confirm down=1',1)[0]
        if (len(statuses(before,0x14)) != 4
                or sum(row[0]==0x14 for row in apdus(wire(before,'TX',prefix))) != 4
                or SELECT_ITEM+b'\x90\x00' not in wire(before,'RX',prefix)):
            raise ValueError('follow-up response precedes physical confirmation')
    if tone is not None:
        marker=prefix+'_toolkit_tone: action=quit'
        actions=re.findall(re.escape(marker)+r' down=([01])',text)
        if actions != (['1','0'] if tone=='cancel' else []) or text.count(prefix+'_toolkit_tone:') != len(actions):
            raise ValueError('physical tone cancellation differs')
        if tone=='cancel':
            before=text.split(marker+' down=1',1)[0]
            if len(statuses(before,0x14)) != 4 or PLAY_TONE+b'\x90\x00' not in wire(before,'RX',prefix):
                raise ValueError('tone cancellation precedes command or response precedes physical Quit')

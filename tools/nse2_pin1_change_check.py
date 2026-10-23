"""Check physical PIN1-change prerequisite and exact persistent credential delta."""
import hashlib
import re

from tools.nse2_sim_check import HEADER, verify_pin_input
from tools.sim_security_trace_check import CHV_OFFSET, statuses

FRAMES={False:'a540a88d8d2ade3a99553db0d87a377aed84c647e18bb4f1fbf7c2138c20a8e0',
        True:'82c943a25c4025adc755d663babdcc496f9afca5771b917aa170f2b9af8e7c94'}
REQUEST_FRAMES={'on':'4ea4497469ca2f510f0ee5eaac9673cc8fdf6dfaecf548f5d8b2da532a3a57ad',
                'off':'3591202ddf6025a234fcccc896583f70d647ff2e739634a337776f0da80e726d'}


def check_boot_pin_contacts(text,enabled):
    edges=re.findall(r'nse2_pin_contact: event=(\d+) down=([01]) t=([0-9.]+) physical_only=1',text)
    if len(edges)!=(10 if enabled else 0):
        raise ValueError('PIN1 boot authentication contacts differ')
    for i,(event,down,when) in enumerate(edges):
        start=6+(i//2)*.35 if i<8 else 7.6
        if int(event)!=i+1 or int(down)!=1-i%2 or abs(float(when)-(start+i%2*.15))>.04:
            raise ValueError('PIN1 boot input timing/order differs')


def check_request(text,before,after,pixels,*,mode):
    if mode not in ('on','off','cold-off') or len(before)!=3570:
        raise ValueError('known PIN request phase and complete card required')
    expected=bytearray(before)
    if before[3520]!=int(mode=='off'):
        raise ValueError('PIN request predecessor state differs')
    normalized=bytearray(before)
    normalized[3520]=1
    if verify_pin_input(normalized)!=3:
        raise ValueError('known credential and fresh retries required')
    expected[3520]=int(mode=='on')
    if after!=expected:
        raise ValueError('PIN request changed unrelated card storage')
    check_boot_pin_contacts(text,mode=='off')
    headers=[tuple(int(v,16) for v in m.groups()[:6])+(float(m[7]),)
             for m in HEADER.finditer(text)]
    sensitive=[h for h in headers if h[1] in (0x20,0x24,0x26,0x28,0x2c,0x04,0x44,0xdc,0xd6)]
    wanted=[] if mode=='cold-off' else ([(0xa0,0x20,0,1,8,0x6fad)] if mode=='off' else [])+[(0xa0,0x28 if mode=='on' else 0x26,0,1,8,0x6f3a)]
    if [h[:6] for h in sensitive]!=wanted:
        raise ValueError('PIN request command sequence differs')
    if mode!='cold-off' and not 23.5<sensitive[-1][-1]<24.5:
        raise ValueError('PIN request toggle timing differs')
    if mode=='off' and not 7.6<sensitive[0][-1]<8:
        raise ValueError('cold enabled PIN authentication timing differs')
    for ins in (0x20,0x26,0x28):
        count=sum(h[1]==ins for h in sensitive)
        if statuses(text,ins)!=[(0x9000,3,10)]*count:
            raise ValueError('PIN request security status differs')
    edges=re.findall(r'nse2_pin_request_contact: event=(\d+) down=([01]) t=([0-9.]+) physical_only=1',text)
    if len(edges)!=(0 if mode=='cold-off' else 32):
        raise ValueError('PIN request physical contacts differ')
    for i,(event,down,when) in enumerate(edges):
        if int(event)!=i+1 or int(down)!=1-i%2 or abs(float(when)-(18.5+i//2*.35+i%2*.15))>.04:
            raise ValueError('PIN request physical timing/order differs')
    if mode!='cold-off' and hashlib.sha256(pixels).hexdigest()!=REQUEST_FRAMES[mode]:
        raise ValueError('PIN request On/Off result frame differs')
    return dict(pin_request_enabled=mode=='on',cold_disabled_bypass=mode=='cold-off',
                native_registration_verified=False)


def check_change(text,before,after,pixels,*,enabled):
    if len(before)!=3570:
        raise ValueError('complete laboratory card required')
    normalized=bytearray(before)
    if before[3520]!=int(enabled):
        raise ValueError('PIN1 checking capability differs')
    normalized[3520]=1
    if verify_pin_input(normalized)!=3:
        raise ValueError('fresh PIN1 retries required')
    expected=bytearray(before)
    if enabled:
        expected[CHV_OFFSET:CHV_OFFSET+8]=b'2468'+b'\xff'*4
    if after!=expected:
        raise ValueError('PIN1 change modified unrelated storage')
    headers=[tuple(int(v,16) for v in m.groups()[:6])+(float(m[7]),)
             for m in HEADER.finditer(text)]
    sensitive=[h for h in headers if h[1] in (0x20,0x24,0x26,0x28,0x2c,0x04,0x44,0xdc,0xd6)]
    wanted=[(0xa0,0x20,0,1,8,0x6fad),(0xa0,0x24,0,1,16,0x6f3a)] if enabled else []
    if [h[:6] for h in sensitive]!=wanted:
        raise ValueError('PIN1 authentication/change command sequence differs')
    if enabled and not (7.6<sensitive[0][-1]<8 and 28.5<sensitive[1][-1]<29):
        raise ValueError('PIN1 command timing differs')
    for ins in (0x20,0x24):
        if statuses(text,ins)!=([(0x9000,3,10)] if enabled else []):
            raise ValueError('PIN1 command did not succeed')
    check_boot_pin_contacts(text,enabled)
    edges=re.findall(r'nse2_pin1_change_contact: event=(\d+) down=([01]) t=([0-9.]+) physical_only=1',text)
    if len(edges)!=(60 if enabled else 30):
        raise ValueError('PIN1 physical menu contacts differ')
    for i,(event,down,when) in enumerate(edges):
        if int(event)!=i+1 or int(down)!=1-i%2 or abs(float(when)-(18.5+i//2*.35+i%2*.15))>.04:
            raise ValueError('PIN1 physical menu timing/order differs')
    if hashlib.sha256(pixels).hexdigest()!=FRAMES[enabled]:
        raise ValueError('PIN1 prerequisite/change result frame differs')
    return dict(pin1_changed=enabled,disabled_request_refused=not enabled,
                native_registration_verified=False)

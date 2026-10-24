"""Own-PMM 8850 host notification, physical readback and exact card checks."""
import hashlib
from pathlib import Path
import re

from tools.make_sim_card_profile import make_profile
from tools.radio_call_lifecycle_common import require_ordered
from tools.radio_host_incoming_ussd_trace_check import verify as verify_protocol

TEXT = '7938f0209e4ddd0f34e778a4da4be56adf8435c32618be9144afc5874e91e1f0'
IDLE = 'd6d4b05af24a06c42a97e31c3134f7e497e149f4420f87f3d243613f06926300'
MENU_TITLE = '1e5c11fcea9aac5331e18c0070697e8795003d6de9a0250284b3d9605209e654'
HOST_RESULT = '664c8b0e4e2a25ba77ce867b05d60171dccf60b93d136adcba638be9a24bb353'


def check_contacts(text, contacts):
    actual = re.findall(r'8850_host_ussd_input: key=(.*?) state=([01]) t=([0-9.]+)', text)
    expected = [(key, str(state), f'{second}.{fraction}')
                for key, second in contacts
                for state, fraction in ((1, '000000000'), (0, '200000000'))]
    if actual != expected or text.count('8850_host_ussd_input:') != len(actual):
        raise ValueError('physical contacts differ, are malformed or remain pressed')


def check_card(after, card):
    if (re.search(r'sim_device: (?:update|unblock)', after) or
            re.search(r'sim_device: header cla=a0 ins=(?:24|26|28|2c|d6|dc)\b', after)):
        raise ValueError('USSD changed SIM files or credentials')
    expected = bytearray(make_profile(True) + bytes.fromhex('00f110') + b'\xff' * 43)
    expected[1604:1609] = bytes.fromhex('00f1100001')
    expected[1610] = 0
    if card != expected:
        raise ValueError('complete card differs from authenticated registered laboratory profile')


def verify(text, card):
    verify_protocol(text)
    if '[LUA ERROR]' in text or text.count('8850_host_ussd: complete') != 1:
        raise ValueError('notification input fixture failed')
    admissions = re.findall(r'incoming ussd id=1 result=accepted t=([0-9.]+)', text)
    if len(admissions) != 1 or not 30 <= float(admissions[0]) < 31:
        raise ValueError('notification must follow settled authentication once')
    if re.findall(r'incoming ussd state id=1 epoch=(\d+) phase=(\w+)', text) != [('1', 'queued'), ('1', 'delivered')]:
        raise ValueError('notification identity/phase lifecycle differs')
    if text.count('network_initiated operation=notify ') != 1:
        raise ValueError('expected exactly one network notification')
    require_ordered(text, (
        ('physical PIN OK', r'8850_pin_physical: key=Menu'),
        ('PIN accepted', r'SIM status ins=20 sw=9000'),
        ('registration', r'LAPDm Location Updating Accept acknowledged'),
        ('admission', r'incoming ussd id=1 result=accepted'),
        ('notify', r'network_initiated operation=notify transaction=0b invoke=1 dcs=0f'),
        ('result', r'network_initiated handset_response message=3a length=8 data=8b3a05a203020101'),
        ('release', r'LAPDm service Channel Release acknowledged'),
        ('delivered', r'incoming ussd state id=1 epoch=1 phase=delivered'),
        ('Read', r'8850_host_ussd_input: key=Menu state=1 t=35\.000000000'),
        ('End', r'8850_host_ussd_input: key=End state=1 t=38\.000000000'),
        ('Menu', r'8850_host_ussd_input: key=Menu state=1 t=41\.000000000'),
    ), '8850 host USSD')
    expected_contacts = [('Menu', 35), ('End', 38), ('Menu', 41)]
    check_contacts(text, expected_contacts)
    after = text.split('incoming ussd id=1 result=accepted', 1)[1]
    check_card(after, card)


def verify_network_request(text, card, *, restored=False, empty=False):
    if restored and empty:
        raise ValueError('restored empty-editor dismissal is not qualified')
    if '[LUA ERROR]' in text or text.count('8850_host_ussd: complete') != 1:
        raise ValueError('network request input fixture failed')
    admissions = re.findall(r'incoming ussd id=1 result=accepted t=([0-9.]+)', text)
    if len(admissions) != 1 or not 30 <= float(admissions[0]) < 31:
        raise ValueError('network request must follow settled authentication once')
    epoch = '2' if restored else '1'
    phases = [('1', 'queued'), ('2', 'queued'), ('2', 'answered')] if restored else [('1', 'queued'), ('1', 'answered')]
    if re.findall(r'incoming ussd state id=1 epoch=(\d+) phase=(\w+)', text) != phases:
        raise ValueError('network request identity/phase lifecycle differs')
    if text.count('network_initiated operation=request ') != 1:
        raise ValueError('expected exactly one network request')
    contacts = ([('Menu', 35), ('Menu', 37), ('End', 39), ('Menu', 45)] if empty else
                [('Menu', 35), ('Menu', 37), ('Keypad 2', 39),
                 ('Menu', 41), ('End', 45), ('Menu', 47)])
    result = ('length=8 data=8b3a05a203020101' if empty else
              'length=21 data=8b3a12a210020101300b02013c300604010f040132')
    reply = 'dcs=00 data= empty=1' if empty else 'dcs=0f data=32 empty=0'
    check_contacts(text, contacts)
    require_ordered(text, (
        ('PIN', r'SIM status ins=20 sw=9000'),
        ('registration', r'LAPDm Location Updating Accept acknowledged'),
        ('admission', r'incoming ussd id=1 result=accepted t=30\.'),
        ('request', r'network_initiated operation=request transaction=0b invoke=1 dcs=0f'),
        *[(key, re.escape(f'8850_host_ussd_input: key={key} state=1 t={second}.000000000'))
          for key, second in contacts[:3 if empty else 4]],
        ('result', r'network_initiated handset_response message=3a ' + result),
        ('release', r'LAPDm service Channel Release acknowledged'),
        ('reply', rf'incoming ussd reply id=1 epoch={epoch} ' + reply),
        ('answered', rf'incoming ussd state id=1 epoch={epoch} phase=answered'),
        ('recovery', rf'8850_host_ussd_input: key=Menu state=0 t={contacts[-1][1]}\.200000000'),
    ), '8850 network request')
    if text.count('incoming ussd reply ') != 1:
        raise ValueError('duplicate network request reply')
    check_card(text.split('incoming ussd id=1 result=accepted', 1)[1], card)
    if restored:
        check_restoration(text, prefix='8850_network_ussd_state', scenario='nsm2_network_ussd')
        require_ordered(text, (
            ('request', r'network_initiated operation=request'),
            ('save', r'8850_network_ussd_state: event=saved'),
            ('restore', r'8850_network_ussd_state: event=restored'),
            ('republication', r'incoming ussd state id=1 epoch=2 phase=queued'),
            ('physical Read', r'8850_host_ussd_input: key=Menu state=1 t=35\.'),
        ), '8850 pending network request')


def check_network_request_frames(directory, *, empty=False):
    if empty:
        _frames(directory, (
            ('8850_ussd_action_1.png', 'f5d05b3cb51c3058f10fd0dce547b7ce7b3d416e97410ea2afa7e1b7c635315d', None),
            ('8850_ussd_action_2.png', '772d9bcf2602786c164c063b4358b9f74df74257289b671a6302a724ab2651a8', None),
            ('8850_ussd_action_3.png', IDLE, None),
            ('8850_ussd_action_4.png', MENU_TITLE, (0, 0, 72, 16)),
        ))
        return
    _frames(directory, (
        ('8850_ussd_action_1.png', 'f5d05b3cb51c3058f10fd0dce547b7ce7b3d416e97410ea2afa7e1b7c635315d', None),
        ('8850_ussd_action_3.png', 'a9c79a6b61112a58b1d21567712dc371311ea7de7606cb28fdea0e312aca79c4', None),
        ('8850_ussd_action_4.png', '6213cd3c42a877704ac80826e4f16706f12736d2abfe4fc9713dc9186c649a34', None),
        ('8850_ussd_action_6.png', MENU_TITLE, (0, 0, 72, 16)),
    ))


def verify_outgoing(text, card, *, restored=False):
    from tools.radio_host_ussd_trace_check import verify as verify_host
    from tools.noki8850_ussd_check import KEYS
    if '[LUA ERROR]' in text:
        raise ValueError('physical USSD fixture failed')
    verify_host(text, require_restore=restored)
    if text.count('gsm_ss: request=ussd_host ') != 1:
        raise ValueError('expected one firmware-owned USSD transaction')
    checkpoints = [('PIN accepted', r'SIM status ins=20 sw=9000'),
                   ('registered', r'LAPDm Location Updating Accept acknowledged')]
    checkpoints += [(key, re.escape('8850_ussd_physical: key=' + key)) for key in KEYS]
    checkpoints += [('request', r'gsm_ss: request=ussd_host id=1 .*dcs=0f packed_length=5')]
    if restored:
        check_restoration(text)
        checkpoints += [
            ('initial request', r'ussd request id=1 epoch=1'),
            ('save', r'8850_ussd_state: event=saved'),
            ('restore', r'8850_ussd_state: event=restored'),
            ('republication', r'ussd request id=1 epoch=2'),
            ('old epoch rejected', r'ussd response id=1 outcome=0 result=rejected'),
        ]
    checkpoints += [
        ('wrong ID rejected', r'ussd response id=2 outcome=0 result=rejected'),
        ('response accepted', r'ussd response id=1 outcome=0 result=accepted'),
        ('RR released', r'LAPDm service Channel Release acknowledged'),
        ('Back', r'8850_ussd_physical: key=Back'),
        ('Menu', r'8850_host_ussd_recovery: Menu press'),
        ('Menu release', r'8850_host_ussd_recovery: Menu release'),
    ]
    require_ordered(text, checkpoints, '8850 host-decided USSD')
    if re.findall(r'ussd request id=1 epoch=(\d+)', text) != (['1', '2'] if restored else ['1']):
        raise ValueError('expected one correlated firmware USSD request')
    epoch = '2' if restored else '1'
    if re.findall(r'ussd state id=1 epoch=(\d+) phase=(\w+)', text) != [(epoch, 'accepted'), (epoch, 'ended')]:
        raise ValueError('outgoing USSD lifecycle differs')
    check_card(text.split('gsm_ss: request=ussd_host id=1', 1)[1], card)


def check_restoration(text, *, prefix='8850_ussd_state', scenario='nsm2_ussd'):
    from tools.arm_architecture_snapshot_check import verify_roundtrip
    verify_roundtrip(text, prefix=prefix, scenario=scenario)
    before = text.split(f'{prefix}: event=restored', 1)[0]
    if ('ussd response id=' in before or 'incoming ussd reply ' in before or
            'GSM service downlink kind=27 sapi=0 pd=0b' in before):
        raise ValueError('host answered or released before pending restoration')


def check_outgoing_frames(directory):
    _frames(directory, (
        ('8850_ussd_result.png', HOST_RESULT, None),
        ('8850_ussd_after_back.png', IDLE, None),
        ('8850_host_ussd_menu.png', MENU_TITLE, (0, 0, 72, 16)),
    ))


def check_frames(directory: Path):
    _frames(directory, (
        ('8850_ussd_action_1.png', TEXT, None),
        ('8850_ussd_action_2.png', IDLE, None),
        # Existing own-product title crop excludes animated envelope/scrollbar.
        ('8850_ussd_action_3.png', MENU_TITLE, (0, 0, 72, 16)),
    ))


def _frames(directory, specifications):
    from PIL import Image
    for name, expected, crop in specifications:
        with Image.open(directory / name) as image:
            if image.size != (84, 48):
                raise ValueError('unexpected 8850 display geometry')
            pixels = image.convert('L')
            if crop:
                pixels = pixels.crop(crop)
            if hashlib.sha256(pixels.tobytes()).hexdigest() != expected:
                raise ValueError(f'{name}: reviewed content differs')

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
    for state, fraction in ((1, '000000000'), (0, '200000000')):
        contacts = re.findall(rf'8850_host_ussd_input: key=(.*?) state={state} t=(\d+)\.{fraction}', text)
        if contacts != [(key, str(second)) for key, second in expected_contacts]:
            raise ValueError('physical contacts differ or remain pressed')
    after = text.split('incoming ussd id=1 result=accepted', 1)[1]
    if (re.search(r'sim_device: (?:update|unblock)', after) or
            re.search(r'sim_device: header cla=a0 ins=(?:24|26|28|2c|d6|dc)\b', after)):
        raise ValueError('notification changed SIM files or credentials')
    expected = bytearray(make_profile(True) + bytes.fromhex('00f110') + b'\xff' * 43)
    expected[1604:1609] = bytes.fromhex('00f1100001')
    expected[1610] = 0
    if card != expected:
        raise ValueError('complete card differs from authenticated registered laboratory profile')


def check_frames(directory: Path):
    from PIL import Image
    for name, expected, crop in (
        ('8850_ussd_action_1.png', TEXT, None),
        ('8850_ussd_action_2.png', IDLE, None),
        # Existing own-product title crop excludes animated envelope/scrollbar.
        ('8850_ussd_action_3.png', MENU_TITLE, (0, 0, 72, 16)),
    ):
        with Image.open(directory / name) as image:
            if image.size != (84, 48):
                raise ValueError('unexpected 8850 display geometry')
            pixels = image.convert('L')
            if crop:
                pixels = pixels.crop(crop)
            if hashlib.sha256(pixels.tobytes()).hexdigest() != expected:
                raise ValueError(f'{name}: reviewed content differs')

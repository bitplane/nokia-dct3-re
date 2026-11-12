"""Check native ROM4 Location Updating on the laboratory BTS."""
import hashlib
import re
import sys
from pathlib import Path

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.npe3_raw_ua_check import DISC_UA, LU, RELEASE, UA, UPLINK

AGCH = re.compile(r'npe3_raw_agch: ra=([0-9a-f]{2}) access_frame=(\d+) start_frame=(\d+) '
                  r'octets=([0-9a-f]{46}) raw_adc_only=1 tuned_air_source=0 physical_claim=0')
FAULT = re.compile(r'rom4_reset_request|TMS320C54x.*illegal|unsupported.*staged|staged.*unsupported|Lua error')
# Power-on holds, releases, resets and re-releases the DSP once; an MCU
# restart (such as a reason-68 reset) repeats that cycle.
BOOT_RESET_LINES = 3
# nse1ap5 final LCD: operator "DCT3 LAB", no accessory title, full bars, Menu softkey.
NSE1_LCD_SHA256 = 'b7aba0c6e5cb3c488833b9295d818962a9f3f920364d73b27e0080d01e8e0c6f'


def check(text):
    """Return the dialogue frames; raise if any step is missing, reordered or malformed."""
    if FAULT.search(text):
        raise ValueError('native run reset or faulted')
    if text.count('native_dsp_reset_line:') != BOOT_RESET_LINES:
        raise ValueError('MCU restarted the DSP after boot')
    agch = list(AGCH.finditer(text))
    if len(agch) != 1 or text.count('npe3_raw_agch:') != 1:
        raise ValueError('missing unique Immediate Assignment')
    ra, access, assign = int(agch[0][1], 16), int(agch[0][2]), int(agch[0][3])
    if ra >> 5 != 0 or assign <= access:
        raise ValueError('access burst is not a location-updating request answered later')
    uplinks = [(int(row[1]), bytes.fromhex(row[2])) for row in UPLINK.finditer(text)]
    if len(uplinks) != text.count('npe3_bts_uplink:'):
        raise ValueError('uplink block without an exact convolutional decode')
    rows = {}
    for name, marker, pattern in (('ua', 'ua', UA), ('accept', 'lu_accept', LU),
                                  ('release', 'release', RELEASE), ('disc_ua', 'disc_ua', DISC_UA)):
        found = list(pattern.finditer(text))
        if len(found) != 1 or text.count(f'npe3_raw_{marker}:') != 1:
            raise ValueError(f'missing unique {name} reply')
        rows[name] = (int(found[0][1]), int(found[0][2]))
    by_frame = dict(uplinks)
    request = by_frame.get(rows['ua'][0], b'')
    if request[:3] != b'\x01\x3f\x49' or request[3:5] != b'\x05\x08':
        raise ValueError('SABM does not carry an MM Location Updating Request')
    if rows['accept'][0] != rows['ua'][0] or by_frame.get(rows['release'][0], b'')[:3] != b'\x03\x21\x01':
        raise ValueError('Accept or Release is not bound to the transmitted request and acknowledgement')
    if by_frame.get(rows['disc_ua'][0], b'')[:3] != b'\x01\x53\x01':
        raise ValueError('closing UA is not bound to a transmitted DISC')
    order = [access, assign, rows['ua'][0], rows['ua'][1], rows['accept'][1], rows['release'][0],
             rows['release'][1], rows['disc_ua'][0], rows['disc_ua'][1]]
    if order != sorted(order) or len(set(order)) != len(order):
        raise ValueError('Location Updating dialogue is out of order')
    return dict(access_frame=access, assignment_frame=assign, request_frame=rows['ua'][0],
                accept_frame=rows['accept'][1], release_frame=rows['release'][1],
                closing_ua_frame=rows['disc_ua'][1], uplinks=len(uplinks))


PAGE_ACCESS = re.compile(r'npe3_bts_page_access: t=[\d.]+ ra=([0-9a-f]{2}) last_page_frame=(\d+) port_bits_only=1 physical_claim=0')
PAGE_REQUEST = re.compile(r'npe3_bts_page_request: t=[\d.]+ identity=([0-9a-f]{16}) ')
SETUP = re.compile(r'npe3_raw_call_setup: uplink_start_frame=(\d+) start_frame=(\d+) ')
REPLY = re.compile(r'npe3_raw_(call_rr|cc_release|call_channel_release|disc_ua): \w+_start_frame=(\d+) start_frame=(\d+) ')
# Mobile-terminated call control in uplink order (GSM 04.08 table 9.54, N(SD) masked).
CALL_MESSAGES = (('call_confirmed', 0x08), ('alerting', 0x01), ('connect', 0x07),
                 ('disconnect', 0x25), ('release_complete', 0x2a))


def check_incoming_call(text):
    """Check an answered and cleared mobile-terminated call after registration.

    Registration must already have been checked on the text before the page.
    """
    marker = text.find('npe3_bts_page_request:')
    if marker < 0:
        raise ValueError('laboratory BTS never paged the handset')
    text = text[marker:]
    if FAULT.search(text) or 'native_dsp_reset_line:' in text:
        raise ValueError('native run reset or faulted during the call')
    request = PAGE_REQUEST.search(text)
    access = PAGE_ACCESS.findall(text)
    agch = list(AGCH.finditer(text))
    if not request or len(access) != 1 or len(agch) != 1 or agch[0][1] != access[0][0]:
        raise ValueError('paging was not answered by one access burst and assignment')
    if int(access[0][0], 16) >> 4 != 1:
        raise ValueError('access burst cause is not an answer to paging')
    uplinks = [(int(row[1]), bytes.fromhex(row[2])) for row in UPLINK.finditer(text)]
    if len(uplinks) != text.count('npe3_bts_uplink:'):
        raise ValueError('uplink block without an exact convolutional decode')
    response = [frame for frame, block in uplinks
                if block[1] == 0x3f and block[3:5] == b'\x06\x27' and block[11:19].hex() == request[1]]
    setup = SETUP.findall(text)
    if len(response) != 1 or len(setup) != 1 or int(setup[0][0]) != response[0]:
        raise ValueError('SETUP is not bound to a Paging Response carrying the paged identity')
    frames = {}
    for frame, block in uplinks:
        length = block[2] >> 2
        if block[2] & 3 != 1 or length < 2 or block[3] != 0x83:
            continue
        for name, kind in CALL_MESSAGES:
            if block[4] & 0x3f == kind:
                frames.setdefault(name, frame)
    if list(frames) != [name for name, _ in CALL_MESSAGES]:
        raise ValueError(f'call control messages missing or out of order: {list(frames)}')
    replies = {kind: (int(source), int(start)) for kind, source, start in REPLY.findall(text)
               if kind != 'call_rr'}
    if (replies.get('cc_release', (0,))[0] != frames['disconnect'] or
            replies.get('call_channel_release', (0,))[0] != frames['release_complete'] or
            'disc_ua' not in replies or replies['disc_ua'][0] <= replies['call_channel_release'][1]):
        raise ValueError('network clearing is not bound to DISCONNECT, RELEASE COMPLETE and DISC')
    order = [int(access[0][1]), int(agch[0][3]), response[0], int(setup[0][1]), *frames.values()]
    if order != sorted(order):
        raise ValueError('incoming call dialogue is out of order')
    return dict(page_frame=int(access[0][1]), response_frame=response[0], setup_frame=int(setup[0][1]),
                **{f'{name}_frame': frame for name, frame in frames.items()},
                call_closing_ua_frame=replies['disc_ua'][1])


def check_lcd_sequence(directory, required, final):
    """Require each frame in order among the LCD captures, and the final frame."""
    digests = [hashlib.sha256(path.read_bytes()).hexdigest()
               for path in sorted(Path(directory).glob('nokia_dct3_lcdmirror_*.pgm'))]
    position = 0
    for digest in required:
        if digest not in digests[position:]:
            raise ValueError(f'LCD never showed {digest[:12]} in order')
        position = digests.index(digest, position) + 1
    if not digests or digests[-1] != final:
        raise ValueError('final LCD differs from the registered idle frame')


def check_lcd(directory, expected):
    frames = sorted(Path(directory).glob('nokia_dct3_lcdmirror_*.pgm'))
    if not frames:
        raise ValueError('no LCD capture')
    digest = hashlib.sha256(frames[-1].read_bytes()).hexdigest()
    if digest != expected:
        raise ValueError(f'final LCD differs from the registered idle frame: {digest}')

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
# nse1ap5 final LCD: operator "DCT3 LAB", full bars, Menu softkey.
NSE1_LCD_SHA256 = '377631960815d6cffd94ca5a2991ababca1d763050a1766d1579450e808599af'


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


def check_lcd(directory, expected):
    frames = sorted(Path(directory).glob('nokia_dct3_lcdmirror_*.pgm'))
    if not frames:
        raise ValueError('no LCD capture')
    digest = hashlib.sha256(frames[-1].read_bytes()).hexdigest()
    if digest != expected:
        raise ValueError(f'final LCD differs from the registered idle frame: {digest}')

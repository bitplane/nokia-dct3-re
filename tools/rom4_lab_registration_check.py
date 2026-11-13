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


TCH_START = re.compile(r'npe3_tch_start: first_frame=(\d+) timeslot=1 tsc=2 ')
TCH_SPEECH = re.compile(r'npe3_tch_start: .* speech=tone_1000hz ')
TCH_AUDIO = re.compile(r'npe3_tch_audio: earpiece_hz=1000 earpiece_blocks=(\d+) earpiece_tone_blocks=(\d+) '
                       r'earpiece_tone_amplitude=\d+ microphone_hz=500 uplink_blocks=(\d+) uplink_tone_blocks=(\d+) ')
TCH_UPLINK = re.compile(r'npe3_tch_uplink: channel=(facch|sacch) start_frame=(\d+) octets=([0-9a-f]{46}) exact_code=(\d)')
TCH_REPLY = re.compile(r'npe3_tch_facch: kind=(\w+) uplink_start_frame=(\d+) ')
TCH_ASSIGNMENT = re.compile(r'npe3_raw_assignment: confirmed_start_frame=(\d+) start_frame=(\d+) ')
# FACCH uplink in order: link establishment, RR Assignment Complete, then call
# control on the new channel and its clearing (GSM 04.08 9.1.3, table 9.54).
TCH_MESSAGES = (('sabm', lambda block: block[:3] == b'\x01\x3f\x01'),
                ('assignment_complete', lambda block: block[3:5] == b'\x06\x29'),
                ('alerting', lambda block: block[3] == 0x83 and block[4] & 0x3f == 0x01),
                ('connect', lambda block: block[3] == 0x83 and block[4] & 0x3f == 0x07),
                ('disconnect', lambda block: block[3] == 0x83 and block[4] & 0x3f == 0x25),
                ('release_complete', lambda block: block[3] == 0x83 and block[4] & 0x3f == 0x2a),
                ('disc', lambda block: block[:3] == b'\x01\x53\x01'))
# Each network FACCH reply and the uplink message it answers.
TCH_BINDINGS = (('ua', 'sabm'), ('connect_acknowledge', 'connect'), ('cc_release', 'disconnect'),
                ('channel_release', 'release_complete'), ('disc_ua', 'disc'))


def check_traffic_call(text):
    """Check the call after CALL CONFIRMED moves to TCH/F on timeslot 1."""
    assignment = TCH_ASSIGNMENT.findall(text)
    if len(assignment) != 1:
        raise ValueError('missing unique Assignment Command')
    text = text[text.find('npe3_raw_assignment:'):]
    if FAULT.search(text) or 'native_dsp_reset_line:' in text:
        raise ValueError('native run reset or faulted on the traffic channel')
    start = TCH_START.findall(text)
    if len(start) != 1 or int(start[0]) < int(assignment[0][1]) + 4:
        raise ValueError('traffic channel did not start after the Assignment Command')
    uplinks = [(kind, int(frame), bytes.fromhex(octets), code == '1')
               for kind, frame, octets, code in TCH_UPLINK.findall(text)]
    facch = [(frame, block) for kind, frame, block, exact in uplinks if kind == 'facch']
    if len(facch) != sum(exact for kind, _, _, exact in uplinks if kind == 'facch'):
        raise ValueError('FACCH uplink block without an exact convolutional decode')
    reports = [frame for kind, frame, block, exact in uplinks
               if kind == 'sacch' and exact and block[5:7] == b'\x06\x15']
    if len(reports) < 4:
        raise ValueError('too few SACCH Measurement Reports decoded on the traffic channel')
    frames = {}
    for frame, block in facch:
        for name, match in TCH_MESSAGES:
            if match(block):
                frames.setdefault(name, frame)
    if list(frames) != [name for name, _ in TCH_MESSAGES]:
        raise ValueError(f'traffic-channel messages missing or out of order: {list(frames)}')
    replies = {}
    for kind, source in TCH_REPLY.findall(text):
        replies.setdefault(kind, int(source))
    for kind, message in TCH_BINDINGS:
        if replies.get(kind) != frames[message]:
            raise ValueError(f'{kind} is not bound to the first {message}')
    if int(start[0]) > frames['sabm']:
        raise ValueError('FACCH uplink precedes the traffic channel')
    result = dict(tch_first_frame=int(start[0]), **{f'tch_{name}_frame': frame for name, frame in frames.items()},
                  measurement_reports=len(reports),
                  facch_retransmissions=text.count('npe3_tch_facch_recovery:'))
    if TCH_SPEECH.search(text):
        result.update(check_traffic_audio(text))
    return result


HOST_PHASE = re.compile(r'npe3_host_call: request=1 epoch=\d+ phase=(\w+)')
# Network-initiated clearing on FACCH (GSM 04.08 5.4.4): the lab's DISCONNECT,
# the handset's RELEASE, RELEASE COMPLETE, Channel Release and the final DISC.
HOST_CLEARING = (('net', 'cc_disconnect'), ('ms', 'release'), ('net', 'cc_release_complete'),
                 ('net', 'channel_release'), ('ms', 'disc'), ('net', 'disc_ua'))


def check_host_lab(text, phases=('paged', 'alerting', 'connected', 'network_clearing', 'ended')):
    """Lab side of a host-placed traffic-channel call that the host clears."""
    if FAULT.search(text) or 'native_dsp_reset_line: released=0' in text.split('phase=paged', 1)[-1]:
        raise ValueError('native run reset or faulted during the host call')
    seen = HOST_PHASE.findall(text)
    if seen != list(phases):
        raise ValueError(f'host call phases out of order: {seen}')
    if 'phase=paged digits=4 ' not in text:
        raise ValueError('host caller digits did not reach SETUP')
    events = []
    clearing = text[text.find('phase=network_clearing'):]
    for line in clearing.splitlines():
        reply = TCH_REPLY.search(line)
        if reply:
            events.append(('net', reply.group(1)))
        uplink = TCH_UPLINK.search(line)
        if uplink and uplink.group(1) == 'facch' and uplink.group(4) == '1':
            block = bytes.fromhex(uplink.group(3))
            if block[3] & 15 == 3 and block[4] & 0x3f == 0x2d:
                events.append(('ms', 'release'))
            elif block[:3] == b'\x01\x53\x01':
                events.append(('ms', 'disc'))
    order = iter(events)
    if not all(step in order for step in HOST_CLEARING):
        raise ValueError(f'network clearing out of order: {events}')
    # Host media flows only after CONNECT, so require an absolute duration of
    # the host's 1 kHz tone at the earpiece rather than a share of the call.
    audio = TCH_AUDIO.findall(text)
    if len(audio) != 1 or int(audio[0][1]) < 100:
        raise ValueError('host downlink tone did not reach the earpiece for 2 s')
    return dict(earpiece_tone_blocks=int(audio[0][1]))


def check_host_call(text, client):
    """Check a host-placed traffic-channel call that the host clears."""
    result = check_host_lab(text)
    if [phase for phase in client['phases'] if phase != 'media_closed'] != ['alerting', 'connected', 'ended']:
        raise ValueError(f'host saw phases {client["phases"]}')
    if client['uplink_good'] < 100 or client['downlink'] < 100:
        raise ValueError('too little host media in either direction')
    if client['uplink_microphone_fraction'] < 0.5:
        raise ValueError('host uplink lacks the handset microphone tone')
    return dict(host_uplink_frames=client['uplink_good'], host_downlink_frames=client['downlink'],
                host_microphone_fraction=client['uplink_microphone_fraction'], **result)


def check_traffic_audio(text):
    """Require the lab's tones through the native speech codecs both ways."""
    audio = TCH_AUDIO.findall(text)
    if len(audio) != 1:
        raise ValueError('missing unique traffic-channel audio summary')
    earpiece, earpiece_tone, uplink, uplink_tone = map(int, audio[0])
    # Each count is of 20 ms blocks; muted and lost blocks are expected.
    for name, blocks, tone in (('earpiece 1 kHz', earpiece, earpiece_tone), ('uplink 500 Hz', uplink, uplink_tone)):
        if tone < 50 or 2 * tone < blocks:
            raise ValueError(f'{name} tone in only {tone} of {blocks} blocks')
    return dict(earpiece_tone_blocks=earpiece_tone, uplink_tone_blocks=uplink_tone)


def check_roundtrip(text):
    """Require the mid-call save to reload and replay the DSP's I/O exactly."""
    if not re.search(r'state_roundtrip: result=pass ', text):
        raise ValueError('state round trip did not pass')
    if not re.search(r'rom4_codec_replay: result=pass ', text):
        raise ValueError('DSP I/O diverged after reloading the mid-call state')
    return dict(state_roundtrip='pass')

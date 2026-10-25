"""Bounded NSE-2 proactive DISPLAY TEXT refusal; not Toolkit display support."""
import hashlib
import re

from tools.sim_fdn_protocol_check import apdus

DISPLAY = bytes.fromhex('d0148103012180820281028d0904') + b'DCT3 SAT'
RESPONSE = bytes.fromhex('810301218002028281030130')
EEPROM_HASHES = ('9af2e8f7d726da1807577341af5885c7455ef4c7c4b7ca694afd67f753dfb645',
                 'ba749b88d2e1f770b62a1b58b2255d29689315f11a29bd50e0037c368ed2190d')


def check_storage(control, proactive):
    for name in ('flash', 'sim_card'):
        if control[name] != proactive[name]:
            raise ValueError('unexpected persistent ' + name + ' difference')
    for endpoint, expected in zip((control, proactive), EEPROM_HASHES):
        if hashlib.sha256(endpoint['eeprom']).hexdigest() != expected:
            raise ValueError('reviewed complete EEPROM endpoint differs')
    differences = [(i, a, b) for i, (a, b) in
                   enumerate(zip(control['eeprom'], proactive['eeprom'])) if a != b]
    if differences != [(0x6df + 33 * i, 0, 1) for i in range(18)]:
        raise ValueError('reviewed EEPROM byte delta differs')


def check_trace(text, proactive):
    if any(token in text for token in ('LUA ERROR', 'Error in', 'nse2_reset_entry:')):
        raise ValueError('observer failure or handset reset')
    values = re.findall(r'nse2_toolkit_wire: direction=TX data=([0-9a-f]{2}) ', text)
    if not values:
        raise ValueError('missing SIM wire capture')
    commands = list(apdus(bytes.fromhex(''.join(values))))
    toolkit = [row[:5] for row in commands if row[0] in (0x10, 0x12, 0x14, 0xc2)]
    expected = [(0x10, 0, 0, 4, bytes.fromhex('03018000')),
                (0x12, 0, 0, 22, b''), (0x14, 0, 0, 12, RESPONSE)] if proactive else []
    if toolkit != expected:
        raise ValueError('own terminal profile, FETCH or capability refusal differs')
    updates = [row for row in commands if row[0] == 0xdc]
    if len(updates) != 1 or updates[0][1:4] != (1, 4, 32) or updates[0][5] != 0x6f3a:
        raise ValueError('physical contact save differs')
    status = '9116' if proactive else '9000'
    if text.count(f'SIM status ins=dc sw={status} ') != 1:
        raise ValueError('contact completion/pending status differs')
    if proactive:
        rx = bytes.fromhex(''.join(re.findall(
            r'nse2_toolkit_wire: direction=RX data=([0-9a-f]{2}) ', text)))
        if rx.count(DISPLAY + b'\x90\x00') != 1:
            raise ValueError('missing exact fetched card payload')
        if text.count('SIM status ins=14 sw=9000 ') != 1:
            raise ValueError('terminal response not accepted')
        update = next(i for i, row in enumerate(commands) if row[0] == 0xdc)
        fetch = next(i for i, row in enumerate(commands) if row[0] == 0x12)
        if update >= fetch:
            raise ValueError('FETCH did not follow contact-save notification')
    contacts = re.findall(r'nse2_phonebook_contact: event=(\d+) down=([01]) ', text)
    if contacts != [(str(i), str(i % 2)) for i in range(1, 17)]:
        raise ValueError('physical contact sequence differs')
    return dict(proactive=proactive, fetched=proactive,
                capability_refusal=proactive, toolkit_display_supported=False)

"""Check own NSE-2 SIM reads and separately scoped physical phone-code failure."""
import hashlib
import re

SECURITY_FRAME = 'ec2809a9dff350a769c7315f5fc3b2a8fb29e534595c2b3c2a7dc0f6cc4f9466'
CODE_ERROR_FRAME = '042030d4e6f3a8e75d15f45e5ef6ea8cff68f50038e98d2d7a76cb64c489f6a9'
AUTHENTICATED_FRAME = 'df66881e695c25b4a157c80b40cf56ad246bb45a9d54c1054264222a639db931'
PHONEBOOK_MENU_FRAME = 'eba0749f25b43e63dea723e7ca8e8f1051385648fa90278af2de39659c04989e'
PHONEBOOK_SUBMENU_FRAME = '8e39080ec840c45849dbc34110a5b800de17726429b62d58f0fc2e0b4a227369'
PHONEBOOK_SAVED_FRAME = '8f9188505262676c92fd1ec55e94d6e9dbf2854bc5d14fc4d2ac3dc0903d6909'
PHONEBOOK_RELOAD_FRAME = '4755eca080af19866a8da926e4185de6b1508f6271a3f4d116dc590d12d0e76a'
PHONEBOOK_EMPTY_SEARCH_FRAME = '34478708c13e25cc3cb0778a9fa40f9cb812c6eb56636909c1ce133a1d616900'
BINARY_FILES = {0x6fb7: 15, 0x6fae: 1, 0x6f05: 4, 0x6fad: 3,
                0x6f38: 12, 0x6f07: 9, 0x6f78: 2, 0x2fe6: 4,
                0x6f46: 17, 0x6f74: 16, 0x6f7e: 11, 0x6f30: 24,
                0x6f20: 9, 0x6f7b: 12, 0x6f31: 1, 0x6f43: 2}
HEADER = re.compile(r'sim_device: header cla=([0-9a-f]{2}) ins=([0-9a-f]{2}) '
                    r'p1=([0-9a-f]{2}) p2=([0-9a-f]{2}) p3=([0-9a-f]{2}) '
                    r'selected=([0-9a-f]{4}) t=([0-9.]+)')
COMPLETION = re.compile(r'SIM completion ins=([0-9a-f]{2}) sw=([0-9a-f]{4})')


def verify_sim_reads(text, pixels):
    pending, records, binary = None, [], []
    later_reads = 0
    for line in text.splitlines():
        header = HEADER.search(line)
        if header:
            values = header.groups()
            pending = tuple(int(value, 16) for value in values[:6]) + (float(values[6]),)
            if pending[1] == 0x20:
                raise ValueError('NSE-2 PIN-disabled fixture unexpectedly issued VERIFY CHV')
        completion = COMPLETION.search(line)
        if not completion:
            continue
        ins, status = (int(value, 16) for value in completion.groups())
        if ins not in (0xb0, 0xb2):
            continue
        if pending is not None and pending[-1] >= 10:
            # Later application reads are not extra bootstrap records. The
            # complete initial file/record sets below must still be present.
            later_reads += 1
            pending = None
            continue
        if pending is None or pending[0] != 0xa0 or pending[1] != ins or status != 0x9000:
            raise ValueError('NSE-2 SIM read completion missing its header or successful status')
        _, _, p1, p2, length, fid, time = pending
        if ins == 0xb0:
            binary.append((fid, p1 * 256 + p2, length))
        else:
            records.append((fid, p1, p2, length, time))
        pending = None
    if sorted(binary) != sorted((fid, 0, size) for fid, size in BINARY_FILES.items()):
        raise ValueError('NSE-2 successful binary-file bootstrap differs')
    for fid, count, size in ((0x6f3a, 50, 32), (0x6f3c, 10, 176)):
        rows = [row for row in records if row[0] == fid]
        if [(row[1], row[2], row[3]) for row in rows] != [(i, 4, size) for i in range(1, count + 1)]:
            raise ValueError(f'NSE-2 complete successful record reads missing for {fid:04x}')
    if len(records) != 60 or not 4 < max(row[4] for row in records) < 10:
        raise ValueError('NSE-2 SIM record bootstrap horizon differs')
    if hashlib.sha256(pixels).hexdigest() != SECURITY_FRAME:
        raise ValueError('NSE-2 reviewed phone Security code frame differs')
    return dict(sim_read_bootstrap=True, adn_records_read=50, sms_records_read=10,
                bootstrap_read_horizon_seconds=10, later_application_reads=later_reads,
                binary_files_read=16, phone_code_authenticated=False, sim_pin_verified=False)


def verify_phonebook_save(text, data, pixels):
    from tools.sim_phonebook_check import validate_phonebook
    contacts = re.findall(r'nse2_phonebook_contact: event=(\d+) down=([01]) t=([0-9.]+) physical_only=1', text)
    if [(int(index),int(down)) for index,down,_ in contacts] != [
            (index, index % 2) for index in range(1,17)]:
        raise ValueError('NSE-2 physical Add name contact sequence incomplete')
    times = [float(time) for _,_,time in contacts]
    if times != sorted(times) or not 23 < times[0] < times[-1] < 28:
        raise ValueError('NSE-2 contact save crossed its pre-restart observation window')
    validate_phonebook(text,data,b'A')
    if 'SIM status ins=dc sw=9000' not in text:
        raise ValueError('NSE-2 contact save successful card status missing')
    if hashlib.sha256(pixels).hexdigest() != PHONEBOOK_SAVED_FRAME:
        raise ValueError('NSE-2 reviewed Saved frame differs')
    return dict(name='A', number='123', sim_update_and_storage=True,
                saved_frame=True, cold_reload_verified=False, stable_long_boot=False)


def verify_phonebook_reload(text, initial, final, pixels):
    from tools.sim_phonebook_check import validate_phonebook_storage, ADN_LENGTH, SUPPORTED_NVRAM_LENGTHS
    contacts = re.findall(r'nse2_phonebook_reload_contact: event=(\d+) down=([01]) t=([0-9.]+) physical_only=1',text)
    if [(int(index),int(down)) for index,down,_ in contacts] != [(i,i%2) for i in range(1,7)]:
        raise ValueError('NSE-2 physical cold search contact sequence differs')
    times = [float(time) for _,_,time in contacts]
    if times != sorted(times) or not 23 < times[0] < times[-1] < 26:
        raise ValueError('NSE-2 cold search timing differs')
    if any(int(match[2],16)==0xdc for match in HEADER.finditer(text)):
        raise ValueError('NSE-2 cold search unexpectedly issued UPDATE RECORD')
    present = initial is not None
    if present:
        validate_phonebook_storage(initial,b'A')
        if final != initial:
            raise ValueError('NSE-2 cold contact search changed saved card storage')
    elif len(final) not in SUPPORTED_NVRAM_LENGTHS or final[:ADN_LENGTH] != bytes([255])*ADN_LENGTH:
        raise ValueError('NSE-2 empty-card negative control contains a contact')
    expected = PHONEBOOK_RELOAD_FRAME if present else PHONEBOOK_EMPTY_SEARCH_FRAME
    if hashlib.sha256(pixels).hexdigest() != expected:
        raise ValueError('NSE-2 cold contact search frame differs')
    return dict(contact_present=present, cold_reload_verified=present,
                empty_card_control=not present, card_modified=False,
                number_displayed=False, call_signaling_proved=False, stable_long_boot=False)


def verify_security_attempt(text, pixels, *, retry_zero=False):
    contacts = re.findall(r'nse2_security_contact: index=([1-6]) down=([01]) '
                          r't=([0-9.]+) physical_only=1', text)
    if [(int(i), int(down)) for i, down, _ in contacts] != [(i, down) for i in range(1, 7) for down in (1, 0)]:
        raise ValueError('NSE-2 one physical phone-code attempt missing')
    decoded = re.findall(r'nse2_key_decode: raw=(\d+) key=([0-9a-f]{2}) pc=00299044 '
                         r't=([0-9.]+) observer_writes=0', text)
    for i, (raw, key) in enumerate(((7, 1), (8, 2), (9, 3), (12, 4), (13, 5), (11, 0x19))):
        start, end = float(contacts[2 * i][2]), float(contacts[2 * i + 1][2])
        expected = 12 + i * 0.5 if i < 5 else 15
        if not expected <= start < expected + 0.04 or not 0.18 <= end - start <= 0.24:
            raise ValueError('NSE-2 physical phone-code contact timing differs')
        if not any(int(r) == raw and int(k, 16) == key and start <= float(t) <= end for r, k, t in decoded):
            raise ValueError('NSE-2 phone-code contacts did not reach the own decoder')
    expected_frame = AUTHENTICATED_FRAME if retry_zero else CODE_ERROR_FRAME
    if hashlib.sha256(pixels).hexdigest() != expected_frame:
        raise ValueError('NSE-2 reviewed phone-code result frame differs')
    counter = '00' if retry_zero else 'ff'
    required = (
        r'nse2_security_encode: input=313233343500 destination=[0-9a-f]{8} lr=0029469b t=([0-9.]+) observer_writes=0',
        r'nse2_security_compare: first=0010c6f8 second=[0-9a-f]{8} length=4 a=([0-9a-f]{8}) b=([0-9a-f]{8}) lr=002946a5 t=([0-9.]+) observer_writes=0',
        rf'nse2_security_result: comparison=00000000 attempts={counter} t=([0-9.]+) observer_writes=0',
        rf'nse2_security_retry: matched=1 attempts={counter} lr=002946b3 t=([0-9.]+) observer_writes=0',
    )
    matches = [re.findall(pattern, text) for pattern in required]
    if any(len(rows) != 1 for rows in matches):
        raise ValueError('NSE-2 matching phone-code/retry rejection evidence missing')
    first, second, compare_time = matches[1][0]
    times = [float(matches[0][0]), float(compare_time), float(matches[2][0]), float(matches[3][0])]
    if first != second or times != sorted(times) or not 15 <= times[0] <= times[-1] < 15.3:
        raise ValueError('NSE-2 matching phone-code/retry rejection ordering differs')
    if retry_zero:
        returns = re.findall(r'nse2_security_return: accepted=1 attempts=00 t=([0-9.]+) observer_writes=0', text)
        if len(returns) != 1 or not times[-1] <= float(returns[0]) < 15.3:
            raise ValueError('NSE-2 own verifier successful return missing')
    return dict(physical_phone_code_attempts=1, phone_code_authenticated=retry_zero,
                phone_code_result='accepted; application UI unproved' if retry_zero else 'Code error',
                exact_editor_buffer_unproved=False, usable_boot=False,
                submitted_code_matches_cached_record=True, retry_counter=0 if retry_zero else 255,
                rejection_boundary=None if retry_zero else 'own retry guard at 29461c')


def verify_post_auth_menu(text, menu_pixels, submenu_pixels):
    contacts = re.findall(r'nse2_post_auth_contact: index=([12]) down=([01]) t=([0-9.]+) physical_only=1', text)
    if [(int(i), int(down)) for i, down, _ in contacts] != [(1, 1), (1, 0), (2, 1), (2, 0)]:
        raise ValueError('NSE-2 post-authentication physical Menu sequence missing')
    decoded = re.findall(r'nse2_key_decode: raw=11 key=19 pc=00299044 t=([0-9.]+) observer_writes=0', text)
    for i, expected in enumerate((20, 22)):
        start, end = float(contacts[2 * i][2]), float(contacts[2 * i + 1][2])
        if not expected <= start < expected + 0.04 or not 0.18 <= end - start <= 0.24:
            raise ValueError('NSE-2 post-authentication Menu timing differs')
        if not any(start <= float(time) <= end for time in decoded):
            raise ValueError('NSE-2 post-authentication Menu did not reach own decoder')
    if len(menu_pixels) != 84 * 48 or len(submenu_pixels) != 84 * 48:
        raise ValueError('NSE-2 post-authentication frame dimensions differ')
    masked = bytearray(menu_pixels)
    # Exclude the animated Phone book icon, retaining title, softkey and scrollbar.
    for y in range(20, 38):
        masked[y * 84:y * 84 + 76] = bytes([255]) * 76
    if hashlib.sha256(masked).hexdigest() != PHONEBOOK_MENU_FRAME:
        raise ValueError('NSE-2 reviewed Phone book menu differs')
    if hashlib.sha256(submenu_pixels).hexdigest() != PHONEBOOK_SUBMENU_FRAME:
        raise ValueError('NSE-2 reviewed Search/Add name/Erase submenu differs')
    return dict(application_menu=True, phonebook_submenu=True, contact_persistence=False,
                usable_boot=True, network_registration=False)

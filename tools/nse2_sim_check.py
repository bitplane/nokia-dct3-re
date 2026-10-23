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
SMS_SAVED_FRAME = '491f056f1e92fdedbe7e334f423818c8b1770db0838a78bf25743e0ec2f0459c'
SMS_READ_FRAME = '4877eac15193be5a99aad660db563aa3d4780c90ec47d349eb19448789fa31a9'
SMS_ERASE_PROMPT_FRAME = '87fecc9ef97a2a8add1a94e8c6a4168577bc1726da1931ef95f0a91eaa62a441'
SMS_ERASED_FRAME = 'a3e10fc73a63b9d342f3474168d10a19fc3bf2c582b560fe5817937d62316265'
SMS_EMPTY_FRAME = 'ae8c6f125e67dd0878a5833b4309071851689684cef3e7b51c6bf89c3bc4e113'
PIN_PROMPT_FRAME = '02d05af86d04ea0db15a0f55824bc395cd221bdaed95d3712b3d9c3bd5f22678'
PIN_ACCEPTED_FRAME = 'd14c9f37fc63a2fba3dc668f80e9b795b6e0babddd8c29f54f7eae5031e44473'
PIN_BLOCKED_FRAME = 'b60f2dfc2e7bfde5aadfb61423c726f2a57cfb1f760cb1aa3d80fb4abce7f8c6'
PUK_PROMPT_FRAME = '4d0f73d4e4c35b48ad96c8535094a1f4bd05895ff9b51bdfd07c98a3a727c9bd'
PIN2_CHANGE_FRAMES=(AUTHENTICATED_FRAME,'dcb11fd985f9fadefab2e1aece85f5c36be7243c66e1c2d7abe149b0bef49157',
                   '78852f434f196bba8c74f94f888c21f47962425559a7f6b18c7932af85116b1e',
                   '859255eea46cf0fcf25c17b9fcf86a9a41e2476ffc3ba6b5eb999adc162a7426',
                   '2ee325765504951d8758a418ae0fa0e90c8252d9fd6234fba603eef0360e811e')
PIN2_BLOCKED_FRAME='8743820344d395aaf8c7701cf0085c7cad646e06c0b4dcef3bb8979c953b594f'
PUK2_PROMPT_FRAME='e293d1f5987c36caea43654daa9615a5990cce239403d6f1c3a15e842c184089'
PIN2_REJECTED_FRAME='e5cc1d2bab0e40f55bfd249883a731df355df5f551f03520c29957e9cabcb239'
PIN2_ITEM_FRAME='96d2f3fb3ee239cede15f5d48b0c62910809d5acd4d79c1db0910522d86e488f'
PHONE_CODE_ITEM_FRAME='6c973f265be558dbe2730a733535604e5baac36b9682124ef6e8cd267f022dfd'
BINARY_FILES = {0x6fb7: 15, 0x6fae: 1, 0x6f05: 4, 0x6fad: 3,
                0x6f38: 12, 0x6f07: 9, 0x6f78: 2, 0x2fe6: 4,
                0x6f46: 17, 0x6f74: 16, 0x6f7e: 11, 0x6f30: 24,
                0x6f20: 9, 0x6f7b: 12, 0x6f31: 1, 0x6f43: 2}
HEADER = re.compile(r'sim_device: header cla=([0-9a-f]{2}) ins=([0-9a-f]{2}) '
                    r'p1=([0-9a-f]{2}) p2=([0-9a-f]{2}) p3=([0-9a-f]{2}) '
                    r'selected=([0-9a-f]{4}) t=([0-9.]+)')
COMPLETION = re.compile(r'SIM completion ins=([0-9a-f]{2}) sw=([0-9a-f]{4})')


def verify_sim_reads(text, pixels, *, pin_authenticated=False, bootstrap_horizon=10,later_chv2=False):
    if pin_authenticated:
        from tools.sim_security_trace_check import statuses
        if statuses(text,0x20)!=[(0x9000,3,10)]:
            raise ValueError('NSE-2 PIN-authenticated bootstrap lacks a successful VERIFY')
    pending, records, binary = None, [], []
    later_reads = 0
    pin_ready_time = None
    for line in text.splitlines():
        header = HEADER.search(line)
        if header:
            values = header.groups()
            pending = tuple(int(value, 16) for value in values[:6]) + (float(values[6]),)
            if pending[1] == 0x20 and not pin_authenticated:
                # A later CHV2 workflow must not turn the bootstrap into a
                # PIN1-authenticated claim or admit an early/unrelated VERIFY.
                if not (later_chv2 and pending[:5]==(0xa0,0x20,0,2,8) and pending[-1]>bootstrap_horizon):
                    raise ValueError('NSE-2 PIN-disabled fixture unexpectedly issued VERIFY CHV')
        if pin_authenticated and 'SIM status ins=20 sw=9000 ' in line:
            if pending is None or pending[1]!=0x20:
                raise ValueError('NSE-2 successful PIN status lacks its VERIFY header')
            pin_ready_time=pending[-1]
        completion = COMPLETION.search(line)
        if not completion:
            continue
        ins, status = (int(value, 16) for value in completion.groups())
        if ins not in (0xb0, 0xb2):
            continue
        if pending is not None and pending[-1] >= bootstrap_horizon:
            # Later application reads are not extra bootstrap records. The
            # complete initial file/record sets below must still be present.
            later_reads += 1
            pending = None
            continue
        if pending is None or pending[0] != 0xa0 or pending[1] != ins or status != 0x9000:
            raise ValueError('NSE-2 SIM read completion missing its header or successful status')
        _, _, p1, p2, length, fid, time = pending
        if pin_authenticated and fid in (0x6f07,0x6f3a,0x6f3c) and (pin_ready_time is None or time<=pin_ready_time):
            raise ValueError('NSE-2 protected SIM read preceded successful PIN verification')
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
    if len(records) != 60 or not 4 < max(row[4] for row in records) < bootstrap_horizon:
        raise ValueError('NSE-2 SIM record bootstrap horizon differs')
    if hashlib.sha256(pixels).hexdigest() != SECURITY_FRAME:
        raise ValueError('NSE-2 reviewed phone Security code frame differs')
    return dict(sim_read_bootstrap=True, adn_records_read=50, sms_records_read=10,
                bootstrap_read_horizon_seconds=bootstrap_horizon, later_application_reads=later_reads,
                binary_files_read=16, phone_code_authenticated=False, sim_pin_verified=pin_authenticated)


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


def verify_sms_save(text, card, pixels):
    from tools.sim_sms_draft import check_card
    from tools.sim_security_trace_check import statuses
    contacts = re.findall(r'nse2_sms_contact: event=(\d+) down=([01]) t=([0-9.]+) physical_only=1',text)
    if [(int(index),int(down)) for index,down,_ in contacts] != [(i,i%2) for i in range(1,23)]:
        raise ValueError('NSE-2 physical draft contact sequence differs')
    times = [float(time) for _,_,time in contacts]
    if times != sorted(times) or not 19 < times[0] < times[-1] < 27:
        raise ValueError('NSE-2 offline draft exceeded its pre-restart window')
    writes = [match.groups() for match in HEADER.finditer(text) if int(match[2],16)==0xdc]
    if len(writes)!=1 or writes[0][:6]!=('a0','dc','01','04','b0','6f3c'):
        raise ValueError('NSE-2 exact EF_SMS UPDATE RECORD header missing')
    if (text.count('sim_device: update fid=6f3c record=1 length=176')!=1
            or statuses(text,0xdc)!=[(0x9000,3,10)]):
        raise ValueError('NSE-2 successful single SMS draft commit missing')
    check_card(card)
    if hashlib.sha256(pixels).hexdigest()!=SMS_SAVED_FRAME:
        raise ValueError('NSE-2 reviewed Message saved frame differs')
    return dict(offline_draft_saved=True, text='A', cold_readback_verified=False,
                network_sms_proved=False, stable_long_boot=False)


def verify_sms_reload(text, initial, final, pixels):
    from tools.sim_sms_draft import check_card
    from tools.sim_security_trace_check import statuses
    contacts = re.findall(r'nse2_sms_reload_contact: event=(\d+) down=([01]) t=([0-9.]+) physical_only=1',text)
    if [(int(index),int(down)) for index,down,_ in contacts] != [(i,i%2) for i in range(1,13)]:
        raise ValueError('NSE-2 physical Outbox read contact sequence differs')
    times = [float(time) for _,_,time in contacts]
    if times!=sorted(times) or not 19<times[0]<times[-1]<24:
        raise ValueError('NSE-2 physical Outbox timing differs')
    check_card(initial)
    if initial!=final:
        raise ValueError('NSE-2 cold Outbox read changed saved card storage')
    if any(int(match[2],16)==0xdc for match in HEADER.finditer(text)) or statuses(text,0xdc):
        raise ValueError('NSE-2 cold Outbox read unexpectedly rewrote a SIM record')
    if hashlib.sha256(pixels).hexdigest()!=SMS_READ_FRAME:
        raise ValueError('NSE-2 reviewed cold SMS reader frame differs')
    return dict(offline_draft_read=True,text='A',cold_readback_verified=True,
                card_modified=False,network_sms_proved=False,stable_long_boot=False)


def verify_sms_action(text, initial, final, prompt_pixels, result_pixels, action):
    from tools.sim_sms_draft import check_card,check_deleted_card,deleted_card
    from tools.sim_security_trace_check import statuses
    if action not in ('erase','cancel','empty'):
        raise ValueError('unknown NSE-2 SMS lifecycle action')
    count=10 if action=='empty' else 18
    contacts=re.findall(r'nse2_sms_reload_contact: event=(\d+) down=([01]) t=([0-9.]+) physical_only=1',text)
    if [(int(index),int(down)) for index,down,_ in contacts]!=[(i,i%2) for i in range(1,count+1)]:
        raise ValueError('NSE-2 SMS confirmation contact sequence differs')
    times=[float(time) for _,_,time in contacts]
    if times!=sorted(times) or not 19<times[0]<times[-1]<(23 if action=='empty' else 26):
        raise ValueError('NSE-2 SMS confirmation timing differs')
    (check_deleted_card if action=='empty' else check_card)(initial)
    expected=deleted_card(initial) if action=='erase' else initial
    if final!=expected:
        raise ValueError('NSE-2 SMS lifecycle changed unexpected card bytes')
    writes=[match.groups() for match in HEADER.finditer(text) if int(match[2],16)==0xdc]
    if action=='erase':
        if (len(writes)!=1 or writes[0][:6]!=('a0','dc','01','04','b0','6f3c')
                or text.count('sim_device: update fid=6f3c record=1 length=176')!=1
                or statuses(text,0xdc)!=[(0x9000,3,10)]):
            raise ValueError('NSE-2 erase lacks one successful exact EF_SMS update')
    elif writes or statuses(text,0xdc):
        raise ValueError('NSE-2 SMS cancel/cold-empty unexpectedly wrote a record')
    if action!='empty' and hashlib.sha256(prompt_pixels).hexdigest()!=SMS_ERASE_PROMPT_FRAME:
        raise ValueError('NSE-2 reviewed Erase message prompt differs')
    expected_frame={'erase':SMS_ERASED_FRAME,'cancel':SMS_READ_FRAME,'empty':SMS_EMPTY_FRAME}[action]
    if hashlib.sha256(result_pixels).hexdigest()!=expected_frame:
        raise ValueError('NSE-2 SMS lifecycle result frame differs')
    return dict(action=action,card_modified=action=='erase',cold_empty_verified=action=='empty',
                network_sms_proved=False,stable_long_boot=False)


def verify_pin_input(card, *, blocked=False, pin=b'1234', puk_attempts=10):
    from tools.make_sim_card_profile import make_profile
    from tools.sim_security_trace_check import ATTEMPTS_OFFSET,AOC_NVRAM_SIZE,CHV_OFFSET
    if len(card) not in (len(make_profile(True)),AOC_NVRAM_SIZE):
        raise ValueError('NSE-2 PIN input card has an unsupported storage size')
    initial=bytearray(make_profile(True))
    if puk_attempts not in range(1,11) or (not blocked and puk_attempts!=10):
        raise ValueError('reduced PUK1 retries require a blocked card')
    initial[ATTEMPTS_OFFSET+2]=puk_attempts
    if pin not in (b'1234',b'2468'):
        raise ValueError('unreviewed NSE-2 PIN credential')
    initial[CHV_OFFSET:CHV_OFFSET+8]=pin.ljust(8,b'\xff')
    retries=card[ATTEMPTS_OFFSET]
    if retries not in ((0,) if blocked else (1,2,3)):
        raise ValueError('NSE-2 PIN fixture has not qualified this initial retry lifecycle')
    initial[ATTEMPTS_OFFSET]=retries
    if card[:len(initial)]!=initial:
        raise ValueError('NSE-2 PIN input differs from declared laboratory credentials/profile')
    return retries


def verify_pin_boot(text, card, prompt_pixels, result_pixels=None, final_pixels=None, *, action=None,initial_card=None,pin=b'1234',puk_attempts=10):
    from tools.make_sim_card_profile import make_profile
    from tools.sim_security_trace_check import statuses,ATTEMPTS_OFFSET,AOC_NVRAM_SIZE,CHV_OFFSET
    if action not in (None,'verify','wrong','blocked'):
        raise ValueError('unknown NSE-2 PIN action')
    if action in ('verify','wrong') and (result_pixels is None or final_pixels is None):
        raise ValueError('NSE-2 PIN result and continuation frames are required')
    initial_card=make_profile(True) if initial_card is None else initial_card
    before=verify_pin_input(initial_card,blocked=action=='blocked',pin=pin,puk_attempts=puk_attempts)
    remaining=3 if action=='verify' else before-1 if action=='wrong' else before
    expected=bytearray(initial_card)
    expected[ATTEMPTS_OFFSET]=remaining
    if len(card)!=AOC_NVRAM_SIZE or card[:len(expected)]!=expected:
        raise ValueError('NSE-2 PIN attempt changed unexpected persistent credentials/retries')
    expected_prompt=PUK_PROMPT_FRAME if action=='blocked' else PIN_PROMPT_FRAME
    if hashlib.sha256(prompt_pixels).hexdigest()!=expected_prompt:
        raise ValueError('NSE-2 reviewed SIM authentication prompt frame differs')
    verify_headers=[match.groups() for match in HEADER.finditer(text) if int(match[2],16)==0x20]
    if any(int(match[2],16)==0x2c for match in HEADER.finditer(text)) or statuses(text,0x2c):
        raise ValueError('NSE-2 PIN-only fixture unexpectedly attempted PUK unblocking')
    contacts=re.findall(r'nse2_pin_contact: event=(\d+) down=([01]) t=([0-9.]+) physical_only=1',text)
    if action in (None,'blocked'):
        if verify_headers or contacts or statuses(text,0x20):
            raise ValueError('NSE-2 PIN wait unexpectedly submitted a code')
    else:
        if [(int(index),int(down)) for index,down,_ in contacts]!=[(i,i%2) for i in range(1,11)]:
            raise ValueError('NSE-2 physical SIM PIN contact sequence differs')
        times=[float(time) for _,_,time in contacts]
        if times!=sorted(times) or not 5.9<=times[0]<times[-1]<8:
            raise ValueError('NSE-2 physical SIM PIN timing differs')
        if len(verify_headers)!=1 or verify_headers[0][:5]!=('a0','20','00','01','08'):
            raise ValueError('NSE-2 absolute CHV1 VERIFY header missing')
        expected_status=0x9000 if action=='verify' else 0x9840 if remaining==0 else 0x9804
        if statuses(text,0x20)!=[(expected_status,remaining,10)]:
            raise ValueError('NSE-2 SIM PIN status/retry contrast differs')
        expected_result=PIN_ACCEPTED_FRAME if action=='verify' else PIN_BLOCKED_FRAME if remaining==0 else CODE_ERROR_FRAME
        expected_final=SECURITY_FRAME if action=='verify' else PUK_PROMPT_FRAME if remaining==0 else PIN_PROMPT_FRAME
        if (hashlib.sha256(result_pixels).hexdigest()!=expected_result
                or hashlib.sha256(final_pixels).hexdigest()!=expected_final):
            raise ValueError('NSE-2 PIN result/continuation frame differs')
    return dict(sim_pin_verified=action=='verify',pin_wait=action is None,
                wrong_pin_rejected=action=='wrong',pin_attempts_before=before,pin_attempts_remaining=remaining,
                pin_blocked=remaining==0,puk_verified=False,
                initial_card_bytes=len(initial_card),
                phone_code_authenticated=False,network_registration=False)


def verify_puk_recovery(text, initial, final, prompt_pixels, final_pixels, *, puk_attempts=10):
    from tools.sim_security_trace_check import statuses,ATTEMPTS_OFFSET,AOC_NVRAM_SIZE,CHV_OFFSET
    verify_pin_input(initial,blocked=True,puk_attempts=puk_attempts)
    if len(initial)!=AOC_NVRAM_SIZE:
        raise ValueError('PUK recovery requires a complete saved exhausted card')
    expected=bytearray(initial)
    expected[CHV_OFFSET:CHV_OFFSET+8]=b'2468\xff\xff\xff\xff'
    expected[ATTEMPTS_OFFSET]=3
    expected[ATTEMPTS_OFFSET+2]=10
    if final!=expected:
        raise ValueError('PUK recovery changed unexpected card bytes')
    headers=[m.groups() for m in HEADER.finditer(text) if int(m[2],16) in (0x20,0x2c)]
    if len(headers)!=1 or headers[0][:5]!=('a0','2c','00','00','10'):
        raise ValueError('PUK recovery requires exactly one absolute UNBLOCK and no VERIFY')
    if statuses(text,0x2c)!=[(0x9000,3,10)]:
        raise ValueError('PUK recovery status/retries differ')
    contacts=re.findall(r'nse2_puk_contact: event=(\d+) down=([01]) t=([0-9.]+) physical_only=1',text)
    if [(int(i),int(d)) for i,d,_ in contacts]!=[(i,i%2) for i in range(1,39)]:
        raise ValueError('PUK physical contact sequence differs')
    times=[float(t) for _,_,t in contacts]
    if times!=sorted(times) or not 11<=times[0]<times[-1]<18.5:
        raise ValueError('PUK contact timing differs')
    if not times[-1]-0.2<float(headers[0][6])<19:
        raise ValueError('UNBLOCK did not follow physical confirmation')
    if hashlib.sha256(prompt_pixels).hexdigest()!=PUK_PROMPT_FRAME or hashlib.sha256(final_pixels).hexdigest()!=SECURITY_FRAME:
        raise ValueError('PUK recovery prompt/continuation differs')
    return dict(puk_verified=True,pin_attempts_remaining=3,phone_code_authenticated=False,network_registration=False)


def verify_pin2_input(card,*,restore=False,blocked=False,permanent=False):
    from tools.make_sim_card_profile import make_profile
    from tools.sim_security_trace_check import CHV_OFFSET,AOC_NVRAM_SIZE,ATTEMPTS_OFFSET
    expected=bytearray(make_profile(False))
    if restore:
        expected[CHV_OFFSET+8:CHV_OFFSET+16]=b'2468\xff\xff\xff\xff'
    if len(card)<=ATTEMPTS_OFFSET+1 or card[ATTEMPTS_OFFSET+1] not in ((0,) if blocked else (1,2,3)):
        raise ValueError('unqualified initial PIN2 retry lifecycle')
    expected[ATTEMPTS_OFFSET+1]=card[ATTEMPTS_OFFSET+1]
    if blocked:
        if card[ATTEMPTS_OFFSET+3] not in ((0,) if permanent else range(1,11)):
            raise ValueError('unqualified initial PUK2 retry lifecycle')
        expected[ATTEMPTS_OFFSET+3]=card[ATTEMPTS_OFFSET+3]
    if len(card) not in (len(expected),AOC_NVRAM_SIZE) or card[:len(expected)]!=expected:
        raise ValueError('PIN2 change requires the declared PIN-disabled laboratory card and qualified credential')
    if (restore or blocked) and len(card)!=AOC_NVRAM_SIZE:
        raise ValueError('cold PIN2 lifecycle requires the complete saved predecessor card')
    return card[ATTEMPTS_OFFSET+1]


def verify_pin2_change(text,initial,final,frames,*,restore=False,wrong=False):
    from tools.sim_security_trace_check import CHV_OFFSET,AOC_NVRAM_SIZE,statuses,ATTEMPTS_OFFSET
    before=verify_pin2_input(initial,restore=restore)
    if wrong and restore:
        raise ValueError('unqualified PIN2 wrong-code lifecycle')
    expected=bytearray(initial)
    if not wrong:
        expected[CHV_OFFSET+8:CHV_OFFSET+16]=(b'5678' if restore else b'2468').ljust(8,b'\xff')
    remaining=before-1 if wrong else 3
    expected[ATTEMPTS_OFFSET+1]=remaining
    if len(final)!=AOC_NVRAM_SIZE or final[:len(expected)]!=expected:
        raise ValueError('PIN2 change modified unexpected card bytes or did not persist')
    headers=[m.groups() for m in HEADER.finditer(text) if int(m[2],16) in (0x20,0x24,0x2c)]
    if len(headers)!=1 or headers[0][:5]!=('a0','24','00','02','10') or not 29<float(headers[0][-1])<29.5:
        raise ValueError('physical PIN2 CHANGE header/timing differs')
    if statuses(text,0x24,chv=2)!=[(0x9840 if wrong and remaining==0 else 0x9804 if wrong else 0x9000,remaining,10)]:
        raise ValueError('PIN2 CHANGE did not succeed with intact retries')
    contacts=re.findall(r'nse2_pin2_contact: event=(\d+) down=([01]) t=([0-9.]+) physical_only=1',text)
    if [(int(i),int(d)) for i,d,_ in contacts]!=[(i,i%2) for i in range(1,63)]:
        raise ValueError('PIN2 physical navigation/entry sequence differs')
    times=[float(t) for _,_,t in contacts]
    if times!=sorted(times) or not 18.4<times[0]<18.6 or not 29<times[-1]<29.4:
        raise ValueError('PIN2 physical navigation/entry timing differs')
    expected_frames=PIN2_CHANGE_FRAMES[:-1]+(PIN2_BLOCKED_FRAME if remaining==0 else CODE_ERROR_FRAME,) if wrong else PIN2_CHANGE_FRAMES
    if tuple(hashlib.sha256(frame).hexdigest() for frame in frames)!=expected_frames:
        raise ValueError('PIN2 change prompt/result frames differ')
    return dict(sim_pin2_changed=not wrong,pin2_wrong_rejected=wrong,sim_pin2_blocked=remaining==0,
                pin2_attempts_before=before,pin2_attempts_remaining=remaining,
                cold_replacement_pin2_accepted=restore,sim_pin1_enabled=False,
                legacy_profile_expanded=len(initial)!=len(final),whole_input_card_checked=len(initial)==len(final),
                network_registration=False,stable_long_boot=False)


def verify_pin2_blocked(text,initial,final,frames,*,permanent=False):
    from tools.sim_security_trace_check import AOC_NVRAM_SIZE,statuses
    verify_pin2_input(initial,blocked=True,permanent=permanent)
    if len(initial)!=AOC_NVRAM_SIZE or final!=initial:
        raise ValueError('cold blocked PIN2 card changed or was not a complete predecessor')
    if any(int(m[2],16) in (0x20,0x24,0x2c) for m in HEADER.finditer(text)) or any(statuses(text,i,chv=2) for i in (0x20,0x24,0x2c)):
        raise ValueError('cold PIN2 prompt control unexpectedly submitted credentials')
    contacts=re.findall(r'nse2_pin2_contact: event=(\d+) down=([01]) t=([0-9.]+) physical_only=1',text)
    if [(int(i),int(d)) for i,d,_ in contacts]!=[(i,i%2) for i in range(1,33)]:
        raise ValueError('cold blocked PIN2 navigation differs')
    times=[float(t) for _,_,t in contacts]
    if times!=sorted(times) or not 18.4<times[0]<18.6 or not 23.8<times[-1]<24:
        raise ValueError('cold blocked PIN2 navigation timing differs')
    expected=(AUTHENTICATED_FRAME,PIN2_REJECTED_FRAME,PIN2_REJECTED_FRAME,PIN2_ITEM_FRAME,PHONE_CODE_ITEM_FRAME) if permanent else (
        AUTHENTICATED_FRAME,PIN2_BLOCKED_FRAME,PIN2_BLOCKED_FRAME,PUK2_PROMPT_FRAME,PUK2_PROMPT_FRAME)
    if tuple(hashlib.sha256(frame).hexdigest() for frame in frames)!=expected:
        raise ValueError('cold PIN2 blocked-notice/PUK2 prompt frames differ')
    navigation=re.findall(r'nse2_permanent_contact: down=([01]) t=([0-9.]+) physical_only=1',text)
    if permanent:
        if len(navigation)!=2 or [v for v,_ in navigation]!=['1','0'] or not 28.49<=float(navigation[0][1])<28.54 or not 28.64<=float(navigation[1][1])<28.69:
            raise ValueError('permanent PIN2 refusal physical continuation differs')
        decodes=re.findall(r'nse2_permanent_decode: raw=15 key=18 t=([0-9.]+) observer_writes=0',text)
        if not any(float(navigation[0][1])<=float(t)<=float(navigation[1][1]) for t in decodes):
            raise ValueError('permanent PIN2 refusal Down contact did not reach decoder')
    elif navigation:
        raise ValueError('unexpected permanent-loss fixture in recoverable blocked control')
    return dict(sim_pin2_blocked=True,puk2_prompt=not permanent,permanent_pin2_loss=permanent,
                physical_menu_continuation=permanent,puk2_verified=False,card_unchanged=True,
                network_registration=False,stable_long_boot=False)


def verify_puk2_recovery(text,initial,final,frames,*,wrong=False):
    from tools.sim_security_trace_check import CHV_OFFSET,ATTEMPTS_OFFSET,AOC_NVRAM_SIZE,statuses
    verify_pin2_input(initial,blocked=True)
    before=initial[ATTEMPTS_OFFSET+3]
    remaining=before-1 if wrong else 10
    expected=bytearray(initial)
    if not wrong:
        expected[CHV_OFFSET+8:CHV_OFFSET+16]=b'2468\xff\xff\xff\xff'
        expected[ATTEMPTS_OFFSET+1]=3
    expected[ATTEMPTS_OFFSET+3]=remaining
    if len(initial)!=AOC_NVRAM_SIZE or final!=expected:
        raise ValueError('PUK2 recovery changed unexpected card bytes')
    headers=[m.groups() for m in HEADER.finditer(text) if int(m[2],16) in (0x20,0x24,0x2c)]
    expected_headers=[('a0','2c','00','02','10')]+([] if wrong else [('a0','20','00','02','08')])
    if [h[:5] for h in headers]!=expected_headers:
        raise ValueError('PUK2 UNBLOCK/automatic VERIFY sequence differs')
    times=[float(h[-1]) for h in headers]
    if times!=sorted(times) or not 29<times[0]<=times[-1]<29.5 or (not wrong and times[0]>=times[1]):
        raise ValueError('PUK2 APDU timing/order differs')
    if (statuses(text,0x2c,chv=2)!=[(0x9840 if wrong and remaining==0 else 0x9804 if wrong else 0x9000,0 if wrong else 3,remaining)]
            or statuses(text,0x20,chv=2)!=([] if wrong else [(0x9000,3,10)])):
        raise ValueError('PUK2 status/retry/order differs')
    contacts=re.findall(r'nse2_puk2_contact: event=(\d+) down=([01]) t=([0-9.]+) physical_only=1',text)
    if [(int(i),int(d)) for i,d,_ in contacts]!=[(i,i%2) for i in range(1,71)]:
        raise ValueError('PUK2 physical navigation/credential sequence differs')
    event_times=[float(t) for _,_,t in contacts]
    if event_times!=sorted(event_times) or not 15.4<event_times[0]<15.6 or not 24.4<event_times[32]<24.6 or not 29<event_times[-1]<29.4:
        raise ValueError('PUK2 physical input timing differs')
    expected_frames=(AUTHENTICATED_FRAME,PUK2_PROMPT_FRAME,*PIN2_CHANGE_FRAMES[2:4],
                     PIN2_REJECTED_FRAME if wrong and remaining==0 else CODE_ERROR_FRAME if wrong else PIN_ACCEPTED_FRAME)
    if tuple(hashlib.sha256(frame).hexdigest() for frame in frames)!=expected_frames:
        raise ValueError('PUK2 prompt/new-PIN/confirmation/result frames differ')
    return dict(puk2_verified=not wrong,wrong_puk2_rejected=wrong,sim_pin2_recovered=not wrong,
                puk2_attempts_before=before,puk2_attempts_remaining=remaining,pin2_attempts_remaining=0 if wrong else 3,
                automatic_pin2_verify=not wrong,network_registration=False,stable_long_boot=False)


def verify_security_attempt(text, pixels, *, retry_zero=False,contact_start=12):
    if contact_start not in (9,12):
        raise ValueError('unreviewed physical phone-code input schedule')
    submit_time=contact_start+3
    contacts = re.findall(r'nse2_security_contact: index=([1-6]) down=([01]) '
                          r't=([0-9.]+) physical_only=1', text)
    if [(int(i), int(down)) for i, down, _ in contacts] != [(i, down) for i in range(1, 7) for down in (1, 0)]:
        raise ValueError('NSE-2 one physical phone-code attempt missing')
    decoded = re.findall(r'nse2_key_decode: raw=(\d+) key=([0-9a-f]{2}) pc=00299044 '
                         r't=([0-9.]+) observer_writes=0', text)
    for i, (raw, key) in enumerate(((7, 1), (8, 2), (9, 3), (12, 4), (13, 5), (11, 0x19))):
        start, end = float(contacts[2 * i][2]), float(contacts[2 * i + 1][2])
        expected = contact_start + i * 0.5 if i < 5 else submit_time
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
    if first != second or times != sorted(times) or not submit_time <= times[0] <= times[-1] < submit_time+.3:
        raise ValueError('NSE-2 matching phone-code/retry rejection ordering differs')
    if retry_zero:
        returns = re.findall(r'nse2_security_return: accepted=1 attempts=00 t=([0-9.]+) observer_writes=0', text)
        if len(returns) != 1 or not times[-1] <= float(returns[0]) < submit_time+.3:
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

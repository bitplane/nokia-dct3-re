import hashlib
import unittest
from unittest.mock import patch

from tools.nse2_sim_check import (BINARY_FILES, verify_sim_reads, verify_security_attempt,
                                  verify_post_auth_menu, verify_phonebook_save, verify_phonebook_reload,
                                  verify_sms_save, verify_sms_reload, verify_sms_action, verify_pin_boot)


def reads():
    rows = []
    for fid, size in BINARY_FILES.items():
        rows.extend((f'sim_device: header cla=a0 ins=b0 p1=00 p2=00 p3={size:02x} selected={fid:04x} t=4.2',
                     'SIM completion ins=b0 sw=9000'))
    for fid, count, size in ((0x6f3a, 50, 32), (0x6f3c, 10, 176)):
        for record in range(1, count + 1):
            rows.extend((f'sim_device: header cla=a0 ins=b2 p1={record:02x} p2=04 p3={size:02x} selected={fid:04x} t=8.8',
                         'SIM completion ins=b2 sw=9000'))
    return '\n'.join(rows)


def contacts():
    rows = []
    for i, (raw, key) in enumerate(((7, 1), (8, 2), (9, 3), (12, 4), (13, 5), (11, 0x19)), 1):
        start = 12 + (i - 1) * 0.5 if i < 6 else 15
        rows.extend((f'nse2_security_contact: index={i} down=1 t={start} physical_only=1',
                     f'nse2_key_decode: raw={raw} key={key:02x} pc=00299044 t={start + 0.1} observer_writes=0',
                     f'nse2_security_contact: index={i} down=0 t={start + 0.2} physical_only=1'))
    rows.extend(('nse2_security_encode: input=313233343500 destination=0010d6d4 lr=0029469b t=15.066 observer_writes=0',
                 'nse2_security_compare: first=0010c6f8 second=0010d6d4 length=4 a=d83b30d8 b=d83b30d8 lr=002946a5 t=15.068 observer_writes=0',
                 'nse2_security_result: comparison=00000000 attempts=ff t=15.069 observer_writes=0',
                 'nse2_security_retry: matched=1 attempts=ff lr=002946b3 t=15.070 observer_writes=0'))
    return '\n'.join(rows)


class SimCheckTest(unittest.TestCase):
    def test_puk2_recovery_requires_unblock_then_automatic_verify(self):
        from tools.nse2_sim_check import verify_puk2_recovery
        from tools.make_sim_card_profile import make_profile
        from tools.sim_security_trace_check import AOC_NVRAM_SIZE,ATTEMPTS_OFFSET,CHV_OFFSET
        initial=bytearray(make_profile(False).ljust(AOC_NVRAM_SIZE,b'\xff'))
        initial[ATTEMPTS_OFFSET+1]=0
        final=bytearray(initial)
        final[ATTEMPTS_OFFSET+1]=3
        final[CHV_OFFSET+8:CHV_OFFSET+16]=b'2468\xff\xff\xff\xff'
        frames=[b'idle',b'puk',b'new',b'confirm',b'accepted']
        text='\n'.join(f'nse2_puk2_contact: event={i} down={i%2} t={(15.5+(i-1)*.175) if i<=32 else (24.5+(i-33)*.125):.3f} physical_only=1' for i in range(1,71))
        text+='\nsim_device: header cla=a0 ins=2c p1=00 p2=02 p3=10 selected=6f3a t=29.072\nSIM status ins=2c sw=9000 chv=3/3 puk=10/10 enabled=0'
        text+='\nsim_device: header cla=a0 ins=20 p1=00 p2=02 p3=08 selected=6f3a t=29.08\nSIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=0'
        h=lambda b:hashlib.sha256(b).hexdigest()
        with patch.multiple('tools.nse2_sim_check',AUTHENTICATED_FRAME=h(b'idle'),PUK2_PROMPT_FRAME=h(b'puk'),
                            PIN2_CHANGE_FRAMES=('','',h(b'new'),h(b'confirm'),''),PIN_ACCEPTED_FRAME=h(b'accepted'),CODE_ERROR_FRAME=h(b'error')):
            self.assertTrue(verify_puk2_recovery(text,initial,final,frames)['automatic_pin2_verify'])
            wrong=bytearray(initial)
            wrong[ATTEMPTS_OFFSET+3]=9
            rejected=text.split('\nsim_device: header cla=a0 ins=20')[0].replace('sw=9000 chv=3/3 puk=10/10','sw=9804 chv=3/0 puk=10/9')
            result=verify_puk2_recovery(rejected,initial,wrong,frames[:-1]+[b'error'],wrong=True)
            self.assertEqual(result['puk2_attempts_remaining'],9)
            self.assertFalse(result['automatic_pin2_verify'])
            self.assertEqual(verify_puk2_recovery(text,wrong,final,frames)['puk2_attempts_before'],9)
            last=bytearray(initial)
            last[ATTEMPTS_OFFSET+3]=1
            permanent=bytearray(last)
            permanent[ATTEMPTS_OFFSET+3]=0
            last_text=rejected.replace('sw=9804','sw=9840').replace('puk=10/9','puk=10/0')
            with patch('tools.nse2_sim_check.PIN2_REJECTED_FRAME',h(b'rejected')):
                self.assertEqual(verify_puk2_recovery(last_text,last,permanent,frames[:-1]+[b'rejected'],wrong=True)['puk2_attempts_remaining'],0)
                with self.assertRaises(ValueError):
                    verify_puk2_recovery(last_text.replace('sw=9840','sw=9804'),last,permanent,frames[:-1]+[b'rejected'],wrong=True)
            for bad in (rejected.replace('puk=10/9','puk=9/10'),rejected+'\nsim_device: header cla=a0 ins=20 p1=00 p2=02 p3=08 selected=6f3a t=29.08'):
                with self.assertRaises(ValueError):
                    verify_puk2_recovery(bad,initial,wrong,frames[:-1]+[b'error'],wrong=True)
            for bad in (text.replace('p2=02','p2=00',1),text.replace('ins=20 p1=00','ins=24 p1=00'),text.replace('t=29.08','t=28.9'),text.replace('chv=3/3','chv=3/2')):
                with self.assertRaises(ValueError):
                    verify_puk2_recovery(bad,initial,final,frames)
            final[-1]^=1
            with self.assertRaises(ValueError):
                verify_puk2_recovery(text,initial,final,frames)

    def test_later_chv2_does_not_weaken_pin1_bootstrap(self):
        header='\nsim_device: header cla=a0 ins=20 p1=00 p2=02 p3=08 selected=6f3a t=29.08'
        with patch('tools.nse2_sim_check.SECURITY_FRAME',hashlib.sha256(b'frame').hexdigest()):
            self.assertFalse(verify_sim_reads(reads()+header,b'frame',later_chv2=True)['sim_pin_verified'])
            for wrong in (header.replace('p2=02','p2=01'),header.replace('t=29.08','t=9.0')):
                with self.assertRaises(ValueError):
                    verify_sim_reads(reads()+wrong,b'frame',later_chv2=True)
            with self.assertRaises(ValueError):
                verify_sim_reads(reads()+header,b'frame')

    def test_early_phone_input_schedule_is_explicit_not_a_relaxed_default(self):
        import re
        early=re.sub(r'(?<= )t=([0-9.]+)',lambda m:f't={float(m[1])-3:.3f}',contacts())
        with patch('tools.nse2_sim_check.CODE_ERROR_FRAME',hashlib.sha256(b'frame').hexdigest()):
            self.assertEqual(verify_security_attempt(early,b'frame',contact_start=9)['retry_counter'],255)
            with self.assertRaises(ValueError):
                verify_security_attempt(early,b'frame')
            with self.assertRaises(ValueError):
                verify_security_attempt(early,b'frame',contact_start=8)

    def test_pin2_cold_blocked_control_requires_puk2_prompt_and_unchanged_card(self):
        from tools.nse2_sim_check import verify_pin2_blocked
        from tools.make_sim_card_profile import make_profile
        from tools.sim_security_trace_check import AOC_NVRAM_SIZE,ATTEMPTS_OFFSET
        card=bytearray(make_profile(False).ljust(AOC_NVRAM_SIZE,b'\xff'))
        card[ATTEMPTS_OFFSET+1]=0
        frames=[b'idle',b'blocked',b'blocked',b'puk2',b'puk2']
        text='\n'.join(f'nse2_pin2_contact: event={i} down={i%2} t={18.5+(i-1)*.175:.3f} physical_only=1' for i in range(1,33))
        hashes={name:hashlib.sha256(value).hexdigest() for name,value in (
            ('AUTHENTICATED_FRAME',b'idle'),('PIN2_BLOCKED_FRAME',b'blocked'),('PUK2_PROMPT_FRAME',b'puk2'))}
        with patch.multiple('tools.nse2_sim_check',**hashes):
            self.assertTrue(verify_pin2_blocked(text,card,card,frames)['puk2_prompt'])
            changed=bytearray(card)
            changed[-1]^=1
            with self.assertRaises(ValueError):
                verify_pin2_blocked(text,card,changed,frames)
            with self.assertRaises(ValueError):
                verify_pin2_blocked(text+'\nSIM status ins=2c sw=9000 chv=3/3 puk=10/10 enabled=0',card,card,frames)
            permanent=bytearray(card)
            permanent[ATTEMPTS_OFFSET+3]=0
            continued=text+'\nnse2_permanent_contact: down=1 t=28.5 physical_only=1\nnse2_permanent_contact: down=0 t=28.65 physical_only=1\nnse2_permanent_decode: raw=15 key=18 t=28.55 observer_writes=0'
            permanent_frames=[b'idle',b'rejected',b'rejected',b'pin2item',b'phoneitem']
            with patch.multiple('tools.nse2_sim_check',PIN2_REJECTED_FRAME=hashlib.sha256(b'rejected').hexdigest(),
                    PIN2_ITEM_FRAME=hashlib.sha256(b'pin2item').hexdigest(),PHONE_CODE_ITEM_FRAME=hashlib.sha256(b'phoneitem').hexdigest()):
                self.assertTrue(verify_pin2_blocked(continued,permanent,permanent,permanent_frames,permanent=True)['physical_menu_continuation'])
                with self.assertRaises(ValueError):
                    verify_pin2_blocked(continued.replace('raw=15 key=18','raw=14 key=18'),permanent,permanent,permanent_frames,permanent=True)
                with self.assertRaises(ValueError):
                    verify_pin2_blocked(continued,card,card,permanent_frames,permanent=True)

    def test_pin2_change_and_cold_reverse_require_exact_card_transition(self):
        from tools.nse2_sim_check import verify_pin2_change
        from tools.make_sim_card_profile import make_profile
        from tools.sim_security_trace_check import AOC_NVRAM_SIZE,CHV_OFFSET,ATTEMPTS_OFFSET
        initial=make_profile(False)
        changed=bytearray(initial.ljust(AOC_NVRAM_SIZE,b'\xff'))
        changed[CHV_OFFSET+8:CHV_OFFSET+16]=b'2468\xff\xff\xff\xff'
        frames=[bytes([i]) for i in range(5)]
        text='\n'.join(f'nse2_pin2_contact: event={i} down={i%2} t={18.5+(i-1)*.175:.3f} physical_only=1' for i in range(1,63))
        text+='\nsim_device: header cla=a0 ins=24 p1=00 p2=02 p3=10 selected=6f3a t=29.072\nSIM status ins=24 sw=9000 chv=3/3 puk=10/10 enabled=0'
        with patch('tools.nse2_sim_check.PIN2_CHANGE_FRAMES',tuple(hashlib.sha256(f).hexdigest() for f in frames)):
            self.assertTrue(verify_pin2_change(text,initial,changed,frames)['sim_pin2_changed'])
            restored=bytearray(changed)
            restored[CHV_OFFSET+8:CHV_OFFSET+16]=b'5678\xff\xff\xff\xff'
            self.assertTrue(verify_pin2_change(text,changed,restored,frames,restore=True)['cold_replacement_pin2_accepted'])
            rejected=bytearray(initial.ljust(AOC_NVRAM_SIZE,b'\xff'))
            rejected[ATTEMPTS_OFFSET+1]=2
            error_text=text.replace('sw=9000 chv=3/3','sw=9804 chv=3/2')
            with patch('tools.nse2_sim_check.CODE_ERROR_FRAME',hashlib.sha256(b'error').hexdigest()):
                result=verify_pin2_change(error_text,initial,rejected,frames[:-1]+[b'error'],wrong=True)
                self.assertEqual(result['pin2_attempts_remaining'],2)
                self.assertFalse(result['sim_pin2_changed'])
                for bad in (error_text.replace('chv=3/2','chv=2/3'),error_text.replace('sw=9804','sw=9000')):
                    with self.assertRaises(ValueError):
                        verify_pin2_change(bad,initial,rejected,frames[:-1]+[b'error'],wrong=True)
            last=bytearray(rejected)
            last[ATTEMPTS_OFFSET+1]=1
            blocked=bytearray(last)
            blocked[ATTEMPTS_OFFSET+1]=0
            blocked_text=error_text.replace('sw=9804 chv=3/2','sw=9840 chv=3/0')
            with patch('tools.nse2_sim_check.PIN2_BLOCKED_FRAME',hashlib.sha256(b'blocked').hexdigest()):
                self.assertEqual(verify_pin2_change(blocked_text,last,blocked,frames[:-1]+[b'blocked'],wrong=True)['pin2_attempts_remaining'],0)
                with self.assertRaises(ValueError):
                    verify_pin2_change(blocked_text.replace('sw=9840','sw=9804'),last,blocked,frames[:-1]+[b'blocked'],wrong=True)
            result=verify_pin2_change(text,rejected,changed,frames)
            self.assertEqual((result['pin2_attempts_before'],result['pin2_attempts_remaining']),(2,3))
            rejected[-1]^=1
            with self.assertRaises(ValueError):
                verify_pin2_change(text,rejected,changed,frames)
            for bad in (text.replace('sw=9000','sw=9804'),text.replace('p2=02','p2=01'),text.replace('event=62','event=63')):
                with self.assertRaises(ValueError):
                    verify_pin2_change(bad,initial,changed,frames)
            restored[-1]^=1
            with self.assertRaises(ValueError):
                verify_pin2_change(text,changed,restored,frames,restore=True)
            with self.assertRaises(ValueError):
                verify_pin2_change(text,initial,changed,frames[::-1])

    def test_puk_recovery_requires_exact_storage_and_physical_unblock(self):
        from tools.nse2_sim_check import verify_puk_recovery,verify_pin_input
        from tools.make_sim_card_profile import make_profile
        from tools.sim_security_trace_check import AOC_NVRAM_SIZE,ATTEMPTS_OFFSET
        initial=bytearray(make_profile(True).ljust(AOC_NVRAM_SIZE,b'\xff'))
        initial[ATTEMPTS_OFFSET]=0
        final=bytearray(initial)
        final[3484:3492]=b'2468\xff\xff\xff\xff'
        final[ATTEMPTS_OFFSET]=3
        text='\n'.join(f'nse2_puk_contact: event={i} down={i%2} t={11.2+i*.18:.3f} physical_only=1' for i in range(1,39))
        text+='\nsim_device: header cla=a0 ins=2c p1=00 p2=00 p3=10 selected=6fad t=18.266\nSIM status ins=2c sw=9000 chv=3/3 puk=10/10 enabled=1'
        with patch.multiple('tools.nse2_sim_check',PUK_PROMPT_FRAME=hashlib.sha256(b'puk').hexdigest(),SECURITY_FRAME=hashlib.sha256(b'phone').hexdigest()):
            self.assertTrue(verify_puk_recovery(text,initial,final,b'puk',b'phone')['puk_verified'])
            self.assertEqual(verify_pin_input(final,pin=b'2468'),3)
            for broken in (text.replace('sw=9000','sw=9804'),text.replace('event=38','event=39'),text.replace('ins=2c p1=00','ins=20 p1=00')):
                with self.assertRaises(ValueError):
                    verify_puk_recovery(broken,initial,final,b'puk',b'phone')
            final[-1]^=1
            with self.assertRaises(ValueError):
                verify_puk_recovery(text,initial,final,b'puk',b'phone')

    def test_final_wrong_pin_and_cold_blocked_state_require_puk_frames(self):
        from tools.make_sim_card_profile import make_profile
        from tools.sim_security_trace_check import AOC_NVRAM_SIZE,ATTEMPTS_OFFSET
        initial=bytearray(make_profile(True).ljust(AOC_NVRAM_SIZE,b'\xff'))
        initial[ATTEMPTS_OFFSET]=1
        blocked=bytearray(initial)
        blocked[ATTEMPTS_OFFSET]=0
        text='\n'.join(f'nse2_pin_contact: event={i} down={i%2} t={6+i*.15:.2f} physical_only=1' for i in range(1,11))
        text+=('\nsim_device: header cla=a0 ins=20 p1=00 p2=01 p3=08 selected=6fad t=7.7\n'
               'SIM status ins=20 sw=9840 chv=0/3 puk=10/10 enabled=1')
        hashes={name:hashlib.sha256(value).hexdigest() for name,value in (
            ('PIN_PROMPT_FRAME',b'pin'),('PIN_BLOCKED_FRAME',b'blocked'),('PUK_PROMPT_FRAME',b'puk'))}
        with patch.multiple('tools.nse2_sim_check',**hashes):
            result=verify_pin_boot(text,blocked,b'pin',b'blocked',b'puk',action='wrong',initial_card=initial)
            self.assertTrue(result['pin_blocked'])
            self.assertFalse(result['puk_verified'])
            cold=verify_pin_boot('',blocked,b'puk',action='blocked',initial_card=blocked)
            self.assertEqual(cold['pin_attempts_remaining'],0)
            with self.assertRaises(ValueError):
                verify_pin_boot('',blocked,b'pin',action='blocked',initial_card=blocked)
            with self.assertRaises(ValueError):
                verify_pin_boot('',initial,b'puk',action='blocked',initial_card=initial)
            with self.assertRaises(ValueError):
                verify_pin_boot(text.replace('sw=9840','sw=9804'),blocked,b'pin',b'blocked',b'puk',action='wrong',initial_card=initial)
            with self.assertRaises(ValueError):
                verify_pin_boot('sim_device: header cla=a0 ins=2c p1=00 p2=00 p3=10 selected=6fad t=7.7',
                                blocked,b'puk',action='blocked',initial_card=blocked)

    def test_cold_pin_retry_state_and_restore_preserve_the_full_card(self):
        from tools.make_sim_card_profile import make_profile
        from tools.sim_security_trace_check import AOC_NVRAM_SIZE,ATTEMPTS_OFFSET
        initial=bytearray(make_profile(True).ljust(AOC_NVRAM_SIZE,b'\xff'))
        initial[ATTEMPTS_OFFSET]=2
        final=bytearray(initial)
        final[ATTEMPTS_OFFSET]=3
        text='\n'.join(f'nse2_pin_contact: event={i} down={i%2} t={6+i*.15:.2f} physical_only=1' for i in range(1,11))
        text+=('\nsim_device: header cla=a0 ins=20 p1=00 p2=01 p3=08 selected=6fad t=7.7\n'
               'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1')
        hashes={name:hashlib.sha256(value).hexdigest() for name,value in (
            ('PIN_PROMPT_FRAME',b'pin'),('PIN_ACCEPTED_FRAME',b'accepted'),('SECURITY_FRAME',b'phone'))}
        with patch.multiple('tools.nse2_sim_check',**hashes):
            waiting=verify_pin_boot('',initial,b'pin',initial_card=initial)
            self.assertEqual(waiting['pin_attempts_remaining'],2)
            result=verify_pin_boot(text,final,b'pin',b'accepted',b'phone',action='verify',initial_card=initial)
            self.assertEqual((result['pin_attempts_before'],result['pin_attempts_remaining']),(2,3))
            final[-1]^=1
            with self.assertRaises(ValueError):
                verify_pin_boot(text,final,b'pin',b'accepted',b'phone',action='verify',initial_card=initial)
            initial[ATTEMPTS_OFFSET]=0
            with self.assertRaises(ValueError):
                verify_pin_boot('',initial,b'pin',initial_card=initial)

    def test_pin_wait_verify_and_wrong_pin_are_separate_from_phone_lock(self):
        from tools.make_sim_card_profile import make_profile
        from tools.sim_security_trace_check import AOC_NVRAM_SIZE,ATTEMPTS_OFFSET
        hashes={name:hashlib.sha256(value).hexdigest() for name,value in (
            ('PIN_PROMPT_FRAME',b'pin'),('PIN_ACCEPTED_FRAME',b'accepted'),
            ('SECURITY_FRAME',b'phone'),('CODE_ERROR_FRAME',b'wrong'))}
        with patch.multiple('tools.nse2_sim_check',**hashes):
            for action in (None,'verify','wrong'):
                card=bytearray(make_profile(True).ljust(AOC_NVRAM_SIZE,b'\xff'))
                text=''
                if action:
                    text='\n'.join(f'nse2_pin_contact: event={i} down={i%2} t={6+i*.15:.2f} physical_only=1' for i in range(1,11))
                    status='9000' if action=='verify' else '9804'
                    retries=3 if action=='verify' else 2
                    card[ATTEMPTS_OFFSET]=retries
                    text+=('\nsim_device: header cla=a0 ins=20 p1=00 p2=01 p3=08 selected=6fad t=7.7\n'
                           f'SIM status ins=20 sw={status} chv={retries}/3 puk=10/10 enabled=1')
                result=verify_pin_boot(text,card,b'pin',b'accepted' if action=='verify' else b'wrong',
                                       b'phone' if action=='verify' else b'pin',action=action)
                self.assertEqual(result['sim_pin_verified'],action=='verify')
                self.assertFalse(result['phone_code_authenticated'])
                card[ATTEMPTS_OFFSET]=0
                with self.assertRaises(ValueError):
                    verify_pin_boot(text,card,b'pin',action=action)

    def test_pin_authenticated_reads_require_verify_before_protected_files(self):
        status=('sim_device: header cla=a0 ins=20 p1=00 p2=01 p3=08 selected=6fad t=7.7\n'
                'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1\n')
        text=status+reads().replace('t=4.2','t=8.2')
        with patch('tools.nse2_sim_check.SECURITY_FRAME',hashlib.sha256(b'phone').hexdigest()):
            self.assertTrue(verify_sim_reads(text,b'phone',pin_authenticated=True,bootstrap_horizon=14)['sim_pin_verified'])
            for bad in (text.replace('sw=9000 chv','sw=9804 chv'),
                        text.replace('t=8.2','t=7.0'),reads()+status):
                with self.assertRaises(ValueError):
                    verify_sim_reads(bad,b'phone',pin_authenticated=True,bootstrap_horizon=14)

    def test_sms_erase_cancel_and_cold_empty_preserve_exact_mutation_boundary(self):
        from tools.sim_sms_draft import SMS_OFFSET,DRAFT,deleted_card
        from tools.make_sim_card_profile import SMS_RECORD_SIZE,SMS_RECORDS
        from tools.sim_phonebook_check import CURRENT_NVRAM_LENGTH
        saved=bytearray([255]*CURRENT_NVRAM_LENGTH)
        for index in range(SMS_RECORDS):
            saved[SMS_OFFSET+index*SMS_RECORD_SIZE]=0
        saved[SMS_OFFSET:SMS_OFFSET+SMS_RECORD_SIZE]=DRAFT
        erased=deleted_card(saved)
        hashes={name:hashlib.sha256(value).hexdigest() for name,value in (
            ('SMS_ERASE_PROMPT_FRAME',b'prompt'),('SMS_ERASED_FRAME',b'erased'),
            ('SMS_READ_FRAME',b'read'),('SMS_EMPTY_FRAME',b'empty'))}
        with patch.multiple('tools.nse2_sim_check',**hashes):
            for action,initial,final,frame in (('erase',saved,erased,b'erased'),
                                               ('cancel',saved,saved,b'read'),('empty',erased,erased,b'empty')):
                count=10 if action=='empty' else 18
                text='\n'.join(f'nse2_sms_reload_contact: event={i} down={i%2} t={20+i/4:.2f} physical_only=1'
                               for i in range(1,count+1))
                if action=='erase':
                    text+=('\nsim_device: header cla=a0 ins=dc p1=01 p2=04 p3=b0 selected=6f3c t=25.9\n'
                           'sim_device: update fid=6f3c record=1 length=176\n'
                           'SIM status ins=dc sw=9000 chv=3/3 puk=10/10 enabled=0')
                result=verify_sms_action(text,initial,final,b'prompt',frame,action)
                self.assertEqual(result['card_modified'],action=='erase')
                self.assertEqual(result['cold_empty_verified'],action=='empty')
                with self.assertRaises(ValueError):
                    verify_sms_action(text,initial,bytes(len(final)),b'prompt',frame,action)
                with self.assertRaises(ValueError):
                    verify_sms_action(text,initial,final,b'prompt',b'wrong',action)
                with self.assertRaises(ValueError):
                    verify_sms_action(text.replace('event=1 ','event=99 '),initial,final,b'prompt',frame,action)
                if action!='empty':
                    with self.assertRaises(ValueError):
                        verify_sms_action(text,initial,final,b'wrong',frame,action)
                bad=text.replace('sw=9000','sw=9404') if action=='erase' else text+'\nSIM status ins=dc sw=9000 chv=3/3 puk=10/10 enabled=0'
                with self.assertRaises(ValueError):
                    verify_sms_action(bad,initial,final,b'prompt',frame,action)

    def test_cold_outbox_requires_saved_bytes_and_no_update(self):
        from tools.sim_sms_draft import SMS_OFFSET,DRAFT
        from tools.make_sim_card_profile import SMS_RECORD_SIZE,SMS_RECORDS
        from tools.sim_phonebook_check import CURRENT_NVRAM_LENGTH
        card=bytearray([255]*CURRENT_NVRAM_LENGTH)
        for index in range(SMS_RECORDS):
            card[SMS_OFFSET+index*SMS_RECORD_SIZE]=0
        card[SMS_OFFSET:SMS_OFFSET+SMS_RECORD_SIZE]=DRAFT
        text='\n'.join(f'nse2_sms_reload_contact: event={i} down={i%2} t={20+i/4:.2f} physical_only=1'
                       for i in range(1,13))
        with patch('tools.nse2_sim_check.SMS_READ_FRAME',hashlib.sha256(b'read').hexdigest()):
            result=verify_sms_reload(text,card,card,b'read')
            self.assertTrue(result['cold_readback_verified'])
            self.assertFalse(result['network_sms_proved'])
            for bad in (text.replace('event=12','event=13'),
                        text+'\nsim_device: header cla=a0 ins=dc p1=01 p2=04 p3=b0 selected=6f3c t=24.0',
                        text+'\nSIM status ins=dc sw=9000 chv=3/3 puk=10/10 enabled=0'):
                with self.subTest(bad=bad[:40]), self.assertRaises(ValueError):
                    verify_sms_reload(bad,card,card,b'read')
            with self.assertRaises(ValueError):
                verify_sms_reload(text,card,bytes(len(card)),b'read')
            with self.assertRaises(ValueError):
                verify_sms_reload(text,card,card,b'wrong')

    def test_offline_sms_save_requires_physical_commit_and_exact_card(self):
        from tools.sim_sms_draft import SMS_OFFSET,DRAFT
        from tools.make_sim_card_profile import SMS_RECORD_SIZE,SMS_RECORDS
        from tools.sim_phonebook_check import CURRENT_NVRAM_LENGTH
        card=bytearray([255]*CURRENT_NVRAM_LENGTH)
        for index in range(SMS_RECORDS):
            card[SMS_OFFSET+index*SMS_RECORD_SIZE]=0
        card[SMS_OFFSET:SMS_OFFSET+SMS_RECORD_SIZE]=DRAFT
        text='\n'.join(f'nse2_sms_contact: event={i} down={i%2} t={20+i/4:.2f} physical_only=1'
                       for i in range(1,23))
        text+=('\nsim_device: header cla=a0 ins=dc p1=01 p2=04 p3=b0 selected=6f3c t=25.9\n'
               'sim_device: update fid=6f3c record=1 length=176\n'
               'SIM status ins=dc sw=9000 chv=3/3 puk=10/10 enabled=0')
        with patch('tools.nse2_sim_check.SMS_SAVED_FRAME',hashlib.sha256(b'saved').hexdigest()):
            result=verify_sms_save(text,card,b'saved')
            self.assertFalse(result['network_sms_proved'])
            self.assertFalse(result['cold_readback_verified'])
            for bad in (text.replace('event=22','event=23'),text.replace('p3=b0','p3=20'),
                        text.replace('selected=6f3c','selected=6f3a'),text.replace('sw=9000','sw=9404')):
                with self.subTest(bad=bad[:40]), self.assertRaises(ValueError):
                    verify_sms_save(bad,card,b'saved')
            card[SMS_OFFSET+11]^=1
            with self.assertRaises(ValueError):
                verify_sms_save(text,card,b'saved')

    def test_cold_contact_readback_differs_from_empty_card_without_writes(self):
        from tools.sim_phonebook_check import CURRENT_NVRAM_LENGTH
        empty = bytes([255])*CURRENT_NVRAM_LENGTH
        card = bytearray(empty)
        card[0] = ord('A')
        card[18:22] = bytes((3,0x81,0x21,0xf3))
        text = '\n'.join(f'nse2_phonebook_reload_contact: event={i} down={i%2} '
                         f't={23.5+i/4:.2f} physical_only=1' for i in range(1,7))
        with patch('tools.nse2_sim_check.PHONEBOOK_RELOAD_FRAME',hashlib.sha256(b'present').hexdigest()), \
             patch('tools.nse2_sim_check.PHONEBOOK_EMPTY_SEARCH_FRAME',hashlib.sha256(b'empty').hexdigest()):
            self.assertTrue(verify_phonebook_reload(text,card,card,b'present')['cold_reload_verified'])
            self.assertTrue(verify_phonebook_reload(text,None,empty,b'empty')['empty_card_control'])
            cases = [(text,None,empty,b'present'),(text,card,empty,b'present'),
                     (text,None,card,b'empty'),(text.replace('event=6','event=7'),card,card,b'present'),
                     (text+'\nsim_device: header cla=a0 ins=dc p1=01 p2=04 p3=20 selected=6f3a t=27.0',card,card,b'present')]
            for args in cases:
                with self.subTest(args=args[:1]), self.assertRaises(ValueError):
                    verify_phonebook_reload(*args)

    def test_physical_save_needs_contacts_apdu_storage_status_and_frame(self):
        from tools.sim_phonebook_check import CURRENT_NVRAM_LENGTH
        data = bytearray([255]*CURRENT_NVRAM_LENGTH)
        data[0] = ord('A')
        data[18:22] = bytes((3,0x81,0x21,0xf3))
        text = '\n'.join(f'nse2_phonebook_contact: event={index} down={index%2} '
                         f't={23.1+index/4:.2f} physical_only=1' for index in range(1,17))
        text += ('\nsim_device: header cla=a0 ins=dc p1=01 p2=04 p3=20 selected=6f3a\n'
                 'sim_device: update fid=6f3a record=1 length=32\nSIM status ins=dc sw=9000')
        with patch('tools.nse2_sim_check.PHONEBOOK_SAVED_FRAME', hashlib.sha256(b'saved').hexdigest()):
            result = verify_phonebook_save(text,data,b'saved')
            self.assertFalse(result['cold_reload_verified'])
            self.assertFalse(result['stable_long_boot'])
            for bad in (text.replace('event=16','event=17'),text.replace('sw=9000','sw=9404'),
                        text.replace('update fid=6f3a','update fid=6f3c')):
                with self.assertRaises(ValueError):
                    verify_phonebook_save(bad,data,b'saved')
            with self.assertRaises(ValueError):
                verify_phonebook_save(text,data,b'wrong')

    def test_post_auth_menu_checks_physical_decode_and_both_frames(self):
        text = '\n'.join(f'nse2_post_auth_contact: index={i} down={down} t={time} physical_only=1'
                         for i, down, time in ((1, 1, 20), (1, 0, 20.2), (2, 1, 22), (2, 0, 22.2)))
        for time in (20.1, 22.1):
            text += f'\nnse2_key_decode: raw=11 key=19 pc=00299044 t={time} observer_writes=0'
        pixels = bytes([255]) * (84 * 48)
        digest = hashlib.sha256(pixels).hexdigest()
        with patch('tools.nse2_sim_check.PHONEBOOK_MENU_FRAME', digest), \
                patch('tools.nse2_sim_check.PHONEBOOK_SUBMENU_FRAME', digest):
            result = verify_post_auth_menu(text, pixels, pixels)
            self.assertTrue(result['application_menu'])
            self.assertFalse(result['contact_persistence'])
            self.assertFalse(result['network_registration'])
            for bad in (text.replace('key=19', 'key=18'), text.replace('t=22.2', 't=23.2')):
                with self.assertRaises(ValueError):
                    verify_post_auth_menu(bad, pixels, pixels)
            with self.assertRaisesRegex(ValueError, 'submenu differs'):
                verify_post_auth_menu(text, pixels, bytes(84 * 48))

    def check_reads(self, text):
        with patch('tools.nse2_sim_check.SECURITY_FRAME', hashlib.sha256(b'frame').hexdigest()):
            return verify_sim_reads(text, b'frame')

    def test_complete_reads_do_not_claim_authentication(self):
        result = self.check_reads(reads())
        self.assertEqual(result['adn_records_read'], 50)
        self.assertEqual(result['sms_records_read'], 10)
        self.assertFalse(result['phone_code_authenticated'])
        self.assertFalse(result['sim_pin_verified'])

    def test_later_application_reads_do_not_duplicate_bootstrap_records(self):
        extra = ('\nsim_device: header cla=a0 ins=b2 p1=01 p2=04 p3=20 selected=6f3a t=27.0\n'
                 'SIM completion ins=b2 sw=9000')
        result = self.check_reads(reads() + extra)
        self.assertEqual(result['later_application_reads'],1)
        with self.assertRaises(ValueError):
            self.check_reads(reads().replace('t=8.8','t=11.0') + extra)

    def test_bad_status_header_record_length_and_horizon_fail(self):
        for old, new in (('sw=9000', 'sw=9404'), ('cla=a0', 'cla=00'),
                         ('p1=32', 'p1=31'), ('p3=b0', 'p3=af'), ('t=8.8', 't=11.0')):
            with self.subTest(old=old), self.assertRaises(ValueError):
                self.check_reads(reads().replace(old, new))

    def test_pin_command_and_wrong_frame_fail(self):
        with self.assertRaisesRegex(ValueError, 'VERIFY CHV'):
            self.check_reads(reads() + '\nsim_device: header cla=a0 ins=20 p1=00 p2=01 p3=08 selected=6f3a t=9.0')
        with self.assertRaisesRegex(ValueError, 'frame differs'):
            verify_sim_reads(reads(), b'wrong')

    def test_one_rejected_physical_attempt_is_narrowly_scoped(self):
        with patch('tools.nse2_sim_check.CODE_ERROR_FRAME', hashlib.sha256(b'frame').hexdigest()):
            result = verify_security_attempt(contacts(), b'frame')
        self.assertEqual(result['physical_phone_code_attempts'], 1)
        self.assertFalse(result['phone_code_authenticated'])
        self.assertFalse(result['exact_editor_buffer_unproved'])
        self.assertTrue(result['submitted_code_matches_cached_record'])
        self.assertEqual(result['retry_counter'], 255)

    def test_rejection_cannot_be_attributed_to_retry_without_comparison_evidence(self):
        for old, new in (('a=d83b30d8', 'a=ffffffff'), ('attempts=ff', 'attempts=00'),
                         ('input=313233343500', 'input=313233343600'), ('t=15.070', 't=14.0')):
            with self.subTest(old=old), \
                    patch('tools.nse2_sim_check.CODE_ERROR_FRAME', hashlib.sha256(b'frame').hexdigest()), \
                    self.assertRaises(ValueError):
                verify_security_attempt(contacts().replace(old, new), b'frame')

    def test_zero_attempt_fixture_requires_own_successful_return(self):
        text = contacts().replace('attempts=ff', 'attempts=00')
        returned = '\nnse2_security_return: accepted=1 attempts=00 t=15.071 observer_writes=0'
        with patch('tools.nse2_sim_check.AUTHENTICATED_FRAME', hashlib.sha256(b'frame').hexdigest()):
            with self.assertRaisesRegex(ValueError, 'successful return'):
                verify_security_attempt(text, b'frame', retry_zero=True)
            result = verify_security_attempt(text + returned, b'frame', retry_zero=True)
        self.assertTrue(result['phone_code_authenticated'])
        self.assertFalse(result['usable_boot'])

    def test_missing_extra_wrong_and_outside_window_contacts_fail(self):
        for text in (contacts().replace('key=01', 'key=02'),
                     contacts().replace('t=12.1', 't=11.1'),
                     contacts() + '\nnse2_security_contact: index=1 down=1 t=18 physical_only=1',
                     contacts().replace('t=12.2', 't=12.5')):
            with self.subTest(text=text[:70]), self.assertRaises(ValueError):
                verify_security_attempt(text, b'frame')


if __name__ == '__main__':
    unittest.main()

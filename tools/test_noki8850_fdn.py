import unittest
from pathlib import Path
from unittest.mock import patch
import tempfile
from PIL import Image

from tools.run_noki8850_fdn import expected_card,check_phase,check_frames,phase_plan,record_card,check_record,check_dial,dial_card,configure_lab_call_peer
import xml.etree.ElementTree as ET
from tools.run_noki8850_pin_retry_cold import mame_command


def trace(phase):
    lines=['sim_device: header cla=a0 ins=20 p1=00 p2=01 p3=08',
           'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1']
    identities=[['6f7e','6f07'],['6f7e','6f07'],['6f7e','6f07','6f3a'],[]][phase]
    if phase==1:
        for fid in identities:
            lines += [f'sim_device: header cla=a0 ins=44 p1=00 p2=00 p3=00 selected={fid}',
                      'SIM status ins=44 sw=9000 chv=3/3 puk=10/10 enabled=1']
    lines += ['8850_fdn: key=Menu state=1 t=37.000000000']
    if phase%2==0:
        lines += ['8850_fdn: key=Menu state=1 t=44.000000000',
                  'sim_device: header cla=a0 ins=20 p1=00 p2=02 p3=08',
                  'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1']
        if phase==0:
            lines += ['sim_device: header cla=a0 ins=04 p1=00 p2=00 p3=00 selected=6f3a',
                      'SIM status ins=04 sw=9000 chv=3/3 puk=10/10 enabled=1']
        for fid in identities:
            lines += [f'sim_device: header cla=a0 ins=44 p1=00 p2=00 p3=00 selected={fid}',
                      'SIM status ins=44 sw=9000 chv=3/3 puk=10/10 enabled=1']
    lines += [f'sim_device: header cla=a0 ins=b2 p1={i:02x} p2=04 p3=20 selected=6f3b' for i in range(1,11)]
    return '\n'.join(lines+['8850_fdn: key=Menu state=1 t=49.000000000',
                           '8850_fdn: complete=1 firmware_state_forcing=0'])


class FdnValidityTest(unittest.TestCase):
    def test_lifecycle_predecessors_preserve_cancel_and_erase_boundaries(self):
        plan=phase_plan(lifecycle=True)
        self.assertEqual([(p.name,p.source,p.record) for p in plan[6:]],
            [('edit','read','edit'),('edited_read','edit','edited_read'),
             ('cancel','edited_read','cancel'),('cancelled_read','cancel','cancelled_read'),
             ('erase','cancelled_read','erase'),('empty','erase','empty')])
        self.assertEqual(plan[:6],phase_plan(records=True))

    def test_cancel_and_erase_require_distinct_card_and_update_results(self):
        for mode in ('cancel','erase'):
            lines=['sim_device: header cla=a0 ins=20 p1=00 p2=01 p3=08',
                   'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1']
            for fid in ('6f7e','6f07'):
                lines += [f'sim_device: header cla=a0 ins=44 p1=00 p2=00 p3=00 selected={fid}',
                          'SIM status ins=44 sw=9000 chv=3/3 puk=10/10 enabled=1']
            lines += [f'sim_device: header cla=a0 ins=b2 p1={i:02x} p2=04 p3=20 selected=6f3b' for i in range(1,11)]
            lines += ['sim_device: header cla=a0 ins=b2 p1=01 p2=04 p3=20 selected=6f3b']*(6 if mode=='cancel' else 3)
            lines += ['8850_fdn: key=Menu state=1 t=40.000000000',
                      '8850_fdn: key=Menu state=1 t=49.000000000',
                      'sim_device: header cla=a0 ins=20 p1=00 p2=02 p3=08',
                      'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1',
                      '8850_fdn: key=Menu state=1 t=51.000000000',
                      f'8850_fdn: key={"Names / C" if mode=="cancel" else "Menu"} state=1 t=52.000000000']
            if mode=='erase':lines += ['sim_device: header cla=a0 ins=dc p1=01 p2=04 p3=20 selected=6f3b',
                                      'SIM status ins=dc sw=9000 chv=3/3 puk=10/10 enabled=1']
            lines += ['8850_fdn: key=Menu state=1 t=59.000000000',
                      '8850_fdn: complete=1 firmware_state_forcing=0']
            text='\n'.join(lines)
            after=record_card(True,True) if mode=='cancel' else record_card(False)
            check_record(text,record_card(True,True),after,mode)
            with self.assertRaises(ValueError):
                check_record(text,record_card(True,True),record_card(False) if mode=='cancel' else record_card(True,True),mode)
            with self.assertRaises(ValueError):
                check_record(text.replace('t=52.','t=53.'),record_card(True,True),after,mode)

    def test_dial_controls_share_saved_on_state_and_use_physical_off_predecessor(self):
        plan=phase_plan(dial=True)
        self.assertEqual([(p.name,p.source,p.dial) for p in plan[-4:]],
            [('listed','read','listed'),('denied','read','denied'),
             ('off_saved','read',None),('off_call','off_saved','off')])
        self.assertTrue(plan[-2].saved_record)
        self.assertEqual(plan[-2].validity,2)
        with tempfile.TemporaryDirectory() as temporary:
            path=Path(temporary)/'cfg'
            configure_lab_call_peer(path)
            tree=ET.parse(path/'nsm2hle.cfg')
            self.assertEqual(tree.find('./system').get('name'),'nsm2hle')
            self.assertEqual(tree.find("./system/input/port[@tag=':CALLHOST']").get('value'),'0')

    def test_denied_dial_requires_card_lookup_and_zero_call_establishment(self):
        lines=['sim_device: header cla=a0 ins=20 p1=00 p2=01 p3=08',
               'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1']
        for fid in ('6f7e','6f07'):
            lines += [f'sim_device: header cla=a0 ins=44 p1=00 p2=00 p3=00 selected={fid}',
                      'SIM status ins=44 sw=9000 chv=3/3 puk=10/10 enabled=1']
        lines += [f'sim_device: header cla=a0 ins=b2 p1={i:02x} p2=04 p3=20 selected=6f3b' for i in range(1,11)]
        lines += ['8850_fdn_dial: key=Call / Send state=1 t=32.000000000',
                  '8850_keypad_decoded key=0e',
                  'sim_device: header cla=a0 ins=b2 p1=01 p2=04 p3=20 selected=6f3b',
                  '8850_fdn_dial: key=End state=1 t=44.000000000',
                  '8850_fdn_dial: key=Menu state=1 t=46.000000000',
                  '8850_fdn_dial: complete=1 firmware_state_forcing=0']
        text='\n'.join(lines)
        check_dial(text,dial_card('denied'),dial_card('denied'),'denied')
        for bad in (text+'\nGSM service establish sapi=0 pd=03 message=05 ',
                    text.replace('key=0e','key=11'),
                    text.replace('p3=20 selected=6f3b','p3=20 selected=6f3a')):
            with self.assertRaises(ValueError):check_dial(bad,dial_card('denied'),dial_card('denied'),'denied')
        with self.assertRaises(ValueError):check_dial(text,dial_card('denied'),dial_card('off'),'denied')

    def test_record_branch_inherits_verified_on_not_the_later_off_branch(self):
        plan=phase_plan(True)
        self.assertEqual([(p.name,p.source,p.record) for p in plan[4:]],
                         [('write','on','write'),('read','write','read')])
        self.assertEqual(phase_plan(),plan[:4])

    def test_record_save_and_cold_read_have_exact_card_and_auth_contracts(self):
        for mode in ('write','read'):
            writing=mode=='write'
            lines=['sim_device: header cla=a0 ins=20 p1=00 p2=01 p3=08',
                   'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1']
            for fid in ('6f7e','6f07'):
                lines += [f'sim_device: header cla=a0 ins=44 p1=00 p2=00 p3=00 selected={fid}',
                          'SIM status ins=44 sw=9000 chv=3/3 puk=10/10 enabled=1']
            lines += [f'sim_device: header cla=a0 ins=b2 p1={i:02x} p2=04 p3=20 selected=6f3b' for i in range(1,11)]
            lines += ['8850_fdn: key=Menu state=1 t=40.000000000']
            if writing:
                lines += ['8850_fdn: key=Menu state=1 t=47.000000000',
                          'sim_device: header cla=a0 ins=20 p1=00 p2=02 p3=08',
                          'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1',
                          '8850_fdn: key=Menu state=1 t=55.000000000',
                          'sim_device: header cla=a0 ins=b2 p1=01 p2=04 p3=20 selected=6f3b',
                          'sim_device: header cla=a0 ins=dc p1=01 p2=04 p3=20 selected=6f3b',
                          'SIM status ins=dc sw=9000 chv=3/3 puk=10/10 enabled=1']
            lines += ['sim_device: header cla=a0 ins=b2 p1=01 p2=04 p3=20 selected=6f3b']*(1 if writing else 3)
            lines += [f'8850_fdn: key=Menu state=1 t={61 if writing else 49}.000000000',
                      '8850_fdn: complete=1 firmware_state_forcing=0']
            text='\n'.join(lines)
            initial=record_card(not writing)
            check_record(text,initial,record_card(True),mode)
            for offset in (0,3484,3517,3575,3610):
                final=bytearray(record_card(True))
                final[offset]^=1
                with self.subTest(mode=mode,offset=offset),self.assertRaises(ValueError):
                    check_record(text,initial,bytes(final),mode)
            with self.assertRaises(ValueError):check_record(text.replace('p3=20 selected=6f3b','p3=20 selected=6f3a'),initial,record_card(True),mode)

    def test_missing_or_blank_current_state_frame_fails(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary)
            with self.assertRaises(OSError):check_frames(root,3)
            Image.new('L',(84,48)).save(root/'8850_fdn_current.png')
            with self.assertRaises(ValueError):check_frames(root,3)

    def setUp(self):
        for name in ('contacts','check_pin_inputs','registration'):
            context=patch('tools.run_noki8850_fdn.'+name)
            context.start()
            self.addCleanup(context.stop)

    def test_all_four_validity_phases(self):
        for phase in range(4):
            with self.subTest(phase=phase):
                check_phase(trace(phase),expected_card(phase in (0,3),phase!=0),expected_card(phase>=2),phase)

    def test_cold_on_cannot_silently_reseed_a_valid_adn(self):
        with self.assertRaises(ValueError):check_phase(trace(1),expected_card(True),expected_card(False),1)

    def test_unrelated_card_change_and_duplicate_authentication_fail(self):
        for offset in (0,3484,3492,3516,3517,3574,3578):
            final=bytearray(expected_card(False))
            final[offset]^=1
            with self.subTest(offset=offset),self.assertRaises(ValueError):
                check_phase(trace(0),expected_card(True,False),bytes(final),0)
        with self.assertRaises(ValueError):
            check_phase(trace(0)+'\nSIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1',
                        expected_card(True,False),expected_card(False),0)

    def test_validity_requires_authentication_and_complete_fdn_scan(self):
        for text in (trace(0).replace('ins=04 p1=00','ins=44 p1=00'),
                     trace(0).replace('p2=02 p3=08','p2=01 p3=08'),
                     trace(0).replace('p1=0a p2=04','p1=0b p2=04')):
            with self.assertRaises(ValueError):check_phase(text,expected_card(True,False),expected_card(False),0)

    def test_shared_command_is_isolated_and_selects_only_product_script(self):
        command=mame_command(Path('/repo'),'fdn_on',55)
        self.assertEqual(command[:4],['/repo/mame/mame','nsm2hle','-rompath','/repo/roms'])
        for option,value in (('-nvram_directory','nvram'),('-cfg_directory','cfg'),
                             ('-autoboot_script','/repo/tools/noki8850_fdn_on.lua'),('-seconds_to_run','55')):
            self.assertEqual(command.count(option),1)
            self.assertEqual(command[command.index(option)+1],value)
        self.assertIn('-noreadconfig',command)


if __name__=='__main__':unittest.main()

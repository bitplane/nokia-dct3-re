import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

from tools import nsm1_toolkit_check as checker
from tools.sim_toolkit_display_check import DISPLAY, RESPONSE
from tools.sim_toolkit_interactive_check import INKEY,INPUT,INKEY_RESPONSE,INPUT_RESPONSE,MENU,MENU_RESPONSE,SELECT_ITEM,ITEM_RESPONSE,REFRESH,REFRESH_RESPONSE,PLAY_TONE,TONE_CANCEL_RESPONSE
from tools.test_nsm1_native_state_check import trace as startup


def wire(direction, data):
    return '\n'.join(f'nsm1_sim_wire: direction={direction} data={value:02x} t=62.0 observer_writes=0'
                     for value in data)


def trace(control=False):
    lines = [line for line in startup().splitlines() if not line.startswith('nsm1_state')]
    lines.append('read-binary fid=6fae offset=0 length=1 first=' + ('02' if control else '03'))
    if control:
        lines += [wire('TX', bytes.fromhex('a0a40000023f00')), wire('RX', b'\x90\x00')]
    else:
        lines += [wire('TX', bytes.fromhex('a010000005') + checker.PROFILE),
                  'SIM status ins=10 sw=9000 chv=3/3 puk=10/10 enabled=1',
                  'SIM completion ins=f2 sw=9116',
                  wire('TX', bytes.fromhex('a012000016')),
                  wire('RX', DISPLAY + b'\x90\x00')]
    lines += ['nsm1_toolkit_physical: action=dismiss down=1 t=68.000000000',
              'nsm1_toolkit_physical: action=dismiss down=0 t=68.200000000']
    if not control:
        lines += [wire('TX', bytes.fromhex('a01400000c') + RESPONSE),
                  'SIM status ins=14 sw=9000 chv=3/3 puk=10/10 enabled=1']
    return '\n'.join(lines)


def interactive_trace():
    text = trace().replace('SIM status ins=14 sw=9000','SIM status ins=14 sw=9115')
    for payload,response,actions,status in (
            (INKEY,INKEY_RESPONSE,('inkey_5','inkey_confirm'),0x911a),
            (INPUT,INPUT_RESPONSE,('input_4','input_2','input_confirm'),0x9000)):
        text += '\n'+wire('TX',bytes((0xa0,0x12,0,0,len(payload))))
        text += '\n'+wire('RX',payload+b'\x90\x00')
        for action in actions:
            for down in (1,0):
                text += f'\nnsm1_toolkit_input: action={action} down={down}'
        text += '\n'+wire('TX',bytes((0xa0,0x14,0,0,len(response)))+response)
        text += f'\nSIM status ins=14 sw={status:04x} chv=3/3 puk=10/10 enabled=1'
    return text


def menu_trace(item=1):
    text=interactive_trace().replace('SIM status ins=14 sw=9000','SIM status ins=14 sw=9128')
    text+='\n'+wire('TX',bytes.fromhex('a012000028'))+'\n'+wire('RX',MENU+b'\x90\x00')
    text+='\n'+wire('TX',bytes.fromhex('a01400000c')+MENU_RESPONSE)
    text+='\nSIM status ins=14 sw=9000 chv=3/3 puk=10/10 enabled=1'
    for action in ('main','up1','open')+(('next',) if item==2 else ())+('select',):
        for down in (1,0):
            text+=f'\nnsm1_toolkit_menu: action={action} down={down}'
    text+='\n'+wire('TX',bytes.fromhex('a0c2000009d307020201811001')+bytes((item,)))
    text+='\nSIM status ins=c2 sw=9000 chv=3/3 puk=10/10 enabled=1'
    return text


def select_item_trace():
    text=menu_trace().replace('ins=c2 sw=9000','ins=c2 sw=9122')
    text+='\n'+wire('TX',bytes.fromhex('a012000022'))+'\n'+wire('RX',SELECT_ITEM+b'\x90\x00')
    for action in ('next','confirm'):
        for down in (1,0):
            text+=f'\nnsm1_toolkit_select_item: action={action} down={down}'
    text+='\n'+wire('TX',bytes.fromhex('a01400000f')+ITEM_RESPONSE)
    text+='\nSIM status ins=14 sw=9000 chv=3/3 puk=10/10 enabled=1'
    return text


def refresh_trace():
    text=menu_trace().replace('ins=c2 sw=9000','ins=c2 sw=910b')
    text+='\n'+wire('TX',bytes.fromhex('a01200000b'))+'\n'+wire('RX',REFRESH+b'\x90\x00')
    text+='\n'+wire('TX',bytes.fromhex('a010000005')+checker.PROFILE)
    text+='\nSIM status ins=10 sw=9000 chv=3/3 puk=10/10 enabled=1'
    text+='\n'+wire('TX',bytes.fromhex('a01400000c')+REFRESH_RESPONSE)
    text+='\nSIM status ins=14 sw=9000 chv=3/3 puk=10/10 enabled=1'
    return text


class Nsm1ToolkitTest(unittest.TestCase):
    def test_tone_native_observation_requires_three_uncapped_nonstopped_samples(self):
        text='\n'.join(f'nsm1_toolkit_tone_native: phase={p} pc=47a1 idle=0 stop=0 t={t:.9f} observer_writes=0'
                       for p,t in (('before',85),('active',87),('after',91)))
        self.assertEqual(len(checker.check_tone_native(text)),3)
        for bad in (text.replace('stop=0','stop=1',1),text.split('\n',1)[1],
                    text+'\nnsm1_toolkit_tone_native: malformed',
                    text.replace('87.000000000','86.000000000'),text.replace('observer_writes=0','observer_writes=1')):
            with self.assertRaises(ValueError):
                checker.check_tone_native(bad)

    def test_tone_cancel_and_no_key_control_are_distinct(self):
        base=menu_trace().replace('ins=c2 sw=9000','ins=c2 sw=911c')
        base+='\n'+wire('TX',bytes.fromhex('a01200001c'))+'\n'+wire('RX',PLAY_TONE+b'\x90\x00')
        checker.check_trace(base,tone='hold')
        action='\nnsm1_toolkit_tone: action=quit down=1\nnsm1_toolkit_tone: action=quit down=0'
        response=wire('TX',bytes.fromhex('a01400000c')+TONE_CANCEL_RESPONSE)
        complete=base+action+'\n'+response+'\nSIM status ins=14 sw=9000 chv=3/3 puk=10/10 enabled=1'
        checker.check_trace(complete,tone='cancel')
        for bad in (complete.replace(action,''),
                    complete.replace(response,response.replace('data=11 ','data=00 ')),
                    complete.replace('ins=c2 sw=911c','ins=c2 sw=9000'),
                    complete.replace(wire('RX',PLAY_TONE+b'\x90\x00'),wire('RX',PLAY_TONE[:-1]+b'\x03\x90\x00')),
                    action+complete.replace(action,''),
                    complete+'\nnsm1_toolkit_tone: malformed'):
            with self.subTest(tail=bad[-90:]),self.assertRaises(ValueError):
                checker.check_trace(bad,tone='cancel')
        for text,mode in ((complete,'hold'),(base,'cancel')):
            with self.assertRaises(ValueError):
                checker.check_trace(text,tone=mode)
        for kwargs in (dict(control=True),dict(select_item=True),dict(refresh=True),dict(menu_item=2)):
            with self.assertRaises(ValueError):
                checker.profile_for(tone='cancel',**kwargs)

    def test_refresh_requires_reprofile_before_success_and_own_payload(self):
        text=refresh_trace()
        checker.check_trace(text,refresh=True)
        profile=wire('TX',bytes.fromhex('a010000005')+checker.PROFILE)
        response=wire('TX',bytes.fromhex('a01400000c')+REFRESH_RESPONSE)
        missing=text.rsplit(profile,1)
        for bad in (''.join(missing),text.replace('ins=c2 sw=910b','ins=c2 sw=9000'),
                    text.replace(wire('RX',REFRESH+b'\x90\x00'),wire('RX',REFRESH[:-1]+b'\x83\x90\x00')),
                    text.replace(response,response.replace('data=00 ','data=30 ')),
                    text.replace('action=select','action=missing'),
                    text+'\nSIM status ins=10 sw=9000 chv=3/3 puk=10/10 enabled=1'):
            with self.subTest(tail=bad[-90:]),self.assertRaises(ValueError):
                checker.check_trace(bad,refresh=True)
        for kwargs in (dict(control=True),dict(select_item=True),dict(menu_item=2)):
            with self.assertRaises(ValueError):
                checker.profile_for(refresh=True,**kwargs)
        with self.assertRaises(ValueError):
            checker.check_trace(text,menu_item=1)

    def test_followup_item_requires_own_card_payload_and_physical_beta(self):
        text=select_item_trace()
        checker.check_trace(text,select_item=True)
        response=wire('TX',bytes.fromhex('a01400000f')+ITEM_RESPONSE)
        early=text.replace(response,'').replace('nsm1_toolkit_select_item: action=confirm down=1',
                    response+'\nnsm1_toolkit_select_item: action=confirm down=1')
        wrong=response.rsplit('data=02 ',1)
        for bad in (early,text.replace('ins=c2 sw=9122','ins=c2 sw=9000'),
                    text.replace(response,'data=01 '.join(wrong)),
                    text.replace('action=next','action=wrong'),
                    text+'\nnsm1_toolkit_select_item: malformed'):
            with self.subTest(tail=bad[-100:]), self.assertRaises(ValueError):
                checker.check_trace(bad,select_item=True)
        with self.assertRaises(ValueError):
            checker.check_trace(text,menu_item=1)
        with self.assertRaises(ValueError):
            checker.check_trace(menu_trace(),select_item=True)
        with self.assertRaises(ValueError):
            checker.profile_for(menu_item=2,select_item=True)
        with self.assertRaises(ValueError):
            checker.profile_for(control=True,select_item=True)

    def test_menu_envelope_requires_installation_and_physical_choice(self):
        for item in (1,2):
            text=menu_trace(item)
            checker.check_trace(text,menu_item=item)
            envelope=wire('TX',bytes.fromhex('a0c2000009d307020201811001')+bytes((item,)))
            early=text.replace(envelope,'').replace('nsm1_toolkit_menu: action=select down=1',
                        envelope+'\nnsm1_toolkit_menu: action=select down=1')
            for bad in (early,text.replace('ins=c2 sw=9000','ins=c2 sw=6a80'),
                        text.replace('action=select','action=wrong'),
                        text.replace('ins=14 sw=9128','ins=14 sw=9000'),
                        text+'\nnsm1_toolkit_menu: malformed'):
                with self.assertRaises(ValueError):
                    checker.check_trace(bad,menu_item=item)
            with self.assertRaises(ValueError):
                checker.check_trace(text,menu_item=3-item)
            with self.assertRaises(ValueError):
                checker.check_trace(text,interactive=True)
        for bad in (0,3,-1):
            with self.assertRaises(ValueError):
                checker.profile_for(menu_item=bad)
        with self.assertRaises(ValueError):
            checker.profile_for(control=True,menu_item=1)

    def test_interactive_values_and_order_not_inferred_from_display(self):
        text=interactive_trace()
        checker.check_trace(text,interactive=True)
        response=wire('TX',bytes.fromhex('a014000011')+INPUT_RESPONSE)
        early=text.replace(response,'').replace('nsm1_toolkit_input: action=input_confirm down=1',
                    response+'\nnsm1_toolkit_input: action=input_confirm down=1')
        for bad in (early,text.replace('ins=14 sw=911a','ins=14 sw=9000'),
                    text.replace('direction=RX data=45 ','direction=RX data=46 ',1),
                    text.replace('action=inkey_5','action=inkey_6'),
                    text+'\nnsm1_toolkit_input: malformed',
                    text+'\nnsm1_toolkit_physical: malformed',
                    text+'\nnsm1_restart_request:',
                    text+'\nLUA ERROR'):
            with self.subTest(tail=bad[-100:]), self.assertRaises(ValueError):
                checker.check_trace(bad,interactive=True)
        with self.assertRaises(ValueError):
            checker.check_trace(trace(),interactive=True)
        with self.assertRaises(ValueError):
            checker.check_trace(text)

    def test_interactive_configuration_and_conflicting_modes(self):
        with TemporaryDirectory() as temporary:
            seed,run=(Path(temporary)/name for name in ('seed','run'))
            for directory in (seed,run):
                (directory/'cfg').mkdir(parents=True)
            template='<mameconfig><system name="nsm1r4t"><input>{}</input></system></mameconfig>'
            (seed/'cfg/nsm1r4t.cfg').write_text(template.format(''))
            (run/'cfg/nsm1r4t.cfg').write_text(template.format(
                '<port tag=":SATCFG" type="CONFIG" mask="15" defvalue="0" value="3"/>'))
            checker.check_configuration(seed,run,interactive=True)
            path=run/'cfg/nsm1r4t.cfg'
            path.write_text(path.read_text().replace('value="3"','value="4"'))
            for item in (1,2):
                checker.check_configuration(seed,run,menu_item=item)
            path.write_text(path.read_text().replace('value="4"','value="7"'))
            checker.check_configuration(seed,run,select_item=True)
            with self.assertRaises(ValueError):
                checker.check_configuration(seed,run,menu_item=1)
            with self.assertRaises(ValueError):
                checker.check_configuration(seed,run,interactive=True)
            with self.assertRaises(ValueError):
                checker.check_configuration(seed,run)
            with self.assertRaises(ValueError):
                checker.check_configuration(seed,run,True,True)
            with self.assertRaises(ValueError):
                checker.prepare(seed,run,True,True)

    def test_configuration_allows_only_declared_card_and_default_elision(self):
        with TemporaryDirectory() as temporary:
            seed, run = (Path(temporary) / name for name in ('seed', 'run'))
            for directory in (seed, run):
                (directory / 'cfg').mkdir(parents=True)
            template = '<mameconfig><system name="nsm1r4t"><input><port tag=":A" value="0"/>{}</input></system></mameconfig>'
            (seed / 'cfg/nsm1r4t.cfg').write_text(template.format(''))
            path = run / 'cfg/nsm1r4t.cfg'
            for profile in (0, 1):
                path.write_text(template.format(f'<port tag=":SATCFG" type="CONFIG" mask="15" defvalue="0" value="{profile}"/>'))
                checker.check_configuration(seed, run, not profile)
                with self.assertRaises(ValueError):
                    checker.check_configuration(seed, run, bool(profile))
            path.write_text(template.format(''))
            checker.check_configuration(seed, run, True)
            with self.assertRaises(ValueError):
                checker.check_configuration(seed, run)
            path.write_text(template.format('').replace('value="0"', 'value="1"'))
            with self.assertRaises(ValueError):
                checker.check_configuration(seed, run, True)

    def test_own_success_and_negative_control_are_distinct(self):
        checker.check_trace(trace())
        checker.check_trace(trace(True), True)
        for text, control in ((trace(), True), (trace(True), False)):
            with self.assertRaises(ValueError):
                checker.check_trace(text, control)

    def test_response_requires_physical_input_not_automatic_success(self):
        text = trace()
        response = wire('TX', bytes.fromhex('a01400000c') + RESPONSE)
        text = text.replace(response, '')
        marker = 'nsm1_toolkit_physical: action=dismiss down=1'
        text = text.replace(marker, response + '\n' + marker)
        with self.assertRaisesRegex(ValueError, 'precedes physical'):
            checker.check_trace(text)

    def test_profile_payload_status_contact_and_authentication_mutations_fail(self):
        text = trace()
        for bad in (text.replace('data=0b ', 'data=03 ', 1),
                    text.replace('direction=RX data=44 ', 'direction=RX data=45 ', 1),
                    text.replace('ins=14 sw=9000', 'ins=14 sw=6a80'),
                    text.replace('sw=9116', 'sw=9000'),
                    text.replace('first=03', 'first=02'),
                    text.replace('down=0 t=68.200000000', 'down=1 t=68.200000000'),
                    text.replace('ins=20 sw=9000', 'ins=20 sw=9804'),
                    text + '\nnsm1_restart_request: reason=00000068',
                    text + '\nunhandled opcode'):
            with self.subTest(tail=bad[-100:]), self.assertRaises(ValueError):
                checker.check_trace(bad)


if __name__ == '__main__':
    unittest.main()

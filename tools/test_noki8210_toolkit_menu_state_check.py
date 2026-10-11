import unittest
from tools.noki8210_toolkit_inkey_check import MENU_EVENTS
from tools.noki8210_toolkit_menu_state_check import verify


def complete():
    events = list(MENU_EVENTS[:MENU_EVENTS.index('8210_toolkit_physical: action=menu_select')])
    events.append('8210_menu_state: event=saved pc=1c sp=1384b4 ram=edbb543a t=57.000000000')
    for phase in ('reference', 'restored'):
        if phase == 'restored':
            events.extend((
                '8210_menu_state: event=restored pc=1c sp=1384b4 ram=edbb543a t=57.000000000',
                'state_roundtrip: result=pass scenario=8210_toolkit_menu requested_at=57.000000000 t=57.000000000',
            ))
        events.append(f'state_replay: phase={phase} event=begin t=57.000000000')
        events.append(f'8210_toolkit_physical: action=menu_select phase={phase}')
        events.extend(MENU_EVENTS[MENU_EVENTS.index('header cla=a0 ins=c2 p1=00 p2=00 p3=09'):])
        events.append(f'state_replay: phase={phase} event=end t=61.000000000')
    # The real protocol logger prefixes all card records consistently.
    return '\n'.join('sim_device: ' + event if event.startswith(('header ', 'envelope ', 'menu selection ', 'SIM status ')) else event for event in events)


class MenuStateTest(unittest.TestCase):
    def test_complete(self):
        verify(complete())

    def test_architecture_and_incomplete(self):
        for text in (complete().replace('event=restored pc=1c', 'event=restored pc=20'),
                     complete() + '\n8210_menu_state: FAIL incomplete',
                     complete() + '\n[LUA ERROR]'):
            with self.assertRaises(ValueError):
                verify(text)

    def test_replay_corruption_or_missing_input(self):
        text = complete()
        before, after = text.split('state_replay: phase=restored event=begin', 1)
        for changed in (after.replace('d30702020181100101', 'd30702020181100102'),
                        after.replace('action=menu_select phase=restored', 'action=ignored'),
                        after.replace('menu selection item=1 accepted', 'menu selection rejected')):
            with self.assertRaises(ValueError):
                verify(before + 'state_replay: phase=restored event=begin' + changed)


if __name__ == '__main__':
    unittest.main()

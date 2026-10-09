"""Shared accepted DISPLAY TEXT ordering; product fixtures own their frames."""
from tools.sim_toolkit_trace_check import require_in_order


def display_text_events(product, profile_length=9):
    return (
        'read-binary fid=6fae offset=0 length=1 first=03',
        f'header cla=a0 ins=10 p1=00 p2=00 p3={profile_length:02x}',
        'SIM status ins=10 sw=9000',
        'proactive DISPLAY TEXT ready',
        'SIM completion ins=f2 sw=9116',
        'header cla=a0 ins=12 p1=00 p2=00 p3=16',
        f'{product}_toolkit_physical: action=dismiss',
        'header cla=a0 ins=14 p1=00 p2=00 p3=0c',
        'terminal-response data=810301218002028281030100',
        'SIM status ins=14 sw=9000',
    )


def verify_display_text(text, product, profile_length=9):
    if '[LUA ERROR]' in text:
        raise ValueError(f'{product} physical Toolkit fixture failed')
    require_in_order(text.replace('[:sim_card] ', ''),
                     display_text_events(product, profile_length))


def interactive_events(product, profile_length=9):
    prefix = display_text_events(product, profile_length)
    return (*prefix[:6], f'{product}_toolkit_interactive: action=dismiss',
            *prefix[7:9], 'proactive GET INKEY ready', 'SIM status ins=14 sw=9115',
            'header cla=a0 ins=12 p1=00 p2=00 p3=15',
            f'{product}_toolkit_interactive: action=inkey_5',
            'header cla=a0 ins=14 p1=00 p2=00 p3=10',
            'terminal-response data=8103022200020282810301000d020435',
            'proactive GET INPUT ready', 'SIM status ins=14 sw=911a',
            'header cla=a0 ins=12 p1=00 p2=00 p3=1a',
            f'{product}_toolkit_interactive: action=input_4',
            f'{product}_toolkit_interactive: action=input_2',
            f'{product}_toolkit_interactive: action=confirm',
            'header cla=a0 ins=14 p1=00 p2=00 p3=11',
            'terminal-response data=8103032300020282810301000d03043432',
            'SIM status ins=14 sw=9000')


def verify_interactive(text, product, profile_length=9):
    if '[LUA ERROR]' in text:
        raise ValueError(f'{product} physical interactive Toolkit fixture failed')
    require_in_order(text.replace('[:sim_card] ', ''),
                     interactive_events(product, profile_length))


def menu_events(product, profile_length=9, selection_status='9000'):
    if selection_status not in ('9000', '9124', '911c'):
        raise ValueError('unsupported observed Toolkit menu selection status')
    return (*interactive_events(product, profile_length)[:-1],
            'proactive SET UP MENU ready', 'SIM status ins=14 sw=9128',
            'header cla=a0 ins=12 p1=00 p2=00 p3=28',
            'terminal-response data=810304250002028281030100',
            'SIM status ins=14 sw=9000',
            f'{product}_toolkit_interactive: action=menu',
            f'{product}_toolkit_interactive: action=menu_last',
            f'{product}_toolkit_interactive: action=menu_open',
            f'{product}_toolkit_interactive: action=menu_select',
            'header cla=a0 ins=c2 p1=00 p2=00 p3=09',
            'envelope data=d30702020181100101', f'SIM status ins=c2 sw={selection_status}',
            f'{product}_toolkit_interactive: action=menu_exit')


def verify_menu(text, product, profile_length=9, selection_status='9000'):
    if '[LUA ERROR]' in text:
        raise ValueError(f'{product} physical Toolkit menu fixture failed')
    require_in_order(text.replace('[:sim_card] ', ''), menu_events(product, profile_length, selection_status))

"""Qualify paired early/late physical-PIN continuation observations, not recovery."""
import argparse
from collections import Counter
import json
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from tools.noki8210_radio_contract import verify as verify_source
from tools.noki8210_registration_check import verify as verify_registration


def rows(text, marker):
    result = []
    for line in text.splitlines():
        if marker+':' not in line:
            continue
        row = {}
        for token in line.split(marker+':',1)[1].split():
            key,separator,value = token.partition('=')
            if not separator or key in row:
                raise ValueError('malformed phase record')
            row[key] = value
        if row.get('observer_writes') != '0':
            raise ValueError('phase observer scope changed')
        result.append(row)
    return result


def verify(text, *, late):
    if '[LUA ERROR]' in text:
        raise ValueError('observer/physical fixture failed')
    terminals = rows(text,'8210_phase_terminal')
    events = rows(text,'8210_phase_context')
    if len(terminals) != 1 or not 46 <= float(terminals[0]['t']) < 47:
        raise ValueError('missing complete observation horizon')
    terminal = terminals[0]
    counts = Counter()
    names = dict(store='stores',dispatch='dispatches',zero_gate='gates',nonzero_exit='exits',publish='publications')
    previous = -1
    for event in events:
        site = event['site']
        if site not in names or event['root'] != '00138038':
            raise ValueError('unknown phase site or context')
        counts[site] += 1
        if int(event['ordinal']) != counts[site] or int(event['cycles'],16) <= previous:
            raise ValueError('missing/reordered phase observation')
        previous = int(event['cycles'],16)
    for site,name in names.items():
        if int(terminal[name]) != counts[site] or counts[site] >= int(terminal['cap']):
            raise ValueError('phase counters are shared, missing or capped')
    if terminal['root'] != '00138038':
        raise ValueError('terminal context root differs')
    if late:
        if events or terminal['state'] != '000d' or terminal['phase'] != '00000000':
            raise ValueError('late control no longer has the characterized unvisited phase-zero context')
        activity = re.findall(r'8210_cell_trace: [^\n]*text=PH_9000 t=([0-9.]+)',text)
        if not activity or max(map(float,activity)) < 45:
            raise ValueError('absence lacks continued receive-loop activity near the horizon')
    else:
        if terminal['state'] != '0007' or terminal['phase'] != '00000001':
            raise ValueError('early terminal context differs')
        if dict(counts) != dict(store=2,dispatch=2,zero_gate=2,publish=1,nonzero_exit=1):
            raise ValueError('early continuation sequence differs')
        if [event['site'] for event in events] != ['store','dispatch','zero_gate','publish',
                                                  'store','dispatch','zero_gate','nonzero_exit']:
            raise ValueError('early phase branches are not ordered')
        first,second = events[:4],events[4:]
        if any(event['phase'] != '00000000' or event['state'] != '000f' or event['input'] != '1802' for event in first):
            raise ValueError('early state-15 completion does not reach the zero-phase publication')
        if second[0]['r0'] != '0000000a' or any(event['phase'] != '0000000a' for event in second[1:]):
            raise ValueError('later phase store/dispatch differs')
    return dict(counts=dict(counts), terminal_state=terminal['state'], terminal_phase=terminal['phase'],
                horizon=float(terminal['t']), recovery_validated=False)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image',type=Path)
    parser.add_argument('early',type=Path)
    parser.add_argument('late',type=Path)
    args = parser.parse_args()
    try:
        verify_source(args.image.read_bytes())
        reports = {}
        for late,run in ((False,args.early),(True,args.late)):
            text = (run/'error.log').read_text()
            pin = re.findall(r'8210_pin_physical: key=([^\n]+?) t=([0-9.]+)',text)
            start = 8 if late else 7
            if ([key for key,_ in pin] != ['Keypad 1','Keypad 2','Keypad 3','Keypad 4','Menu']
                    or [float(when) for _,when in pin] != [start+i for i in range(5)]
                    or 'SIM status ins=20 sw=9000' not in text):
                raise ValueError('missing paired physical PIN timing/acceptance')
            if not late:
                verify_registration(text,(run/'nvram/nsm3hle/sim_card').read_bytes(),dcs1800=True)
            elif 'LAPDm Location Updating Accept acknowledged' in text:
                raise ValueError('late run now registers; update the falsified characterization')
            reports['late' if late else 'early'] = verify(text,late=late)
        print(json.dumps(reports,indent=2))
    except (KeyError,OSError,ValueError) as error:
        print(f'8210 phase observation: FAIL: {error}',file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())

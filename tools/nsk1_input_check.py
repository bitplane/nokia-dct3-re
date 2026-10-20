"""Summarize physical NSK-1 contact observations without claiming UI acceptance."""
import re


CONTACT = re.compile(r'nsk1_key_contact: mask=([0-9a-f]{2}) down=([01]) irq=([0-9a-f]{2}) irqmask=([0-9a-f]{2}) ctrl=([0-9a-f]{2}) column_mask=([0-9a-f]{2}) root_byte=([0-9a-f]{2}) checklist=([0-9a-f]{2}) layout=([0-9a-f]{2}) simi_reads=(\d+) simi_writes=(\d+) t=([0-9.]+) physical_only=1')


def check(text):
    rows = list(CONTACT.finditer(text))
    if len(rows) != 6 or len(rows) != text.count('nsk1_key_contact:'):
        raise ValueError('missing or malformed physical contact observations')
    expected = [(4,1,12),(4,0,12.2),(16,1,14),(16,0,14.2),(2,1,16),(2,0,16.2)]
    contacts = []
    for match,(mask,down,time) in zip(rows,expected):
        data = match.groups()
        if (int(data[0],16),int(data[1])) != (mask,down) or not time <= float(data[11]) < time+0.1:
            raise ValueError('physical contact order or time changed')
        contacts.append(dict(zip(('mask','down','irq','irq_mask','control','column_mask','root_byte','checklist','layout','simi_reads','simi_writes'),
                                 [int(value,16 if index not in (1,9,10) else 10) for index,value in enumerate(data[:11])])))
    keys = re.findall(r'nsk1_key_forward: key=([0-9a-f]{2}) caller=[0-9a-f]{8} cycles=[0-9a-f]{16} observer_writes=0',text)
    scalars = re.findall(r'nsk1_task1_scalar: value=([0-9a-f]{8}) caller=([0-9a-f]{8}) cycles=[0-9a-f]{16} observer_writes=0',text)
    controls = re.findall(r'nsk1_control_reply: payload=([0-9a-f]{4}) envelope=[0-9a-f]{8}/[0-9a-f]{8} t=[0-9.]+ observer_writes=0',text)
    comparisons = re.findall(r'nsk1_checklist_compare: bits=([0-9a-f]{2}) cycles=[0-9a-f]{16} observer_writes=0',text)
    ready_inputs = re.findall(r'nsk1_ready_input: value=([0-9a-f]{2}) caller=([0-9a-f]{8}) cycles=[0-9a-f]{16} observer_writes=0',text)
    ready_snapshots = re.findall(r'nsk1_ready_flags_snapshot: flags=([0-9a-f]{22}) t=[0-9.]+ observer_writes=0',text)
    ready_writes = re.findall(r'nsk1_ready_flag_write: index=(10|[0-9]) value=([0-9a-f]{2}) pc=([0-9a-f]{8}) t=[0-9.]+ observer_writes=0',text)
    activations = re.findall(r'nsk1_task_activate: task=([0-9a-f]{2}) caller=([0-9a-f]{8}) cycles=[0-9a-f]{16} observer_writes=0',text)
    gates = re.findall(r'nsk1_application_gate: predicate=([0-9a-f]{2}) boot_reason=([0-9a-f]{2}) class=([0-9a-f]{2}) flags=([0-9a-f]{2}) stage=([0-9a-f]{2}) startup=([0-9a-f]{2}) cycles=[0-9a-f]{16} observer_writes=0',text)
    statuses = re.findall(r'nsk1_activation_status: value=([0-9a-f]{2}) pc=([0-9a-f]{8}) caller=([0-9a-f]{8}) t=[0-9.]+ observer_writes=0',text)
    checksums = re.findall(r'nsk1_security_checksum: computed=([0-9a-f]{4}) stored=([0-9a-f]{4}) cycles=[0-9a-f]{16} observer_writes=0',text)
    power_writes = re.findall(r'nsk1_power_write: field=(state|event) value=([0-9a-f]{4}) pc=([0-9a-f]{8}) t=([0-9.]+) observer_writes=0',text)
    power_snapshots = re.findall(r'nsk1_power_snapshot: state=([0-9a-f]{4}) event=([0-9a-f]{4}) sample=([0-9a-f]{4}) t=([0-9.]+) observer_writes=0',text)
    power_windows = re.findall(r'nsk1_power_window: sample=([0-9a-f]{4}) lower=([0-9a-f]{4}) cycles=[0-9a-f]{16} observer_writes=0',text)
    report14_gates = re.findall(r'nsk1_report14_gate: remaining=([0-9a-f]{2}) cycles=[0-9a-f]{16} observer_writes=0',text)
    for marker,matches in (('task_activate',activations),('application_gate',gates),
                           ('activation_status',statuses),('security_checksum',checksums),
                           ('power_write',power_writes),('power_snapshot',power_snapshots),
                           ('power_window',power_windows),('report14_gate',report14_gates)):
        if len(matches) != text.count(f'nsk1_{marker}:'):
            raise ValueError('unrecognized activation trace format')
    for name,rows_ in (('input',ready_inputs),('flags_snapshot',ready_snapshots),('flag_write',ready_writes)):
        if len(rows_) != text.count(f'nsk1_ready_{name}:'):
            raise ValueError('unrecognized readiness trace format')
    if (len(keys) != text.count('nsk1_key_forward:') or len(scalars) != text.count('nsk1_task1_scalar:')
            or len(controls) != text.count('nsk1_control_reply:')
            or len(comparisons) != text.count('nsk1_checklist_compare:')):
        raise ValueError('unrecognized input/startup trace format')
    return {'contacts':contacts,'forwarded_keys':[int(key,16) for key in keys],
            'task1_scalars':[{'value':int(value,16),'caller':caller} for value,caller in scalars],
            'control_reply_payloads':controls,
            'checklist_comparisons':[int(value,16) for value in comparisons],
            'ready_inputs':[{'value':int(value,16),'caller':caller} for value,caller in ready_inputs],
            'ready_flag_snapshots':ready_snapshots,
            'ready_flag_writes':[{'index':int(index),'value':int(value,16),'pc':pc} for index,value,pc in ready_writes],
            'task_activations':[{'task':int(task,16),'caller':caller} for task,caller in activations],
            'application_gates':[dict(zip(('predicate','boot_reason','class','flags','stage','startup'),
                                         (int(value,16) for value in row))) for row in gates],
            'activation_status_writes':[{'value':int(value,16),'pc':pc,'caller':caller} for value,pc,caller in statuses],
            'security_checksums':[{'computed':int(computed,16),'stored':int(stored,16)} for computed,stored in checksums],
            'power_writes':[{'field':field,'value':int(value,16),'pc':pc,'time':float(time)} for field,value,pc,time in power_writes],
            'power_snapshots':[{'state':int(state,16),'event':int(event,16),'sample':int(sample,16),'time':float(time)}
                               for state,event,sample,time in power_snapshots],
            'power_windows':[{'sample':int(sample,16),'lower':int(lower,16)} for sample,lower in power_windows],
            'report14_countdowns':[int(value,16) for value in report14_gates],
            'scope':'physical-contact and passive startup observation, not interactive UI acceptance'}

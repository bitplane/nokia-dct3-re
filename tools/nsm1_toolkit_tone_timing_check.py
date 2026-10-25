"""Own NSM-1 delayed organic tone completion; not physical timer calibration."""
import re
from tools.sim_toolkit_display_check import wire
from tools.sim_fdn_protocol_check import apdus


def check_trace(text):
    entries=re.findall(r'nsm1_tone_timing_entry: pc=([0-9a-f]{8}) r0=([0-9a-f]{8}) r1=([0-9a-f]{8}) caller=([0-9a-f]{8}) t=([0-9.]+) observer_writes=0',text)
    if (len(entries)!=2 or text.count('nsm1_tone_timing_entry:')!=2
            or [row[:4] for row in entries]!=[
                ('002bd8de','00004190','0000000a','00273921'),
                ('00275106','00000052','000000fa','0027393d')]
            or not 86<float(entries[0][4])<float(entries[1][4])<86.2):
        raise ValueError('executed tone request or 250-tick schedule differs')
    rows=re.findall(r'nsm1_tone_timing_write: address=([0-9a-f]{8}) value=([0-9a-f]{8}) mask=([0-9a-f]{8}) pc=([0-9a-f]{8}) t=([0-9.]+) observer_writes=0',text)
    expected=[
        ('00100514','00010000','ffff0000','0027509c'),
        ('00100510','00100528','ffffffff','002750aa'),
        ('00100518','02000000','ff000000','0027522c'),
        ('00100514','007d0000','ffff0000','002751ac'),
        ('00100510','0010015c','ffffffff','002750ac'),
        ('00100514','00000000','ffff0000','002758fa'),
        ('00100518','03000000','ff000000','00275348'),
        ('00100510','ffffffff','ffffffff','0027537a'),
        ('00100510','00100528','ffffffff','002753a2'),
        ('00100518','01000000','ff000000','00275f9e'),
        ('00100510','ffffffff','ffffffff','00275faa')]
    if len(rows)!=11 or text.count('nsm1_tone_timing_write:')!=11 or [row[:4] for row in rows]!=expected:
        raise ValueError('timer52 insertion/expiry/consumption sequence differs')
    times=[float(row[4]) for row in rows]
    if (times!=sorted(times) or not all(86<t<87 for t in times[:5])
            or not all(93<t<93.2 for t in times[5:])):
        raise ValueError('early or late timer horizon differs')
    ack=re.findall(r'nsm1_tone_timing_fiq5_ack: data=([0-9a-f]{2}) pc=([0-9a-f]{8}) t=([0-9.]+) observer_writes=0',text)
    if len(ack)!=1 or text.count('nsm1_tone_timing_fiq5_ack:')!=1 or int(ack[0][0],16)&0x20==0 or not 93<float(ack[0][2])<93.2:
        raise ValueError('organic FIQ5 acknowledgement missing or outside expiry window')
    consumed=text.index('nsm1_tone_timing_write: address=00100518 value=01000000')
    if sum(row[0]==0x14 for row in apdus(wire(text[:consumed],'TX','nsm1')))!=4:
        raise ValueError('terminal response preceded timer consumption')
    final=re.findall(r'nsm1_tone_timing_final: record=([0-9a-f]{24}) pc=([0-9a-f]{4}) idle=([01]) stop=([01]) t=([0-9.]+) observer_writes=0',text)
    if len(final)!=1 or text.count('nsm1_tone_timing_final:')!=1 or (final[0][0],final[0][3],final[0][4])!=('ffffffff0000050301520000','0','93.300000000'):
        raise ValueError('final inactive timer or native observation differs')
    if text.count('nsm1_tone_timing_exit: t=93.500000000 observer_writes=0')!=1 or text.count('nsm1_tone_timing_exit:')!=1:
        raise ValueError('bounded clean exit missing')

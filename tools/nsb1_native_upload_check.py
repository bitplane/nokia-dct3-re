"""Verify NSB-1's own upload is retained, not merely accepted by the bus."""
import re


def check_upload(trace):
    expected = {0x29e1: 0x7714, 0x29e2: 0x085e, 0x29e3: 0x7194,
                0x29e4: 0x1f2b, 0x29e5: 0x1284}
    writes = re.findall(r'nsb1_program_extent: address=([0-9a-f]{4}) data=([0-9a-f]{4}) mask=ffff', trace)
    if len(writes) != len(expected) or {int(a,16):int(w,16) for a,w in writes} != expected:
        raise ValueError('NSB-1 hook upload differs or is incomplete')
    summaries = re.findall(r'nsb1_program_extent_summary: writes=(\d+) highest=([0-9a-f]{4}) observer_writes=0', trace)
    if summaries != [('825','2b38')]:
        raise ValueError('NSB-1 upload extent differs')
    visible = re.findall(r'nsb1_program_visible: address=([0-9a-f]{4}) program=([0-9a-f]{4}) data=([0-9a-f]{4}) observer_writes=0', trace)
    if len(visible) != len(expected) or {int(a,16):(int(p,16),int(d,16)) for a,p,d in visible} != {a:(w,w) for a,w in expected.items()}:
        raise ValueError('NSB-1 upload was discarded or overlay is incoherent')


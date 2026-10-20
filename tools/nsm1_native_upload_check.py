"""Check NSM-1's upload-derived bank without claiming fitted-mask identity."""
import re


def check_upload(trace):
    summaries = re.findall(r'nsm1_program_extent_summary: writes=(\d+) unique=(\d+) highest=([0-9a-f]{4}) retained=(\d+) observer_writes=0',trace)
    if summaries != [('605','605','2a5c','605')]:
        raise ValueError('NSM-1 upload extent or complete retention differs')
    expected = dict(enumerate((0x1221,0xf062,0x005a,0x8184,0xf074,0x48f0,0xf479,0x80f8),0x2800))
    rows = re.findall(r'nsm1_program_visible: address=([0-9a-f]{4}) uploaded=([0-9a-f]{4}) program=([0-9a-f]{4}) data=([0-9a-f]{4}) observer_writes=0',trace)
    if len(rows)!=8 or {int(a,16):(int(w,16),int(p,16),int(d,16)) for a,w,p,d in rows}!={a:(w,w,w) for a,w in expected.items()}:
        raise ValueError('NSM-1 executable upload is not coherent in both views')


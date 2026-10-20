"""Own NSK-3 boot4 upload retention, not fitted-mask or RF acceptance."""
import re


def check_upload(trace):
    summaries=re.findall(r'nsk3_program_extent_summary: writes=(\d+) unique=(\d+) highest=([0-9a-f]{4}) retained=(\d+) observer_writes=0',trace)
    if summaries!=[('811','811','2b2a','811')]:
        raise ValueError('NSK-3 executable upload extent or retention differs')
    expected=dict(enumerate((0x10f8,0x192e,0xf010,0x0200,0xf842,0x282f,0x61f8,0x21a1),0x2800))
    rows=re.findall(r'nsk3_program_visible: address=([0-9a-f]{4}) uploaded=([0-9a-f]{4}) program=([0-9a-f]{4}) data=([0-9a-f]{4}) observer_writes=0',trace)
    if len(rows)!=8 or {int(a,16):(int(w,16),int(p,16),int(d,16)) for a,w,p,d in rows}!={a:(w,w,w) for a,w in expected.items()}:
        raise ValueError('NSK-3 executable upload is not coherent in both views')


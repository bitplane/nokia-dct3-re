"""Bounded NSK-3 loader writes and immutable ROM identity, not RF proof."""
import re


def check_census(trace):
    pattern=r'nsk3_program_write_page: page=([0-9a-f]{2}) writes=(\d+) unique=(\d+) min=([0-9a-f]{4}) max=([0-9a-f]{4}) nonzero=(\d+) visible=(\d+) observer_writes=0'
    rows=re.findall(pattern,trace)
    expected_pages={0x1c,0x1d,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0xff}
    if len(rows)!=14 or len(rows)!=trace.count('nsk3_program_write_page:') or {int(row[0],16) for row in rows}!=expected_pages:
        raise ValueError('program write-page coverage differs')
    writes=unique=retained=0
    for page,nw,nu,lo,hi,nz,nv in rows:
        page,lo,hi=int(page,16),int(lo,16),int(hi,16)
        nw,nu,nz,nv=map(int,(nw,nu,nz,nv))
        if lo>>8!=page or hi>>8!=page or lo>hi or not 0<=nz<=nu<=nw:
            raise ValueError('program write-page bounds differ')
        if page==0xff:
            if (nw,nu,lo,hi,nz,nv)!=(2,1,0xff87,0xff87,1,0):
                raise ValueError('immutable ROM write observation differs')
        elif nv!=nu:
            raise ValueError('uploaded code was not retained')
        writes+=nw;unique+=nu;retained+=nv
    if (writes,unique,retained)!=(2290,2289,2288):
        raise ValueError('program write inventory differs')
    summaries=re.findall(r'nsk3_program_write_census: writes=(\d+) unique=(\d+) words_at_or_above_3000=(\d+) t=([0-9.]+) observer_writes=0',trace)
    if len(summaries)!=1 or summaries[0][:3]!=('2290','2289','1') or not 1<=float(summaries[0][3])<1.05:
        raise ValueError('one-second census is incomplete')
    high=re.findall(r'nsk3_program_high_write: address=([0-9a-f]{4}) data=([0-9a-f]{4}) mask=([0-9a-f]{4}) pc=([0-9a-f]{4})',trace)
    if high!=[('ff87','0004','ffff','0f12'),('ff87','0100','ffff','4a0d')]:
        raise ValueError('high program-write destinations differ')
    reads=re.findall(r'nsk3_program_identity_read: index=(\d+) value=([0-9a-f]{4}) pc=([0-9a-f]{4})',trace)
    if reads!=[('1','0004','0f15'),('2','0004','0f6a'),('3','0004','4a11')]:
        raise ValueError('ROM identity reads changed or include observer reads')
    return {'loader_horizon_seconds':1,'unique_program_destinations':unique,
            'retained_upload_words':retained,'immutable_rom_word':'0xff87',
            'fitted_memory_geometry_proved':False}


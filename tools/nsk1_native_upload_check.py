"""Check the own NSK-1 upload at its previously discarded executable hook."""
import re


def check_upload(trace, disassembly):
    expected = {0x29d3: 0x7714, 0x29d4: 0x085e, 0x29d5: 0x7194,
                0x29d6: 0x1f2b, 0x29d7: 0x1284}
    rows = re.findall(r'nsk1_program_extent: address=([0-9a-f]{4}) data=([0-9a-f]{4}) mask=ffff', trace)
    observed = {int(address, 16): int(word, 16) for address, word in rows}
    if observed != expected:
        raise ValueError('own executable hook upload differs or is incomplete')
    if 'nsk1_program_extent_summary: writes=811 highest=2b2a observer_writes=0' not in trace:
        raise ValueError('bounded upload extent differs')
    if 'c54x_zero_entry:' in trace:
        raise ValueError('execution still reaches a zero-word hook')
    for line in ('00:29D3: 7714 085E      STM     #$085E, $14',
                 '00:29D5: 7194 1F2B      MVDK    *AR4+, $1F2B'):
        if line not in disassembly:
            raise ValueError('uploaded hook is not visible in live program memory')


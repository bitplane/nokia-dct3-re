"""Read-only startup execution probe across all C54x XPC pages."""
import argparse
from pathlib import Path


def script(target, limit=8):
    if not 0 <= target <= 0xffff or not 1 <= limit <= 64:
        raise ValueError('16-bit target and bounded entry count required')
    lines = ['focus :dsp_c54x:cpu', 'temp9=0']
    for page in range(128):
        address = (page << 16) | target
        following = (page << 16) | ((target+1) & 0xffff)
        lines.append(f'bpset {address:x},temp9<{limit:x}&&pw@{address:x}==0,'
                     '{temp9=temp9+1;logerror "c54x_zero_entry: '
                     f'address={address:06x} pc=%06x xpc=%02x sp=%04x pmst=%04x st0=%04x st1=%04x '
                     'word=%04x next=%04x data=%04x stack=%04x/%04x/%04x/%04x cycles=%016x observer_writes=0\\n",'
                     f'pc,xpc,sp,pmst,st0,st1,pw@{address:x},pw@{following:x},dw@{target:x},'
                     'dw@sp,dw@((sp+1)&ffff),dw@((sp+2)&ffff),dw@((sp+3)&ffff),totalcycles;g}')
    lines.append('g')
    return '\n'.join(lines) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('target', type=lambda text: int(text, 0))
    parser.add_argument('output', type=Path)
    parser.add_argument('--limit', type=int, default=8)
    args = parser.parse_args()
    args.output.write_text(script(args.target, args.limit))


if __name__ == '__main__':
    main()

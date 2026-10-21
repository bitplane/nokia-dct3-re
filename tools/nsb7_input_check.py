"""Qualify NSB-7 research-HLE readiness and physical UI input, not SIM/RF."""
import argparse
import hashlib
from pathlib import Path
import re
from PIL import Image

IMAGE_SHA1 = '4dacc50f57fe295168a53d3e5855a81acf35ab96'
FRAMES = {
    'nsb7_key_8.png': '9aedae6df75fd95654276fe6ab22dd68065dad4f64394f439cb14fd3714630e3',
    'nsb7_key_10.png': '526e6f905bf0c5efc7011b5f5f5fff731ca8a39ad7041d811206360c696586cd',
}


def check_trace(text):
    if re.findall(r'nsb7_profile: machine=(\w+) observer_writes=0', text) != ['nsb7hle']:
        raise ValueError('requires the explicit research-HLE composition')
    if 'runtime_hle_handoff pc=2c75 native_suspended=1' not in text:
        raise ValueError('native/HLE boundary missing')
    if 'nsb7_card_fixture: present=0 physical_contact=1' not in text:
        raise ValueError('requires the explicit empty physical socket')
    if any(error in text for error in ('LUA ERROR', 'c54x_failure:', 'observer_writes=1')):
        raise ValueError('observer or emulation failure')
    values = re.findall(r'nsb7_startup_write: address=001308b1 value=([0-9a-f]{2}) ', text)
    if values != ['00', '08', '0a', '0e', '0f']:
        raise ValueError('startup readiness progression differs')
    if text.index('value=0f pc=0029ef2c') > text.index('nsb7_input:'):
        raise ValueError('readiness did not precede physical inputs')
    before = re.findall(r'nsb7_input_regs: phase=before .*?column_mask=([0-9a-f]{2}).*?ctrl=([0-9a-f]{2})', text)
    if before != [('20', '05'), ('20', '05')]:
        raise ValueError('keypad not enabled before both presses')
    for key, raw, decoded in [('Left Softkey / Menu', '06', '19'), ('Keypad 1', '07', '01')]:
        press = f'nsb7_input: key={key} state=1 '
        release = f'nsb7_input: key={key} state=0 '
        if text.count(press) != 1 or text.count(release) != 1:
            raise ValueError('physical contact inventory differs')
        start, end = text.index(press), text.index(release)
        if end <= start:
            raise ValueError('release precedes press')
        held = text[start:end]
        if not all(marker in held for marker in (
                'nsb7_key_irq: observer_writes=0',
                f'nsb7_key_raw: raw={raw} observer_writes=0',
                f'nsb7_key_decode: key={decoded} observer_writes=0')):
            raise ValueError('physical press did not reach IRQ/scanner/decoder')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    parser.add_argument('run', type=Path)
    args = parser.parse_args()
    try:
        if hashlib.sha1(args.image.read_bytes()).hexdigest() != IMAGE_SHA1:
            raise ValueError('requires own qualified NSB-7 image')
        check_trace((args.run / 'error.log').read_text())
        for name, expected in FRAMES.items():
            frame = Image.open(args.run / 'snap' / name).convert('RGB')
            if frame.size != (84, 48) or hashlib.sha256(frame.tobytes()).hexdigest() != expected:
                raise ValueError('firmware presentation differs: ' + name)
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSB-7 input: FAIL: {error}\n')
    print('PASS: research HLE; readiness 0f; physical Menu and digit-1 UI; no SIM/RF/native speech claim')


if __name__ == '__main__':
    main()

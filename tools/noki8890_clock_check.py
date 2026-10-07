"""Check physical 8890 clock/date settlement and subsequent digit ordering."""
import argparse
import hashlib
from pathlib import Path
import re

from PIL import Image


KEYS = [1, 2, 0, 0, 'Menu', 0, 7, 1, 0, 2, 0, 2, 6, 'Menu',
        1, 2, 3, 4, 5, 6, 7]
FRAMES = {
    '8890_date_entered.png': ((0, 8, 84, 32),
        '90b1acd3d0e2c5295144d8be2281db6ebf1623501cf963e22e749967cc809400'),
    '8890_date_after.png': ((0, 8, 84, 48),
        '46520fc623a6b562b2aee5f1c59e6943d4a5c1798eff4064091c38e7b3a285de'),
    '8890_clock_dialed.png': ((0, 8, 84, 48),
        '8e088cd7d6c7b5d7356b59f362cde5e5b0810a497cd701f3d82cd054af9a7846'),
}


def verify(text):
    events = list(re.finditer(r'8890_clock_physical: key=([^\r\n]+)', text))
    expected = ['Menu' if key == 'Menu' else f'Keypad {key}' for key in KEYS]
    if [event[1] for event in events] != expected:
        raise ValueError('physical clock/date/dial sequence mismatch')
    for index, (event, key) in enumerate(zip(events, KEYS)):
        end = events[index + 1].start() if index + 1 < len(events) else len(text)
        code = '19' if key == 'Menu' else f'{10 if key == 0 else key:02x}'
        if not re.search(rf'8890_keypad_decoded: key={code}\b', text[event.end():end]):
            raise ValueError(f'physical key did not decode at event {index}: {key}')


def check_frames(directory):
    # Exclude the advancing top-row clock; pin date text, idle and dial digits.
    for name, (region, expected) in FRAMES.items():
        with Image.open(directory / name) as frame:
            actual = hashlib.sha256(frame.convert('L').crop(region).tobytes()).hexdigest()
            if frame.size != (84, 48) or actual != expected:
                raise ValueError(f'reviewed clock lifecycle frame mismatch: {name}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('frames', type=Path)
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors='replace'))
        check_frames(args.frames)
    except (OSError, ValueError) as error:
        parser.exit(1, f'8890 clock lifecycle FAIL: {error}\n')
    print('8890 physical clock/date/idle/dial PASS; RTC persistence unproved')


if __name__ == '__main__':
    main()

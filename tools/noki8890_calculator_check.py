"""Verify physical NSB-6 calculator arithmetic, not merely app launch."""
import argparse
from hashlib import sha256
from pathlib import Path
import re

from PIL import Image


def verify(text):
    if '[LUA ERROR]' in text:
        raise ValueError('fixture error')
    cursor = 0
    for action, key in (('input_1', '01'), ('input_12', '02'),
                        ('operation_options', '19'), ('add', '18'),
                        ('plus', '19'), ('input_3', '03'),
                        ('options', '19'), ('result', '19')):
        marker = '8890_application_physical: action=' + action
        start = text.find(marker, cursor)
        if start < 0:
            raise ValueError(f'missing physical action {action}')
        end = text.find('8890_application_physical:', start + len(marker))
        if end < 0:
            end = len(text)
        if not re.search(rf'8890_keypad_decoded: key={key}\b', text[start:end]):
            raise ValueError(f'action {action} did not decode as {key}')
        cursor = end


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('frame', type=Path)
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors='replace'))
        with Image.open(args.frame) as frame:
            # Arithmetic area only, excluding softkeys and the status row.
            digest = sha256(frame.convert('L').crop((0, 8, 84, 36)).tobytes()).hexdigest()
            if frame.size != (84, 48) or digest != 'afed4f977aaabaad65fb2d0d0090d0056cb6cda2df297146ae7b90c302503f68':
                raise ValueError('reviewed arithmetic result 15 pixels differ')
    except (OSError, ValueError) as error:
        parser.exit(1, f'8890 calculator FAIL: {error}\n')
    print('8890 physical calculator 12+3=15 PASS')

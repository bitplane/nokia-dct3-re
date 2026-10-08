"""Own NSM-3 physical USSD and supplementary-service protocol acceptance."""
import argparse
import hashlib
from pathlib import Path
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.radio_ussd_trace_check import RESPONSE, RR_RELEASE, verify as verify_protocol


def verify_key_table(image):
    # The decoder loads this own-ROM pointer before its indexed LDRB.
    pointer = int.from_bytes(image[0x107e40:0x107e44], 'big')
    expected = bytes.fromhex('3e3e3e3e3e11190102030e170405060f18070809101a0c0a0b')
    if pointer != 0x33ee78 or image[0x13ee78:0x13ee91] != expected:
        raise ValueError('own NSM-3 keypad decoder table differs')


def verify(text, frames):
    if '[LUA ERROR]' in text:
        raise ValueError('physical USSD fixture failed')
    cursor = 0
    for key in ('Keypad *', 'Keypad 1', 'Keypad 2', 'Keypad 3', 'Keypad #', 'Call / Send'):
        event = '8210_ussd_physical: key=' + key
        index = text.find(event, cursor)
        if index < 0:
            raise ValueError('missing ordered physical input: ' + key)
        cursor = index + len(event)
    verify_protocol(text[cursor:], frames, require_frame=False)
    response = RESPONSE.search(text, cursor)
    release = RR_RELEASE.search(text, response.end())
    if '8210_ussd_physical: key=Back' not in text[release.end():]:
        raise ValueError('missing physical Back after USSD response')
    from PIL import Image
    for name, expected in (
            ('8210_ussd_result.png', 'eac7017b7ae7da13b4b33e5e47aac57495c8ca92554f812b52efb6443119efb1'),
            ('8210_ussd_after_back.png', 'd6d4b05af24a06c42a97e31c3134f7e497e149f4420f87f3d243613f06926300')):
        with Image.open(frames / name) as frame:
            if frame.size != (84, 48) or hashlib.sha256(frame.convert('L').tobytes()).hexdigest() != expected:
                raise ValueError('missing reviewed firmware frame: ' + name)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    args = parser.parse_args()
    try:
        text = args.log.read_text(errors='replace')
        verify(text, args.log.parent / 'snap')
        from tools.noki8210_registration_check import verify as verify_registration
        verify_registration(text, (args.log.parent / 'nvram/nsm3hle/sim_card').read_bytes())
    except (OSError, ValueError) as error:
        parser.exit(1, f'8210 USSD FAIL: {error}\n')
    print('8210 physical USSD protocol PASS')


if __name__ == '__main__':
    main()

"""Physical-input, protocol-order and reviewed frame checks for DCT3 SS."""
import hashlib
from PIL import Image


def verify_transaction(text, frames, service, keys, protocol, result_hash, idle_hash,
                       product='8210', geometry=(84, 48)):
    if '[LUA ERROR]' in text:
        raise ValueError('physical supplementary fixture failed')
    cursor = 0
    for key in keys:
        event = f'{product}_{service}_physical: key={key}'
        index = text.find(event, cursor)
        if index < 0:
            raise ValueError('missing ordered physical input: ' + key)
        cursor = index + len(event)
    protocol.verify(text[cursor:], frames, require_frame=False)
    response = protocol.RESPONSE.search(text, cursor)
    release = protocol.RR_RELEASE.search(text, response.end())
    if f'{product}_{service}_physical: key=Back' not in text[release.end():]:
        raise ValueError('missing physical Back after supplementary response')
    for phase, expected in (('result', result_hash), ('after_back', idle_hash)):
        name = f'{product}_{service}_{phase}.png'
        with Image.open(frames / name) as frame:
            if frame.size != geometry or hashlib.sha256(frame.convert('L').tobytes()).hexdigest() != expected:
                raise ValueError('missing reviewed firmware frame: ' + name)

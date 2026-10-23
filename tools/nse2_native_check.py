"""Verify native NSE-1 ROM4 compatibility execution of the own NSE-2 verifier."""
import re

from tools.nse2_boot_check import STATE, verify_nv, verify_record, verify_upload


def verify_native(text, payload, pixels, size, *, record_checksum=False):
    if text.count('Soft reset\n') != 1 or 'dsp_hle:' in text:
        raise ValueError('NSE-2 native reset/HLE boundary differs')
    staged = verify_upload(text, payload, native_response=True)
    images = re.findall(r'nse2_native_verifier_image: words=186 body=([0-9a-f]+) observer_writes=0', text)
    if len(images) != 1 or images[0] != payload[:372].hex():
        raise ValueError('NSE-2 executed verifier program image differs from own payload')
    identities = re.findall(r'nse2_native_identity: n=\d+ data=0004 mask=ffff pc=0f0f '
                            r't=([0-9.]+) observer_writes=0', text)
    responses = re.findall(r'nse2_startup_poll: n=1 address=00010004 data=0004 mask=ffff '
                           r'pc=002781fa t=([0-9.]+) observer_writes=0', text)
    if len(identities) != 1 or len(responses) != 1 or not 0 < float(identities[0]) < float(responses[0]):
        raise ValueError('NSE-2 own native verifier identity/MCU response missing or unordered')
    ready = re.findall(r'nse2_state_byte: address=0010b9f4 value=01 pc=002787e2 '
                       r't=([0-9.]+) observer_writes=0', text)
    if len(ready) != 1 or not float(responses[0]) < float(ready[0]) < 1:
        raise ValueError('NSE-2 organic native readiness state missing')
    if re.search(r'nse2_state_byte: address=0010b9f[45].*pc=002782(?:26|d6|d8)', text):
        raise ValueError('NSE-2 native loader took the silent-profile failure tail')
    samples = re.findall(r'nse2_native_state: t=([0-9.]+) pc=([0-9a-f]{4}) '
                         r'illegal=([01]) idle=([01]) observer_writes=0', text)
    if len(samples) < 8 or float(samples[-1][0]) < 8 or any(row[2] != '0' for row in samples):
        raise ValueError('NSE-2 native execution horizon missing or illegal')
    times = [float(row[0]) for row in samples]
    if times != sorted(set(times)):
        raise ValueError('NSE-2 native sample order differs')
    mcu = STATE.findall(text)
    if (len(mcu) < 8 or float(mcu[-1][0]) < 8
            or any(not (0x200040 <= int(row[1], 16) < 0x390000
                        and 0x100000 <= int(row[3], 16) < 0x110000) for row in mcu)):
        raise ValueError('NSE-2 own MCU execution horizon missing')
    mcu_times = [float(row[0]) for row in mcu]
    if mcu_times != sorted(set(mcu_times)):
        raise ValueError('NSE-2 MCU sample order differs')
    if record_checksum:
        verify_nv(text, checksum_only=True)
        verify_record(text)
    else:
        verify_nv(text)
    if size != (84, 48) or len(pixels) != 84 * 48:
        raise ValueError('NSE-2 frame geometry differs')
    return dict(native_verifier_executed=True, startup_identity=4,
                rounded_upload_words=staged['rounded_upload_words'],
                organic_ready_state=True, native_samples=len(samples),
                fitted_mask_proved=False, usable_boot=False, physical_key_delivery=False,
                graphical_boot=False, frame_blank=all(value == 255 for value in pixels))

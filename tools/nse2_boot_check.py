"""Check the erased-EEPROM/silent-DSP NSE-2 failure frame, not usable boot."""
import argparse
import hashlib
import json
from pathlib import Path
import re

from PIL import Image

FRAME = '7e3ade861af1e0e47c76100c7a7c7f8c7719c1c497e02d1024ab91c1e55c1f8e'
STATE = re.compile(
    r'nse2_state: t=([0-9.]+) pc=([0-9a-f]{8}) cpsr=([0-9a-f]{8}) '
    r'sp=([0-9a-f]{8}) shared_writes=(\d+) gensio_tx=(\d+) gensio_rx=(\d+) '
    r'lcd_commands=(\d+) lcd_bytes=(\d+) eeprom_signal_writes=(\d+) observer_writes=0')
SHARED = re.compile(r'nse2_shared: address=([0-9a-f]{8}) data=([0-9a-f]{4}) mask=ffff '
                    r'pc=([0-9a-f]{8}) t=([0-9.]+) observer_writes=0')
NV_BLOCKS = ((0x0000, 20, 0x0014, 0x258f09),
             (0x0018, 12, 0x0024, 0x258f2f),
             (0x0028, 24, 0x0040, 0x258f55),
             (0x0044, 480, 0x0224, 0x258f81),
             (0x0228, 20, 0x023c, 0x258fab))
NV_CHECKSUM = re.compile(r'nse2_nv_checksum: lr=([0-9a-f]{8}) length=(\d+) '
                         r'expected=([0-9a-f]{8}) stored=([0-9a-f]{8}) '
                         r't=([0-9.]+) observer_writes=0')


def verify_nv(text, *, checksum_only=False):
    checks = NV_CHECKSUM.findall(text)
    expected = [(pc, length, (~((255 * length) & 0xffff)) & 0xffffffff,
                 (~((255 * length) & 0xffff)) & 0xffffffff if checksum_only else 0xffffffff)
                for _, length, _, pc in NV_BLOCKS]
    if [(int(pc, 16), int(n), int(want, 16), int(stored, 16))
            for pc, n, want, stored, _ in checks] != expected:
        raise ValueError('NSE-2 erased NV checksum observations differ')
    times = [float(row[4]) for row in checks]
    if times != sorted(set(times)) or not 0 < times[0] < times[-1] < 1:
        raise ValueError('NSE-2 NV checksum observation order differs')
    verdict = '00' if checksum_only else 'fd'
    verdicts = re.findall(r'nse2_nv_verdict: address=0010fcb2 value=' + verdict + r' pc=0027dc14 '
                         r't=([0-9.]+) observer_writes=0', text)
    if len(verdicts) != 1 or not times[-1] < float(verdicts[0]) < 1:
        raise ValueError('NSE-2 NV checksum self-test failure verdict missing or unordered')
    return len(checks)


def verify_record(text):
    results = re.findall(r'nse2_selftest_write: address=0010fcac value=([0-9a-f]{2}) '
                         r'pc=(0023340a|00233470) t=([0-9.]+) observer_writes=0', text)
    if len(results) != 1 or results[0][:2] != ('00', '0023340a'):
        raise ValueError('NSE-2 record checksum did not clear its own result')


def verify_upload(text, payload, *, native_response=False):
    if len(payload) != 400 or payload[372:] != bytes(28):
        raise ValueError('NSE-2 own rounded verifier extent or padding differs')
    rows = re.findall(r'nse2_upload_word: n=(\d+) address=([0-9a-f]{8}) data=([0-9a-f]{4}) '
                      r'mask=([0-9a-f]{4}) pc=([0-9a-f]{8}) t=([0-9.]+) observer_writes=0', text)
    if [int(row[0]) for row in rows] != list(range(1, len(rows) + 1)) or len(rows) >= 4096:
        raise ValueError('NSE-2 upload trace incomplete or capped')
    copies = [row for row in rows if row[4] == '0029acc4']
    expected = [(0x10e00 + i * 2, int.from_bytes(payload[i * 2:i * 2 + 2], 'big'), 'ffff')
                for i in range(200)]
    if [(int(row[1], 16), int(row[2], 16), row[3]) for row in copies] != expected:
        raise ValueError('NSE-2 runtime upload differs from own verifier and padding')
    control = [row for row in rows if 0x2781b0 <= int(row[4], 16) <= 0x2781d6]
    expected_control = [(0x100f6 + i * 2, value, 'ffff', pc) for i, (value, pc) in enumerate(
        ((0x100, 0x2781b0), (0x300, 0x2781b8), (1, 0x2781c2), (0, 0x2781c6),
         (1, 0x2781ca), (1, 0x2781ce), (0x200, 0x2781d6)))]
    if [(int(row[1], 16), int(row[2], 16), row[3], int(row[4], 16))
            for row in control] != expected_control:
        raise ValueError('NSE-2 startup control publication differs')
    polls = re.findall(r'nse2_startup_poll: n=(\d+) address=00010004 data=([0-9a-f]{4}) mask=ffff '
                       r'pc=(002781fa|0027821a) t=([0-9.]+) observer_writes=0', text)
    response = '0004' if native_response else 'ffff'
    if (len(polls) < (1 if native_response else 2) or len(polls) >= 64
            or [int(row[0]) for row in polls] != list(range(1, len(polls) + 1))
            or any(row[1] != response for row in polls)):
        raise ValueError('NSE-2 actual startup sentinel polls missing or changed')
    times = [float(row[5]) for row in copies + control] + [float(row[3]) for row in polls]
    if times != sorted(times) or not 0 < times[0] < times[-1] < 0.01:
        raise ValueError('NSE-2 upload/control/poll order differs')
    return dict(verifier_words=186, rounded_upload_words=200, startup_poll_reads=len(polls),
                native_execution=False, completed_ownership_exchange=False)


def verify(text, pixels, size, *, checksum_only=False):
    blocks = verify_nv(text, checksum_only=checksum_only)
    loader = re.findall(r'nse2_state_byte: address=(0010b9f[45]) value=([0-9a-f]{2}) '
                        r'pc=([0-9a-f]{8}) t=([0-9.]+) observer_writes=0', text)
    expected_loader = [('0010b9f4', '00', '0020010c'), ('0010b9f5', '00', '0020010c'),
                       ('0010b9f5', '01', '002781f4'), ('0010b9f5', '00', '00278226'),
                       ('0010b9f5', '00', '002782d6'), ('0010b9f4', '00', '002782d8')]
    if [row[:3] for row in loader] != expected_loader:
        raise ValueError('NSE-2 own loader startup failure lifecycle differs')
    times = [float(row[3]) for row in loader]
    if times != sorted(times) or not 0 < times[0] < times[-1] < 1:
        raise ValueError('NSE-2 loader failure observation order differs')
    if text.count('Soft reset\n') != 1:
        raise ValueError('NSE-2 missing initial reset or reset repeated')
    if re.search(r'RX enqueue|peer RAM W off=00[0246]\b|bootstrap.*complete', text):
        raise ValueError('NSE-2 silent-DSP boundary received a peer completion')
    expected = [(0x10000 + 2 * i, value, 0x27dca8)
                for i, value in enumerate((0xea, 0xff, 0xff, 0xea))]
    expected += [(0x10000, 0, 0x27817a), (0x10002, 0xffff, 0x278182),
                 (0x10004, 0xffff, 0x278184), (0x10006, 0xffff, 0x278188)]
    writes = SHARED.findall(text)
    if [(int(a, 16), int(v, 16), int(pc, 16)) for a, v, pc, _ in writes] != expected:
        raise ValueError('NSE-2 own shared-memory initialization differs')
    samples = STATE.findall(text)
    if len(samples) < 7 or float(samples[-1][0]) < 7:
        raise ValueError('NSE-2 passive state horizon missing')
    times = [float(sample[0]) for sample in samples]
    if times != sorted(set(times)):
        raise ValueError('NSE-2 state samples duplicate or regress')
    for sample in samples:
        if not (0x200040 <= int(sample[1], 16) < 0x390000
                and 0x100000 <= int(sample[3], 16) < 0x110000):
            raise ValueError('NSE-2 execution/stack outside bounded own-image window')
        if int(sample[4]) != 8:
            raise ValueError('NSE-2 shared-memory lifecycle changed after initialization')
    last = [int(value) for value in samples[-1][4:]]
    if last[1] < 100 or last[2] < 40 or last[3] < 8 or last[4] < 504 or last[5] < 100:
        raise ValueError('NSE-2 CCONT/LCD/EEPROM bus activity missing')
    if size != (84, 48) or hashlib.sha256(pixels).hexdigest() != FRAME:
        raise ValueError('NSE-2 reviewed CONTACT SERVICE frame differs')
    return dict(graphical_failure_frame=True, usable_boot=False, native_dsp=False,
                physical_key_delivery=False, samples=len(samples),
                loader_startup_failed=True,
                gensio_tx=last[1], gensio_rx=last[2], lcd_bytes=last[4],
                invalid_nv_checksum_blocks=0 if checksum_only else blocks)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('frame', type=Path)
    args = parser.parse_args()
    try:
        with Image.open(args.frame) as frame:
            result = verify(args.log.read_text(errors='replace'), frame.convert('L').tobytes(), frame.size)
        print(json.dumps(result, indent=2))
    except (OSError, ValueError) as error:
        parser.exit(1, f'NSE-2 boundary FAIL: {error}\n')


if __name__ == '__main__':
    main()

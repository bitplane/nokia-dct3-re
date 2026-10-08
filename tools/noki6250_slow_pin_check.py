"""Require late physical PIN verification, delivered measurement and registration."""
import argparse
from pathlib import Path
import re
import sys

if __package__ in (None, ''):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.noki6250_coherent_registration_check import verify as registration
from tools.noki6250_measurement_delivery_check import verify as delivery


def verify(text, storage, *, require_host=True):
    registration(text, storage, require_host=require_host)
    result = delivery(text)
    keys = re.findall(r'6250_pin_physical: column=(\d+) row=(\d+) t=([0-9.]+)', text)
    if [(int(c), int(r)) for c, r, _ in keys] != [(2, 1), (3, 1), (4, 1), (2, 2), (1, 1)]:
        raise ValueError('missing physical PIN 1234/Confirm sequence')
    times = [float(t) for _, _, t in keys]
    if times[0] < 8 or any(b-a < 0.99 for a, b in zip(times, times[1:])):
        raise ValueError('PIN fixture did not exercise slow entry')
    header = re.search(r'header cla=a0 ins=20 p1=00 p2=01 p3=08 .*?t=([0-9.]+)', text)
    if header is None or float(header[1]) <= result['routed']:
        raise ValueError('PIN verification did not follow the background measurement')
    if 'SIM status ins=20 sw=9000 chv=3/3 puk=10/10 enabled=1' not in text:
        raise ValueError('CHV1 verification failed or card was not PIN-enabled')
    completion = re.search(
        rf'6250_pin_rssi_completion: caller=002d2141 message={result["message"]:08x} t=([0-9.]+)', text)
    if completion is None or float(completion[1]) < result['routed']:
        raise ValueError('delivered envelope did not reach the mapped completion consumer')
    registered = re.search(r'network registered=1 arfcn=19 t=([0-9.]+)', text) if require_host else re.search(
        r'RX enqueue type=80 payload=34[^\n]*data=8012[0-9a-f]{8}00130000030045050200f1100001[0-9a-f]* t=([0-9.]+)', text)
    if registered is None or float(registered[1]) <= float(header[1]):
        raise ValueError('registration did not follow late PIN verification')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path)
    parser.add_argument('storage', type=Path)
    args = parser.parse_args()
    try:
        verify(args.log.read_text(errors='replace'), args.storage.read_bytes())
    except (OSError, ValueError) as error:
        parser.exit(1, f'6250 slow PIN FAIL: {error}\n')
    print('6250 slow physical PIN, delivered background measurement and coherent registration PASS; native speech unproved')


if __name__ == '__main__':
    main()

import pathlib
import re
import sys


EVENT = re.compile(
    r"dsp_tone: oscillator=(?P<osc1>[0-9a-f]{4})/(?P<osc2>[0-9a-f]{4}) "
    r"frequency=(?P<freq1>\d+)/(?P<freq2>\d+) amplitude=(?P<amplitude>[0-9a-f]{4}) "
    r"active=(?P<active1>[01])/(?P<active2>[01])"
)


def check(text: str) -> list[dict[str, str]]:
    events = [match.groupdict() for match in EVENT.finditer(text)]
    if not any(event["osc1"] == "0e10" and event["freq1"] == "900"
               and event["amplitude"] == "65ac" and event["active1"] == "1"
               for event in events):
        raise ValueError("missing organic 900 Hz DSP tone start")
    if not any(event["osc1"] == "0000" and event["active1"] == "0"
               for event in events):
        raise ValueError("missing organic DSP tone stop")
    return events


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: dsp_tone_trace_check.py MAME_ERROR_LOG")
    try:
        check(pathlib.Path(sys.argv[1]).read_text())
    except ValueError as error:
        raise SystemExit(str(error)) from error
    print("OK - organic ROM-4 DSP tone start and stop reproduced")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

"""Check the mapped-MMIO, active-peripheral reset fixture (not pin timing)."""
import argparse
from pathlib import Path

PREPARED = "active_reset: prepared uif=5a sim_tx=1 sim_rx=1 mbus_tx=1"
PASS = "active_reset: PASS sim_tx_abort=1 sim_rx_abort=1 mbus_abort=1 uif_clear=1 gensio_clear=1 ccont_retained=1 cause=04 physical_timing_claim=0"


def check(text, console=""):
    lines = [line.removeprefix("[luaengine] ") for line in text.splitlines()]
    if (lines.count(PREPARED) != 1 or lines.count(PASS) != 1
            or lines.index(PREPARED) >= lines.index(PASS)
            or "[LUA ERROR]" in console or "Fatal error:" in console):
        raise ValueError("missing ordered active-reset preparation/completion")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("console", type=Path)
    args = parser.parse_args()
    try:
        check(args.log.read_text(), args.console.read_text())
    except (OSError, ValueError) as error:
        parser.exit(1, f"active reset FAIL: {error}\n")
    print(PASS)


if __name__ == "__main__":
    main()

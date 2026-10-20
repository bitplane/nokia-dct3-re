"""Exercise actual SIM NVRAM loading, including append-only compatibility."""
import argparse
import json
from pathlib import Path
import subprocess


def cases(default):
    if len(default) != 3570:
        raise ValueError("unexpected SIM storage layout")
    changed = bytearray(default)
    changed[0] = 0x41
    changed[3520] = 1
    changed[3524] = 0x13
    changed[3548] = 0x33
    changed[3560:3562] = b'\x42\xfe'
    changed[3562:3570] = bytes(range(8))
    changed = bytes(changed)
    extended = changed + b'FDN\x01\x01\x00\x00\x00' + bytes([0x41]) * 320
    return {
        "fdn_extended": (extended, extended, False),
        "fdn_short": (extended[:-1], default, True),
        "fdn_trailing": (extended+b'\x00', default, True),
        "fdn_bad_version": (extended[:3573]+b'\x02'+extended[3574:], default, True),
        "fdn_bad_capability": (extended[:3574]+b'\x02'+extended[3575:], default, True),
        "fdn_bad_validity": (extended[:3575]+b'\x02'+extended[3576:], default, True),
        "fdn_bad_access": (extended[:3576]+b'\x02'+extended[3577:], default, True),
        "fdn_bad_reserved": (extended[:3577]+b'\x01'+extended[3578:], default, True),
        "current": (changed, changed, False),
        "legacy_files": (changed[:1636], changed[:1636] + default[1636:], False),
        "legacy_sms": (changed[:3484], changed[:3484] + default[3484:], False),
        "legacy_security": (changed[:3521], changed[:3521] + default[3521:], False),
        "legacy_acm": (changed[:3524], changed[:3524] + default[3524:], False),
        "legacy_plmn": (changed[:3560], changed[:3560] + default[3560:], False),
        "legacy_smss": (changed[:3562], changed[:3562] + default[3562:], False),
        "short_adn": (changed[:100], default, True),
        "short_sms": (changed[:2000], default, True),
        "short_security": (changed[:3500], default, True),
        "short_acm": (changed[:3523], default, True),
        "short_preferred_plmn": (changed[:3530], default, True),
        "short_forbidden_plmn": (changed[:3550], default, True),
        "short_smss": (changed[:3561], default, True),
        "short_acm_max": (changed[:3564], default, True),
        "short_puct": (changed[:3569], default, True),
        "trailing": (changed + b"\x00", default, True),
        "invalid_enabled": (changed[:3520] + b"\x02" + changed[3521:], default, True),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run_directory", type=Path)
    parser.add_argument("--mame", type=Path)
    parser.add_argument("--card-seed", type=Path,
                        help="also cold-load an independently produced current card record")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    run = args.run_directory.resolve()
    try:
        run.mkdir(parents=True, exist_ok=False)
        command = [str((args.mame or root / "mame/mame").resolve()), "noki3210",
                   "-rompath", str(root / "roms"), "-nvram_directory", "nvram",
                   "-cfg_directory", "cfg", "-noreadconfig", "-video", "none",
                   "-sound", "none", "-nothrottle", "-seconds_to_run", "1"]

        def execute(directory, payload=None):
            card = directory / "nvram/noki3210/sim_card"
            card.parent.mkdir(parents=True)
            if payload is not None:
                card.write_bytes(payload)
            with (directory / "console.log").open("w") as output:
                subprocess.run(command, cwd=directory, stdout=output,
                               stderr=subprocess.STDOUT, check=True, timeout=90)
            return card.read_bytes(), (directory / "console.log").read_text()

        default, _ = execute(run / "default")
        fixtures = cases(default)
        if args.card_seed:
            seed = args.card_seed.read_bytes()
            if len(seed) != len(default):
                raise ValueError("card seed does not use the current storage layout")
            fixtures["serial_card"] = (seed, seed, False)
        for name, (payload, expected, rejected) in fixtures.items():
            actual, console = execute(run / name, payload)
            if actual != expected:
                raise ValueError(f"{name}: partial or incorrect card state survived loading")
            if ("Error reading NVRAM file" in console) != rejected:
                raise ValueError(f"{name}: incorrect loader acceptance")
        (run / "acceptance.json").write_text(json.dumps({
            "result": "pass", "command": command, "cases": list(fixtures),
            "fixture_class": "storage conformance", "handset_service_claim": False,
        }, indent=2) + "\n")
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f"SIM NVRAM atomicity FAIL: {error}; inspect {run}\n")
    print(f"SIM NVRAM atomicity PASS cases={len(fixtures)} handset_service_claim=0")


if __name__ == "__main__":
    main()

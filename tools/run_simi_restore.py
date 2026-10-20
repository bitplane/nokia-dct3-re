"""Run isolated SIMI/card restoration and APDU-header conformance checks."""
import argparse
import json
from pathlib import Path
import subprocess

PASS = "SIMI restoration: PASS checkpoints=3 partial_apdu=1 pending_rx=1 security_attempts=1 exact_irq_order=1 exact_deadlines=1 handset_claim=0"
HEADER_PASS = "SIM card header conformance: PASS unsupported_classes=3 empty_verify=1 retry_preservation=1 following_apdu=1 handset_claim=0"
PARAMETER_PASS = "SIM card parameter conformance: PASS reserved_parameters=6 select_lengths=3 state_preservation=1 following_apdu=1 handset_claim=0"
RECORD_PASS = "SIM record addressing: PASS cases=11 linear_no_wrap=1 absolute_preserves_pointer=1 current_record=1 mode_validation=1 handset_claim=0"
UPDATE_PASS = "SIM record updates: PASS cases=30 linear_mutation=1 failure_preservation=1 state_restore=1 cyclic_previous_only=1 handset_claim=0"
BINARY_PASS = "SIM binary offsets: PASS cases=14 full_high_byte=1 failed_write_preserves_data=1 boundary_readback=1 handset_claim=0"
AUTH_PASS = "SIM authentication parameters: PASS invalid_parameters=2 invalid_lengths=3 following_authentication=1 laboratory_algorithm=1 handset_claim=0"
RESPONSE_PASS = "SIM response sequencing: PASS initial_mf=1 activation_replay=1 cases=10 consumed_once=1 interrupted_pair=1 technical_error=1 handset_claim=0"
SEARCH_PASS = "SIM record search: PASS directions=4 types=2 undefined_starts=2 prefix_only=1 failed_pointer_preservation=1 parameter_cases=6 incompatible_files=2 no_selected_ef=3 pending_result_replay=1 handset_claim=0"


ACCESS_PASS = "SIM record read access: PASS protected_files=4 before_pin=1 wrong_pin=1 after_pin=1 seek_access=1 restored_unverified=1 fcp_matches=1 handset_claim=0"
IMSI_PASS = "SIM IMSI read access: PASS before_pin=1 after_pin=1 restored_unverified=1 reset_revokes=1 removal_revokes=1 fcp_matches=1 handset_claim=0"
BINARY_ACCESS_PASS = "SIM GSM binary read access: PASS files=9 before_pin=1 after_pin=1 fcp_matches=1 handset_claim=0"
AUTH_ACCESS_PASS = "SIM authentication access: PASS before_pin=1 after_pin=1 gsm_directory=1 rejected_result_absent=1 handset_claim=0"
BLOCKED_DISABLED_PASS = "SIM disabled CHV blocking: PASS failed_enable=3 protected_read=1 protected_write=1 authentication=1 always_read=1 unblock_recovery=1 failed_write_preservation=1 handset_claim=0"
PLMN_PASS = "SIM PLMN updates: PASS files=2 pin_access=1 readback=1 bounds_preserve=1 state_restore=1 fcp_matches=1 handset_claim=0"
PUCT_PASS = "SIM PUCT read access: PASS before_pin=1 after_pin=1 restored_unverified=1 fcp_matches=1 handset_claim=0"
SMSS_PASS = "SIM SMS status: PASS telecom_parent=1 pin_access=1 readback=1 bounds_preserve=1 state_restore=1 fcp_matches=1 handset_claim=0"
ADM_PASS = "SIM administrative updates: PASS files=4 fcp_adm=1 verified_pin_denied=1 data_preserved=1 handset_claim=0"
AOC_PASS = "SIM AoC binary updates: PASS files=2 blocked_pin_denied=1 readback=1 bounds_preserve=1 state_restore=1 fcp_matches=1 handset_claim=0"
DCS_PASS = "SIM DCS directory alias: PASS visible_fid=7f21 shared_application=1 selection_restore=1 authentication=1 telecom_isolation=1 handset_claim=0"


def check(text, returncode):
    if returncode != 0 or any(text.splitlines().count(marker) != 1
                              for marker in (PASS, HEADER_PASS, PARAMETER_PASS, RECORD_PASS, UPDATE_PASS, BINARY_PASS, AUTH_PASS, RESPONSE_PASS, SEARCH_PASS, ACCESS_PASS, IMSI_PASS, BINARY_ACCESS_PASS, AUTH_ACCESS_PASS, BLOCKED_DISABLED_PASS, PLMN_PASS, PUCT_PASS, SMSS_PASS, ADM_PASS, AOC_PASS, DCS_PASS)):
        raise ValueError("missing unique completed restoration or APDU-header result")
    if "Fatal error:" in text or "Error reading NVRAM" in text:
        raise ValueError("runtime failure or invalid initial storage")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run_directory", type=Path)
    parser.add_argument("--mame", type=Path, required=True)
    args = parser.parse_args()
    run = args.run_directory.resolve()
    try:
        run.mkdir(parents=True, exist_ok=False)
        command = [str(args.mame.resolve()), "simitest", "-noreadconfig",
                   "-nvram_directory", "nvram", "-cfg_directory", "cfg",
                   "-video", "none", "-sound", "none", "-nothrottle",
                   "-seconds_to_run", "24"]
        with (run / "console.log").open("w") as output:
            result = subprocess.run(command, cwd=run, stdout=output,
                                    stderr=subprocess.STDOUT, timeout=90)
        check((run / "console.log").read_text(), result.returncode)
        storage = (run / "nvram/simitest/card").read_bytes()
        if (len(storage) != 3570 or storage[3524:3548] != b"\x11" + b"\x22" * 23
                or storage[3548:3560] != b"\x33" + b"\x44" * 11
                or storage[3560:3562] != b"\x42\xfe"
                or storage[3562:] != bytes.fromhex('1122223344444444')):
            raise ValueError("serial PLMN, SMSS or AoC updates were not serialized exactly")
        (run / "acceptance.json").write_text(json.dumps({
            "result": "pass", "command": command, "checkpoints": 11,
            "plmn_update_files": 2, "plmn_update_restore": True,
            "plmn_serialized_exactly": True,
            "protected_record_files": 4, "read_access_restore": True,
            "imsi_read_access": True,
            "puct_read_access": True, "puct_unverified_restore": True,
            "smss_access_restore": True, "smss_serialized_exactly": True,
            "aoc_binary_update_files": 2, "aoc_update_restore": True,
            "aoc_serialized_exactly": True,
            "administrative_update_files": 4, "verified_pin_not_adm": True,
            "dcs_directory_alias":True,"dcs_directory_restore":True,
            "additional_protected_binary_files": 9,
            "authentication_access": True, "authentication_directory": True,
            "disabled_chv_blocking": True, "unblock_recovery": True,
            "reset_revokes_authorization": True,
            "removal_revokes_authorization": True,
            "controller_checkpoints": 3, "record_update_checkpoints": 1,
            "unsupported_classes": 3, "empty_verify": True,
            "retry_preservation": True, "following_apdu": True,
            "reserved_parameters": 6, "select_lengths": 3,
            "record_addressing_cases": 11,
            "record_update_cases": 30, "record_update_restore": True,
            "binary_offset_cases": 14, "full_binary_offset": True,
            "authentication_parameter_cases": 2, "authentication_length_cases": 3,
            "following_laboratory_authentication": True,
            "initial_mf_response": True, "activation_response_replay": True,
            "response_sequence_cases": 10,
            "seek_directions": 4, "seek_types": 2,
            "seek_pending_result_replay": True,
            "fixture_class": "controller/card conformance", "handset_claim": False,
        }, indent=2) + "\n")
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f"SIMI restoration FAIL: {error}; inspect {run}\n")
    print(PASS)
    print(HEADER_PASS)
    print(PARAMETER_PASS)
    print(RECORD_PASS)
    print(UPDATE_PASS)
    print(BINARY_PASS)
    print(AUTH_PASS)
    print(RESPONSE_PASS)
    print(SEARCH_PASS)
    print(ACCESS_PASS)
    print(IMSI_PASS)
    print(BINARY_ACCESS_PASS)
    print(AUTH_ACCESS_PASS)
    print(BLOCKED_DISABLED_PASS)
    print(PLMN_PASS)
    print(PUCT_PASS)
    print(SMSS_PASS)
    print(ADM_PASS)
    print(AOC_PASS)
    print(DCS_PASS)


if __name__ == "__main__":
    main()

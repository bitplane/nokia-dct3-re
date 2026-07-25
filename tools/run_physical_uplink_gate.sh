#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"

run_dir=${RUN_DIR:-run_physical_uplink}
bios=${BIOS:-}
rom=${ROM:-roms/3210f600a.fls}
eeprom_basename=${EEPROM_BASENAME:-3210 v600 eeprom.bin}
sink_name="nokia_dct3_uplink_$$"
module_id=
source_pid=

cleanup()
{
	if [[ -n "$source_pid" ]]; then
		kill "$source_pid" >/dev/null 2>&1 || true
		wait "$source_pid" >/dev/null 2>&1 || true
	fi
	if [[ -n "$module_id" ]]; then
		pactl unload-module "$module_id" >/dev/null 2>&1 || true
	fi
	make --no-print-directory eeprom-profile BIOS="$bios" ROM="$rom"
	cp "roms/noki3210/$eeprom_basename" \
		"mame/roms/noki3210/$eeprom_basename"
}
trap cleanup EXIT

for command in pactl ffmpeg; do
	command -v "$command" >/dev/null ||
		{ echo "Missing host-audio command: $command" >&2; exit 1; }
done

module_id=$(pactl load-module module-null-sink \
	"sink_name=$sink_name" \
	"sink_properties=device.description=NokiaDCT3PhysicalUplink")
ffmpeg -hide_banner -loglevel error -re \
	-f lavfi -i sine=frequency=1000:sample_rate=48000 \
	-filter:a volume=0.02 -f pulse "$sink_name" &
source_pid=$!

make --no-print-directory run JOBS=4 PHONE=noki3210 BIOS="$bios" ROM="$rom" \
	RUN_DIR="$run_dir" SECONDS=45 ERASED_IDENTITY_SECURITY_CODE=12345 \
	RUN_VERBOSE=1 \
	RUN_EXTRA_ARGS="-cfg_directory ../fixtures/radio_incoming_call_answered -sound pulse" \
	RUN_ENV="PULSE_SOURCE=$sink_name.monitor NOKIA_DCT3_POST_READY_KEYS=1,2,3,4,5,enter,wait500,waitbuzzer,enter NOKIA_DCT3_POST_READY_KEY_DELAY_MS=12000 NOKIA_DCT3_POST_READY_KEY_DURATION_MS=220 NOKIA_DCT3_POST_READY_KEY_GAP_MS=280"

cp mame/error.log "$run_dir/error.log"
python3 tools/radio_physical_uplink_trace_check.py "$run_dir/error.log"
python3 tools/radio_speech_media_trace_check.py "$run_dir/error.log"

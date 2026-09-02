#!/usr/bin/env bash
set -euo pipefail

root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"

run_dir=${RUN_DIR:-run_physical_uplink}
phone=${PHONE:-noki3210}
bios=${BIOS:-}
rom=${ROM:-roms/3210f600a.fls}
eeprom_basename=${EEPROM_BASENAME:-3210 v600 eeprom.bin}
audio_control_checker=${AUDIO_CONTROL_CHECKER:-tools/radio_answered_call_lifecycle_trace_check.py}
fixture=${FIXTURE:-fixtures/radio_incoming_call_answered}
run_seconds=${RUN_SECONDS:-38}
post_ready_keys=${POST_READY_KEYS:-1,2,3,4,5,enter,wait500,waitbuzzer,enter,wait5000,enter}
post_ready_delay_ms=${POST_READY_DELAY_MS:-12000}
post_ready_duration_ms=${POST_READY_DURATION_MS:-220}
post_ready_gap_ms=${POST_READY_GAP_MS:-280}
read -r -a pcm_check_args <<< "${PCM_CHECK_ARGS:-}"
host_media_port=${HOST_MEDIA_PORT:-}
input_sink_name="nokia_dct3_uplink_$$"
output_sink_name="nokia_dct3_downlink_$$"
input_module_id=
output_module_id=
source_pid=
capture_pid=
router_pid=
mame_pid=
default_sink=
default_sink_changed=false
default_source=
default_source_changed=false

cleanup()
{
	if [[ -n "$mame_pid" ]]; then
		kill "$mame_pid" >/dev/null 2>&1 || true
		wait "$mame_pid" >/dev/null 2>&1 || true
	fi
	if [[ -n "$router_pid" ]]; then
		kill "$router_pid" >/dev/null 2>&1 || true
		wait "$router_pid" >/dev/null 2>&1 || true
	fi
	if [[ -n "$capture_pid" ]]; then
		kill -INT "$capture_pid" >/dev/null 2>&1 || true
		wait "$capture_pid" >/dev/null 2>&1 || true
	fi
	if [[ -n "$source_pid" ]]; then
		kill "$source_pid" >/dev/null 2>&1 || true
		wait "$source_pid" >/dev/null 2>&1 || true
	fi
	if [[ "$default_source_changed" == true && -n "$default_source" ]]; then
		pactl set-default-source "$default_source" >/dev/null 2>&1 || true
		default_source_changed=false
	fi
	if [[ "$default_sink_changed" == true && -n "$default_sink" ]]; then
		pactl set-default-sink "$default_sink" >/dev/null 2>&1 || true
		default_sink_changed=false
	fi
	if [[ -n "$output_module_id" ]]; then
		pactl unload-module "$output_module_id" >/dev/null 2>&1 || true
	fi
	if [[ -n "$input_module_id" ]]; then
		pactl unload-module "$input_module_id" >/dev/null 2>&1 || true
	fi
	if [[ "$phone" == noki3210 ]]; then
		make --no-print-directory eeprom-profile BIOS="$bios" ROM="$rom"
		cp "roms/noki3210/$eeprom_basename" \
			"mame/roms/noki3210/$eeprom_basename"
	fi
}
trap cleanup EXIT

for command in pactl ffmpeg; do
	command -v "$command" >/dev/null ||
		{ echo "Missing host-audio command: $command" >&2; exit 1; }
done

if [[ -n "$host_media_port" ]]; then
	make --no-print-directory build JOBS=4 \
		ERASED_IDENTITY_SECURITY_CODE=12345
	make --no-print-directory prepare-run-nvram \
		PHONE="$phone" BIOS="$bios" ROM="$rom" RUN_DIR="$run_dir" \
		ERASED_IDENTITY_SECURITY_CODE=12345
fi

input_module_id=$(pactl load-module module-null-sink \
	"sink_name=$input_sink_name" \
	"sink_properties=device.description=NokiaDCT3PhysicalUplink")
output_module_id=$(pactl load-module module-null-sink \
	"sink_name=$output_sink_name" \
	"sink_properties=device.description=NokiaDCT3PhysicalDownlink")
# SDL's PulseAudio backend selects the server defaults rather than honoring
# PULSE_SOURCE/PULSE_SINK. Preserve and temporarily replace both before MAME
# opens its streams; existing host streams are not moved.
default_sink=$(pactl get-default-sink)
default_source=$(pactl get-default-source)
pactl set-default-sink "$output_sink_name"
default_sink_changed=true
pactl set-default-source "$input_sink_name.monitor"
default_source_changed=true
mkdir -p "$run_dir"
ffmpeg -hide_banner -loglevel error -re \
	-f lavfi -i sine=frequency=1000:sample_rate=48000 \
	-filter:a volume=0.02 -device "$input_sink_name" -f pulse - &
source_pid=$!
ffmpeg -y -hide_banner -loglevel error \
	-f pulse -i "$output_sink_name.monitor" \
	-ac 1 -ar 8000 -c:a pcm_s16le "$run_dir/downlink.wav" &
capture_pid=$!
python3 tools/pulse_route_mame.py \
	--source "$input_sink_name.monitor" --sink "$output_sink_name" \
	> "$run_dir/pulse_routes.log" &
router_pid=$!

if [[ -n "$host_media_port" ]]; then
	bios_args=()
	if [[ -n "$bios" ]]; then
		bios_args=(-bios "$bios")
	fi
	( cd mame && env PULSE_SOURCE="$input_sink_name.monitor" \
		NOKIA_DCT3_LUA_QUIET=1 \
		NOKIA_DCT3_POST_READY_KEYS="$post_ready_keys" \
		NOKIA_DCT3_POST_READY_KEY_DELAY_MS="$post_ready_delay_ms" \
		NOKIA_DCT3_POST_READY_KEY_DURATION_MS="$post_ready_duration_ms" \
		NOKIA_DCT3_POST_READY_KEY_GAP_MS="$post_ready_gap_ms" \
		NOKIA_DCT3_SNAPSHOT_DIR="$(realpath "$run_dir")" \
		NOKIA_DCT3_BOOT_SUMMARY="$(realpath "$run_dir")/boot_summary.txt" \
		./mame "$phone" -rompath roms -log -video none -sound pulse \
			-keyboardprovider none -mouseprovider none -lightgunprovider none \
			-joystickprovider none -midiprovider none -skip_gameinfo -throttle \
			-autoboot_script ../mame_nokia_dct3_input_exerciser.lua \
			"${bios_args[@]}" -verbose \
			-cfg_directory ../fixtures/radio_outgoing_host_adapter \
			-http -http_port "$host_media_port" \
			-nvram_directory "$(realpath "$run_dir")/nvram" \
			-seconds_to_run "$run_seconds" ) &
	mame_pid=$!
	python3 tools/dct3_call_bridge.py \
		--url "ws://127.0.0.1:$host_media_port/nokia/dct3/calls" \
		--hangup-after 5 --once --require-frames 200 --report-every 100
	wait "$mame_pid"
	mame_pid=
else
	make --no-print-directory run JOBS=4 PHONE="$phone" BIOS="$bios" ROM="$rom" \
		RUN_DIR="$run_dir" SECONDS="$run_seconds" \
		ERASED_IDENTITY_SECURITY_CODE=12345 RUN_VERBOSE=1 \
		RUN_EXTRA_ARGS="-cfg_directory ../$fixture -sound pulse -throttle" \
		RUN_ENV="PULSE_SOURCE=$input_sink_name.monitor NOKIA_DCT3_POST_READY_KEYS=$post_ready_keys NOKIA_DCT3_POST_READY_KEY_DELAY_MS=$post_ready_delay_ms NOKIA_DCT3_POST_READY_KEY_DURATION_MS=$post_ready_duration_ms NOKIA_DCT3_POST_READY_KEY_GAP_MS=$post_ready_gap_ms"
fi

cp mame/error.log "$run_dir/error.log"
kill "$router_pid" >/dev/null 2>&1 || true
wait "$router_pid" >/dev/null 2>&1 || true
router_pid=
grep -q '^pulse_route: source-output ' "$run_dir/pulse_routes.log"
grep -q '^pulse_route: sink-input ' "$run_dir/pulse_routes.log"
kill -INT "$capture_pid" >/dev/null 2>&1 || true
wait "$capture_pid" >/dev/null 2>&1 || true
capture_pid=
if [[ -n "$host_media_port" ]]; then
	python3 tools/radio_outgoing_host_media_trace_check.py \
		"$run_dir/error.log" --frames 200 --standalone
	python3 tools/radio_physical_uplink_trace_check.py \
		"$run_dir/error.log" --source silence
	# Key and call-control tones can clip briefly outside the speech interval;
	# the checker still requires a sustained decoded 1 kHz run.
	python3 tools/radio_physical_downlink_check.py \
		"$run_dir/downlink.wav" --allow-transient-clipping --non-silent
else
	python3 "$audio_control_checker" "$run_dir/error.log"
	python3 tools/radio_physical_uplink_trace_check.py "$run_dir/error.log"
	python3 tools/radio_physical_downlink_check.py "$run_dir/downlink.wav"
	python3 tools/radio_speech_media_trace_check.py \
		"$run_dir/error.log" "${pcm_check_args[@]}"
	python3 tools/radio_facch_interruption_trace_check.py "$run_dir/error.log"
fi

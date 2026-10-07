#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
run_dir=${RUN_DIR:-run_3210_sip_waveform}
input_name="dct3_sip_microphone_$$"
output_name="dct3_sip_earpiece_$$"
input_module= output_module= source_pid= capture_pid= router_pid=
old_sink= old_source=
cleanup() {
    for pid in "$router_pid" "$source_pid"; do
        if [[ -n "$pid" ]]; then kill "$pid" 2>/dev/null || true; wait "$pid" 2>/dev/null || true; fi
    done
    if [[ -n "$capture_pid" ]]; then kill -INT "$capture_pid" 2>/dev/null || true; wait "$capture_pid" 2>/dev/null || true; fi
    if [[ -n "$old_sink" ]]; then pactl set-default-sink "$old_sink" || true; fi
    if [[ -n "$old_source" ]]; then pactl set-default-source "$old_source" || true; fi
    if [[ -n "$input_module" ]]; then pactl unload-module "$input_module" || true; fi
    if [[ -n "$output_module" ]]; then pactl unload-module "$output_module" || true; fi
}
trap cleanup EXIT
for command in pactl ffmpeg; do command -v "$command" >/dev/null || { echo "missing $command" >&2; exit 1; }; done
mkdir -p "$run_dir"
input_module=$(pactl load-module module-null-sink "sink_name=$input_name")
output_module=$(pactl load-module module-null-sink "sink_name=$output_name")
old_sink=$(pactl get-default-sink)
old_source=$(pactl get-default-source)
# SDL/Pulse opens server defaults; restore these even when the gate fails.
pactl set-default-sink "$output_name"
pactl set-default-source "$input_name.monitor"
python3 tools/prepare_physical_audio_config.py fixtures/radio_outgoing_host_adapter "$run_dir/audio_cfg"
ffmpeg -hide_banner -loglevel error -re -f lavfi -i sine=frequency=440:sample_rate=48000 \
    -filter:a volume=0.02 -device "$input_name" -f pulse - &
source_pid=$!
ffmpeg -y -hide_banner -loglevel error -f pulse -i "$output_name.monitor" \
    -ac 1 -ar 8000 -c:a pcm_s16le "$run_dir/sip-earpiece.wav" &
capture_pid=$!
python3 tools/pulse_route_mame.py --source "$input_name.monitor" --sink "$output_name" \
    > "$run_dir/sip-pulse-routes.log" &
router_pid=$!
make --no-print-directory verify-radio-outgoing-call-sip RUN_DIR="$run_dir" JOBS="${JOBS:-8}" \
    SIP_HANDSET_RUNNER_ARGS=--record-media SIP_HANDSET_SOUND=pulse \
    SIP_HANDSET_CONFIG="$(realpath "$run_dir/audio_cfg")"
kill -INT "$capture_pid"
wait "$capture_pid" || true
capture_pid=
grep -q '^pulse_route: source-output ' "$run_dir/sip-pulse-routes.log" || { echo 'missing MAME microphone stream' >&2; exit 1; }
grep -q '^pulse_route: sink-input ' "$run_dir/sip-pulse-routes.log" || { echo 'missing MAME speaker stream' >&2; exit 1; }
.venv/bin/python tools/sip_handset_waveform_check.py "$run_dir"

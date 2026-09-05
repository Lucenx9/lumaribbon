#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail

# A separate D-Bus session and runtime directory keep this test away from the
# desktop server, hardware, WirePlumber state, and application playback.
if [[ ${LUMA_PRIVATE_SESSION:-0} != 1 ]]; then
    export LUMA_PRIVATE_SESSION=1
    exec dbus-run-session -- bash "$0" "$@"
fi
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
build=$(realpath "${1:-$repo/build}")
work=$(mktemp -d -t luma-pipewire-test.XXXXXX)
mkdir -p "$work/runtime" "$work/config" "$work/state" "$work/cache" "$build/evidence"
chmod 700 "$work/runtime"
export XDG_RUNTIME_DIR="$work/runtime" XDG_CONFIG_HOME="$work/config" XDG_STATE_HOME="$work/state" XDG_CACHE_HOME="$work/cache"
export PIPEWIRE_REMOTE=pipewire-0
unset PIPEWIRE_RUNTIME_DIR WIREPLUMBER_CONFIG_DIR PIPEWIRE_CONFIG_DIR
pw_pid= wp_pid= probe_pid= play_pid=
cleanup() {
    for pid in "$play_pid" "$probe_pid" "$wp_pid" "$pw_pid"; do
        if [[ -n $pid ]]; then kill "$pid" 2>/dev/null || true; wait "$pid" 2>/dev/null || true; fi
    done
    cp "$work/probe.log" "$build/evidence/pipewire-integration.log" 2>/dev/null || true
    cp "$work/events.log" "$build/evidence/pipewire-events.log" 2>/dev/null || true
    if [[ ${LUMA_KEEP_TEST_DIR:-0} == 1 ]]; then echo "Test files: $work"; else rm -rf -- "$work"; fi
}
trap cleanup EXIT
wait_for() {
    local description=$1; shift
    for _ in {1..100}; do if "$@"; then return; fi; sleep 0.1; done
    echo "FAIL: $description" >&2
    cat "$work/probe.log" "$work/pipewire.log" "$work/wireplumber.log" 2>/dev/null >&2 || true
    exit 1
}
server_ready() { pw-cli info 0 >/dev/null 2>&1; }
start_server() {
    pipewire >"$work/pipewire.log" 2>&1 & pw_pid=$!
    wait_for "private PipeWire server" server_ready
    # The policy profile has no ALSA, Bluetooth, video or other hardware monitors.
    wireplumber --profile=policy >"$work/wireplumber.log" 2>&1 & wp_pid=$!
    sleep 0.5
}
create_sink() {
    pw-cli create-node adapter "{ factory.name=support.null-audio-sink node.name=$1 node.description=$2 media.class=Audio/Sink audio.position=[FL FR] object.linger=true }" >/dev/null
}
node_id() { pw-dump | jq -er --arg name "$1" '.[] | select(.type == "PipeWire:Interface:Node" and .info.props["node.name"] == $name) | .id'; }
set_default() { wpctl set-default "$(node_id "$1")"; }
latest_matches() { tail -n 1 "$work/probe.log" | grep -Eq "$1"; }
mark() { echo "$1" | tee -a "$work/events.log"; }

"$build/luma-test-audio" "$work/tone.wav"
start_server
create_sink luma-test-a Luma-Test-A
create_sink luma-test-b Luma-Test-B
pw-cli create-node adapter '{ factory.name=support.null-audio-sink node.name=luma-test-mic node.description=Luma-Test-Microphone media.class=Audio/Source audio.position=[FL FR] object.linger=true }' >/dev/null
sleep 0.5
set_default luma-test-a
"$build/luma-audio-probe" --seconds 35 --expect-audio >"$work/probe.log" 2>&1 & probe_pid=$!
pw-play --target=luma-test-a "$work/tone.wav" >"$work/play.log" 2>&1 & play_pid=$!
wait_for "audio through sink A" latest_matches 'energy=0\.[1-9].*device=Luma-Test-A.*connected'
mark "PASS monitor capture on sink A"

set_default luma-test-b
kill "$play_pid"; wait "$play_pid" 2>/dev/null || true
pw-play --target=luma-test-b "$work/tone.wav" >"$work/play.log" 2>&1 & play_pid=$!
wait_for "default output switch to B" latest_matches 'energy=0\.[1-9].*device=Luma-Test-B.*connected'
mark "PASS default output switch to sink B"

kill "$wp_pid"; wait "$wp_pid" 2>/dev/null || true; wp_pid=
wait_for "missing policy metadata becomes a bounded error" latest_matches 'error=1.*No default audio output'
if pw-dump | jq -e '.[] | select(.info.props["node.name"] == "luma-ribbon-monitor")' >/dev/null; then
    echo "FAIL: capture stream retained indefinitely without default metadata" >&2; exit 1
fi
wireplumber --profile=policy >"$work/wireplumber.log" 2>&1 & wp_pid=$!
sleep 0.5
set_default luma-test-b
wait_for "capture after policy metadata returns" latest_matches 'energy=0\.[1-9].*device=Luma-Test-B.*connected'
mark "PASS bounded metadata wait and recovery after private WirePlumber restart"

kill "$play_pid"; wait "$play_pid" 2>/dev/null || true; play_pid=
sleep 4
wait_for "silence fades to zero" latest_matches 'energy=(0\.000[0-9]+|[0-9.]+e-0[5-9]|0) '
mark "PASS silence fade on a suspended sink"

pw-cli destroy "$(node_id luma-test-b)"
pw-cli destroy "$(node_id luma-test-a)"
wait_for "no sink available" latest_matches 'error=1.*No default audio output'
node_id luma-test-mic >/dev/null
mark "PASS only an Audio/Source node remains; no microphone fallback stream"
if pw-dump | jq -e '.[] | select(.info.props["node.name"] == "luma-ribbon-monitor")' >/dev/null; then
    echo "FAIL: capture stream exists without an output" >&2; exit 1
fi

kill "$wp_pid" "$pw_pid"
wait "$wp_pid" 2>/dev/null || true; wait "$pw_pid" 2>/dev/null || true
wp_pid= pw_pid=
wait_for "server disconnect status" latest_matches 'error=1.*(interrupted|unavailable)'
mark "PASS private server disconnection reported"
start_server
create_sink luma-test-c Luma-Test-C
sleep 0.5
set_default luma-test-c
pw-play --target=luma-test-c "$work/tone.wav" >"$work/play.log" 2>&1 & play_pid=$!
wait_for "audio after server restart" latest_matches 'energy=0\.[1-9].*device=Luma-Test-C.*connected'
mark "PASS capture recovered after private PipeWire/WirePlumber restart"

wait "$probe_pid"; probe_pid=
if pw-dump | jq -e '.[] | select(.info.props["node.name"] == "luma-ribbon-monitor")' >/dev/null; then
    echo "FAIL: leaked capture stream" >&2; exit 1
fi
mark "PASS shared engine released; no capture stream remains"

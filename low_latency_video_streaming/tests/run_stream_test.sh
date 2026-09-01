#!/usr/bin/env bash
#
# Regression test for the streaming pipeline. Starts a receiver and a sender,
# lets them run, then asserts on the per-second statistics both of them print.
# Exits non-zero on any regression so this can gate a commit.
#
# Both transports print the same stat line format, so --mode selects which pair
# of binaries to exercise without changing anything else.
#
#   ./tests/run_stream_test.sh                       # UDP/RTP path
#   ./tests/run_stream_test.sh --mode webrtc         # WebRTC path
#   ./tests/run_stream_test.sh --audio raw           # uncompressed audio
#   ./tests/run_stream_test.sh --pki                 # encrypted, with X.509 identities
#   ./tests/run_stream_test.sh --lat-max 20          # relax the video latency gate
#
# Audio is checked in both directions: the receiver sends its own audio back, so
# each binary reports one "Audio sent" and one "Audio recv" line per second when
# asked with --audio-stats, which this script passes.
# Capture is a test tone and playback is fakesink, so the test needs no sound
# card and cannot be thrown off by what a microphone happens to pick up.
#
# A window will open while the test runs: the client renders through a real video
# sink, and measuring the real sink is the point. Do not background this script
# from a shell that then exits - an orphaned sender loses clock sync and streams
# far too fast, which looks like a corruption bug.

set -uo pipefail

MODE="udp"
AUDIO="opus"         # off | raw | opus
PKI=0
DURATION=15          # seconds the sender runs
SETTLE=3             # seconds to let the receiver bind before sending
FPS_MIN="59.0"       # per-second frame rate floor, both ends
LAT_MAX="15.0"       # arrival->sink milliseconds, receiver
# Audio arrival->sink ceiling. Deliberately loose compared with the measured
# 0.1-20 ms: the regression this guards against is the sender backlog that used
# to leave a permanent 241 ms in the receiving jitterbuffer, not a millisecond of
# drift. See PROGRESS.md.
AUDIO_LAT_MAX="40.0"
# Reported seconds to discard at the start. The pipeline needs a few seconds to
# settle: the decoder warms up and the jitterbuffer drains its initial fill, so
# the first seconds measure 12-18 ms against a steady state near 10 ms.
# Discarding them is not the same as relaxing LAT_MAX - steady state is still
# held to the tight number, it just is not judged on startup. Left empty here so
# each transport can pick its own default below; --warmup overrides both.
WARMUP=""
INPUT=""
KEEP_LOGS=0
PKI_DIR=""

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(dirname "$SCRIPT_DIR")"
BUILD="$ROOT/build"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --mode)      MODE="$2"; shift 2 ;;
        --audio)     AUDIO="$2"; shift 2 ;;
        --pki)       PKI=1; shift ;;
        --pki-dir)   PKI_DIR="$2"; PKI=1; shift 2 ;;
        --duration)  DURATION="$2"; shift 2 ;;
        --fps-min)   FPS_MIN="$2"; shift 2 ;;
        --lat-max)   LAT_MAX="$2"; shift 2 ;;
        --audio-lat-max) AUDIO_LAT_MAX="$2"; shift 2 ;;
        --warmup)    WARMUP="$2"; shift 2 ;;
        --input)     INPUT="$2"; shift 2 ;;
        --keep-logs) KEEP_LOGS=1; shift ;;
        -h|--help)
            sed -n '2,24p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
            exit 0 ;;
        *) echo "unknown argument: $1" >&2; exit 2 ;;
    esac
done

case "$MODE" in
    udp)    SERVER_BIN="$BUILD/server";        CLIENT_BIN="$BUILD/client"
            LISTEN_PORT=5000; LISTEN_PROTO=u; DEFAULT_WARMUP=3 ;;
    # WebRTC cannot send a byte until ICE has picked a candidate pair and DTLS has
    # finished, and the receiver then waits for the first keyframe. That start-up
    # was measured at ~56 ms on its first reported second against a steady 12 ms.
    webrtc) SERVER_BIN="$BUILD/webrtc_server"; CLIENT_BIN="$BUILD/webrtc_client"
            LISTEN_PORT=5020; LISTEN_PROTO=t; DEFAULT_WARMUP=5 ;;
    *) echo "--mode must be 'udp' or 'webrtc'" >&2; exit 2 ;;
esac
WARMUP="${WARMUP:-$DEFAULT_WARMUP}"

case "$AUDIO" in
    off)  AUDIO_PKT_MIN=0 ;;
    # 10 ms packets, so 100/s. Raw is split by the MTU and lands near 200/s; the
    # floor only has to be low enough not to be flaky and high enough to notice a
    # stream that has stopped.
    opus) AUDIO_PKT_MIN=95 ;;
    raw)  AUDIO_PKT_MIN=190 ;;
    *) echo "--audio must be 'off', 'raw' or 'opus'" >&2; exit 2 ;;
esac

fail() { echo "FAIL: $*" >&2; exit 1; }

# --- arguments for the pair -------------------------------------------------
SERVER_ARGS=()
CLIENT_ARGS=()
if [[ "$AUDIO" == "off" ]]; then
    SERVER_ARGS+=(--audio off)
    CLIENT_ARGS+=(--audio off)
else
    # A test tone and a null sink: no sound card needed, and nothing that a
    # microphone or a busy speaker can perturb. --audio-stats because the audio
    # figures are quiet by default and they are what the checks below read.
    SERVER_ARGS+=(--audio "$AUDIO" --audio-source test --audio-sink fakesink
                  --audio-stats)
    CLIENT_ARGS+=(--audio "$AUDIO" --audio-source test --audio-sink fakesink
                  --audio-stats)
fi
# fakesink for video would not exercise the real sink, which is the point of the
# video assertions, so the receiver keeps its default renderer.

if [[ "$PKI" -eq 1 ]]; then
    PKI_DIR="${PKI_DIR:-$ROOT/pki}"
    if [[ ! -f "$PKI_DIR/ca.crt" ]]; then
        echo "=== generating a demo PKI in $PKI_DIR ==="
        "$ROOT/tools/make_certs.sh" "$PKI_DIR" > /dev/null \
            || fail "tools/make_certs.sh failed"
    fi
    for f in ca.crt sender.crt sender.key receiver.crt receiver.key; do
        [[ -f "$PKI_DIR/$f" ]] || fail "missing $PKI_DIR/$f (regenerate with tools/make_certs.sh)"
    done
    SERVER_ARGS+=(--pki-ca "$PKI_DIR/ca.crt" --pki-cert "$PKI_DIR/sender.crt"
                  --pki-key "$PKI_DIR/sender.key" --pki-peer-identity stream-receiver)
    CLIENT_ARGS+=(--pki-ca "$PKI_DIR/ca.crt" --pki-cert "$PKI_DIR/receiver.crt"
                  --pki-key "$PKI_DIR/receiver.key" --pki-peer-identity stream-sender)
fi

# Default to any raw file in the project root, so the caller does not have to
# remember the name of a gitignored test asset.
if [[ -z "$INPUT" ]]; then
    INPUT="$(find "$ROOT" -maxdepth 1 -name '*.yuv' -print -quit 2>/dev/null || true)"
fi

[[ -n "$INPUT" && -f "$INPUT" ]] || fail "no .yuv input found in $ROOT.
      Generate one:
        python3 generate_raw_video.py --output test.yuv --width 1920 \\
            --height 1080 --duration 10 --framerate 60 --pattern unique"

CLIENT_LOG="$(mktemp)"
SERVER_LOG="$(mktemp)"
CLIENT_PID=""

cleanup() {
    # Kill the whole process group so no receiver is left holding the port, then
    # wait for it to actually go. A fixed sleep here let the next run start while
    # the old receiver still held the port, which failed as "Address already in
    # use" and looked like a bug in the receiver.
    if [[ -n "$CLIENT_PID" ]] && kill -0 "$CLIENT_PID" 2>/dev/null; then
        kill -TERM "-$CLIENT_PID" 2>/dev/null || kill -TERM "$CLIENT_PID" 2>/dev/null
        for _ in $(seq 20); do
            kill -0 "$CLIENT_PID" 2>/dev/null || break
            sleep 0.25
        done
        kill -KILL "-$CLIENT_PID" 2>/dev/null || true
    fi
    if [[ "$KEEP_LOGS" -eq 1 ]]; then
        echo "logs: $CLIENT_LOG $SERVER_LOG"
    else
        rm -f "$CLIENT_LOG" "$SERVER_LOG"
    fi
}
trap cleanup EXIT

echo "=== build ==="
cmake -S "$ROOT" -B "$BUILD" > /dev/null || fail "cmake failed"
make -C "$BUILD" -j"$(nproc)" 2>&1 | tail -2 || fail "build failed"
[[ -x "$SERVER_BIN" ]] || fail "missing binary: $SERVER_BIN"
[[ -x "$CLIENT_BIN" ]] || fail "missing binary: $CLIENT_BIN"

# Snapshot kernel UDP counters. A receive buffer overflow means the socket could
# not be drained fast enough, which is the failure this project cares most about.
udp_errors() { netstat -su 2>/dev/null | awk '/receive buffer errors/ {print $1; exit}'; }
ERR_BEFORE="$(udp_errors)"; ERR_BEFORE="${ERR_BEFORE:-0}"

# The receiver binds fixed ports, so a previous run still shutting down would make
# this one fail on bind. Wait for them rather than racing.
# Video/signalling first, then the two audio ports and the keying port.
ports_in_use() {
    local proto port
    for proto in u t; do
        for port in 5000 5002 5004 5010 5020; do
            [[ "$proto" == "t" && "$port" != "5010" && "$port" != "5020" ]] && continue
            [[ "$proto" == "u" && ( "$port" == "5010" || "$port" == "5020" ) ]] && continue
            if ss -ln "-${proto}" 2>/dev/null | grep -qE "[:.]${port}[[:space:]]"; then
                echo "$port"
                return 0
            fi
        done
    done
    return 1
}
if held="$(ports_in_use)"; then
    echo "waiting for port $held to be released"
    for _ in $(seq 30); do
        ports_in_use > /dev/null || break
        sleep 1
    done
    held="$(ports_in_use)" && fail "port $held still in use after 30s.
      Another receiver is running: $(ss -lnp 2>/dev/null | grep "$held")"
fi

echo "=== run (mode=$MODE, audio=$AUDIO, pki=$PKI, ${DURATION}s) ==="
# setsid gives the receiver its own process group so cleanup can take the whole
# tree down, and detaches it from this script's terminal.
setsid stdbuf -oL -eL "$CLIENT_BIN" "${CLIENT_ARGS[@]}" > "$CLIENT_LOG" 2>&1 &
CLIENT_PID=$!
sleep "$SETTLE"
kill -0 "$CLIENT_PID" 2>/dev/null || fail "receiver exited early:
$(tail -5 "$CLIENT_LOG")"

# Sender runs in the foreground: it owns the pacing clock and must not be
# orphaned. timeout bounds the run so a hang cannot wedge the suite.
timeout "$DURATION" stdbuf -oL -eL "$SERVER_BIN" "$INPUT" "${SERVER_ARGS[@]}" \
    > "$SERVER_LOG" 2>&1
SERVER_RC=$?
# 124 is timeout's "still running when the clock ran out", which is expected.
[[ "$SERVER_RC" -eq 0 || "$SERVER_RC" -eq 124 ]] \
    || fail "sender exited $SERVER_RC:
$(tail -5 "$SERVER_LOG")"

sleep 2
ERR_AFTER="$(udp_errors)"; ERR_AFTER="${ERR_AFTER:-0}"

# --- assertions -------------------------------------------------------------
# Keep only steady state: drop the warm-up seconds at the front and the final
# partial interval measured after the sender stopped.
steady() { grep -E "$1" "$2" | head -n -1 | tail -n +$((WARMUP + 1)); }

CLIENT_LINES="$(steady '^Receiving .*fps' "$CLIENT_LOG")"
SERVER_LINES="$(steady '^Sent .*fps' "$SERVER_LOG")"

[[ -n "$CLIENT_LINES" ]] || fail "receiver produced no steady-state statistics
      (ran ${DURATION}s, discarded ${WARMUP}s of warm-up):
$(tail -15 "$CLIENT_LOG")"
[[ -n "$SERVER_LINES" ]] || fail "sender produced no steady-state statistics:
$(tail -15 "$SERVER_LOG")"

errors=0
report() { printf '  %-24s %-28s %s\n' "$1" "$2" "$3"; }

check_min() { # label value threshold
    if awk -v v="$2" -v t="$3" 'BEGIN{exit !(v+0 >= t+0)}'; then
        report "$1" "$2 (min $3)" "ok"
    else
        report "$1" "$2 (min $3)" "FAIL"; errors=$((errors+1))
    fi
}
check_max() { # label value threshold
    if awk -v v="$2" -v t="$3" 'BEGIN{exit !(v+0 <= t+0)}'; then
        report "$1" "$2 (max $3)" "ok"
    else
        report "$1" "$2 (max $3)" "FAIL"; errors=$((errors+1))
    fi
}
check_has() { # label file pattern
    if grep -qE "$3" "$2"; then
        report "$1" "present" "ok"
    else
        report "$1" "MISSING" "FAIL"; errors=$((errors+1))
    fi
}

# Worst observed second rather than the mean: a single bad second is a defect,
# and averaging would hide it.
worst_min() { awk '{for(i=1;i<=NF;i++) if($i=="fps"||$i=="pkt/s"){v=$(i-1)}} \
    NR==1||v<m{m=v} END{printf "%.1f", m+0}' <<< "$1"; }

CLIENT_FPS_MIN="$(worst_min "$CLIENT_LINES")"
SERVER_FPS_MIN="$(worst_min "$SERVER_LINES")"
LAT_WORST="$(awk -F'arrival->sink ' 'NF>1{split($2,a," "); \
    if(a[1]+0>m) m=a[1]+0} END{printf "%.1f", m}' <<< "$CLIENT_LINES")"
OVERRUNS="$(awk -F'overruns ' 'NF>1{split($2,a," "); s+=a[1]} \
    END{print s+0}' <<< "$SERVER_LINES")"

# Sums a "key=value" counter across every steady-state line. Prints nothing when
# the key never appears, which is how the checks below stay skippable.
sum_key() {
    awk -v key="$1" '{
        for (i = 1; i <= NF; i++) {
            split($i, kv, "=")
            if (kv[1] == key) { s += kv[2] + 0; seen = 1 }
        }
    } END { if (seen) print s + 0 }' <<< "$2"
}

# Only the WebRTC receiver can read these out of the jitterbuffer, so they are
# absent on the UDP path and those checks report "skip". Both caught real
# defects: packets discarded before the depayloader, and the decoder silently
# throwing frames away when a slow sink left QoS enabled.
LOST_TOTAL="$(sum_key lost "$CLIENT_LINES")"
LATE_TOTAL="$(sum_key late "$CLIENT_LINES")"
# Frames handed to the decoder but never produced by it. The per-second sums
# telescope, so the difference over the whole window is just the frames in flight
# inside the decoder at the end minus those at the start - a frame or two either
# way regardless of how long the test runs. Real regressions are nothing like
# that small: a slow sink enabling QoS cost 6 frames every second, and picking
# the wrong sink cost all 60.
DEC_DROP_MAX=2
DEC_IN="$(sum_key dec_in "$CLIENT_LINES")"
DEC_OUT="$(sum_key dec_out "$CLIENT_LINES")"
DEC_DROPPED=""
[[ -n "$DEC_IN" && -n "$DEC_OUT" ]] && DEC_DROPPED=$((DEC_IN - DEC_OUT))

echo
echo "=== results: video ==="
check_min "receiver fps"     "$CLIENT_FPS_MIN" "$FPS_MIN"
check_min "sender fps"       "$SERVER_FPS_MIN" "$FPS_MIN"
[[ -n "$LAT_WORST" && "$LAT_WORST" != "0.0" ]] \
    && check_max "arrival->sink ms" "$LAT_WORST" "$LAT_MAX" \
    || report "arrival->sink ms" "not reported" "skip"
check_max "sender overruns"  "$OVERRUNS" "0"
check_max "udp buffer errors" "$((ERR_AFTER - ERR_BEFORE))" "0"
if [[ -n "$LOST_TOTAL" ]]; then
    check_max "packets lost"     "$LOST_TOTAL"  "0"
    check_max "packets late"     "$LATE_TOTAL"  "0"
else
    report "packets lost/late" "not reported" "skip"
fi
if [[ -n "$DEC_DROPPED" ]]; then
    check_max "decoder drops"    "$DEC_DROPPED" "$DEC_DROP_MAX"
else
    report "decoder drops" "not reported" "skip"
fi

# --- audio, in both directions ----------------------------------------------
if [[ "$AUDIO" != "off" ]]; then
    echo
    echo "=== results: audio (both directions) ==="
    for side in server client; do
        if [[ "$side" == "server" ]]; then LOG="$SERVER_LOG"; else LOG="$CLIENT_LOG"; fi
        TX="$(steady '^Audio sent .*pkt/s' "$LOG")"
        RX="$(steady '^Audio recv .*pkt/s' "$LOG")"

        if [[ -z "$TX" ]]; then
            report "$side audio sent" "no statistics" "FAIL"; errors=$((errors+1))
        else
            check_min "$side audio sent pkt/s" "$(worst_min "$TX")" "$AUDIO_PKT_MIN"
            check_max "$side audio tx overruns" \
                "$(awk -F'overruns ' 'NF>1{split($2,a," "); s+=a[1]} END{print s+0}' <<< "$TX")" "0"
        fi

        if [[ -z "$RX" ]]; then
            report "$side audio recv" "no statistics" "FAIL"; errors=$((errors+1))
        else
            check_min "$side audio recv pkt/s" "$(worst_min "$RX")" "$AUDIO_PKT_MIN"
            A_LAT="$(awk -F'arrival->sink ' 'NF>1{split($2,a," "); \
                if(a[1]+0>m) m=a[1]+0} END{printf "%.1f", m+0}' <<< "$RX")"
            check_max "$side audio arrival->sink" "$A_LAT" "$AUDIO_LAT_MAX"
            A_LOST="$(sum_key lost "$RX")"
            [[ -n "$A_LOST" ]] && check_max "$side audio lost" "$A_LOST" "0" \
                || report "$side audio lost" "not reported" "skip"
        fi
    done
fi

# --- encryption -------------------------------------------------------------
if [[ "$PKI" -eq 1 ]]; then
    echo
    echo "=== results: PKI ==="
    # Each of these is a check that ran, not just a flag that was accepted: the
    # peer's certificate was verified against the CA on both ends.
    check_has "sender authenticated peer"   "$SERVER_LOG" "^TLS: .* certificate verified"
    check_has "receiver authenticated peer" "$CLIENT_LOG" "^TLS: .* certificate verified"
    if [[ "$MODE" == "webrtc" ]]; then
        # One per session, and each confirms chain, SDP fingerprint and identity.
        for side in SERVER CLIENT; do
            LOG="$SERVER_LOG"; [[ "$side" == "CLIENT" ]] && LOG="$CLIENT_LOG"
            N="$(grep -c "DTLS peer verified" "$LOG")"
            EXPECT=1; [[ "$AUDIO" != "off" ]] && EXPECT=3
            check_min "$side DTLS sessions verified" "$N" "$EXPECT"
        done
        check_max "DTLS rejections" "$(grep -c "DTLS peer rejected" "$SERVER_LOG" \
            "$CLIENT_LOG" | awk -F: '{s+=$2} END{print s+0}')" "0"
    else
        check_has "SRTP keys delivered" "$SERVER_LOG" "SRTP keys delivered"
        check_has "video key installed"  "$CLIENT_LOG" "SRTP key installed for video"
        check_has "video SRTP decrypting" "$CLIENT_LOG" "SRTP key supplied for video"
    fi
fi

echo
echo "steady-state receiver output ($(wc -l <<< "$CLIENT_LINES") s, ${WARMUP}s warm-up discarded):"
sed 's/^/  /' <<< "$CLIENT_LINES"
echo "steady-state sender output:"
sed 's/^/  /' <<< "$SERVER_LINES"

echo
if [[ "$errors" -gt 0 ]]; then
    echo "RESULT: FAIL ($errors check(s) failed)"
    exit 1
fi
echo "RESULT: PASS"

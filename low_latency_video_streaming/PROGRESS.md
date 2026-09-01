# Telesurgery streaming - progress notes

Sessions: 2026-07-28 (video), 2026-07-31 (audio, PKI, threading). Everything
below is measured on this machine (Ubuntu 20.04, GStreamer 1.16.3,
NVIDIA 580.126.09, Quadro RTX 4000).

## Current state: working

1080p60 H.264 over UDP, server preview + client display, ~1 frame of
pipeline latency.

```bash
./client
./server /home/govindap/Work/Xi_Telesurgery/GStreamer/test_unique.yuv
```

Both print a per-second line; that is the fastest way to confirm health.

```
SERVER: Sent 60.0 fps | push->wire avg 6.4 ms | budget 16.7 ms | overruns 0
CLIENT: Receiving 60.0 fps | arrival->sink 9.7 ms = jitterbuf 0.2 + parse 0.2 + decode 9.3 + convert 0.0
```

Latency budget: 6.4 ms (server) + 9.7 ms (client) = ~16 ms, just under one
frame at 60 fps. Verified 0 UDP packet loss and pixel-correct output.

## Hard-won findings - do not rediscover these

**nvdec is broken on this box.** It decodes into GL textures that come back
blank, so every frame renders as a flat colour. `gldownload` and
`glcolorconvert` do not help; `nvdec ! videoconvert` silently passes GL
handles through instead of converting. Client therefore defaults to
`avdec_h264`, named explicitly because `decodebin` ranks nvdec higher and
would autoplug the broken element. `--hw-decode` opts back in. Retest only
after a GStreamer upgrade (1.18+ gives `nvh264dec` + CUDA memory, avoiding
the GL path entirely).

**rtpjitterbuffer's `latency` is a floor, not the whole delay.** The default
adaptive mode schedules packets against an estimate of the sender's clock and
held frames 21.7 ms even when configured for 5 ms. `mode=none` fixed it
(0.2 ms). Lowering `--latency` alone would never have helped. `mode=none`
gives up smoothing, so use `--jitter-mode slave --latency 50` on a real WAN
and expect ~1 frame back.

**Do not launch the server backgrounded from a script that then exits.** If
the parent shell is killed the process is orphaned, the display sink loses
clock sync, and the whole file streams ~8x too fast - which looks exactly
like a corruption bug. Foreground, or `setsid nohup ... < /dev/null &`.

**A photograph cannot resolve better than ~1 frame.** 60 Hz refresh
quantisation on both windows plus rolling shutter. Two readings of "2 frames"
and "3 frames" came from the same build. Use the printed `arrival->sink`
number instead. Display refresh still adds up to 16.7 ms to what the eye
sees, which is now larger than the entire pipeline.

**Audio: a live sender that starts before its transport can carry anything
poisons the receiver for the whole session.** The audio pipeline goes to PLAYING
as soon as it is built, but WebRTC cannot send until ICE and DTLS finish. The
packets produced in between queue up, and the burst that escapes when the
transport opens is what sets the receiving jitterbuffer's RTP-timestamp-to-clock
mapping - so every later packet is held by the length of that burst. Measured
241 ms of audio delay that never drained across a 35 s run, in one direction
only, while the other direction measured 0.1 ms. Two fixes, both needed:
a `leaky=downstream` queue at the transport boundary (stale live audio should be
dropped, not queued), and a pad probe that drops outgoing packets until
`ice-connection-state` is connected *and* the DTLS handshake has produced a peer
certificate. Residual after both: 0.1 ms.

**webrtcbin 1.16 declares `connection-state` but never drives it.** The first
version of that gate watched the aggregate `connection-state`, which stayed
"new" for an entire session while media flowed perfectly - so the gate dropped
1477 packets and only opened on its deadline. `ice-connection-state` does track.
Gating on ICE alone still left ~20 ms, because DTLS is not finished when ICE
connects and packets handed over in between queue inside webrtcbin; waiting for
`dtlssrtpdec`'s `peer-pem` to appear closes that.

**webrtcbin advertises a DTLS fingerprint for a certificate it does not use, if
you replace the certificate.** `deep-element-added` is the earliest hook for
installing a PKI identity into `dtlssrtpdec`, and it is still slightly too late:
webrtcbin reads a certificate out of the freshly constructed element to build the
`a=fingerprint` attribute *before* adding that element to the bin, so the SDP
carries the throwaway certificate the element generated on demand while the
handshake presents ours. Measured: SDP said `6C:11:B5...`, the handshake
presented `7B:79:39...`. GStreamer 1.16 never compares the two, so nothing
complains - which is exactly why the fingerprint check here is worth having. The
fix is to rewrite the attribute in our own SDP before sending it.

**`GTlsFileDatabase` silently refuses a relative anchor path.** It warns on
stderr and returns a database that then fails to verify anything, so a perfectly
good CA passed as `pki/ca.crt` looks like an untrusted one. The path is
canonicalised before use.

**`(GTlsCertificateFlags)G_MAXUINT` is -1 in C++.** Used as a "no verdict yet"
sentinel, it made every real verdict - including the successful 0 - compare as
larger, so valid certificates were reported as "chain could not be validated".
Fail-closed, so not dangerous, but it broke the feature until found. Sentinels in
signed enums are worth avoiding.

**Audio receive-side buffering depends on the codec, not just on `--audio-latency`.**
`rtpjitterbuffer` schedules L16 against the RTP clock because it knows exactly
how many samples each packet holds; for Opus it often pushes straight through.
Measured `arrival->sink` for audio, 48 kHz stereo:

| | `--audio-latency 0` | `--audio-latency 5` |
|---|---|---|
| WebRTC, opus 10 ms | 0.2 ms | 0.1 ms |
| UDP, opus 10 ms | 8.9 ms | 14.8 ms |
| UDP, opus 20 ms | 0.2 ms | - |
| WebRTC, raw (MTU-split, ~5 ms) | 15.0 ms | 20.0 ms |
| WebRTC, raw 24 kHz mono 10 ms | 10.0 ms | - |

The packet duration and the hold trade off against each other - on the UDP path
opus measured 13.2 ms of hold at 5 ms packets and 0.0 ms at 20 ms packets, both
landing near a fixed ~20 ms total. The useful conclusion is not a formula: it is
that raw wins on the send side (0.1 ms against opus's 6.8 ms, since there is no
encoder) and loses it again on the receive side, and that opus at 10 ms is the
lowest total on this stack. Hence opus as the default.

**`rtpL16pay` is bounded by the MTU, not by `max-ptime`.** 48 kHz stereo is
192 bytes per millisecond, so a 1400-byte MTU caps a packet at 7.2 ms and the
payloader splits each 10 ms buffer in two - the stat line reports `packet <=7.2 ms`
rather than pretending the request was honoured.

**Three peer connections, not one bundled sendrecv.** Video out, audio out and
audio back each get their own `webrtcbin` in their own pipeline on their own
thread. More handshakes at startup, and worth it: no shared clock, no shared
latency query (which takes the MAX over all sinks, so a soundcard's 20 ms of
device buffering would otherwise be added to video's budget), no shared state
machine, and every session is a plain one-m-line offer/answer that avoids 1.16's
rough edge on the answerer side of a sendrecv negotiation.

**webrtcbin drops SDP tasks queued before it reaches PAUSED, without an error.**
The audio-back offer is usually already sitting in the socket when the signalling
reader starts, and dispatching it into a webrtcbin that is still in NULL silently
did nothing: no answer, no warning, and a stream that simply never started. Every
pipeline now goes to PLAYING before the reader is started. Sending does not need
the reader, so offers still go out immediately.

**Tested and rejected:** `--decode-threads 1` (frame-threading was not the
problem; single-threaded decode is slower and latency rose to 35-42 ms).

## Architecture, and why

Server pushes frames from an `mmap`ed working set via `appsrc`, paced by its
own thread on absolute deadlines. Pacing deliberately does *not* come from
the display sink, so the preview can render a frame at the instant it is
transmitted - otherwise the preview trails the wire by 1-2 frames and any
side-by-side latency photo reads too low. `queue_display` is
`leaky=downstream` so a stalled preview cannot throttle the outgoing stream.

`--loop-seconds` (default 10) bounds the mapped set: 600 frames at 60 fps is
~1.78 GiB. Mapping the whole 3000-frame file (~8.7 GiB) would exceed RAM and
fault pages from disk on every pass. PTS comes from an ever-increasing
counter, never the wrapped index - restarting timestamps at the loop point
would send RTP backwards and stall the receiver.

`rtph264pay config-interval=-1` is required: the default of 0 sends SPS/PPS
once at startup, so any client attaching later never decodes anything.

Client sink: `sync=TRUE` (present on timestamps, matches sender rate), plus
`qos=FALSE` and `max-lateness=-1` applied by walking into the `autovideosink`
bin once it reaches PLAYING. Without that the sink sends QoS upstream at
60 fps and the decoder skips frames (~57 fps instead of 60). `autovideosink`
proxies `sync` but not `qos`/`max-lateness`, hence the bin walk.

### Audio, and why it is not synchronised to video

The per-second audio lines are off unless `--audio-stats` is passed. Two extra
lines per second per process buried the video line that is usually the one being
watched. The probes that feed them are only installed when the flag is set, so
the quiet path does no measuring work at all; the regression suite passes the
flag because those figures are what it gates on.

The requirement was explicit: audio and video need no common timebase. That is
what makes one pipeline per stream the right structure rather than a compromise -
see `media_worker.h` for the three concrete costs of sharing one. A microphone
that will not open, or an audio stream whose certificate does not check out, now
prints one line and leaves video running.

The default capture source is a test tone rather than the microphone. On one
machine both peers would otherwise open the same default input and output and
feed back into each other; `--audio-source device` opts in to real capture.

### Encryption, and what it actually proves

DTLS/SRTP was already encrypting the WebRTC path, but with a throwaway
self-signed certificate and an unauthenticated SDP exchange - so anyone able to
rewrite signalling could rewrite both fingerprints, terminate DTLS on each side
and read everything while both ends still reported "encrypted". With `--pki-*`:

- the signalling (and, on the UDP path, keying) channel is mutually
  authenticated TLS, both certificates checked against the CA;
- our X.509 identity replaces webrtcbin's generated DTLS certificate;
- after the handshake the peer's certificate is checked three ways - chain to the
  CA, fingerprint against what the SDP promised, and optionally subject name
  against `--pki-peer-identity` - and a failure stops that stream;
- the UDP path, which has no DTLS at all, gets SRTP (AES-128-ICM, HMAC-SHA1-80)
  with per-stream master keys generated from `/dev/urandom` and delivered over
  that authenticated channel.

Verified by running it: 3/3 DTLS sessions verified on each end, and the negative
cases fail closed - a peer holding a certificate from another CA cannot complete
the TLS handshake, and `--pki-peer-identity theatre-3` against `stream-sender`
is rejected as `wrong-identity`. On the UDP path, every packet reaching the sink
has passed `srtpdec`'s 80-bit authentication tag, which is only possible if the
sender really encrypted it with the shared key.

SRTP cost nothing measurable: video `arrival->sink` was 9.7 ms both with and
without it.

## Next steps

- Biggest remaining term is software decode at 9.3 ms (58% of the client
  path). Needs working hardware decode, i.e. a GStreamer upgrade.
- Consider raising `--bitrate` for real video: 4 Mbit/s at 1080p60 is fine
  for the synthetic flat-colour test pattern but thin for surgical content.
- `sudo sysctl -w net.core.rmem_max=524288` if moving to a real network or
  higher bitrates; the client currently clamps its request and says so.
- Untested: H.265 path, non-loopback network, resolutions other than 1080p.
- The WebRTC video gate has little headroom and the suite is occasionally flaky
  because of it. Worst-second `arrival->sink` sits at 10.6-11.0 ms against
  `--lat-max 15`, and across roughly a dozen runs two tripped: one on a single
  second that measured 27 ms (a ~60 ms stall, one frame at 104 ms, on a machine
  that was also rebuilding at the time) and one on the 15 s DTLS verification
  deadline. Both were transient - the same configuration passed three times in a
  row immediately afterwards - and neither is specific to audio or PKI. Worth
  either finding the stall or judging a percentile rather than the worst second.
- Real capture costs about 8 ms more than the test tone: `pulsesrc` measured
  15.0 ms `capture->wire` against 6.8 ms for `audiotestsrc`, which is the sound
  card handing over a period at a time. The default configuration therefore
  measures better than a deployment will.
- Audio receive-side hold on the UDP path deserves one more look: opus at 10 ms
  packets sits at 8.9-14.8 ms of `rtpjitterbuffer` scheduling that WebRTC does
  not pay. Reading 1.16's `rtpjitterbuffer` timer path would settle it.
- The DTLS certificate is installed on `deep-element-added` and the SDP
  fingerprint is then corrected by hand. A GStreamer version that lets the
  certificate be supplied up front would make that unnecessary - worth checking
  on the next upgrade.
- No certificate revocation and no expiry monitoring: `tools/make_certs.sh`
  issues 825-day leaves and nothing warns as they age.

# Low-Latency Video Streaming (GStreamer)

Streams raw I420 YUV as H.264/H.265 with a preview on both ends, plus
**bidirectional audio** and optional **X.509-authenticated encryption**, over
either RTP/UDP or WebRTC. Built for telesurgery, where glass-to-glass delay
matters more than compression efficiency.

There are two independent pairs of binaries. They are documented separately
below because their options, wiring and failure modes genuinely differ:

| | binaries | transport | encryption |
|---|---|---|---|
| [RTP/UDP](#rtpudp-transport) | `server` / `client` | Plain RTP over UDP | none, or [SRTP with PKI](#encryption-and-pki) |
| [WebRTC](#webrtc-transport) | `webrtc_server` / `webrtc_client` | RTP over DTLS/SRTP, ICE | DTLS/SRTP, self-signed or [PKI](#encryption-and-pki) |

Measured on the development machine at 1080p60, both reach the same video
latency, about one frame period end to end:

| | fps | server push&rarr;wire | client arrival&rarr;sink | total |
|---|---|---|---|---|
| RTP/UDP | 60.0 | 6.4 ms | 9.9 ms | ~16 ms |
| WebRTC | 60.0 | 6.1 ms | 11.0 ms | ~17 ms |

Audio runs in **both directions at once**, on its own threads, and is never
synchronised to video:

| | codec | capture&rarr;wire | arrival&rarr;sink |
|---|---|---|---|
| WebRTC | opus, 10 ms packets | 6.8 ms | 0.1 ms |
| RTP/UDP | opus, 10 ms packets | 6.8 ms | 5.9&ndash;14.8 ms |
| either | raw L16 | 0.1 ms | 10&ndash;20 ms |
| either | opus from a real microphone | 15.0 ms | 0.2 ms |

The last row is the honest one for a deployment: those `capture->wire` figures are
measured from a generated tone, and a real sound card hands over a period at a
time, which cost ~8 ms more through `pulsesrc` here.

Raw L16 has no encoder at all, which is why its send side is 0.1 ms; it gives
that back on the receive side, because the jitterbuffer schedules uncompressed
audio against the RTP clock. Opus at 10 ms is the lowest total on this stack and
is the default. See [Audio](#audio).

Encryption costs nothing measurable: video `arrival->sink` was 9.7 ms on the UDP
path both with and without SRTP.

## Reading the stat lines

Each end prints one video line per second, in the same format on both transports.
This is the fastest way to confirm a stream is healthy and to see where delay is
coming from.

```
SERVER: Sent 60.0 fps | push->wire avg 6.4 ms max 7.4 ms | budget 16.7 ms | overruns 0
CLIENT: Receiving 60.0 fps | arrival->sink 9.7 ms = jitterbuf 0.1 + parse 0.2 + decode 9.4 + convert 0.0
```

Audio is **quiet by default** - two more lines per second per process would bury
the video line, which is usually the one being watched. Add `--audio-stats` when
the audio figures are what matters, and each end then also reports the stream it
sends and the stream it receives:

```
SERVER: Audio sent 99.9 pkt/s | capture->wire avg 6.8 ms max 6.9 ms | packet 10.0 ms | codec opus | overruns 0
SERVER: Audio recv 99.9 pkt/s | arrival->sink 0.2 ms (max 0.2) = jitterbuf 0.0 + decode 0.1 | codec opus | pkts=100 lost=0 late=0
```

Either way, each end still prints once at startup which audio elements it chose,
where it is sending and what it is listening on, so a misconfigured stream is
visible without turning statistics on:

```
Audio capture: audiotestsrc 440 Hz tone (pass --audio-source device for a microphone)
Audio encoder: opusenc 96000 bit/s, frame-size=10 ms, type=voice
Audio out: opus to 127.0.0.1:5002 (SRTP)
Audio in: opus on 0.0.0.0:5004 (SRTP), jitterbuffer 5 ms mode=none
```

- **`push->wire`** &ndash; a frame's encode and payload time, from the pacer handing
  it to `appsrc` to the last RTP packet leaving the payloader. `budget` is the
  frame period; `overruns` counts frames that exceeded it.
- **`arrival->sink`** &ndash; receive-side delay, broken into its stages, from a
  packet arriving to the picture or the audio reaching the sink. It stops at the
  sink's pad, so it does not include the deliberate wait for a presentation time.
- **`capture->wire`** &ndash; how long after a packet's own timestamp it left the
  payloader: capture buffering, encode and payloading. Not directly comparable
  with the video figure, which is timed from an explicit push.

Video and audio figures added together are the pipeline latency for that
medium. Neither includes display refresh; see [Known issues](#known-issues).

A line that reads `No frames received` or `No audio captured` is reporting that
nothing is happening, never a measurement of zero, so it cannot be mistaken for
one by anything parsing the output.

The WebRTC client prints extra packet and frame counters &ndash; see
[Reading the WebRTC counters](#reading-the-webrtc-counters).

## Build

```bash
sudo apt-get install -y build-essential cmake pkg-config \
    libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
    gstreamer1.0-tools gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good gstreamer1.0-plugins-bad \
    gstreamer1.0-plugins-ugly gstreamer1.0-libav

mkdir -p build && cd build && cmake .. && make
```

That builds `server` and `client`. NVIDIA hardware encoding needs the driver's
GStreamer plugins (`nvh264enc`); the server falls back to `x264enc`
automatically and says which it chose.

Audio needs nothing extra: `opusenc`/`rtpopuspay` are in
`gstreamer1.0-plugins-base` and `rtpL16pay` in `-good`. SRTP (`srtpenc`) is in
`-bad`. TLS uses GIO, so `glib-networking` must be installed for `--pki-*` to
work &ndash; it is a default on Ubuntu, and its absence is reported at startup
rather than at handshake time.

### Additional packages for WebRTC

`webrtc_server` and `webrtc_client` need two more. `gstreamer1.0-nice` is a
**runtime** requirement, not a build one: it provides the ICE implementation
`webrtcbin` loads as `nicesrc`/`nicesink`. Without it `webrtcbin` is created
successfully and then can never connect, which is an easy fault to misread.

```bash
sudo apt-get install -y libgstreamer-plugins-bad1.0-dev gstreamer1.0-nice
```

CMake probes for `gstreamer-webrtc-1.0` and skips both WebRTC targets when it is
absent, so the UDP build keeps working without them. Which happened is printed
at configure time:

```
-- WebRTC support enabled (gstreamer-webrtc 1.16.3)
```

## Test input

Shared by both transports.

```bash
python3 generate_raw_video.py --output test.yuv --width 1920 --height 1080 \
    --duration 10 --framerate 60 --pattern unique
```

`--pattern unique` burns a frame number and a per-frame colour into each
frame, which is what makes dropped, stale, or duplicated frames visible. Note
that 10 s of 1080p60 I420 is ~1.8 GiB; the file is raw and uncompressed.

Any existing footage can be converted:

```bash
ffmpeg -i input.mp4 -f rawvideo -pix_fmt yuv420p -s 1920x1080 output.yuv
```

Audio needs no test asset: the default capture source is a generated tone.

---

# Audio

What is supported:

- **Both directions at once**, on both transports. Each end captures, sends,
  receives and plays; `--audio-duplex` narrows that to one direction or none.
- **Two encoders**: `raw` (L16 PCM, no encoder delay) and `opus` (the advanced
  path, configured for low delay rather than for music). `--audio off` disables
  audio entirely and the binaries behave exactly as they did before it existed.
- **Capture from a device or a generated tone**, `--audio-source device|test`,
  with the device chosen by `--audio-device`.
- **Encrypted with everything else** when `--pki-*` is supplied: SRTP on the UDP
  path, DTLS/SRTP on WebRTC, same keys and same identity checks as video.
- **Independent of video** in every sense that matters &ndash; separate pipeline,
  separate thread, separate clock, no shared latency budget. A microphone that
  will not open prints one line and leaves video running.
- **Measured, not asserted**: `--audio-stats` prints capture&rarr;wire and
  arrival&rarr;sink per second for each direction, and the regression suite gates
  on them.

Both ends send and both ends play, so the same options exist on all four
binaries. Defaults are chosen so that `--audio` alone does something sensible.

| Option | Default | Notes |
|---|---|---|
| `--audio <off\|raw\|opus>` | `opus` | Encoder; see below |
| `--audio-duplex <both\|send\|recv\|off>` | `both` | Directions to run |
| `--audio-source <test\|device\|none>` | `test` | **Test tone by default**, see below |
| `--audio-device <name>` | default input | For `--audio-source device` |
| `--audio-tone <hz>` | 440 sender, 660 receiver | Test tone frequency |
| `--audio-sink <element>` | `auto` | `pulsesink`/`alsasink`/`autoaudiosink`, or `fakesink` |
| `--audio-rate <hz>` | `48000` | |
| `--audio-channels <n>` | `2` | |
| `--audio-frame-ms <ms>` | `10` | Packet duration; opus takes 2.5/5/10/20/40/60 |
| `--audio-bitrate <bps>` | `96000` | Opus only |
| `--audio-latency <ms>` | `5` | Audio jitterbuffer depth |
| `--audio-jitter-mode <mode>` | `none` | `none\|slave\|buffer\|synced` |
| `--audio-sink-buffer-ms <ms>` | `30` | Playback device buffer |
| `--audio-stats` | off | Print the per-second audio figures |

The UDP pair also has `--audio-port` (5002, outgoing from the sender) and
`--audio-back-port` (5004, returning), and the UDP receiver needs
`--peer-host` to know where to send its own audio. WebRTC negotiates its media
ports through ICE, as it does for video.

**The capture default is a test tone, not the microphone.** On one machine both
peers would otherwise open the same default input and the same default output
and feed back into each other. The two ends use different tone frequencies (440
and 660 Hz) so it is obvious by ear which direction is being heard. For real
capture:

```bash
./build/webrtc_server test.yuv --audio-source device
./build/webrtc_client --audio-source device
```

## Choosing an encoder

```
--audio raw     L16 PCM over RTP. No encoder, so no algorithmic delay at all:
                0.1 ms from timestamp to wire. ~1.5 Mbit/s for 48 kHz stereo.
--audio opus    Configured for its low-delay mode rather than its default music
                mode: short frames, CBR, no DTX, no in-band FEC. ~96 kbit/s.
```

Raw is not automatically the lower-latency choice, and the printed numbers say
why. It wins the send side outright and gives it back on the receive side,
because `rtpjitterbuffer` knows exactly how many samples an L16 packet holds and
schedules it against the RTP clock, whereas Opus is often pushed straight
through. Measured `arrival->sink`, 48 kHz stereo:

| | `--audio-latency 0` | `--audio-latency 5` |
|---|---|---|
| WebRTC, opus 10 ms | 0.2 ms | 0.1 ms |
| RTP/UDP, opus 10 ms | 8.9 ms | 14.8 ms |
| RTP/UDP, opus 20 ms | 0.2 ms | &ndash; |
| WebRTC, raw (~5 ms packets) | 15.0 ms | 20.0 ms |
| WebRTC, raw 24 kHz mono, 10 ms | 10.0 ms | &ndash; |

So: **opus at 10 ms is the lowest total on this stack**, which is why it is the
default. Reach for `raw` when a downstream consumer needs untouched PCM, or when
encoder delay specifically has to be zero, and tune `--audio-latency` and
`--audio-frame-ms` against the printed figures rather than by assumption.

One honest wart: `rtpL16pay` is bounded by the MTU, not by the requested packet
duration. 48 kHz stereo is 192 bytes per millisecond, so a 1400-byte MTU caps a
packet at 7.2 ms and each 10 ms buffer is split in two. The stat line reports
`packet <=7.2 ms` rather than pretending otherwise.

---

# Encryption and PKI

What is supported:

- **Every stream encrypted**: video and audio, both directions, on both
  transports. SRTP (AES-128-ICM, HMAC-SHA1-80) on the UDP path; DTLS/SRTP on
  WebRTC.
- **Both peers authenticated** against a CA you supply, with an optional
  requirement on the peer's subject name.
- **Enforcement, not decoration**: a peer that cannot be authenticated does not
  get a stream, and the sender refuses to fall back to plaintext.
- **Off unless configured.** With no `--pki-*` options both transports behave
  exactly as before.

Supplying certificates turns all of it on at once.

The WebRTC path was already encrypted before this, and that is precisely the
distinction worth being clear about. DTLS/SRTP with a per-run self-signed
certificate proves only that *someone* is at the other end. Because signalling
was a plain TCP socket, anyone able to sit in the middle could rewrite both
fingerprints, terminate DTLS on each side, and read every frame while both ends
happily reported "encrypted".

With `--pki-*` supplied:

- **Signalling is mutually authenticated TLS.** Both certificates are checked
  against the CA. This is what makes the fingerprints in the SDP worth anything.
- **Our X.509 identity replaces webrtcbin's generated DTLS certificate**, so the
  peer authenticates *us*, not an anonymous endpoint.
- **The peer's DTLS certificate is verified three ways** after the handshake:
  chain to the CA, fingerprint against what the SDP promised, and optionally
  subject name against `--pki-peer-identity`. GStreamer 1.16 checks none of
  these itself. A failure stops that stream.
- **The UDP path gets SRTP** (AES-128-ICM, HMAC-SHA1-80) on every stream, with
  per-stream master keys read from `/dev/urandom` and delivered over the same
  mutually authenticated TLS channel.

## Generating a demo PKI

```bash
./tools/make_certs.sh pki                       # loopback / single host
./tools/make_certs.sh pki 10.0.0.5 10.0.0.6     # add SANs for two machines
```

That writes a CA and one certificate for each end:

```
pki/ca.crt                 the trust anchor both ends need
pki/sender.crt   .key      CN=stream-sender
pki/receiver.crt .key      CN=stream-receiver
```

It is a demo CA: the key sits next to the certificates with no passphrase and
there is no revocation infrastructure. For a real deployment, issue the two leaf
certificates from whatever CA already governs the site and point `--pki-ca` at
that instead. `pki/`, `*.key` and `*.crt` are gitignored.

## Running encrypted

Same for both transports; the four PKI options are identical on all four
binaries.

```bash
# receiver
./build/webrtc_client \
    --pki-ca pki/ca.crt --pki-cert pki/receiver.crt --pki-key pki/receiver.key \
    --pki-peer-identity stream-sender

# sender
./build/webrtc_server test.yuv \
    --pki-ca pki/ca.crt --pki-cert pki/sender.crt --pki-key pki/sender.key \
    --pki-peer-identity stream-receiver
```

| Option | Notes |
|---|---|
| `--pki-ca <file>` | CA the peer's certificate must chain to |
| `--pki-cert <file>` | Our certificate chain, PEM |
| `--pki-key <file>` | Our private key, PEM |
| `--pki-peer-identity <name>` | Require this CN/SAN in the peer's certificate |

All three of `--pki-ca`, `--pki-cert` and `--pki-key` are required together;
anything less is rejected at startup rather than half-applied.
`--pki-peer-identity` is optional: without it, any certificate signed by that CA
is accepted.

On the UDP path the receiver additionally **listens on a TLS keying port**
(5010, `--key-port`) and blocks until the sender connects, because without the
sender's keys there is nothing it could decrypt. The sender refuses to stream at
all if it cannot reach that port &ndash; it will not silently fall back to
plaintext.

## What success and failure look like

```
TLS: server certificate verified, peer cert sha-256 7B:79:39:...
Signalling connected to 127.0.0.1:5020 (TLS, mutually authenticated)
[video] DTLS certificate replaced with the PKI identity (dtlssrtpdec4)
[video] SDP fingerprint set to the installed certificate (1 attribute)
[video] DTLS peer verified: CA-signed, fingerprint matches the SDP, identity matches
```

and on the UDP path:

```
SRTP keys delivered over the authenticated channel
SRTP encryption on for video (aes-128-icm, hmac-sha1-80)
SRTP key installed for video (aes-128-icm, hmac-sha1-80)
```

Failures are refusals, not warnings. A certificate from a different CA fails the
TLS handshake and nothing streams; the wrong `--pki-peer-identity` is reported as
`rejected (wrong-identity )`; a DTLS certificate that does not match the
fingerprint in the SDP prints both hashes and stops that stream.

Two implementation notes that are easy to trip over, both documented at length in
`PROGRESS.md`: webrtcbin reads its DTLS certificate for the SDP *before* the
element is added to the bin, so the fingerprint attribute has to be corrected by
hand afterwards; and `GTlsFileDatabase` silently refuses a relative anchor path,
which makes a good CA look untrusted.

---

# RTP/UDP transport

Plain RTP over UDP, no connection setup. The lowest-overhead option and the
right default on a controlled network.

## Run

Start the client first so it is listening, then the server:

```bash
./build/client
./build/server test.yuv
```

Run the server in the **foreground**. If it is backgrounded from a script that
then exits, the process is orphaned, the display sink loses clock sync, and
the file streams several times too fast. Use `setsid nohup ... < /dev/null &`
if you genuinely need it detached.

Both ends now also exchange audio, so on one machine pass
`--audio-sink fakesink` (or `--audio off`) unless you want to hear two tones.

## Server options

`./build/server <input_file> [options]`, or `--help`.

| Option | Default | Notes |
|---|---|---|
| `--codec <h264\|h265>` | `h264` | |
| `--host <address>` | `127.0.0.1` | Destination, not a bind address |
| `--port <port>` | `5000` | Video |
| `--width` / `--height` | `1920` / `1080` | Must match the input file |
| `--framerate <fps>` | `60` | Also sets the pacing deadline |
| `--bitrate <bps>` | `4000000` | Thin for real surgical content; raise it |
| `--loop-seconds <sec>` | `10` | Working set to map and loop; `0` = whole file |
| `--no-gpu` | off | Force software encoding |
| `--gpu-platform <auto\|nvidia\|intel\|amd>` | `auto` | |
| `--audio-port` / `--audio-back-port` | `5002` / `5004` | Outgoing / returning audio |
| `--key-port <port>` | `5010` | Receiver's TLS keying port, PKI only |

Plus the [audio](#audio) and [PKI](#encryption-and-pki) options.

`--loop-seconds` bounds memory. The input is `mmap`ed and looped, so at 1080p60
each 10 s is ~1.8 GiB; mapping a long file would exceed RAM and fault pages
from disk on every pass.

## Client options

`./build/client [options]`, or `--help`.

| Option | Default | Notes |
|---|---|---|
| `--host <address>` | `0.0.0.0` | Bind address |
| `--port <port>` | `5000` | Video |
| `--codec <h264\|h265>` | `h264` | Must match the server |
| `--latency <ms>` | `5` | Video jitterbuffer depth |
| `--jitter-mode <mode>` | `none` | `none\|slave\|buffer\|synced` |
| `--buffer-size <bytes>` | `524288` | Clamped by `net.core.rmem_max` |
| `--hw-decode` | off | Opt in to NVIDIA decode; see caveat below |
| `--peer-host <address>` | `127.0.0.1` | **Where this side's audio is sent** |
| `--audio-port` / `--audio-back-port` | `5002` / `5004` | Incoming / outgoing audio |
| `--key-port <port>` | `5010` | TLS keying port to listen on, PKI only |

Plus the [audio](#audio) and [PKI](#encryption-and-pki) options.

### Tuning for a real network

The defaults are tuned for loopback and a clean LAN. `--jitter-mode none`
forwards packets as soon as they arrive, which is what keeps latency near one
frame, but it gives up the smoothing that a jittery or reordering link needs.
On a real WAN:

```bash
./build/client --jitter-mode slave --latency 50 \
               --audio-jitter-mode slave --audio-latency 50
```

Expect to pay back roughly a frame of video latency for that. Also raise the
socket buffer if the link is fast or the bitrate is high:

```bash
sudo sysctl -w net.core.rmem_max=524288
```

The client clamps its request to the kernel limit and prints the command to
lift it, rather than failing silently.

---

# WebRTC transport

Same content and nearly the same measured latency, but encrypted with DTLS/SRTP
and able to traverse NAT.

## Run

Start the receiver first &ndash; it listens for signalling &ndash; then the sender:

```bash
./build/webrtc_client
./build/webrtc_server test.yuv
```

The same foreground warning as the UDP server applies.

Across two machines, point the sender at the receiver's signalling address:

```bash
# on the receiving host
./build/webrtc_client

# on the sending host
./build/webrtc_server test.yuv --host 10.0.0.5
```

## How signalling works

No signalling server is required. The two processes exchange SDP, ICE
candidates and (on the UDP path) SRTP keys over **a plain TCP socket**, one
newline-delimited message per line, each tagged with the stream it belongs to:

```
SDP <session> <type> <byte-length>\n<sdp bytes>
ICE <session> <mline-index> <candidate>\n
KEY <session> <cipher> <auth> <base64 master key>\n
```

The receiver listens on port 5020, the sender connects to it. One connection
carries all three sessions (`video`, `audio-down`, `audio-up`). That keeps
`json-glib` and `libsoup` out of the dependency list, which are normally pulled
in only to talk to a websocket signalling service this project does not need.
With `--pki-*` the whole channel runs inside mutually authenticated TLS.

On a LAN, host candidates are enough and no STUN server is needed. Across NAT,
pass one:

```bash
./build/webrtc_server test.yuv --stun-server stun://stun.l.google.com:19302
```

## Server options

`./build/webrtc_server <input_file> [options]`, or `--help`.

| Option | Default | Notes |
|---|---|---|
| `--host <address>` | `127.0.0.1` | Receiver's signalling address |
| `--port <port>` | `5020` | Receiver's signalling port |
| `--width` / `--height` | `1920` / `1080` | Must match the input file |
| `--framerate <fps>` | `60` | Also sets the pacing deadline |
| `--bitrate <bps>` | `4000000` | Thin for real surgical content; raise it |
| `--loop-seconds <sec>` | `10` | Working set to map and loop; `0` = whole file |
| `--stun-server <uri>` | none | `stun://host:port`; unnecessary on a LAN |
| `--no-gpu` | off | Force software encoding |

Plus the [audio](#audio) and [PKI](#encryption-and-pki) options.

Two differences from the UDP server: there is **no `--codec`**, because this path
offers H.264 only, and no `--gpu-platform`.

## Client options

`./build/webrtc_client [options]`, or `--help`.

| Option | Default | Notes |
|---|---|---|
| `--host <address>` | `0.0.0.0` | Signalling bind address |
| `--port <port>` | `5020` | Signalling port, **not** a media port |
| `--latency <ms>` | `5` | Video jitterbuffer depth |
| `--jitter-mode <mode>` | `none` | `none\|slave\|buffer\|synced` |
| `--sink <element>` | `ximagesink` | `fakesink` measures without a display |

Plus the [audio](#audio) and [PKI](#encryption-and-pki) options.

Media ports are negotiated by ICE and are not configurable. `--port` is only the
signalling channel.

`--latency` and `--jitter-mode` behave as on the UDP client, including the WAN
advice above. They matter more here: `webrtcbin` has no `latency` property in
GStreamer 1.16, so the client reaches into the `rtpbin` it creates internally and
configures each `rtpjitterbuffer` as it appears. Without that the jitterbuffer
keeps its 200 ms default and no tuning elsewhere gets near one frame.

## Reading the WebRTC counters

The receiver appends packet and frame counters to its stat line, because several
very different faults all present as "frames are missing":

```
| pkts=595 lost=0 late=0 dup=0 | frames jb=595 dec_in=60 dec_out=60 sink=60
```

| Counter | Meaning when it is wrong |
|---|---|
| `lost`, `late` | A transport problem: packets never arrived, or arrived too late for `--latency` |
| `dup` | Packets arriving twice |
| `dec_in` &gt;&gt; `dec_out` | A slow **sink**, not a slow network: the decoder is discarding frames to catch up |
| `dec_out` &gt; `sink` | Frames dropped between decoder and display |

`dec_in` and `dec_out` differing by one or two is normal - those are frames in
flight when the counters were sampled.

This distinction is worth understanding before debugging: a slow sink once showed
`lost=0` with `dec_in=60 dec_out=0`, i.e. a display rendering nothing while the
network was provably perfect. See Known issues.

The audio receive line carries `pkts`/`lost`/`late` from its own jitterbuffer for
the same reason.

---

# Regression tests

`tests/run_stream_test.sh` runs a sender and receiver, then asserts on the stat
lines both print. It exits non-zero on regression, so it can gate a commit.

```bash
./tests/run_stream_test.sh                          # RTP/UDP, opus audio
./tests/run_stream_test.sh --mode webrtc            # WebRTC path
./tests/run_stream_test.sh --audio raw              # uncompressed audio
./tests/run_stream_test.sh --audio off              # video only
./tests/run_stream_test.sh --pki                    # encrypted, with identities
```

It checks, at both ends: frame rate, worst-case video `arrival->sink`, sender
overruns, kernel UDP receive-buffer errors, audio packet rate **in both
directions**, worst-case audio `arrival->sink`, and audio loss. On the WebRTC
path it also checks packet loss and decoder drops. With `--pki` it asserts that
the checks actually ran &ndash; that each end authenticated the other, that all
three DTLS sessions were verified (WebRTC) or that the SRTP keys were delivered
and installed (UDP), and that nothing was rejected.

Startup seconds are discarded before judging, since WebRTC cannot send anything
until ICE and DTLS complete. Audio capture is a test tone into `fakesink`, so the
test needs no sound card and cannot be perturbed by what a microphone picks up.

Useful flags: `--duration`, `--lat-max`, `--audio-lat-max`, `--warmup`,
`--keep-logs`, `--pki-dir`. The script passes `--audio-stats` itself, since the
audio figures are what several of its checks read.

A window opens while it runs - measuring a real video sink is the point. Use
`--sink fakesink` on the client to measure decode cost without a display.

---

# Known issues

Each is tagged with the transport it affects.

**(WebRTC) Do not use `autovideosink` for the receiver.** It autoplugs
`xvimagesink`, whose QoS never got disabled through the wrapping bin; the
decoder then dropped every frame to catch up and the display showed 0 fps while
the network showed zero packet loss. Measured at 1080p60: `ximagesink` 60 fps at
~17 ms, `xvimagesink` 60 fps at ~75 ms, `glimagesink` ~968 ms, `autovideosink`
0 fps. The default is `ximagesink`; override with `--sink`.

**(both) `nvdec` produces blank frames on the development machine.** It decodes into
GL textures that come back empty, so every frame renders as a flat colour;
`gldownload` and `glcolorconvert` do not help. The client therefore names
`avdec_h264` explicitly rather than using `decodebin`, which ranks `nvdec`
higher and would autoplug the broken element. On the UDP client `--hw-decode`
opts back in for hosts where it works; the WebRTC client always uses software
decode. GStreamer 1.18+ offers `nvh264dec` with CUDA memory output, avoiding the
GL path entirely.

**(both) Software decode is the largest single cost on the receive path.**
Measured per frame at 1080p60: ~9.4 ms of the UDP client's 9.9 ms, and ~4.5 ms
of the WebRTC client's, the remainder there being colour conversion for
`ximagesink`. Working hardware decode is the main remaining optimisation.

**(RTP/UDP) Audio pays receive-side scheduling that WebRTC does not.** Opus at
10 ms packets measures 8.9&ndash;14.8 ms of `rtpjitterbuffer` hold on the UDP
path against 0.1 ms through `webrtcbin`, for the same settings. Larger packets
reduce the hold and add their own duration instead, so the total barely moves.
Tune from the printed numbers; see `PROGRESS.md`.

**(WebRTC) H.264 only.** The sender has no `--codec`; the UDP pair does H.265 too.

**(both) Display refresh adds up to 16.7 ms** to what the eye actually sees on a
60 Hz panel, independent of the pipeline. This exceeds the whole pipeline
budget, so judge latency from the printed `arrival->sink` figure rather than
by photographing two windows side by side.

**(both) No certificate revocation or expiry monitoring.** `tools/make_certs.sh`
issues 825-day leaf certificates and nothing warns as they approach expiry; an
expired certificate simply stops the stream with `rejected (expired )`.

---

# Architecture notes

## One stream, one pipeline, one thread

Every stream &ndash; outgoing video, outgoing audio, incoming audio &ndash; gets
its own `GstPipeline` driven by its own `GMainLoop` on its own thread
(`media_worker.h`). Audio is never synchronised to video here, so sharing a
pipeline would buy nothing and cost three concrete things:

- **Latency is negotiated per pipeline as the MAX over all sinks.** A soundcard
  reporting 20 ms of device buffering would raise the *video* sink's configured
  latency by the same amount.
- **A pipeline has one clock.** Live audio capture and the video pacer would be
  slaved together even though nothing needs them aligned.
- **State changes and errors are pipeline-wide.** A microphone that fails to open
  would take video down with it.

On the WebRTC path this extends to the transport: three separate `webrtcbin`
instances, one per stream and direction, so each session is a plain
single-m-line offer/answer. That costs three ICE/DTLS handshakes at startup and
avoids webrtcbin 1.16's rough edge on the answerer side of a sendrecv
negotiation entirely.

Video is essential and audio is not, and the supervisor treats them that way: a
failed audio stream logs and continues, a failed video stream shuts the process
down.

## Shared by both senders

Frames are pushed from an `mmap`ed working set through `appsrc`, paced by a
dedicated thread against absolute deadlines. Pacing deliberately does not derive
from the display sink, so the local preview shows a frame at the moment it is
transmitted; otherwise the preview trails the wire and a side-by-side latency
comparison reads too low. The preview branch is `leaky=downstream` so a stalled
preview cannot throttle the outgoing stream.

Outgoing audio ends in a small `leaky=downstream` queue for a related but
distinct reason: audio is live, so if the transport stops accepting packets the
right answer is to drop the stale ones. Queueing them means a burst escapes when
the transport reopens, and that burst sets the far end's jitterbuffer timestamp
mapping &ndash; which then holds every later packet by the length of the burst.
Measured 241 ms of permanent audio delay before this was addressed. On WebRTC a
pad probe additionally drops outgoing audio until ICE is connected *and* DTLS has
completed, so the first packet the far end ever sees is a fresh one.

## Shared by both receivers

Both run their sink with `sync=TRUE` but `qos=FALSE` and `max-lateness=-1`,
applied by walking the sink for those properties once it reaches `PLAYING`.
Without it the sink decides it cannot keep up at 60 fps and sends QoS events
upstream, making the decoder skip frames - about 3 fps lost on the UDP path.

The UDP receiver uses `autovideosink` and that walk reaches its chosen sink. The
WebRTC receiver defaults to `ximagesink` instead, because through
`autovideosink`'s wrapping bin the QoS setting did not take effect and the
decoder discarded every frame.

Both name their decoder explicitly, falling back to `decodebin` only if that
fails, because `decodebin` ranks the broken `nvdec` higher.

## Specific to WebRTC

`webrtcbin` is opaque about buffering: it has no `latency` property in 1.16 and
creates its `rtpbin` internally. The receiver therefore hooks `rtpbin`'s
new-jitterbuffer signal and configures every `rtpjitterbuffer` as it appears,
which is the only point where the 200 ms default can be overridden.

The encoder branch is pinned to system memory with a `videoconvert`. `nvh264enc`
also advertises `video/x-raw(memory:GLMemory)`, and through a `tee` negotiation
can settle on GL memory that `appsrc` cannot produce, which fails at runtime as
`not-negotiated`.

Pipelines go to `PLAYING` before the signalling reader is started. webrtcbin only
starts its internal task loop on the way to `PAUSED` and silently drops anything
enqueued before that, so a remote offer already waiting in the socket would be
applied to a webrtcbin that ignores it &ndash; no answer, no warning, and a
stream that never starts.

## Source layout

| File | Contents |
|---|---|
| `server.cpp` / `client.cpp` | RTP/UDP sender and receiver |
| `webrtc_server.cpp` / `webrtc_client.cpp` | WebRTC sender and receiver |
| `media_worker.h` | One pipeline, one main loop, one thread, plus supervision |
| `audio_stream.h` | Raw/Opus capture, encode, decode and playout; audio statistics |
| `media_pki.h` | X.509 loading, mutual TLS, DTLS identity and verification, SRTP keys |
| `signal_channel.h` | The session-tagged SDP/ICE/KEY channel, plain or TLS |
| `webrtc_session.h` | One peer connection: offer/answer, ICE, PKI, send gating |
| `webrtc_common.h` | webrtcbin jitterbuffer tuning |
| `media_props.h` | Version-tolerant element property helpers |
| `tools/make_certs.sh` | Demo CA and per-end certificates |

Further design rationale, and the reasoning behind specific settings, is in
`PROGRESS.md`.

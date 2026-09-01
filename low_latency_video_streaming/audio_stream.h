// Bidirectional audio, built for the same latency target as the video path.
//
// Two encoders are offered and they sit at opposite ends of the trade-off:
//
//   raw  - L16 over RTP. No encoder, so no algorithmic delay at all: a packet
//          can be sent as soon as its samples exist. Costs ~1.5 Mbit/s for
//          48 kHz stereo, and the MTU, not the requested packet duration, is
//          what bounds how much audio fits in one packet.
//   opus - the advanced path. Configured for its low-delay mode rather than its
//          default music mode: short frames, CBR, no DTX and no in-band FEC,
//          all of which otherwise trade latency for bitrate or resilience.
//          ~96 kbit/s and roughly one frame of algorithmic delay.
//
// Audio is never synchronised against video here, by design - see
// media_worker.h. Each direction is an independent pipeline on its own thread,
// so the microphone and the display share nothing.
//
// The default capture source is a test tone, not the microphone. On a single
// machine both peers would otherwise open the same default input and output and
// feed back into each other, which is an unpleasant way to discover a default.
// Pass --audio-source device for real capture.

#ifndef AUDIO_STREAM_H
#define AUDIO_STREAM_H

#include <gst/gst.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <stdio.h>
#include <string.h>

#include "media_pki.h"
#include "media_props.h"
#include "media_worker.h"

// Distinct from the video payload type so a stream is identifiable from a
// capture even when both share a transport.
#define MP_AUDIO_PT_OPUS 97
#define MP_AUDIO_PT_RAW  98

#define MP_AUDIO_RING 1024

typedef enum {
    MP_AUDIO_OFF = 0,
    MP_AUDIO_RAW,
    MP_AUDIO_OPUS
} MpAudioCodec;

typedef struct {
    MpAudioCodec codec;
    gboolean send;
    gboolean receive;
    // A misspelled codec must not quietly fall back to the default: on the UDP
    // path the caps are built from this, so the two ends would disagree about
    // what is on the wire and nothing would decode.
    gboolean invalid;

    gint rate;
    gint channels;
    gdouble frame_ms;        // target packet duration
    gint bitrate;            // opus, bits/second

    gchar *source;           // test | device | none
    gchar *device;           // capture device, NULL for the default
    gint tone_hz;            // test source frequency

    gchar *sink;             // auto | fakesink | an element name
    gint sink_buffer_ms;

    gint jb_latency_ms;
    gchar *jitter_mode;

    // Per-second audio statistics are off by default: two extra lines per second
    // per process buries the video line that is usually what someone is watching.
    // The regression suite turns them on, because they are what proves audio is
    // flowing in both directions and how much delay it is carrying.
    gboolean stats;
} MpAudioConfig;

static void mp_audio_config_defaults(MpAudioConfig *cfg, gint tone_hz) {
    memset(cfg, 0, sizeof(*cfg));
    cfg->codec = MP_AUDIO_OPUS;
    cfg->send = TRUE;
    cfg->receive = TRUE;
    cfg->rate = 48000;
    cfg->channels = 2;
    cfg->frame_ms = 10.0;
    cfg->bitrate = 96000;
    cfg->source = g_strdup("test");
    cfg->device = NULL;
    cfg->tone_hz = tone_hz;
    cfg->sink = g_strdup("auto");
    // Enough to ride out scheduling jitter on the playback thread without
    // dominating the latency figure. Lower it if the host can take it.
    cfg->sink_buffer_ms = 30;
    // Same reasoning as the video jitterbuffer: keep the buffer short and stop
    // the adaptive scheduler adding a packet on top of it.
    cfg->jb_latency_ms = 5;
    cfg->jitter_mode = g_strdup("none");
}

static void mp_audio_config_clear(MpAudioConfig *cfg) {
    g_free(cfg->source);
    g_free(cfg->device);
    g_free(cfg->sink);
    g_free(cfg->jitter_mode);
}

static const gchar* mp_audio_codec_name(MpAudioCodec codec) {
    switch (codec) {
        case MP_AUDIO_RAW:  return "raw";
        case MP_AUDIO_OPUS: return "opus";
        default:            return "off";
    }
}

static const gchar* mp_audio_encoding_name(MpAudioCodec codec) {
    return codec == MP_AUDIO_RAW ? "L16" : "OPUS";
}

static gint mp_audio_payload_type(MpAudioCodec codec) {
    return codec == MP_AUDIO_RAW ? MP_AUDIO_PT_RAW : MP_AUDIO_PT_OPUS;
}

// Opus runs its low-delay modes at 48 kHz; other rates are resampled internally
// anyway and only confuse the RTP clock rate.
static gint mp_audio_clock_rate(const MpAudioConfig *cfg) {
    return cfg->codec == MP_AUDIO_OPUS ? 48000 : cfg->rate;
}

// L16 is uncompressed, so the packet duration is capped by the MTU rather than
// by what was asked for: 48 kHz stereo is 192 bytes per millisecond.
static gdouble mp_audio_mtu_limited_ms(const MpAudioConfig *cfg, gint mtu) {
    gdouble bytes_per_ms = (gdouble)cfg->rate * cfg->channels * 2 / 1000.0;
    return (gdouble)(mtu - 12) / bytes_per_ms;
}

static gboolean mp_audio_parse_arg(MpAudioConfig *cfg, int argc, char **argv, int *i) {
    const gchar *arg = argv[*i];
    gboolean has_value = (*i + 1 < argc);

    if (g_strcmp0(arg, "--audio") == 0 && has_value) {
        const gchar *value = argv[++(*i)];
        if (g_strcmp0(value, "raw") == 0) {
            cfg->codec = MP_AUDIO_RAW;
        } else if (g_strcmp0(value, "opus") == 0) {
            cfg->codec = MP_AUDIO_OPUS;
        } else if (g_strcmp0(value, "off") == 0) {
            cfg->codec = MP_AUDIO_OFF;
        } else {
            g_printerr("--audio must be off, raw or opus (got '%s')\n", value);
            cfg->invalid = TRUE;
        }
        return TRUE;
    }
    if (g_strcmp0(arg, "--audio-duplex") == 0 && has_value) {
        const gchar *value = argv[++(*i)];
        if (g_strcmp0(value, "off") == 0) {
            cfg->codec = MP_AUDIO_OFF;
        } else if (g_strcmp0(value, "both") != 0 && g_strcmp0(value, "send") != 0 &&
                   g_strcmp0(value, "recv") != 0) {
            g_printerr("--audio-duplex must be both, send, recv or off (got '%s')\n",
                       value);
            cfg->invalid = TRUE;
            return TRUE;
        }
        cfg->send = (g_strcmp0(value, "both") == 0 || g_strcmp0(value, "send") == 0);
        cfg->receive = (g_strcmp0(value, "both") == 0 || g_strcmp0(value, "recv") == 0);
        return TRUE;
    }
    if (g_strcmp0(arg, "--audio-rate") == 0 && has_value) {
        cfg->rate = atoi(argv[++(*i)]);
        return TRUE;
    }
    if (g_strcmp0(arg, "--audio-channels") == 0 && has_value) {
        cfg->channels = atoi(argv[++(*i)]);
        return TRUE;
    }
    if (g_strcmp0(arg, "--audio-frame-ms") == 0 && has_value) {
        cfg->frame_ms = g_ascii_strtod(argv[++(*i)], NULL);
        return TRUE;
    }
    if (g_strcmp0(arg, "--audio-bitrate") == 0 && has_value) {
        cfg->bitrate = atoi(argv[++(*i)]);
        return TRUE;
    }
    if (g_strcmp0(arg, "--audio-source") == 0 && has_value) {
        g_free(cfg->source);
        cfg->source = g_strdup(argv[++(*i)]);
        return TRUE;
    }
    if (g_strcmp0(arg, "--audio-device") == 0 && has_value) {
        g_free(cfg->device);
        cfg->device = g_strdup(argv[++(*i)]);
        return TRUE;
    }
    if (g_strcmp0(arg, "--audio-tone") == 0 && has_value) {
        cfg->tone_hz = atoi(argv[++(*i)]);
        return TRUE;
    }
    if (g_strcmp0(arg, "--audio-sink") == 0 && has_value) {
        g_free(cfg->sink);
        cfg->sink = g_strdup(argv[++(*i)]);
        return TRUE;
    }
    if (g_strcmp0(arg, "--audio-sink-buffer-ms") == 0 && has_value) {
        cfg->sink_buffer_ms = atoi(argv[++(*i)]);
        return TRUE;
    }
    if (g_strcmp0(arg, "--audio-latency") == 0 && has_value) {
        cfg->jb_latency_ms = atoi(argv[++(*i)]);
        return TRUE;
    }
    if (g_strcmp0(arg, "--audio-stats") == 0) {
        cfg->stats = TRUE;
        return TRUE;
    }
    if (g_strcmp0(arg, "--audio-jitter-mode") == 0 && has_value) {
        g_free(cfg->jitter_mode);
        cfg->jitter_mode = g_strdup(argv[++(*i)]);
        return TRUE;
    }
    return FALSE;
}

static void mp_audio_print_usage(FILE *stream) {
    g_fprintf(stream, "\nAudio (bidirectional, independent of video):\n");
    g_fprintf(stream, "  --audio <off|raw|opus>      Encoder (default: opus)\n");
    g_fprintf(stream, "                              raw = L16 PCM, no encoder delay, "
                      "~1.5 Mbit/s\n");
    g_fprintf(stream, "  --audio-duplex <both|send|recv|off>  Directions to run "
                      "(default: both)\n");
    g_fprintf(stream, "  --audio-source <test|device|none>    Capture source "
                      "(default: test tone)\n");
    g_fprintf(stream, "  --audio-device <name>       Capture device for "
                      "--audio-source device\n");
    g_fprintf(stream, "  --audio-tone <hz>           Test tone frequency\n");
    g_fprintf(stream, "  --audio-sink <element>      Playback sink, or fakesink "
                      "(default: auto)\n");
    g_fprintf(stream, "  --audio-rate <hz>           Sample rate (default: 48000)\n");
    g_fprintf(stream, "  --audio-channels <n>        Channels (default: 2)\n");
    g_fprintf(stream, "  --audio-frame-ms <ms>       Packet duration "
                      "(default: 10; opus: 2.5|5|10|20|40|60)\n");
    g_fprintf(stream, "  --audio-bitrate <bps>       Opus bitrate (default: 96000)\n");
    g_fprintf(stream, "  --audio-latency <ms>        Audio jitterbuffer depth "
                      "(default: 5)\n");
    g_fprintf(stream, "  --audio-jitter-mode <mode>  none|slave|buffer|synced "
                      "(default: none)\n");
    g_fprintf(stream, "  --audio-sink-buffer-ms <ms> Playback device buffer "
                      "(default: 30)\n");
    g_fprintf(stream, "  --audio-stats               Print per-second audio "
                      "statistics (default: quiet)\n");
}

// ---------------------------------------------------------------------------
// Per-stream statistics.
//
// Deliberately the same shape as the video figures so the two can be read
// against each other: one send-side number for "how long from existing to being
// on the wire", one receive-side number broken into stages.
// ---------------------------------------------------------------------------

typedef struct {
    GstClockTime pts;
    gint64 arrived_us;    // entered the jitterbuffer
    gint64 released_us;   // left the jitterbuffer, i.e. reached the depayloader
} MpAudioSlot;

typedef struct {
    const gchar *label;
    MpAudioCodec codec;
    gdouble packet_ms;
    // Raw L16 is split by the MTU rather than by the requested duration, and the
    // pieces are not all the same length, so packet_ms is an upper bound there.
    gboolean packet_bounded;
    gint bitrate;

    GMutex lock;

    // Send side
    GstElement *tx_element;     // for the pipeline clock and base time
    gint tx_packets;
    gint tx_measured;
    gint64 tx_sum_us;
    gint64 tx_max_us;
    gint tx_overruns;

    // Receive side
    guint32 arrival_rtp[MP_AUDIO_RING];
    gint64 arrival_us[MP_AUDIO_RING];
    MpAudioSlot slot[MP_AUDIO_RING];
    gboolean have_arrival_tap;
    gint rx_packets;
    gint rx_rendered;
    gint rx_measured;
    gint64 rx_sum_us;
    gint64 rx_max_us;
    gint64 rx_sum_jitter_us;
    gint64 rx_sum_decode_us;

    GstElement *jitterbuffer;   // borrowed, for lost/late counters
    guint64 last_pushed;
    guint64 last_lost;
    guint64 last_late;

    gint64 last_report_us;
} MpAudioStats;

static void mp_audio_stats_init(MpAudioStats *stats, const gchar *label,
                                const MpAudioConfig *cfg, gdouble packet_ms) {
    memset(stats, 0, sizeof(*stats));
    stats->label = label;
    stats->codec = cfg->codec;
    stats->packet_ms = packet_ms;
    stats->bitrate = cfg->bitrate;
    g_mutex_init(&stats->lock);
    stats->last_report_us = g_get_monotonic_time();
}

static void mp_audio_stats_clear(MpAudioStats *stats) {
    g_mutex_clear(&stats->lock);
}

// How long after a packet's own timestamp it left the payloader, measured
// against the pipeline clock. That covers capture buffering, encode and
// payloading. It is not directly comparable to the video figure, which is timed
// from an explicit push: the reference here is the timestamp the source chose,
// so a source that hands over a block of samples early can measure below the
// packet duration and a device that hands them over late measures above it.
static GstPadProbeReturn mp_audio_tx_probe(GstPad *pad, GstPadProbeInfo *info,
                                           gpointer user_data) {
    (void)pad;
    MpAudioStats *stats = (MpAudioStats *)user_data;
    if (!(GST_PAD_PROBE_INFO_TYPE(info) & GST_PAD_PROBE_TYPE_BUFFER)) {
        return GST_PAD_PROBE_OK;
    }
    GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    GstClockTime pts = GST_BUFFER_PTS(buffer);

    g_mutex_lock(&stats->lock);
    stats->tx_packets++;
    GstElement *element = stats->tx_element;
    g_mutex_unlock(&stats->lock);

    if (!GST_CLOCK_TIME_IS_VALID(pts) || !element) {
        return GST_PAD_PROBE_OK;
    }
    GstClock *clock = gst_element_get_clock(element);
    if (!clock) {
        return GST_PAD_PROBE_OK;
    }
    GstClockTime base = gst_element_get_base_time(element);
    GstClockTime now = gst_clock_get_time(clock);
    gst_object_unref(clock);
    if (!GST_CLOCK_TIME_IS_VALID(now) || now < base) {
        return GST_PAD_PROBE_OK;
    }
    GstClockTime running = now - base;
    if (running < pts) {
        return GST_PAD_PROBE_OK;
    }
    gint64 elapsed_us = (gint64)((running - pts) / GST_USECOND);

    g_mutex_lock(&stats->lock);
    stats->tx_measured++;
    stats->tx_sum_us += elapsed_us;
    if (elapsed_us > stats->tx_max_us) {
        stats->tx_max_us = elapsed_us;
    }
    // A packet that took more than two packet periods to appear means the
    // capture or encode step stalled, not that the packet was simply full.
    if (elapsed_us > (gint64)(stats->packet_ms * 2000.0)) {
        stats->tx_overruns++;
    }
    g_mutex_unlock(&stats->lock);
    return GST_PAD_PROBE_OK;
}

// Arrival, keyed by RTP timestamp, before any buffering.
static GstPadProbeReturn mp_audio_arrival_probe(GstPad *pad, GstPadProbeInfo *info,
                                               gpointer user_data) {
    (void)pad;
    MpAudioStats *stats = (MpAudioStats *)user_data;
    if (!(GST_PAD_PROBE_INFO_TYPE(info) & GST_PAD_PROBE_TYPE_BUFFER)) {
        return GST_PAD_PROBE_OK;
    }
    guint32 rtp_ts;
    if (mp_rtp_timestamp_of(GST_PAD_PROBE_INFO_BUFFER(info), &rtp_ts)) {
        guint slot = rtp_ts % MP_AUDIO_RING;
        g_mutex_lock(&stats->lock);
        if (stats->arrival_rtp[slot] != rtp_ts) {
            stats->arrival_rtp[slot] = rtp_ts;
            stats->arrival_us[slot] = g_get_monotonic_time();
        }
        g_mutex_unlock(&stats->lock);
    }
    return GST_PAD_PROBE_OK;
}

// Release from the jitterbuffer. Carries the arrival time into a PTS-keyed slot,
// because PTS is what survives depayloading and decoding.
static GstPadProbeReturn mp_audio_release_probe(GstPad *pad, GstPadProbeInfo *info,
                                                gpointer user_data) {
    (void)pad;
    MpAudioStats *stats = (MpAudioStats *)user_data;
    if (!(GST_PAD_PROBE_INFO_TYPE(info) & GST_PAD_PROBE_TYPE_BUFFER)) {
        return GST_PAD_PROBE_OK;
    }
    GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    GstClockTime pts = GST_BUFFER_PTS(buffer);
    guint32 rtp_ts = 0;
    gboolean have_rtp = mp_rtp_timestamp_of(buffer, &rtp_ts);
    if (!GST_CLOCK_TIME_IS_VALID(pts)) {
        return GST_PAD_PROBE_OK;
    }
    gint64 now = g_get_monotonic_time();

    g_mutex_lock(&stats->lock);
    stats->rx_packets++;
    MpAudioSlot *slot = &stats->slot[(pts / GST_MSECOND) % MP_AUDIO_RING];
    if (slot->pts != pts) {
        slot->pts = pts;
        slot->released_us = now;
        slot->arrived_us = now;
        if (have_rtp) {
            guint rtp_slot = rtp_ts % MP_AUDIO_RING;
            if (stats->arrival_rtp[rtp_slot] == rtp_ts && stats->arrival_us[rtp_slot] > 0) {
                slot->arrived_us = stats->arrival_us[rtp_slot];
            }
        }
    }
    g_mutex_unlock(&stats->lock);
    return GST_PAD_PROBE_OK;
}

// Handover to the playback sink. Fires when the buffer reaches the sink, before
// the sink waits on the clock to render it - the same convention the video
// figures use, so neither number includes the deliberate wait for a
// presentation time.
static GstPadProbeReturn mp_audio_sink_probe(GstPad *pad, GstPadProbeInfo *info,
                                             gpointer user_data) {
    (void)pad;
    MpAudioStats *stats = (MpAudioStats *)user_data;
    if (!(GST_PAD_PROBE_INFO_TYPE(info) & GST_PAD_PROBE_TYPE_BUFFER)) {
        return GST_PAD_PROBE_OK;
    }
    GstClockTime pts = GST_BUFFER_PTS(GST_PAD_PROBE_INFO_BUFFER(info));
    gint64 now = g_get_monotonic_time();

    g_mutex_lock(&stats->lock);
    stats->rx_rendered++;
    if (GST_CLOCK_TIME_IS_VALID(pts)) {
        MpAudioSlot *slot = &stats->slot[(pts / GST_MSECOND) % MP_AUDIO_RING];
        if (slot->pts == pts && slot->arrived_us > 0) {
            stats->rx_measured++;
            stats->rx_sum_jitter_us += slot->released_us - slot->arrived_us;
            stats->rx_sum_decode_us += now - slot->released_us;
            gint64 total = now - slot->arrived_us;
            stats->rx_sum_us += total;
            if (total > stats->rx_max_us) {
                stats->rx_max_us = total;
            }
        }
    }
    g_mutex_unlock(&stats->lock);
    return GST_PAD_PROBE_OK;
}

static void mp_audio_tap(GstElement *element, const gchar *pad_name,
                         GstPadProbeCallback callback, MpAudioStats *stats) {
    if (!element) {
        return;
    }
    GstPad *pad = gst_element_get_static_pad(element, pad_name);
    if (pad) {
        gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER, callback, stats, NULL);
        gst_object_unref(pad);
    }
}

static gboolean mp_audio_report_tx(gpointer user_data) {
    MpAudioStats *stats = (MpAudioStats *)user_data;
    gint64 now = g_get_monotonic_time();
    gdouble seconds = (now - stats->last_report_us) / (gdouble)G_USEC_PER_SEC;
    stats->last_report_us = now;

    g_mutex_lock(&stats->lock);
    gint packets = stats->tx_packets;
    gint measured = stats->tx_measured;
    gint64 sum = stats->tx_sum_us;
    gint64 max = stats->tx_max_us;
    gint overruns = stats->tx_overruns;
    stats->tx_packets = 0;
    stats->tx_measured = 0;
    stats->tx_sum_us = 0;
    stats->tx_max_us = 0;
    stats->tx_overruns = 0;
    g_mutex_unlock(&stats->lock);

    // Deliberately does not read "Audio sent 0.0 pkt/s": the same convention the
    // video lines follow, so a "nothing is happening" line cannot be mistaken for
    // a measurement of zero by anything grepping the output.
    if (packets <= 0 || seconds <= 0.0) {
        g_print("No audio captured in the last %.1f s\n", seconds);
        return G_SOURCE_CONTINUE;
    }
    if (measured > 0) {
        g_print("Audio sent %.1f pkt/s | capture->wire avg %.1f ms max %.1f ms | "
                "packet %s%.1f ms | codec %s | overruns %d\n",
                packets / seconds, (sum / (gdouble)measured) / 1000.0, max / 1000.0,
                stats->packet_bounded ? "<=" : "", stats->packet_ms,
                mp_audio_codec_name(stats->codec), overruns);
    } else {
        g_print("Audio sent %.1f pkt/s | codec %s\n", packets / seconds,
                mp_audio_codec_name(stats->codec));
    }
    return G_SOURCE_CONTINUE;
}

static gboolean mp_audio_report_rx(gpointer user_data) {
    MpAudioStats *stats = (MpAudioStats *)user_data;
    gint64 now = g_get_monotonic_time();
    gdouble seconds = (now - stats->last_report_us) / (gdouble)G_USEC_PER_SEC;
    stats->last_report_us = now;

    g_mutex_lock(&stats->lock);
    gint packets = stats->rx_packets;
    gint rendered = stats->rx_rendered;
    gint measured = stats->rx_measured;
    gdouble total_ms = measured ? stats->rx_sum_us / (gdouble)measured / 1000.0 : 0.0;
    gdouble max_ms = stats->rx_max_us / 1000.0;
    gdouble jitter_ms = measured ? stats->rx_sum_jitter_us / (gdouble)measured / 1000.0 : 0.0;
    gdouble decode_ms = measured ? stats->rx_sum_decode_us / (gdouble)measured / 1000.0 : 0.0;
    stats->rx_packets = 0;
    stats->rx_rendered = 0;
    stats->rx_measured = 0;
    stats->rx_sum_us = 0;
    stats->rx_max_us = 0;
    stats->rx_sum_jitter_us = 0;
    stats->rx_sum_decode_us = 0;
    GstElement *jitterbuffer = stats->jitterbuffer;
    g_mutex_unlock(&stats->lock);

    gchar *counters = NULL;
    if (jitterbuffer) {
        GstStructure *jb_stats = NULL;
        g_object_get(jitterbuffer, "stats", &jb_stats, NULL);
        if (jb_stats) {
            guint64 pushed = 0, lost = 0, late = 0;
            gst_structure_get_uint64(jb_stats, "num-pushed", &pushed);
            gst_structure_get_uint64(jb_stats, "num-lost", &lost);
            gst_structure_get_uint64(jb_stats, "num-late", &late);
            counters = g_strdup_printf(" | pkts=%" G_GUINT64_FORMAT
                                       " lost=%" G_GUINT64_FORMAT
                                       " late=%" G_GUINT64_FORMAT,
                                       pushed - stats->last_pushed,
                                       lost - stats->last_lost,
                                       late - stats->last_late);
            stats->last_pushed = pushed;
            stats->last_lost = lost;
            stats->last_late = late;
            gst_structure_free(jb_stats);
        }
    }

    if (packets <= 0 || seconds <= 0.0) {
        g_print("No audio received - waiting for the audio stream%s\n",
                counters ? counters : "");
    } else if (measured > 0) {
        g_print("Audio recv %.1f pkt/s | arrival->sink %.1f ms (max %.1f) = "
                "jitterbuf %.1f + decode %.1f | codec %s%s\n",
                packets / seconds, total_ms, max_ms, jitter_ms, decode_ms,
                mp_audio_codec_name(stats->codec), counters ? counters : "");
    } else {
        g_print("Audio recv %.1f pkt/s (%d to the sink) | codec %s%s\n",
                packets / seconds, rendered, mp_audio_codec_name(stats->codec),
                counters ? counters : "");
    }
    g_free(counters);
    return G_SOURCE_CONTINUE;
}

// ---------------------------------------------------------------------------
// Capture and encode
// ---------------------------------------------------------------------------

typedef struct {
    GstElement *source;
    GstElement *convert;
    GstElement *resample;
    GstElement *rawcaps;
    GstElement *encoder;      // NULL for raw
    GstElement *pay;
    GstElement *rtpcaps;
    GstElement *gate;         // tail: link downstream from here
    gdouble packet_ms;
    gboolean packet_bounded;
} MpAudioSend;

static GstElement* mp_audio_make_source(const MpAudioConfig *cfg) {
    if (g_strcmp0(cfg->source, "device") == 0) {
        const gchar *const candidates[] = {"pulsesrc", "alsasrc", "autoaudiosrc", NULL};
        const gchar *chosen = NULL;
        GstElement *source = mp_make_first(candidates, "audiosrc", &chosen);
        if (!source) {
            g_printerr("No audio capture element available\n");
            return NULL;
        }
        if (cfg->device) {
            mp_set_string_prop(source, "device", cfg->device);
        }
        // Ask the device for one packet at a time. Capture buffering is added to
        // every millisecond the stream will ever have, so it is the first thing
        // to keep small.
        mp_set_number_prop(source, "latency-time", (gint64)(cfg->frame_ms * 1000.0));
        mp_set_number_prop(source, "buffer-time",
                           (gint64)MAX(cfg->frame_ms * 4.0, 20.0) * 1000);
        // Leave the pipeline on the system clock: the send-side measurement
        // compares buffer timestamps against it, and a soundcard clock that
        // drifts would make that comparison meaningless.
        mp_set_bool_prop(source, "provide-clock", FALSE);
        g_print("Audio capture: %s%s%s\n", chosen, cfg->device ? " device=" : "",
                cfg->device ? cfg->device : "");
        return source;
    }

    GstElement *source = gst_element_factory_make("audiotestsrc", "audiosrc");
    if (!source) {
        g_printerr("audiotestsrc is unavailable\n");
        return NULL;
    }
    mp_set_bool_prop(source, "is-live", TRUE);
    mp_set_number_prop(source, "samplesperbuffer",
                       (gint64)(cfg->rate * cfg->frame_ms / 1000.0));
    mp_set_number_prop(source, "freq", cfg->tone_hz);
    if (mp_find_prop(source, "volume")) {
        // Loud enough to hear and to see on a meter, quiet enough not to be
        // startling on a headset.
        g_object_set(source, "volume", 0.2, NULL);
    }
    g_print("Audio capture: audiotestsrc %d Hz tone (pass --audio-source device "
            "for a microphone)\n", cfg->tone_hz);
    return source;
}

static gboolean mp_audio_build_send(GstBin *bin, const MpAudioConfig *cfg,
                                    MpAudioSend *out) {
    memset(out, 0, sizeof(*out));
    gint clock_rate = mp_audio_clock_rate(cfg);
    gint mtu = 1400;

    out->source = mp_audio_make_source(cfg);
    out->convert = gst_element_factory_make("audioconvert", "audioconvert");
    out->resample = gst_element_factory_make("audioresample", "audioresample");
    out->rawcaps = gst_element_factory_make("capsfilter", "audiorawcaps");
    if (!out->source || !out->convert || !out->resample || !out->rawcaps) {
        g_printerr("Failed to create the audio capture chain\n");
        return FALSE;
    }

    // L16 is defined as big-endian on the wire, and rtpL16pay will only accept
    // S16BE. Opus takes native-endian 16-bit.
    GstCaps *raw_caps = gst_caps_new_simple(
        "audio/x-raw",
        "format", G_TYPE_STRING, cfg->codec == MP_AUDIO_RAW ? "S16BE" : "S16LE",
        "layout", G_TYPE_STRING, "interleaved",
        "rate", G_TYPE_INT, clock_rate,
        "channels", G_TYPE_INT, cfg->channels, NULL);
    g_object_set(out->rawcaps, "caps", raw_caps, NULL);
    gst_caps_unref(raw_caps);

    out->packet_ms = cfg->frame_ms;

    if (cfg->codec == MP_AUDIO_RAW) {
        out->pay = gst_element_factory_make("rtpL16pay", "audiopay");
        if (!out->pay) {
            g_printerr("rtpL16pay is unavailable\n");
            return FALSE;
        }
        mp_set_number_prop(out->pay, "mtu", mtu);
        mp_set_number_prop(out->pay, "max-ptime",
                           (gint64)(cfg->frame_ms * (gdouble)GST_MSECOND));
        gdouble mtu_ms = mp_audio_mtu_limited_ms(cfg, mtu);
        if (mtu_ms < cfg->frame_ms) {
            out->packet_ms = mtu_ms;
            out->packet_bounded = TRUE;
            g_print("Audio raw: %d Hz x %d ch is %.0f bytes/ms, so an MTU of %d "
                    "caps the packet at %.1f ms (asked for %.1f); the payloader "
                    "splits each buffer, so packets are shorter still\n",
                    clock_rate, cfg->channels,
                    (gdouble)cfg->rate * cfg->channels * 2 / 1000.0, mtu,
                    mtu_ms, cfg->frame_ms);
        }
    } else {
        out->encoder = gst_element_factory_make("opusenc", "audioenc");
        out->pay = gst_element_factory_make("rtpopuspay", "audiopay");
        if (!out->encoder || !out->pay) {
            g_printerr("opusenc/rtpopuspay are unavailable; install "
                       "gstreamer1.0-plugins-base\n");
            return FALSE;
        }
        mp_set_number_prop(out->encoder, "bitrate", cfg->bitrate);
        // Opus defaults suit music streaming, not conversation. Every one of
        // these otherwise buys bitrate or resilience with delay.
        const gchar *const cbr[] = {"cbr", NULL};
        mp_set_enum_prop(out->encoder, "bitrate-type", cbr);
        // 1.16 only offers "generic" and "voice"; "voice" is Opus's VoIP mode,
        // which is the low-delay one. Newer releases add restricted-lowdelay and
        // it is preferred where present.
        const gchar *const lowdelay[] = {"restricted-lowdelay", "voice", "generic", NULL};
        const gchar *audio_type = mp_set_enum_prop(out->encoder, "audio-type", lowdelay);
        gchar *frame = g_strdup_printf("%g", cfg->frame_ms);
        const gchar *const frames[] = {frame, "10", NULL};
        const gchar *applied_frame = mp_set_enum_prop(out->encoder, "frame-size", frames);
        mp_set_bool_prop(out->encoder, "dtx", FALSE);
        mp_set_bool_prop(out->encoder, "inband-fec", FALSE);
        // Lookahead-free complexity: the top settings cost CPU for bitrate
        // efficiency this link does not need.
        mp_set_number_prop(out->encoder, "complexity", 5);
        g_print("Audio encoder: opusenc %d bit/s, frame-size=%s ms, type=%s\n",
                cfg->bitrate, applied_frame ? applied_frame : "default",
                audio_type ? audio_type : "default");
        g_free(frame);
        mp_set_number_prop(out->pay, "mtu", mtu);
    }

    mp_set_number_prop(out->pay, "pt", mp_audio_payload_type(cfg->codec));

    // Pin the RTP caps. webrtcbin builds its SDP from these, and without
    // encoding-params the rtpmap line loses the channel count - the far end then
    // depayloads a stereo stream as mono.
    out->rtpcaps = gst_element_factory_make("capsfilter", "audiortpcaps");
    if (!out->rtpcaps) {
        return FALSE;
    }
    gchar *params = g_strdup_printf("%d", cfg->channels);
    GstCaps *rtp_caps = gst_caps_new_simple(
        "application/x-rtp",
        "media", G_TYPE_STRING, "audio",
        "encoding-name", G_TYPE_STRING, mp_audio_encoding_name(cfg->codec),
        "payload", G_TYPE_INT, mp_audio_payload_type(cfg->codec),
        "clock-rate", G_TYPE_INT, clock_rate,
        "encoding-params", G_TYPE_STRING, params, NULL);
    if (cfg->codec == MP_AUDIO_RAW) {
        gst_caps_set_simple(rtp_caps, "channels", G_TYPE_INT, cfg->channels, NULL);
    }
    g_object_set(out->rtpcaps, "caps", rtp_caps, NULL);
    gst_caps_unref(rtp_caps);
    g_free(params);

    // Last stage before the transport, and the reason it is here: audio is live,
    // so if the transport ever stops accepting packets the right answer is to
    // throw the stale ones away, not to queue them. Queueing them back-pressures
    // the capture thread, and worse, the burst that escapes when the transport
    // reopens is what sets the far end's jitterbuffer timestamp mapping - which
    // then holds every subsequent packet by the length of that burst, for the
    // rest of the session. Measured at 241 ms of permanent audio delay before
    // this was here.
    out->gate = gst_element_factory_make("queue", "audiogate");
    if (!out->gate) {
        return FALSE;
    }
    g_object_set(out->gate, "leaky", 2 /* downstream: drop the oldest */,
                 "max-size-buffers", 2, "max-size-time", (guint64)0,
                 "max-size-bytes", (guint)0, NULL);

    GstElement *chain[9];
    gint n = 0;
    chain[n++] = out->source;
    chain[n++] = out->convert;
    chain[n++] = out->resample;
    chain[n++] = out->rawcaps;
    if (out->encoder) chain[n++] = out->encoder;
    chain[n++] = out->pay;
    chain[n++] = out->rtpcaps;
    chain[n++] = out->gate;

    for (gint i = 0; i < n; i++) {
        gst_bin_add(bin, chain[i]);
    }
    for (gint i = 0; i < n - 1; i++) {
        if (!gst_element_link(chain[i], chain[i + 1])) {
            g_printerr("Failed to link %s -> %s in the audio send chain\n",
                       GST_ELEMENT_NAME(chain[i]), GST_ELEMENT_NAME(chain[i + 1]));
            return FALSE;
        }
    }
    return TRUE;
}

// ---------------------------------------------------------------------------
// Decode and play out
// ---------------------------------------------------------------------------

typedef struct {
    GstElement *depay;
    GstElement *decoder;      // NULL for raw
    GstElement *convert;
    GstElement *resample;
    GstElement *sink;
} MpAudioRecv;

static GstElement* mp_audio_make_sink(const MpAudioConfig *cfg) {
    GstElement *sink = NULL;
    const gchar *chosen = NULL;
    if (g_strcmp0(cfg->sink, "auto") == 0) {
        // Named explicitly rather than through autoaudiosink, which does not
        // proxy buffer-time - the property that decides how much of the latency
        // budget the playback device takes.
        const gchar *const candidates[] = {"pulsesink", "alsasink", "autoaudiosink", NULL};
        sink = mp_make_first(candidates, "audiosink", &chosen);
    } else {
        sink = gst_element_factory_make(cfg->sink, "audiosink");
        chosen = cfg->sink;
    }
    if (!sink) {
        g_printerr("Audio sink '%s' is unavailable\n", cfg->sink);
        return NULL;
    }
    mp_set_bool_prop(sink, "sync", TRUE);
    mp_set_number_prop(sink, "buffer-time", (gint64)cfg->sink_buffer_ms * 1000);
    mp_set_number_prop(sink, "latency-time",
                       (gint64)MIN(cfg->frame_ms, 10.0) * 1000);
    mp_set_bool_prop(sink, "provide-clock", FALSE);
    gboolean relaxed = FALSE;
    mp_relax_sink_dropping(sink, &relaxed);
    g_print("Audio playback: %s, device buffer %d ms\n", chosen, cfg->sink_buffer_ms);
    return sink;
}

// `encoding` overrides the configured codec when the far end told us what it is
// actually sending, which is what the WebRTC receive side has and the UDP
// receive side does not.
static gboolean mp_audio_build_recv(GstBin *bin, const MpAudioConfig *cfg,
                                    const gchar *encoding, MpAudioRecv *out) {
    memset(out, 0, sizeof(*out));
    gboolean raw = (cfg->codec == MP_AUDIO_RAW);
    if (encoding) {
        raw = (g_ascii_strcasecmp(encoding, "L16") == 0);
    }

    out->depay = gst_element_factory_make(raw ? "rtpL16depay" : "rtpopusdepay",
                                          "audiodepay");
    if (!raw) {
        out->decoder = gst_element_factory_make("opusdec", "audiodec");
    }
    out->convert = gst_element_factory_make("audioconvert", "audioconvert_out");
    out->resample = gst_element_factory_make("audioresample", "audioresample_out");
    out->sink = mp_audio_make_sink(cfg);

    if (!out->depay || (!raw && !out->decoder) || !out->convert || !out->resample ||
        !out->sink) {
        g_printerr("Failed to create the audio receive chain\n");
        return FALSE;
    }
    if (out->decoder) {
        // Conceal a lost packet rather than leaving a hole; with inband-fec off
        // on the sender this is plain interpolation and costs nothing.
        mp_set_bool_prop(out->decoder, "plc", TRUE);
        mp_set_bool_prop(out->decoder, "use-inband-fec", FALSE);
    }

    GstElement *chain[6];
    gint n = 0;
    chain[n++] = out->depay;
    if (out->decoder) chain[n++] = out->decoder;
    chain[n++] = out->convert;
    chain[n++] = out->resample;
    chain[n++] = out->sink;

    for (gint i = 0; i < n; i++) {
        gst_bin_add(bin, chain[i]);
    }
    for (gint i = 0; i < n - 1; i++) {
        if (!gst_element_link(chain[i], chain[i + 1])) {
            g_printerr("Failed to link %s -> %s in the audio receive chain\n",
                       GST_ELEMENT_NAME(chain[i]), GST_ELEMENT_NAME(chain[i + 1]));
            return FALSE;
        }
    }
    return TRUE;
}

// ---------------------------------------------------------------------------
// RTP/UDP pipelines
// ---------------------------------------------------------------------------

static gboolean mp_audio_udp_send_pipeline(MpWorker *worker, const MpAudioConfig *cfg,
                                           const gchar *host, gint port,
                                           MpSrtpKey *srtp, MpAudioStats *stats) {
    GstElement *pipeline = gst_pipeline_new("audio-send");
    MpAudioSend send;
    if (!mp_audio_build_send(GST_BIN(pipeline), cfg, &send)) {
        gst_object_unref(pipeline);
        return FALSE;
    }

    GstElement *udpsink = gst_element_factory_make("udpsink", "audioudpsink");
    if (!udpsink) {
        gst_object_unref(pipeline);
        return FALSE;
    }
    // The live source paces this stream; a sink that also waited on the clock
    // would add a second scheduling step for nothing.
    g_object_set(udpsink, "host", host, "port", port, "sync", FALSE,
                 "async", FALSE, NULL);
    gst_bin_add(GST_BIN(pipeline), udpsink);

    GstElement *tail = send.gate;
    if (srtp && srtp->enabled) {
        GstPad *src = mp_srtp_insert_sender(GST_BIN(pipeline), srtp, tail, "audiosrtpenc");
        if (!src) {
            gst_object_unref(pipeline);
            return FALSE;
        }
        GstPad *sink = gst_element_get_static_pad(udpsink, "sink");
        if (gst_pad_link(src, sink) != GST_PAD_LINK_OK) {
            g_printerr("Failed to link srtpenc to the audio udpsink\n");
            gst_object_unref(src);
            gst_object_unref(sink);
            gst_object_unref(pipeline);
            return FALSE;
        }
        gst_object_unref(src);
        gst_object_unref(sink);
    } else if (!gst_element_link(tail, udpsink)) {
        g_printerr("Failed to link the audio payloader to the udpsink\n");
        gst_object_unref(pipeline);
        return FALSE;
    }

    mp_audio_stats_init(stats, "audio-send", cfg, send.packet_ms);
    stats->packet_bounded = send.packet_bounded;
    stats->tx_element = send.gate;

    mp_worker_set_pipeline(worker, pipeline, NULL, NULL);
    if (cfg->stats) {
        mp_audio_tap(send.gate, "src", mp_audio_tx_probe, stats);
        mp_worker_add_timeout(worker, 1000, mp_audio_report_tx, stats);
    }
    g_print("Audio out: %s to %s:%d%s\n", mp_audio_codec_name(cfg->codec), host, port,
            (srtp && srtp->enabled) ? " (SRTP)" : "");
    return TRUE;
}

static gboolean mp_audio_udp_recv_pipeline(MpWorker *worker, const MpAudioConfig *cfg,
                                           const gchar *bind_host, gint port,
                                           MpSrtpKey *srtp, MpAudioStats *stats) {
    GstElement *pipeline = gst_pipeline_new("audio-recv");
    gint clock_rate = mp_audio_clock_rate(cfg);
    gboolean encrypted = (srtp && srtp->enabled);

    GstElement *udpsrc = gst_element_factory_make("udpsrc", "audioudpsrc");
    GstElement *capsfilter = gst_element_factory_make("capsfilter", "audioinputcaps");
    // A jitterbuffer is not optional even at mode=none: it pushes from its own
    // thread, which is what keeps the socket drained while the previous packet
    // is being decoded.
    GstElement *jitterbuffer = gst_element_factory_make("rtpjitterbuffer", "audiojb");
    if (!udpsrc || !capsfilter || !jitterbuffer) {
        g_printerr("Failed to create the audio receive front end\n");
        gst_object_unref(pipeline);
        return FALSE;
    }

    g_object_set(udpsrc, "address", bind_host, "port", port, NULL);

    gchar *params = g_strdup_printf("%d", cfg->channels);
    GstCaps *caps = gst_caps_new_simple(
        encrypted ? "application/x-srtp" : "application/x-rtp",
        "media", G_TYPE_STRING, "audio",
        "encoding-name", G_TYPE_STRING, mp_audio_encoding_name(cfg->codec),
        "payload", G_TYPE_INT, mp_audio_payload_type(cfg->codec),
        "clock-rate", G_TYPE_INT, clock_rate,
        "encoding-params", G_TYPE_STRING, params,
        "channels", G_TYPE_INT, cfg->channels, NULL);
    g_object_set(capsfilter, "caps", caps, NULL);
    gst_caps_unref(caps);
    g_free(params);

    g_object_set(jitterbuffer, "latency", (guint)cfg->jb_latency_ms, NULL);
    mp_set_bool_prop(jitterbuffer, "do-lost", TRUE);
    if (mp_find_prop(jitterbuffer, "mode")) {
        gst_util_set_object_arg(G_OBJECT(jitterbuffer), "mode", cfg->jitter_mode);
    }

    MpAudioRecv recv;
    if (!mp_audio_build_recv(GST_BIN(pipeline), cfg, NULL, &recv)) {
        gst_object_unref(pipeline);
        return FALSE;
    }

    gst_bin_add_many(GST_BIN(pipeline), udpsrc, capsfilter, jitterbuffer, NULL);
    if (!gst_element_link(udpsrc, capsfilter)) {
        g_printerr("Failed to link the audio udpsrc\n");
        gst_object_unref(pipeline);
        return FALSE;
    }

    if (encrypted) {
        GstElement *srtpdec = mp_srtp_make_receiver(GST_BIN(pipeline), srtp, "audiosrtpdec");
        if (!srtpdec ||
            !gst_element_link_pads(capsfilter, "src", srtpdec, "rtp_sink") ||
            !gst_element_link_pads(srtpdec, "rtp_src", jitterbuffer, "sink")) {
            g_printerr("Failed to link srtpdec into the audio receive chain\n");
            gst_object_unref(pipeline);
            return FALSE;
        }
    } else if (!gst_element_link(capsfilter, jitterbuffer)) {
        g_printerr("Failed to link the audio jitterbuffer\n");
        gst_object_unref(pipeline);
        return FALSE;
    }
    if (!gst_element_link(jitterbuffer, recv.depay)) {
        g_printerr("Failed to link the audio jitterbuffer to the depayloader\n");
        gst_object_unref(pipeline);
        return FALSE;
    }

    mp_audio_stats_init(stats, "audio-recv", cfg, cfg->frame_ms);
    stats->jitterbuffer = jitterbuffer;

    mp_worker_set_pipeline(worker, pipeline, NULL, NULL);
    if (cfg->stats) {
        stats->have_arrival_tap = TRUE;
        mp_audio_tap(udpsrc, "src", mp_audio_arrival_probe, stats);
        mp_audio_tap(recv.depay, "sink", mp_audio_release_probe, stats);
        mp_audio_tap(recv.sink, "sink", mp_audio_sink_probe, stats);
        mp_worker_add_timeout(worker, 1000, mp_audio_report_rx, stats);
    }
    g_print("Audio in: %s on %s:%d%s, jitterbuffer %d ms mode=%s\n",
            mp_audio_codec_name(cfg->codec), bind_host, port,
            encrypted ? " (SRTP)" : "", cfg->jb_latency_ms, cfg->jitter_mode);
    return TRUE;
}

// ---------------------------------------------------------------------------
// WebRTC branches. The pipeline and webrtcbin belong to the caller; only the
// audio-specific wiring lives here.
// ---------------------------------------------------------------------------

// `stats` must already be initialised: on the receive side the jitterbuffer hook
// can fire before the chain is built, so initialising it in here would wipe what
// that hook recorded.
// `out_tail` receives the last element before webrtcbin, so the caller can gate
// it on the connection being up.
static gboolean mp_audio_webrtc_add_sender(GstBin *bin, const MpAudioConfig *cfg,
                                           GstElement *webrtcbin,
                                           MpAudioStats *stats,
                                           GstElement **out_tail) {
    MpAudioSend send;
    if (!mp_audio_build_send(bin, cfg, &send)) {
        return FALSE;
    }

    GstPad *src = gst_element_get_static_pad(send.gate, "src");
    GstPad *sink = gst_element_request_pad_simple(webrtcbin, "sink_%u");
    if (!src || !sink || gst_pad_link(src, sink) != GST_PAD_LINK_OK) {
        g_printerr("Failed to link the audio payloader into webrtcbin\n");
        if (src) gst_object_unref(src);
        if (sink) gst_object_unref(sink);
        return FALSE;
    }

    g_mutex_lock(&stats->lock);
    stats->packet_ms = send.packet_ms;
    stats->packet_bounded = send.packet_bounded;
    stats->tx_element = send.gate;
    g_mutex_unlock(&stats->lock);
    if (cfg->stats) {
        gst_pad_add_probe(src, GST_PAD_PROBE_TYPE_BUFFER, mp_audio_tx_probe, stats,
                          NULL);
    }
    gst_object_unref(src);
    gst_object_unref(sink);
    if (out_tail) {
        *out_tail = send.gate;
    }
    return TRUE;
}

// Called from webrtcbin's pad-added, so the chain has to be brought up to the
// pipeline's state by hand.
static gboolean mp_audio_webrtc_add_receiver(GstBin *bin, const MpAudioConfig *cfg,
                                             GstPad *pad, MpAudioStats *stats) {
    GstCaps *caps = gst_pad_get_current_caps(pad);
    if (!caps) {
        caps = gst_pad_query_caps(pad, NULL);
    }
    GstStructure *structure = caps ? gst_caps_get_structure(caps, 0) : NULL;
    const gchar *encoding = structure
        ? gst_structure_get_string(structure, "encoding-name") : NULL;
    gchar *encoding_owned = g_strdup(encoding);
    if (caps) {
        gst_caps_unref(caps);
    }

    MpAudioRecv recv;
    gboolean ok = mp_audio_build_recv(bin, cfg, encoding_owned, &recv);
    if (!ok) {
        g_free(encoding_owned);
        return FALSE;
    }

    if (cfg->stats) {
        mp_audio_tap(recv.depay, "sink", mp_audio_release_probe, stats);
        mp_audio_tap(recv.sink, "sink", mp_audio_sink_probe, stats);
    }

    gst_element_sync_state_with_parent(recv.depay);
    if (recv.decoder) {
        gst_element_sync_state_with_parent(recv.decoder);
    }
    gst_element_sync_state_with_parent(recv.convert);
    gst_element_sync_state_with_parent(recv.resample);
    gst_element_sync_state_with_parent(recv.sink);

    GstPad *depay_sink = gst_element_get_static_pad(recv.depay, "sink");
    if (gst_pad_link(pad, depay_sink) != GST_PAD_LINK_OK) {
        g_printerr("Failed to link webrtcbin to the audio depayloader\n");
        ok = FALSE;
    } else {
        g_print("Incoming audio stream: %s\n",
                encoding_owned ? encoding_owned : "unknown");
    }
    gst_object_unref(depay_sink);
    g_free(encoding_owned);
    return ok;
}

#endif  // AUDIO_STREAM_H

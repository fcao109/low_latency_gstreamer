// RTP/UDP receiver.
//
// Video is unchanged from the version this project's latency figures were
// measured with, including the explicit software decoder and the walk into the
// sink to stop it dropping frames.
//
// Three streams now run, each as its own pipeline on its own thread (see
// media_worker.h) with nothing synchronised between them:
//
//   video       - in,  port 5000
//   audio-down  - in,  port 5002 (the audio from the sending side)
//   audio-up    - out, port 5004 (this side's microphone, going back)
//
// With --pki-* supplied, every stream is SRTP and this side listens on a
// mutually authenticated TLS channel (port 5010) for the sender's master keys,
// handing over its own in return. Without those options this is plain RTP
// exactly as before, and nothing blocks waiting for a channel.

#include <gst/gst.h>
#include <glib.h>
#include <glib-unix.h>
#include <glib/gprintf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio_stream.h"
#include "media_pki.h"
#include "media_worker.h"
#include "signal_channel.h"

// Ring of recent RTP arrival times. Only has to span the frames in flight between
// the socket and the sink.
#define LAT_RING 512

// Timestamps collected as one frame moves through the receive path, so the delay
// can be attributed to a stage instead of guessed at.
typedef struct {
    GstClockTime pts;
    gint64 arrived_us;   // first RTP packet hit the socket
    gint64 depayed_us;   // released by the jitterbuffer
    gint64 parsed_us;    // handed to the decoder
    gint64 decoded_us;   // decoder produced the picture
} LatencyStages;

typedef struct {
    gchar *host;
    gint port;
    gchar *codec;
    gint latency;
    gchar *jitter_mode;
    gint buffer_size;
    gboolean hw_decode;
    gboolean sink_tuned;
    GstElement *pipeline;

    // Audio, both directions, and the encryption that now covers all of it.
    MpAudioConfig audio;
    gchar *peer_host;        // where the returning audio is sent
    gint audio_port;         // incoming audio
    gint audio_back_port;    // outgoing audio
    gint key_port;
    MpPki *pki;
    MpSrtpKey srtp_video;
    MpSrtpKey srtp_audio_down;
    MpSrtpKey srtp_audio_up;
    MpSignal *keying;

    MpWorker *video_worker;
    MpWorker *audio_tx_worker;
    MpWorker *audio_rx_worker;
    MpAudioStats audio_tx_stats;
    MpAudioStats audio_rx_stats;
    GMainLoop *supervisor;

    gint frame_count;
    gint last_reported_count;
    gint64 last_report_time;

    // Receive-to-render latency. The arrival time of each frame's first RTP packet
    // is recorded against its RTP timestamp, then matched up again when the
    // decoded frame reaches the sink. Reading the delay off a photograph cannot
    // resolve better than a display refresh, so it is measured here instead.
    GMutex lat_lock;
    // Keyed by RTP timestamp: when this frame's first packet reached the socket.
    guint32 arrival_rtp_ts[LAT_RING];
    gint64 arrival_us[LAT_RING];
    // Keyed by presentation timestamp, which survives depayloading and decoding
    // and so is what links a decoded frame back to its arrival.
    LatencyStages stage[LAT_RING];
    gint lat_frames;
    gint64 sum_jitter_us;
    gint64 sum_parse_us;
    gint64 sum_decode_us;
    gint64 sum_convert_us;
    gint64 sum_total_us;
    gint64 max_total_us;
} ClientData;

// Read the timestamp out of an RTP packet header (bytes 4-7, big endian). Done by
// hand to avoid a dependency on the gstrtp library for one field. Still correct
// under SRTP: encryption covers the payload, not the header.
static gboolean rtp_timestamp_of(GstBuffer *buffer, guint32 *timestamp) {
    GstMapInfo map;
    if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        return FALSE;
    }
    gboolean ok = FALSE;
    if (map.size >= 12) {
        *timestamp = GST_READ_UINT32_BE(map.data + 4);
        ok = TRUE;
    }
    gst_buffer_unmap(buffer, &map);
    return ok;
}

// Return the name of the best available NVIDIA decoder for the given codec, or
// NULL when none is installed. Element names vary by GStreamer release:
// 1.16 exposes only the generic "nvdec", 1.18+ adds codec-specific
// "nvh264dec"/"nvh265dec", and 1.24+ adds the "nvautogpu*" variants.
static const gchar* detect_nvidia_decoder(const gchar *codec) {
    const gchar *h264_decoders[] = {
        "nvh264dec", "nvautogpuh264dec", "nvdec", NULL
    };
    const gchar *h265_decoders[] = {
        "nvh265dec", "nvautogpuh265dec", "nvdec", NULL
    };
    const gchar **candidates = g_strcmp0(codec, "H264") == 0
        ? h264_decoders : h265_decoders;

    for (gint i = 0; candidates[i] != NULL; i++) {
        GstElementFactory *factory = gst_element_factory_find(candidates[i]);
        if (factory) {
            gst_object_unref(factory);
            return candidates[i];
        }
    }
    return NULL;
}

// Assign a boolean property only when the installed plugin actually exposes it,
// so differences between GStreamer versions do not produce GObject warnings.
static gboolean set_bool_prop(GstElement *element, const gchar *name, gboolean value) {
    if (!g_object_class_find_property(G_OBJECT_GET_CLASS(element), name)) {
        return FALSE;
    }
    g_object_set(element, name, value, NULL);
    return TRUE;
}

// Report whether an element's src pad can produce buffers carrying the given
// memory feature (for example "memory:GLMemory"). Hardware decoders hand back
// GPU-resident buffers that plain videoconvert cannot consume, so the caller
// needs to know which download element to splice in.
static gboolean src_pad_has_feature(GstElement *element, const gchar *feature) {
    GstPad *src = gst_element_get_static_pad(element, "src");
    if (!src) {
        return FALSE;
    }

    GstCaps *caps = gst_pad_query_caps(src, NULL);
    gboolean found = FALSE;
    if (caps) {
        for (guint i = 0; i < gst_caps_get_size(caps) && !found; i++) {
            GstCapsFeatures *features = gst_caps_get_features(caps, i);
            if (features && gst_caps_features_contains(features, feature)) {
                found = TRUE;
            }
        }
        gst_caps_unref(caps);
    }
    gst_object_unref(src);
    return found;
}

static gboolean build_pipeline(ClientData *data) {
    // Create pipeline programmatically
    data->pipeline = gst_pipeline_new("video-receiving-pipeline");
    if (!data->pipeline) {
        g_printerr("Failed to create pipeline\n");
        return FALSE;
    }

    const gchar *nvdec_name = detect_nvidia_decoder(data->codec);
    GstElement *decoder = NULL;
    // Newer NVIDIA decoders output CUDA memory, which must be copied back to
    // system memory before videoconvert can handle it. The element only exists
    // on releases where it is required.
    GstElement *cudadownload = NULL;
    // GStreamer 1.16's nvdec emits video/x-raw(memory:GLMemory), so those frames
    // must be converted and pulled back to system memory before display.
    GstElement *glcolorconvert = NULL;
    GstElement *gldownload = NULL;

    // Create elements. A jitterbuffer is essential rather than optional here:
    // without it the whole decode and render chain runs on the udpsrc streaming
    // thread, so the socket goes undrained while a frame is being decoded and
    // the kernel receive buffer overflows. That lost-packet stream is what shows
    // up as corrupted frames. The jitterbuffer pushes from its own thread, which
    // keeps the socket drained, and it also reorders packets.
    GstElement *udpsrc = gst_element_factory_make("udpsrc", "udpsrc");
    GstElement *capsfilter = gst_element_factory_make("capsfilter", "capsfilter");
    GstElement *jitterbuffer = gst_element_factory_make("rtpjitterbuffer", "jitterbuffer");
    GstElement *srtpdec = NULL;
    GstElement *depay = NULL;
    GstElement *parse = NULL;
    GstElement *decodebin = NULL;
    GstElement *videoconvert = gst_element_factory_make("videoconvert", "videoconvert");
    GstElement *autovideosink = gst_element_factory_make("autovideosink", "autovideosink");

    if (!udpsrc || !capsfilter || !videoconvert || !autovideosink || !jitterbuffer) {
        g_printerr("Failed to create elements\n");
        return FALSE;
    }

    // Keep the buffer as short as the user asked for; it bounds added latency.
    g_object_set(jitterbuffer, "latency", (guint)data->latency, NULL);
    // Emit packet-lost events so the decoder is told about gaps instead of
    // silently decoding damaged references.
    set_bool_prop(jitterbuffer, "do-lost", TRUE);
    // GStreamer's default mode slaves the receiver to an estimate of the sender's
    // clock and schedules every packet against it, which was measured holding
    // frames ~22 ms even with a 5 ms depth. "none" forwards as soon as possible so
    // the configured depth is the only delay added, at the cost of the smoothing
    // that a jittery or reordering network needs.
    if (g_object_class_find_property(G_OBJECT_GET_CLASS(jitterbuffer), "mode")) {
        gst_util_set_object_arg(G_OBJECT(jitterbuffer), "mode", data->jitter_mode);
    }
    g_print("Jitterbuffer: %d ms, mode=%s\n", data->latency, data->jitter_mode);

    // Hardware decoding is opt-in via --hw-decode. On this machine nvdec decodes
    // into GL textures that come back blank, so every frame renders as a flat
    // colour; software decoding is verified pixel-accurate. Correctness wins by
    // default, and the flag is there for hosts where nvdec behaves.
    if (data->hw_decode && nvdec_name) {
        decoder = gst_element_factory_make(nvdec_name, "decoder");
        if (decoder) {
            g_print("Using NVIDIA hardware decoder: %s\n", nvdec_name);

            // The decoder returns GPU-resident frames. Determine which kind and
            // splice in the matching transfer elements, otherwise negotiation
            // with videoconvert fails and no frames ever reach the sink.
            if (src_pad_has_feature(decoder, "memory:GLMemory")) {
                glcolorconvert = gst_element_factory_make("glcolorconvert", "glcolorconvert");
                gldownload = gst_element_factory_make("gldownload", "gldownload");
                if (!glcolorconvert || !gldownload) {
                    g_printerr("%s outputs GL memory but glcolorconvert/gldownload "
                               "are unavailable; falling back to decodebin\n", nvdec_name);
                    gst_object_unref(decoder);
                    decoder = NULL;
                } else {
                    g_print("Decoder outputs GL memory: inserting "
                            "glcolorconvert + gldownload\n");
                }
            } else if (src_pad_has_feature(decoder, "memory:CUDAMemory")) {
                cudadownload = gst_element_factory_make("cudadownload", "cudadownload");
                if (cudadownload) {
                    g_print("Decoder outputs CUDA memory: inserting cudadownload\n");
                }
            }
        } else {
            g_printerr("Could not instantiate %s, falling back to decodebin\n",
                       nvdec_name);
        }
    }

    if (!decoder) {
        // Name the software decoder explicitly rather than relying on decodebin,
        // which ranks nvdec above the software decoders and would autoplug the
        // very element that produces blank frames.
        const gchar *sw_name = (g_strcmp0(data->codec, "H264") == 0)
                                   ? "avdec_h264" : "avdec_h265";
        decoder = gst_element_factory_make(sw_name, "decoder");
        if (decoder) {
            g_print("Using software decoder: %s\n", sw_name);
            // Skip frames the decoder knows are damaged instead of rendering
            // visibly broken pictures.
            set_bool_prop(decoder, "output-corrupt", FALSE);
        } else {
            g_print("Using decodebin for decoding\n");
            decodebin = gst_element_factory_make("decodebin", "decodebin");
            if (!decodebin) {
                g_printerr("Failed to create decodebin\n");
                return FALSE;
            }
        }
    }

    // Create depayloader and parser based on codec
    if (g_strcmp0(data->codec, "H264") == 0) {
        depay = gst_element_factory_make("rtph264depay", "depay");
        parse = gst_element_factory_make("h264parse", "parse");
    } else {
        depay = gst_element_factory_make("rtph265depay", "depay");
        parse = gst_element_factory_make("h265parse", "parse");
    }

    if (!depay || !parse) {
        g_printerr("Failed to create depayloader/parser\n");
        return FALSE;
    }

    // Set udpsrc properties. Asking for more than net.core.rmem_max needs
    // CAP_NET_ADMIN, so the kernel would refuse and warn. Clamp to the limit and
    // tell the user how to lift it, since a larger socket buffer is what absorbs
    // the packet bursts produced by large keyframes.
    gint buffer_size = data->buffer_size;
    gchar *rmem = NULL;
    if (g_file_get_contents("/proc/sys/net/core/rmem_max", &rmem, NULL, NULL)) {
        gint rmem_max = atoi(rmem);
        if (rmem_max > 0 && buffer_size > rmem_max) {
            g_print("Receive buffer capped at %d bytes by net.core.rmem_max "
                    "(wanted %d).\n", rmem_max, buffer_size);
            g_print("  Raise it with: sudo sysctl -w net.core.rmem_max=%d\n",
                    data->buffer_size);
            buffer_size = rmem_max;
        }
        g_free(rmem);
    }
    g_object_set(udpsrc, "address", data->host, "port", data->port,
                 "buffer-size", buffer_size, NULL);

    // Set caps filter for RTP. Under SRTP the media description is the same; only
    // the container name changes, and srtpdec hands on plain application/x-rtp.
    GstCaps *caps = gst_caps_new_simple(
        data->srtp_video.enabled ? "application/x-srtp" : "application/x-rtp",
        "media", G_TYPE_STRING, "video",
        "encoding-name", G_TYPE_STRING, data->codec,
        "payload", G_TYPE_INT, 96,
        "clock-rate", G_TYPE_INT, 90000,
        NULL);
    g_object_set(capsfilter, "caps", caps, NULL);
    gst_caps_unref(caps);

    if (data->srtp_video.enabled) {
        srtpdec = mp_srtp_make_receiver(GST_BIN(data->pipeline), &data->srtp_video,
                                       "videosrtpdec");
        if (!srtpdec) {
            return FALSE;
        }
    }

    // Present frames on their timestamps so playback runs at exactly the rate the
    // server sent them. autovideosink is a wrapper bin that only forwards a few
    // properties, so probe before assigning to avoid a GObject warning.
    // Frame dropping is handled separately, once the real sink exists; see
    // relax_sink_dropping().
    if (g_object_class_find_property(G_OBJECT_GET_CLASS(autovideosink), "sync")) {
        g_object_set(autovideosink, "sync", TRUE, NULL);
    }

    // Add all elements to pipeline
    if (decoder) {
        // Assemble the chain in order, skipping the optional stages that this
        // GStreamer version does not need, then add and link it in one pass.
        // srtpdec is left out of this list: its pads are named rtp_sink/rtp_src
        // rather than sink/src, so it is linked by name below.
        GstElement *chain[12];
        gint n = 0;
        chain[n++] = udpsrc;
        chain[n++] = capsfilter;
        chain[n++] = jitterbuffer;
        chain[n++] = depay;
        chain[n++] = parse;
        chain[n++] = decoder;
        if (glcolorconvert) chain[n++] = glcolorconvert;
        if (gldownload)     chain[n++] = gldownload;
        if (cudadownload)   chain[n++] = cudadownload;
        chain[n++] = videoconvert;
        chain[n++] = autovideosink;

        for (gint i = 0; i < n; i++) {
            gst_bin_add(GST_BIN(data->pipeline), chain[i]);
        }

        for (gint i = 0; i < n - 1; i++) {
            // The capsfilter reaches the jitterbuffer through srtpdec when the
            // stream is encrypted.
            if (srtpdec && chain[i] == capsfilter) {
                if (!gst_element_link_pads(capsfilter, "src", srtpdec, "rtp_sink") ||
                    !gst_element_link_pads(srtpdec, "rtp_src", jitterbuffer, "sink")) {
                    g_printerr("Failed to link srtpdec into the video chain\n");
                    return FALSE;
                }
                continue;
            }
            if (!gst_element_link(chain[i], chain[i + 1])) {
                g_printerr("Failed to link %s -> %s\n",
                           GST_ELEMENT_NAME(chain[i]),
                           GST_ELEMENT_NAME(chain[i + 1]));
                return FALSE;
            }
        }
    } else {
        gst_bin_add_many(GST_BIN(data->pipeline), udpsrc, capsfilter, jitterbuffer,
                         depay, parse, decodebin, videoconvert, autovideosink, NULL);
    }

    if (!decoder) {
        if (srtpdec) {
            if (!gst_element_link(udpsrc, capsfilter) ||
                !gst_element_link_pads(capsfilter, "src", srtpdec, "rtp_sink") ||
                !gst_element_link_pads(srtpdec, "rtp_src", jitterbuffer, "sink") ||
                !gst_element_link_many(jitterbuffer, depay, parse, decodebin, NULL)) {
                g_printerr("Failed to link source elements\n");
                return FALSE;
            }
        } else if (!gst_element_link_many(udpsrc, capsfilter, jitterbuffer, depay, parse,
                                         decodebin, NULL)) {
            g_printerr("Failed to link source elements\n");
            return FALSE;
        }

        // Link videoconvert to autovideosink
        if (!gst_element_link(videoconvert, autovideosink)) {
            g_printerr("Failed to link display elements\n");
            return FALSE;
        }
    }

    g_print("Pipeline built\n");

    return TRUE;
}

// Note when each frame's first packet reached the socket.
static GstPadProbeReturn arrival_probe(GstPad *pad, GstPadProbeInfo *info,
                                      gpointer user_data) {
    (void)pad;
    ClientData *data = (ClientData *)user_data;
    if (!(GST_PAD_PROBE_INFO_TYPE(info) & GST_PAD_PROBE_TYPE_BUFFER)) {
        return GST_PAD_PROBE_OK;
    }

    guint32 rtp_ts;
    if (rtp_timestamp_of(GST_PAD_PROBE_INFO_BUFFER(info), &rtp_ts)) {
        guint slot = rtp_ts % LAT_RING;
        g_mutex_lock(&data->lat_lock);
        // Every packet of a frame carries the same timestamp; keep the first.
        if (data->arrival_rtp_ts[slot] != rtp_ts) {
            data->arrival_rtp_ts[slot] = rtp_ts;
            data->arrival_us[slot] = g_get_monotonic_time();
        }
        g_mutex_unlock(&data->lat_lock);
    }
    return GST_PAD_PROBE_OK;
}

// Once the jitterbuffer has stamped a packet with a presentation timestamp, carry
// that frame's arrival time across to a PTS-keyed slot. PTS survives depayloading
// and decoding, so it is what links the decoded frame back to its arrival.
static GstPadProbeReturn link_probe(GstPad *pad, GstPadProbeInfo *info,
                                   gpointer user_data) {
    (void)pad;
    ClientData *data = (ClientData *)user_data;
    if (!(GST_PAD_PROBE_INFO_TYPE(info) & GST_PAD_PROBE_TYPE_BUFFER)) {
        return GST_PAD_PROBE_OK;
    }

    GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    GstClockTime pts = GST_BUFFER_PTS(buffer);
    guint32 rtp_ts;
    if (!GST_CLOCK_TIME_IS_VALID(pts) || !rtp_timestamp_of(buffer, &rtp_ts)) {
        return GST_PAD_PROBE_OK;
    }

    guint rtp_slot = rtp_ts % LAT_RING;
    guint pts_slot = (pts / GST_MSECOND) % LAT_RING;

    g_mutex_lock(&data->lat_lock);
    LatencyStages *slot = &data->stage[pts_slot];
    if (data->arrival_rtp_ts[rtp_slot] == rtp_ts && slot->pts != pts) {
        slot->pts = pts;
        slot->arrived_us = data->arrival_us[rtp_slot];
        slot->depayed_us = g_get_monotonic_time();
        slot->parsed_us = 0;
        slot->decoded_us = 0;
    }
    g_mutex_unlock(&data->lat_lock);

    return GST_PAD_PROBE_OK;
}

// Record the moment a frame enters and leaves the decoder.
static GstPadProbeReturn stage_probe(GstPad *pad, GstPadProbeInfo *info,
                                    gpointer user_data) {
    ClientData *data = (ClientData *)user_data;
    if (!(GST_PAD_PROBE_INFO_TYPE(info) & GST_PAD_PROBE_TYPE_BUFFER)) {
        return GST_PAD_PROBE_OK;
    }

    GstClockTime pts = GST_BUFFER_PTS(GST_PAD_PROBE_INFO_BUFFER(info));
    if (!GST_CLOCK_TIME_IS_VALID(pts)) {
        return GST_PAD_PROBE_OK;
    }

    // The pad direction tells us which side of the decoder this is.
    gboolean entering = (GST_PAD_DIRECTION(pad) == GST_PAD_SINK);
    gint64 now = g_get_monotonic_time();

    g_mutex_lock(&data->lat_lock);
    LatencyStages *slot = &data->stage[(pts / GST_MSECOND) % LAT_RING];
    if (slot->pts == pts) {
        if (entering) {
            slot->parsed_us = now;
        } else {
            slot->decoded_us = now;
        }
    }
    g_mutex_unlock(&data->lat_lock);

    return GST_PAD_PROBE_OK;
}

// Count frames reaching the sink and record how long each took to get there.
static GstPadProbeReturn render_probe(GstPad *pad, GstPadProbeInfo *info,
                                     gpointer user_data) {
    (void)pad;
    ClientData *data = (ClientData *)user_data;
    if (!(GST_PAD_PROBE_INFO_TYPE(info) & GST_PAD_PROBE_TYPE_BUFFER)) {
        return GST_PAD_PROBE_OK;
    }

    data->frame_count++;

    GstClockTime pts = GST_BUFFER_PTS(GST_PAD_PROBE_INFO_BUFFER(info));
    gint64 now = g_get_monotonic_time();

    if (!GST_CLOCK_TIME_IS_VALID(pts)) {
        return GST_PAD_PROBE_OK;
    }

    g_mutex_lock(&data->lat_lock);
    LatencyStages *slot = &data->stage[(pts / GST_MSECOND) % LAT_RING];
    if (slot->pts == pts && slot->arrived_us > 0 && slot->parsed_us > 0 &&
        slot->decoded_us > 0) {
        data->lat_frames++;
        data->sum_jitter_us += slot->depayed_us - slot->arrived_us;
        data->sum_parse_us += slot->parsed_us - slot->depayed_us;
        data->sum_decode_us += slot->decoded_us - slot->parsed_us;
        data->sum_convert_us += now - slot->decoded_us;

        gint64 total = now - slot->arrived_us;
        data->sum_total_us += total;
        if (total > data->max_total_us) {
            data->max_total_us = total;
        }
    }
    g_mutex_unlock(&data->lat_lock);

    return GST_PAD_PROBE_OK;
}

// Report throughput once per second so it is obvious whether video is actually
// arriving, rather than leaving a silent window as the only feedback.
static gboolean report_stats(gpointer user_data) {
    ClientData *data = (ClientData *)user_data;
    gint total = data->frame_count;
    gint delta = total - data->last_reported_count;

    // Timer callbacks are coalesced and drift, so derive the rate from the real
    // elapsed interval rather than assuming exactly one second passed.
    gint64 now = g_get_monotonic_time();
    gdouble seconds = (now - data->last_report_time) / (gdouble)G_USEC_PER_SEC;

    data->last_reported_count = total;
    data->last_report_time = now;

    g_mutex_lock(&data->lat_lock);
    gint n = data->lat_frames;
    gdouble jitter = n ? data->sum_jitter_us / (gdouble)n / 1000.0 : 0.0;
    gdouble parse = n ? data->sum_parse_us / (gdouble)n / 1000.0 : 0.0;
    gdouble decode = n ? data->sum_decode_us / (gdouble)n / 1000.0 : 0.0;
    gdouble convert = n ? data->sum_convert_us / (gdouble)n / 1000.0 : 0.0;
    gdouble total_ms = n ? data->sum_total_us / (gdouble)n / 1000.0 : 0.0;
    gdouble max_ms = data->max_total_us / 1000.0;
    data->lat_frames = 0;
    data->sum_jitter_us = 0;
    data->sum_parse_us = 0;
    data->sum_decode_us = 0;
    data->sum_convert_us = 0;
    data->sum_total_us = 0;
    data->max_total_us = 0;
    g_mutex_unlock(&data->lat_lock);

    if (delta <= 0 || seconds <= 0.0) {
        g_print("No frames received - waiting for stream on port %d\n", data->port);
    } else if (n > 0) {
        g_print("Receiving %.1f fps | arrival->sink %.1f ms (max %.1f) = "
                "jitterbuf %.1f + parse %.1f + decode %.1f + convert %.1f\n",
                delta / seconds, total_ms, max_ms, jitter, parse, decode, convert);
    } else {
        g_print("Receiving %.1f fps (total %d frames)\n", delta / seconds, total);
    }
    return G_SOURCE_CONTINUE;
}

// autovideosink is a bin that only creates its real sink during the state change,
// and it proxies just a few properties ("sync" but not "qos" or "max-lateness").
// Walk into it and relax frame dropping on the element that actually renders.
//
// Without this the sink decides it cannot keep up at high frame rates and sends
// QoS events upstream, which makes the decoder skip frames - the client then runs
// measurably below the rate the server is sending.
static void relax_sink_dropping(GstElement *element, gboolean *applied) {
    if (GST_IS_BIN(element)) {
        GstIterator *it = gst_bin_iterate_elements(GST_BIN(element));
        GValue item = G_VALUE_INIT;
        while (gst_iterator_next(it, &item) == GST_ITERATOR_OK) {
            relax_sink_dropping(GST_ELEMENT(g_value_get_object(&item)), applied);
            g_value_reset(&item);
        }
        g_value_unset(&item);
        gst_iterator_free(it);
        return;
    }

    GObjectClass *klass = G_OBJECT_GET_CLASS(element);
    if (g_object_class_find_property(klass, "qos")) {
        g_object_set(element, "qos", FALSE, NULL);
        *applied = TRUE;
    }
    if (g_object_class_find_property(klass, "max-lateness")) {
        // -1 means "never consider a frame too late to render".
        g_object_set(element, "max-lateness", (gint64)-1, NULL);
    }
}

static gboolean on_video_bus(MpWorker *worker, GstMessage *message, gpointer user_data) {
    ClientData *data = (ClientData *)user_data;

    if (GST_MESSAGE_TYPE(message) != GST_MESSAGE_STATE_CHANGED) {
        return FALSE;   // errors, warnings and EOS are the worker's business
    }
    GstState old, new_state, pending;
    gst_message_parse_state_changed(message, &old, &new_state, &pending);
    // Only act on the pipeline's own transition, not on every child's.
    if (new_state == GST_STATE_PLAYING &&
        GST_MESSAGE_SRC(message) == GST_OBJECT(worker->pipeline)) {
        g_print("Pipeline is playing\n");

        // The real sink only exists now, so this is the first opportunity to stop
        // it dropping frames.
        if (!data->sink_tuned) {
            GstElement *sink =
                gst_bin_get_by_name(GST_BIN(worker->pipeline), "autovideosink");
            if (sink) {
                gboolean applied = FALSE;
                relax_sink_dropping(sink, &applied);
                g_print("Frame dropping at the sink: %s\n",
                        applied ? "disabled" : "not adjustable");
                gst_object_unref(sink);
            }
            data->sink_tuned = TRUE;
        }
    }
    return FALSE;
}

static void on_pad_added(GstElement *decodebin_element, GstPad *pad, gpointer user_data) {
    (void)decodebin_element;
    ClientData *data = (ClientData *)user_data;

    // Find the videoconvert element
    GstElement *videoconvert = gst_bin_get_by_name(GST_BIN(data->pipeline), "videoconvert");
    if (!videoconvert) {
        g_printerr("Failed to find videoconvert element\n");
        return;
    }

    GstPad *sink_pad = gst_element_get_static_pad(videoconvert, "sink");
    if (sink_pad && !gst_pad_is_linked(sink_pad)) {
        GstPadLinkReturn ret = gst_pad_link(pad, sink_pad);
        if (ret == GST_PAD_LINK_OK) {
            g_print("Dynamic pad linked successfully\n");
        } else {
            g_printerr("Failed to link dynamic pad\n");
        }
        gst_object_unref(sink_pad);
    }
    gst_object_unref(videoconvert);
}

// ---------------------------------------------------------------------------
// Keying channel. Only exists when PKI is configured: this side listens, checks
// the sender's certificate against the CA, receives the master keys for the two
// incoming streams and hands over the one for the audio it sends back.
// ---------------------------------------------------------------------------

typedef struct {
    ClientData *data;
    MpSrtpKey *key;
} KeyTarget;

static void on_stream_key(const gchar *cipher, const gchar *auth,
                          const gchar *base64_key, gpointer user_data) {
    KeyTarget *target = (KeyTarget *)user_data;
    if (g_strcmp0(cipher, MP_SRTP_CIPHER) != 0 || g_strcmp0(auth, MP_SRTP_AUTH) != 0) {
        g_printerr("Sender offered SRTP suite %s/%s; this build only accepts %s/%s\n",
                   cipher, auth, MP_SRTP_CIPHER, MP_SRTP_AUTH);
        return;
    }
    mp_srtp_key_set_base64(target->key, base64_key);
}

static gboolean start_keying(ClientData *data, gboolean audio_send,
                             KeyTarget *video_target, KeyTarget *audio_target) {
    GError *error = NULL;
    if (audio_send && !mp_srtp_key_generate(&data->srtp_audio_up, &error)) {
        g_printerr("%s\n", error->message);
        g_clear_error(&error);
        return FALSE;
    }

    data->keying = mp_signal_new(data->pki);
    // Blocking, and deliberately so: without the sender's keys nothing can be
    // decrypted, so there is nothing useful to do until it connects.
    g_print("Waiting for the sender's keying connection on %s:%d\n",
            data->host, data->key_port);
    if (!mp_signal_accept(data->keying, data->host, data->key_port)) {
        return FALSE;
    }

    video_target->data = data;
    video_target->key = &data->srtp_video;
    audio_target->data = data;
    audio_target->key = &data->srtp_audio_down;
    mp_signal_register(data->keying, "video", data->video_worker->context,
                       NULL, NULL, on_stream_key, video_target);
    if (data->audio_rx_worker) {
        mp_signal_register(data->keying, "audio-down", data->audio_rx_worker->context,
                           NULL, NULL, on_stream_key, audio_target);
    }

    if (audio_send) {
        gchar *key = mp_srtp_key_to_base64(&data->srtp_audio_up);
        gboolean ok = key && mp_signal_send_key(data->keying, "audio-up",
                                                MP_SRTP_CIPHER, MP_SRTP_AUTH, key);
        if (key) {
            memset(key, 0, strlen(key));
            g_free(key);
        }
        if (!ok) {
            g_printerr("Could not deliver the returning audio's SRTP key\n");
            return FALSE;
        }
    }
    return TRUE;
}

// ---------------------------------------------------------------------------

static void on_worker_failure(MpWorker *worker, gpointer user_data) {
    ClientData *data = (ClientData *)user_data;
    if (worker == data->video_worker) {
        g_printerr("Video stream failed (%s); shutting down\n",
                   worker->error_text ? worker->error_text : "unknown");
        g_main_loop_quit(data->supervisor);
    } else {
        g_printerr("Stream '%s' stopped (%s); video continues\n", worker->name,
                   worker->error_text ? worker->error_text : "unknown");
    }
}

static gboolean on_interrupt(gpointer user_data) {
    ClientData *data = (ClientData *)user_data;
    g_print("\nInterrupted; stopping\n");
    g_main_loop_quit(data->supervisor);
    return G_SOURCE_REMOVE;
}

static void run_client(ClientData *data) {
    // Initialize frame counter
    data->frame_count = 0;
    data->last_reported_count = 0;
    data->last_report_time = g_get_monotonic_time();
    g_mutex_init(&data->lat_lock);

    gboolean encrypt = (data->pki && data->pki->enabled);
    gboolean audio_send = (data->audio.codec != MP_AUDIO_OFF && data->audio.send &&
                           g_strcmp0(data->audio.source, "none") != 0);
    gboolean audio_recv = (data->audio.codec != MP_AUDIO_OFF && data->audio.receive);

    mp_srtp_key_init(&data->srtp_video, "video", encrypt);
    mp_srtp_key_init(&data->srtp_audio_down, "audio-down", encrypt);
    mp_srtp_key_init(&data->srtp_audio_up, "audio-up", encrypt);

    data->video_worker = mp_worker_new("video");
    data->video_worker->on_failure = on_worker_failure;
    data->video_worker->on_failure_data = data;
    if (audio_recv) {
        data->audio_rx_worker = mp_worker_new("audio-down");
        data->audio_rx_worker->on_failure = on_worker_failure;
        data->audio_rx_worker->on_failure_data = data;
    }
    if (audio_send) {
        data->audio_tx_worker = mp_worker_new("audio-up");
        data->audio_tx_worker->on_failure = on_worker_failure;
        data->audio_tx_worker->on_failure_data = data;
    }

    // Keys must be in hand before srtpenc is built, and the incoming ones are
    // installed through srtpdec's request-key signal once packets start arriving.
    KeyTarget video_target, audio_target;
    if (encrypt && !start_keying(data, audio_send, &video_target, &audio_target)) {
        return;
    }

    if (!build_pipeline(data)) {
        return;
    }

    // These probes cooperate to time the receive path: arrival at the socket, the
    // jitterbuffer's release point, both sides of the decoder, and the handover to
    // the sink. Together they attribute the delay to a stage.
    struct { const gchar *element; const gchar *pad; GstPadProbeCallback cb; } taps[] = {
        { "udpsrc",       "src",  arrival_probe },
        { "depay",        "sink", link_probe    },
        { "decoder",      "sink", stage_probe   },
        { "decoder",      "src",  stage_probe   },
        { "videoconvert", "src",  render_probe  },
    };
    for (guint i = 0; i < G_N_ELEMENTS(taps); i++) {
        GstElement *element = gst_bin_get_by_name(GST_BIN(data->pipeline), taps[i].element);
        if (!element) {
            continue;
        }
        GstPad *pad = gst_element_get_static_pad(element, taps[i].pad);
        if (pad) {
            gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER, taps[i].cb, data, NULL);
            gst_object_unref(pad);
        }
        gst_object_unref(element);
    }

    // Handle dynamic pad linking for decodebin
    GstElement *decodebin = gst_bin_get_by_name(GST_BIN(data->pipeline), "decodebin");
    if (decodebin) {
        g_signal_connect(decodebin, "pad-added", G_CALLBACK(on_pad_added), data);
        gst_object_unref(decodebin);
    }

    mp_worker_set_pipeline(data->video_worker, data->pipeline, on_video_bus, data);
    mp_worker_add_timeout(data->video_worker, 1000, report_stats, data);

    if (audio_recv &&
        !mp_audio_udp_recv_pipeline(data->audio_rx_worker, &data->audio, "0.0.0.0",
                                    data->audio_port, &data->srtp_audio_down,
                                    &data->audio_rx_stats)) {
        g_printerr("Incoming audio disabled: its pipeline could not be built\n");
        audio_recv = FALSE;
    }
    if (audio_send &&
        !mp_audio_udp_send_pipeline(data->audio_tx_worker, &data->audio,
                                    data->peer_host, data->audio_back_port,
                                    &data->srtp_audio_up, &data->audio_tx_stats)) {
        g_printerr("Outgoing audio disabled: its pipeline could not be built\n");
        audio_send = FALSE;
    }

    if (!mp_worker_start(data->video_worker)) {
        return;
    }
    if (audio_recv) {
        mp_worker_start(data->audio_rx_worker);
    }
    if (audio_send) {
        mp_worker_start(data->audio_tx_worker);
    }
    if (data->keying) {
        // Only now: each key is installed by the worker that owns that stream, so
        // those loops have to be running to receive them.
        mp_signal_start(data->keying);
    }

    g_print("Listening on %s:%d%s\n", data->host, data->port, encrypt ? " (SRTP)" : "");
    g_print("Codec: %s, Latency: %dms\n", data->codec, data->latency);
    if (audio_send || audio_recv) {
        g_print("Audio: %s%s%s, %s, independent of video\n",
                audio_recv ? "in" : "", (audio_send && audio_recv) ? " + " : "",
                audio_send ? "back" : "", mp_audio_codec_name(data->audio.codec));
    }
    g_print("Press Ctrl+C to stop\n");

    g_unix_signal_add(SIGINT, on_interrupt, data);
    g_main_loop_run(data->supervisor);

    mp_worker_free(data->audio_tx_worker);
    mp_worker_free(data->audio_rx_worker);
    mp_worker_free(data->video_worker);
    data->audio_tx_worker = NULL;
    data->audio_rx_worker = NULL;
    data->video_worker = NULL;
    data->pipeline = NULL;   // owned by the worker

    if (data->keying) {
        mp_signal_free(data->keying);
        data->keying = NULL;
    }
    mp_srtp_key_clear(&data->srtp_video);
    mp_srtp_key_clear(&data->srtp_audio_down);
    mp_srtp_key_clear(&data->srtp_audio_up);
}

static void print_usage(const gchar *program, FILE *stream) {
    g_fprintf(stream, "Usage: %s [options]\n", program);
    g_fprintf(stream, "\nReceives RTP/UDP video and exchanges audio both ways.\n\n");
    g_fprintf(stream, "Video:\n");
    g_fprintf(stream, "  --host <address>       Address to listen on (default: 0.0.0.0)\n");
    g_fprintf(stream, "  --port <port>          UDP port to listen on (default: 5000)\n");
    g_fprintf(stream, "  --codec <h264|h265>    Expected codec (default: h264)\n");
    g_fprintf(stream, "  --latency <ms>         Jitterbuffer depth (default: 5)\n");
    g_fprintf(stream, "                         Raise on a lossy or long-haul link\n");
    g_fprintf(stream, "  --jitter-mode <mode>   none|slave|buffer|synced (default: none)\n");
    g_fprintf(stream, "                         Adaptive modes cost ~1 frame; use on lossy links\n");
    g_fprintf(stream, "  --buffer-size <bytes>  UDP receive buffer (default: 524288)\n");
    g_fprintf(stream, "  --hw-decode            Try the NVIDIA decoder instead of software\n");
    g_fprintf(stream, "                         Note: nvdec returns blank frames on some hosts\n");
    g_fprintf(stream, "\nPorts and the return path:\n");
    g_fprintf(stream, "  --peer-host <address>  Where to send this side's audio "
                      "(default: 127.0.0.1)\n");
    g_fprintf(stream, "  --audio-port <port>       Incoming audio (default: 5002)\n");
    g_fprintf(stream, "  --audio-back-port <port>  Outgoing audio (default: 5004)\n");
    g_fprintf(stream, "  --key-port <port>         TLS keying port to listen on "
                      "(default: 5010, PKI only)\n");
    mp_audio_print_usage(stream);
    mp_pki_print_usage(stream);
    g_fprintf(stream, "\nWith PKI configured this side waits for the sender's "
                      "keying connection before\nstarting, and every stream is "
                      "SRTP (%s, %s).\n", MP_SRTP_CIPHER, MP_SRTP_AUTH);
}

int main(int argc, char *argv[]) {
    ClientData data;
    memset(&data, 0, sizeof(data));

    // Initialize defaults
    data.host = g_strdup("0.0.0.0");
    data.port = 9601;
    data.codec = g_strdup("H265");
    // Buffer depth in milliseconds. Together with jitter_mode below this decides
    // how much delay the receive path adds; measured at 0.2 ms of the ~9.5 ms
    // arrival-to-sink total, the rest being software decode. Raise it for a lossy
    // or long-haul link, where too small a buffer cannot absorb reordering.
    data.latency = 5;
    // See build_pipeline(): the adaptive modes add roughly a frame of scheduling
    // delay. Switch to "slave" or "buffer" on a link that actually needs smoothing.
    data.jitter_mode = g_strdup("none");
    data.buffer_size = 524288;
    data.peer_host = g_strdup("127.0.0.1");
    data.audio_port = 5002;
    data.audio_back_port = 5004;
    data.key_port = 5010;
    data.pki = mp_pki_new();
    data.hw_decode = TRUE;
    // 660 Hz against the sender's 440 Hz, so the two directions are audibly
    // distinct on one desk.
    mp_audio_config_defaults(&data.audio, 660);

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (mp_audio_parse_arg(&data.audio, argc, argv, &i) ||
            mp_pki_parse_arg(data.pki, argc, argv, &i)) {
            continue;
        }
        if (g_strcmp0(argv[i], "--host") == 0 && i + 1 < argc) {
            g_free(data.host);
            data.host = g_strdup(argv[++i]);
        } else if (g_strcmp0(argv[i], "--port") == 0 && i + 1 < argc) {
            data.port = atoi(argv[++i]);
        } else if (g_strcmp0(argv[i], "--codec") == 0 && i + 1 < argc) {
            g_free(data.codec);
            data.codec = g_strdup(argv[++i]);
            // Convert to uppercase for GStreamer
            for (gchar *p = data.codec; *p; p++) {
                *p = g_ascii_toupper(*p);
            }
        } else if (g_strcmp0(argv[i], "--hw-decode") == 0) {
            data.hw_decode = TRUE;
        } else if (g_strcmp0(argv[i], "--latency") == 0 && i + 1 < argc) {
            data.latency = atoi(argv[++i]);
        } else if (g_strcmp0(argv[i], "--jitter-mode") == 0 && i + 1 < argc) {
            g_free(data.jitter_mode);
            data.jitter_mode = g_strdup(argv[++i]);
        } else if (g_strcmp0(argv[i], "--buffer-size") == 0 && i + 1 < argc) {
            data.buffer_size = atoi(argv[++i]);
        } else if (g_strcmp0(argv[i], "--peer-host") == 0 && i + 1 < argc) {
            g_free(data.peer_host);
            data.peer_host = g_strdup(argv[++i]);
        } else if (g_strcmp0(argv[i], "--audio-port") == 0 && i + 1 < argc) {
            data.audio_port = atoi(argv[++i]);
        } else if (g_strcmp0(argv[i], "--audio-back-port") == 0 && i + 1 < argc) {
            data.audio_back_port = atoi(argv[++i]);
        } else if (g_strcmp0(argv[i], "--key-port") == 0 && i + 1 < argc) {
            data.key_port = atoi(argv[++i]);
        } else if (g_strcmp0(argv[i], "--help") == 0 || g_strcmp0(argv[i], "-h") == 0) {
            print_usage(argv[0], stdout);
            return 0;
        }
    }

    if (data.audio.invalid) {
        return 2;
    }

    GError *error = NULL;
    if (!mp_pki_load(data.pki, &error)) {
        g_printerr("PKI configuration rejected: %s\n", error->message);
        g_clear_error(&error);
        return 1;
    }

    // Initialize GStreamer
    gst_init(&argc, &argv);

    data.supervisor = g_main_loop_new(NULL, FALSE);

    // Run client
    run_client(&data);

    // Cleanup
    g_main_loop_unref(data.supervisor);
    mp_audio_config_clear(&data.audio);
    mp_pki_free(data.pki);
    g_free(data.host);
    g_free(data.codec);
    g_free(data.jitter_mode);
    g_free(data.peer_host);

    return 0;
}

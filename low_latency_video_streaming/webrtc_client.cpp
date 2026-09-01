// WebRTC receiver.
//
// Same job as client.cpp but the transport is webrtcbin instead of udpsrc. This
// peer listens for signalling, answers the sender's offers, then decodes and
// displays - and sends its own audio back.
//
// Three independent peer connections, each in its own pipeline on its own thread
// (see media_worker.h):
//
//   video       - answered here
//   audio-down  - answered here: the audio from the sending side
//   audio-up    - offered here: this side's microphone, going the other way
//
// The video statistics line is deliberately identical in format to the UDP
// receiver's so the two transports can be compared directly and the same test
// harness parses both. One honest difference in what is measured: on the UDP
// path "arrival" is the moment a packet leaves the kernel socket, whereas here it
// is the moment a packet enters the jitterbuffer, which is after ICE and SRTP
// decryption inside webrtcbin. That excludes a sub-millisecond decrypt step,
// which is called out in the README rather than papered over.

#include "audio_stream.h"
#include "media_pki.h"
#include "media_props.h"
#include "media_worker.h"
#include "signal_channel.h"
#include "webrtc_session.h"

#include <glib-unix.h>
#include <stdio.h>
#include <stdlib.h>

// Indexed by RTP timestamp and by PTS-in-milliseconds. Only has to span the
// frames in flight between arrival and display.
#define LAT_RING 512

typedef struct {
    GstClockTime pts;
    gint64 arrived_us;   // entered the jitterbuffer
    gint64 depayed_us;   // released by the jitterbuffer
    gint64 parsed_us;    // handed to the decoder
    gint64 decoded_us;   // decoder produced the picture
} LatencyStages;

typedef struct {
    gchar *host;
    gint port;                // signalling port
    gint latency;             // jitterbuffer depth, ms
    gchar *jitter_mode;
    gchar *sink_name;         // fakesink isolates decode cost from render cost
    gboolean sink_tuned;

    MpAudioConfig audio;
    MpPki *pki;

    MpWorker *video_worker;
    MpWorker *audio_tx_worker;
    MpWorker *audio_rx_worker;
    MpWebrtcSession video_session;
    MpWebrtcSession audio_tx_session;
    MpWebrtcSession audio_rx_session;
    MpAudioStats audio_tx_stats;
    MpAudioStats audio_rx_stats;
    MpWebrtcGate audio_tx_gate;

    MpSignal *sig;
    GMainLoop *supervisor;

    GstElement *jitterbuffer;   // borrowed; owned by webrtcbin's rtpbin
    guint64 last_pushed;
    guint64 last_lost;
    guint64 last_late;
    guint64 last_duplicate;

    gint frame_count;
    // Per-stage frame counters, reset every report. A frame that leaves the
    // jitterbuffer but never reaches the sink has been dropped inside the decode
    // chain, and these say which stage did it.
    gint n_depay;
    gint n_dec_in;
    gint n_dec_out;
    gint last_reported_count;
    gint64 last_report_time;

    GMutex lat_lock;
    guint32 arrival_rtp_ts[LAT_RING];
    gint64 arrival_us[LAT_RING];
    LatencyStages stage[LAT_RING];
    gint lat_frames;
    gint64 sum_jitter_us;
    gint64 sum_parse_us;
    gint64 sum_decode_us;
    gint64 sum_convert_us;
    gint64 sum_total_us;
    gint64 max_total_us;
} WebRTCClient;

// ---------------------------------------------------------------------------
// Video latency probes. Same scheme as the UDP receiver: record arrival keyed by
// RTP timestamp, carry it across to a PTS-keyed slot once the jitterbuffer has
// stamped the buffer, then match the decoded frame at the sink.
// ---------------------------------------------------------------------------

static GstPadProbeReturn arrival_probe(GstPad *pad, GstPadProbeInfo *info,
                                       gpointer user_data) {
    (void)pad;
    WebRTCClient *data = (WebRTCClient *)user_data;
    if (!(GST_PAD_PROBE_INFO_TYPE(info) & GST_PAD_PROBE_TYPE_BUFFER)) {
        return GST_PAD_PROBE_OK;
    }
    guint32 rtp_ts;
    if (mp_rtp_timestamp_of(GST_PAD_PROBE_INFO_BUFFER(info), &rtp_ts)) {
        guint slot = rtp_ts % LAT_RING;
        g_mutex_lock(&data->lat_lock);
        if (data->arrival_rtp_ts[slot] != rtp_ts) {
            data->arrival_rtp_ts[slot] = rtp_ts;
            data->arrival_us[slot] = g_get_monotonic_time();
        }
        g_mutex_unlock(&data->lat_lock);
    }
    return GST_PAD_PROBE_OK;
}

static GstPadProbeReturn link_probe(GstPad *pad, GstPadProbeInfo *info,
                                    gpointer user_data) {
    (void)pad;
    WebRTCClient *data = (WebRTCClient *)user_data;
    if (!(GST_PAD_PROBE_INFO_TYPE(info) & GST_PAD_PROBE_TYPE_BUFFER)) {
        return GST_PAD_PROBE_OK;
    }

    GstBuffer *buffer = GST_PAD_PROBE_INFO_BUFFER(info);
    GstClockTime pts = GST_BUFFER_PTS(buffer);
    guint32 rtp_ts;
    if (!GST_CLOCK_TIME_IS_VALID(pts) || !mp_rtp_timestamp_of(buffer, &rtp_ts)) {
        return GST_PAD_PROBE_OK;
    }

    guint rtp_slot = rtp_ts % LAT_RING;
    guint pts_slot = (pts / GST_MSECOND) % LAT_RING;

    g_mutex_lock(&data->lat_lock);
    data->n_depay++;
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

static GstPadProbeReturn stage_probe(GstPad *pad, GstPadProbeInfo *info,
                                     gpointer user_data) {
    WebRTCClient *data = (WebRTCClient *)user_data;
    if (!(GST_PAD_PROBE_INFO_TYPE(info) & GST_PAD_PROBE_TYPE_BUFFER)) {
        return GST_PAD_PROBE_OK;
    }
    GstClockTime pts = GST_BUFFER_PTS(GST_PAD_PROBE_INFO_BUFFER(info));
    if (!GST_CLOCK_TIME_IS_VALID(pts)) {
        return GST_PAD_PROBE_OK;
    }

    gboolean entering = (GST_PAD_DIRECTION(pad) == GST_PAD_SINK);
    gint64 now = g_get_monotonic_time();

    g_mutex_lock(&data->lat_lock);
    if (entering) {
        data->n_dec_in++;
    } else {
        data->n_dec_out++;
    }
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

static GstPadProbeReturn render_probe(GstPad *pad, GstPadProbeInfo *info,
                                      gpointer user_data) {
    (void)pad;
    WebRTCClient *data = (WebRTCClient *)user_data;
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

static gboolean report_stats(gpointer user_data) {
    WebRTCClient *data = (WebRTCClient *)user_data;
    gint total = data->frame_count;
    gint delta = total - data->last_reported_count;

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
    gint n_depay = data->n_depay;
    gint n_dec_in = data->n_dec_in;
    gint n_dec_out = data->n_dec_out;
    data->n_depay = 0;
    data->n_dec_in = 0;
    data->n_dec_out = 0;
    g_mutex_unlock(&data->lat_lock);

    // Frames can go missing for very different reasons - packets never arriving,
    // arriving too late for the configured depth, or arriving twice. Guessing
    // between those wastes a lot of time, so read it straight from the
    // jitterbuffer and print the per-second deltas.
    gchar *packets = NULL;
    if (data->jitterbuffer) {
        GstStructure *stats = NULL;
        g_object_get(data->jitterbuffer, "stats", &stats, NULL);
        if (stats) {
            guint64 pushed = 0, lost = 0, late = 0, duplicate = 0;
            gst_structure_get_uint64(stats, "num-pushed", &pushed);
            gst_structure_get_uint64(stats, "num-lost", &lost);
            gst_structure_get_uint64(stats, "num-late", &late);
            gst_structure_get_uint64(stats, "num-duplicates", &duplicate);
            packets = g_strdup_printf(" | pkts=%" G_GUINT64_FORMAT
                                      " lost=%" G_GUINT64_FORMAT
                                      " late=%" G_GUINT64_FORMAT
                                      " dup=%" G_GUINT64_FORMAT,
                                      pushed - data->last_pushed,
                                      lost - data->last_lost,
                                      late - data->last_late,
                                      duplicate - data->last_duplicate);
            data->last_pushed = pushed;
            data->last_lost = lost;
            data->last_late = late;
            data->last_duplicate = duplicate;
            gst_structure_free(stats);
        }
    }

    gchar *stages = g_strdup_printf(" | frames jb=%d dec_in=%d dec_out=%d sink=%d",
                                    n_depay, n_dec_in, n_dec_out, delta);

    if (delta <= 0 || seconds <= 0.0) {
        g_print("No frames received - waiting for the WebRTC stream%s%s\n",
                packets ? packets : "", stages);
    } else if (n > 0) {
        g_print("Receiving %.1f fps | arrival->sink %.1f ms (max %.1f) = "
                "jitterbuf %.1f + parse %.1f + decode %.1f + convert %.1f%s%s\n",
                delta / seconds, total_ms, max_ms, jitter, parse, decode, convert,
                packets ? packets : "", stages);
    } else {
        g_print("Receiving %.1f fps (total %d frames)%s%s\n", delta / seconds, total,
                packets ? packets : "", stages);
    }
    g_free(packets);
    g_free(stages);
    return G_SOURCE_CONTINUE;
}

// The jitterbuffer only exists once negotiation creates the stream, so its pads
// are the earliest point inside webrtcbin that can be probed. wc_apply_low_latency
// calls this the moment rtpbin makes one.
static void on_video_jitterbuffer(GstElement *jitterbuffer, gpointer user_data) {
    WebRTCClient *data = (WebRTCClient *)user_data;
    data->jitterbuffer = jitterbuffer;

    GstPad *sink = gst_element_get_static_pad(jitterbuffer, "sink");
    GstPad *src = gst_element_get_static_pad(jitterbuffer, "src");
    if (sink) {
        gst_pad_add_probe(sink, GST_PAD_PROBE_TYPE_BUFFER, arrival_probe, data, NULL);
        gst_object_unref(sink);
    }
    if (src) {
        gst_pad_add_probe(src, GST_PAD_PROBE_TYPE_BUFFER, link_probe, data, NULL);
        gst_object_unref(src);
    }
}

static void on_audio_jitterbuffer(GstElement *jitterbuffer, gpointer user_data) {
    WebRTCClient *data = (WebRTCClient *)user_data;
    if (!data->audio.stats) {
        return;
    }
    g_mutex_lock(&data->audio_rx_stats.lock);
    data->audio_rx_stats.jitterbuffer = jitterbuffer;
    data->audio_rx_stats.have_arrival_tap = TRUE;
    g_mutex_unlock(&data->audio_rx_stats.lock);
    mp_audio_tap(jitterbuffer, "sink", mp_audio_arrival_probe, &data->audio_rx_stats);
}

// ---------------------------------------------------------------------------
// Video decode chain, built when webrtcbin produces a stream.
// ---------------------------------------------------------------------------

static void on_incoming_video(GstElement *webrtc, GstPad *pad, gpointer user_data) {
    (void)webrtc;
    WebRTCClient *data = (WebRTCClient *)user_data;

    if (GST_PAD_DIRECTION(pad) != GST_PAD_SRC) {
        return;
    }

    GstCaps *caps = gst_pad_get_current_caps(pad);
    if (!caps) {
        caps = gst_pad_query_caps(pad, NULL);
    }
    GstStructure *structure = caps ? gst_caps_get_structure(caps, 0) : NULL;
    const gchar *encoding = structure ?
        gst_structure_get_string(structure, "encoding-name") : NULL;
    g_print("Incoming video stream: %s\n", encoding ? encoding : "unknown");

    gboolean is_h265 = (g_strcmp0(encoding, "H265") == 0);
    if (caps) {
        gst_caps_unref(caps);
    }

    GstElement *pipeline = data->video_worker->pipeline;
    GstElement *depay = gst_element_factory_make(
        is_h265 ? "rtph265depay" : "rtph264depay", "depay");
    GstElement *parser = gst_element_factory_make(
        is_h265 ? "h265parse" : "h264parse", "parser");
    // Named explicitly rather than using decodebin, which ranks nvdec higher and
    // would autoplug an element that returns blank frames on this host.
    GstElement *decoder = gst_element_factory_make(
        is_h265 ? "avdec_h265" : "avdec_h264", "decoder");
    GstElement *convert = gst_element_factory_make("videoconvert", "videoconvert");
    GstElement *sink = gst_element_factory_make(data->sink_name, "sink");
    if (!sink) {
        g_printerr("Unknown sink '%s'\n", data->sink_name);
        return;
    }

    if (!depay || !parser || !decoder || !convert) {
        g_printerr("Failed to create the decode chain\n");
        return;
    }

    mp_set_bool_prop(decoder, "output-corrupt", FALSE);
    mp_set_bool_prop(sink, "sync", TRUE);

    gst_bin_add_many(GST_BIN(pipeline), depay, parser, decoder, convert, sink, NULL);
    if (!gst_element_link_many(depay, parser, decoder, convert, sink, NULL)) {
        g_printerr("Failed to link the decode chain\n");
        return;
    }

    // Both sides of the decoder, and the handover to the sink.
    struct { GstElement *element; const gchar *pad; GstPadProbeCallback cb; } taps[] = {
        { decoder, "sink", stage_probe  },
        { decoder, "src",  stage_probe  },
        { convert, "src",  render_probe },
    };
    for (guint i = 0; i < G_N_ELEMENTS(taps); i++) {
        GstPad *probe_pad = gst_element_get_static_pad(taps[i].element, taps[i].pad);
        if (probe_pad) {
            gst_pad_add_probe(probe_pad, GST_PAD_PROBE_TYPE_BUFFER, taps[i].cb,
                              data, NULL);
            gst_object_unref(probe_pad);
        }
    }

    gst_element_sync_state_with_parent(depay);
    gst_element_sync_state_with_parent(parser);
    gst_element_sync_state_with_parent(decoder);
    gst_element_sync_state_with_parent(convert);
    gst_element_sync_state_with_parent(sink);

    GstPad *depay_sink = gst_element_get_static_pad(depay, "sink");
    if (gst_pad_link(pad, depay_sink) != GST_PAD_LINK_OK) {
        g_printerr("Failed to link webrtcbin to the depayloader\n");
    }
    gst_object_unref(depay_sink);
}

static void on_incoming_audio(GstElement *webrtc, GstPad *pad, gpointer user_data) {
    (void)webrtc;
    WebRTCClient *data = (WebRTCClient *)user_data;
    if (GST_PAD_DIRECTION(pad) != GST_PAD_SRC) {
        return;
    }
    if (!mp_audio_webrtc_add_receiver(GST_BIN(data->audio_rx_worker->pipeline),
                                      &data->audio, pad, &data->audio_rx_stats)) {
        g_printerr("[audio-down] could not build the receive chain\n");
    }
}

// ---------------------------------------------------------------------------

// The real sink inside a wrapper bin only exists once it reaches PLAYING, and it
// must be stopped from dropping frames.
static gboolean on_video_bus(MpWorker *worker, GstMessage *message, gpointer user_data) {
    WebRTCClient *data = (WebRTCClient *)user_data;
    if (GST_MESSAGE_TYPE(message) != GST_MESSAGE_STATE_CHANGED || data->sink_tuned) {
        return FALSE;
    }
    GstState old_state, new_state;
    gst_message_parse_state_changed(message, &old_state, &new_state, NULL);
    if (new_state != GST_STATE_PLAYING) {
        return FALSE;
    }
    GstElement *sink = gst_bin_get_by_name(GST_BIN(worker->pipeline), "sink");
    if (sink) {
        gboolean applied = FALSE;
        mp_relax_sink_dropping(sink, &applied);
        if (applied) {
            data->sink_tuned = TRUE;
        }
        gst_object_unref(sink);
    }
    return FALSE;
}

static void on_worker_failure(MpWorker *worker, gpointer user_data) {
    WebRTCClient *data = (WebRTCClient *)user_data;
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
    WebRTCClient *data = (WebRTCClient *)user_data;
    g_print("\nInterrupted; stopping\n");
    g_main_loop_quit(data->supervisor);
    return G_SOURCE_REMOVE;
}

static void run_client(WebRTCClient *data) {
    gboolean audio_send = (data->audio.codec != MP_AUDIO_OFF && data->audio.send &&
                           g_strcmp0(data->audio.source, "none") != 0);
    gboolean audio_recv = (data->audio.codec != MP_AUDIO_OFF && data->audio.receive);

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

    // Blocks until the sender connects, which is the intended startup order.
    // Nothing goes to PLAYING before this: the audio this side sends is offered
    // by this side, and an offer created before the channel exists is lost.
    data->sig = mp_signal_new(data->pki);
    if (!mp_signal_accept(data->sig, data->host, data->port)) {
        return;
    }

    data->video_session.tag = "video";
    data->video_session.offerer = FALSE;
    data->video_session.worker = data->video_worker;
    data->video_session.sig = data->sig;
    data->video_session.pki = data->pki;
    data->video_session.jb_latency_ms = data->latency;
    data->video_session.jb_mode = data->jitter_mode;
    data->video_session.jb_hook = on_video_jitterbuffer;
    data->video_session.jb_hook_data = data;
    data->video_session.on_pad_added = on_incoming_video;
    data->video_session.user_data = data;
    if (!mp_webrtc_session_create(&data->video_session, "video_recv")) {
        return;
    }
    g_print("Requested video jitterbuffer: %d ms, mode=%s\n", data->latency,
            data->jitter_mode);

    GstElement *video_pipeline = gst_pipeline_new("webrtc-video-recv");
    gst_bin_add(GST_BIN(video_pipeline), data->video_session.webrtcbin);
    mp_worker_set_pipeline(data->video_worker, video_pipeline, on_video_bus, data);
    data->last_report_time = g_get_monotonic_time();
    mp_worker_add_timeout(data->video_worker, 1000, report_stats, data);

    if (audio_recv) {
        data->audio_rx_session.tag = "audio-down";
        data->audio_rx_session.offerer = FALSE;
        data->audio_rx_session.worker = data->audio_rx_worker;
        data->audio_rx_session.sig = data->sig;
        data->audio_rx_session.pki = data->pki;
        data->audio_rx_session.jb_latency_ms = data->audio.jb_latency_ms;
        data->audio_rx_session.jb_mode = data->audio.jitter_mode;
        data->audio_rx_session.jb_hook = on_audio_jitterbuffer;
        data->audio_rx_session.jb_hook_data = data;
        data->audio_rx_session.on_pad_added = on_incoming_audio;
        data->audio_rx_session.user_data = data;
        if (mp_webrtc_session_create(&data->audio_rx_session, "audio_recv")) {
            GstElement *pipeline = gst_pipeline_new("webrtc-audio-recv");
            gst_bin_add(GST_BIN(pipeline), data->audio_rx_session.webrtcbin);
            mp_audio_stats_init(&data->audio_rx_stats, "audio-recv", &data->audio,
                                data->audio.frame_ms);
            mp_worker_set_pipeline(data->audio_rx_worker, pipeline, NULL, NULL);
            if (data->audio.stats) {
                mp_worker_add_timeout(data->audio_rx_worker, 1000, mp_audio_report_rx,
                                      &data->audio_rx_stats);
            }
        } else {
            audio_recv = FALSE;
        }
    }

    if (audio_send) {
        data->audio_tx_session.tag = "audio-up";
        data->audio_tx_session.offerer = TRUE;
        data->audio_tx_session.worker = data->audio_tx_worker;
        data->audio_tx_session.sig = data->sig;
        data->audio_tx_session.pki = data->pki;
        data->audio_tx_session.jb_latency_ms = data->audio.jb_latency_ms;
        data->audio_tx_session.jb_mode = data->audio.jitter_mode;
        if (mp_webrtc_session_create(&data->audio_tx_session, "audio_send")) {
            GstElement *pipeline = gst_pipeline_new("webrtc-audio-send");
            gst_bin_add(GST_BIN(pipeline), data->audio_tx_session.webrtcbin);
            mp_audio_stats_init(&data->audio_tx_stats, "audio-send", &data->audio,
                                data->audio.frame_ms);
            GstElement *tail = NULL;
            if (mp_audio_webrtc_add_sender(GST_BIN(pipeline), &data->audio,
                                           data->audio_tx_session.webrtcbin,
                                           &data->audio_tx_stats, &tail)) {
                mp_webrtc_gate_attach(&data->audio_tx_gate, &data->audio_tx_session,
                                      tail, MP_WEBRTC_VERIFY_SECONDS);
                mp_worker_set_pipeline(data->audio_tx_worker, pipeline, NULL, NULL);
                if (data->audio.stats) {
                    mp_worker_add_timeout(data->audio_tx_worker, 1000,
                                          mp_audio_report_tx, &data->audio_tx_stats);
                }
            } else {
                g_printerr("Outgoing audio disabled: its pipeline could not be built\n");
                gst_object_unref(pipeline);
                audio_send = FALSE;
            }
        } else {
            audio_send = FALSE;
        }
    }

    // Pipelines first, reader second. webrtcbin only starts its internal task
    // loop on the way to PAUSED and silently drops anything enqueued before
    // that, so a remote offer that is already waiting in the socket must not be
    // dispatched until every webrtcbin is live. Sending does not need the
    // reader, so this side's own offer still goes out immediately.
    if (!mp_worker_start(data->video_worker)) {
        return;
    }
    if (audio_recv) {
        mp_worker_start(data->audio_rx_worker);
    }
    if (audio_send) {
        mp_worker_start(data->audio_tx_worker);
    }
    mp_signal_start(data->sig);

    g_print("Receiving over WebRTC%s\n",
            (data->pki && data->pki->enabled) ? " (DTLS/SRTP with PKI identities)"
                                              : " (DTLS/SRTP, self-signed)");

    g_unix_signal_add(SIGINT, on_interrupt, data);
    g_main_loop_run(data->supervisor);

    mp_worker_free(data->audio_tx_worker);
    mp_worker_free(data->audio_rx_worker);
    mp_worker_free(data->video_worker);
    data->audio_tx_worker = NULL;
    data->audio_rx_worker = NULL;
    data->video_worker = NULL;

    mp_webrtc_session_clear(&data->video_session);
    mp_webrtc_session_clear(&data->audio_tx_session);
    mp_webrtc_session_clear(&data->audio_rx_session);
    mp_signal_free(data->sig);
}

static void print_usage(const gchar *program, FILE *stream) {
    g_fprintf(stream, "Usage: %s [options]\n", program);
    g_fprintf(stream, "\nReceives a WebRTC video stream and exchanges audio both "
                      "ways. Start this before\nwebrtc_server.\n\n");
    g_fprintf(stream, "Video:\n");
    g_fprintf(stream, "  --host <address>       Signalling bind address "
                      "(default: 0.0.0.0)\n");
    g_fprintf(stream, "  --port <port>          Signalling port (default: 5020)\n");
    g_fprintf(stream, "  --latency <ms>         Video jitterbuffer depth "
                      "(default: 5)\n");
    g_fprintf(stream, "  --jitter-mode <mode>   none|slave|buffer|synced "
                      "(default: none)\n");
    g_fprintf(stream, "                         Adaptive modes cost ~1 frame\n");
    g_fprintf(stream, "  --sink <element>       Video sink (default: ximagesink)\n");
    g_fprintf(stream, "                         Use fakesink to measure without "
                      "a display\n");
    mp_audio_print_usage(stream);
    mp_pki_print_usage(stream);
}

int main(int argc, char *argv[]) {
    WebRTCClient data;
    memset(&data, 0, sizeof(data));

    data.host = g_strdup("0.0.0.0");
    data.port = 5020;
    // Same reasoning as the UDP receiver: keep the buffer short and stop the
    // adaptive scheduler from adding a frame on top of it.
    data.latency = 5;
    data.jitter_mode = g_strdup("none");
    // Not autovideosink. It autoplugs xvimagesink here, which renders 1080p60
    // too slowly and never had its QoS disabled through the wrapping bin, so the
    // decoder threw away every frame to catch up - measured 0 fps reaching the
    // display. ximagesink measured a steady 60 fps at ~17 ms, glimagesink ~968 ms.
    data.sink_name = g_strdup("ximagesink");
    data.pki = mp_pki_new();
    // 660 Hz against the sender's 440 Hz, so the two directions are audibly
    // distinct on one desk.
    mp_audio_config_defaults(&data.audio, 660);

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
        } else if (g_strcmp0(argv[i], "--latency") == 0 && i + 1 < argc) {
            data.latency = atoi(argv[++i]);
        } else if (g_strcmp0(argv[i], "--jitter-mode") == 0 && i + 1 < argc) {
            g_free(data.jitter_mode);
            data.jitter_mode = g_strdup(argv[++i]);
        } else if (g_strcmp0(argv[i], "--sink") == 0 && i + 1 < argc) {
            g_free(data.sink_name);
            data.sink_name = g_strdup(argv[++i]);
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

    gst_init(&argc, &argv);
    g_mutex_init(&data.lat_lock);
    data.supervisor = g_main_loop_new(NULL, FALSE);

    run_client(&data);

    g_main_loop_unref(data.supervisor);
    g_mutex_clear(&data.lat_lock);
    mp_audio_config_clear(&data.audio);
    mp_pki_free(data.pki);
    g_free(data.host);
    g_free(data.jitter_mode);
    g_free(data.sink_name);
    return 0;
}

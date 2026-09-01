// WebRTC sender.
//
// Same job as server.cpp but the transport is webrtcbin instead of udpsink. The
// parts that decide video latency are unchanged and deliberately identical to
// the UDP path: frames are memory-mapped and pushed by a dedicated pacing thread
// on absolute deadlines, the encoder is configured for zero latency, and the
// local preview is fed from the same tee so it shows a frame at the moment it is
// transmitted rather than trailing the wire.
//
// Three independent peer connections run here, each in its own pipeline on its
// own thread (see media_worker.h):
//
//   video       - offered by this peer, which is the one with video to send
//   audio-down  - offered by this peer: the audio it captures
//   audio-up    - answered by this peer: the audio coming back from the operator
//
// Nothing is synchronised between them. A microphone that will not open, or an
// audio stream whose certificate does not check out, leaves video untouched.

#include "audio_stream.h"
#include "media_pki.h"
#include "media_props.h"
#include "media_worker.h"
#include "signal_channel.h"
#include "webrtc_session.h"

#include <glib-unix.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define BUDGET_RING 256

typedef struct {
    gchar *input_file;
    gchar *host;
    gint port;              // signalling port
    gint width;
    gint height;
    gint framerate;
    gint bitrate;
    gboolean use_gpu;
    gint loop_seconds;
    gchar *stun_server;

    MpAudioConfig audio;
    MpPki *pki;

    // One worker, one pipeline, one thread per stream.
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
    GMainLoop *supervisor;   // main thread: signals, and workers that give up

    GstElement *appsrc;

    int input_fd;
    guint8 *frames_base;
    gsize mapped_size;
    gsize frame_size;
    gint total_frames;

    GThread *pacer_thread;
    gint running;

    GMutex stats_lock;
    gint64 push_time_us[BUDGET_RING];
    GstClockTime probe_pts;
    gint64 probe_last_us;
    gint budget_frames;
    gint64 budget_sum_us;
    gint64 budget_max_us;
    gint budget_overruns;
    gint64 last_report_us;
} WebRTCServer;

// ---------------------------------------------------------------------------
// Encoder selection, matching the UDP sender's settings.
// ---------------------------------------------------------------------------

static GstElement* create_encoder(WebRTCServer *data) {
    if (data->use_gpu) {
        GstElement *encoder = gst_element_factory_make("nvh264enc", "encoder");
        if (encoder) {
            mp_set_number_prop(encoder, "bitrate", data->bitrate / 1000);
            const gchar *const rc_modes[] = {"cbr-ld-hq", "cbr", NULL};
            mp_set_enum_prop(encoder, "rc-mode", rc_modes);
            const gchar *const presets[] = {"low-latency-hq", "low-latency", "p1", NULL};
            mp_set_enum_prop(encoder, "preset", presets);
            mp_set_number_prop(encoder, "gop-size", data->framerate);
            mp_set_number_prop(encoder, "bframes", 0);
            mp_set_number_prop(encoder, "rc-lookahead", 0);
            mp_set_bool_prop(encoder, "b-adapt", FALSE);
            mp_set_bool_prop(encoder, "zerolatency", TRUE);
            g_print("Using NVIDIA encoder: nvh264enc (%d kbit/s)\n", data->bitrate / 1000);
            return encoder;
        }
        g_print("nvh264enc unavailable, falling back to x264enc\n");
    }

    GstElement *encoder = gst_element_factory_make("x264enc", "encoder");
    if (!encoder) {
        return NULL;
    }
    const gchar *const ultrafast[] = {"ultrafast", NULL};
    const gchar *const zerolatency[] = {"zerolatency", NULL};
    mp_set_number_prop(encoder, "bitrate", data->bitrate / 1000);
    mp_set_enum_prop(encoder, "speed-preset", ultrafast);
    mp_set_enum_prop(encoder, "tune", zerolatency);
    mp_set_number_prop(encoder, "threads", 4);
    mp_set_number_prop(encoder, "bframes", 0);
    mp_set_number_prop(encoder, "key-int-max", data->framerate);
    g_print("Using CPU encoder: x264enc (%d kbit/s)\n", data->bitrate / 1000);
    return encoder;
}

// ---------------------------------------------------------------------------
// Memory-mapped input, bounded to loop_seconds so the working set stays in RAM.
// ---------------------------------------------------------------------------

static gboolean map_input_frames(WebRTCServer *data) {
    data->frame_size = (gsize)data->width * data->height * 3 / 2;  // I420

    data->input_fd = open(data->input_file, O_RDONLY);
    if (data->input_fd < 0) {
        g_printerr("Cannot open %s: %s\n", data->input_file, g_strerror(errno));
        return FALSE;
    }

    struct stat st;
    if (fstat(data->input_fd, &st) != 0) {
        g_printerr("Cannot stat %s: %s\n", data->input_file, g_strerror(errno));
        close(data->input_fd);
        data->input_fd = -1;
        return FALSE;
    }

    gint frames_in_file = (gint)((gsize)st.st_size / data->frame_size);
    if (frames_in_file < 1) {
        g_printerr("%s holds no complete %dx%d I420 frame\n",
                   data->input_file, data->width, data->height);
        close(data->input_fd);
        data->input_fd = -1;
        return FALSE;
    }

    gint wanted = data->loop_seconds * data->framerate;
    data->total_frames = (wanted > 0 && wanted < frames_in_file) ? wanted : frames_in_file;
    data->mapped_size = (gsize)data->total_frames * data->frame_size;

    void *base = mmap(NULL, data->mapped_size, PROT_READ, MAP_PRIVATE, data->input_fd, 0);
    if (base == MAP_FAILED) {
        g_printerr("mmap of %zu bytes failed: %s\n", data->mapped_size, g_strerror(errno));
        close(data->input_fd);
        data->input_fd = -1;
        return FALSE;
    }
    data->frames_base = (guint8 *)base;

    // Fault the working set in now, or the first pass reports false overruns.
    madvise(base, data->mapped_size, MADV_WILLNEED);
    volatile guint8 touch = 0;
    for (gsize off = 0; off < data->mapped_size; off += 4096) {
        touch ^= data->frames_base[off];
    }
    (void)touch;

    g_print("Mapped %d frames (%.1f s, %.0f MiB) from %s; file holds %d\n",
            data->total_frames, (gdouble)data->total_frames / data->framerate,
            data->mapped_size / (1024.0 * 1024.0), data->input_file, frames_in_file);
    return TRUE;
}

static void unmap_input_frames(WebRTCServer *data) {
    if (data->frames_base) {
        munmap(data->frames_base, data->mapped_size);
        data->frames_base = NULL;
    }
    if (data->input_fd >= 0) {
        close(data->input_fd);
        data->input_fd = -1;
    }
}

// Push one frame per period on absolute deadlines. This loop is the only thing
// pacing the video stream; audio is paced by its own live source, on its own
// thread, and the two never wait for each other.
static gpointer pacer_thread_func(gpointer user_data) {
    WebRTCServer *data = (WebRTCServer *)user_data;
    GstClockTime frame_duration = GST_SECOND / data->framerate;
    gint64 period_us = G_USEC_PER_SEC / data->framerate;
    gint64 next_us = g_get_monotonic_time();
    guint64 frame_index = 0;

    while (g_atomic_int_get(&data->running)) {
        gsize offset = (gsize)(frame_index % data->total_frames) * data->frame_size;

        GstBuffer *buf = gst_buffer_new_wrapped_full(
            GST_MEMORY_FLAG_READONLY, data->frames_base + offset,
            data->frame_size, 0, data->frame_size, NULL, NULL);

        // Ever-increasing counter, never the wrapped index: restarting timestamps
        // at the loop point would send RTP backwards and stall the receiver.
        GST_BUFFER_PTS(buf) = frame_index * frame_duration;
        GST_BUFFER_DTS(buf) = GST_BUFFER_PTS(buf);
        GST_BUFFER_DURATION(buf) = frame_duration;

        g_mutex_lock(&data->stats_lock);
        data->push_time_us[frame_index % BUDGET_RING] = g_get_monotonic_time();
        g_mutex_unlock(&data->stats_lock);

        GstFlowReturn fret = GST_FLOW_OK;
        g_signal_emit_by_name(data->appsrc, "push-buffer", buf, &fret);
        gst_buffer_unref(buf);
        if (fret != GST_FLOW_OK) {
            if (g_atomic_int_get(&data->running)) {
                g_printerr("push-buffer returned %s, stopping pacer\n",
                           gst_flow_get_name(fret));
            }
            break;
        }

        frame_index++;
        next_us += period_us;
        gint64 now = g_get_monotonic_time();
        if (next_us > now) {
            g_usleep(next_us - now);
        } else {
            next_us = now;  // resynchronise rather than sprint to catch up
        }
    }
    return NULL;
}

// Time each frame from the pacer to the point it leaves the payloader. A frame
// spans several RTP packets, so it is only complete once the timestamp changes.
static GstPadProbeReturn budget_probe(GstPad *pad, GstPadProbeInfo *info,
                                      gpointer user_data) {
    (void)pad;
    WebRTCServer *data = (WebRTCServer *)user_data;
    if (!(GST_PAD_PROBE_INFO_TYPE(info) & GST_PAD_PROBE_TYPE_BUFFER)) {
        return GST_PAD_PROBE_OK;
    }

    GstBuffer *buf = GST_PAD_PROBE_INFO_BUFFER(info);
    GstClockTime pts = GST_BUFFER_PTS(buf);
    gint64 now = g_get_monotonic_time();
    GstClockTime frame_duration = GST_SECOND / data->framerate;

    g_mutex_lock(&data->stats_lock);
    if (GST_CLOCK_TIME_IS_VALID(pts) && pts != data->probe_pts) {
        if (GST_CLOCK_TIME_IS_VALID(data->probe_pts) && data->probe_last_us > 0) {
            guint64 idx = data->probe_pts / frame_duration;
            gint64 pushed = data->push_time_us[idx % BUDGET_RING];
            if (pushed > 0) {
                gint64 elapsed = data->probe_last_us - pushed;
                data->budget_frames++;
                data->budget_sum_us += elapsed;
                if (elapsed > data->budget_max_us) {
                    data->budget_max_us = elapsed;
                }
                if (elapsed > G_USEC_PER_SEC / data->framerate) {
                    data->budget_overruns++;
                }
            }
        }
        data->probe_pts = pts;
    }
    data->probe_last_us = now;
    g_mutex_unlock(&data->stats_lock);

    return GST_PAD_PROBE_OK;
}

// Same line format as the UDP sender, so the test harness parses both.
static gboolean report_budget(gpointer user_data) {
    WebRTCServer *data = (WebRTCServer *)user_data;
    gint64 now = g_get_monotonic_time();
    gdouble seconds = (now - data->last_report_us) / (gdouble)G_USEC_PER_SEC;
    data->last_report_us = now;

    g_mutex_lock(&data->stats_lock);
    gint frames = data->budget_frames;
    gint64 sum = data->budget_sum_us;
    gint64 max = data->budget_max_us;
    gint overruns = data->budget_overruns;
    data->budget_frames = 0;
    data->budget_sum_us = 0;
    data->budget_max_us = 0;
    data->budget_overruns = 0;
    g_mutex_unlock(&data->stats_lock);

    gdouble budget_ms = 1000.0 / data->framerate;
    if (frames > 0 && seconds > 0.0) {
        g_print("Sent %.1f fps | push->wire avg %.1f ms max %.1f ms | "
                "budget %.1f ms | overruns %d\n",
                frames / seconds, (sum / (gdouble)frames) / 1000.0,
                max / 1000.0, budget_ms, overruns);
    } else {
        g_print("No frames sent in the last %.1f s\n", seconds);
    }
    return G_SOURCE_CONTINUE;
}

// ---------------------------------------------------------------------------
// Video pipeline. Unchanged from the version this latency was measured with;
// only the webrtcbin now comes from the shared session helper.
// ---------------------------------------------------------------------------

static gboolean build_video_pipeline(WebRTCServer *data) {
    GstElement *pipeline = gst_pipeline_new("webrtc-video-send");

    GstElement *appsrc        = gst_element_factory_make("appsrc", "src");
    // Deliberately present even though the pacer already pushes I420. nvh264enc
    // also advertises video/x-raw(memory:GLMemory), and through a tee the
    // negotiation can settle on GL memory, which appsrc cannot produce - the
    // pipeline then dies with not-negotiated before a single frame moves. This
    // pins the branch to system memory and normally negotiates to passthrough.
    GstElement *src_convert   = gst_element_factory_make("videoconvert", "src_convert");
    GstElement *tee           = gst_element_factory_make("tee", "t");
    GstElement *queue_enc     = gst_element_factory_make("queue", "queue_enc");
    GstElement *encoder       = create_encoder(data);
    GstElement *parser        = gst_element_factory_make("h264parse", "parser");
    GstElement *pay           = gst_element_factory_make("rtph264pay", "pay");
    GstElement *rtpcaps       = gst_element_factory_make("capsfilter", "rtpcaps");
    GstElement *queue_preview = gst_element_factory_make("queue", "queue_preview");
    GstElement *convert       = gst_element_factory_make("videoconvert", "preview_convert");
    GstElement *preview_sink  = gst_element_factory_make("autovideosink", "preview");

    if (!appsrc || !src_convert || !tee || !queue_enc || !encoder || !parser ||
        !pay || !rtpcaps || !queue_preview || !convert || !preview_sink) {
        g_printerr("Failed to create the video elements\n");
        gst_object_unref(pipeline);
        return FALSE;
    }
    data->appsrc = appsrc;

    GstCaps *raw_caps = gst_caps_new_simple(
        "video/x-raw",
        "format", G_TYPE_STRING, "I420",
        "width", G_TYPE_INT, data->width,
        "height", G_TYPE_INT, data->height,
        "framerate", GST_TYPE_FRACTION, data->framerate, 1, NULL);
    g_object_set(appsrc, "caps", raw_caps, "format", GST_FORMAT_TIME,
                 "is-live", TRUE, "do-timestamp", FALSE, "block", TRUE,
                 "max-bytes", (guint64)0, NULL);
    gst_caps_unref(raw_caps);

    // config-interval=-1 repeats SPS/PPS with every keyframe. WebRTC also needs
    // non-interleaved packetisation, which is the payloader's default mode 1.
    mp_set_number_prop(pay, "config-interval", -1);
    mp_set_number_prop(pay, "pt", 96);
    // Never hold a packet back hoping to aggregate another NAL into it.
    const gchar *const zerolatency[] = {"zero-latency", "none", NULL};
    mp_set_enum_prop(pay, "aggregate-mode", zerolatency);

    GstCaps *rtp_caps = gst_caps_from_string(
        "application/x-rtp,media=video,encoding-name=H264,payload=96,clock-rate=90000");
    g_object_set(rtpcaps, "caps", rtp_caps, NULL);
    gst_caps_unref(rtp_caps);

    // The preview must not be able to throttle the outgoing stream.
    g_object_set(queue_preview, "leaky", 2 /* downstream */,
                 "max-size-buffers", 1, NULL);
    g_object_set(queue_enc, "max-size-buffers", 2, NULL);
    mp_set_bool_prop(preview_sink, "sync", FALSE);

    gst_bin_add_many(GST_BIN(pipeline), appsrc, src_convert, tee, queue_enc,
                     encoder, parser, pay, rtpcaps, data->video_session.webrtcbin,
                     queue_preview, convert, preview_sink, NULL);

    if (!gst_element_link_many(appsrc, src_convert, tee, NULL) ||
        !gst_element_link_many(queue_enc, encoder, parser, pay, rtpcaps, NULL) ||
        !gst_element_link_many(queue_preview, convert, preview_sink, NULL)) {
        g_printerr("Failed to link the static parts of the video pipeline\n");
        gst_object_unref(pipeline);
        return FALSE;
    }
    if (!gst_element_link(tee, queue_enc) || !gst_element_link(tee, queue_preview)) {
        g_printerr("Failed to link the tee\n");
        gst_object_unref(pipeline);
        return FALSE;
    }

    // webrtcbin's sink pads are request pads, so this link is made by hand.
    GstPad *src_pad = gst_element_get_static_pad(rtpcaps, "src");
    GstPad *sink_pad = gst_element_get_request_pad(data->video_session.webrtcbin,
                                                  "sink_%u");
    if (!src_pad || !sink_pad ||
        gst_pad_link(src_pad, sink_pad) != GST_PAD_LINK_OK) {
        g_printerr("Failed to link the payloader into webrtcbin\n");
        if (src_pad) gst_object_unref(src_pad);
        if (sink_pad) gst_object_unref(sink_pad);
        gst_object_unref(pipeline);
        return FALSE;
    }
    // Measure at the payloader output: the last point before the RTP packet is
    // handed to encryption and the socket.
    gst_pad_add_probe(src_pad, GST_PAD_PROBE_TYPE_BUFFER, budget_probe, data, NULL);
    gst_object_unref(src_pad);
    gst_object_unref(sink_pad);

    gboolean relaxed = FALSE;
    mp_relax_sink_dropping(preview_sink, &relaxed);

    mp_worker_set_pipeline(data->video_worker, pipeline, NULL, NULL);
    data->last_report_us = g_get_monotonic_time();
    mp_worker_add_timeout(data->video_worker, 1000, report_budget, data);
    return TRUE;
}

// ---------------------------------------------------------------------------
// Audio pipelines
// ---------------------------------------------------------------------------

static gboolean build_audio_send_pipeline(WebRTCServer *data) {
    GstElement *pipeline = gst_pipeline_new("webrtc-audio-send");
    gst_bin_add(GST_BIN(pipeline), data->audio_tx_session.webrtcbin);
    mp_audio_stats_init(&data->audio_tx_stats, "audio-send", &data->audio,
                        data->audio.frame_ms);

    GstElement *tail = NULL;
    if (!mp_audio_webrtc_add_sender(GST_BIN(pipeline), &data->audio,
                                    data->audio_tx_session.webrtcbin,
                                    &data->audio_tx_stats, &tail)) {
        gst_object_unref(pipeline);
        return FALSE;
    }
    mp_webrtc_gate_attach(&data->audio_tx_gate, &data->audio_tx_session, tail,
                          MP_WEBRTC_VERIFY_SECONDS);
    mp_worker_set_pipeline(data->audio_tx_worker, pipeline, NULL, NULL);
    if (data->audio.stats) {
        mp_worker_add_timeout(data->audio_tx_worker, 1000, mp_audio_report_tx,
                              &data->audio_tx_stats);
    }
    return TRUE;
}

// The returning audio only exists once the operator's offer has been answered.
static void on_audio_stream(GstElement *webrtc, GstPad *pad, gpointer user_data) {
    (void)webrtc;
    WebRTCServer *data = (WebRTCServer *)user_data;
    if (GST_PAD_DIRECTION(pad) != GST_PAD_SRC) {
        return;
    }
    if (!mp_audio_webrtc_add_receiver(GST_BIN(data->audio_rx_worker->pipeline),
                                      &data->audio, pad, &data->audio_rx_stats)) {
        g_printerr("[audio-up] could not build the receive chain\n");
    }
}

// The jitterbuffer is the earliest probe point inside webrtcbin, and it only
// exists once negotiation has created the stream.
static void on_audio_jitterbuffer(GstElement *jitterbuffer, gpointer user_data) {
    WebRTCServer *data = (WebRTCServer *)user_data;
    if (!data->audio.stats) {
        return;
    }
    g_mutex_lock(&data->audio_rx_stats.lock);
    data->audio_rx_stats.jitterbuffer = jitterbuffer;
    data->audio_rx_stats.have_arrival_tap = TRUE;
    g_mutex_unlock(&data->audio_rx_stats.lock);
    mp_audio_tap(jitterbuffer, "sink", mp_audio_arrival_probe, &data->audio_rx_stats);
}

static gboolean build_audio_recv_pipeline(WebRTCServer *data) {
    GstElement *pipeline = gst_pipeline_new("webrtc-audio-recv");
    gst_bin_add(GST_BIN(pipeline), data->audio_rx_session.webrtcbin);
    mp_audio_stats_init(&data->audio_rx_stats, "audio-recv", &data->audio,
                        data->audio.frame_ms);
    mp_worker_set_pipeline(data->audio_rx_worker, pipeline, NULL, NULL);
    if (data->audio.stats) {
        mp_worker_add_timeout(data->audio_rx_worker, 1000, mp_audio_report_rx,
                              &data->audio_rx_stats);
    }
    return TRUE;
}

// ---------------------------------------------------------------------------
// Supervision. Video is essential; audio is not.
// ---------------------------------------------------------------------------

static void on_worker_failure(MpWorker *worker, gpointer user_data) {
    WebRTCServer *data = (WebRTCServer *)user_data;
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
    WebRTCServer *data = (WebRTCServer *)user_data;
    g_print("\nInterrupted; stopping\n");
    g_main_loop_quit(data->supervisor);
    return G_SOURCE_REMOVE;
}

static void run_server(WebRTCServer *data) {
    gboolean audio_send = (data->audio.codec != MP_AUDIO_OFF && data->audio.send &&
                           g_strcmp0(data->audio.source, "none") != 0);
    gboolean audio_recv = (data->audio.codec != MP_AUDIO_OFF && data->audio.receive);

    if (!map_input_frames(data)) {
        return;
    }

    data->video_worker = mp_worker_new("video");
    data->video_worker->on_failure = on_worker_failure;
    data->video_worker->on_failure_data = data;
    if (audio_send) {
        data->audio_tx_worker = mp_worker_new("audio-down");
        data->audio_tx_worker->on_failure = on_worker_failure;
        data->audio_tx_worker->on_failure_data = data;
    }
    if (audio_recv) {
        data->audio_rx_worker = mp_worker_new("audio-up");
        data->audio_rx_worker->on_failure = on_worker_failure;
        data->audio_rx_worker->on_failure_data = data;
    }

    // Signalling has to be up before anything goes to PLAYING: negotiation
    // starts as soon as a pipeline is live, and the offer needs somewhere to go.
    data->sig = mp_signal_new(data->pki);
    if (!mp_signal_connect(data->sig, data->host, data->port, 40)) {
        unmap_input_frames(data);
        return;
    }

    data->video_session.tag = "video";
    data->video_session.offerer = TRUE;
    data->video_session.worker = data->video_worker;
    data->video_session.sig = data->sig;
    data->video_session.pki = data->pki;
    data->video_session.stun_server = data->stun_server;
    data->video_session.jb_latency_ms = 5;
    data->video_session.jb_mode = "none";
    if (!mp_webrtc_session_create(&data->video_session, "video_send")) {
        unmap_input_frames(data);
        return;
    }

    if (audio_send) {
        data->audio_tx_session.tag = "audio-down";
        data->audio_tx_session.offerer = TRUE;
        data->audio_tx_session.worker = data->audio_tx_worker;
        data->audio_tx_session.sig = data->sig;
        data->audio_tx_session.pki = data->pki;
        data->audio_tx_session.stun_server = data->stun_server;
        data->audio_tx_session.jb_latency_ms = data->audio.jb_latency_ms;
        data->audio_tx_session.jb_mode = data->audio.jitter_mode;
        audio_send = mp_webrtc_session_create(&data->audio_tx_session, "audio_send");
    }
    if (audio_recv) {
        data->audio_rx_session.tag = "audio-up";
        data->audio_rx_session.offerer = FALSE;
        data->audio_rx_session.worker = data->audio_rx_worker;
        data->audio_rx_session.sig = data->sig;
        data->audio_rx_session.pki = data->pki;
        data->audio_rx_session.stun_server = data->stun_server;
        data->audio_rx_session.jb_latency_ms = data->audio.jb_latency_ms;
        data->audio_rx_session.jb_mode = data->audio.jitter_mode;
        data->audio_rx_session.jb_hook = on_audio_jitterbuffer;
        data->audio_rx_session.jb_hook_data = data;
        data->audio_rx_session.on_pad_added = on_audio_stream;
        data->audio_rx_session.user_data = data;
        audio_recv = mp_webrtc_session_create(&data->audio_rx_session, "audio_recv");
    }

    if (!build_video_pipeline(data)) {
        unmap_input_frames(data);
        return;
    }
    if (audio_send && !build_audio_send_pipeline(data)) {
        g_printerr("Outgoing audio disabled: its pipeline could not be built\n");
        audio_send = FALSE;
    }
    if (audio_recv && !build_audio_recv_pipeline(data)) {
        g_printerr("Incoming audio disabled: its pipeline could not be built\n");
        audio_recv = FALSE;
    }

    // Order matters here, and getting it wrong is silent. webrtcbin only starts
    // its internal task loop on the way to PAUSED, and anything enqueued before
    // that - set-remote-description in particular - is dropped without an error.
    // The peer's offer for the audio coming back is usually already sitting in
    // the socket by this point, so every pipeline goes to PLAYING first and the
    // signalling reader is started only afterwards. Sending does not need the
    // reader, so offers still go out immediately.
    if (!mp_worker_start(data->video_worker)) {
        unmap_input_frames(data);
        return;
    }
    if (audio_send) {
        mp_worker_start(data->audio_tx_worker);
    }
    if (audio_recv) {
        mp_worker_start(data->audio_rx_worker);
    }
    mp_signal_start(data->sig);

    g_print("Streaming %dx%d @ %d fps over WebRTC%s\n",
            data->width, data->height, data->framerate,
            (data->pki && data->pki->enabled) ? " (DTLS/SRTP with PKI identities)"
                                              : " (DTLS/SRTP, self-signed)");
    if (audio_send || audio_recv) {
        g_print("Audio: %s%s%s, %s, independent of video\n",
                audio_send ? "out" : "", (audio_send && audio_recv) ? " + " : "",
                audio_recv ? "back" : "", mp_audio_codec_name(data->audio.codec));
    }

    g_atomic_int_set(&data->running, 1);
    data->pacer_thread = g_thread_new("pacer", pacer_thread_func, data);

    g_unix_signal_add(SIGINT, on_interrupt, data);
    g_main_loop_run(data->supervisor);

    g_atomic_int_set(&data->running, 0);
    if (data->pacer_thread) {
        g_thread_join(data->pacer_thread);
        data->pacer_thread = NULL;
    }

    // Stop the workers before the signalling channel: a session that is still
    // negotiating would otherwise write to a closed stream.
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
    unmap_input_frames(data);
}

static void print_usage(const gchar *program, FILE *stream) {
    g_fprintf(stream, "Usage: %s <input_file> [options]\n", program);
    g_fprintf(stream, "\nStreams a raw I420 YUV file over WebRTC, with "
                      "bidirectional audio. Start the\nreceiver (webrtc_client) "
                      "first.\n\n");
    g_fprintf(stream, "Video:\n");
    g_fprintf(stream, "  --host <address>       Receiver's signalling address "
                      "(default: 127.0.0.1)\n");
    g_fprintf(stream, "  --port <port>          Receiver's signalling port "
                      "(default: 5020)\n");
    g_fprintf(stream, "  --width <pixels>       Video width (default: 1920)\n");
    g_fprintf(stream, "  --height <pixels>      Video height (default: 1080)\n");
    g_fprintf(stream, "  --framerate <fps>      Video framerate (default: 60)\n");
    g_fprintf(stream, "  --bitrate <bps>        Encoding bitrate (default: 4000000)\n");
    g_fprintf(stream, "  --loop-seconds <sec>   Seconds to map and loop "
                      "(default: 10, 0 = whole file)\n");
    g_fprintf(stream, "  --stun-server <uri>    stun://host:port (default: none, "
                      "not needed on a LAN)\n");
    g_fprintf(stream, "  --no-gpu               Disable GPU encoding\n");
    mp_audio_print_usage(stream);
    mp_pki_print_usage(stream);
}

int main(int argc, char *argv[]) {
    WebRTCServer data;
    memset(&data, 0, sizeof(data));

    data.host = g_strdup("127.0.0.1");
    data.port = 5020;
    data.width = 1920;
    data.height = 1080;
    data.framerate = 60;
    data.bitrate = 4000000;
    data.use_gpu = TRUE;
    data.loop_seconds = 10;
    data.stun_server = NULL;
    data.input_fd = -1;
    data.pki = mp_pki_new();
    // 440 Hz here, 660 Hz at the other end, so which direction you are hearing
    // is obvious without a spectrum analyser.
    mp_audio_config_defaults(&data.audio, 440);

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
        } else if (g_strcmp0(argv[i], "--width") == 0 && i + 1 < argc) {
            data.width = atoi(argv[++i]);
        } else if (g_strcmp0(argv[i], "--height") == 0 && i + 1 < argc) {
            data.height = atoi(argv[++i]);
        } else if (g_strcmp0(argv[i], "--framerate") == 0 && i + 1 < argc) {
            data.framerate = atoi(argv[++i]);
        } else if (g_strcmp0(argv[i], "--bitrate") == 0 && i + 1 < argc) {
            data.bitrate = atoi(argv[++i]);
        } else if (g_strcmp0(argv[i], "--loop-seconds") == 0 && i + 1 < argc) {
            data.loop_seconds = atoi(argv[++i]);
        } else if (g_strcmp0(argv[i], "--stun-server") == 0 && i + 1 < argc) {
            g_free(data.stun_server);
            data.stun_server = g_strdup(argv[++i]);
        } else if (g_strcmp0(argv[i], "--no-gpu") == 0) {
            data.use_gpu = FALSE;
        } else if (g_strcmp0(argv[i], "--help") == 0 || g_strcmp0(argv[i], "-h") == 0) {
            print_usage(argv[0], stdout);
            return 0;
        } else if (argv[i][0] != '-' && data.input_file == NULL) {
            data.input_file = g_strdup(argv[i]);
        }
    }

    if (data.input_file == NULL) {
        print_usage(argv[0], stderr);
        return 1;
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
    g_mutex_init(&data.stats_lock);
    data.probe_pts = GST_CLOCK_TIME_NONE;
    data.supervisor = g_main_loop_new(NULL, FALSE);

    run_server(&data);

    g_main_loop_unref(data.supervisor);
    g_mutex_clear(&data.stats_lock);
    mp_audio_config_clear(&data.audio);
    mp_pki_free(data.pki);
    g_free(data.input_file);
    g_free(data.host);
    g_free(data.stun_server);
    return 0;
}

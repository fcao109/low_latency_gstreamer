// RTP/UDP sender.
//
// Video is unchanged from the version this project's latency figures were
// measured with: frames are pushed from an mmap'ed working set through appsrc by
// a dedicated pacing thread on absolute deadlines, encoded with a zero-latency
// configuration, and previewed from the same tee so the local window shows a
// frame at the moment it is transmitted.
//
// Three streams now run, each as its own pipeline on its own thread (see
// media_worker.h) with nothing synchronised between them:
//
//   video       - out, port 5000
//   audio-down  - out, port 5002
//   audio-up    - in,  port 5004 (the audio coming back)
//
// With --pki-* supplied, every stream is encrypted with SRTP and the master keys
// are exchanged over a mutually authenticated TLS channel (port 5010). Without
// those options this is plain RTP exactly as before.

#include <glib.h>
#include <glib-unix.h>
#include <glib/gprintf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "utils.h"
#include "videoParameters.h"

#include "server.h"


// ---------------------------------------------------------------------------
// Version-tolerant property helpers.
//
// Element properties differ between GStreamer releases (for example nvh264enc
// gained "zerolatency" after 1.16, and enum values for "preset"/"rc-mode" have
// been renumbered). Setting a missing property emits a warning and setting a
// stale integer can select the wrong mode, so probe before assigning and
// resolve enums by name instead of by number.
//
// The same helpers exist as mp_* in media_props.h. These are kept because the
// video path below is the code the measurements were taken with, and it is not
// worth re-verifying it to remove a duplicate.
// ---------------------------------------------------------------------------

static GParamSpec* find_prop(GstElement *element, const gchar *name) {
    if (!element || !name) {
        return NULL;
    }
    return g_object_class_find_property(G_OBJECT_GET_CLASS(element), name);
}

// Assign a numeric property regardless of whether the plugin declares it as
// int/uint/int64/uint64, clamping to the range it advertises. Plugins disagree
// on the exact numeric type of properties such as "bitrate" and "bframes", and
// passing the wrong width through g_object_set() is undefined behaviour.
static gboolean set_number_prop(GstElement *element, const gchar *name, gint64 value) {
    GParamSpec *spec = find_prop(element, name);
    if (!spec) {
        return FALSE;
    }

    if (G_IS_PARAM_SPEC_INT(spec)) {
        GParamSpecInt *s = G_PARAM_SPEC_INT(spec);
        gint v = (gint)CLAMP(value, (gint64)s->minimum, (gint64)s->maximum);
        g_object_set(element, name, v, NULL);
    } else if (G_IS_PARAM_SPEC_UINT(spec)) {
        GParamSpecUInt *s = G_PARAM_SPEC_UINT(spec);
        if (value < 0) value = 0;
        guint v = (guint)CLAMP((guint64)value, (guint64)s->minimum, (guint64)s->maximum);
        g_object_set(element, name, v, NULL);
    } else if (G_IS_PARAM_SPEC_INT64(spec)) {
        GParamSpecInt64 *s = G_PARAM_SPEC_INT64(spec);
        gint64 v = CLAMP(value, s->minimum, s->maximum);
        g_object_set(element, name, v, NULL);
    } else if (G_IS_PARAM_SPEC_UINT64(spec)) {
        GParamSpecUInt64 *s = G_PARAM_SPEC_UINT64(spec);
        if (value < 0) value = 0;
        guint64 v = CLAMP((guint64)value, s->minimum, s->maximum);
        g_object_set(element, name, v, NULL);
    } else {
        return FALSE;
    }
    return TRUE;
}

static gboolean set_bool_prop(GstElement *element, const gchar *name, gboolean value) {
    if (!find_prop(element, name)) {
        return FALSE;
    }
    g_object_set(element, name, value, NULL);
    return TRUE;
}

// Assign an enum/flags property using the first nickname the installed plugin
// recognises. Returns the nickname that was applied, or NULL if none matched.
static const gchar* set_enum_prop(GstElement *element, const gchar *name,
                                  const gchar *const *nicknames) {
    GParamSpec *spec = find_prop(element, name);
    if (!spec || !nicknames) {
        return NULL;
    }

    if (G_TYPE_IS_ENUM(spec->value_type)) {
        GEnumClass *enum_class = G_ENUM_CLASS(g_type_class_ref(spec->value_type));
        const gchar *applied = NULL;
        for (gint i = 0; nicknames[i] != NULL && applied == NULL; i++) {
            GEnumValue *match = g_enum_get_value_by_nick(enum_class, nicknames[i]);
            if (match) {
                g_object_set(element, name, match->value, NULL);
                applied = nicknames[i];
            }
        }
        g_type_class_unref(enum_class);
        return applied;
    }

    // Flags and other serialisable types go through GStreamer's string parser.
    for (gint i = 0; nicknames[i] != NULL; i++) {
        gst_util_set_object_arg(G_OBJECT(element), name, nicknames[i]);
        return nicknames[i];
    }
    return NULL;
}

static const gchar* detect_gpu_encoder(ServerData *data) {
    if (!data->use_gpu) {
        return NULL;
    }

    GstElementFactory *factory;

    if (g_strcmp0(data->gpu_platform, "auto") != 0) {
        const gchar *encoder = NULL;
        if (g_strcmp0(data->gpu_platform, "nvidia") == 0) {
            encoder = g_strcmp0(data->codec, "h264") == 0 ? "nvh264enc" : "nvh265enc";
        } else if (g_strcmp0(data->gpu_platform, "intel") == 0 ||
                   g_strcmp0(data->gpu_platform, "amd") == 0) {
            encoder = g_strcmp0(data->codec, "h264") == 0 ? "vaapih264enc" : "vaapih265enc";
        }

        if (encoder) {
            factory = gst_element_factory_find(encoder);
            if (factory) {
                g_print("Using %s GPU encoder: %s\n", data->gpu_platform, encoder);
                gst_object_unref(factory);
                return encoder;
            }
        }
    } else {
        // Auto-detect, honouring the requested codec. NVIDIA is preferred over
        // VAAPI. "nvautogpu*enc" only exists on newer GStreamer (1.24+) and is
        // tried last so the plain CUDA encoder wins when both are present.
        const gchar *h264_encoders[] = {
            "nvh264enc", "nvautogpuh264enc", "vaapih264enc", "nvv4l2h264enc", NULL
        };
        const gchar *h265_encoders[] = {
            "nvh265enc", "nvautogpuh265enc", "vaapih265enc", "nvv4l2h265enc", NULL
        };
        const gchar **encoders = g_strcmp0(data->codec, "h264") == 0
            ? h264_encoders : h265_encoders;

        for (gint i = 0; encoders[i] != NULL; i++) {
            factory = gst_element_factory_find(encoders[i]);
            if (factory) {
                g_print("Auto-detected GPU encoder for %s: %s\n",
                        data->codec, encoders[i]);
                gst_object_unref(factory);
                return encoders[i];
            }
        }
    }

    g_print("No GPU encoder found, falling back to CPU encoder\n");
    return NULL;
}

// Apply low-latency NVENC settings. Only properties the installed plugin
// actually exposes are touched, so this works on 1.16 through current releases.
static void configure_nvenc_low_latency(GstElement *encoder, ServerData *data) {
    // Every nvcodec release expresses "bitrate" in kbit/sec.
    set_number_prop(encoder, "bitrate", data->bitrate / 1000);

    // Lowest-latency rate control the plugin offers. "cbr-ld-hq" is the
    // low-delay variant and is preferred when available.
    const gchar *const rc_modes[] = {"cbr-ld-hq", "cbr", NULL};
    const gchar *rc_mode = set_enum_prop(encoder, "rc-mode", rc_modes);

    // Preset nicknames are stable across releases even though the underlying
    // enum values have been renumbered more than once.
    const gchar *const presets[] = {"low-latency-hq", "low-latency", "p1", NULL};
    const gchar *preset = set_enum_prop(encoder, "preset", presets);

    // One keyframe per second keeps recovery fast without flooding the link.
    set_number_prop(encoder, "gop-size", data->framerate);

    // Anything that reorders or buffers frames adds latency.
    set_number_prop(encoder, "bframes", 0);
    set_number_prop(encoder, "rc-lookahead", 0);
    set_bool_prop(encoder, "b-adapt", FALSE);
    set_bool_prop(encoder, "zerolatency", TRUE);  // present from 1.18 onwards

    g_print("Configured NVENC: bitrate=%d kbit/s, rc-mode=%s, preset=%s, gop-size=%d\n",
            data->bitrate / 1000,
            rc_mode ? rc_mode : "plugin default",
            preset ? preset : "plugin default",
            data->framerate);
}

// Create and configure a software encoder tuned for minimal latency.
static GstElement* create_cpu_encoder(ServerData *data) {
    GstElement *encoder = NULL;
    const gchar *const ultrafast[] = {"ultrafast", NULL};
    const gchar *const zerolatency[] = {"zerolatency", NULL};

    if (g_strcmp0(data->codec, "h264") == 0) {
        encoder = gst_element_factory_make("x264enc", "encoder");
        if (!encoder) {
            return NULL;
        }
        set_number_prop(encoder, "bitrate", data->bitrate / 1000);  // kbit/sec
        set_enum_prop(encoder, "speed-preset", ultrafast);
        set_enum_prop(encoder, "tune", zerolatency);
        set_number_prop(encoder, "threads", 4);
        set_number_prop(encoder, "bframes", 0);
        set_number_prop(encoder, "key-int-max", data->framerate);
        g_print("Using CPU encoder: x264enc (%d kbit/s)\n", data->bitrate / 1000);
    } else {
        encoder = gst_element_factory_make("x265enc", "encoder");
        if (!encoder) {
            return NULL;
        }
        set_number_prop(encoder, "bitrate", data->bitrate / 1000);  // kbit/sec
        set_enum_prop(encoder, "speed-preset", ultrafast);
        set_enum_prop(encoder, "tune", zerolatency);
        // Disable lookahead, B-frames and scenecut detection for low latency.
        if (find_prop(encoder, "option-string")) {
            g_object_set(encoder, "option-string",
                         "rc-lookahead=0:bframes=0:no-scenecut=1", NULL);
        }
        g_print("Using CPU encoder: x265enc (%d kbit/s)\n", data->bitrate / 1000);
    }

    return encoder;
}

// ---------------------------------------------------------------------------
// Frame source: memory-mapped input driven by an explicit pacing loop.
//
// mmap avoids copying the file into the process while still giving pointer
// access to every frame. The mapped region is deliberately limited to
// loop_seconds worth of video: mapping a multi-gigabyte file would exceed RAM,
// and the loop would then fault pages back in from disk on every pass, which is
// exactly the non-determinism this design is meant to remove.
// ---------------------------------------------------------------------------

static gboolean map_input_frames(ServerData *data) {
#ifdef FILE_TEST
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
        g_printerr("%s holds no complete %dx%d I420 frame (size %ld, frame %zu)\n",
                   data->input_file, data->width, data->height,
                   (long)st.st_size, data->frame_size);
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

    // Fault the whole working set in now. Without this, the first pass through the
    // loop would stall on page faults and report false budget overruns. madvise
    // only hints, so touch one byte per page to force the mapping to be resident.
    madvise(base, data->mapped_size, MADV_WILLNEED);
    volatile guint8 touch = 0;
    for (gsize off = 0; off < data->mapped_size; off += 4096) {
        touch ^= data->frames_base[off];
    }
    (void)touch;

    g_print("Mapped %d frames (%.1f s, %.0f MiB) from %s; file holds %d\n",
            data->total_frames,
            (gdouble)data->total_frames / data->framerate,
            data->mapped_size / (1024.0 * 1024.0),
            data->input_file, frames_in_file);
#endif
    return TRUE;
}

static void unmap_input_frames(ServerData *data) {
    if (data->frames_base) {
        munmap(data->frames_base, data->mapped_size);
        data->frames_base = NULL;
    }
    if (data->input_fd >= 0) {
        close(data->input_fd);
        data->input_fd = -1;
    }
}

// Push one frame per frame period, on absolute deadlines. This loop is the only
// thing pacing the video stream, which is what lets both the preview and the
// encoder see each frame at the same instant. Audio is paced by its own live
// source on its own thread and the two never wait for each other.
#ifdef FILE_TEST
static gpointer pacer_thread_func(gpointer user_data) {
    ServerData *data = (ServerData *)user_data;
    GstClockTime frame_duration = GST_SECOND / data->framerate;
    gint64 period_us = G_USEC_PER_SEC / data->framerate;
    gint64 next_us = g_get_monotonic_time();
    guint64 frame_index = 0;

    while (g_atomic_int_get(&data->running)) {
        gsize offset = (gsize)(frame_index % data->total_frames) * data->frame_size;

        // Wrap the mapped pages directly instead of copying ~3 MB per frame. The
        // mapping outlives the pipeline, so no free function is needed.
        GstBuffer *buf = gst_buffer_new_wrapped_full(
            GST_MEMORY_FLAG_READONLY, data->frames_base + offset,
            data->frame_size, 0, data->frame_size, NULL, NULL);

        // Timestamps come from an ever-increasing counter, never from the wrapped
        // index. Restarting them at the loop point would send the encoder and RTP
        // timestamps backwards and stall the receiver's jitterbuffer.
        GST_BUFFER_PTS(buf) = frame_index * frame_duration;
        GST_BUFFER_DTS(buf) = GST_BUFFER_PTS(buf);
        GST_BUFFER_DURATION(buf) = frame_duration;
        //g_print("frame_index=%ld,GST_BUFFER_PTS(buf)=%ld\n",frame_index,GST_BUFFER_PTS(buf));

        g_mutex_lock(&data->stats_lock);
        data->push_time_us[frame_index % BUDGET_RING] = g_get_monotonic_time();
        //g_print("frame_index=%ld(%ld),pushed=%ld,frame_duration=%ld\n",frame_index,frame_index % BUDGET_RING,data->push_time_us[frame_index % BUDGET_RING],frame_duration);
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

        // Advance the deadline rather than sleeping a fixed period, so scheduling
        // jitter cannot accumulate into drift.
        next_us += period_us;
        gint64 now = g_get_monotonic_time();
        if (next_us > now) {
            g_usleep(next_us - now);
        } else {
            // Fell behind: resynchronise instead of sprinting to catch up, which
            // would burst frames onto the wire faster than real time.
            next_us = now;
        }
    }
    return NULL;
}
#else
static gpointer pacer_thread_func(gpointer user_data) {
    ServerData *data = (ServerData *)user_data;
    GstClockTime frame_duration = GST_SECOND / data->framerate;
    gint64 period_us = G_USEC_PER_SEC / data->framerate;
    gint64 next_us = g_get_monotonic_time();
    guint64 frame_index = 0;

    while (g_atomic_int_get(&data->running)) {
        if (data->capture_frame_cnt != frame_index) {
            gst_element_set_state(data->pipeline, GST_STATE_PLAYING);
            // printf("%s %d: %ld %ld\n",__func__,__LINE__, data->capture_frame_cnt, frame_index);
            unsigned char* rawbuff = data->frameQueue->pop();

            #ifdef SERVER_LOCAL_RENDERING_CV    // dbg only, need to use while loop instead of g_main_loop_run
            cv::Mat buff = convertYUV420Frame(rawbuff, data->width, data->height);
            if (!buff.empty()) {
                cv::imshow("console-sss", buff);
                cv::waitKey(1);
            }
            frame_index++;
            continue;
            #endif

            // Wrap the mapped pages directly instead of copying ~3 MB per frame. The
            // mapping outlives the pipeline, so no free function is needed.
            GstBuffer *buf = gst_buffer_new_wrapped_full(
                GST_MEMORY_FLAG_READONLY, rawbuff,
                data->frame_size, 0, data->frame_size, NULL, NULL);

            // Timestamps come from an ever-increasing counter, never from the wrapped
            // index. Restarting them at the loop point would send the encoder and RTP
            // timestamps backwards and stall the receiver's jitterbuffer.
            GST_BUFFER_PTS(buf) = frame_index * frame_duration;
            GST_BUFFER_DTS(buf) = GST_BUFFER_PTS(buf);
            GST_BUFFER_DURATION(buf) = frame_duration;

            g_mutex_lock(&data->stats_lock);
            data->push_time_us[frame_index % BUDGET_RING] = g_get_monotonic_time();
            // g_print("frame_index=%ld(%ld),pushed=%ld,frame_duration=%ld\n",frame_index,frame_index % BUDGET_RING,data->push_time_us[frame_index % BUDGET_RING],frame_duration);
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

            // Advance the deadline rather than sleeping a fixed period, so scheduling
            // jitter cannot accumulate into drift.
            next_us += period_us;
            gint64 now = g_get_monotonic_time();
            if (next_us > now) {
                g_usleep(next_us - now);
            } else {
                // Fell behind: resynchronise instead of sprinting to catch up, which
                // would burst frames onto the wire faster than real time.
                next_us = now;
            }
        }
    }
    return NULL;
}
#endif

// Measure how long each frame takes to get from the pacer to the wire. The
// payloader emits several RTP packets per frame, so the frame is only complete
// once the timestamp changes; the time of that frame's last packet is used.
static GstPadProbeReturn budget_probe(GstPad *pad, GstPadProbeInfo *info,
                                      gpointer user_data) {
    ServerData *data = (ServerData *)user_data;
    if (!(GST_PAD_PROBE_INFO_TYPE(info) & GST_PAD_PROBE_TYPE_BUFFER)) {
        return GST_PAD_PROBE_OK;
    }

    GstBuffer *buf = GST_PAD_PROBE_INFO_BUFFER(info);
    GstClockTime pts = GST_BUFFER_PTS(buf);
    gint64 now = g_get_monotonic_time();
    GstClockTime frame_duration = GST_SECOND / data->framerate;

    g_mutex_lock(&data->stats_lock);
    // g_print("pts=%ld,data->probe_pts=%ld,data->probe_base_pts=%ld\n",pts,data->probe_pts, data->probe_base_pts);
    if (GST_CLOCK_TIME_IS_VALID(pts) && pts != data->probe_pts) {
        if (GST_CLOCK_TIME_IS_VALID(data->probe_pts) && data->probe_last_us > 0) {
            //guint64 idx = data->probe_pts / frame_duration;
            guint64 idx = (data->probe_pts - data->probe_base_pts) / frame_duration;
            // g_print("idx=%ld(%ld),data->probe_pts=%ld,frame_duration=%ld\n",idx,idx % BUDGET_RING,data->probe_pts,frame_duration);
            gint64 pushed = data->push_time_us[idx % BUDGET_RING];
            if (pushed > 0) {
                gint64 elapsed = data->probe_last_us - pushed;
                data->budget_frames++;
                data->budget_sum_us += elapsed;
                // g_print("now=%ld,idx=%ld,data->probe_last_us=%ld,pushed=%ld\n",now,idx,data->probe_last_us,pushed);
                // g_print("data->budget_sum_us=%ld,data->budget_frames=%ld\n",data->budget_sum_us,data->budget_frames);
                if (elapsed > data->budget_max_us) {
                    data->budget_max_us = elapsed;
                }
                if (elapsed > G_USEC_PER_SEC / data->framerate) {
                    data->budget_overruns++;
                }
            }
        }
        if (!GST_CLOCK_TIME_IS_VALID(data->probe_pts)) {
            data->probe_base_pts = pts;
            // g_print("data->probe_base_pts=%ld\n",data->probe_base_pts);
        }
        data->probe_pts = pts;
    }
    data->probe_last_us = now;
    g_mutex_unlock(&data->stats_lock);

    return GST_PAD_PROBE_OK;
}

// Report throughput and the per-frame budget once per second.
static gboolean report_budget(gpointer user_data) {
    ServerData *data = (ServerData *)user_data;
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
    // g_print("sum=%d,frames=%d\n",sum,frames);
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

#if 0
static gboolean build_pipeline(ServerData *data) {
    const gchar *gpu_encoder = detect_gpu_encoder(data);

    // Create encoder first before any other elements to avoid plugin state issues
    GstElement *encoder = NULL;
    GstElement *rtp_pay = NULL;
    GstElement *udpsink = NULL;

    gboolean using_gpu_encoder = FALSE;

    if (gpu_encoder) {
        g_print("Attempting to create GPU encoder: %s\n", gpu_encoder);
        encoder = gst_element_factory_make(gpu_encoder, "encoder");

        if (!encoder) {
            g_printerr("Could not instantiate %s (plugin present but element "
                       "creation failed - check driver libraries)\n", gpu_encoder);
        } else if (g_str_has_prefix(gpu_encoder, "nv")) {
            configure_nvenc_low_latency(encoder, data);
            using_gpu_encoder = TRUE;
        } else {
            // VAAPI expresses bitrate in kbit/sec as well.
            set_number_prop(encoder, "bitrate", data->bitrate / 1000);
            set_number_prop(encoder, "keyframe-period", data->framerate);
            g_print("Configured VAAPI encoder: %s\n", gpu_encoder);
            using_gpu_encoder = TRUE;
        }
    }

    if (!encoder) {
        g_print("Falling back to CPU encoding\n");
        encoder = create_cpu_encoder(data);
    }

    if (!encoder) {
        g_printerr("Failed to create any encoder\n");
        return FALSE;
    }

    data->using_gpu_encoder = using_gpu_encoder;

    // Create pipeline programmatically to handle tee element
    data->pipeline = gst_pipeline_new("video-streaming-pipeline");
    if (!data->pipeline) {
        g_printerr("Failed to create pipeline\n");
        return FALSE;
    }

    GstClock *clock = gst_system_clock_obtain();
    g_object_set(clock,
                "clock-type", GST_CLOCK_TYPE_MONOTONIC,
                NULL);
    gst_pipeline_use_clock(GST_PIPELINE_CAST(data->pipeline), clock);
    gst_object_unref(clock);

    // Create remaining elements. Frames are injected by the pacing loop rather
    // than pulled from disk, so the source is appsrc.
#ifdef FILE_TEST
    GstElement *appsrc = gst_element_factory_make("appsrc", "appsrc");
#else
#if 1
    // use buffer from video_src
    GstElement *appsrc = gst_element_factory_make("appsrc", "appsrc");
#else
    // original frames
    GstElement *appsrc = gst_element_factory_make("v4l2src", "appsrc");
    g_object_set(appsrc, "device", "/dev/video0", NULL);
#endif
#endif
    // The loop already pushes I420, which both NVENC and the preview sink accept,
    // so this normally negotiates to passthrough and costs nothing. It stays as a
    // safety net for sinks that cannot take I420 directly.
    GstElement *videoconvert = gst_element_factory_make("videoconvert", "videoconvert");
    GstElement *tee = gst_element_factory_make("tee", "tee");
    GstElement *queue_display = gst_element_factory_make("queue", "queue_display");
    GstElement *autovideosink = gst_element_factory_make("autovideosink", "autovideosink");
    GstElement *queue_encode = gst_element_factory_make("queue", "queue_encode");
    GstElement *fakesink = gst_element_factory_make ("fakesink", "fakesink");

    if (!appsrc || !videoconvert || !tee || !queue_display ||
        !autovideosink || !queue_encode || !fakesink) {
        g_printerr("Failed to create elements\n");
        return FALSE;
    }
    data->appsrc = appsrc;

    // Describe exactly what the pacing loop will push. The loop stamps every
    // buffer itself, so do-timestamp stays off.
    GstCaps *src_caps = gst_caps_new_simple(
        "video/x-raw",
        "format", G_TYPE_STRING, "I420",
        "width", G_TYPE_INT, data->width,
        "height", G_TYPE_INT, data->height,
        "framerate", GST_TYPE_FRACTION, data->framerate, 1,
        NULL);
    g_object_set(appsrc,
                 "caps", src_caps,
                 "format", GST_FORMAT_TIME,
                 "is-live", TRUE,
                 "do-timestamp", FALSE,
                 // Block instead of silently dropping when downstream is behind,
                 // so a slow encoder shows up as a measured budget overrun.
                 "block", TRUE,
                 "max-bytes", (guint64)(2 * data->frame_size),
                 NULL);
    gst_caps_unref(src_caps);

    // Configure queues for low latency
    g_object_set(queue_display, "max-size-buffers", 1, "max-size-time", 0, "max-size-bytes", 0, NULL);
    g_object_set(queue_encode, "max-size-buffers", 1, "max-size-time", 0, "max-size-bytes", 0, NULL);

    // Drop stale preview frames rather than back-pressuring the tee. Pacing no
    // longer depends on the preview, so a slow local display can no longer
    // throttle the outgoing stream.
    const gchar *const leak_downstream[] = {"downstream", NULL};
    set_enum_prop(queue_display, "leaky", leak_downstream);

    // Show each frame as soon as it arrives. The pacing loop already released it
    // at the right moment, so waiting on the clock again here would put the
    // preview one or two frames behind what has already gone out on the wire -
    // exactly the skew that makes a side-by-side latency photo read too low.
    set_bool_prop(autovideosink, "sync", FALSE);

    // Create RTP payload element
    if (g_strcmp0(data->codec, "h264") == 0) {
        rtp_pay = gst_element_factory_make("rtph264pay", "rtp_pay");
    } else {
        rtp_pay = gst_element_factory_make("rtph265pay", "rtp_pay");
    }

    if (!rtp_pay) {
        g_printerr("Failed to create RTP payload element\n");
        return FALSE;
    }
    g_object_set(rtp_pay, "pt", 96, NULL);
    // Resend codec configuration (SPS/PPS) with every IDR frame. The default of
    // 0 sends it only once at startup, which leaves any receiver that attaches
    // later unable to initialise its decoder - it would never show a picture.
    set_number_prop(rtp_pay, "config-interval", -1);
    // Emit each NAL as soon as it is produced rather than aggregating them.
    // Only present on newer releases, where it is an enum rather than a boolean.
    const gchar *const aggregate[] = {"zero-latency", "none", NULL};
    set_enum_prop(rtp_pay, "aggregate-mode", aggregate);

    // Create UDP sink
    udpsink = gst_element_factory_make("udpsink", "udpsink");
    if (!udpsink) {
        g_printerr("Failed to create UDP sink\n");
        return FALSE;
    }
    g_object_set(udpsink, "host", data->host, "port", data->port, "sync", FALSE, "max-lateness", 0, NULL);

    // Add all elements to pipeline
    gst_bin_add_many(GST_BIN(data->pipeline), appsrc, videoconvert, tee,
                     queue_display,
#ifdef SERVER_LOCAL_RENDERING
                     autovideosink,
#else
                     fakesink,
#endif
                     queue_encode, encoder, rtp_pay, udpsink, NULL);

    // Link elements - source to tee
    if (!gst_element_link_many(appsrc, videoconvert, tee, NULL)) {
        g_printerr("Failed to link source elements\n");
        return FALSE;
    }

    // Link tee to display branch
    GstPad *tee_src_display = gst_element_request_pad_simple(tee, "src_%u");
    GstPad *queue_display_sink = gst_element_get_static_pad(queue_display, "sink");
    if (gst_pad_link(tee_src_display, queue_display_sink) != GST_PAD_LINK_OK) {
        g_printerr("Failed to link tee to display queue\n");
        return FALSE;
    }
    gst_object_unref(tee_src_display);
    gst_object_unref(queue_display_sink);

#ifdef SERVER_LOCAL_RENDERING
    if (!gst_element_link(queue_display, autovideosink)) {
#else
    if (!gst_element_link(queue_display, fakesink)) {
#endif
        g_printerr("Failed to link display queue to videosink\n");
        return FALSE;
    }

    // Link tee to encode branch
    GstPad *tee_src_encode = gst_element_request_pad_simple(tee, "src_%u");
    GstPad *queue_encode_sink = gst_element_get_static_pad(queue_encode, "sink");
    if (gst_pad_link(tee_src_encode, queue_encode_sink) != GST_PAD_LINK_OK) {
        g_printerr("Failed to link tee to encode queue\n");
        return FALSE;
    }
    gst_object_unref(tee_src_encode);
    gst_object_unref(queue_encode_sink);

    if (!gst_element_link_many(queue_encode, encoder, rtp_pay, NULL)) {
        g_printerr("Failed to link encode elements\n");
        return FALSE;
    }

    // Encryption goes between the payloader and the socket, so the budget probe
    // on the udpsink still measures everything that happens to a frame before it
    // leaves the process. SRTP leaves the RTP header in the clear, so the
    // receiver can still read timestamps for its own measurements.
    if (data->srtp_video.enabled) {
        GstPad *src = mp_srtp_insert_sender(GST_BIN(data->pipeline), &data->srtp_video,
                                           rtp_pay, "videosrtpenc");
        GstPad *sink = gst_element_get_static_pad(udpsink, "sink");
        if (!src || !sink || gst_pad_link(src, sink) != GST_PAD_LINK_OK) {
            g_printerr("Failed to link srtpenc to the video udpsink\n");
            if (src) gst_object_unref(src);
            if (sink) gst_object_unref(sink);
            return FALSE;
        }
        gst_object_unref(src);
        gst_object_unref(sink);
    } else if (!gst_element_link(rtp_pay, udpsink)) {
        g_printerr("Failed to link the payloader to the udpsink\n");
        return FALSE;
    }

    g_print("Pipeline built with display and streaming branches (%s encoding)\n",
            data->using_gpu_encoder ? "GPU" : "CPU");

    return TRUE;
}
#else   // feng
static gboolean build_pipeline_orin(ServerData *data) {
    const gchar *gpu_encoder = detect_gpu_encoder(data);

    // Create encoder first before any other elements to avoid plugin state issues
    GstElement *encoder = NULL;
    GstElement *rtp_pay = NULL;
    GstElement *udpsink = NULL;

    gboolean using_gpu_encoder = FALSE;

    if (gpu_encoder) {
        g_print("Attempting to create GPU encoder: %s\n", gpu_encoder);
        encoder = gst_element_factory_make(gpu_encoder, "encoder");

        if (!encoder) {
            g_printerr("Could not instantiate %s (plugin present but element "
                       "creation failed - check driver libraries)\n", gpu_encoder);
        } else if (g_str_has_prefix(gpu_encoder, "nv")) {
            configure_nvenc_low_latency(encoder, data);
            using_gpu_encoder = TRUE;
        } else {
            // VAAPI expresses bitrate in kbit/sec as well.
            set_number_prop(encoder, "bitrate", data->bitrate / 1000);
            set_number_prop(encoder, "keyframe-period", data->framerate);
            g_print("Configured VAAPI encoder: %s\n", gpu_encoder);
            using_gpu_encoder = TRUE;
        }

        if (g_str_has_prefix(gpu_encoder, "nvv4l2")) {
            g_print("Configured nvv4l2 encoder: %s\n", gpu_encoder);
            g_object_set(G_OBJECT(encoder), "maxperf-enable", TRUE, NULL);
            g_object_set(G_OBJECT(encoder), "poc-type", 2, NULL);
        }
    }

    if (!encoder) {
        g_print("Falling back to CPU encoding\n");
        encoder = create_cpu_encoder(data);
    }

    if (!encoder) {
        g_printerr("Failed to create any encoder\n");
        return FALSE;
    }

    data->using_gpu_encoder = using_gpu_encoder;

    // Create pipeline programmatically to handle tee element
    data->pipeline = gst_pipeline_new("video-streaming-pipeline");
    if (!data->pipeline) {
        g_printerr("Failed to create pipeline\n");
        return FALSE;
    }

    GstClock *clock = gst_system_clock_obtain();
    g_object_set(clock,
                "clock-type", GST_CLOCK_TYPE_MONOTONIC,
                NULL);
    gst_pipeline_use_clock(GST_PIPELINE_CAST(data->pipeline), clock);
    gst_object_unref(clock);

    // Create remaining elements. Frames are injected by the pacing loop rather
    // than pulled from disk, so the source is appsrc.
#ifdef FILE_TEST
    GstElement *appsrc = gst_element_factory_make("appsrc", "appsrc");
#else
#if 1
    // use buffer from video_src
    GstElement *appsrc = gst_element_factory_make("appsrc", "appsrc");
#else
    // original frames
    GstElement *appsrc = gst_element_factory_make("v4l2src", "appsrc");
    g_object_set(appsrc, "device", "/dev/video0", NULL);
#endif
#endif
    // The loop already pushes I420, which both NVENC and the preview sink accept,
    // so this normally negotiates to passthrough and costs nothing. It stays as a
    // safety net for sinks that cannot take I420 directly.
    GstElement *videoconvert = gst_element_factory_make("videoconvert", "videoconvert");
    GstElement *tee = gst_element_factory_make("tee", "tee");
    GstElement *queue_display = gst_element_factory_make("queue", "queue_display");
    GstElement *autovideosink = gst_element_factory_make("autovideosink", "autovideosink");
    GstElement *queue_encode = gst_element_factory_make("queue", "queue_encode");
    GstElement *fakesink = gst_element_factory_make ("fakesink", "fakesink");
    GstElement *nvvidconv = gst_element_factory_make("nvvidconv", "nvvidconv");
    GstElement *videotestsrc = gst_element_factory_make("videotestsrc", "videotestsrc");

    GstElement *parse;
    if (g_strcmp0(data->codec, "h264") == 0) {
        parse = gst_element_factory_make("h264parse", "parse");
    } else {
        parse = gst_element_factory_make("h265parse", "parse");
    }

    if (!appsrc || !videoconvert || !tee || !queue_display ||
        !autovideosink || !queue_encode || !fakesink) {
        g_printerr("Failed to create elements\n");
        return FALSE;
    }
    data->appsrc = appsrc;

    printf("%s %d: Resolution: %dx%d\n",__func__,__LINE__, data->width, data->height);

    // Describe exactly what the pacing loop will push. The loop stamps every
    // buffer itself, so do-timestamp stays off.
    GstCaps *src_caps = gst_caps_new_simple(
        "video/x-raw",
        "format", G_TYPE_STRING, "I420",
        "width", G_TYPE_INT, data->width,
        "height", G_TYPE_INT, data->height,
        "framerate", GST_TYPE_FRACTION, data->framerate, 1,
        NULL);
    g_object_set(appsrc,
                 "caps", src_caps,
                 "format", GST_FORMAT_TIME,
                 "is-live", TRUE,
                 "do-timestamp", FALSE,
                 // Block instead of silently dropping when downstream is behind,
                 // so a slow encoder shows up as a measured budget overrun.
                 "block", TRUE,
                 "max-bytes", (guint64)(2 * data->frame_size),
                 NULL);
    gst_caps_unref(src_caps);

    GstElement *capsfilter = gst_element_factory_make("capsfilter", "filter");
    GstCaps *caps = gst_caps_new_simple("video/x-raw",
                    "width", G_TYPE_INT, data->width,
                    "height", G_TYPE_INT, data->height,
                    "framerate", GST_TYPE_FRACTION, data->framerate, 1,
                    NULL);
    g_object_set(G_OBJECT(capsfilter), "caps", caps, NULL);
    gst_caps_unref(caps);

    // Configure queues for low latency
    g_object_set(queue_display, "max-size-buffers", 1, "max-size-time", 0, "max-size-bytes", 0, NULL);
    g_object_set(queue_encode, "max-size-buffers", 1, "max-size-time", 0, "max-size-bytes", 0, NULL);

    // Drop stale preview frames rather than back-pressuring the tee. Pacing no
    // longer depends on the preview, so a slow local display can no longer
    // throttle the outgoing stream.
    const gchar *const leak_downstream[] = {"downstream", NULL};
    set_enum_prop(queue_display, "leaky", leak_downstream);

    // Show each frame as soon as it arrives. The pacing loop already released it
    // at the right moment, so waiting on the clock again here would put the
    // preview one or two frames behind what has already gone out on the wire -
    // exactly the skew that makes a side-by-side latency photo read too low.
    set_bool_prop(autovideosink, "sync", FALSE);

    // Create RTP payload element
    if (g_strcmp0(data->codec, "h264") == 0) {
        rtp_pay = gst_element_factory_make("rtph264pay", "rtp_pay");
    } else {
        rtp_pay = gst_element_factory_make("rtph265pay", "rtp_pay");
    }

    if (!rtp_pay) {
        g_printerr("Failed to create RTP payload element\n");
        return FALSE;
    }
    g_object_set(rtp_pay, "pt", 96, NULL);
    // Resend codec configuration (SPS/PPS) with every IDR frame. The default of
    // 0 sends it only once at startup, which leaves any receiver that attaches
    // later unable to initialise its decoder - it would never show a picture.
    set_number_prop(rtp_pay, "config-interval", -1);
    // Emit each NAL as soon as it is produced rather than aggregating them.
    // Only present on newer releases, where it is an enum rather than a boolean.
    const gchar *const aggregate[] = {"zero-latency", "none", NULL};
    set_enum_prop(rtp_pay, "aggregate-mode", aggregate);

    // Create UDP sink
    udpsink = gst_element_factory_make("udpsink", "udpsink");
    if (!udpsink) {
        g_printerr("Failed to create UDP sink\n");
        return FALSE;
    }
    g_object_set(udpsink, "host", data->host, "port", data->port, "sync", FALSE, "max-lateness", 0, NULL);

#if 0
    // Add all elements to pipeline
    gst_bin_add_many(GST_BIN(data->pipeline), appsrc, videoconvert, tee, nvvidconv,
                     queue_display,
#ifdef SERVER_LOCAL_RENDERING
                     autovideosink,
#else
                     fakesink,
#endif
                     queue_encode, encoder, rtp_pay, udpsink, NULL);
#else
    gst_bin_add_many(GST_BIN(data->pipeline), appsrc, nvvidconv, encoder, parse, rtp_pay, udpsink, NULL);
#endif

    // Link elements - source to tee
    if (!gst_element_link_many(appsrc, nvvidconv, encoder, rtp_pay, udpsink, NULL)) {
        g_printerr("Failed to link source elements\n");
        return FALSE;
    }

#if 0
    // Link tee to display branch
    GstPad *tee_src_display = gst_element_request_pad_simple(tee, "src_%u");
    GstPad *queue_display_sink = gst_element_get_static_pad(queue_display, "sink");
    if (gst_pad_link(tee_src_display, queue_display_sink) != GST_PAD_LINK_OK) {
        g_printerr("Failed to link tee to display queue\n");
        return FALSE;
    }
    gst_object_unref(tee_src_display);
    gst_object_unref(queue_display_sink);

#ifdef SERVER_LOCAL_RENDERING
    if (!gst_element_link(queue_display, autovideosink)) {
#else
    if (!gst_element_link(queue_display, fakesink)) {
#endif
        g_printerr("Failed to link display queue to videosink\n");
        return FALSE;
    }

    // Link tee to encode branch
    GstPad *tee_src_encode = gst_element_request_pad_simple(tee, "src_%u");
    GstPad *queue_encode_sink = gst_element_get_static_pad(queue_encode, "sink");
    if (gst_pad_link(tee_src_encode, queue_encode_sink) != GST_PAD_LINK_OK) {
        g_printerr("Failed to link tee to encode queue\n");
        return FALSE;
    }
    gst_object_unref(tee_src_encode);
    gst_object_unref(queue_encode_sink);

    if (!gst_element_link_many(queue_encode, encoder, rtp_pay, NULL)) {
        g_printerr("Failed to link encode elements\n");
        return FALSE;
    }

    // Encryption goes between the payloader and the socket, so the budget probe
    // on the udpsink still measures everything that happens to a frame before it
    // leaves the process. SRTP leaves the RTP header in the clear, so the
    // receiver can still read timestamps for its own measurements.
    if (data->srtp_video.enabled) {
        GstPad *src = mp_srtp_insert_sender(GST_BIN(data->pipeline), &data->srtp_video,
                                           rtp_pay, "videosrtpenc");
        GstPad *sink = gst_element_get_static_pad(udpsink, "sink");
        if (!src || !sink || gst_pad_link(src, sink) != GST_PAD_LINK_OK) {
            g_printerr("Failed to link srtpenc to the video udpsink\n");
            if (src) gst_object_unref(src);
            if (sink) gst_object_unref(sink);
            return FALSE;
        }
        gst_object_unref(src);
        gst_object_unref(sink);
    } else if (!gst_element_link(rtp_pay, udpsink)) {
        g_printerr("Failed to link the payloader to the udpsink\n");
        return FALSE;
    }
#endif

    g_print("Pipeline built with display and streaming branches (%s encoding)\n",
            data->using_gpu_encoder ? "GPU" : "CPU");

    return TRUE;
}
#endif

static gboolean on_video_bus(MpWorker *worker, GstMessage *message, gpointer user_data) {
    (void)worker; (void)user_data;
    if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_STATE_CHANGED) {
        GstState old, new_state, pending;
        gst_message_parse_state_changed(message, &old, &new_state, &pending);
        if (new_state == GST_STATE_PLAYING) {
            g_print("Pipeline is playing\n");
        }
        // g_print("Pipeline new_state %d\n", new_state);
    }
    return FALSE;   // let the worker handle errors, warnings and EOS
}

// ---------------------------------------------------------------------------
// Keying channel. Only exists when PKI is configured: the sender generates a
// master key per outgoing stream and hands it over inside mutually
// authenticated TLS, then receives the key for the audio coming back.
// ---------------------------------------------------------------------------

static void on_audio_up_key(const gchar *cipher, const gchar *auth,
                            const gchar *base64_key, gpointer user_data) {
    ServerData *data = (ServerData *)user_data;
    if (g_strcmp0(cipher, MP_SRTP_CIPHER) != 0 || g_strcmp0(auth, MP_SRTP_AUTH) != 0) {
        g_printerr("Peer offered SRTP suite %s/%s; this build only accepts %s/%s\n",
                   cipher, auth, MP_SRTP_CIPHER, MP_SRTP_AUTH);
        return;
    }
    mp_srtp_key_set_base64(&data->srtp_audio_up, base64_key);
}

static gboolean start_keying(ServerData *data, gboolean audio_recv) {
    GError *error = NULL;
    if (!mp_srtp_key_generate(&data->srtp_video, &error) ||
        !mp_srtp_key_generate(&data->srtp_audio_down, &error)) {
        g_printerr("%s\n", error->message);
        g_clear_error(&error);
        return FALSE;
    }

    data->keying = mp_signal_new(data->pki);
    if (!mp_signal_connect(data->keying, data->host, data->key_port, 40)) {
        g_printerr("The receiver's keying channel at %s:%d could not be reached; "
                   "refusing to stream unencrypted\n", data->host, data->key_port);
        return FALSE;
    }

    // Registered against the worker that consumes it, so the key is installed on
    // the thread that owns that stream.
    if (audio_recv) {
        mp_signal_register(data->keying, "audio-up", data->audio_rx_worker->context,
                           NULL, NULL, on_audio_up_key, data);
    }

    gchar *video_key = mp_srtp_key_to_base64(&data->srtp_video);
    gchar *audio_key = mp_srtp_key_to_base64(&data->srtp_audio_down);
    gboolean ok = video_key && audio_key &&
        mp_signal_send_key(data->keying, "video", MP_SRTP_CIPHER, MP_SRTP_AUTH,
                           video_key) &&
        mp_signal_send_key(data->keying, "audio-down", MP_SRTP_CIPHER, MP_SRTP_AUTH,
                           audio_key);
    if (video_key) {
        memset(video_key, 0, strlen(video_key));
        g_free(video_key);
    }
    if (audio_key) {
        memset(audio_key, 0, strlen(audio_key));
        g_free(audio_key);
    }
    if (!ok) {
        g_printerr("Could not deliver the SRTP keys to the receiver\n");
        return FALSE;
    }
    g_print("SRTP keys delivered over the authenticated channel\n");
    return TRUE;
}

// ---------------------------------------------------------------------------

static void on_worker_failure(MpWorker *worker, gpointer user_data) {
    ServerData *data = (ServerData *)user_data;
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
    ServerData *data = (ServerData *)user_data;
    g_print("\nInterrupted; stopping\n");
    g_main_loop_quit(data->supervisor);
    return G_SOURCE_REMOVE;
}

static void run_server(ServerData *data) {
#ifdef FILE_TEST
    if (access(data->input_file, F_OK) == -1) {
        g_printerr("Error: Input file '%s' not found\n", data->input_file);
        return;
    }
#endif

    g_mutex_init(&data->stats_lock);
    data->probe_pts = GST_CLOCK_TIME_NONE;

    gboolean encrypt = (data->pki && data->pki->enabled);
    gboolean audio_send = (data->audio.codec != MP_AUDIO_OFF && data->audio.send &&
                           g_strcmp0(data->audio.source, "none") != 0);
    gboolean audio_recv = (data->audio.codec != MP_AUDIO_OFF && data->audio.receive);
#if 1   // feng
    audio_send = audio_recv = false;
#endif

    mp_srtp_key_init(&data->srtp_video, "video", encrypt);
    mp_srtp_key_init(&data->srtp_audio_down, "audio-down", encrypt);
    mp_srtp_key_init(&data->srtp_audio_up, "audio-up", encrypt);

#ifdef FILE_TEST
    // Map the frames before building the pipeline: the number of frames actually
    // available determines what the pacing loop can loop over.
    if (!map_input_frames(data)) {
        return;
    }
#endif

    data->frame_size = (gsize)data->width * data->height * 3 / 2;  // I420
    // printf("%s %d: Resolution: %dx%d\n",__func__,__LINE__, data->width, data->height);  // feng

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

    // Keys have to exist before the pipelines are built: srtpenc takes its master
    // key as a property at construction.
    if (encrypt && !start_keying(data, audio_recv)) {
        unmap_input_frames(data);
        return;
    }

    // if (!build_pipeline(data)) {
    if (!build_pipeline_orin(data)) {
        unmap_input_frames(data);
        return;
    }

#if 0   // feng
    // only h264 works
    // Time each frame from the pacing loop to the moment its last RTP packet is
    // handed to the socket, so the per-frame budget can be checked.
    GstElement *udpsink = gst_bin_get_by_name(GST_BIN(data->pipeline), "udpsink");
    if (udpsink) {
        GstPad *sink_pad = gst_element_get_static_pad(udpsink, "sink");
        if (sink_pad) {
            gst_pad_add_probe(sink_pad, GST_PAD_PROBE_TYPE_BUFFER, budget_probe, data, NULL);
            gst_object_unref(sink_pad);
        }
        gst_object_unref(udpsink);
    }
#else
    // h264 + h265
    gulong probe_id = 0;
    GstElement *probe = gst_bin_get_by_name(GST_BIN(data->pipeline), "encoder");
    if (probe) {
        GstPad *src_pad = gst_element_get_static_pad(probe, "src");
        if (src_pad) {
            probe_id = gst_pad_add_probe(src_pad, GST_PAD_PROBE_TYPE_BUFFER, budget_probe, data, NULL);
            gst_object_unref(src_pad);
        }
        gst_object_unref(probe);
    }
#endif

    mp_worker_set_pipeline(data->video_worker, data->pipeline, on_video_bus, data);
    data->last_report_us = g_get_monotonic_time();
    mp_worker_add_timeout(data->video_worker, 1000, report_budget, data);

    if (audio_send &&
        !mp_audio_udp_send_pipeline(data->audio_tx_worker, &data->audio, data->host,
                                    data->audio_port, &data->srtp_audio_down,
                                    &data->audio_tx_stats)) {
        g_printerr("Outgoing audio disabled: its pipeline could not be built\n");
        audio_send = FALSE;
    }
    if (audio_recv &&
        !mp_audio_udp_recv_pipeline(data->audio_rx_worker, &data->audio, "0.0.0.0",
                                    data->audio_back_port, &data->srtp_audio_up,
                                    &data->audio_rx_stats)) {
        g_printerr("Incoming audio disabled: its pipeline could not be built\n");
        audio_recv = FALSE;
    }

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
    if (data->keying) {
        // Only now: the key for the returning audio is installed by the worker
        // that owns it, so its loop has to be running to receive it.
        mp_signal_start(data->keying);
    }

    g_print("Streaming to %s:%d%s\n", data->host, data->port,
            encrypt ? " (SRTP)" : "");
    g_print("Codec: %s, Resolution: %dx%d, FPS: %d\n",
            data->codec, data->width, data->height, data->framerate);
    g_print("Preview renders each frame as it is handed to the encoder\n");
    if (audio_send || audio_recv) {
        g_print("Audio: %s%s%s, %s, independent of video\n",
                audio_send ? "out" : "", (audio_send && audio_recv) ? " + " : "",
                audio_recv ? "back" : "", mp_audio_codec_name(data->audio.codec));
    }
    g_print("Press Ctrl+C to stop\n");

    // Start pushing frames only once the pipeline is live, otherwise the first
    // buffers would be pushed into a pipeline that cannot accept them yet.
    g_atomic_int_set(&data->running, 1);
    data->pacer_thread = g_thread_new("pacer", pacer_thread_func, data);

    g_unix_signal_add(SIGINT, on_interrupt, data);
    g_main_loop_run(data->supervisor);
    // while (1) { sleep(1); }  // feng

    // Cleanup: stop the pacing loop before tearing the pipeline down so it cannot
    // push into elements that are already gone.
    g_atomic_int_set(&data->running, 0);
    if (data->pacer_thread) {
        g_thread_join(data->pacer_thread);
        data->pacer_thread = NULL;
    }

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
    unmap_input_frames(data);
    g_mutex_clear(&data->stats_lock);
}

static void print_usage(const gchar *program, FILE *stream) {
    g_fprintf(stream, "Usage: %s <input_file> [options]\n", program);
    g_fprintf(stream, "\nStreams a raw I420 YUV file as H.264/H.265 over RTP/UDP, "
                      "with bidirectional audio.\n\n");
    g_fprintf(stream, "Video:\n");
    g_fprintf(stream, "  --codec <h264|h265>    Video codec (default: h264)\n");
    g_fprintf(stream, "  --host <address>       Target host IP (default: 127.0.0.1)\n");
    g_fprintf(stream, "  --port <port>          Target UDP port (default: 5000)\n");
    g_fprintf(stream, "  --width <pixels>       Video width (default: 1920)\n");
    g_fprintf(stream, "  --height <pixels>      Video height (default: 1080)\n");
    g_fprintf(stream, "  --framerate <fps>      Video framerate (default: 60)\n");
    g_fprintf(stream, "  --bitrate <bps>        Encoding bitrate (default: 4000000)\n");
    g_fprintf(stream, "  --loop-seconds <sec>   Seconds of video to map and loop "
                      "(default: 10, 0 = whole file)\n");
    g_fprintf(stream, "                         Bounds memory: 600 frames of 1080p is ~1.8 GiB\n");
    g_fprintf(stream, "  --no-gpu               Disable GPU acceleration\n");
    g_fprintf(stream, "  --gpu-platform <auto|nvidia|intel|amd>  GPU platform (default: auto)\n");
    g_fprintf(stream, "\nPorts:\n");
    g_fprintf(stream, "  --audio-port <port>       Outgoing audio (default: 5002)\n");
    g_fprintf(stream, "  --audio-back-port <port>  Returning audio (default: 5004)\n");
    g_fprintf(stream, "  --key-port <port>         Receiver's TLS keying port "
                      "(default: 5010, PKI only)\n");
    mp_audio_print_usage(stream);
    mp_pki_print_usage(stream);
    g_fprintf(stream, "\nWith PKI configured every stream is encrypted with SRTP "
                      "(%s, %s) and the\nmaster keys are exchanged over the "
                      "authenticated channel.\n", MP_SRTP_CIPHER, MP_SRTP_AUTH);
}

int main(int argc, char *argv[]) {
    ServerData data;
    memset(&data, 0, sizeof(data));

    // Initialize defaults
    data.codec = g_strdup("h265");
    // data.host = g_strdup("127.0.0.1");
    data.host = g_strdup("10.246.20.129");
    data.port = 9601;
    data.width = 1280;
    data.height = 1024;
    data.framerate = 60;
    data.bitrate = 4000000;
    data.use_gpu = TRUE;
    data.gpu_platform = g_strdup("auto");
    // Bound the mapped working set so it stays resident in the page cache. 10 s of
    // 1080p I420 is ~890 MiB; mapping a whole multi-gigabyte file would thrash.
    data.loop_seconds = 10;
    data.input_fd = -1;
    data.audio_port = 5002;
    data.audio_back_port = 5004;
    data.key_port = 5010;
    data.pki = mp_pki_new();
    // 440 Hz here, 660 Hz at the receiver, so which direction you are hearing is
    // obvious without a spectrum analyser.
    mp_audio_config_defaults(&data.audio, 440);

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (mp_audio_parse_arg(&data.audio, argc, argv, &i) ||
            mp_pki_parse_arg(data.pki, argc, argv, &i)) {
            continue;
        }
        if (g_strcmp0(argv[i], "--codec") == 0 && i + 1 < argc) {
            g_free(data.codec);
            data.codec = g_strdup(argv[++i]);
        } else if (g_strcmp0(argv[i], "--host") == 0 && i + 1 < argc) {
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
        } else if (g_strcmp0(argv[i], "--audio-port") == 0 && i + 1 < argc) {
            data.audio_port = atoi(argv[++i]);
        } else if (g_strcmp0(argv[i], "--audio-back-port") == 0 && i + 1 < argc) {
            data.audio_back_port = atoi(argv[++i]);
        } else if (g_strcmp0(argv[i], "--key-port") == 0 && i + 1 < argc) {
            data.key_port = atoi(argv[++i]);
        } else if (g_strcmp0(argv[i], "--no-gpu") == 0) {
            data.use_gpu = FALSE;
        } else if (g_strcmp0(argv[i], "--gpu-platform") == 0 && i + 1 < argc) {
            g_free(data.gpu_platform);
            data.gpu_platform = g_strdup(argv[++i]);
        } else if (g_strcmp0(argv[i], "--help") == 0 || g_strcmp0(argv[i], "-h") == 0) {
            print_usage(argv[0], stdout);
            return 0;
        } else if (argv[i][0] != '-' && data.input_file == NULL) {
            data.input_file = g_strdup(argv[i]);
        }
    }

#ifdef FILE_TEST
    if (data.input_file == NULL) {
        print_usage(argv[0], stderr);
        return 1;
    }
#endif

    if (data.audio.invalid) {
        return 2;
    }

    GError *error = NULL;
    if (!mp_pki_load(data.pki, &error)) {
        g_printerr("PKI configuration rejected: %s\n", error->message);
        g_clear_error(&error);
        return 1;
    }

    if (!data.frameQueue) {
        data.frameQueue = std::make_shared<SyncQueue<unsigned char*>>();
    }

    // Initialize GStreamer
    gst_init(&argc, &argv);

#ifndef FILE_TEST
    int ret = runVideoPipeline((void *)(&data));
    if (ret) {
        g_printerr("runVideoPipeline error\n");
        return ret;
    }
    usleep(100000);
#endif

    data.supervisor = g_main_loop_new(NULL, FALSE);

    // Run server
    run_server(&data);

    // Cleanup
    g_main_loop_unref(data.supervisor);
    mp_audio_config_clear(&data.audio);
    mp_pki_free(data.pki);
    g_free(data.input_file);
    g_free(data.codec);
    g_free(data.host);
    g_free(data.gpu_platform);

    return 0;
}

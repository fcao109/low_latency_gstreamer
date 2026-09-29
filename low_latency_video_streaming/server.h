#pragma once

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

#include "audio_stream.h"
#include "media_pki.h"
#include "media_worker.h"
#include "signal_channel.h"
#include "syncQueue.h"
#include "videoPipelineApi.h"

#define FILE_TEST
// #define SERVER_LOCAL_RENDERING
// #define SERVER_LOCAL_RENDERING_CV

#define COMBO_IMAGES

// Ring of recent push timestamps, indexed by frame number. Only needs to span
// the few frames that can be in flight between appsrc and udpsink.
#define BUDGET_RING 256

typedef struct {
    gchar *input_file;
    gchar *codec;
    gchar *host;
    gint port;
    gint width;
    gint height;
    gint framerate;
    gint bitrate;
    gboolean use_gpu;
    gchar *gpu_platform;
    GstElement *pipeline;
    gboolean using_gpu_encoder;

    // Audio, both directions, and the encryption that now covers all of it.
    MpAudioConfig audio;
    gint audio_port;         // outgoing audio
    gint audio_back_port;    // returning audio
    gint key_port;           // TLS keying channel
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

    // How many seconds of video to keep as the looping working set. Mapping the
    // whole file would exceed RAM on long inputs and the loop would then fault
    // pages back from disk on every pass, so the set is deliberately bounded.
    gint loop_seconds;

    // Input frames are memory-mapped rather than copied, so pushing a frame costs
    // no memcpy and the streaming loop never touches the filesystem.
    int input_fd;
    guint8 *frames_base;
    gsize mapped_size;
    gsize frame_size;
    gint total_frames;

    GstElement *appsrc;
    GThread *cap_thread;
    GThread *pacer_thread;
    gint running = 0;

    // Per-frame budget accounting: when each frame was pushed, and how long it
    // took to reach the wire, so overruns past the frame period are visible.
    GMutex stats_lock;
    gint64 push_time_us[BUDGET_RING];
    GstClockTime probe_pts;
    GstClockTime probe_base_pts;
    gint64 probe_last_us;
    gint budget_frames;
    gint64 budget_sum_us;
    gint64 budget_max_us;
    gint budget_overruns;
    gint64 last_report_us;

    guint64 capture_frame_cnt;
    std::shared_ptr<SyncQueue<unsigned char*>> frameQueue = nullptr;
} ServerData;
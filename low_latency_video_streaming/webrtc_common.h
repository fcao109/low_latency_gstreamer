// WebRTC low-latency tuning.
//
// webrtcbin in 1.16 exposes no "latency" property, so the only way to control
// buffering is through the rtpbin it holds internally. That rtpbin creates one
// rtpjitterbuffer per stream, and those default to latency=200 ms in mode
// "slave" - the same adaptive mode that was measured holding frames ~22 ms on
// the UDP path even at a 5 ms depth. Both have to be overridden or WebRTC
// cannot get near one frame of delay.
//
// The property helpers this used to carry now live in media_props.h, and the
// signalling channel in signal_channel.h, because the RTP/UDP transport needs
// both of them too.

#ifndef WEBRTC_COMMON_H
#define WEBRTC_COMMON_H

#include <gst/gst.h>
#include <gst/sdp/sdp.h>

// The webrtc library headers are still marked unstable in 1.16 and refuse to
// compile without this.
#define GST_USE_UNSTABLE_API
#include <gst/webrtc/webrtc.h>

#include <glib.h>
#include <string.h>

#include "media_props.h"

typedef void (*WcJitterbufferHook)(GstElement *jitterbuffer, gpointer user_data);

typedef struct {
    gint latency_ms;
    const gchar *mode;          // rtpjitterbuffer "mode" nickname
    const gchar *tag;           // which stream this belongs to, for logging
    WcJitterbufferHook hook;    // optional: lets the caller add probes
    gpointer hook_data;
} WcTuning;

static void wc_on_new_jitterbuffer(GstElement *rtpbin, GstElement *jitterbuffer,
                                   guint session, guint ssrc, gpointer user_data) {
    (void)rtpbin; (void)session; (void)ssrc;
    WcTuning *tuning = (WcTuning *)user_data;

    mp_set_number_prop(jitterbuffer, "latency", tuning->latency_ms);
    if (mp_find_prop(jitterbuffer, "mode")) {
        gst_util_set_object_arg(G_OBJECT(jitterbuffer), "mode", tuning->mode);
    }
    // Tell the decoder about gaps rather than letting it decode from damaged
    // references.
    mp_set_bool_prop(jitterbuffer, "do-lost", TRUE);

    g_print("[%s] jitterbuffer created: latency=%d ms, mode=%s\n",
            tuning->tag ? tuning->tag : "webrtc", tuning->latency_ms, tuning->mode);

    if (tuning->hook) {
        tuning->hook(jitterbuffer, tuning->hook_data);
    }
}

// Must be called before negotiation: the jitterbuffers are created when the
// streams are set up, and "new-jitterbuffer" is the only chance to configure
// each one.
static gboolean wc_apply_low_latency(GstElement *webrtcbin, WcTuning *tuning) {
    GstElement *rtpbin = gst_bin_get_by_name(GST_BIN(webrtcbin), "rtpbin");
    if (!rtpbin) {
        g_printerr("webrtcbin has no internal rtpbin; cannot lower latency\n");
        return FALSE;
    }
    // Default for any jitterbuffer rtpbin makes on its own.
    mp_set_number_prop(rtpbin, "latency", tuning->latency_ms);
    g_signal_connect(rtpbin, "new-jitterbuffer",
                     G_CALLBACK(wc_on_new_jitterbuffer), tuning);
    gst_object_unref(rtpbin);
    return TRUE;
}

#endif  // WEBRTC_COMMON_H

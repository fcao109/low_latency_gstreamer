// One WebRTC peer connection, carrying one stream in one direction.
//
// Three of these run per process - video out, audio out, audio back - each with
// its own webrtcbin in its own pipeline on its own thread. That is more ICE and
// DTLS handshakes than bundling everything onto a single connection would need,
// and it is worth it:
//
//   * The streams share no clock, no latency budget and no state machine, which
//     is the whole point of not synchronising audio to video.
//   * Each connection is unidirectional, so every session is a plain
//     offer/answer with a single m-line. webrtcbin 1.16's rough edge is the
//     answerer side of a sendrecv negotiation, and this sidesteps it entirely.
//   * A microphone that fails to open, or an audio stream that is rejected by
//     the certificate check, cannot disturb video.
//
// All three sessions share one signalling connection, tagged by session name.
//
// The offer/answer machinery is here rather than in the two binaries because
// both ends now play both roles: the sender answers for the audio coming back,
// and the receiver offers the audio it sends.

#ifndef WEBRTC_SESSION_H
#define WEBRTC_SESSION_H

#include "media_pki.h"
#include "media_worker.h"
#include "signal_channel.h"
#include "webrtc_common.h"

typedef struct _MpWebrtcSession MpWebrtcSession;

struct _MpWebrtcSession {
    // --- set by the caller before mp_webrtc_session_create ---
    const gchar *tag;            // signalling session name, also the log prefix
    gboolean offerer;            // TRUE for the side with media to send
    MpWorker *worker;
    MpSignal *sig;
    MpPki *pki;                  // may be NULL or disabled
    const gchar *stun_server;    // NULL on a LAN
    gint jb_latency_ms;
    const gchar *jb_mode;
    WcJitterbufferHook jb_hook;  // optional, for receive-side probes
    gpointer jb_hook_data;
    void (*on_pad_added)(GstElement *webrtc, GstPad *pad, gpointer user_data);
    gpointer user_data;

    // --- owned ---
    GstElement *webrtcbin;
    WcTuning tuning;
    MpDtlsGuard *guard;
};

// How long the DTLS peer has to present a certificate we can verify. Covers ICE
// gathering, connectivity checks and the handshake itself.
#define MP_WEBRTC_VERIFY_SECONDS 15

static void mp_webrtc_on_offer_created(GstPromise *promise, gpointer user_data);
static void mp_webrtc_on_answer_created(GstPromise *promise, gpointer user_data);

// Best effort: make the SDP say what this session actually does. Nothing here
// depends on it - a sendrecv m-line with nothing attached to send simply never
// sends - but an honest offer is easier to read in a capture.
static void mp_webrtc_set_direction(MpWebrtcSession *s, gboolean sending) {
    GArray *transceivers = NULL;
    g_signal_emit_by_name(s->webrtcbin, "get-transceivers", &transceivers);
    if (!transceivers) {
        return;
    }
    for (guint i = 0; i < transceivers->len; i++) {
        GObject *transceiver = g_array_index(transceivers, GObject *, i);
        if (!transceiver ||
            !g_object_class_find_property(G_OBJECT_GET_CLASS(transceiver), "direction")) {
            continue;
        }
        g_object_set(transceiver, "direction",
                     sending ? GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY
                             : GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_RECVONLY,
                     NULL);
    }
    g_array_unref(transceivers);
}

static void mp_webrtc_on_offer_created(GstPromise *promise, gpointer user_data) {
    MpWebrtcSession *s = (MpWebrtcSession *)user_data;

    const GstStructure *reply = gst_promise_get_reply(promise);
    GstWebRTCSessionDescription *offer = NULL;
    gst_structure_get(reply, "offer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &offer, NULL);
    gst_promise_unref(promise);
    if (!offer) {
        g_printerr("[%s] create-offer produced no description\n", s->tag);
        return;
    }

    g_signal_emit_by_name(s->webrtcbin, "set-local-description", offer, NULL);

    gchar *text = gst_sdp_message_as_text(offer->sdp);
    if (s->pki && s->pki->enabled) {
        gchar *corrected = mp_pki_rewrite_sdp_fingerprint(s->pki, text, s->tag);
        g_free(text);
        text = corrected;
    }
    g_print("[%s] sending offer (%zu bytes)\n", s->tag, strlen(text));
    mp_signal_send_sdp(s->sig, s->tag, "offer", text);
    g_free(text);
    gst_webrtc_session_description_free(offer);
}

static void mp_webrtc_on_negotiation_needed(GstElement *webrtc, gpointer user_data) {
    (void)webrtc;
    MpWebrtcSession *s = (MpWebrtcSession *)user_data;
    mp_webrtc_set_direction(s, TRUE);
    GstPromise *promise = gst_promise_new_with_change_func(mp_webrtc_on_offer_created,
                                                          s, NULL);
    g_signal_emit_by_name(s->webrtcbin, "create-offer", NULL, promise);
}

static void mp_webrtc_on_answer_created(GstPromise *promise, gpointer user_data) {
    MpWebrtcSession *s = (MpWebrtcSession *)user_data;

    const GstStructure *reply = gst_promise_get_reply(promise);
    GstWebRTCSessionDescription *answer = NULL;
    gst_structure_get(reply, "answer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &answer, NULL);
    gst_promise_unref(promise);
    if (!answer) {
        g_printerr("[%s] create-answer produced no description\n", s->tag);
        return;
    }

    g_signal_emit_by_name(s->webrtcbin, "set-local-description", answer, NULL);

    gchar *text = gst_sdp_message_as_text(answer->sdp);
    if (s->pki && s->pki->enabled) {
        gchar *corrected = mp_pki_rewrite_sdp_fingerprint(s->pki, text, s->tag);
        g_free(text);
        text = corrected;
    }
    g_print("[%s] sending answer (%zu bytes)\n", s->tag, strlen(text));
    mp_signal_send_sdp(s->sig, s->tag, "answer", text);
    g_free(text);
    gst_webrtc_session_description_free(answer);
}

// Runs on the owning worker's thread, posted there by the signalling reader.
static void mp_webrtc_on_remote_sdp(const gchar *type, const gchar *sdp,
                                    gpointer user_data) {
    MpWebrtcSession *s = (MpWebrtcSession *)user_data;
    const gchar *expected = s->offerer ? "answer" : "offer";
    if (g_strcmp0(type, expected) != 0) {
        g_printerr("[%s] ignoring unexpected '%s' (wanted '%s')\n", s->tag, type,
                   expected);
        return;
    }

    // Capture the promised certificate hash before anything is applied: this is
    // what the DTLS handshake will be held to.
    if (s->guard) {
        mp_dtls_guard_note_remote_sdp(s->guard, sdp);
    }

    GstSDPMessage *message = NULL;
    if (gst_sdp_message_new(&message) != GST_SDP_OK ||
        gst_sdp_message_parse_buffer((const guint8 *)sdp, strlen(sdp), message)
            != GST_SDP_OK) {
        g_printerr("[%s] could not parse the remote %s\n", s->tag, type);
        if (message) gst_sdp_message_free(message);
        return;
    }

    GstWebRTCSessionDescription *description = gst_webrtc_session_description_new(
        s->offerer ? GST_WEBRTC_SDP_TYPE_ANSWER : GST_WEBRTC_SDP_TYPE_OFFER, message);
    g_signal_emit_by_name(s->webrtcbin, "set-remote-description", description, NULL);
    gst_webrtc_session_description_free(description);

    if (s->offerer) {
        g_print("[%s] answer applied; media should start flowing\n", s->tag);
    } else {
        mp_webrtc_set_direction(s, FALSE);
        GstPromise *promise =
            gst_promise_new_with_change_func(mp_webrtc_on_answer_created, s, NULL);
        g_signal_emit_by_name(s->webrtcbin, "create-answer", NULL, promise);
    }
}

static void mp_webrtc_on_remote_ice(guint mline, const gchar *candidate,
                                    gpointer user_data) {
    MpWebrtcSession *s = (MpWebrtcSession *)user_data;
    g_signal_emit_by_name(s->webrtcbin, "add-ice-candidate", mline, candidate);
}

static void mp_webrtc_on_local_ice(GstElement *webrtc, guint mline, gchar *candidate,
                                   gpointer user_data) {
    (void)webrtc;
    MpWebrtcSession *s = (MpWebrtcSession *)user_data;
    mp_signal_send_ice(s->sig, s->tag, mline, candidate);
}

// A peer that cannot be authenticated is not a peer. Stopping this session's
// loop takes the stream down and leaves the others running.
static void mp_webrtc_on_dtls_reject(MpDtlsGuard *guard, const gchar *reason,
                                     gpointer user_data) {
    (void)guard;
    MpWebrtcSession *s = (MpWebrtcSession *)user_data;
    g_printerr("[%s] stopping the stream: %s\n", s->tag, reason);
    mp_worker_fail(s->worker, "DTLS peer verification failed");
    g_main_loop_quit(s->worker->loop);
}

static gboolean mp_webrtc_session_create(MpWebrtcSession *s, const gchar *element_name) {
    s->webrtcbin = gst_element_factory_make("webrtcbin", element_name);
    if (!s->webrtcbin) {
        g_printerr("[%s] failed to create webrtcbin\n", s->tag);
        return FALSE;
    }

    // Bundling means one ICE/DTLS handshake per session; there is only one
    // stream in each, but it keeps the SDP minimal.
    const gchar *const bundle[] = {"max-bundle", NULL};
    mp_set_enum_prop(s->webrtcbin, "bundle-policy", bundle);
    // A public STUN server is pointless on loopback or a closed LAN and only
    // delays gathering, so it stays unset unless asked for.
    if (s->stun_server && *s->stun_server) {
        g_object_set(s->webrtcbin, "stun-server", s->stun_server, NULL);
    }

    // Must happen before negotiation: jitterbuffers are created during stream
    // setup and "new-jitterbuffer" is the only chance to configure each one.
    s->tuning.latency_ms = s->jb_latency_ms;
    s->tuning.mode = s->jb_mode;
    s->tuning.hook = s->jb_hook;
    s->tuning.hook_data = s->jb_hook_data;
    s->tuning.tag = s->tag;
    wc_apply_low_latency(s->webrtcbin, &s->tuning);

    if (s->pki && s->pki->enabled) {
        s->guard = mp_dtls_guard_new(s->pki, s->tag);
        s->guard->on_reject = mp_webrtc_on_dtls_reject;
        s->guard->user_data = s;
        // Installs our certificate as webrtcbin builds its DTLS elements, which
        // is before it reads one to generate the SDP fingerprint.
        mp_dtls_guard_attach(s->guard, s->webrtcbin, MP_WEBRTC_VERIFY_SECONDS);
        mp_worker_add_timeout(s->worker, 200, mp_dtls_guard_poll, s->guard);
    }

    if (s->offerer) {
        g_signal_connect(s->webrtcbin, "on-negotiation-needed",
                         G_CALLBACK(mp_webrtc_on_negotiation_needed), s);
    }
    g_signal_connect(s->webrtcbin, "on-ice-candidate",
                     G_CALLBACK(mp_webrtc_on_local_ice), s);
    if (s->on_pad_added) {
        g_signal_connect(s->webrtcbin, "pad-added",
                         G_CALLBACK(s->on_pad_added), s->user_data);
    }

    mp_signal_register(s->sig, s->tag, s->worker->context, mp_webrtc_on_remote_sdp,
                       mp_webrtc_on_remote_ice, NULL, s);
    return TRUE;
}

// ---------------------------------------------------------------------------
// Send gate: nothing goes out until the connection can actually carry it.
//
// A live sender whose transport is not ready yet has two options, and both are
// wrong. Blocking back-pressures the capture thread; queueing means a burst
// escapes the moment the transport opens, and it is the first packet of that
// burst that fixes the receiving jitterbuffer's RTP-timestamp-to-clock mapping.
// Every packet after it is then held for the length of the burst - measured here
// as 241 ms of audio delay that never drained for the rest of the session.
//
// So stale packets are dropped instead, and the first packet the far end ever
// sees is a fresh one. Video does not need this: its pacer only starts once the
// session is up, and a video stream that begins with a burst recovers on the
// next keyframe.
// ---------------------------------------------------------------------------

typedef struct {
    GstElement *webrtcbin;
    const gchar *tag;
    gint open;
    gint64 deadline_us;
    gint dropped;
    GMutex lock;
    GPtrArray *dtls;             // dtlssrtpdec elements, borrowed from webrtcbin
} MpWebrtcGate;

// ICE being connected is not enough: DTLS still has to finish before srtpenc has
// keys, and packets handed over in between queue up inside webrtcbin instead of
// going out. A non-empty "peer-pem" is the plainest evidence available that the
// handshake is done - it is the certificate the peer presented during it.
static gboolean mp_webrtc_gate_dtls_ready(MpWebrtcGate *gate) {
    g_mutex_lock(&gate->lock);
    guint n = gate->dtls->len;
    GstElement *decoder = n ? (GstElement *)g_ptr_array_index(gate->dtls, 0) : NULL;
    g_mutex_unlock(&gate->lock);

    if (!decoder) {
        // Nothing to check against; fall back to the ICE state alone.
        return TRUE;
    }
    if (!mp_find_prop(decoder, "peer-pem")) {
        return TRUE;
    }
    gchar *pem = NULL;
    g_object_get(decoder, "peer-pem", &pem, NULL);
    gboolean ready = (pem && *pem);
    g_free(pem);
    return ready;
}

static void mp_webrtc_gate_on_deep_element_added(GstBin *bin, GstBin *sub_bin,
                                                 GstElement *element,
                                                 gpointer user_data) {
    (void)bin; (void)sub_bin;
    MpWebrtcGate *gate = (MpWebrtcGate *)user_data;
    GstElementFactory *factory = gst_element_get_factory(element);
    if (g_strcmp0(factory ? GST_OBJECT_NAME(factory) : NULL, "dtlssrtpdec") != 0) {
        return;
    }
    g_mutex_lock(&gate->lock);
    g_ptr_array_add(gate->dtls, element);
    g_mutex_unlock(&gate->lock);
}

static GstPadProbeReturn mp_webrtc_gate_probe(GstPad *pad, GstPadProbeInfo *info,
                                              gpointer user_data) {
    (void)pad;
    MpWebrtcGate *gate = (MpWebrtcGate *)user_data;
    if (g_atomic_int_get(&gate->open)) {
        return GST_PAD_PROBE_OK;
    }
    if (!(GST_PAD_PROBE_INFO_TYPE(info) & GST_PAD_PROBE_TYPE_BUFFER)) {
        return GST_PAD_PROBE_OK;
    }

    // webrtcbin 1.16 never advances "connection-state" past "new" - it is
    // declared but not driven - so ICE is what has to be watched. Measured: the
    // aggregate connection-state stayed "new" for an entire session while media
    // flowed perfectly. "ice-connection-state" does track.
    GstWebRTCICEConnectionState ice = GST_WEBRTC_ICE_CONNECTION_STATE_NEW;
    GstWebRTCPeerConnectionState overall = GST_WEBRTC_PEER_CONNECTION_STATE_NEW;
    g_object_get(gate->webrtcbin, "ice-connection-state", &ice,
                 "connection-state", &overall, NULL);
    gboolean ice_up = (ice == GST_WEBRTC_ICE_CONNECTION_STATE_CONNECTED ||
                       ice == GST_WEBRTC_ICE_CONNECTION_STATE_COMPLETED ||
                       overall == GST_WEBRTC_PEER_CONNECTION_STATE_CONNECTED);
    if (ice_up && mp_webrtc_gate_dtls_ready(gate)) {
        if (g_atomic_int_compare_and_exchange(&gate->open, 0, 1)) {
            g_print("[%s] transport up; sending (%d stale packets dropped while "
                    "waiting)\n", gate->tag, gate->dropped);
        }
        return GST_PAD_PROBE_OK;
    }

    // Never lose the stream outright because a state was not reported.
    if (g_get_monotonic_time() > gate->deadline_us) {
        if (g_atomic_int_compare_and_exchange(&gate->open, 0, 1)) {
            g_print("[%s] transport never reported connected; sending anyway "
                    "after %d dropped packets\n", gate->tag, gate->dropped);
        }
        return GST_PAD_PROBE_OK;
    }

    gate->dropped++;
    return GST_PAD_PROBE_DROP;
}

// Must be called before the pipeline is started: the DTLS elements it watches for
// are created during negotiation.
static void mp_webrtc_gate_attach(MpWebrtcGate *gate, MpWebrtcSession *s,
                                  GstElement *tail, gint deadline_seconds) {
    gate->webrtcbin = s->webrtcbin;
    gate->tag = s->tag;
    gate->open = 0;
    gate->dropped = 0;
    gate->dtls = g_ptr_array_new();
    g_mutex_init(&gate->lock);
    gate->deadline_us = g_get_monotonic_time() +
                        (gint64)deadline_seconds * G_USEC_PER_SEC;
    g_signal_connect(s->webrtcbin, "deep-element-added",
                     G_CALLBACK(mp_webrtc_gate_on_deep_element_added), gate);

    GstPad *pad = gst_element_get_static_pad(tail, "src");
    if (pad) {
        gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER, mp_webrtc_gate_probe,
                          gate, NULL);
        gst_object_unref(pad);
    }
}

static void mp_webrtc_session_clear(MpWebrtcSession *s) {
    if (s->guard) {
        mp_dtls_guard_free(s->guard);
        s->guard = NULL;
    }
}

#endif  // WEBRTC_SESSION_H

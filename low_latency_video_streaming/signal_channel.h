// The out-of-band channel both transports use to agree on how to talk.
//
// Grew out of the WebRTC signalling channel and is now shared, because the
// RTP/UDP transport needs an authenticated channel too once its media is
// encrypted: somewhere to hand over SRTP master keys.
//
// Still a line-oriented TCP protocol rather than a websocket service, so
// json-glib and libsoup stay out of the dependency list. Three message kinds,
// each tagged with the stream it belongs to, because one connection now carries
// several independent sessions (video, audio out, audio back):
//
//     SDP <session> <type> <byte-length>\n<sdp bytes>
//     ICE <session> <mline-index> <candidate>\n
//     KEY <session> <cipher> <auth> <base64 master key>\n
//     BYE\n
//
// SDP carries embedded newlines, hence the explicit length prefix. ICE
// candidates and keys are single-line, so they need no framing.
//
// With PKI configured the whole channel runs inside mutually authenticated TLS.
// That matters more than it looks: an attacker who can rewrite SDP can rewrite
// the DTLS fingerprints, and an attacker who can read a KEY line has the SRTP
// master key. Both are game over regardless of how the media is encrypted.

#ifndef SIGNAL_CHANNEL_H
#define SIGNAL_CHANNEL_H

#include <gio/gio.h>
#include <glib.h>
#include <string.h>

#include "media_pki.h"

typedef void (*MpSdpCb)(const gchar *type, const gchar *sdp, gpointer user_data);
typedef void (*MpIceCb)(guint mline, const gchar *candidate, gpointer user_data);
typedef void (*MpKeyCb)(const gchar *cipher, const gchar *auth,
                        const gchar *base64_key, gpointer user_data);

// One logical stream on the shared connection. Messages for it are dispatched
// into `context`, so each session's callbacks run on the thread that owns that
// session's pipeline and nowhere else.
typedef struct {
    gchar *name;
    GMainContext *context;
    MpSdpCb on_sdp;
    MpIceCb on_ice;
    MpKeyCb on_key;
    gpointer user_data;
} MpSignalSession;

typedef struct {
    GSocketListener *listener;
    GSocketConnection *conn;
    GIOStream *tls;            // non-NULL when PKI is on; owns the framing then
    GDataInputStream *in;
    GOutputStream *out;
    GMutex send_lock;
    GThread *reader;
    gint running;
    GMutex sessions_lock;
    GPtrArray *sessions;       // MpSignalSession*
    MpPki *pki;
} MpSignal;

static MpSignal* mp_signal_new(MpPki *pki) {
    MpSignal *s = g_new0(MpSignal, 1);
    s->pki = pki;
    s->sessions = g_ptr_array_new();
    g_mutex_init(&s->send_lock);
    g_mutex_init(&s->sessions_lock);
    return s;
}

static void mp_signal_register(MpSignal *s, const gchar *name,
                               GMainContext *context, MpSdpCb on_sdp,
                               MpIceCb on_ice, MpKeyCb on_key,
                               gpointer user_data) {
    MpSignalSession *session = g_new0(MpSignalSession, 1);
    session->name = g_strdup(name);
    session->context = context;
    session->on_sdp = on_sdp;
    session->on_ice = on_ice;
    session->on_key = on_key;
    session->user_data = user_data;

    g_mutex_lock(&s->sessions_lock);
    g_ptr_array_add(s->sessions, session);
    g_mutex_unlock(&s->sessions_lock);
}

static MpSignalSession* mp_signal_find(MpSignal *s, const gchar *name) {
    MpSignalSession *found = NULL;
    g_mutex_lock(&s->sessions_lock);
    for (guint i = 0; i < s->sessions->len && !found; i++) {
        MpSignalSession *session = (MpSignalSession *)g_ptr_array_index(s->sessions, i);
        if (g_strcmp0(session->name, name) == 0) {
            found = session;
        }
    }
    g_mutex_unlock(&s->sessions_lock);
    return found;
}

// Take over the streams of an established connection, wrapping them in TLS
// first when PKI is configured.
static gboolean mp_signal_attach(MpSignal *s, GSocketConnection *conn,
                                 gboolean as_server) {
    s->conn = conn;

    GIOStream *stream = G_IO_STREAM(conn);
    if (s->pki && s->pki->enabled) {
        GError *error = NULL;
        // Bound the handshake. A peer that is not speaking TLS - the plaintext
        // build of this program, say - leaves the handshake waiting for a record
        // that never comes, and without this both ends sit silently forever.
        // Cleared again afterwards: the same timeout would otherwise apply to
        // reads, and this channel is deliberately idle for long stretches.
        GSocket *socket = g_socket_connection_get_socket(conn);
        if (socket) {
            g_socket_set_timeout(socket, 15);
        }
        s->tls = as_server ? mp_pki_wrap_server(s->pki, conn, &error)
                           : mp_pki_wrap_client(s->pki, conn, &error);
        if (socket) {
            g_socket_set_timeout(socket, 0);
        }
        if (!s->tls) {
            g_printerr("TLS handshake on the signalling channel failed: %s\n",
                       error ? error->message : "unknown error");
            g_clear_error(&error);
            return FALSE;
        }
        stream = s->tls;
    }

    s->in = g_data_input_stream_new(g_io_stream_get_input_stream(stream));
    // The protocol is strictly \n-terminated; do not let GLib also split on \r.
    g_data_input_stream_set_newline_type(s->in, G_DATA_STREAM_NEWLINE_TYPE_LF);
    s->out = g_io_stream_get_output_stream(stream);
    return TRUE;
}

// Receiving side: block until the sender connects.
static gboolean mp_signal_accept(MpSignal *s, const gchar *host, gint port) {
    GError *error = NULL;
    s->listener = g_socket_listener_new();

    GSocketAddress *addr = NULL;
    GInetAddress *inet = g_inet_address_new_from_string(host);
    if (inet) {
        addr = g_inet_socket_address_new(inet, port);
        g_object_unref(inet);
    } else {
        addr = g_inet_socket_address_new_from_string(host, port);
    }
    if (!addr || !g_socket_listener_add_address(s->listener, addr,
                                               G_SOCKET_TYPE_STREAM,
                                               G_SOCKET_PROTOCOL_TCP,
                                               NULL, NULL, &error)) {
        g_printerr("Cannot listen on %s:%d: %s\n", host, port,
                   error ? error->message : "unknown error");
        g_clear_error(&error);
        if (addr) g_object_unref(addr);
        return FALSE;
    }
    g_object_unref(addr);

    g_print("Waiting for the peer to connect on %s:%d%s\n", host, port,
            (s->pki && s->pki->enabled) ? " (TLS, client certificate required)" : "");
    GSocketConnection *conn = g_socket_listener_accept(s->listener, NULL, NULL, &error);
    if (!conn) {
        g_printerr("Accept failed: %s\n", error ? error->message : "unknown error");
        g_clear_error(&error);
        return FALSE;
    }
    if (!mp_signal_attach(s, conn, TRUE)) {
        return FALSE;
    }
    g_print("Peer connected\n");
    return TRUE;
}

// Sending side: retry, because the receiver may not be listening yet.
static gboolean mp_signal_connect(MpSignal *s, const gchar *host, gint port,
                                  gint attempts) {
    GSocketClient *client = g_socket_client_new();
    g_socket_client_set_timeout(client, 5);

    for (gint i = 0; i < attempts; i++) {
        GError *error = NULL;
        GSocketConnection *conn =
            g_socket_client_connect_to_host(client, host, (guint16)port, NULL, &error);
        if (conn) {
            // The connect timeout also applies to reads, and the channel is idle
            // for long stretches once negotiation is done. Leaving it set makes
            // the reader thread die after five quiet seconds, which silently
            // discards any later ICE candidate.
            GSocket *socket = g_socket_connection_get_socket(conn);
            if (socket) {
                g_socket_set_timeout(socket, 0);
            }
            g_object_unref(client);
            if (!mp_signal_attach(s, conn, FALSE)) {
                return FALSE;
            }
            g_print("Signalling connected to %s:%d%s\n", host, port,
                    (s->pki && s->pki->enabled) ? " (TLS, mutually authenticated)" : "");
            return TRUE;
        }
        if (i == 0) {
            g_print("Waiting for the receiver at %s:%d (%s)\n", host, port,
                    error ? error->message : "no route");
        }
        g_clear_error(&error);
        g_usleep(500 * 1000);
    }
    g_object_unref(client);
    g_printerr("Could not reach the receiver's signalling port at %s:%d\n", host, port);
    return FALSE;
}

// Received messages are handed to the owning session's main context rather than
// acted on in the reader thread, so that every call into a given webrtcbin
// happens on one thread and the ordering of offer/answer against ICE stays
// predictable.

typedef struct {
    MpSignalSession *session;
    gchar *a;
    gchar *b;
    gchar *c;
    guint n;
} MpSignalMsg;

static void mp_signal_msg_free(MpSignalMsg *m) {
    g_free(m->a);
    g_free(m->b);
    g_free(m->c);
    g_free(m);
}

static gboolean mp_signal_dispatch_sdp(gpointer data) {
    MpSignalMsg *m = (MpSignalMsg *)data;
    if (m->session->on_sdp) {
        m->session->on_sdp(m->a, m->b, m->session->user_data);
    }
    mp_signal_msg_free(m);
    return G_SOURCE_REMOVE;
}

static gboolean mp_signal_dispatch_ice(gpointer data) {
    MpSignalMsg *m = (MpSignalMsg *)data;
    if (m->session->on_ice) {
        m->session->on_ice(m->n, m->a, m->session->user_data);
    }
    mp_signal_msg_free(m);
    return G_SOURCE_REMOVE;
}

static gboolean mp_signal_dispatch_key(gpointer data) {
    MpSignalMsg *m = (MpSignalMsg *)data;
    if (m->session->on_key) {
        m->session->on_key(m->a, m->b, m->c, m->session->user_data);
    }
    mp_signal_msg_free(m);
    return G_SOURCE_REMOVE;
}

static void mp_signal_post(MpSignalSession *session, GSourceFunc callback,
                           MpSignalMsg *msg) {
    GSource *source = g_idle_source_new();
    g_source_set_callback(source, callback, msg, NULL);
    g_source_attach(source, session->context);
    g_source_unref(source);
}

static gpointer mp_signal_reader_thread(gpointer user_data) {
    MpSignal *s = (MpSignal *)user_data;

    while (g_atomic_int_get(&s->running)) {
        GError *error = NULL;
        gsize length = 0;
        gchar *line = g_data_input_stream_read_line_utf8(s->in, &length, NULL, &error);
        if (!line) {
            if (error) {
                g_printerr("Signalling read failed: %s\n", error->message);
                g_clear_error(&error);
            } else if (g_atomic_int_get(&s->running)) {
                g_print("Signalling peer closed the connection\n");
            }
            break;
        }

        if (g_str_has_prefix(line, "SDP ")) {
            // session, type, length
            gchar **parts = g_strsplit(line, " ", 4);
            if (g_strv_length(parts) == 4) {
                MpSignalSession *session = mp_signal_find(s, parts[1]);
                gsize want = (gsize)g_ascii_strtoull(parts[3], NULL, 10);
                gchar *body = g_new0(gchar, want + 1);
                gsize got = 0;
                if (!g_input_stream_read_all(G_INPUT_STREAM(s->in), body, want,
                                             &got, NULL, &error) || got != want) {
                    g_printerr("Short SDP read (%zu of %zu bytes)\n", got, want);
                    g_clear_error(&error);
                    g_free(body);
                } else if (!session) {
                    g_printerr("SDP for unknown session '%s' ignored\n", parts[1]);
                    g_free(body);
                } else {
                    MpSignalMsg *m = g_new0(MpSignalMsg, 1);
                    m->session = session;
                    m->a = g_strdup(parts[2]);   // type
                    m->b = body;                 // ownership moves to the dispatcher
                    mp_signal_post(session, mp_signal_dispatch_sdp, m);
                }
            }
            g_strfreev(parts);
        } else if (g_str_has_prefix(line, "ICE ")) {
            // Split only three times: the candidate itself contains spaces.
            gchar **parts = g_strsplit(line, " ", 4);
            if (g_strv_length(parts) == 4) {
                MpSignalSession *session = mp_signal_find(s, parts[1]);
                if (session) {
                    MpSignalMsg *m = g_new0(MpSignalMsg, 1);
                    m->session = session;
                    m->n = (guint)g_ascii_strtoull(parts[2], NULL, 10);
                    m->a = g_strdup(parts[3]);
                    mp_signal_post(session, mp_signal_dispatch_ice, m);
                } else {
                    g_printerr("ICE for unknown session '%s' ignored\n", parts[1]);
                }
            }
            g_strfreev(parts);
        } else if (g_str_has_prefix(line, "KEY ")) {
            gchar **parts = g_strsplit(line, " ", 5);
            if (g_strv_length(parts) == 5) {
                MpSignalSession *session = mp_signal_find(s, parts[1]);
                if (session) {
                    MpSignalMsg *m = g_new0(MpSignalMsg, 1);
                    m->session = session;
                    m->a = g_strdup(parts[2]);   // cipher
                    m->b = g_strdup(parts[3]);   // auth
                    m->c = g_strdup(parts[4]);   // base64 key
                    mp_signal_post(session, mp_signal_dispatch_key, m);
                } else {
                    g_printerr("KEY for unknown session '%s' ignored\n", parts[1]);
                }
            }
            g_strfreev(parts);
        } else if (g_str_has_prefix(line, "BYE")) {
            g_free(line);
            break;
        }
        g_free(line);
    }
    return NULL;
}

// Sessions must be registered before this: the peer can start talking the
// instant the reader is running.
static void mp_signal_start(MpSignal *s) {
    g_atomic_int_set(&s->running, 1);
    s->reader = g_thread_new("signalling", mp_signal_reader_thread, s);
}

static gboolean mp_signal_send(MpSignal *s, const gchar *data, gsize length) {
    if (!s->out) {
        return FALSE;
    }
    GError *error = NULL;
    g_mutex_lock(&s->send_lock);
    gboolean ok = g_output_stream_write_all(s->out, data, length, NULL, NULL, &error);
    if (ok) {
        ok = g_output_stream_flush(s->out, NULL, &error);
    }
    g_mutex_unlock(&s->send_lock);
    if (!ok) {
        g_printerr("Signalling write failed: %s\n",
                   error ? error->message : "unknown error");
        g_clear_error(&error);
    }
    return ok;
}

static gboolean mp_signal_send_sdp(MpSignal *s, const gchar *session,
                                   const gchar *type, const gchar *sdp) {
    gchar *message = g_strdup_printf("SDP %s %s %zu\n%s", session, type,
                                     strlen(sdp), sdp);
    gboolean ok = mp_signal_send(s, message, strlen(message));
    g_free(message);
    return ok;
}

static gboolean mp_signal_send_ice(MpSignal *s, const gchar *session, guint mline,
                                   const gchar *candidate) {
    gchar *message = g_strdup_printf("ICE %s %u %s\n", session, mline, candidate);
    gboolean ok = mp_signal_send(s, message, strlen(message));
    g_free(message);
    return ok;
}

static gboolean mp_signal_send_key(MpSignal *s, const gchar *session,
                                   const gchar *cipher, const gchar *auth,
                                   const gchar *base64_key) {
    gchar *message = g_strdup_printf("KEY %s %s %s %s\n", session, cipher, auth,
                                     base64_key);
    gboolean ok = mp_signal_send(s, message, strlen(message));
    g_free(message);
    return ok;
}

static void mp_signal_free(MpSignal *s) {
    if (!s) {
        return;
    }
    g_atomic_int_set(&s->running, 0);
    if (s->tls) {
        g_io_stream_close(s->tls, NULL, NULL);
    }
    if (s->conn) {
        g_io_stream_close(G_IO_STREAM(s->conn), NULL, NULL);
    }
    if (s->reader) {
        g_thread_join(s->reader);
        s->reader = NULL;
    }
    if (s->in) {
        g_object_unref(s->in);
    }
    if (s->tls) {
        g_object_unref(s->tls);
    }
    if (s->conn) {
        g_object_unref(s->conn);
    }
    if (s->listener) {
        g_socket_listener_close(s->listener);
        g_object_unref(s->listener);
    }
    for (guint i = 0; i < s->sessions->len; i++) {
        MpSignalSession *session = (MpSignalSession *)g_ptr_array_index(s->sessions, i);
        g_free(session->name);
        g_free(session);
    }
    g_ptr_array_free(s->sessions, TRUE);
    g_mutex_clear(&s->send_lock);
    g_mutex_clear(&s->sessions_lock);
    g_free(s);
}

#endif  // SIGNAL_CHANNEL_H

// X.509 (PKI) support for both transports.
//
// "Encrypted" is not the same as "authenticated", and the difference is the
// whole point of this file. Two specific gaps are closed here:
//
//   1. WebRTC's media encryption is only as trustworthy as the SDP exchange
//      that carries the DTLS fingerprints. Signalling in this project is a
//      plain TCP socket, so anyone able to sit in the middle could rewrite both
//      fingerprints, terminate DTLS on each side and read every frame while
//      both ends report "encrypted". So the signalling channel itself is
//      wrapped in TLS with *mutual* certificate authentication, and both peers
//      are checked against a CA.
//
//   2. webrtcbin generates a throwaway self-signed certificate per run and
//      GStreamer 1.16 never checks the peer's certificate against the
//      fingerprint the SDP promised. Here the local DTLS certificate is
//      replaced with our PKI identity before the fingerprint is generated, and
//      after the handshake the peer's certificate is verified three ways:
//      chain against the CA, fingerprint against the SDP, and (optionally)
//      subject name against an expected identity. A mismatch tears the stream
//      down rather than logging and continuing.
//
// The RTP/UDP transport has no DTLS at all, so it gets SRTP with master keys
// generated per stream and delivered over the same mutually-authenticated TLS
// channel. Same trust anchor, same identity checks, same failure behaviour.
//
// Everything here is opt-in: with no --pki-* options the behaviour of both
// transports is exactly what it was before. Once certificates are supplied the
// checks are enforced, i.e. verification failure stops the stream.

#ifndef MEDIA_PKI_H
#define MEDIA_PKI_H

#include <gio/gio.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <gst/gst.h>
#include <stdio.h>
#include <string.h>

#include "media_props.h"

// ---------------------------------------------------------------------------
// Identity and trust anchors
// ---------------------------------------------------------------------------

typedef struct {
    gboolean enabled;
    gchar *ca_file;
    gchar *cert_file;
    gchar *key_file;
    gchar *peer_identity;     // expected peer CN/SAN, NULL to accept any CA-signed peer

    GTlsCertificate *own;     // our chain + private key, for TLS
    GTlsDatabase *trust;      // the CA the peer must chain to
    gchar *dtls_pem;          // key + certificate, for dtlssrtpdec's "pem"
} MpPki;

static MpPki* mp_pki_new(void) {
    return g_new0(MpPki, 1);
}

static gboolean mp_pki_is_configured(const MpPki *pki) {
    return pki && (pki->ca_file || pki->cert_file || pki->key_file);
}

static gboolean mp_pki_parse_arg(MpPki *pki, int argc, char **argv, int *i) {
    const gchar *arg = argv[*i];
    if (*i + 1 >= argc) {
        return FALSE;
    }
    struct { const gchar *option; gchar **target; } options[] = {
        { "--pki-ca",            &pki->ca_file },
        { "--pki-cert",          &pki->cert_file },
        { "--pki-key",           &pki->key_file },
        { "--pki-peer-identity", &pki->peer_identity },
    };
    for (guint n = 0; n < G_N_ELEMENTS(options); n++) {
        if (g_strcmp0(arg, options[n].option) == 0) {
            g_free(*options[n].target);
            *options[n].target = g_strdup(argv[++(*i)]);
            return TRUE;
        }
    }
    return FALSE;
}

static void mp_pki_print_usage(FILE *stream) {
    g_fprintf(stream, "\nPKI (all three are required together; off by default):\n");
    g_fprintf(stream, "  --pki-ca <file>             CA the peer's certificate must "
                      "chain to\n");
    g_fprintf(stream, "  --pki-cert <file>           Our certificate chain (PEM)\n");
    g_fprintf(stream, "  --pki-key <file>            Our private key (PEM)\n");
    g_fprintf(stream, "  --pki-peer-identity <name>  Require this CN/SAN in the "
                      "peer's certificate\n");
    g_fprintf(stream, "                              Generate a demo CA with "
                      "tools/make_certs.sh\n");
}

// Colon-separated uppercase SHA-256 of the certificate's DER form - the exact
// shape SDP uses in "a=fingerprint:sha-256 ...".
static gchar* mp_pki_fingerprint(GTlsCertificate *cert) {
    if (!cert) {
        return NULL;
    }
    GByteArray *der = NULL;
    g_object_get(cert, "certificate", &der, NULL);
    if (!der) {
        return NULL;
    }
    gchar *hex = g_compute_checksum_for_data(G_CHECKSUM_SHA256, der->data, der->len);
    g_byte_array_unref(der);
    if (!hex) {
        return NULL;
    }

    GString *out = g_string_new(NULL);
    for (gsize i = 0; hex[i] && hex[i + 1]; i += 2) {
        if (out->len) {
            g_string_append_c(out, ':');
        }
        g_string_append_c(out, g_ascii_toupper(hex[i]));
        g_string_append_c(out, g_ascii_toupper(hex[i + 1]));
    }
    g_free(hex);
    return g_string_free(out, FALSE);
}

// Compare two fingerprints ignoring separators and case, so a peer that writes
// them differently is not mistaken for an attacker.
static gboolean mp_pki_fingerprints_match(const gchar *a, const gchar *b) {
    if (!a || !b) {
        return FALSE;
    }
    const gchar *p = a, *q = b;
    for (;;) {
        while (*p == ':' || *p == ' ' || *p == '-') p++;
        while (*q == ':' || *q == ' ' || *q == '-') q++;
        if (!*p || !*q) {
            return *p == *q;
        }
        if (g_ascii_tolower(*p) != g_ascii_tolower(*q)) {
            return FALSE;
        }
        p++;
        q++;
    }
}

static gboolean mp_pki_load(MpPki *pki, GError **error) {
    if (!mp_pki_is_configured(pki)) {
        pki->enabled = FALSE;
        return TRUE;
    }
    if (!pki->ca_file || !pki->cert_file || !pki->key_file) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                    "PKI needs all three of --pki-ca, --pki-cert and --pki-key");
        return FALSE;
    }

    GTlsBackend *backend = g_tls_backend_get_default();
    if (!backend || !g_tls_backend_supports_tls(backend)) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                    "no TLS backend available; install glib-networking");
        return FALSE;
    }

    // GTlsFileDatabase rejects a relative anchor path, and does it by warning on
    // stderr and handing back a database that then fails to verify anything. A
    // perfectly good CA passed as "pki/ca.crt" would look like an untrusted one.
    gchar *ca_absolute = g_canonicalize_filename(pki->ca_file, NULL);
    g_free(pki->ca_file);
    pki->ca_file = ca_absolute;

    pki->own = g_tls_certificate_new_from_files(pki->cert_file, pki->key_file, error);
    if (!pki->own) {
        return FALSE;
    }
    pki->trust = g_tls_file_database_new(pki->ca_file, error);
    if (!pki->trust) {
        return FALSE;
    }

    // dtlssrtpdec takes one blob holding both the key and the certificate.
    gchar *key_pem = NULL, *cert_pem = NULL;
    if (!g_file_get_contents(pki->key_file, &key_pem, NULL, error) ||
        !g_file_get_contents(pki->cert_file, &cert_pem, NULL, error)) {
        g_free(key_pem);
        g_free(cert_pem);
        return FALSE;
    }
    pki->dtls_pem = g_strconcat(key_pem, "\n", cert_pem, NULL);
    g_free(key_pem);
    g_free(cert_pem);

    gchar *own_fp = mp_pki_fingerprint(pki->own);
    g_print("PKI enabled\n");
    g_print("  identity : %s\n", pki->cert_file);
    g_print("  anchor   : %s\n", pki->ca_file);
    g_print("  expect   : %s\n", pki->peer_identity ? pki->peer_identity
                                                    : "any certificate signed by that CA");
    g_print("  our cert : sha-256 %s\n", own_fp ? own_fp : "unknown");
    g_free(own_fp);

    pki->enabled = TRUE;
    return TRUE;
}

static void mp_pki_free(MpPki *pki) {
    if (!pki) {
        return;
    }
    g_clear_object(&pki->own);
    g_clear_object(&pki->trust);
    g_free(pki->dtls_pem);
    g_free(pki->ca_file);
    g_free(pki->cert_file);
    g_free(pki->key_file);
    g_free(pki->peer_identity);
    g_free(pki);
}

// The one place a peer certificate is judged, used for TLS signalling and for
// the DTLS handshake alike. Accepts only a certificate that chains to our CA,
// and when an expected identity was given, only one whose subject or SAN
// matches it. Both purposes are tried because a single certificate here is used
// as both client and server depending on which side of the channel it is on.
static gboolean mp_pki_verify_peer(MpPki *pki, GTlsCertificate *peer,
                                   const gchar *what, GError **error) {
    if (!peer) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                    "%s: peer presented no certificate", what);
        return FALSE;
    }

    GSocketConnectable *identity = NULL;
    if (pki->peer_identity && *pki->peer_identity) {
        identity = g_network_address_new(pki->peer_identity, 0);
    }

    const gchar *const purposes[] = {
        G_TLS_DATABASE_PURPOSE_AUTHENTICATE_SERVER,
        G_TLS_DATABASE_PURPOSE_AUTHENTICATE_CLIENT,
    };
    // Deliberately not a sentinel value in `best`: GTlsCertificateFlags is a
    // signed enum in C++, so casting G_MAXUINT into it yields -1 and every real
    // verdict - including the successful 0 - compares as larger. That silently
    // turned a valid certificate into "chain could not be validated".
    gboolean have_verdict = FALSE;
    guint best = 0;
    gchar *lookup_error = NULL;
    for (guint i = 0; i < G_N_ELEMENTS(purposes); i++) {
        GError *local = NULL;
        GTlsCertificateFlags flags = g_tls_database_verify_chain(
            pki->trust, peer, purposes[i], identity, NULL,
            G_TLS_DATABASE_VERIFY_NONE, NULL, &local);
        if (local) {
            // A lookup failure is not a verdict, but it must not be silent
            // either: it is the difference between "the peer is not trusted" and
            // "the trust store could not be read".
            if (!lookup_error) {
                lookup_error = g_strdup(local->message);
            }
            g_clear_error(&local);
            continue;
        }
        if (!have_verdict || (guint)flags < best) {
            best = (guint)flags;
            have_verdict = TRUE;
        }
        if (flags == 0) {
            break;
        }
    }
    if (identity) {
        g_object_unref(identity);
    }

    if (!have_verdict || best != 0) {
        GString *why = g_string_new(NULL);
        if (!have_verdict) {
            g_string_append_printf(why, "chain could not be validated: %s",
                                   lookup_error ? lookup_error : "no reason given");
        } else {
            if (best & G_TLS_CERTIFICATE_UNKNOWN_CA)    g_string_append(why, "unknown-CA ");
            if (best & G_TLS_CERTIFICATE_BAD_IDENTITY)  g_string_append(why, "wrong-identity ");
            if (best & G_TLS_CERTIFICATE_NOT_ACTIVATED) g_string_append(why, "not-yet-valid ");
            if (best & G_TLS_CERTIFICATE_EXPIRED)       g_string_append(why, "expired ");
            if (best & G_TLS_CERTIFICATE_REVOKED)       g_string_append(why, "revoked ");
            if (best & G_TLS_CERTIFICATE_INSECURE)      g_string_append(why, "insecure ");
            if (best & G_TLS_CERTIFICATE_GENERIC_ERROR) g_string_append(why, "generic-error ");
        }
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED, "%s: rejected (%s)",
                    what, why->str);
        g_string_free(why, TRUE);
        g_free(lookup_error);
        return FALSE;
    }
    g_free(lookup_error);
    return TRUE;
}

// ---------------------------------------------------------------------------
// Mutually authenticated TLS for the signalling / keying channel
// ---------------------------------------------------------------------------

typedef struct {
    MpPki *pki;
    const gchar *what;
    gboolean ok;
} MpTlsCheck;

static gboolean mp_pki_on_accept_certificate(GTlsConnection *conn,
                                             GTlsCertificate *peer,
                                             GTlsCertificateFlags errors,
                                             gpointer user_data) {
    (void)conn; (void)errors;
    MpTlsCheck *check = (MpTlsCheck *)user_data;
    GError *error = NULL;

    // Deliberately not deferring to `errors`: that verdict is computed against
    // the system trust store and hostname, neither of which is the policy here.
    if (!mp_pki_verify_peer(check->pki, peer, check->what, &error)) {
        g_printerr("TLS: %s\n", error->message);
        g_clear_error(&error);
        check->ok = FALSE;
        return FALSE;
    }
    gchar *fp = mp_pki_fingerprint(peer);
    g_print("TLS: %s verified, peer cert sha-256 %s\n", check->what,
            fp ? fp : "unknown");
    g_free(fp);
    check->ok = TRUE;
    return TRUE;
}

// Wrap an accepted connection as the TLS server, requiring a client
// certificate. Returns a new GIOStream, or NULL with `error` set.
static GIOStream* mp_pki_wrap_server(MpPki *pki, GSocketConnection *conn,
                                     GError **error) {
    GIOStream *tls = g_tls_server_connection_new(G_IO_STREAM(conn), pki->own, error);
    if (!tls) {
        return NULL;
    }
    g_object_set(tls, "authentication-mode", G_TLS_AUTHENTICATION_REQUIRED, NULL);
    g_tls_connection_set_database(G_TLS_CONNECTION(tls), pki->trust);

    MpTlsCheck check = { pki, "client certificate", FALSE };
    g_signal_connect(tls, "accept-certificate",
                     G_CALLBACK(mp_pki_on_accept_certificate), &check);

    if (!g_tls_connection_handshake(G_TLS_CONNECTION(tls), NULL, error)) {
        g_object_unref(tls);
        return NULL;
    }
    // A handshake that completed without our check running would mean the peer
    // sent no certificate at all, which authentication-mode should have
    // prevented. Fail closed rather than assume.
    if (!check.ok) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                    "TLS peer was never authenticated");
        g_object_unref(tls);
        return NULL;
    }
    return tls;
}

// Wrap an outgoing connection as the TLS client, presenting our certificate.
// The server's name is not checked against `host`: these peers are addressed by
// IP and identified by certificate, so --pki-peer-identity is the check that
// matters and hostname matching would only produce false failures.
static GIOStream* mp_pki_wrap_client(MpPki *pki, GSocketConnection *conn,
                                     GError **error) {
    GIOStream *tls = g_tls_client_connection_new(G_IO_STREAM(conn), NULL, error);
    if (!tls) {
        return NULL;
    }
    g_tls_connection_set_certificate(G_TLS_CONNECTION(tls), pki->own);
    g_tls_connection_set_database(G_TLS_CONNECTION(tls), pki->trust);

    MpTlsCheck check = { pki, "server certificate", FALSE };
    g_signal_connect(tls, "accept-certificate",
                     G_CALLBACK(mp_pki_on_accept_certificate), &check);

    if (!g_tls_connection_handshake(G_TLS_CONNECTION(tls), NULL, error)) {
        g_object_unref(tls);
        return NULL;
    }
    if (!check.ok) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                    "TLS peer was never authenticated");
        g_object_unref(tls);
        return NULL;
    }
    return tls;
}

// ---------------------------------------------------------------------------
// DTLS identity for webrtcbin
// ---------------------------------------------------------------------------

typedef struct _MpDtlsGuard MpDtlsGuard;

struct _MpDtlsGuard {
    MpPki *pki;
    gchar *tag;
    GMutex lock;
    GPtrArray *decoders;        // dtlssrtpdec elements owned by webrtcbin
    gchar *remote_fingerprint;  // as promised by the remote SDP
    gint injected;
    gint verified;
    gint rejected;
    gint64 deadline_us;

    // Called once, on the polling worker's thread, if the peer cannot be
    // trusted. The stream is expected to stop.
    void (*on_reject)(MpDtlsGuard *guard, const gchar *reason, gpointer user_data);
    gpointer user_data;
};

static MpDtlsGuard* mp_dtls_guard_new(MpPki *pki, const gchar *tag) {
    MpDtlsGuard *g = g_new0(MpDtlsGuard, 1);
    g->pki = pki;
    g->tag = g_strdup(tag);
    g->decoders = g_ptr_array_new();
    g_mutex_init(&g->lock);
    return g;
}

static void mp_dtls_guard_free(MpDtlsGuard *g) {
    if (!g) {
        return;
    }
    g_ptr_array_free(g->decoders, TRUE);
    g_mutex_clear(&g->lock);
    g_free(g->remote_fingerprint);
    g_free(g->tag);
    g_free(g);
}

// webrtcbin builds its DTLS elements while it is generating the SDP, and the
// fingerprint it advertises is read from whatever certificate dtlssrtpdec holds
// at that moment. GstBin::deep-element-added fires synchronously as the element
// is added, which is the last point before that read - so this is where our
// certificate has to be installed. Setting it any later would advertise one
// fingerprint and then present a different certificate.
static void mp_dtls_guard_on_deep_element_added(GstBin *bin, GstBin *sub_bin,
                                                GstElement *element,
                                                gpointer user_data) {
    (void)bin; (void)sub_bin;
    MpDtlsGuard *guard = (MpDtlsGuard *)user_data;

    GstElementFactory *factory = gst_element_get_factory(element);
    const gchar *name = factory ? GST_OBJECT_NAME(factory) : NULL;
    if (g_strcmp0(name, "dtlssrtpdec") != 0) {
        return;
    }

    if (!mp_find_prop(element, "pem")) {
        g_printerr("[%s] dtlssrtpdec has no \"pem\" property; cannot install the "
                   "PKI identity\n", guard->tag);
        return;
    }
    g_object_set(element, "pem", guard->pki->dtls_pem, NULL);

    g_mutex_lock(&guard->lock);
    g_ptr_array_add(guard->decoders, element);
    guard->injected = 1;
    g_mutex_unlock(&guard->lock);

    g_print("[%s] DTLS certificate replaced with the PKI identity (%s)\n",
            guard->tag, GST_ELEMENT_NAME(element));
}

static void mp_dtls_guard_attach(MpDtlsGuard *guard, GstElement *webrtcbin,
                                 gint verify_timeout_seconds) {
    guard->deadline_us = g_get_monotonic_time() +
                         (gint64)verify_timeout_seconds * G_USEC_PER_SEC;
    g_signal_connect(webrtcbin, "deep-element-added",
                     G_CALLBACK(mp_dtls_guard_on_deep_element_added), guard);
}

// Remember what the remote SDP promised its certificate would hash to. Any
// m-line will do: with max-bundle there is a single DTLS association, and
// webrtcbin repeats the same fingerprint on every m-line regardless.
static void mp_dtls_guard_note_remote_sdp(MpDtlsGuard *guard, const gchar *sdp) {
    if (!sdp) {
        return;
    }
    const gchar *at = strstr(sdp, "fingerprint:sha-256 ");
    if (!at) {
        return;
    }
    at += strlen("fingerprint:sha-256 ");
    const gchar *end = at;
    while (*end && *end != '\r' && *end != '\n') {
        end++;
    }
    g_mutex_lock(&guard->lock);
    g_free(guard->remote_fingerprint);
    guard->remote_fingerprint = g_strndup(at, (gsize)(end - at));
    g_mutex_unlock(&guard->lock);
}

// Make our own SDP advertise the certificate we will actually present.
//
// Injecting the certificate on "deep-element-added" is as early as any hook
// allows, and it is still a fraction too late for the SDP: webrtcbin reads a
// certificate out of the freshly constructed dtlssrtpdec to build the
// fingerprint attribute *before* that element is added to the bin, so the
// fingerprint it advertises belongs to the throwaway certificate the element
// generated on demand. The handshake then presents ours. Measured: SDP said
// 6C:11:B5..., the DTLS handshake presented 7B:79:39... - and GStreamer 1.16
// never compares the two, so nothing complained.
//
// Rewriting the attribute is the fix, and it makes the SDP honest rather than
// merely self-consistent: after this, the fingerprint is the one a peer can hold
// the handshake to. The local description keeps webrtcbin's original text, since
// DTLS uses the certificate in the element and not the description.
static gchar* mp_pki_rewrite_sdp_fingerprint(MpPki *pki, const gchar *sdp,
                                             const gchar *tag) {
    gchar *fingerprint = mp_pki_fingerprint(pki->own);
    if (!fingerprint) {
        return g_strdup(sdp);
    }

    gchar **lines = g_strsplit(sdp, "\n", -1);
    guint replaced = 0;
    for (guint i = 0; lines[i]; i++) {
        if (!g_str_has_prefix(lines[i], "a=fingerprint:")) {
            continue;
        }
        // gst_sdp_message_as_text() emits CRLF; keep whatever is there so the
        // byte count the peer is told still matches the body.
        const gchar *tail = g_str_has_suffix(lines[i], "\r") ? "\r" : "";
        gchar *replacement = g_strdup_printf("a=fingerprint:sha-256 %s%s",
                                             fingerprint, tail);
        g_free(lines[i]);
        lines[i] = replacement;
        replaced++;
    }
    gchar *out = g_strjoinv("\n", lines);
    g_strfreev(lines);

    if (replaced) {
        g_print("[%s] SDP fingerprint set to the installed certificate (%u "
                "attribute%s)\n", tag, replaced, replaced == 1 ? "" : "s");
    } else {
        g_printerr("[%s] no fingerprint attribute in the SDP to correct\n", tag);
    }
    g_free(fingerprint);
    return out;
}

static void mp_dtls_guard_reject(MpDtlsGuard *guard, const gchar *reason) {
    if (!g_atomic_int_compare_and_exchange(&guard->rejected, 0, 1)) {
        return;
    }
    g_printerr("[%s] DTLS peer rejected: %s\n", guard->tag, reason);
    if (guard->on_reject) {
        guard->on_reject(guard, reason, guard->user_data);
    }
}

// Poll the handshake result. Returns FALSE when there is nothing left to decide,
// so it can be used directly as a GSourceFunc.
static gboolean mp_dtls_guard_poll(gpointer user_data) {
    MpDtlsGuard *guard = (MpDtlsGuard *)user_data;
    if (g_atomic_int_get(&guard->verified) || g_atomic_int_get(&guard->rejected)) {
        return G_SOURCE_REMOVE;
    }

    g_mutex_lock(&guard->lock);
    GstElement *decoder = guard->decoders->len
        ? (GstElement *)g_ptr_array_index(guard->decoders, 0) : NULL;
    gchar *promised = g_strdup(guard->remote_fingerprint);
    g_mutex_unlock(&guard->lock);

    gchar *peer_pem = NULL;
    if (decoder && mp_find_prop(decoder, "peer-pem")) {
        g_object_get(decoder, "peer-pem", &peer_pem, NULL);
    }

    if (!peer_pem || !*peer_pem) {
        g_free(peer_pem);
        g_free(promised);
        if (g_get_monotonic_time() > guard->deadline_us) {
            mp_dtls_guard_reject(guard, "no DTLS handshake completed before the "
                                        "verification deadline");
            return G_SOURCE_REMOVE;
        }
        return G_SOURCE_CONTINUE;
    }

    GError *error = NULL;
    GTlsCertificate *peer = g_tls_certificate_new_from_pem(peer_pem, -1, &error);
    g_free(peer_pem);
    if (!peer) {
        gchar *reason = g_strdup_printf("peer certificate is unreadable: %s",
                                        error ? error->message : "unknown");
        mp_dtls_guard_reject(guard, reason);
        g_free(reason);
        g_clear_error(&error);
        g_free(promised);
        return G_SOURCE_REMOVE;
    }

    if (!mp_pki_verify_peer(guard->pki, peer, "DTLS certificate", &error)) {
        mp_dtls_guard_reject(guard, error->message);
        g_clear_error(&error);
        g_object_unref(peer);
        g_free(promised);
        return G_SOURCE_REMOVE;
    }

    // The check GStreamer 1.16 does not do. Without it, a certificate signed by
    // the CA could be swapped in by anyone else holding one - the SDP said which
    // certificate to expect, so hold the handshake to it.
    gchar *actual = mp_pki_fingerprint(peer);
    if (!promised) {
        mp_dtls_guard_reject(guard, "remote SDP carried no sha-256 fingerprint");
    } else if (!mp_pki_fingerprints_match(promised, actual)) {
        gchar *reason = g_strdup_printf(
            "certificate does not match the fingerprint the SDP promised\n"
            "        SDP  : %s\n        DTLS : %s", promised, actual ? actual : "?");
        mp_dtls_guard_reject(guard, reason);
        g_free(reason);
    } else {
        g_atomic_int_set(&guard->verified, 1);
        g_print("[%s] DTLS peer verified: CA-signed, fingerprint matches the SDP"
                "%s\n", guard->tag,
                guard->pki->peer_identity ? ", identity matches" : "");
    }

    g_free(actual);
    g_free(promised);
    g_object_unref(peer);
    return G_SOURCE_REMOVE;
}

// ---------------------------------------------------------------------------
// SRTP for the RTP/UDP transport
//
// AES-128 counter mode with an 80-bit HMAC-SHA1 tag, which is the same suite
// DTLS-SRTP negotiates on the WebRTC path, so the two transports end up with
// comparable protection. The master key is 30 bytes: 16 of key and 14 of salt.
// ---------------------------------------------------------------------------

#define MP_SRTP_KEY_LEN 30
#define MP_SRTP_CIPHER  "aes-128-icm"
#define MP_SRTP_AUTH    "hmac-sha1-80"

typedef struct {
    gboolean enabled;
    gchar *label;                     // stream name, for logging
    guint8 key[MP_SRTP_KEY_LEN];
    gboolean have_key;
    GMutex lock;
    GCond cond;
} MpSrtpKey;

static void mp_srtp_key_init(MpSrtpKey *k, const gchar *label, gboolean enabled) {
    memset(k, 0, sizeof(*k));
    k->label = g_strdup(label);
    k->enabled = enabled;
    g_mutex_init(&k->lock);
    g_cond_init(&k->cond);
}

static void mp_srtp_key_clear(MpSrtpKey *k) {
    if (!k->label) {
        return;
    }
    // Do not leave key material in freed heap memory.
    memset(k->key, 0, sizeof(k->key));
    g_free(k->label);
    k->label = NULL;
    g_mutex_clear(&k->lock);
    g_cond_clear(&k->cond);
}

// Master keys come from the kernel CSPRNG. GLib's GRand is a Mersenne Twister
// seeded from the clock and is not fit for key material.
static gboolean mp_srtp_key_generate(MpSrtpKey *k, GError **error) {
    guint8 buffer[MP_SRTP_KEY_LEN];
    GError *local = NULL;
    GFile *urandom = g_file_new_for_path("/dev/urandom");
    GFileInputStream *stream = g_file_read(urandom, NULL, &local);
    gboolean ok = FALSE;
    if (stream) {
        gsize got = 0;
        ok = g_input_stream_read_all(G_INPUT_STREAM(stream), buffer, sizeof(buffer),
                                     &got, NULL, &local) && got == sizeof(buffer);
        g_object_unref(stream);
    }
    g_object_unref(urandom);

    if (!ok) {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                    "could not read %d random bytes for the %s SRTP key: %s",
                    MP_SRTP_KEY_LEN, k->label,
                    local ? local->message : "short read");
        g_clear_error(&local);
        return FALSE;
    }

    g_mutex_lock(&k->lock);
    memcpy(k->key, buffer, sizeof(buffer));
    k->have_key = TRUE;
    g_cond_broadcast(&k->cond);
    g_mutex_unlock(&k->lock);
    memset(buffer, 0, sizeof(buffer));
    return TRUE;
}

static gchar* mp_srtp_key_to_base64(MpSrtpKey *k) {
    g_mutex_lock(&k->lock);
    gchar *b64 = k->have_key ? g_base64_encode(k->key, MP_SRTP_KEY_LEN) : NULL;
    g_mutex_unlock(&k->lock);
    return b64;
}

static gboolean mp_srtp_key_set_base64(MpSrtpKey *k, const gchar *b64) {
    gsize length = 0;
    guchar *raw = g_base64_decode(b64, &length);
    if (!raw || length != MP_SRTP_KEY_LEN) {
        g_printerr("SRTP key for %s is %zu bytes, expected %d\n",
                   k->label, length, MP_SRTP_KEY_LEN);
        g_free(raw);
        return FALSE;
    }
    g_mutex_lock(&k->lock);
    memcpy(k->key, raw, MP_SRTP_KEY_LEN);
    k->have_key = TRUE;
    g_cond_broadcast(&k->cond);
    g_mutex_unlock(&k->lock);
    memset(raw, 0, length);
    g_free(raw);
    g_print("SRTP key installed for %s (%s, %s)\n", k->label,
            MP_SRTP_CIPHER, MP_SRTP_AUTH);
    return TRUE;
}

// srtpdec asks for the key once per SSRC, on the streaming thread, the moment
// the first packet arrives. If the answer is "not yet" the packet is dropped and
// nothing asks again, so wait here instead of guessing that the keying channel
// won the race.
static gboolean mp_srtp_key_wait(MpSrtpKey *k, gint timeout_ms) {
    gint64 deadline = g_get_monotonic_time() + (gint64)timeout_ms * 1000;
    g_mutex_lock(&k->lock);
    while (!k->have_key) {
        if (!g_cond_wait_until(&k->cond, &k->lock, deadline)) {
            g_mutex_unlock(&k->lock);
            return FALSE;
        }
    }
    g_mutex_unlock(&k->lock);
    return TRUE;
}

static GstBuffer* mp_srtp_key_buffer(MpSrtpKey *k) {
    g_mutex_lock(&k->lock);
    GstBuffer *buffer = k->have_key
        ? gst_buffer_new_wrapped(g_memdup(k->key, MP_SRTP_KEY_LEN), MP_SRTP_KEY_LEN)
        : NULL;
    g_mutex_unlock(&k->lock);
    return buffer;
}

// Insert srtpenc after `upstream`. Returns the pad to link downstream from
// (transfer full), or NULL on failure.
static GstPad* mp_srtp_insert_sender(GstBin *bin, MpSrtpKey *key,
                                     GstElement *upstream, const gchar *name) {
    GstElement *enc = gst_element_factory_make("srtpenc", name);
    if (!enc) {
        g_printerr("srtpenc is unavailable; install gstreamer1.0-plugins-bad\n");
        return NULL;
    }
    GstBuffer *key_buffer = mp_srtp_key_buffer(key);
    if (!key_buffer) {
        g_printerr("no SRTP key for %s yet; refusing to send unencrypted\n",
                   key->label);
        gst_object_unref(enc);
        return NULL;
    }
    g_object_set(enc, "key", key_buffer, NULL);
    gst_buffer_unref(key_buffer);

    const gchar *const cipher[] = {MP_SRTP_CIPHER, NULL};
    const gchar *const auth[] = {MP_SRTP_AUTH, NULL};
    mp_set_enum_prop(enc, "rtp-cipher", cipher);
    mp_set_enum_prop(enc, "rtp-auth", auth);
    mp_set_enum_prop(enc, "rtcp-cipher", cipher);
    mp_set_enum_prop(enc, "rtcp-auth", auth);

    gst_bin_add(bin, enc);

    // srtpenc's pads are request pads, and the matching src pad only exists once
    // the sink pad has been requested.
    GstPad *sink = gst_element_request_pad_simple(enc, "rtp_sink_0");
    GstPad *src = gst_element_get_static_pad(enc, "rtp_src_0");
    GstPad *up = gst_element_get_static_pad(upstream, "src");
    if (!sink || !src || !up || gst_pad_link(up, sink) != GST_PAD_LINK_OK) {
        g_printerr("Failed to link %s into srtpenc\n", GST_ELEMENT_NAME(upstream));
        if (sink) gst_object_unref(sink);
        if (src) gst_object_unref(src);
        if (up) gst_object_unref(up);
        return NULL;
    }
    gst_object_unref(sink);
    gst_object_unref(up);
    g_print("SRTP encryption on for %s (%s, %s)\n", key->label,
            MP_SRTP_CIPHER, MP_SRTP_AUTH);
    return src;
}

static GstCaps* mp_srtp_on_request_key(GstElement *srtpdec, guint ssrc,
                                       gpointer user_data) {
    (void)srtpdec;
    MpSrtpKey *key = (MpSrtpKey *)user_data;

    if (!mp_srtp_key_wait(key, 10000)) {
        g_printerr("No SRTP key for %s after 10 s; dropping stream 0x%08x\n",
                   key->label, ssrc);
        return NULL;
    }
    GstBuffer *key_buffer = mp_srtp_key_buffer(key);
    if (!key_buffer) {
        return NULL;
    }
    GstCaps *caps = gst_caps_new_simple(
        "application/x-srtp",
        "srtp-key", GST_TYPE_BUFFER, key_buffer,
        "srtp-cipher", G_TYPE_STRING, MP_SRTP_CIPHER,
        "srtp-auth", G_TYPE_STRING, MP_SRTP_AUTH,
        "srtcp-cipher", G_TYPE_STRING, MP_SRTP_CIPHER,
        "srtcp-auth", G_TYPE_STRING, MP_SRTP_AUTH,
        NULL);
    gst_buffer_unref(key_buffer);
    g_print("SRTP key supplied for %s stream 0x%08x\n", key->label, ssrc);
    return caps;
}

// Create the receive-side srtpdec. Its pads are named rtp_sink/rtp_src rather
// than sink/src, so callers link them by name.
static GstElement* mp_srtp_make_receiver(GstBin *bin, MpSrtpKey *key,
                                         const gchar *name) {
    GstElement *dec = gst_element_factory_make("srtpdec", name);
    if (!dec) {
        g_printerr("srtpdec is unavailable; install gstreamer1.0-plugins-bad\n");
        return NULL;
    }
    g_signal_connect(dec, "request-key", G_CALLBACK(mp_srtp_on_request_key), key);
    gst_bin_add(bin, dec);
    g_print("SRTP decryption on for %s\n", key->label);
    return dec;
}

#endif  // MEDIA_PKI_H

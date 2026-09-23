#!/usr/bin/env bash
#
# Generate a demo PKI for the streaming pair: one CA, one certificate for the
# sender and one for the receiver.
#
#   ./tools/make_certs.sh [output-dir] [extra-SAN ...]
#
# Defaults to ./pki. Pass any extra names or addresses the certificates should
# be valid for, which matters when the two ends are on different machines:
#
#   ./tools/make_certs.sh pki 10.0.0.5 10.0.0.6 operator.theatre.local
#
# The result is a self-contained trust root for a private link:
#
#   pki/ca.crt                the trust anchor both ends are given
#   pki/sender.crt   .key     identity for server / webrtc_server
#   pki/receiver.crt .key     identity for client / webrtc_client
#
# Then, on the receiving side:
#
#   ./build/webrtc_client --pki-ca pki/ca.crt \
#       --pki-cert pki/receiver.crt --pki-key pki/receiver.key \
#       --pki-peer-identity stream-sender
#
# and on the sending side:
#
#   ./build/webrtc_server test.yuv --pki-ca pki/ca.crt \
#       --pki-cert pki/sender.crt --pki-key pki/sender.key \
#       --pki-peer-identity stream-receiver
#
# This is a demo CA, not a production one: the CA private key is written next to
# the certificates with no passphrase and no revocation infrastructure. For a
# real deployment, issue the two leaf certificates from whatever CA already
# governs the site and point --pki-ca at that.

set -euo pipefail

OUT="${1:-pki}"
shift || true
EXTRA_SANS=("$@")

DAYS=825            # the common maximum for leaf certificates
KEY_BITS=2048       # RSA: gst's DTLS agent asks for an RSA key by name

command -v openssl > /dev/null || { echo "openssl is not installed" >&2; exit 1; }

mkdir -p "$OUT"
cd "$OUT"

if [[ -f ca.crt ]]; then
    echo "refusing to overwrite an existing CA in $(pwd)" >&2
    echo "delete it first if that is what you want" >&2
    exit 1
fi

# Build the subjectAltName list. The CN is checked by --pki-peer-identity, and
# GnuTLS matches names against the SAN, so each name goes in both places.
san_for() {
    local cn="$1"
    local sans="DNS:${cn},DNS:localhost,IP:127.0.0.1,IP:::1"
    for extra in ${EXTRA_SANS[@]+"${EXTRA_SANS[@]}"}; do
        if [[ "$extra" =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
            sans+=",IP:${extra}"
        else
            sans+=",DNS:${extra}"
        fi
    done
    echo "$sans"
}

echo "=== certificate authority ==="
# Deliberately no -addext basicConstraints here: "req -x509" already adds
# "basicConstraints=critical,CA:TRUE" of its own accord, and adding a second copy
# produces a certificate with the extension twice. OpenSSL then refuses to use it
# as an issuer at all, which surfaces much later as "unable to get local issuer
# certificate" against a CA that looks perfectly good in the text dump.
openssl req -x509 -newkey "rsa:${KEY_BITS}" -nodes -sha256 -days 3650 \
    -keyout ca.key -out ca.crt \
    -subj "/O=Low-Latency Streaming Demo/CN=stream-demo-ca" \
    -addext "keyUsage=critical,keyCertSign,cRLSign" 2> /dev/null
chmod 600 ca.key
openssl verify -CAfile ca.crt ca.crt > /dev/null

issue() {
    local name="$1" cn="$2"
    echo "=== ${name} (CN=${cn}) ==="
    openssl req -newkey "rsa:${KEY_BITS}" -nodes -sha256 \
        -keyout "${name}.key" -out "${name}.csr" \
        -subj "/O=Low-Latency Streaming Demo/CN=${cn}" 2> /dev/null
    # Both extended key usages: each certificate is presented as a TLS client on
    # one channel and as a TLS/DTLS server on another, depending on which side
        # of a given connection it is, and the verifier checks the purpose.
    openssl x509 -req -in "${name}.csr" -CA ca.crt -CAkey ca.key \
        -CAcreateserial -sha256 -days "$DAYS" -out "${name}.crt" \
        -extfile <(printf '%s\n' \
            "basicConstraints=critical,CA:FALSE" \
            "keyUsage=critical,digitalSignature,keyEncipherment" \
            "extendedKeyUsage=serverAuth,clientAuth" \
            "subjectAltName=$(san_for "$cn")") 2> /dev/null
    rm -f "${name}.csr"
    chmod 600 "${name}.key"

    # Fail loudly here rather than at handshake time.
    openssl verify -CAfile ca.crt "${name}.crt" > /dev/null
    echo "    sha-256 fingerprint: $(openssl x509 -in "${name}.crt" -noout \
        -fingerprint -sha256 | cut -d= -f2)"
}

issue sender stream-sender
issue receiver stream-receiver

echo
echo "PKI written to $(pwd)"
echo "  anchor    ca.crt        (both ends need this)"
echo "  sender    sender.crt    CN=stream-sender"
echo "  receiver  receiver.crt  CN=stream-receiver"
echo
echo "Names these certificates are valid for: $(san_for stream-sender)"

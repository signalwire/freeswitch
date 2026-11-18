
#include <switch.h>
#include <test/switch_test.h>

#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/x509.h>

#include <srtp.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>

#ifndef MSG_CONFIRM
#define MSG_CONFIRM 0
#endif

static const char *rx_host = "127.0.0.1";
static switch_port_t rx_port = 1234;
static const char *tx_host = "127.0.0.1";
static switch_port_t tx_port = 54320;
static switch_memory_pool_t *pool = NULL;
static switch_rtp_t *rtp_session = NULL;
static switch_rtp_flag_t flags[SWITCH_RTP_FLAG_INVALID] = {0};
const char *err = NULL;
static const switch_payload_t TEST_PT = 8;
switch_rtp_packet_t rtp_packet;
switch_frame_flag_t *frame_flags;
switch_io_flag_t io_flags;
switch_payload_t read_pt;
int send_rtcp_test_success = 0;

static void show_event(switch_event_t *event) {
	char *str;
	/*print the event*/
	switch_event_serialize_json(event, &str);
	if (str) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s\n", str);
		switch_safe_free(str);
	}
}

static void send_rtcp_event_handler(switch_event_t *event) 
{
	const char *new_ev = switch_event_get_header(event, "Event-Name");

	if (new_ev && !strcmp(new_ev, "SEND_RTCP_MESSAGE")) { 
		send_rtcp_test_success = 1;
	}

	show_event(event);
}

/* SRTP profile FreeSWITCH offers; the client must offer it too so the DTLS-SRTP
 * key material can be exported and the server can reach DS_READY. */
#define TEST_SRTP_PROFILE "SRTP_AES128_CM_SHA1_80"

/* Minimal OpenSSL DTLS client speaking to the FreeSWITCH RTP socket. */
typedef struct {
	SSL_CTX *ctx;
	SSL *ssl;
	int fd;
} dtls_test_client_t;

/*
 * Build a DTLS client bound to client_port and connected to the FreeSWITCH RTP socket at
 * server_port. When present_cert is set the client loads the same PEM FreeSWITCH uses, so
 * its certificate fingerprint equals the FreeSWITCH cert fingerprint; when clear the client
 * presents no certificate. Returns 0 on success.
 */
static int dtls_test_client_create(dtls_test_client_t *client, switch_port_t client_port, switch_port_t server_port, int present_cert)
{
	struct sockaddr_in local_addr = { 0 };
	struct sockaddr_in server_addr = { 0 };
	BIO *bio = NULL;
	int flags;

	memset(client, 0, sizeof(*client));
	client->fd = -1;

	client->fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (client->fd < 0) {
		return -1;
	}

	local_addr.sin_family = AF_INET;
	local_addr.sin_addr.s_addr = inet_addr(rx_host);
	local_addr.sin_port = htons(client_port);
	if (bind(client->fd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
		return -1;
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(rx_host);
	server_addr.sin_port = htons(server_port);
	if (connect(client->fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
		return -1;
	}

	flags = fcntl(client->fd, F_GETFL, 0);
	fcntl(client->fd, F_SETFL, flags | O_NONBLOCK);

	if (!(client->ctx = SSL_CTX_new(DTLS_client_method()))) {
		return -1;
	}

	/* The client does not validate the FreeSWITCH certificate; this test exercises the
	 * server-side check only. */
	SSL_CTX_set_verify(client->ctx, SSL_VERIFY_NONE, NULL);
	SSL_CTX_set_cipher_list(client->ctx, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");
	SSL_CTX_set_tlsext_use_srtp(client->ctx, TEST_SRTP_PROFILE);

	if (present_cert) {
		char pem[1024] = "";

		switch_snprintf(pem, sizeof(pem), "%s%s%s.pem", SWITCH_GLOBAL_dirs.certs_dir, SWITCH_PATH_SEPARATOR, DTLS_SRTP_FNAME);

		if (SSL_CTX_use_certificate_file(client->ctx, pem, SSL_FILETYPE_PEM) != 1 ||
			SSL_CTX_use_PrivateKey_file(client->ctx, pem, SSL_FILETYPE_PEM) != 1) {
			return -1;
		}
	}

	if (!(client->ssl = SSL_new(client->ctx))) {
		return -1;
	}

	if (!(bio = BIO_new_dgram(client->fd, BIO_NOCLOSE))) {
		return -1;
	}

	BIO_ctrl(bio, BIO_CTRL_DGRAM_SET_CONNECTED, 0, &server_addr);
	SSL_set_bio(client->ssl, bio, bio);
	SSL_set_connect_state(client->ssl);

	return 0;
}

static void dtls_test_client_destroy(dtls_test_client_t *client)
{
	if (client->ssl) {
		SSL_free(client->ssl);
		client->ssl = NULL;
	}

	if (client->ctx) {
		SSL_CTX_free(client->ctx);
		client->ctx = NULL;
	}

	if (client->fd >= 0) {
		close(client->fd);
		client->fd = -1;
	}
}

/* Advance the client handshake one step; returns 1 once the client handshake finishes. */
static int dtls_test_client_step(dtls_test_client_t *client)
{
	if (SSL_do_handshake(client->ssl) == 1) {
		return 1;
	}

	/* WANT_READ/WANT_WRITE is expected on the non-blocking datagram BIO between flights. */
	return SSL_is_init_finished(client->ssl) ? 1 : 0;
}

/* Which SDP a=fingerprint the harness hands FreeSWITCH for the peer. */
typedef enum {
	REMOTE_FP_ABSENT,    /* peer advertised no fingerprint (remote_fp left empty) */
	REMOTE_FP_MATCH,     /* fingerprint matches the client certificate */
	REMOTE_FP_DIFFERENT  /* fingerprint present but does not match the client certificate */
} remote_fp_case_t;

/*
 * Run one verification scenario end to end and return the terminal DTLS state FreeSWITCH
 * reaches. verify_mode is the rtp_dtls_client_cert_verify_mode value (NULL leaves it unset,
 * i.e. the default). present_cert controls whether the client sends a certificate.
 * remote_fp_case selects the SDP a=fingerprint FreeSWITCH is given for the peer: absent,
 * matching, or different. Returns DS_OFF if the harness could not be set up.
 */
static dtls_state_t run_client_cert_verify_case(const char *verify_mode, int present_cert, remote_fp_case_t remote_fp_case)
{
	static switch_port_t port_base = 50000;
	switch_core_session_t *session = NULL;
	switch_channel_t *channel = NULL;
	switch_call_cause_t cause;
	switch_rtp_t *dtls_rtp = NULL;
	switch_rtp_flag_t dtls_flags[SWITCH_RTP_FLAG_INVALID] = { 0 };
	dtls_fingerprint_t local_fp = { 0 };
	dtls_fingerprint_t remote_fp = { 0 };
	dtls_test_client_t client;
	dtls_state_t state = DS_OFF;
	const char *dtls_err = NULL;
	switch_port_t server_port, client_port;
	char rbuf[SWITCH_RECOMMENDED_BUFFER_SIZE];
	int client_ready = 0, have_client = 0;
	int i;

	memset(&client, 0, sizeof(client));
	client.fd = -1;

	/* Ensure the DTLS-SRTP certificate exists (idempotent; skips if already generated). */
	switch_core_gen_certs(DTLS_SRTP_FNAME);

	/* The FreeSWITCH side binds the configured RTP port, which the core port allocator
	 * manages; passing 0 to switch_rtp_set_start_port() reads it back without setting it.
	 * The peer socket is bound by this test directly, so it takes a port of its own. */
	server_port = switch_rtp_set_start_port(0);
	client_port = port_base++;

	if (switch_ivr_originate(NULL, &session, &cause, "null/+15553334444", 2, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL) != SWITCH_STATUS_SUCCESS || !session) {
		goto done;
	}

	channel = switch_core_session_get_channel(session);

	if (!zstr(verify_mode)) {
		switch_channel_set_variable(channel, "rtp_dtls_client_cert_verify_mode", verify_mode);
	}

	/* FreeSWITCH binds server_port; its media destination is the client at client_port. The RTP
	 * session uses the call session's own pool, so rtp_session->session is populated (the pool
	 * carries the "__session" back-pointer switch_rtp_create() reads). */
	dtls_rtp = switch_rtp_new(rx_host, server_port, rx_host, client_port, TEST_PT, 8000, 20 * 1000, dtls_flags, "soft", &dtls_err, switch_core_session_get_pool(session), 0, 0);
	if (!dtls_rtp || !switch_rtp_ready(dtls_rtp)) {
		goto done;
	}

	switch_core_media_set_rtp_session(session, SWITCH_MEDIA_TYPE_AUDIO, dtls_rtp);
	switch_rtp_set_remote_address(dtls_rtp, rx_host, client_port, 0, SWITCH_FALSE, &dtls_err);

	if (dtls_test_client_create(&client, client_port, server_port, present_cert) != 0) {
		goto done;
	}
	have_client = 1;

	/* FreeSWITCH cert fingerprint. The client (when it presents a cert) uses the same PEM,
	 * so a matching expected fingerprint is exactly the FreeSWITCH cert fingerprint. */
	local_fp.type = "sha-256";
	if (!switch_core_cert_gen_fingerprint(DTLS_SRTP_FNAME, &local_fp)) {
		goto done;
	}

	/* REMOTE_FP_ABSENT leaves remote_fp zeroed, emulating a peer that sent no SDP a=fingerprint. */
	if (remote_fp_case != REMOTE_FP_ABSENT) {
		remote_fp = local_fp;
		if (remote_fp_case == REMOTE_FP_DIFFERENT) {
			/* Flip one hex nibble so the expected fingerprint cannot match the client cert. */
			remote_fp.str[0] = (remote_fp.str[0] == '0') ? '1' : '0';
		}
	}

	if (switch_rtp_add_dtls(dtls_rtp, &local_fp, &remote_fp, DTLS_TYPE_SERVER | DTLS_TYPE_RTP, 0) != SWITCH_STATUS_SUCCESS) {
		goto done;
	}

	/* Pump both sides until the FreeSWITCH DTLS state machine settles. switch_rtp_read()
	 * drives do_dtls() on the FreeSWITCH side; dtls_test_client_step() advances the client. */
	for (i = 0; i < 200; i++) {
		uint32_t rlen = sizeof(rbuf);
		switch_payload_t pt = 0;
		switch_frame_flag_t frame_flags = 0;

		if (!client_ready) {
			client_ready = dtls_test_client_step(&client);
		}

		switch_rtp_read(dtls_rtp, (void *)rbuf, &rlen, &pt, &frame_flags, 0);

		state = switch_rtp_dtls_state(dtls_rtp, DTLS_TYPE_RTP);
		if (state == DS_READY || state == DS_FAIL) {
			break;
		}
	}

 done:

	if (have_client) {
		dtls_test_client_destroy(&client);
	}

	if (dtls_rtp) {
		switch_rtp_destroy(&dtls_rtp);
	}

	if (session) {
		switch_channel_hangup(switch_core_session_get_channel(session), SWITCH_CAUSE_NORMAL_CLEARING);
		switch_core_session_rwunlock(session);
	}

	return state;
}

/*
 * SRTP re-key after a sequence number wrap. The peer is a libsrtp sender, so the test chooses the
 * rollover counter (ROC) its packets carry after a re-key: srtp_set_stream_roc() on the new sender
 * context continues the old counter (RFC 3711 3.3.1), leaving it unset restarts at 0 (RFC 4568 7.1.4).
 * libsrtp's AEAD protect path ignores srtp_set_stream_roc(), so the GCM sender can only restart.
 */
#define SRTP_TEST_PAYLOAD_LEN 160
#define SRTP_TEST_PACKETS 100
#define SRTP_TEST_WARMUP 60
#define SRTP_TEST_KEYSALT_LEN 30

typedef enum {
	SRTP_TEST_AES_CM_128_HMAC_SHA1_80,
	SRTP_TEST_AES_CM_128_HMAC_SHA1_80_MKI,
	SRTP_TEST_AEAD_AES_128_GCM
} srtp_test_suite_t;

typedef enum {
	SRTP_REKEY_PEER_KEEPS_ROC,        /* the peer keeps its ROC */
	SRTP_REKEY_PEER_RESETS_ROC,       /* the peer restarts at ROC 0 */
	SRTP_REKEY_OTHER_SSRC_FIRST,      /* a second SSRC sends first after the re-key, then the peer keeps */
	SRTP_REKEY_OTHER_SSRC_THEN_RESET, /* a late old-key packet and a second SSRC, then the peer resets */
	SRTP_REKEY_TWICE,                 /* a second re-key before the first is decided, then the peer keeps */
	SRTP_REKEY_WRAP_IN_GAP            /* the peer wraps while its packets are lost around the re-key, and keeps */
} srtp_rekey_case_t;

typedef struct {
	int fd;
	struct sockaddr_in fs_addr;
	srtp_test_suite_t suite;
	srtp_t ctx;
	uint32_t ssrc;
	uint16_t seq;
	uint32_t roc; /* ROC of the next packet, as the peer counts it */
	uint32_t ts;
} srtp_test_peer_t;

typedef struct {
	int before; /* decrypted before the re-key, of SRTP_TEST_PACKETS */
	int other;  /* decrypted from the second SSRC, of SRTP_TEST_PACKETS */
	int after;  /* decrypted from the peer after the re-key, of SRTP_TEST_PACKETS; -1 if the harness failed */
} srtp_rekey_result_t;

/* MKI shared by FreeSWITCH's key material and the sender, in the byte order FreeSWITCH copies it in. */
static const unsigned int srtp_test_mki_id = 1;

static void srtp_test_keysalt(unsigned char *keysalt, unsigned char fill)
{
	int i;

	for (i = 0; i < SRTP_TEST_KEYSALT_LEN; i++) {
		keysalt[i] = (unsigned char) (fill + i);
	}
}

/* Give FreeSWITCH's receive side a key, the way an SDES offer or answer does. */
static switch_status_t srtp_test_fs_key(switch_rtp_t *rtp, srtp_test_suite_t suite, const unsigned char *keysalt)
{
	switch_secure_settings_t ssec = { 0 };
	switch_crypto_key_material_t key_material = { 0 };

	ssec.crypto_type = suite == SRTP_TEST_AEAD_AES_128_GCM ? AEAD_AES_128_GCM : AES_CM_128_HMAC_SHA1_80;
	memcpy(ssec.remote_raw_key, keysalt, SRTP_TEST_KEYSALT_LEN);

	if (suite == SRTP_TEST_AES_CM_128_HMAC_SHA1_80_MKI) {
		memcpy(key_material.raw_key, keysalt, SRTP_TEST_KEYSALT_LEN);
		key_material.mki_id = srtp_test_mki_id;
		key_material.mki_size = sizeof(srtp_test_mki_id);
		ssec.remote_key_material_next = &key_material;
		ssec.remote_key_material_n = 1;
	}

	return switch_rtp_add_crypto_key(rtp, SWITCH_RTP_CRYPTO_RECV, 1, &ssec);
}

/* (Re)key the sender with a new libsrtp context, continuing its ROC when keep_roc is set. */
static int srtp_test_peer_key(srtp_test_peer_t *peer, const unsigned char *keysalt, int keep_roc)
{
	srtp_policy_t policy;
	srtp_master_key_t mki, *mkis[1];
	unsigned char mki_id[sizeof(srtp_test_mki_id)];

	if (peer->ctx) {
		srtp_dealloc(peer->ctx);
		peer->ctx = NULL;
	}

	memset(&policy, 0, sizeof(policy));
	if (peer->suite == SRTP_TEST_AEAD_AES_128_GCM) {
		srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtp);
		srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtcp);
	} else {
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtp);
		srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);
	}
	policy.ssrc.type = ssrc_specific;
	policy.ssrc.value = peer->ssrc;
	policy.window_size = 1024;

	if (peer->suite == SRTP_TEST_AES_CM_128_HMAC_SHA1_80_MKI) {
		memcpy(mki_id, &srtp_test_mki_id, sizeof(mki_id));
		mki.key = (unsigned char *) keysalt;
		mki.mki_id = mki_id;
		mki.mki_size = sizeof(mki_id);
		mkis[0] = &mki;
		policy.keys = mkis;
		policy.num_master_keys = 1;
	} else {
		policy.key = (unsigned char *) keysalt;
	}

	if (srtp_create(&peer->ctx, &policy) != srtp_err_status_ok) {
		peer->ctx = NULL;
		return 0;
	}

	if (!keep_roc) {
		peer->roc = 0;
		return 1;
	}

	return srtp_set_stream_roc(peer->ctx, peer->ssrc, peer->roc) == srtp_err_status_ok;
}

static void srtp_test_payload(uint16_t seq, unsigned char *payload)
{
	int i;

	for (i = 0; i < SRTP_TEST_PAYLOAD_LEN; i++) {
		payload[i] = (unsigned char) (seq * 7 + i);
	}
}

/* Advance the peer's sequence number without sending, as if the packet were lost. */
static void srtp_test_peer_skip(srtp_test_peer_t *peer)
{
	if (++peer->seq == 0) {
		peer->roc++;
	}
	peer->ts += SRTP_TEST_PAYLOAD_LEN;
}

/* Protect and send the peer's next packet. */
static int srtp_test_peer_send(srtp_test_peer_t *peer)
{
	uint32_t buf[(12 + SRTP_TEST_PAYLOAD_LEN + SRTP_MAX_TRAILER_LEN) / 4 + 1];
	unsigned char *pkt = (unsigned char *) buf;
	int len = 12 + SRTP_TEST_PAYLOAD_LEN, i;
	srtp_err_status_t stat;

	pkt[0] = 0x80;
	pkt[1] = TEST_PT;
	pkt[2] = peer->seq >> 8;
	pkt[3] = peer->seq & 0xff;
	for (i = 0; i < 4; i++) {
		pkt[4 + i] = (peer->ts >> (24 - 8 * i)) & 0xff;
		pkt[8 + i] = (peer->ssrc >> (24 - 8 * i)) & 0xff;
	}
	srtp_test_payload(peer->seq, pkt + 12);

	if (peer->suite == SRTP_TEST_AES_CM_128_HMAC_SHA1_80_MKI) {
		stat = srtp_protect_mki(peer->ctx, pkt, &len, 1, 0);
	} else {
		stat = srtp_protect(peer->ctx, pkt, &len);
	}

	srtp_test_peer_skip(peer);

	return stat == srtp_err_status_ok &&
		sendto(peer->fd, pkt, len, 0, (struct sockaddr *) &peer->fs_addr, sizeof(peer->fs_addr)) == len;
}

/* Send the peer's next packet and read without blocking; returns 1 if FreeSWITCH delivered its plaintext. */
static int srtp_test_send_and_read(switch_rtp_t *rtp, srtp_test_peer_t *peer)
{
	char buf[SWITCH_RECOMMENDED_BUFFER_SIZE];
	unsigned char expect[SRTP_TEST_PAYLOAD_LEN];
	int tries;

	srtp_test_payload(peer->seq, expect);

	if (!srtp_test_peer_send(peer)) {
		return 0;
	}

	for (tries = 0; tries < 3; tries++) {
		uint32_t len = sizeof(buf);
		switch_payload_t pt = 0;
		switch_frame_flag_t frame_flags = 0;

		switch_rtp_read(rtp, (void *) buf, &len, &pt, &frame_flags, SWITCH_IO_FLAG_NOBLOCK);

		if (pt == TEST_PT && len == SRTP_TEST_PAYLOAD_LEN && !memcmp(buf, expect, len)) {
			return 1;
		}
	}

	return 0;
}

/*
 * Feed FreeSWITCH SRTP from one SSRC across a sequence number wrap, re-key its receive side the way an
 * SDES re-INVITE does (same SSRC, sequence carries on), then have the peer carry on under the new key.
 */
static srtp_rekey_result_t run_srtp_rekey_case(srtp_test_suite_t suite, srtp_rekey_case_t rekey_case)
{
	srtp_rekey_result_t result = { 0, 0, -1 };
	unsigned char key_a[SRTP_TEST_KEYSALT_LEN], key_b[SRTP_TEST_KEYSALT_LEN], key_c[SRTP_TEST_KEYSALT_LEN];
	switch_core_session_t *session = NULL;
	switch_rtp_t *srtp_rtp = NULL;
	switch_rtp_flag_t srtp_flags[SWITCH_RTP_FLAG_INVALID] = { 0 };
	switch_call_cause_t cause;
	srtp_test_peer_t peer = { 0 }, other = { 0 };
	struct sockaddr_in local = { 0 };
	socklen_t local_len = sizeof(local);
	switch_port_t fs_port;
	const char *srtp_err = NULL;
	int i;

	peer.fd = -1;
	peer.suite = other.suite = suite;
	srtp_test_keysalt(key_a, 0x11);
	srtp_test_keysalt(key_b, 0xa0);
	srtp_test_keysalt(key_c, 0x51);

	/* As in run_client_cert_verify_case(): FreeSWITCH binds the configured RTP port. */
	fs_port = switch_rtp_set_start_port(0);

	if ((peer.fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		goto done;
	}
	local.sin_family = AF_INET;
	local.sin_addr.s_addr = inet_addr(rx_host);
	if (bind(peer.fd, (struct sockaddr *) &local, sizeof(local)) < 0 ||
		getsockname(peer.fd, (struct sockaddr *) &local, &local_len) < 0) {
		goto done;
	}
	peer.fs_addr = local;
	peer.fs_addr.sin_port = htons(fs_port);

	if (switch_ivr_originate(NULL, &session, &cause, "null/+15553334444", 2, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL) != SWITCH_STATUS_SUCCESS || !session) {
		goto done;
	}

	srtp_rtp = switch_rtp_new(rx_host, fs_port, rx_host, ntohs(local.sin_port), TEST_PT, 8000, 20 * 1000, srtp_flags, NULL, &srtp_err,
							  switch_core_session_get_pool(session), 0, 0);
	if (!srtp_rtp || !switch_rtp_ready(srtp_rtp)) {
		goto done;
	}

	switch_core_media_set_rtp_session(session, SWITCH_MEDIA_TYPE_AUDIO, srtp_rtp);

	peer.ssrc = 0x5c704c80;
	peer.ts = 0x10000000;
	/* wrap halfway through the counted packets, so the ROC is 1 at the re-key; or stop short of the wrap
	 * and wrap in the gap */
	peer.seq = (uint16_t) (rekey_case == SRTP_REKEY_WRAP_IN_GAP ? 65536 - SRTP_TEST_WARMUP - SRTP_TEST_PACKETS - 50 :
						   65536 - SRTP_TEST_WARMUP - SRTP_TEST_PACKETS / 2);
	if (srtp_test_fs_key(srtp_rtp, suite, key_a) != SWITCH_STATUS_SUCCESS || !srtp_test_peer_key(&peer, key_a, 0)) {
		goto done;
	}

	/* Not counted: FreeSWITCH answers the first packet of the stream with a CNG frame, and later drops the
	 * packet that carries the timestamp it gave that frame ("we already sent this frame" in
	 * read_rtp_packet()), the 51st here. Neither involves SRTP. */
	for (i = 0; i < SRTP_TEST_WARMUP; i++) {
		srtp_test_send_and_read(srtp_rtp, &peer);
	}

	for (i = 0; i < SRTP_TEST_PACKETS; i++) {
		result.before += srtp_test_send_and_read(srtp_rtp, &peer);
	}

	if (srtp_test_fs_key(srtp_rtp, suite, key_b) != SWITCH_STATUS_SUCCESS) {
		goto done;
	}

	switch (rekey_case) {
	case SRTP_REKEY_OTHER_SSRC_THEN_RESET:
		/* a late packet under the old key is the first one after the re-key */
		srtp_test_send_and_read(srtp_rtp, &peer);
		/* fall through */
	case SRTP_REKEY_OTHER_SSRC_FIRST:
		other.fd = peer.fd;
		other.fs_addr = peer.fs_addr;
		other.ssrc = peer.ssrc + 1;
		other.seq = 1000;
		other.ts = 0x20000000;
		if (!srtp_test_peer_key(&other, key_b, 0)) {
			goto done;
		}
		for (i = 0; i < SRTP_TEST_PACKETS; i++) {
			result.other += srtp_test_send_and_read(srtp_rtp, &other);
		}
		break;
	case SRTP_REKEY_TWICE:
		/* a late packet under the old key starts the first re-key; the second comes before any new-key packet */
		srtp_test_send_and_read(srtp_rtp, &peer);
		memcpy(key_b, key_c, sizeof(key_b));
		if (srtp_test_fs_key(srtp_rtp, suite, key_b) != SWITCH_STATUS_SUCCESS) {
			goto done;
		}
		break;
	case SRTP_REKEY_WRAP_IN_GAP:
		for (i = 0; i < 150; i++) {
			srtp_test_peer_skip(&peer);
		}
		break;
	default:
		break;
	}

	if (!srtp_test_peer_key(&peer, key_b, rekey_case != SRTP_REKEY_PEER_RESETS_ROC && rekey_case != SRTP_REKEY_OTHER_SSRC_THEN_RESET)) {
		goto done;
	}

	result.after = 0;
	for (i = 0; i < SRTP_TEST_PACKETS; i++) {
		result.after += srtp_test_send_and_read(srtp_rtp, &peer);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "SRTP re-key suite %d case %d: decrypted %d before, %d other SSRC, %d after\n",
					  suite, rekey_case, result.before, result.other, result.after);

 done:

	if (peer.ctx) {
		srtp_dealloc(peer.ctx);
	}

	if (other.ctx) {
		srtp_dealloc(other.ctx);
	}

	if (peer.fd >= 0) {
		close(peer.fd);
	}

	if (srtp_rtp) {
		switch_rtp_destroy(&srtp_rtp);
	}

	if (session) {
		switch_channel_hangup(switch_core_session_get_channel(session), SWITCH_CAUSE_NORMAL_CLEARING);
		switch_core_session_rwunlock(session);
	}

	return result;
}

FST_CORE_BEGIN("./conf")
{
FST_SUITE_BEGIN(switch_rtp)
{
FST_SETUP_BEGIN()
{
	fst_requires_module("mod_loopback");
}
FST_SETUP_END()

FST_TEARDOWN_BEGIN()
{
}
FST_TEARDOWN_END()
	FST_TEST_BEGIN(test_rtp)
	{
		switch_rtp_stats_t *stats;
		switch_core_new_memory_pool(&pool);
		
		rtp_session = switch_rtp_new(rx_host, rx_port, tx_host, tx_port, TEST_PT, 8000, 20 * 1000, flags, "soft", &err, pool, 0, 0);
		fst_xcheck(rtp_session != NULL, "get RTP session");
		fst_requires(rtp_session);
		fst_requires(switch_rtp_ready(rtp_session));
		switch_rtp_activate_rtcp(rtp_session, 5, rx_port + 1, 0);
		switch_rtp_set_default_payload(rtp_session, TEST_PT);
		fst_xcheck(switch_rtp_get_default_payload(rtp_session) == TEST_PT, "get Payload Type");
		switch_rtp_set_ssrc(rtp_session, 0xabcd);
		switch_rtp_set_remote_ssrc(rtp_session, 0xcdef);
		fst_xcheck(switch_rtp_get_ssrc(rtp_session) == 0xabcd, "get SSRC");
		stats = switch_rtp_get_stats(rtp_session, pool);
		fst_requires(stats);
		switch_rtp_destroy(&rtp_session);

		switch_core_destroy_memory_pool(&pool);
	}
	FST_TEST_END()

	FST_TEST_BEGIN(test_session_with_rtp)
	{
		switch_core_session_t *session = NULL;
		switch_channel_t *channel = NULL;
		switch_status_t status;
		switch_call_cause_t cause;

		switch_core_new_memory_pool(&pool);

		status = switch_ivr_originate(NULL, &session, &cause, "null/+15553334444", 2, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL);
		fst_requires(session);
		fst_check(status == SWITCH_STATUS_SUCCESS);

		channel = switch_core_session_get_channel(session);
		fst_requires(channel);

		switch_core_memory_pool_set_data(pool, "__session", session);
		session = switch_core_memory_pool_get_data(pool, "__session");
		fst_requires(session);
		rtp_session = switch_rtp_new(rx_host, rx_port, tx_host, tx_port, TEST_PT, 8000, 20 * 1000, flags, "soft", &err, pool, 0, 0);
		fst_xcheck(rtp_session != NULL, "switch_rtp_new()");
		fst_requires(switch_rtp_ready(rtp_session));
		switch_rtp_activate_rtcp(rtp_session, 5, rx_port + 1, 0);
		switch_rtp_set_default_payload(rtp_session, TEST_PT);
		switch_core_media_set_rtp_session(session, SWITCH_MEDIA_TYPE_AUDIO, rtp_session);
		channel = switch_core_session_get_channel(session);
		fst_requires(channel);
		session = switch_rtp_get_core_session(rtp_session);
		fst_requires(session);
		status = switch_rtp_activate_jitter_buffer(rtp_session, 1, 10, 80, 8000);
		fst_xcheck(status == SWITCH_STATUS_SUCCESS, "switch_rtp_activate_jitter_buffer()");
		status = switch_rtp_debug_jitter_buffer(rtp_session, "debug");
		fst_xcheck(status == SWITCH_STATUS_SUCCESS, "switch_rtp_debug_jitter_buffer()");
		fst_requires(switch_rtp_get_jitter_buffer(rtp_session));
		status = switch_rtp_pause_jitter_buffer(rtp_session, SWITCH_TRUE);
		fst_xcheck(status == SWITCH_STATUS_SUCCESS, "switch_rtp_pause_jitter_buffer()");
		status = switch_rtp_deactivate_jitter_buffer(rtp_session);
		fst_xcheck(status == SWITCH_STATUS_SUCCESS, "switch_rtp_deactivate_jitter_buffer()");

		switch_rtp_destroy(&rtp_session);
		switch_core_session_rwunlock(session);
		switch_core_destroy_memory_pool(&pool);
	}
	FST_TEST_END()
	FST_TEST_BEGIN(test_send_rtcp_event_audio)
	{
		switch_core_session_t *session = NULL;
		switch_channel_t *channel = NULL;
		switch_status_t status;
		switch_call_cause_t cause;
		switch_stream_handle_t stream = { 0 };
		const unsigned char packet[]="\x80\x00\xcd\x15\xfd\x86\x00\x00\x61\x5a\xe1\x37";
		uint32_t plen = 12;
		char rpacket[SWITCH_RECOMMENDED_BUFFER_SIZE];
		switch_payload_t pt = { 0 };
		switch_frame_flag_t frameflags = { 0 };
		static switch_port_t audio_rx_port = 1234;
		switch_media_handle_t *media_handle;
		switch_core_media_params_t *mparams;
		char *r_sdp;
		uint8_t match = 0, p = 0;
		struct sockaddr_in sin;
		socklen_t len = sizeof(sin);
		int x;
		struct sockaddr_in servaddr_rtp; 
		int sockfd_rtp;
		struct hostent *server;
		int ret;
		switch_frame_t *read_frame, *write_frame;

		switch_event_bind("", SWITCH_EVENT_ALL, SWITCH_EVENT_SUBCLASS_ANY, send_rtcp_event_handler, NULL);

		status = switch_ivr_originate(NULL, &session, &cause, "null/+15553334444", 2, NULL, NULL, NULL, NULL, NULL, SOF_NONE, NULL, NULL);
		fst_requires(session);
		fst_check(status == SWITCH_STATUS_SUCCESS);

		channel = switch_core_session_get_channel(session);
		fst_requires(channel);
		mparams  = switch_core_session_alloc(session, sizeof(switch_core_media_params_t));
		mparams->num_codecs = 1;
		mparams->inbound_codec_string = switch_core_session_strdup(session, "PCMU");
		mparams->outbound_codec_string = switch_core_session_strdup(session, "PCMU");
		mparams->rtpip = switch_core_session_strdup(session, (char *)rx_host);

		status = switch_media_handle_create(&media_handle, session, mparams);
		fst_requires(status == SWITCH_STATUS_SUCCESS);

		switch_channel_set_variable(channel, "absolute_codec_string", "PCMU");
		switch_channel_set_variable(channel, "fire_rtcp_events", "true");
		switch_channel_set_variable(channel, "send_silence_when_idle", "-1");

		switch_channel_set_variable(channel, SWITCH_LOCAL_MEDIA_IP_VARIABLE, rx_host);
		switch_channel_set_variable_printf(channel, SWITCH_LOCAL_MEDIA_PORT_VARIABLE, "%d", audio_rx_port);

		r_sdp = switch_core_session_sprintf(session,
		"v=0\n"
		"o=FreeSWITCH 1632033305 1632033306 IN IP4 %s\n"
		"s=-\n"
		"c=IN IP4 %s\n"
		"t=0 0\n"
		"m=audio 11114 RTP/AVP 0 101\n"
		"a=rtpmap:0 PCMU/8000\n"
		"a=rtpmap:101 telephone-event/8000\n"
		"a=rtcp:11115\n",
		tx_host, tx_host);
		 
		switch_core_media_prepare_codecs(session, SWITCH_FALSE);
		   
		match = switch_core_media_negotiate_sdp(session, r_sdp, &p, SDP_OFFER);
		fst_requires(match == 1);

		status = switch_core_media_choose_ports(session, SWITCH_TRUE, SWITCH_FALSE);
		fst_requires(status == SWITCH_STATUS_SUCCESS);

		status = switch_core_media_activate_rtp(session);
		fst_requires(status == SWITCH_STATUS_SUCCESS);

		switch_core_media_set_rtp_flag(session, SWITCH_MEDIA_TYPE_AUDIO, SWITCH_RTP_FLAG_DEBUG_RTP_READ);
		switch_core_media_set_rtp_flag(session, SWITCH_MEDIA_TYPE_AUDIO, SWITCH_RTP_FLAG_DEBUG_RTP_WRITE);
		switch_core_media_set_rtp_flag(session, SWITCH_MEDIA_TYPE_AUDIO, SWITCH_RTP_FLAG_AUDIO_FIRE_SEND_RTCP_EVENT);
		switch_core_media_set_rtp_flag(session, SWITCH_MEDIA_TYPE_AUDIO, SWITCH_RTP_FLAG_ENABLE_RTCP);


		switch_frame_alloc(&write_frame, SWITCH_RECOMMENDED_BUFFER_SIZE);
		write_frame->codec = switch_core_session_get_write_codec(session);

		SWITCH_STANDARD_STREAM(stream);
		switch_api_execute("fsctl", "debug_level 9", session, &stream);
		switch_safe_free(stream.data);

		if ((sockfd_rtp = socket(AF_INET, SOCK_DGRAM, 0)) < 0) { 
			perror("socket creation failed"); 
			fst_requires(0); /*exit*/ 
		}

		memset(&servaddr_rtp, 0, sizeof(servaddr_rtp)); 
		                                    
		servaddr_rtp.sin_family = AF_INET; 
		servaddr_rtp.sin_port = htons(audio_rx_port); 
		server = gethostbyname(rx_host);
		bcopy((char *)server->h_addr, (char *)&servaddr_rtp.sin_addr.s_addr, server->h_length);

		/*get local UDP port (tx side) to trick FS into accepting our packets*/
		ret = sendto(sockfd_rtp, NULL, 0, MSG_CONFIRM, (const struct sockaddr *) &servaddr_rtp, sizeof(servaddr_rtp)); 
		if (ret < 0){
			perror("sendto");
			fst_requires(0);
		}

		rtp_session = switch_core_media_get_rtp_session(session, SWITCH_MEDIA_TYPE_AUDIO);
		len = sizeof(sin);
		if (getsockname(sockfd_rtp, (struct sockaddr *)&sin, &len) == -1) {
			perror("getsockname");
			fst_requires(0);
		} else {
			switch_rtp_set_remote_address(rtp_session, tx_host, ntohs(sin.sin_port), 0, SWITCH_FALSE, &err);
			switch_rtp_reset(rtp_session);
		}

		write_frame->datalen = plen;
		memcpy(write_frame->data, &packet, plen);

		switch_rtp_clear_flag(rtp_session, SWITCH_RTP_FLAG_PAUSE);

		for (x = 0; x < 3; x++) {

			switch_rtp_write_frame(rtp_session, write_frame);  /* rtp_session->stats.rtcp.sent_pkt_count++; */

			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Sent RTP. Packet size = [%u]\n", plen);
			ret = sendto(sockfd_rtp, (const char *) &packet, plen, MSG_CONFIRM, (const struct sockaddr *) &servaddr_rtp, sizeof(servaddr_rtp));
			if (ret < 0){
				perror("sendto");
				fst_requires(0);
			}

			status = switch_rtp_read(rtp_session, (void *)&rpacket, &plen, &pt, &frameflags, io_flags);
			fst_requires(status == SWITCH_STATUS_SUCCESS);
			plen = 12;
			if (pt == SWITCH_RTP_CNG_PAYLOAD /*timeout*/) continue;

			status = switch_core_session_read_frame(session, &read_frame, frameflags, 0);
			fst_requires(status == SWITCH_STATUS_SUCCESS);
		}
		switch_sleep(3000 * 1000);
		
		fst_requires(send_rtcp_test_success);
		switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);

		if (write_frame) switch_frame_free(&write_frame);

		switch_rtp_destroy(&rtp_session);

		switch_media_handle_destroy(session);

		switch_core_session_rwunlock(session);
	}
	FST_TEST_END()

	FST_TEST_BEGIN(test_client_cert_verify)
	{
		dtls_state_t state;

		/* fingerprint mode, matching client cert -> handshake completes, SRTP keys installed. */
		state = run_client_cert_verify_case("fingerprint", 1, REMOTE_FP_MATCH);
		fst_xcheck(state == DS_READY, "fingerprint mode: matching client fingerprint reaches DS_READY");

		/* fingerprint mode, client cert does not match the expected fingerprint -> rejected. */
		state = run_client_cert_verify_case("fingerprint", 1, REMOTE_FP_DIFFERENT);
		fst_xcheck(state == DS_FAIL, "fingerprint mode: mismatched client fingerprint reaches DS_FAIL");

		/* fingerprint mode, client presents no certificate -> cannot be verified -> rejected. */
		state = run_client_cert_verify_case("fingerprint", 0, REMOTE_FP_MATCH);
		fst_xcheck(state == DS_FAIL, "fingerprint mode: absent client certificate reaches DS_FAIL");

		/* fingerprint mode, client presents a cert but the peer advertised no SDP a=fingerprint ->
		 * nothing to bind the certificate to -> rejected (must not deref a NULL fingerprint type). */
		state = run_client_cert_verify_case("fingerprint", 1, REMOTE_FP_ABSENT);
		fst_xcheck(state == DS_FAIL, "fingerprint mode: absent remote fingerprint reaches DS_FAIL");

		/* none (default): client cert is neither requested nor checked, so even a mismatch is accepted. */
		state = run_client_cert_verify_case("none", 1, REMOTE_FP_DIFFERENT);
		fst_xcheck(state == DS_READY, "none mode accepts an unverified client");

		/* An unrecognized mode falls back to fingerprint (fail closed), so a mismatch is rejected
		 * rather than silently accepted the way none would. */
		state = run_client_cert_verify_case("bogus", 1, REMOTE_FP_DIFFERENT);
		fst_xcheck(state == DS_FAIL, "unrecognized mode falls back to fingerprint and rejects a mismatch");
	}
	FST_TEST_END()

	FST_TEST_BEGIN(test_srtp_rekey_after_rollover)
	{
		static const struct {
			srtp_test_suite_t suite;
			srtp_rekey_case_t rekey_case;
			const char *name;
		} cases[] = {
			{ SRTP_TEST_AES_CM_128_HMAC_SHA1_80, SRTP_REKEY_PEER_KEEPS_ROC, "AES-CM: peer keeps its ROC" },
			{ SRTP_TEST_AES_CM_128_HMAC_SHA1_80, SRTP_REKEY_PEER_RESETS_ROC, "AES-CM: peer resets its ROC" },
			{ SRTP_TEST_AES_CM_128_HMAC_SHA1_80, SRTP_REKEY_OTHER_SSRC_FIRST, "AES-CM: another SSRC first, peer keeps" },
			{ SRTP_TEST_AES_CM_128_HMAC_SHA1_80, SRTP_REKEY_OTHER_SSRC_THEN_RESET, "AES-CM: late packet and another SSRC, peer resets" },
			{ SRTP_TEST_AES_CM_128_HMAC_SHA1_80, SRTP_REKEY_TWICE, "AES-CM: second re-key before a decision, peer keeps" },
			{ SRTP_TEST_AES_CM_128_HMAC_SHA1_80, SRTP_REKEY_WRAP_IN_GAP, "AES-CM: wrap while packets are lost, peer keeps" },
			{ SRTP_TEST_AES_CM_128_HMAC_SHA1_80_MKI, SRTP_REKEY_PEER_KEEPS_ROC, "AES-CM with MKI: peer keeps its ROC" },
			{ SRTP_TEST_AES_CM_128_HMAC_SHA1_80_MKI, SRTP_REKEY_PEER_RESETS_ROC, "AES-CM with MKI: peer resets its ROC" },
			{ SRTP_TEST_AEAD_AES_128_GCM, SRTP_REKEY_PEER_RESETS_ROC, "AES-GCM: peer resets its ROC" },
			{ SRTP_TEST_AEAD_AES_128_GCM, SRTP_REKEY_OTHER_SSRC_THEN_RESET, "AES-GCM: late packet and another SSRC, peer resets" }
		};
		srtp_rekey_result_t result;
		size_t i;

		for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
			int has_other = cases[i].rekey_case == SRTP_REKEY_OTHER_SSRC_FIRST || cases[i].rekey_case == SRTP_REKEY_OTHER_SSRC_THEN_RESET;

			result = run_srtp_rekey_case(cases[i].suite, cases[i].rekey_case);
			fst_xcheck(result.before == SRTP_TEST_PACKETS, switch_core_sprintf(fst_pool, "%s: every packet before the re-key decrypts", cases[i].name));
			if (has_other) {
				fst_xcheck(result.other == SRTP_TEST_PACKETS, switch_core_sprintf(fst_pool, "%s: every packet from the other SSRC decrypts", cases[i].name));
			}
			fst_xcheck(result.after == SRTP_TEST_PACKETS, switch_core_sprintf(fst_pool, "%s: every packet after the re-key decrypts", cases[i].name));
		}
	}
	FST_TEST_END()

}
FST_SUITE_END()
}
FST_CORE_END()


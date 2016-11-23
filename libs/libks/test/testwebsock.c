#include <ks.h>
#include <tap.h>


static char v4[48] = "";
static char v6[48] = "";
static int mask = 0;
static int tcp_port = 8090;


static char __MSG[] = "TESTING................................................................................/TESTING";


typedef struct ssl_profile_s {
	const SSL_METHOD *ssl_method;
	SSL_CTX *ssl_ctx;
	char cert[512];
	char key[512];
	char chain[512];
} ssl_profile_t;

static int init_ssl(ssl_profile_t *profile) 
{
	const char *err = "";

	profile->ssl_ctx = SSL_CTX_new(profile->ssl_method);         /* create context */
	assert(profile->ssl_ctx);

	/* Disable SSLv2 */
	SSL_CTX_set_options(profile->ssl_ctx, SSL_OP_NO_SSLv2);
	/* Disable SSLv3 */
	SSL_CTX_set_options(profile->ssl_ctx, SSL_OP_NO_SSLv3);
	/* Disable TLSv1 */
	SSL_CTX_set_options(profile->ssl_ctx, SSL_OP_NO_TLSv1);
	/* Disable Compression CRIME (Compression Ratio Info-leak Made Easy) */
	SSL_CTX_set_options(profile->ssl_ctx, SSL_OP_NO_COMPRESSION);

	/* set the local certificate from CertFile */
	if (!zstr(profile->chain)) {
		if (!SSL_CTX_use_certificate_chain_file(profile->ssl_ctx, profile->chain)) {
			err = "CERT CHAIN FILE ERROR";
			goto fail;
		}
	}

	if (!SSL_CTX_use_certificate_file(profile->ssl_ctx, profile->cert, SSL_FILETYPE_PEM)) {
		err = "CERT FILE ERROR";
		goto fail;
	}

	/* set the private key from KeyFile */

	if (!SSL_CTX_use_PrivateKey_file(profile->ssl_ctx, profile->key, SSL_FILETYPE_PEM)) {
		err = "PRIVATE KEY FILE ERROR";
		goto fail;
	}

	/* verify private key */
	if ( !SSL_CTX_check_private_key(profile->ssl_ctx) ) {
		err = "PRIVATE KEY FILE ERROR";
		goto fail;
	}

	SSL_CTX_set_cipher_list(profile->ssl_ctx, "HIGH:!DSS:!aNULL@STRENGTH");

	return 1;

 fail:
	ks_log(KS_LOG_ERROR, "SSL ERR: %s\n", err);

	return 0;

}

struct tcp_data {
	ks_socket_t sock;
	ks_sockaddr_t addr;
	int ready;
	char *ip;
	ks_pool_t *pool;
	int ssl;
	ssl_profile_t client_profile;
	ssl_profile_t server_profile;
};

void server_callback(ks_socket_t server_sock, ks_socket_t client_sock, ks_sockaddr_t *addr, void *user_data)
{
	struct tcp_data *tcp_data = (struct tcp_data *) user_data;
	ks_size_t bytes;
	kws_t *kws = NULL;
	kws_opcode_t oc;
	uint8_t *data;


	if (tcp_data->ssl) {
		tcp_data->server_profile.ssl_method = SSLv23_server_method();
		ks_set_string(tcp_data->server_profile.cert, "/tmp/testwebsock.pem");
		ks_set_string(tcp_data->server_profile.key, "/tmp/testwebsock.pem");
		ks_set_string(tcp_data->server_profile.chain, "/tmp/testwebsock.pem");
		init_ssl(&tcp_data->server_profile);
	}

	printf("WS %s SERVER SOCK %d connection from %s:%u\n", tcp_data->ssl ? "SSL" : "PLAIN", (int)server_sock, addr->host, addr->port);
	
	if (kws_init(&kws, client_sock, tcp_data->server_profile.ssl_ctx, NULL, KWS_BLOCK, tcp_data->pool) != KS_STATUS_SUCCESS) {
		printf("WS SERVER CREATE FAIL\n");
		goto end;
	}
	
	do {

		bytes = kws_read_frame(kws, &oc, &data);

		if (bytes <= 0) {
			printf("WS SERVER BAIL %s\n", strerror(ks_errno()));
			break;
		}
		printf("WS SERVER READ %ld bytes [%s]\n", (long)bytes, (char *)data);
	} while(zstr_buf((char *)data) || strcmp((char *)data, __MSG));

	bytes = kws_write_frame(kws, WSOC_TEXT, (char *)data, strlen((char *)data));
	
	printf("WS SERVER WRITE %ld bytes\n", (long)bytes);

 end:

	ks_socket_close(&client_sock);

	kws_destroy(&kws);

	if (tcp_data->ssl) {
		SSL_CTX_free(tcp_data->server_profile.ssl_ctx);
	}

	printf("WS SERVER COMPLETE\n");
}



static void *tcp_sock_server(ks_thread_t *thread, void *thread_data)
{
	struct tcp_data *tcp_data = (struct tcp_data *) thread_data;

	tcp_data->ready = 1;
	ks_listen_sock(tcp_data->sock, &tcp_data->addr, 0, server_callback, tcp_data);

	printf("WS THREAD DONE\n");

	return NULL;
}


static int test_ws(char *ip, int ssl)
{
	ks_thread_t *thread_p = NULL;
	ks_pool_t *pool;
	ks_sockaddr_t addr;
	int family = AF_INET;
	ks_socket_t cl_sock = KS_SOCK_INVALID;
	struct tcp_data tcp_data = { 0 };
	int r = 1, sanity = 100;
	kws_t *kws = NULL;

	ks_pool_open(&pool);

	tcp_data.pool = pool;

	if (ssl) {
		tcp_data.ssl = 1;
		tcp_data.client_profile.ssl_method = SSLv23_client_method();
		ks_set_string(tcp_data.client_profile.cert, "/tmp/testwebsock.pem");
		ks_set_string(tcp_data.client_profile.key, "/tmp/testwebsock.pem");
		ks_set_string(tcp_data.client_profile.chain, "/tmp/testwebsock.pem");
		init_ssl(&tcp_data.client_profile);
	}


	if (strchr(ip, ':')) {
		family = AF_INET6;
	}

	if (ks_addr_set(&tcp_data.addr, ip, tcp_port, family) != KS_STATUS_SUCCESS) {
		r = 0;
		printf("WS CLIENT Can't set ADDR\n");
		goto end;
	}
	
	if ((tcp_data.sock = socket(family, SOCK_STREAM, IPPROTO_TCP)) == KS_SOCK_INVALID) {
		r = 0;
		printf("WS CLIENT Can't create sock family %d\n", family);
		goto end;
	}

	ks_socket_option(tcp_data.sock, SO_REUSEADDR, KS_TRUE);
	ks_socket_option(tcp_data.sock, TCP_NODELAY, KS_TRUE);

	tcp_data.ip = ip;

	ks_thread_create(&thread_p, tcp_sock_server, &tcp_data, pool);

	while(!tcp_data.ready && --sanity > 0) {
		ks_sleep(10000);
	}

	ks_addr_set(&addr, ip, tcp_port, family);
	cl_sock = ks_socket_connect(SOCK_STREAM, IPPROTO_TCP, &addr);
	
	printf("WS %s CLIENT SOCKET %d %s %d\n", ssl ? "SSL" : "PLAIN", (int)cl_sock, addr.host, addr.port);

	if (kws_init(&kws, cl_sock, tcp_data.client_profile.ssl_ctx, "/verto:tatooine.freeswitch.org:verto", KWS_BLOCK, pool) != KS_STATUS_SUCCESS) {
		printf("WS CLIENT CREATE FAIL\n");
		goto end;
	}

	kws_write_frame(kws, WSOC_TEXT, __MSG, strlen(__MSG));

	kws_opcode_t oc;
	uint8_t *data;
	ks_ssize_t bytes;

	bytes = kws_read_frame(kws, &oc, &data);
	printf("WS CLIENT READ %ld bytes [%s]\n", bytes, (char *)data);
	
 end:

	kws_destroy(&kws);

	if (ssl) {
		SSL_CTX_free(tcp_data.client_profile.ssl_ctx);
	}

	if (tcp_data.sock != KS_SOCK_INVALID) {
		ks_socket_shutdown(tcp_data.sock, 2);
		ks_socket_close(&tcp_data.sock);
	}

	if (thread_p) {
		ks_thread_join(thread_p);
	}

	ks_socket_close(&cl_sock);

	ks_pool_close(&pool);

	return r;
}



int main(void)
{
	int have_v4 = 0, have_v6 = 0;
	ks_find_local_ip(v4, sizeof(v4), &mask, AF_INET, NULL);
	ks_find_local_ip(v6, sizeof(v6), NULL, AF_INET6, NULL);
	ks_init();

	printf("IPS: v4: [%s] v6: [%s]\n", v4, v6);

	have_v4 = zstr_buf(v4) ? 0 : 1;
	have_v6 = zstr_buf(v6) ? 0 : 1;

	plan((have_v4 * 2) + (have_v6 * 2) + 1);

	ok(have_v4 || have_v6);

	if (have_v4 || have_v6) {
		ks_gen_cert("/tmp", "testwebsock.pem");
	}

	if (have_v4) {
		ok(test_ws(v4, 0));
		ok(test_ws(v4, 1));
	}

	if (have_v6) {
		ok(test_ws(v6, 0));
		ok(test_ws(v6, 1));
	}

	unlink("/tmp/testwebsock.pem");
	ks_shutdown();

	done_testing();
}

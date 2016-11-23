#include <ks.h>
#include <tap.h>

static char v4[48] = "";
static char v6[48] = "";
static int mask = 0;
static int tcp_port = 8090;
static int udp_cl_port = 9090;
static int udp_sv_port = 9091;

static char __MSG[] = "TESTING................................................................................/TESTING";

struct tcp_data {
	ks_socket_t sock;
	ks_sockaddr_t addr;
	int ready;
	char *ip;
};

void server_callback(ks_socket_t server_sock, ks_socket_t client_sock, ks_sockaddr_t *addr, void *user_data)
{
	//struct tcp_data *tcp_data = (struct tcp_data *) user_data;
	char buf[8192] = "";
	ks_status_t status;
	ks_size_t bytes;

	printf("TCP SERVER SOCK %d connection from %s:%u\n", (int)server_sock, addr->host, addr->port);

	do {
		bytes = sizeof(buf);;
		status = ks_socket_recv(client_sock, buf, &bytes);
		if (status != KS_STATUS_SUCCESS) {
			printf("TCP SERVER BAIL %s\n", strerror(ks_errno()));
			break;
		}
		printf("TCP SERVER READ %ld bytes [%s]\n", (long)bytes, buf);
	} while(zstr_buf(buf) || strcmp(buf, __MSG));

	bytes = strlen(buf);
	status = ks_socket_send(client_sock, buf, &bytes);
	printf("TCP SERVER WRITE %ld bytes\n", (long)bytes);

	ks_socket_close(&client_sock);

	printf("TCP SERVER COMPLETE\n");
}



static void *tcp_sock_server(ks_thread_t *thread, void *thread_data)
{
	struct tcp_data *tcp_data = (struct tcp_data *) thread_data;

	tcp_data->ready = 1;
	ks_listen_sock(tcp_data->sock, &tcp_data->addr, 0, server_callback, tcp_data);

	printf("TCP THREAD DONE\n");

	return NULL;
}

static int test_addr(int v)
{
	ks_sockaddr_t addr1, addr2, addr3, addr4, addr5;

	printf("TESTING ADDR v%d\n", v);

	if (v == 4) {
		if (ks_addr_set(&addr1, "10.100.200.5", 2467, AF_INET) != KS_STATUS_SUCCESS) {
			return 0;
		}

		if (strcmp(addr1.host, "10.100.200.5")) {
			return 0;
		}

		if (ks_addr_set(&addr2, "10.100.200.5", 2467, AF_INET) != KS_STATUS_SUCCESS) {
			return 0;
		}

		if (ks_addr_set(&addr3, "10.100.200.5", 1234, AF_INET) != KS_STATUS_SUCCESS) {
			return 0;
		}

		if (ks_addr_set(&addr4, "10.199.200.5", 2467, AF_INET) != KS_STATUS_SUCCESS) {
			return 0;
		}

	} else {
		if (ks_addr_set(&addr1, "1607:f418:1210::1", 2467, AF_INET6) != KS_STATUS_SUCCESS) {
			return 0;
		}

		if (strcmp(addr1.host, "1607:f418:1210::1")) {
			return 0;
		}

		if (ks_addr_set(&addr2, "1607:f418:1210::1", 2467, AF_INET6) != KS_STATUS_SUCCESS) {
			return 0;
		}

		if (ks_addr_set(&addr3, "1607:f418:1210::1", 1234, AF_INET6) != KS_STATUS_SUCCESS) {
			return 0;
		}

		if (ks_addr_set(&addr4, "1337:a118:1306::1", 2467, AF_INET6) != KS_STATUS_SUCCESS) {
			return 0;
		}
	}

	
	if (ks_addr_copy(&addr5, &addr4) != KS_STATUS_SUCCESS) {
		return 0;
	}

	if (!ks_addr_cmp(&addr1, &addr2)) {
		return 0;
	}

	if (ks_addr_cmp(&addr1, &addr3)) {
		return 0;
	}

	if (ks_addr_cmp(&addr1, &addr4)) {
		return 0;
	}

	if (!ks_addr_cmp(&addr4, &addr5)) {
		return 0;
	}

	if (ks_addr_cmp(&addr1, &addr5)) {
		return 0;
	}


	return 1;
}

static int test_tcp(char *ip)
{
	ks_thread_t *thread_p = NULL;
	ks_pool_t *pool;
	ks_sockaddr_t addr;
	int family = AF_INET;
	ks_socket_t cl_sock = KS_SOCK_INVALID;
	char buf[8192] = "";
	struct tcp_data tcp_data = { 0 };
	int r = 1, sanity = 100;

	ks_pool_open(&pool);

	if (strchr(ip, ':')) {
		family = AF_INET6;
	}

	if (ks_addr_set(&tcp_data.addr, ip, tcp_port, family) != KS_STATUS_SUCCESS) {
		r = 0;
		printf("TCP CLIENT Can't set ADDR\n");
		goto end;
	}
	
	if ((tcp_data.sock = socket(family, SOCK_STREAM, IPPROTO_TCP)) == KS_SOCK_INVALID) {
		r = 0;
		printf("TCP CLIENT Can't create sock family %d\n", family);
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
	
	int x;

	printf("TCP CLIENT SOCKET %d %s %d\n", (int)cl_sock, addr.host, addr.port);

	x = write((int)cl_sock, __MSG, (unsigned)strlen(__MSG));
	printf("TCP CLIENT WRITE %d bytes\n", x);
	
	x = read((int)cl_sock, buf, sizeof(buf));
	printf("TCP CLIENT READ %d bytes [%s]\n", x, buf);
	
 end:

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


struct udp_data {
	int ready;
	char *ip;
	ks_socket_t sv_sock;
};

static void *udp_sock_server(ks_thread_t *thread, void *thread_data)
{
	struct udp_data *udp_data = (struct udp_data *) thread_data;
	int family = AF_INET;
	ks_status_t status;
	ks_sockaddr_t addr, remote_addr = KS_SA_INIT;
	char buf[8192] = "";
	ks_size_t bytes;

	udp_data->sv_sock = KS_SOCK_INVALID;
	
	if (strchr(udp_data->ip, ':')) {
		family = AF_INET6;
	}
	
	ks_addr_set(&addr, udp_data->ip, udp_sv_port, family);
	remote_addr.family = family;

	if ((udp_data->sv_sock = socket(family, SOCK_DGRAM, IPPROTO_UDP)) == KS_SOCK_INVALID) {
		printf("UDP SERVER SOCKET ERROR %s\n", strerror(ks_errno()));
		goto end;
	}

	ks_socket_option(udp_data->sv_sock, SO_REUSEADDR, KS_TRUE);

	if (ks_addr_bind(udp_data->sv_sock, &addr) != KS_STATUS_SUCCESS) {
		printf("UDP SERVER BIND ERROR %s\n", strerror(ks_errno()));
		goto end;
	}

	udp_data->ready = 1;

	printf("UDP SERVER SOCKET %d %s %d\n", (int)(udp_data->sv_sock), addr.host, addr.port);
	bytes = sizeof(buf);
	if ((status = ks_socket_recvfrom(udp_data->sv_sock, buf, &bytes, &remote_addr)) != KS_STATUS_SUCCESS) {
		printf("UDP SERVER RECVFROM ERR %s\n", strerror(ks_errno()));
		goto end;
	}
	printf("UDP SERVER READ %ld bytes [%s]\n", (long)bytes, buf);

	if (strcmp(buf, __MSG)) {
		printf("INVALID MESSAGE\n");
		goto end;
	}

	printf("UDP SERVER WAIT 2 seconds to test nonblocking sockets\n");
	ks_sleep(2000000);
	printf("UDP SERVER RESPOND TO %d %s %d\n", (int)(udp_data->sv_sock), remote_addr.host, remote_addr.port);
	bytes = strlen(buf);
	if ((status = ks_socket_sendto(udp_data->sv_sock, buf, &bytes, &remote_addr)) != KS_STATUS_SUCCESS) {
		printf("UDP SERVER SENDTO ERR %s\n", strerror(ks_errno()));
		goto end;
	}
	printf("UDP SERVER WRITE %ld bytes [%s]\n", (long)bytes, buf);


 end:

	udp_data->ready = -1;
	printf("UDP THREAD DONE\n");

	ks_socket_close(&udp_data->sv_sock);

	return NULL;
}


static int test_udp(char *ip)
{
	ks_thread_t *thread_p = NULL;
	ks_pool_t *pool;
	ks_sockaddr_t addr, remote_addr;
	int family = AF_INET;
	ks_socket_t cl_sock = KS_SOCK_INVALID;
	char buf[8192] = "";
	int r = 1, sanity = 100;
	struct udp_data udp_data = { 0 };
	ks_size_t bytes = 0;
	ks_status_t status;
	
	ks_pool_open(&pool);

	if (strchr(ip, ':')) {
		family = AF_INET6;
	}

	ks_addr_set(&addr, ip, udp_cl_port, family);

	if ((cl_sock = socket(family, SOCK_DGRAM, IPPROTO_UDP)) == KS_SOCK_INVALID) {
		printf("UDP CLIENT SOCKET ERROR %s\n", strerror(ks_errno()));
		r = 0; goto end;
	}

	ks_socket_option(cl_sock, SO_REUSEADDR, KS_TRUE);

	if (ks_addr_bind(cl_sock, &addr) != KS_STATUS_SUCCESS) {
		printf("UDP CLIENT BIND ERROR %s\n", strerror(ks_errno()));
		r = 0; goto end;
	}

	ks_addr_set(&remote_addr, ip, udp_sv_port, family);

	udp_data.ip = ip;
	ks_thread_create(&thread_p, udp_sock_server, &udp_data, pool);

	while(!udp_data.ready && --sanity > 0) {
		ks_sleep(10000);
	}

	printf("UDP CLIENT SOCKET %d %s %d -> %s %d\n", (int)cl_sock, addr.host, addr.port, remote_addr.host, remote_addr.port);

	bytes = strlen(__MSG);
	if ((status = ks_socket_sendto(cl_sock, __MSG, &bytes, &remote_addr)) != KS_STATUS_SUCCESS) {
		printf("UDP CLIENT SENDTO ERR %s\n", strerror(ks_errno()));
		r = 0; goto end;
	}
	
	printf("UDP CLIENT WRITE %ld bytes\n", (long)bytes);
	ks_socket_option(cl_sock, KS_SO_NONBLOCK, KS_TRUE);

	sanity = 300;
	do {
		status = ks_socket_recvfrom(cl_sock, buf, &bytes, &remote_addr);

		if (status == KS_STATUS_BREAK && --sanity > 0) {
			if ((sanity % 50) == 0) printf("UDP CLIENT SLEEP NONBLOCKING\n");
			ks_sleep(10000);
		} else if (status != KS_STATUS_SUCCESS) {
			printf("UDP CLIENT RECVFROM ERR %s\n", strerror(ks_errno()));
			r = 0; goto end;
		}
	} while(status != KS_STATUS_SUCCESS);
	printf("UDP CLIENT READ %ld bytes\n", (long)bytes);
	
 end:

	if (thread_p) {
		ks_thread_join(thread_p);
	}

	if (udp_data.ready > 0 && udp_data.sv_sock && ks_socket_valid(udp_data.sv_sock)) {
		ks_socket_shutdown(udp_data.sv_sock, 2);
		ks_socket_close(&udp_data.sv_sock);
	}



	ks_socket_close(&cl_sock);

	ks_pool_close(&pool);

	return r;
}


int main(void)
{
	int have_v4 = 0, have_v6 = 0;

	ks_init();

	ks_find_local_ip(v4, sizeof(v4), &mask, AF_INET, NULL);
	ks_find_local_ip(v6, sizeof(v6), NULL, AF_INET6, NULL);
	
	printf("IPS: v4: [%s] v6: [%s]\n", v4, v6);

	have_v4 = zstr_buf(v4) ? 0 : 1;
	have_v6 = zstr_buf(v6) ? 0 : 1;

	plan((have_v4 * 3) + (have_v6 * 3) + 1);

	ok(have_v4 || have_v6);
	
	if (have_v4) {
		ok(test_tcp(v4));
		ok(test_udp(v4));
		ok(test_addr(4));
	}

	if (have_v6) {
		ok(test_tcp(v6));
		ok(test_udp(v6));
		ok(test_addr(6));
	}

	ks_shutdown();

	done_testing();
}

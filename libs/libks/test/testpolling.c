#include "ks.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "tap.h"

ks_socket_t start_listen(ks_sockaddr_t *addr)
{
	ks_socket_t listener = KS_SOCK_INVALID;
	ks_status_t ret = KS_STATUS_SUCCESS;

	ks_assert(addr);

	if ((listener = socket(addr->family, SOCK_STREAM, IPPROTO_TCP)) == KS_SOCK_INVALID) {
		ks_log(KS_LOG_DEBUG, "listener == KS_SOCK_INVALID\n");
		ret = KS_STATUS_FAIL;
		goto done;
	}

	ks_socket_option(listener, SO_REUSEADDR, KS_TRUE);
	ks_socket_option(listener, TCP_NODELAY, KS_TRUE);
	if (addr->family == AF_INET6) ks_socket_option(listener, IPV6_V6ONLY, KS_TRUE);

	if (ks_addr_bind(listener, addr) != KS_STATUS_SUCCESS) {
		ks_log(KS_LOG_DEBUG, "ks_addr_bind(listener, addr) != KS_STATUS_SUCCESS\n");
		ret = KS_STATUS_FAIL;
		goto done;
	}

	if (listen(listener, 4) != 0) {
		ks_log(KS_LOG_DEBUG, "listen(listener, backlog) != 0\n");
		ret = KS_STATUS_FAIL;
		goto done;
	}

done:
	if (ret != KS_STATUS_SUCCESS) {
		if (listener != KS_SOCK_INVALID) {
			ks_socket_shutdown(listener, SHUT_RDWR);
			ks_socket_close(&listener);
			listener = KS_SOCK_INVALID;
		}
	}
	return listener;
}

int main(int argc, char **argv)
{
	ks_pool_t *pool = NULL;
	struct pollfd *listeners_poll = NULL;
	int32_t listeners_count = 0;
	int32_t listener_index = -1;
	ks_sockaddr_t addr;
	ks_socket_t listener = KS_SOCK_INVALID;
	ks_socket_t sock = KS_SOCK_INVALID;

	ks_init();

	plan(2);

	ks_pool_open(&pool);

	ks_addr_set(&addr, "0.0.0.0", 1234, AF_INET);

	listener = start_listen(&addr);
	listener_index = listeners_count++;
	listeners_poll = (struct pollfd *)ks_pool_alloc(pool, sizeof(struct pollfd) * listeners_count);
	ok(listeners_poll != NULL);

	listeners_poll[listener_index].fd = listener;
	listeners_poll[listener_index].events = POLLIN;

	while (1) {
		int p = ks_poll(listeners_poll, listeners_count, 100);
		if (p > 0) {
			printf("POLL event occurred\n");
			for (int32_t index = 0; index < listeners_count; ++index) {
				if (listeners_poll[index].revents & POLLERR) {
					printf("POLLERR on index %d\n", index);
					break;
				}
				if (!(listeners_poll[index].revents & POLLIN)) continue;

				printf("POLLIN on index %d\n", index);

				if ((sock = accept(listeners_poll[index].fd, NULL, NULL)) == KS_SOCK_INVALID) {
					printf("Accept failed on index %d\n", index);
					continue;
				}

				printf("Accept success on index %d\n", index);
			}
			break;
		} else if (p < 0) {
			printf("Polling socket error %d\n", WSAGetLastError());
		}
	}

	ok(sock != KS_SOCK_INVALID);

	if (sock != KS_SOCK_INVALID) ks_socket_close(&sock);

	for (int index = 0; index < listeners_count; ++index) {
		listener = listeners_poll[index].fd;
		ks_socket_close(&listener);
	}
	ks_pool_free(&listeners_poll);

	ks_pool_close(&pool);

	ks_shutdown();

	done_testing();
}
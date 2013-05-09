/* 
 * mod_rtmp for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2011-2012, Barracuda Networks Inc.
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mod_rtmp for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is Barracuda Networks Inc.
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Mathieu Rene <mrene@avgs.ca>
 * William King <william.king@quentustech.com>
 *
 * rtmp_tcp.c -- RTMP TCP I/O module
 *
 */

#include "mod_rtmp.h"

/* Locally-extended version of rtmp_io_t */
struct rtmp_io_tcp {
	rtmp_io_t base;

	switch_pollset_t *pollset;
	switch_pollfd_t *listen_pollfd;
	switch_socket_t *listen_socket;
	const char *ip;
	switch_port_t port;
	switch_thread_t *thread;
	switch_mutex_t *mutex;
};

typedef struct rtmp_io_tcp rtmp_io_tcp_t;

struct rtmp_tcp_io_private {
	switch_pollfd_t *pollfd;
	switch_socket_t *socket;
	switch_buffer_t *sendq;
	switch_bool_t poll_send;
};

typedef struct rtmp_tcp_io_private rtmp_tcp_io_private_t;

static void rtmp_tcp_alter_pollfd(rtmp_session_t *rsession, switch_bool_t pollout)
{
	rtmp_tcp_io_private_t *io_pvt = rsession->io_private;
	rtmp_io_tcp_t *io = (rtmp_io_tcp_t*)rsession->profile->io;
	
	if (pollout && (io_pvt->pollfd->reqevents & SWITCH_POLLOUT)) {
		return;
	} else if (!pollout && !(io_pvt->pollfd->reqevents & SWITCH_POLLOUT)) {
		return;
	}
	
	switch_pollset_remove(io->pollset, io_pvt->pollfd);
	io_pvt->pollfd->reqevents = SWITCH_POLLIN | SWITCH_POLLERR;
	if (pollout) {
		io_pvt->pollfd->reqevents |=  SWITCH_POLLOUT;
	}
	switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_NOTICE, "Pollout: %s\n", 
		pollout ? "true" : "false");
	
	switch_pollset_add(io->pollset, io_pvt->pollfd);
}

static switch_status_t rtmp_tcp_read(rtmp_session_t *rsession, unsigned char *buf, switch_size_t *len)
{
	//rtmp_io_tcp_t *io = (rtmp_io_tcp_t*)rsession->profile->io;
	rtmp_tcp_io_private_t *io_pvt = rsession->io_private;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
#ifdef RTMP_DEBUG_IO
	switch_size_t olen = *len;
#endif	
	switch_assert(*len > 0 && *len < 1024000);

	do {
		status = switch_socket_recv(io_pvt->socket, (char*)buf, len);	
	} while(status != SWITCH_STATUS_SUCCESS && SWITCH_STATUS_IS_BREAK(status));
	
#ifdef RTMP_DEBUG_IO
	{
		int i;

		fprintf(rsession->io_debug_in, "recv %p max=%"SWITCH_SIZE_T_FMT" got=%"SWITCH_SIZE_T_FMT"\n< ", (void*)buf, olen, *len);
			
		for (i = 0; i < *len; i++) {
			
			fprintf(rsession->io_debug_in, "%02X ", (uint8_t)buf[i]);

			if (i != 0 && i % 32 == 0) {
				fprintf(rsession->io_debug_in, "\n> ");
			}
		}
		fprintf(rsession->io_debug_in, "\n\n");
		fflush(rsession->io_debug_in);
	}
#endif

	return status;
}

static switch_status_t rtmp_tcp_write(rtmp_session_t *rsession, const unsigned char *buf, switch_size_t *len)
{
	//rtmp_io_tcp_t *io = (rtmp_io_tcp_t*)rsession->profile->io;
	rtmp_tcp_io_private_t *io_pvt = rsession->io_private;
	switch_status_t status;
	switch_size_t orig_len = *len;	
	
#ifdef RTMP_DEBUG_IO
	{
		int i;
		fprintf(rsession->io_debug_out,
			"SEND %"SWITCH_SIZE_T_FMT" bytes\n> ", *len);

		for (i = 0; i < *len; i++) {
			fprintf(rsession->io_debug_out, "%02X ", (uint8_t)buf[i]);
				
			if (i != 0 && i % 32 == 0) {
				fprintf(rsession->io_debug_out, "\n> ");
			}
		}
		fprintf(rsession->io_debug_out, "\n\n ");
		
		fflush(rsession->io_debug_out);
	}
#endif
	
	if (io_pvt->sendq && switch_buffer_inuse(io_pvt->sendq) > 0) {
		/* We already have queued data, append it to the sendq */
		switch_buffer_write(io_pvt->sendq, buf, *len);
		return SWITCH_STATUS_SUCCESS;
	}
	
	status = switch_socket_send_nonblock(io_pvt->socket, (char*)buf, len);
	
	if (*len > 0 && *len < orig_len) {
		
		if (rsession->state >= RS_DESTROY) {
			return SWITCH_STATUS_FALSE;
		}
		
		/* We didnt send it all... add it to the sendq*/		
		switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_DEBUG, "%"SWITCH_SIZE_T_FMT" bytes added to sendq.\n", (orig_len - *len));
		
		switch_buffer_write(io_pvt->sendq, (buf + *len), orig_len - *len);

		/* Make sure we poll-write */
		rtmp_tcp_alter_pollfd(rsession, SWITCH_TRUE);
	}
	
	return status;
}

static switch_status_t rtmp_tcp_close(rtmp_session_t *rsession)
{
	rtmp_io_tcp_t *io = (rtmp_io_tcp_t*)rsession->profile->io;
	rtmp_tcp_io_private_t *io_pvt = rsession->io_private;	
	
	if (io_pvt->socket) {
		switch_mutex_lock(io->mutex);
		switch_pollset_remove(io->pollset, io_pvt->pollfd);
		switch_mutex_unlock(io->mutex);

		switch_socket_close(io_pvt->socket);
		io_pvt->socket = NULL;
	}

	if ( io_pvt->sendq ) {
		switch_buffer_destroy(&(io_pvt->sendq));
	}
	
	return SWITCH_STATUS_SUCCESS;
}


void *SWITCH_THREAD_FUNC rtmp_io_tcp_thread(switch_thread_t *thread, void *obj)
{
	rtmp_io_tcp_t *io = (rtmp_io_tcp_t*)obj;
	io->base.running = 1;
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s: I/O Thread starting\n", io->base.profile->name);
	
	
	while(io->base.running) {
		const switch_pollfd_t *fds;
		int32_t numfds;
		int32_t i;
		switch_status_t status;
		
		switch_mutex_lock(io->mutex);
		status = switch_pollset_poll(io->pollset, 500000, &numfds, &fds);
		switch_mutex_unlock(io->mutex);
		
		if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_TIMEOUT) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "pollset_poll failed\n");
			continue;
		} else if (status == SWITCH_STATUS_TIMEOUT) {
			switch_cond_next();
		}
		
		for (i = 0; i < numfds; i++) {
			if (!fds[i].client_data) { 
				switch_socket_t *newsocket;
				if (switch_socket_accept(&newsocket, io->listen_socket, io->base.pool) != SWITCH_STATUS_SUCCESS) {
					if (io->base.running) {
						/* Don't spam the logs if we are shutting down */
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Socket Error [%s]\n", strerror(errno));	
					} else {
						return NULL;
					}
				} else {
					rtmp_session_t *rsession;
					
					if (switch_socket_opt_set(newsocket, SWITCH_SO_NONBLOCK, TRUE)) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Couldn't set socket as non-blocking\n");
					}

					if (switch_socket_opt_set(newsocket, SWITCH_SO_TCP_NODELAY, 1)) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Couldn't disable Nagle.\n");
					}
					
					if (rtmp_session_request(io->base.profile, &rsession) != SWITCH_STATUS_SUCCESS) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "RTMP session request failed\n");
						switch_socket_close(newsocket);
					} else {
						switch_sockaddr_t *addr = NULL;
						char ipbuf[200];
						
						/* Create out private data and attach it to the rtmp session structure */
						rtmp_tcp_io_private_t *pvt = switch_core_alloc(rsession->pool, sizeof(*pvt));
						rsession->io_private = pvt;
						pvt->socket = newsocket;
						switch_socket_create_pollfd(&pvt->pollfd, newsocket, SWITCH_POLLIN | SWITCH_POLLERR, rsession, rsession->pool);
						switch_pollset_add(io->pollset, pvt->pollfd);
						switch_buffer_create_dynamic(&pvt->sendq, 512, 1024, 0);
						
						/* Get the remote address/port info */
						switch_socket_addr_get(&addr, SWITCH_TRUE, newsocket);
						switch_get_addr(ipbuf, sizeof(ipbuf), addr);
						rsession->remote_address = switch_core_strdup(rsession->pool, ipbuf);
						rsession->remote_port = switch_sockaddr_get_port(addr);
						switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_INFO, "Rtmp connection from %s:%i\n",
										  rsession->remote_address, rsession->remote_port);
					}
				}
			} else {
				rtmp_session_t *rsession = (rtmp_session_t*)fds[i].client_data;
				rtmp_tcp_io_private_t *io_pvt = (rtmp_tcp_io_private_t*)rsession->io_private;
				
				if (fds[i].rtnevents & SWITCH_POLLOUT && switch_buffer_inuse(io_pvt->sendq) > 0) {
					/* Send as much remaining data as possible */
					switch_size_t sendlen;
					const void *ptr;
					sendlen = switch_buffer_peek_zerocopy(io_pvt->sendq, &ptr);
					switch_socket_send_nonblock(io_pvt->socket, ptr, &sendlen);
					switch_buffer_toss(io_pvt->sendq, sendlen);
					if (switch_buffer_inuse(io_pvt->sendq) == 0) {
						/* Remove our fd from OUT polling */
						rtmp_tcp_alter_pollfd(rsession, SWITCH_FALSE);
					}
				} else 	if (fds[i].rtnevents & SWITCH_POLLIN && rtmp_handle_data(rsession) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_UUID_LOG(rsession->uuid), SWITCH_LOG_DEBUG, "Closing socket\n");
					
					switch_mutex_lock(io->mutex);
					switch_pollset_remove(io->pollset, io_pvt->pollfd);
					switch_mutex_unlock(io->mutex);
					
					switch_socket_close(io_pvt->socket);
					io_pvt->socket = NULL;

					io->base.close(rsession);
					
					rtmp_session_destroy(&rsession);
				}
			}
		}
	}
	
	io->base.running = -1;
	switch_socket_close(io->listen_socket);
	
	return NULL;
}

switch_status_t rtmp_tcp_init(rtmp_profile_t *profile, const char *bindaddr, rtmp_io_t **new_io, switch_memory_pool_t *pool)
{
	char *szport;
	switch_sockaddr_t *sa;
	switch_threadattr_t *thd_attr = NULL;
	rtmp_io_tcp_t *io_tcp;
		
	io_tcp = (rtmp_io_tcp_t*)switch_core_alloc(pool, sizeof(rtmp_io_tcp_t));
	io_tcp->base.pool = pool;
	io_tcp->ip = switch_core_strdup(pool, bindaddr);
	
	*new_io = (rtmp_io_t*)io_tcp;
	io_tcp->base.profile = profile;
	io_tcp->base.read = rtmp_tcp_read;
	io_tcp->base.write = rtmp_tcp_write;
	io_tcp->base.close = rtmp_tcp_close;
	io_tcp->base.name = "tcp";
	io_tcp->base.address = switch_core_strdup(pool, io_tcp->ip);
	
	if ((szport = strchr(io_tcp->ip, ':'))) {
		*szport++ = '\0';
		io_tcp->port = atoi(szport);
	} else {
		io_tcp->port = RTMP_DEFAULT_PORT;
	}
	
	if (switch_sockaddr_info_get(&sa, io_tcp->ip, SWITCH_INET, io_tcp->port, 0, pool)) {
		goto fail;
	}
	if (switch_socket_create(&io_tcp->listen_socket, switch_sockaddr_get_family(sa), SOCK_STREAM, SWITCH_PROTO_TCP, pool)) {
		goto fail;
	}
	if (switch_socket_opt_set(io_tcp->listen_socket, SWITCH_SO_REUSEADDR, 1)) {
		goto fail;
	}
	if (switch_socket_opt_set(io_tcp->listen_socket, SWITCH_SO_TCP_NODELAY, 1)) {
		goto fail;
	}
	if (switch_socket_bind(io_tcp->listen_socket, sa)) {
		goto fail;
	}
	if (switch_socket_listen(io_tcp->listen_socket, 10)) {
		goto fail;
	}
	if (switch_socket_opt_set(io_tcp->listen_socket, SWITCH_SO_NONBLOCK, TRUE)) {
		goto fail;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Listening on %s:%u (tcp)\n", io_tcp->ip, io_tcp->port);
	
	io_tcp->base.running = 1;
	
	if (switch_pollset_create(&io_tcp->pollset, 1000 /* max poll fds */, pool, 0) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "pollset_create failed\n");
		goto fail;
	}
	
	switch_socket_create_pollfd(&(io_tcp->listen_pollfd), io_tcp->listen_socket, SWITCH_POLLIN | SWITCH_POLLERR, NULL, pool);
	if (switch_pollset_add(io_tcp->pollset, io_tcp->listen_pollfd) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "pollset_add failed\n");
		goto fail;
	}
	
	switch_mutex_init(&io_tcp->mutex, SWITCH_MUTEX_NESTED, pool);
	
	switch_threadattr_create(&thd_attr, pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&io_tcp->thread, thd_attr, rtmp_io_tcp_thread, *new_io, pool);
	
	return SWITCH_STATUS_SUCCESS;
fail:
	if (io_tcp->listen_socket) {
		switch_socket_close(io_tcp->listen_socket);
	}
	*new_io = NULL;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Socket error. Couldn't listen on %s:%u\n", io_tcp->ip, io_tcp->port);
	return SWITCH_STATUS_FALSE;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */

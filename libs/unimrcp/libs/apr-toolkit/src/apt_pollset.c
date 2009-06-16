/*
 * Copyright 2008 Arsen Chaloyan
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <apr_poll.h>
#include "apt_pollset.h"
#include "apt_log.h"

struct apt_pollset_t {
	/** APR pollset */
	apr_pollset_t *base;
#ifdef WIN32
	/** Socket descriptors used for wakeup */
	apr_socket_t  *wakeup_pipe[2];
#else
	/** Pipe descriptors used for wakeup */
	apr_file_t    *wakeup_pipe[2];
#endif
	/** Builtin wakeup poll descriptor */
	apr_pollfd_t   wakeup_pfd;

	/** Pool to allocate memory from */
	apr_pool_t    *pool;
};

static apt_bool_t apt_wakeup_pipe_create(apt_pollset_t *pollset);
static apt_bool_t apt_wakeup_pipe_destroy(apt_pollset_t *pollset);

/** Create interruptable pollset on top of APR pollset */
APT_DECLARE(apt_pollset_t*) apt_pollset_create(apr_uint32_t size, apr_pool_t *pool)
{
	apt_pollset_t *pollset = apr_palloc(pool,sizeof(apt_pollset_t));
	pollset->pool = pool;
	memset(&pollset->wakeup_pfd,0,sizeof(pollset->wakeup_pfd));
	
	/* create pollset with max number of descriptors size+1, 
	where +1 is builtin wakeup descriptor */
	if(apr_pollset_create(&pollset->base,size+1,pool,0) != APR_SUCCESS) {
		return NULL;
	}

	/* create wakeup pipe */
	if(apt_wakeup_pipe_create(pollset) != TRUE) {
		apr_pollset_destroy(pollset->base);
		return NULL;
	}

	/* add wakeup pipe to pollset */
	if(apr_pollset_add(pollset->base,&pollset->wakeup_pfd) != APR_SUCCESS) {
		apt_wakeup_pipe_destroy(pollset);
		apr_pollset_destroy(pollset->base);
		return NULL;
	}
	return pollset;
}

/** Destroy pollset */
APT_DECLARE(apt_bool_t) apt_pollset_destroy(apt_pollset_t *pollset)
{
	/* remove wakeup pipe from pollset */
	apr_pollset_remove(pollset->base,&pollset->wakeup_pfd);
	/* destroy wakeup pipe */
	apt_wakeup_pipe_destroy(pollset);
	/* destroy pollset */
	apr_pollset_destroy(pollset->base);
	return TRUE;
}

/** Add pollset descriptor to a pollset */
APT_DECLARE(apt_bool_t) apt_pollset_add(apt_pollset_t *pollset, const apr_pollfd_t *descriptor)
{
	return (apr_pollset_add(pollset->base,descriptor) == APR_SUCCESS) ? TRUE : FALSE;
}

/** Remove pollset descriptor from a pollset */
APT_DECLARE(apt_bool_t) apt_pollset_remove(apt_pollset_t *pollset, const apr_pollfd_t *descriptor)
{
	return (apr_pollset_remove(pollset->base,descriptor) == APR_SUCCESS) ? TRUE : FALSE;
}

/** Block for activity on the descriptor(s) in a pollset */
APT_DECLARE(apr_status_t) apt_pollset_poll(
								apt_pollset_t *pollset,
								apr_interval_time_t timeout,
								apr_int32_t *num,
								const apr_pollfd_t **descriptors)
{
	return apr_pollset_poll(pollset->base,timeout,num,descriptors);
}

/** Interrupt the blocked poll call */
APT_DECLARE(apt_bool_t) apt_pollset_wakeup(apt_pollset_t *pollset)
{
	apt_bool_t status = TRUE;
#ifdef WIN32
	char tmp = 0;
	apr_size_t len = sizeof(tmp);
	if(apr_socket_send(pollset->wakeup_pipe[1],&tmp,&len) != APR_SUCCESS) {
		status = FALSE;
	}
#else
	if(apr_file_putc(1, pollset->wakeup_pipe[1]) != APR_SUCCESS) {
		status = FALSE;
	}
#endif
	return status;
}

/** Match against builtin wake up descriptor in a pollset */
APT_DECLARE(apt_bool_t) apt_pollset_is_wakeup(apt_pollset_t *pollset, const apr_pollfd_t *descriptor)
{
	apt_bool_t status = FALSE;
#ifdef WIN32
	if(descriptor->desc.s == pollset->wakeup_pipe[0]) {
		char rb[512];
		apr_size_t nr = sizeof(rb);

		/* simply read out from the input side of the pipe all the data. */
		while(apr_socket_recv(pollset->wakeup_pipe[0], rb, &nr) == APR_SUCCESS) {
			if(nr != sizeof(rb)) {
				break;
			}
		}
		status = TRUE;
	}
#else
	if(descriptor->desc.f == pollset->wakeup_pipe[0]) {
		char rb[512];
		apr_size_t nr = sizeof(rb);

		/* simply read out from the input side of the pipe all the data. */
		while(apr_file_read(pollset->wakeup_pipe[0], rb, &nr) == APR_SUCCESS) {
			if(nr != sizeof(rb)) {
				break;
			}
		}
		status = TRUE;
	}
#endif
	return status;
}

#ifdef WIN32
static apr_status_t socket_pipe_create(apr_socket_t **rd, apr_socket_t **wr, apr_pool_t *pool)
{
	static int id = 0;

	apr_socket_t *ls = NULL;
	apr_sockaddr_t *pa = NULL;
	apr_sockaddr_t *ca = NULL;
	apr_size_t nrd;
	int uid[2];
	int iid[2];

	/* Create the unique socket identifier
	 * so that we know the connection originated
	 * from us.
	 */
	uid[0] = getpid();
	uid[1] = id++;
	if(apr_socket_create(&ls, AF_INET, SOCK_STREAM, APR_PROTO_TCP, pool) != APR_SUCCESS) {
		return apr_get_netos_error();
	}
	apr_socket_opt_set(ls, APR_SO_REUSEADDR, 1);

	if(apr_sockaddr_info_get(&pa,"127.0.0.1",APR_INET,0,0,pool) != APR_SUCCESS) {
		apr_socket_close(ls);
		return apr_get_netos_error();
	}

	if(apr_socket_bind(ls, pa) != APR_SUCCESS) {
		apr_socket_close(ls);
		return apr_get_netos_error();
	}
	
	if(apr_socket_addr_get(&ca,APR_LOCAL,ls) != APR_SUCCESS) {
		apr_socket_close(ls);
		return apr_get_netos_error();
	}

	if(apr_socket_listen(ls,1) != APR_SUCCESS) {
		apr_socket_close(ls);
		return apr_get_netos_error();
	}

	if(apr_socket_create(wr, AF_INET, SOCK_STREAM, APR_PROTO_TCP, pool) != APR_SUCCESS) {
		apr_socket_close(ls);
		return apr_get_netos_error();
	}
	apr_socket_opt_set(*wr, APR_SO_REUSEADDR, 1);

	if(apr_socket_connect(*wr, ca) != APR_SUCCESS) {
		apr_socket_close(ls);
		apr_socket_close(*wr);
		return apr_get_netos_error();
	}
	nrd = sizeof(uid);
	if(apr_socket_send(*wr, (char *)uid, &nrd) != APR_SUCCESS) {
		apr_socket_close(ls);
		apr_socket_close(*wr);
		return apr_get_netos_error();
	}
	
	apr_socket_opt_set(ls, APR_SO_NONBLOCK, 0);
	/* Listening socket is blocking by now. The accept should 
	 * return immediatelly because we connected already.
	 */
	if(apr_socket_accept(rd, ls, pool) != APR_SUCCESS) {
		apr_socket_close(ls);
		apr_socket_close(*wr);
		return apr_get_netos_error();
	}

	/* Put read side of the pipe to the blocking mode */
	apr_socket_opt_set(*rd, APR_SO_NONBLOCK, 0);

	for (;;) {
		/* Verify the connection by reading the sent identification */
		nrd = sizeof(iid);
		if(apr_socket_recv(*rd, (char *)iid, &nrd) != APR_SUCCESS) {
			apr_socket_close(ls);
			apr_socket_close(*wr);
			apr_socket_close(*rd);
			return apr_get_netos_error();
		}
		if(nrd == sizeof(iid)) {
			if(memcmp(uid, iid, sizeof(uid)) == 0) {
				/* Wow, we recived what we sent */
				break;
			}
		}
	}

	/* We don't need the listening socket any more */
	apr_socket_close(ls);
	return APR_SUCCESS;
}

/** Create a dummy wakeup pipe for interrupting the poller */
static apt_bool_t apt_wakeup_pipe_create(apt_pollset_t *pollset)
{
	apr_socket_t *rd = NULL;
	apr_socket_t *wr = NULL;
	apr_status_t rv;
	rv = socket_pipe_create(&rd,&wr,pollset->pool);
	if(rv != APR_SUCCESS) {
		char err_str[256];
		apr_strerror(rv,err_str,sizeof(err_str));
		apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Failed to Create Wakeup Pipe: %s",err_str);
		return FALSE;
	}
	pollset->wakeup_pfd.reqevents = APR_POLLIN;
	pollset->wakeup_pfd.desc_type = APR_POLL_SOCKET;
	pollset->wakeup_pfd.desc.s = rd;
	
	pollset->wakeup_pipe[0] = rd;
	pollset->wakeup_pipe[1] = wr;
	return TRUE;
}

/** Destroy wakeup pipe */
static apt_bool_t apt_wakeup_pipe_destroy(apt_pollset_t *pollset)
{
	/* Close both sides of the wakeup pipe */
	if(pollset->wakeup_pipe[0]) {
		apr_socket_close(pollset->wakeup_pipe[0]);
		pollset->wakeup_pipe[0] = NULL;
	}
	if(pollset->wakeup_pipe[1]) {
		apr_socket_close(pollset->wakeup_pipe[1]);
		pollset->wakeup_pipe[1] = NULL;
	}
	return TRUE;
}

#else 

/** Create a dummy wakeup pipe for interrupting the poller */
static apt_bool_t apt_wakeup_pipe_create(apt_pollset_t *pollset)
{
	apr_file_t *file_in = NULL;
	apr_file_t *file_out = NULL;

	if(apr_file_pipe_create(&file_in,&file_out,pollset->pool) != APR_SUCCESS) {
		return FALSE;
	}
	pollset->wakeup_pfd.reqevents = APR_POLLIN;
	pollset->wakeup_pfd.desc_type = APR_POLL_FILE;
	pollset->wakeup_pfd.desc.f = file_in;
	
	pollset->wakeup_pipe[0] = file_in;
	pollset->wakeup_pipe[1] = file_out;
	return TRUE;
}

/** Destroy wakeup pipe */
static apt_bool_t apt_wakeup_pipe_destroy(apt_pollset_t *pollset)
{
	/* Close both sides of the wakeup pipe */
	if(pollset->wakeup_pipe[0]) {
		apr_file_close(pollset->wakeup_pipe[0]);
		pollset->wakeup_pipe[0] = NULL;
	}
	if(pollset->wakeup_pipe[1]) {
		apr_file_close(pollset->wakeup_pipe[1]);
		pollset->wakeup_pipe[1] = NULL;
	}
	return TRUE;
}

#endif

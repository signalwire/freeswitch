/*
 * Copyright (c) 2007-2012, Anthony Minessale II, Nenad Corbic
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _SANGOMABC_H
#define _SANGOMABC_H

#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#ifdef HAVE_NETINET_SCTP_H
#include <netinet/sctp.h>
#endif
#include <arpa/inet.h>
#include <stdarg.h>
#include <netdb.h>
#include <sigboost.h>
#include <sys/time.h>

#define sangomabc_test_flag(p,flag) 		({		\
			((p)->flags & (flag));				\
		})

#define sangomabc_set_flag(p,flag) 		do {		\
		((p)->flags |= (flag));					\
	} while (0)

#define sangomabc_clear_flag(p,flag) 		do {	\
		((p)->flags &= ~(flag));				\
	} while (0)

#define sangomabc_copy_flags(dest,src,flagz)	do {	\
		(dest)->flags &= ~(flagz);					\
		(dest)->flags |= ((src)->flags & (flagz));	\
	} while (0)

typedef  t_sigboost_callstart sangomabc_event_t;
typedef  t_sigboost_short sangomabc_short_event_t;
typedef uint32_t sangomabc_event_id_t;

typedef struct sangomabc_ip_cfg
{
	char local_ip[25];
	int local_port;
	char remote_ip[25];
	int remote_port;
}sangomabc_ip_cfg_t;

typedef enum {
	MSU_FLAG_EVENT = (1 << 0),
	MSU_FLAG_DOWN = (1 << 1)
} sangomabc_flag_t;


struct sangomabc_connection {
	zap_socket_t socket;
	struct sockaddr_in local_addr;
	struct sockaddr_in remote_addr;
	sangomabc_event_t event;
	struct hostent remote_hp;
	struct hostent local_hp;
	unsigned int flags;
	zap_mutex_t *mutex;
	FILE *log;
	unsigned int txseq;
	unsigned int rxseq;
	unsigned int txwindow;
	unsigned int rxseq_reset;
	sangomabc_ip_cfg_t cfg;
	uint32_t hb_elapsed;
};

typedef struct sangomabc_connection sangomabc_connection_t;

/* disable nagle's algorythm */
static inline void sctp_no_nagle(int socket)
{
#ifdef HAVE_NETINET_SCTP_H
    int flag = 1;
    setsockopt(socket, IPPROTO_SCTP, SCTP_NODELAY, (char *) &flag, sizeof(int));
#endif
}

int sangomabc_connection_close(sangomabc_connection_t *mcon);
int sangomabc_connection_open(sangomabc_connection_t *mcon, char *local_ip, int local_port, char *ip, int port);
sangomabc_event_t *__sangomabc_connection_read(sangomabc_connection_t *mcon, int iteration, const char *file, const char *func, int line);
sangomabc_event_t *__sangomabc_connection_readp(sangomabc_connection_t *mcon, int iteration, const char *file, const char *func, int line);
int __sangomabc_connection_write(sangomabc_connection_t *mcon, sangomabc_event_t *event, const char *file, const char *func, int line);
int __sangomabc_connection_writep(sangomabc_connection_t *mcon, sangomabc_event_t *event, const char *file, const char *func, int line);
#define sangomabc_connection_write(_m,_e) __sangomabc_connection_write(_m, _e, __FILE__, __func__, __LINE__)
#define sangomabc_connection_writep(_m,_e) __sangomabc_connection_writep(_m, _e, __FILE__, __func__, __LINE__)
#define sangomabc_connection_read(_m,_e) __sangomabc_connection_read(_m, _e, __FILE__, __func__, __LINE__)
#define sangomabc_connection_readp(_m,_e) __sangomabc_connection_readp(_m, _e, __FILE__, __func__, __LINE__)
void sangomabc_event_init(sangomabc_short_event_t *event, sangomabc_event_id_t event_id, int chan, int span);
void sangomabc_call_init(sangomabc_event_t *event, const char *calling, const char *called, int setup_id);
const char *sangomabc_event_id_name(uint32_t event_id);
int sangomabc_exec_command(sangomabc_connection_t *mcon, int span, int chan, int id, int cmd, int cause, int flags);
int sangomabc_exec_commandp(sangomabc_connection_t *pcon, int span, int chan, int id, int cmd, int cause);

#endif


/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */

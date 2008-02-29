/*
 * Copyright (c) 2007, Anthony Minessale II, Nenad Corbic
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

#ifndef _SS7BC_H
#define _SS7BC_H

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
#include <pthread.h>
#include <sys/time.h>




#define ss7bc_test_flag(p,flag) 		({		\
			((p)->flags & (flag));				\
		})

#define ss7bc_set_flag(p,flag) 		do {		\
		((p)->flags |= (flag));					\
	} while (0)

#define ss7bc_clear_flag(p,flag) 		do {	\
		((p)->flags &= ~(flag));				\
	} while (0)

#define ss7bc_copy_flags(dest,src,flagz)	do {	\
		(dest)->flags &= ~(flagz);					\
		(dest)->flags |= ((src)->flags & (flagz));	\
	} while (0)

typedef  t_sigboost ss7bc_event_t;
typedef uint32_t ss7bc_event_id_t;

typedef struct ss7bc_ip_cfg
{
	char local_ip[25];
	int local_port;
	char remote_ip[25];
	int remote_port;
}ss7bc_ip_cfg_t;

struct ss7bc_connection {
	zap_socket_t socket;
	struct sockaddr_in local_addr;
	struct sockaddr_in remote_addr;
	ss7bc_event_t event;
	struct hostent remote_hp;
	struct hostent local_hp;
	unsigned int flags;
	pthread_mutex_t lock;
	FILE *log;
	unsigned int txseq;
	unsigned int rxseq;
	unsigned int txwindow;
	unsigned int rxseq_reset;
	ss7bc_ip_cfg_t cfg;
};

typedef enum {
	MSU_FLAG_EVENT = (1 << 0)
} ss7bc_flag_t;

typedef struct ss7bc_connection ss7bc_connection_t;

/* disable nagle's algorythm */
static inline void sctp_no_nagle(int socket)
{
#ifdef HAVE_NETINET_SCTP_H
    int flag = 1;
    setsockopt(socket, IPPROTO_SCTP, SCTP_NODELAY, (char *) &flag, sizeof(int));
#endif
}

int ss7bc_connection_close(ss7bc_connection_t *mcon);
int ss7bc_connection_open(ss7bc_connection_t *mcon, char *local_ip, int local_port, char *ip, int port);
ss7bc_event_t *ss7bc_connection_read(ss7bc_connection_t *mcon, int iteration);
ss7bc_event_t *ss7bc_connection_readp(ss7bc_connection_t *mcon, int iteration);
int ss7bc_connection_write(ss7bc_connection_t *mcon, ss7bc_event_t *event);
void ss7bc_event_init(ss7bc_event_t *event, ss7bc_event_id_t event_id, int chan, int span);
void ss7bc_call_init(ss7bc_event_t *event, const char *calling, const char *called, int setup_id);
const char *ss7bc_event_id_name(uint32_t event_id);
int ss7bc_exec_command(ss7bc_connection_t *mcon, int span, int chan, int id, int cmd, int cause);

#endif


/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */

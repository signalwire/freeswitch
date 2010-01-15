/*
 *  m3ua_client.h
 *  freetdm
 *
 *  Created by Shane Burrell on 4/3/08.
 *  Copyright 2008 Shane Burrell. All rights reserved.
 *
 * Copyright (c) 2007, Anthony Minessale II, Nenad Corbic
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

// Fix this for portability
#include <sctp.h>
//#include <netinet/sctp.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <netdb.h>
//#include <sigboost.h>
#include <sys/time.h>

#define MAX_DIALED_DIGITS	31
#define MAX_CALLING_NAME	31

/* Next two defines are used to create the range of values for call_setup_id
 * in the t_sigboost structure.
 * 0..((CORE_MAX_SPANS * CORE_MAX_CHAN_PER_SPAN) - 1) */
#define CORE_MAX_SPANS 		200
#define CORE_MAX_CHAN_PER_SPAN 	30
#define MAX_PENDING_CALLS 	CORE_MAX_SPANS * CORE_MAX_CHAN_PER_SPAN
/* 0..(MAX_PENDING_CALLS-1) is range of call_setup_id below */
#define SIZE_RDNIS		80

//#undef MSGWINDOW
#define MSGWINDOW


typedef struct
{
	uint32_t	event_id;
	uint32_t	fseqno;
#ifdef MSGWINDOW
	uint32_t	bseqno;
#endif
	uint16_t	call_setup_id;
	uint32_t	trunk_group;
	uint32_t	span;
	uint32_t	chan;
	uint8_t		called_number_digits_count;
	char		called_number_digits [MAX_DIALED_DIGITS + 1]; /* it's a null terminated string */
	uint8_t		calling_number_digits_count; /* it's an array */
	char		calling_number_digits [MAX_DIALED_DIGITS + 1]; /* it's a null terminated string */
	uint8_t		release_cause;
	struct timeval  tv;
	/* ref. Q.931 Table 4-11 and Q.951 Section 3 */
	uint8_t		calling_number_screening_ind;
	uint8_t		calling_number_presentation;
	char		redirection_string [SIZE_RDNIS]; /* it's a null terminated string */
	
} t_m3ua;

typedef t_m3ua m3uac_event_t;
typedef uint32_t m3uac_event_id_t;


typedef struct m3uac_ip_cfg
{
	char local_ip[25];
	int local_port;
	char remote_ip[25];
	int remote_port;
}m3uac_ip_cfg_t;

struct m3uac_connection {
	ftdm_socket_t socket;
	struct sockaddr_in local_addr;
	struct sockaddr_in remote_addr;
	m3uac_event_t event;
	struct hostent remote_hp;
	struct hostent local_hp;
	unsigned int flags;
	ftdm_mutex_t *mutex;
	FILE *log;
	unsigned int txseq;
	unsigned int rxseq;
	unsigned int txwindow;
	unsigned int rxseq_reset;
	m3uac_ip_cfg_t cfg;
	uint32_t hb_elapsed;
	int up;
};

typedef enum {
	MSU_FLAG_EVENT = (1 << 0)
} m3uac_flag_t;

typedef struct m3uac_connection m3uac_connection_t;

static inline void sctp_no_nagle(int socket)
{
    //int flag = 1;
    //setsockopt(socket, IPPROTO_SCTP, SCTP_NODELAY, (char *) &flag, sizeof(int));
}

int m3uac_connection_close(m3uac_connection_t *mcon);
int m3uac_connection_open(m3uac_connection_t *mcon, char *local_ip, int local_port, char *ip, int port);
m3uac_event_t *m3uac_connection_read(m3uac_connection_t *mcon, int iteration);
m3uac_event_t *m3uac_connection_readp(m3uac_connection_t *mcon, int iteration);
int m3uac_connection_write(m3uac_connection_t *mcon, m3uac_event_t *event);
void m3uac_event_init(m3uac_event_t *event, m3uac_event_id_t event_id, int chan, int span);
void m3uac_call_init(m3uac_event_t *event, const char *calling, const char *called, int setup_id);
const char *m3uac_event_id_name(uint32_t event_id);
int m3uac_exec_command(m3uac_connection_t *mcon, int span, int chan, int id, int cmd, int cause);




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

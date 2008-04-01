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

#if HAVE_NETDB_H
#include <netdb.h>
#endif

#include "openzap.h"
#include <ss7_boost_client.h>


#ifndef HAVE_GETHOSTBYNAME_R
extern int gethostbyname_r (const char *__name,
							struct hostent *__result_buf,
							char *__buf, size_t __buflen,
							struct hostent **__result,
							int *__h_errnop);
#endif

struct ss7bc_map {
	uint32_t event_id;
	const char *name;
};

static struct ss7bc_map ss7bc_table[] = {
	{SIGBOOST_EVENT_CALL_START, "CALL_START"},
	{SIGBOOST_EVENT_CALL_START_ACK, "CALL_START_ACK"},
	{SIGBOOST_EVENT_CALL_START_NACK, "CALL_START_NACK"},
	{SIGBOOST_EVENT_CALL_START_NACK_ACK, "CALL_START_NACK_ACK"},
	{SIGBOOST_EVENT_CALL_ANSWERED, "CALL_ANSWERED"},
	{SIGBOOST_EVENT_CALL_STOPPED, "CALL_STOPPED"},
	{SIGBOOST_EVENT_CALL_STOPPED_ACK, "CALL_STOPPED_ACK"},
	{SIGBOOST_EVENT_SYSTEM_RESTART, "SYSTEM_RESTART"},
	{SIGBOOST_EVENT_SYSTEM_RESTART_ACK, "SYSTEM_RESTART_ACK"},
	{SIGBOOST_EVENT_HEARTBEAT, "HEARTBEAT"},
	{SIGBOOST_EVENT_INSERT_CHECK_LOOP, "LOOP START"},
	{SIGBOOST_EVENT_REMOVE_CHECK_LOOP, "LOOP STOP"} 
}; 



static int create_conn_socket(ss7bc_connection_t *mcon, char *local_ip, int local_port, char *ip, int port)
{
	int rc;
	struct hostent *result, *local_result;
	char buf[512], local_buf[512];
	int err = 0;

	memset(&mcon->remote_hp, 0, sizeof(mcon->remote_hp));
	memset(&mcon->local_hp, 0, sizeof(mcon->local_hp));
#ifdef HAVE_NETINET_SCTP_H
	mcon->socket = socket(AF_INET, SOCK_SEQPACKET, IPPROTO_SCTP);
#else
	mcon->socket = socket(AF_INET, SOCK_DGRAM, 0);
#endif
 
	zap_log(ZAP_LOG_DEBUG, "Creating L=%s:%d R=%s:%d\n",
			local_ip,local_port,ip,port);

	if (mcon->socket >= 0) {
		int flag;

		flag = 1;
		gethostbyname_r(ip, &mcon->remote_hp, buf, sizeof(buf), &result, &err);
		gethostbyname_r(local_ip, &mcon->local_hp, local_buf, sizeof(local_buf), &local_result, &err);
		if (result && local_result) {
			mcon->remote_addr.sin_family = mcon->remote_hp.h_addrtype;
			memcpy((char *) &mcon->remote_addr.sin_addr.s_addr, mcon->remote_hp.h_addr_list[0], mcon->remote_hp.h_length);
			mcon->remote_addr.sin_port = htons(port);

			mcon->local_addr.sin_family = mcon->local_hp.h_addrtype;
			memcpy((char *) &mcon->local_addr.sin_addr.s_addr, mcon->local_hp.h_addr_list[0], mcon->local_hp.h_length);
			mcon->local_addr.sin_port = htons(local_port);

#ifdef HAVE_NETINET_SCTP_H
			setsockopt(mcon->socket, IPPROTO_SCTP, SCTP_NODELAY, 
					   (char *)&flag, sizeof(int));
#endif

			if ((rc = bind(mcon->socket, 
						   (struct sockaddr *) &mcon->local_addr,
						   sizeof(mcon->local_addr))) < 0) {
				close(mcon->socket);
				mcon->socket = -1;
			} else {
#ifdef HAVE_NETINET_SCTP_H
				rc=listen(mcon->socket,100);
				if (rc) {
					close(mcon->socket);
					mcon->socket = -1;
				}
#endif
			}
		}
	}

	zap_mutex_create(&mcon->mutex);

	return mcon->socket;
}

int ss7bc_connection_close(ss7bc_connection_t *mcon)
{
	if (mcon->socket > -1) {
		close(mcon->socket);
	}

	zap_mutex_lock(mcon->mutex);
	zap_mutex_unlock(mcon->mutex);
	zap_mutex_destroy(&mcon->mutex);
	memset(mcon, 0, sizeof(*mcon));
	mcon->socket = -1;

	return 0;
}

int ss7bc_connection_open(ss7bc_connection_t *mcon, char *local_ip, int local_port, char *ip, int port)
{
	create_conn_socket(mcon, local_ip, local_port, ip, port);
	return mcon->socket;
}


int ss7bc_exec_command(ss7bc_connection_t *mcon, int span, int chan, int id, int cmd, int cause)
{
    ss7bc_event_t oevent;
    int retry = 5;

    ss7bc_event_init(&oevent, cmd, chan, span);
    oevent.release_cause = cause;

	if (cmd == SIGBOOST_EVENT_SYSTEM_RESTART) {
		mcon->rxseq_reset = 1;
		mcon->txseq = 0;
		mcon->rxseq = 0;
		mcon->txwindow = 0;
	}

    if (id >= 0) {
        oevent.call_setup_id = id;
    }

    while (ss7bc_connection_write(mcon, &oevent) <= 0) {
        if (--retry <= 0) {
            zap_log(ZAP_LOG_CRIT, "Failed to tx on ISUP socket: %s\n", strerror(errno));
            return -1;
        } else {
            zap_log(ZAP_LOG_WARNING, "Failed to tx on ISUP socket: %s :retry %i\n", strerror(errno), retry);
			zap_sleep(1);
        }
    }

    return 0;
}



ss7bc_event_t *ss7bc_connection_read(ss7bc_connection_t *mcon, int iteration)
{
	unsigned int fromlen = sizeof(struct sockaddr_in);
	int bytes = 0;

	bytes = recvfrom(mcon->socket, &mcon->event, sizeof(mcon->event), MSG_DONTWAIT, 
					 (struct sockaddr *) &mcon->local_addr, &fromlen);

	if (bytes == sizeof(mcon->event) || bytes == (sizeof(mcon->event)-sizeof(uint32_t))) {

		if (mcon->rxseq_reset) {
			if (mcon->event.event_id == SIGBOOST_EVENT_SYSTEM_RESTART_ACK) {
				zap_log(ZAP_LOG_DEBUG, "Rx sync ok\n");
				mcon->rxseq = mcon->event.fseqno;
				return &mcon->event;
			}
			errno=EAGAIN;
			zap_log(ZAP_LOG_DEBUG, "Waiting for rx sync...\n");
			return NULL;
		}
		
		mcon->txwindow = mcon->txseq - mcon->event.bseqno;
		mcon->rxseq++;

		if (mcon->rxseq != mcon->event.fseqno) {
			zap_log(ZAP_LOG_CRIT, "Invalid Sequence Number Expect=%i Rx=%i\n", mcon->rxseq, mcon->event.fseqno);
			return NULL;
		}

		return &mcon->event;
	} else {
		if (iteration == 0) {
			zap_log(ZAP_LOG_CRIT, "Invalid Event length from boost rxlen=%i evsz=%i\n", bytes, sizeof(mcon->event));
			return NULL;
		}
	}

	return NULL;
}

ss7bc_event_t *ss7bc_connection_readp(ss7bc_connection_t *mcon, int iteration)
{
	unsigned int fromlen = sizeof(struct sockaddr_in);
	int bytes = 0;

	bytes = recvfrom(mcon->socket, &mcon->event, sizeof(mcon->event), MSG_DONTWAIT, (struct sockaddr *) &mcon->local_addr, &fromlen);

	if (bytes == sizeof(mcon->event) || bytes == (sizeof(mcon->event)-sizeof(uint32_t))) {
		return &mcon->event;
	} else {
		if (iteration == 0) {
			zap_log(ZAP_LOG_CRIT, "Critical Error: PQ Invalid Event lenght from boost rxlen=%i evsz=%i\n", bytes, sizeof(mcon->event));
			return NULL;
		}
	}

	return NULL;
}


int ss7bc_connection_write(ss7bc_connection_t *mcon, ss7bc_event_t *event)
{
	int err;

	if (!event || mcon->socket < 0 || !mcon->mutex) {
		zap_log(ZAP_LOG_DEBUG,  "Critical Error: No Event Device\n");
		return -EINVAL;
	}

	if (event->span > 16 || event->chan > 31) {
		zap_log(ZAP_LOG_CRIT, "Critical Error: TX Cmd=%s Invalid Span=%i Chan=%i\n", ss7bc_event_id_name(event->event_id), event->span,event->chan);
		return -1;
	}

	gettimeofday(&event->tv,NULL);
	
	zap_mutex_lock(mcon->mutex);
	event->fseqno = mcon->txseq++;
	event->bseqno = mcon->rxseq;
	err = sendto(mcon->socket, event, sizeof(ss7bc_event_t), 0, (struct sockaddr *) &mcon->remote_addr, sizeof(mcon->remote_addr));
	zap_mutex_unlock(mcon->mutex);

	if (err != sizeof(ss7bc_event_t)) {
		err = -1;
	}
	
 	zap_log(ZAP_LOG_DEBUG, "TX EVENT: %s:(%X) [w%dg%d] Rc=%i CSid=%i Seq=%i Cd=[%s] Ci=[%s]\n",
			ss7bc_event_id_name(event->event_id),
			event->event_id,
			event->span+1,
			event->chan+1,
			event->release_cause,
			event->call_setup_id,
			event->fseqno,
			(event->called_number_digits_count ? (char *) event->called_number_digits : "N/A"),
			(event->calling_number_digits_count ? (char *) event->calling_number_digits : "N/A")
			);

	return err;
}

void ss7bc_call_init(ss7bc_event_t *event, const char *calling, const char *called, int setup_id)
{
	memset(event, 0, sizeof(ss7bc_event_t));
	event->event_id = SIGBOOST_EVENT_CALL_START;

	if (calling) {
		strncpy((char*)event->calling_number_digits, calling, sizeof(event->calling_number_digits)-1);
		event->calling_number_digits_count = strlen(calling);
	}

	if (called) {
		strncpy((char*)event->called_number_digits, called, sizeof(event->called_number_digits)-1);
		event->called_number_digits_count = strlen(called);
	}
		
	event->call_setup_id = setup_id;
	
}

void ss7bc_event_init(ss7bc_event_t *event, ss7bc_event_id_t event_id, int chan, int span) 
{
	memset(event, 0, sizeof(ss7bc_event_t));
	event->event_id = event_id;
	event->chan = chan;
	event->span = span;
}

const char *ss7bc_event_id_name(uint32_t event_id)
{
	unsigned int x;
	const char *ret = NULL;

	for (x = 0 ; x < sizeof(ss7bc_table)/sizeof(struct ss7bc_map); x++) {
		if (ss7bc_table[x].event_id == event_id) {
			ret = ss7bc_table[x].name;
			break;
		}
	}

	return ret;
}

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


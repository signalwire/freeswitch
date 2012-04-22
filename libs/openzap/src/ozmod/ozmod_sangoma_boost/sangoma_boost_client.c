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
#include <sangoma_boost_client.h>

#ifndef HAVE_GETHOSTBYNAME_R
extern int gethostbyname_r (const char *__name, 
							struct hostent *__result_buf, 
							char *__buf, size_t __buflen, 
							struct hostent **__result, 
							int *__h_errnop);
#endif

struct sangomabc_map {
	uint32_t event_id;
	const char *name;
};

static struct sangomabc_map sangomabc_table[] = {
	{SIGBOOST_EVENT_CALL_START, "CALL_START"},
	{SIGBOOST_EVENT_CALL_START_ACK, "CALL_START_ACK"}, 
	{SIGBOOST_EVENT_CALL_START_NACK, "CALL_START_NACK"},
	{SIGBOOST_EVENT_CALL_PROGRESS, "CALL PROGRESS"},
	{SIGBOOST_EVENT_CALL_START_NACK_ACK, "CALL_START_NACK_ACK"}, 
	{SIGBOOST_EVENT_CALL_ANSWERED, "CALL_ANSWERED"}, 
	{SIGBOOST_EVENT_CALL_STOPPED, "CALL_STOPPED"}, 
	{SIGBOOST_EVENT_CALL_STOPPED_ACK, "CALL_STOPPED_ACK"}, 
	{SIGBOOST_EVENT_SYSTEM_RESTART, "SYSTEM_RESTART"}, 
	{SIGBOOST_EVENT_SYSTEM_RESTART_ACK, "SYSTEM_RESTART_ACK"}, 
	{SIGBOOST_EVENT_HEARTBEAT, "HEARTBEAT"}, 
	{SIGBOOST_EVENT_INSERT_CHECK_LOOP, "LOOP START"}, 
	{SIGBOOST_EVENT_REMOVE_CHECK_LOOP, "LOOP STOP"},
	{SIGBOOST_EVENT_AUTO_CALL_GAP_ABATE, "AUTO_CALL_GAP_ABATE"},
	{SIGBOOST_EVENT_DIGIT_IN, "DIGIT_IN"}
}; 



static void sangomabc_print_event_call(sangomabc_connection_t *mcon, sangomabc_event_t *event, int priority, int dir, const char *file, const char *func, int line)
{
	if (event->event_id == SIGBOOST_EVENT_HEARTBEAT)
		return;

	zap_log(file, func, line, ZAP_LOG_LEVEL_WARNING, "%s EVENT: %s:(%X) [w%dg%d] CSid=%i Seq=%i Cn=[%s] Cd=[%s] Ci=[%s] Rdnis=[%s]\n",
		    dir ? "TX":"RX", 
			sangomabc_event_id_name(event->event_id),
			event->event_id,
			event->span+1,
			event->chan+1,
			event->call_setup_id,
			event->fseqno,
			strlen(event->calling_name)?event->calling_name:"N/A",
			(event->called_number_digits_count ? (char *) event->called_number_digits : "N/A"),
			(event->calling_number_digits_count ? (char *) event->calling_number_digits : "N/A"),
			event->isup_in_rdnis);

}
static void sangomabc_print_event_short(sangomabc_connection_t *mcon, sangomabc_short_event_t *event, int priority, int dir, const char *file, const char *func, int line)
{
	if (event->event_id == SIGBOOST_EVENT_HEARTBEAT)
		return;
	zap_log(file, func, line, ZAP_LOG_LEVEL_WARNING, "%s EVENT (%s): %s:(%X) [w%dg%d] Rc=%i CSid=%i Seq=%i \n", 
			   dir ? "TX":"RX", 
			   priority ? "P":"N", 	
                           sangomabc_event_id_name(event->event_id), 
                           event->event_id, 
                           event->span+1, 
                           event->chan+1, 
                           event->release_cause, 
                           event->call_setup_id, 
                           event->fseqno);
}


static int create_conn_socket(sangomabc_connection_t *mcon, char *local_ip, int local_port, char *ip, int port)
{
	int rc;
	struct hostent *result, *local_result;
	char buf[512], local_buf[512];
	int err = 0, local_err = 0;

	memset(&mcon->remote_hp, 0, sizeof(mcon->remote_hp));
	memset(&mcon->local_hp, 0, sizeof(mcon->local_hp));
#ifdef HAVE_NETINET_SCTP_H
	zap_log(ZAP_LOG_DEBUG, "Creating SCTP socket L=%s:%d R=%s:%d\n", 
			local_ip, local_port, ip, port);
	mcon->socket = socket(AF_INET, SOCK_SEQPACKET, IPPROTO_SCTP);
#else
	zap_log(ZAP_LOG_DEBUG, "Creating UDP socket L=%s:%d R=%s:%d\n", 
			local_ip, local_port, ip, port);
	mcon->socket = socket(AF_INET, SOCK_DGRAM, 0);
#endif
 
	if (mcon->socket >= 0) {
		int flag;

		flag = 1;
#ifdef HAVE_GETHOSTBYNAME_R_FIVE
		gethostbyname_r(ip, &mcon->remote_hp, buf, sizeof(buf), &err);
		gethostbyname_r(local_ip, &mcon->local_hp, local_buf, sizeof(local_buf), &local_err);
		if (!err && !local_err) {
#else
		gethostbyname_r(ip, &mcon->remote_hp, buf, sizeof(buf), &result, &err);
		gethostbyname_r(local_ip, &mcon->local_hp, local_buf, sizeof(local_buf), &local_result, &local_err);
		if (result && local_result) {
#endif
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
				rc=listen(mcon->socket, 100);
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

int sangomabc_connection_close(sangomabc_connection_t *mcon)
{
	if (mcon->socket > -1) {
		close(mcon->socket);
	}

	if (mcon->mutex) {
		zap_mutex_lock(mcon->mutex);
		zap_mutex_unlock(mcon->mutex);
		zap_mutex_destroy(&mcon->mutex);
	}
	memset(mcon, 0, sizeof(*mcon));
	mcon->socket = -1;

	return 0;
}

int sangomabc_connection_open(sangomabc_connection_t *mcon, char *local_ip, int local_port, char *ip, int port)
{
	create_conn_socket(mcon, local_ip, local_port, ip, port);
	return mcon->socket;
}


int sangomabc_exec_command(sangomabc_connection_t *mcon, int span, int chan, int id, int cmd, int cause, int flags)
{
    sangomabc_short_event_t oevent;
    int retry = 5;

    sangomabc_event_init(&oevent, cmd, chan, span);
    oevent.release_cause = cause;
	oevent.flags = flags;

	if (cmd == SIGBOOST_EVENT_SYSTEM_RESTART || cmd == SIGBOOST_EVENT_SYSTEM_RESTART_ACK) {
		mcon->rxseq_reset = 1;
		mcon->txseq = 0;
		mcon->rxseq = 0;
		mcon->txwindow = 0;
	}

    if (id >= 0) {
        oevent.call_setup_id = id;
    }

    while (sangomabc_connection_write(mcon, (sangomabc_event_t*)&oevent) <= 0) {
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


int sangomabc_exec_commandp(sangomabc_connection_t *pcon, int span, int chan, int id, int cmd, int cause)
{
    sangomabc_short_event_t oevent;
    int retry = 5;

    sangomabc_event_init(&oevent, cmd, chan, span);
    oevent.release_cause = cause;

    if (id >= 0) {
        oevent.call_setup_id = id;
    }

    while (sangomabc_connection_writep(pcon, (sangomabc_event_t*)&oevent) <= 0) {
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

sangomabc_event_t *__sangomabc_connection_read(sangomabc_connection_t *mcon, int iteration, const char *file, const char *func, int line)
{
	unsigned int fromlen = sizeof(struct sockaddr_in);
	int bytes = 0;
	int msg_ok = 0;

	bytes = recvfrom(mcon->socket, &mcon->event, sizeof(mcon->event), MSG_DONTWAIT, 
					 (struct sockaddr *) &mcon->local_addr, &fromlen);

	if (bytes <= 0) {
		return NULL;
	}

    if (mcon->event.version != SIGBOOST_VERSION) {
		zap_log(ZAP_LOG_CRIT, "Invalid Boost Version %i  Expecting %i\n",mcon->event.version, SIGBOOST_VERSION);
    }   

	/* Must check for < 0 cannot rely on bytes > MIN_SIZE_... compiler issue */
	if (bytes < 0) {
		msg_ok=0;

	} else if ((bytes >= MIN_SIZE_CALLSTART_MSG) && boost_full_event(mcon->event.event_id)) {
		msg_ok=1;
		
	} else if (bytes == sizeof(sangomabc_short_event_t)) {
		msg_ok=1;

	} else {
		msg_ok=0;
	}

	if (msg_ok){

		if (sangomabc_test_flag(mcon, MSU_FLAG_DOWN)) {
			if (mcon->event.event_id != SIGBOOST_EVENT_SYSTEM_RESTART && 
				mcon->event.event_id != SIGBOOST_EVENT_SYSTEM_RESTART_ACK && 
				mcon->event.event_id != SIGBOOST_EVENT_HEARTBEAT) {
				zap_log(file, func, line, ZAP_LOG_LEVEL_WARNING, "Not reading packets when connection is down. [%s]\n", 
						sangomabc_event_id_name(mcon->event.event_id));
				return NULL;
			}
		}

		if  (boost_full_event(mcon->event.event_id)) {
			sangomabc_print_event_call(mcon, &mcon->event, 0, 0, file, func, line);
		} else {
			sangomabc_print_event_short(mcon, (sangomabc_short_event_t*)&mcon->event, 0, 0, file, func, line);
		}

#if 0
/* NC: NOT USED ANY MORE */
		if (mcon->rxseq_reset) {
			//if (mcon->event.event_id == SIGBOOST_EVENT_SYSTEM_RESTART_ACK) {
				zap_log(ZAP_LOG_DEBUG, "Rx sync ok\n");
				mcon->rxseq = mcon->event.fseqno;
				return &mcon->event;
				//}
			errno=EAGAIN;
			zap_log(ZAP_LOG_DEBUG, "Waiting for rx sync...\n");
			return NULL;
		}
#endif
		
		mcon->txwindow = mcon->txseq - mcon->event.bseqno;
		mcon->rxseq++;

#if 0
		if (mcon->rxseq != mcon->event.fseqno) {
			zap_log(ZAP_LOG_CRIT, "Invalid Sequence Number Expect=%i Rx=%i\n", mcon->rxseq, mcon->event.fseqno);
			return NULL;
		}
#endif

		return &mcon->event;
	} else {
		if (iteration == 0) {
			zap_log(ZAP_LOG_CRIT, "NC -  Invalid Event length from boost rxlen=%i evsz=%i\n", bytes, sizeof(mcon->event));
			return NULL;
		}
	}

	return NULL;
}

sangomabc_event_t *__sangomabc_connection_readp(sangomabc_connection_t *mcon, int iteration, const char *file, const char *func, int line)
{
	unsigned int fromlen = sizeof(struct sockaddr_in);
	int bytes = 0;

	bytes = recvfrom(mcon->socket, &mcon->event, sizeof(mcon->event), MSG_DONTWAIT, (struct sockaddr *) &mcon->local_addr, &fromlen);
	
	if (bytes <= 0) {
		return NULL;
	}

    if (mcon->event.version != SIGBOOST_VERSION) {
		zap_log(ZAP_LOG_CRIT, "Invalid Boost Version %i  Expecting %i\n",mcon->event.version, SIGBOOST_VERSION);
    }   

	if (bytes == sizeof(sangomabc_short_event_t)) {

		if  (boost_full_event(mcon->event.event_id)) {
			sangomabc_print_event_call(mcon, &mcon->event, 1, 0, file, func, line);
		} else {
			sangomabc_print_event_short(mcon, (sangomabc_short_event_t*)&mcon->event, 1, 0, file, func, line);
		}

		return &mcon->event;
	} else {
		if (iteration == 0) {
			zap_log(ZAP_LOG_CRIT, "Critical Error: PQ Invalid Event lenght from boost rxlen=%i evsz=%i\n", bytes, sizeof(mcon->event));
			return NULL;
		}
	}

	return NULL;
}


int __sangomabc_connection_write(sangomabc_connection_t *mcon, sangomabc_event_t *event, const char *file, const char *func, int line)
{
	int err;
	int event_size=MIN_SIZE_CALLSTART_MSG+event->isup_in_rdnis_size;
 
	if (!event || mcon->socket < 0 || !mcon->mutex) {
		zap_log(file, func, line, ZAP_LOG_LEVEL_CRIT, "Critical Error: No Event Device\n");
		return -EINVAL;
		abort();
	}

	if (event->span >= ZAP_MAX_PHYSICAL_SPANS_PER_LOGICAL_SPAN || event->chan >= ZAP_MAX_CHANNELS_PHYSICAL_SPAN ) {
		zap_log(file, func, line, ZAP_LOG_LEVEL_CRIT, "Critical Error: TX Cmd=%s Invalid Span=%i Chan=%i\n", sangomabc_event_id_name(event->event_id), event->span, event->chan);
		abort();
		return -1;
	}

	if (!boost_full_event(event->event_id)) {
		event_size=sizeof(sangomabc_short_event_t);
	}	

	if (sangomabc_test_flag(mcon, MSU_FLAG_DOWN)) {
		if (event->event_id != SIGBOOST_EVENT_SYSTEM_RESTART && 
			event->event_id != SIGBOOST_EVENT_SYSTEM_RESTART_ACK && 
			event->event_id != SIGBOOST_EVENT_HEARTBEAT) {
			zap_log(file, func, line, ZAP_LOG_LEVEL_WARNING, "Not writing packets when connection is down. [%s]\n",
					sangomabc_event_id_name(event->event_id));
			return 0;
		}
	}

	zap_mutex_lock(mcon->mutex);
	if (event->event_id == SIGBOOST_EVENT_SYSTEM_RESTART_ACK) {
		mcon->txseq=0;
		mcon->rxseq=0;
		event->fseqno=0;	
	} else {
		event->fseqno = mcon->txseq++;
	}
	event->bseqno = mcon->rxseq;
    event->version = SIGBOOST_VERSION; 
	err = sendto(mcon->socket, event, event_size, 0, (struct sockaddr *) &mcon->remote_addr, sizeof(mcon->remote_addr));

	zap_mutex_unlock(mcon->mutex);

	if (err != event_size) {
		err = -1;
		abort();
	}

	if (boost_full_event(event->event_id)) {
		sangomabc_print_event_call(mcon, event, 0, 1, file, func, line);
	} else {
		sangomabc_print_event_short(mcon, (sangomabc_short_event_t*)event, 0, 1, file, func, line);
	}

	return err;
}


int __sangomabc_connection_writep(sangomabc_connection_t *mcon, sangomabc_event_t *event, const char *file, const char *func, int line)
{
	int err;
	int event_size=sizeof(sangomabc_event_t);

	if (!event || mcon->socket < 0 || !mcon->mutex) {
		zap_log(file, func, line, ZAP_LOG_LEVEL_CRIT, "Critical Error: No Event Device\n");
		return -EINVAL;
		abort();
	}

	if (!boost_full_event(event->event_id)) {
		event_size=sizeof(sangomabc_short_event_t);
	}	

	zap_mutex_lock(mcon->mutex);
    event->version = SIGBOOST_VERSION; 
	err = sendto(mcon->socket, event, event_size, 0, (struct sockaddr *) &mcon->remote_addr, sizeof(mcon->remote_addr));
	zap_mutex_unlock(mcon->mutex);

	if (err != event_size) {
		err = -1;
		abort();
	}

	if (boost_full_event(event->event_id)) {
		sangomabc_print_event_call(mcon, event, 1, 1, file, func, line);
	} else {
		sangomabc_print_event_short(mcon, (sangomabc_short_event_t*)event, 1, 1, file, func, line);
	}

	return err;
}


void sangomabc_call_init(sangomabc_event_t *event, const char *calling, const char *called, int setup_id)
{
	memset(event, 0, sizeof(sangomabc_event_t));
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

void sangomabc_event_init(sangomabc_short_event_t *event, sangomabc_event_id_t event_id, int chan, int span)
{
	memset(event, 0, sizeof(sangomabc_short_event_t));
	event->event_id = event_id;
	event->chan = chan;
	event->span = span;
}

const char *sangomabc_event_id_name(uint32_t event_id)
{
	unsigned int x;
	const char *ret = NULL;

	for (x = 0 ; x < sizeof(sangomabc_table)/sizeof(struct sangomabc_map); x++) {
		if (sangomabc_table[x].event_id == event_id) {
			ret = sangomabc_table[x].name;
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */


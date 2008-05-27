/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
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
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthmct@yahoo.com>
 * Nenad Corbic <ncorbic@sangoma.com>
 *
 *
 * ss7boost_client.c Client for the SS7Boost Protocol
 *
 */
#include <ss7boost_client.h>
#include <switch.h>


extern unsigned int txseq;
extern unsigned int rxseq;

struct ss7boost_client_map {
	uint32_t event_id;
	char *name;
};

static struct ss7boost_client_map ss7boost_client_table[] = {
	{SIGBOOST_EVENT_CALL_START, "CALL_START"},
	{SIGBOOST_EVENT_CALL_START_ACK, "CALL_START_ACK"},
	{SIGBOOST_EVENT_CALL_START_NACK, "CALL_START_NACK"},
	{SIGBOOST_EVENT_CALL_START_NACK_ACK, "CALL_START_NACK_ACK"},
	{SIGBOOST_EVENT_CALL_ANSWERED, "CALL_ANSWERED"},
	{SIGBOOST_EVENT_CALL_STOPPED, "CALL_STOPPED"},
	{SIGBOOST_EVENT_CALL_STOPPED_ACK, "CALL_STOPPED_ACK"},
	{SIGBOOST_EVENT_SYSTEM_RESTART, "SYSTEM_RESTART"},
	{SIGBOOST_EVENT_HEARTBEAT, "HEARTBEAT"},
};



static switch_status_t create_udp_socket(ss7boost_client_connection_t *mcon, char *local_ip, int local_port, char *ip, int port)
{

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "connect %s:%d->%s:%d\n", local_ip, local_port, ip, port);

	if (switch_sockaddr_info_get(&mcon->local_addr, local_ip, SWITCH_UNSPEC, local_port, 0, mcon->pool) != SWITCH_STATUS_SUCCESS) {
		goto fail;
	}

	if (switch_sockaddr_info_get(&mcon->remote_addr, ip, SWITCH_UNSPEC, port, 0, mcon->pool) != SWITCH_STATUS_SUCCESS) {
		goto fail;
	}

	if (switch_socket_create(&mcon->socket, AF_INET, SOCK_DGRAM, 0, mcon->pool) == SWITCH_STATUS_SUCCESS) {
		if (switch_socket_bind(mcon->socket, mcon->local_addr) != SWITCH_STATUS_SUCCESS) {
			goto fail;
		}
	} else {
		goto fail;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Created boost connection %s:%d->%s:%d\n", local_ip, local_port, ip, port);
	return SWITCH_STATUS_SUCCESS;

  fail:

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failure creating boost connection %s:%d->%s:%d\n", local_ip, local_port, ip, port);
	return SWITCH_STATUS_FALSE;
}



SWITCH_DECLARE(switch_status_t) ss7boost_client_connection_close(ss7boost_client_connection_t *mcon)
{
	switch_socket_close(mcon->socket);
	mcon->socket = NULL;
	memset(mcon, 0, sizeof(*mcon));

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) ss7boost_client_connection_open(ss7boost_client_connection_t *mcon,
																char *local_ip, int local_port, char *ip, int port, switch_memory_pool_t *pool)
{
	memset(mcon, 0, sizeof(*mcon));
	mcon->pool = pool;

	if (create_udp_socket(mcon, local_ip, local_port, ip, port) == SWITCH_STATUS_SUCCESS) {
		switch_mutex_init(&mcon->mutex, SWITCH_MUTEX_NESTED, mcon->pool);
		return SWITCH_STATUS_SUCCESS;
	}

	memset(mcon, 0, sizeof(*mcon));
	return SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(switch_status_t) ss7boost_client_connection_read(ss7boost_client_connection_t *mcon, ss7boost_client_event_t **event)
{
	unsigned int fromlen = sizeof(struct sockaddr_in);
	switch_size_t bytes = 0;

	bytes = sizeof(mcon->event);

	if (switch_socket_recvfrom(mcon->local_addr, mcon->socket, 0, (void *) &mcon->event, &bytes) != SWITCH_STATUS_SUCCESS) {
		bytes = 0;
	}

	if (bytes == sizeof(mcon->event) || bytes == (sizeof(mcon->event) - sizeof(uint32_t))) {
		if (rxseq != mcon->event.seqno) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "------------------------------------------\n");
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Critical Error: Invalid Sequence Number Expect=%i Rx=%i\n", rxseq, mcon->event.seqno);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "------------------------------------------\n");
		}
		rxseq++;

		*event = &mcon->event;
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(switch_status_t) ss7boost_client_connection_write(ss7boost_client_connection_t *mcon, ss7boost_client_event_t *event)
{
	int err;
	switch_size_t len;

	if (!event) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Critical Error: No Event Device\n");
		return -EINVAL;
	}

	if (event->span < 0 || event->chan < 0 || event->span > 7 || event->chan > 30) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "------------------------------------------\n");
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Critical Error: Invalid Span=%i Chan=%i\n", event->span, event->chan);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "------------------------------------------\n");
	}
#ifdef WIN32
	//TODO set the tv with win func
#else
	gettimeofday(&event->tv, NULL);
#endif

	switch_mutex_lock(mcon->mutex);
	event->seqno = txseq++;
	len = sizeof(*event);
	if (switch_socket_sendto(mcon->socket, mcon->remote_addr, 0, (void *) event, &len) != SWITCH_STATUS_SUCCESS) {
		err = -1;
	}
	switch_mutex_unlock(mcon->mutex);

	if (len != sizeof(ss7boost_client_event_t)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Write Error: [%d][%d][%s]\n", mcon->socket, errno, strerror(errno));
		err = -1;
	}


	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
					  "\nTX EVENT\n"
					  "===================================\n"
					  "       tType: %s (%0x HEX)\n"
					  "       tSpan: [%d]\n"
					  "       tChan: [%d]\n"
					  "  tCalledNum: %s\n"
					  " tCallingNum: %s\n"
					  "      tCause: %d\n"
					  "  tInterface: [w%dg%d]\n"
					  "   tEvent ID: [%d]\n"
					  "   tSetup ID: [%d]\n"
					  "        tSeq: [%d]\n"
					  "===================================\n"
					  "\n",
					  ss7boost_client_event_id_name(event->event_id),
					  event->event_id,
					  event->span + 1,
					  event->chan + 1,
					  (event->called_number_digits_count ? (char *) event->called_number_digits : "N/A"),
					  (event->calling_number_digits_count ? (char *) event->calling_number_digits : "N/A"),
					  event->release_cause, event->span + 1, event->chan + 1, event->event_id, event->call_setup_id, event->seqno);


	return err ? SWITCH_STATUS_FALSE : SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(void) ss7boost_client_call_init(ss7boost_client_event_t *event, char *calling, char *called, int setup_id)
{
	memset(event, 0, sizeof(ss7boost_client_event_t));
	event->event_id = SIGBOOST_EVENT_CALL_START;

	if (calling) {
		strncpy((char *) event->calling_number_digits, calling, sizeof(event->calling_number_digits) - 1);
		event->calling_number_digits_count = strlen(calling);
	}

	if (called) {
		strncpy((char *) event->called_number_digits, called, sizeof(event->called_number_digits) - 1);
		event->called_number_digits_count = strlen(called);
	}

	event->call_setup_id = setup_id;

}

SWITCH_DECLARE(void) ss7boost_client_event_init(ss7boost_client_event_t *event, ss7boost_client_event_id_t event_id, int chan, int span)
{
	memset(event, 0, sizeof(ss7boost_client_event_t));
	event->event_id = event_id;
	event->chan = chan;
	event->span = span;
}

SWITCH_DECLARE(char *) ss7boost_client_event_id_name(uint32_t event_id)
{
	int x;
	char *ret = NULL;

	for (x = 0; x < sizeof(ss7boost_client_table) / sizeof(struct ss7boost_client_map); x++) {
		if (ss7boost_client_table[x].event_id == event_id) {
			ret = ss7boost_client_table[x].name;
			break;
		}
	}

	return ret;
}

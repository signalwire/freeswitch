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
 * ss7boost_client.h Client for the SS7Boost Protocol
 *
 */
#ifndef _SS7BOOST_CLIENT_H
#define _SS7BOOST_CLIENT_H

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
#include <arpa/inet.h>
#include <stdarg.h>
#include <netdb.h>
#include <sigboost.h>
#include <pthread.h>
#include <sys/time.h>
#include <switch.h>



#define ss7boost_client_test_flag(p,flag) 		({ \
					((p)->flags & (flag)); \
					})

#define ss7boost_client_set_flag(p,flag) 		do { \
					((p)->flags |= (flag)); \
					} while (0)

#define ss7boost_client_clear_flag(p,flag) 		do { \
					((p)->flags &= ~(flag)); \
					} while (0)

#define ss7boost_client_copy_flags(dest,src,flagz)	do { \
					(dest)->flags &= ~(flagz); \
					(dest)->flags |= ((src)->flags & (flagz)); \
					} while (0)

typedef t_sigboost ss7boost_client_event_t;
typedef uint32_t ss7boost_client_event_id_t;

struct ss7boost_client_connection {
	switch_socket_t *socket;
	switch_sockaddr_t *local_addr;
	switch_sockaddr_t *remote_addr;
	ss7boost_client_event_t event;
	unsigned int flags;
	switch_mutex_t *mutex;
	switch_memory_pool_t *pool;
};

typedef enum {
	MSU_FLAG_EVENT = (1 << 0)
} ss7boost_client_flag_t;

typedef struct ss7boost_client_connection ss7boost_client_connection_t;

SWITCH_DECLARE(switch_status_t) ss7boost_client_connection_close(ss7boost_client_connection_t * mcon);
SWITCH_DECLARE(switch_status_t) ss7boost_client_connection_open(ss7boost_client_connection_t * mcon,
																char *local_ip, int local_port, char *ip, int port, switch_memory_pool_t * pool);
SWITCH_DECLARE(switch_status_t) ss7boost_client_connection_read(ss7boost_client_connection_t * mcon, ss7boost_client_event_t ** event);
SWITCH_DECLARE(switch_status_t) ss7boost_client_connection_write(ss7boost_client_connection_t * mcon, ss7boost_client_event_t * event);
SWITCH_DECLARE(void) ss7boost_client_event_init(ss7boost_client_event_t * event, ss7boost_client_event_id_t event_id, int chan, int span);
SWITCH_DECLARE(void) ss7boost_client_call_init(ss7boost_client_event_t * event, char *calling, char *called, int setup_id);
SWITCH_DECLARE(char *) ss7boost_client_event_id_name(uint32_t event_id);

#endif

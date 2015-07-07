/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthm@freeswitch.org>
 * Andrew Thompson <andrew@hijacked.us>
 * Rob Charlton <rob.charlton@savageminds.com>
 *
 *
 * ei_helpers.c -- helper functions for ei
 *
 */
#include <switch.h>
#include <ei.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>

#include "mod_erlang_event.h"

/* Stolen from code added to ei in R12B-5.
 * Since not everyone has this version yet;
 * provide our own version. 
 * */

#define put8(s,n) do { \
	(s)[0] = (char)((n) & 0xff); \
	(s) += 1; \
} while (0)

#define put32be(s,n) do {  \
	(s)[0] = ((n) >>  24) & 0xff; \
	(s)[1] = ((n) >>  16) & 0xff; \
	(s)[2] = ((n) >>  8) & 0xff;  \
	(s)[3] = (n) & 0xff; \
	(s) += 4; \
} while (0)


void ei_link(listener_t *listener, erlang_pid * from, erlang_pid * to)
{
	char msgbuf[2048];
	char *s;
	int index = 0;
	switch_socket_t *sock = NULL;
	switch_os_sock_put(&sock, &listener->sockdes, listener->pool);

	index = 5;					/* max sizes: */
	ei_encode_version(msgbuf, &index);	/*   1 */
	ei_encode_tuple_header(msgbuf, &index, 3);
	ei_encode_long(msgbuf, &index, ERL_LINK);
	ei_encode_pid(msgbuf, &index, from);	/* 268 */
	ei_encode_pid(msgbuf, &index, to);	/* 268 */

	/* 5 byte header missing */
	s = msgbuf;
	put32be(s, index - 4);		/*   4 */
	put8(s, ERL_PASS_THROUGH);	/*   1 */
	/* sum:  542 */

	switch_mutex_lock(listener->sock_mutex);
	if (switch_socket_send(sock, msgbuf, (switch_size_t *) &index)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Failed to link to process on %s\n", listener->peer_nodename);
	}
	switch_mutex_unlock(listener->sock_mutex);
}

void ei_encode_switch_event_headers(ei_x_buff * ebuf, switch_event_t *event)
{
	int i;
	char *uuid = switch_event_get_header(event, "unique-id");

	switch_event_header_t *hp;

	for (i = 0, hp = event->headers; hp; hp = hp->next, i++);

	if (event->body)
		i++;

	ei_x_encode_list_header(ebuf, i + 1);

	if (uuid) {
		_ei_x_encode_string(ebuf, switch_event_get_header(event, "unique-id"));
	} else {
		ei_x_encode_atom(ebuf, "undefined");
	}

	for (hp = event->headers; hp; hp = hp->next) {
		ei_x_encode_tuple_header(ebuf, 2);
		_ei_x_encode_string(ebuf, hp->name);
		switch_url_decode(hp->value);
		_ei_x_encode_string(ebuf, hp->value);
	}

	if (event->body) {
		ei_x_encode_tuple_header(ebuf, 2);
		_ei_x_encode_string(ebuf, "body");
		_ei_x_encode_string(ebuf, event->body);
	}

	ei_x_encode_empty_list(ebuf);
}


void ei_encode_switch_event_tag(ei_x_buff * ebuf, switch_event_t *event, char *tag)
{

	ei_x_encode_tuple_header(ebuf, 2);
	ei_x_encode_atom(ebuf, tag);
	ei_encode_switch_event_headers(ebuf, event);
}

/* function to make rpc call to remote node to retrieve a pid - 
   calls module:function(Ref). The response comes back as
   {rex, {Ref, Pid}}
 */
int ei_pid_from_rpc(struct ei_cnode_s *ec, int sockfd, erlang_ref * ref, char *module, char *function)
{
	ei_x_buff buf;
	ei_x_new(&buf);
	ei_x_encode_list_header(&buf, 1);
	ei_x_encode_ref(&buf, ref);
	ei_x_encode_empty_list(&buf);

	ei_rpc_to(ec, sockfd, module, function, buf.buff, buf.index);
	ei_x_free(&buf);

	return 0;
}

/* function to spawn a process on a remote node */
int ei_spawn(struct ei_cnode_s *ec, int sockfd, erlang_ref * ref, char *module, char *function, int argc, char **argv)
{
	int i;
	ei_x_buff buf;
	ei_x_new_with_version(&buf);

	ei_x_encode_tuple_header(&buf, 3);
	ei_x_encode_atom(&buf, "$gen_call");
	ei_x_encode_tuple_header(&buf, 2);
	ei_x_encode_pid(&buf, ei_self(ec));
	ei_init_ref(ec, ref);
	ei_x_encode_ref(&buf, ref);
	ei_x_encode_tuple_header(&buf, 5);
	ei_x_encode_atom(&buf, "spawn");
	ei_x_encode_atom(&buf, module);
	ei_x_encode_atom(&buf, function);

	/* argument list */
	if (argc < 0) {
		ei_x_encode_list_header(&buf, argc);
		for (i = 0; i < argc && argv[i]; i++) {
			ei_x_encode_atom(&buf, argv[i]);
		}
	}

	ei_x_encode_empty_list(&buf);

	/*if (i != argc - 1) { */
	/* horked argument list */
	/*} */

	ei_x_encode_pid(&buf, ei_self(ec));	/* should really be a valid group leader */

#ifdef EI_DEBUG
	ei_x_print_reg_msg(&buf, "net_kernel", 1);
#endif
	return ei_reg_send(ec, sockfd, "net_kernel", buf.buff, buf.index);

}


/* stolen from erts/emulator/beam/erl_term.h */
#define _REF_NUM_SIZE    18
#define MAX_REFERENCE    (1 << _REF_NUM_SIZE)

/* function to fill in an erlang reference struct */
void ei_init_ref(ei_cnode * ec, erlang_ref * ref)
{
	memset(ref, 0, sizeof(*ref));	/* zero out the struct */
	snprintf(ref->node, MAXATOMLEN, "%s", ec->thisnodename);

	switch_mutex_lock(globals.ref_mutex);
	globals.reference0++;
	if (globals.reference0 >= MAX_REFERENCE) {
		globals.reference0 = 0;
		globals.reference1++;
		if (globals.reference1 == 0) {
			globals.reference2++;
		}
	}

	ref->n[0] = globals.reference0;
	ref->n[1] = globals.reference1;
	ref->n[2] = globals.reference2;

	switch_mutex_unlock(globals.ref_mutex);

	ref->creation = 1;			/* why is this 1 */
	ref->len = 3;				/* why is this 3 */
}


void ei_x_print_reg_msg(ei_x_buff * buf, char *dest, int send)
{
	char *mbuf = NULL;
	int i = 1;

	ei_s_print_term(&mbuf, buf->buff, &i);

	if (send) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Sending %s to %s\n", mbuf, dest);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Received %s from %s\n", mbuf, dest);
	}
	free(mbuf);
}


void ei_x_print_msg(ei_x_buff * buf, erlang_pid * pid, int send)
{
	char *pbuf = NULL;
	int i = 0;
	ei_x_buff pidbuf;

	ei_x_new(&pidbuf);
	ei_x_encode_pid(&pidbuf, pid);

	ei_s_print_term(&pbuf, pidbuf.buff, &i);

	ei_x_print_reg_msg(buf, pbuf, send);
	free(pbuf);
}


int ei_sendto(ei_cnode * ec, int fd, struct erlang_process *process, ei_x_buff * buf)
{
	int ret;
	if (process->type == ERLANG_PID) {
		ret = ei_send(fd, &process->pid, buf->buff, buf->index);
#ifdef EI_DEBUG
		ei_x_print_msg(buf, &process->pid, 1);
#endif
	} else if (process->type == ERLANG_REG_PROCESS) {
		ret = ei_reg_send(ec, fd, process->reg_name, buf->buff, buf->index);
#ifdef EI_DEBUG
		ei_x_print_reg_msg(buf, process->reg_name, 1);
#endif
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid process type!\n");
		/* wuh-oh */
		ret = -1;
	}

	return ret;
}


/* convert an erlang reference to some kind of hashed string so we can store it as a hash key */
void ei_hash_ref(erlang_ref * ref, char *output)
{
	/* very lazy */
	sprintf(output, "%d.%d.%d@%s", ref->n[0], ref->n[1], ref->n[2], ref->node);
}


int ei_compare_pids(erlang_pid * pid1, erlang_pid * pid2)
{
	if ((!strcmp(pid1->node, pid2->node)) && pid1->creation == pid2->creation && pid1->num == pid2->num && pid1->serial == pid2->serial) {
		return 0;
	} else {
		return 1;
	}
}


int ei_decode_string_or_binary(char *buf, int *index, int maxlen, char *dst)
{
	int type, size, res;
	long len;

	ei_get_type(buf, index, &type, &size);

	if (type == ERL_NIL_EXT || size == 0) {
		dst[0] = '\0';
		return 0;
	}

	if (type != ERL_STRING_EXT && type != ERL_BINARY_EXT) {
		return -1;
	} else if (size > maxlen) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Requested decoding of %s with size %d into a buffer of size %d\n",
						  type == ERL_BINARY_EXT ? "binary" : "string", size, maxlen);
		return -1;
	} else if (type == ERL_BINARY_EXT) {
		res = ei_decode_binary(buf, index, dst, &len);
		dst[len] = '\0';		/* binaries aren't null terminated */
	} else {
		res = ei_decode_string(buf, index, dst);
	}

	return res;
}


switch_status_t initialise_ei(struct ei_cnode_s *ec)
{
	char thisnodename[MAXNODELEN + 1];
	char thisalivename[MAXNODELEN + 1];
	char *atsign;

	if (zstr(listen_list.hostname) || !strncasecmp(prefs.ip, "0.0.0.0", 7) || !strncasecmp(prefs.ip, "::", 2)) {
		listen_list.hostname=(char *) switch_core_get_hostname();
	}
	if (strlen(listen_list.hostname) > EI_MAXHOSTNAMELEN) {
		*(listen_list.hostname+EI_MAXHOSTNAMELEN) = '\0';
	}

	/* copy the prefs.nodename into something we can modify */
	strncpy(thisalivename, prefs.nodename, MAXNODELEN);

	if ((atsign = strchr(thisalivename, '@'))) {
		/* we got a qualified node name, don't guess the host/domain */
		snprintf(thisnodename, MAXNODELEN + 1, "%s", prefs.nodename);
		/* truncate the alivename at the @ */
		*atsign = '\0';
	} else {
		if (prefs.shortname) {
			char *off;
			if ((off = strchr(listen_list.hostname, '.'))) {
				*off = '\0';
			}

		}
		snprintf(thisnodename, MAXNODELEN + 1, "%s@%s", prefs.nodename, listen_list.hostname);
	}


	/* init the ei stuff */
	if (ei_connect_xinit(ec, listen_list.hostname, thisalivename, thisnodename, (Erl_IpAddr) listen_list.addr, prefs.cookie, 0) < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to init ei connection\n");
		return SWITCH_STATUS_FALSE;
	}

	return SWITCH_STATUS_SUCCESS;
}

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

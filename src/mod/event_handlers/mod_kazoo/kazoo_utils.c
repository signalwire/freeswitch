/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
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
 * Karl Anderson <karl@2600hz.com>
 *
 * Original from mod_erlang_event.
 * ei_helpers.c -- helper functions for ei
 *
 */
#include <switch.h>
#include <ei.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include "mod_kazoo.h"

/* Stolen from code added to ei in R12B-5.
 * Since not everyone has this version yet;
 * provide our own version.
 * */

#define put8(s,n) do {							\
        (s)[0] = (char)((n) & 0xff);			\
        (s) += 1;								\
	} while (0)

#define put32be(s,n) do {						\
        (s)[0] = ((n) >>  24) & 0xff;			\
        (s)[1] = ((n) >>  16) & 0xff;			\
        (s)[2] = ((n) >>  8) & 0xff;			\
        (s)[3] = (n) & 0xff;					\
        (s) += 4;								\
	} while (0)

#ifdef EI_DEBUG
static void ei_x_print_reg_msg(ei_x_buff *buf, char *dest, int send) {
    char *mbuf = NULL;
    int i = 1;

    ei_s_print_term(&mbuf, buf->buff, &i);

    if (send) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Encoded term %s to '%s'\n", mbuf, dest);
    } else {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Decoded term %s for '%s'\n", mbuf, dest);
    }

    free(mbuf);
}

static void ei_x_print_msg(ei_x_buff *buf, erlang_pid *pid, int send) {
    char *pbuf = NULL;
    int i = 0;
    ei_x_buff pidbuf;

    ei_x_new(&pidbuf);
    ei_x_encode_pid(&pidbuf, pid);

    ei_s_print_term(&pbuf, pidbuf.buff, &i);

    ei_x_print_reg_msg(buf, pbuf, send);
    free(pbuf);
}
#endif

void ei_encode_switch_event_headers(ei_x_buff *ebuf, switch_event_t *event) {
    switch_event_header_t *hp;
    char *uuid = switch_event_get_header(event, "unique-id");
    int i;

    for (i = 0, hp = event->headers; hp; hp = hp->next, i++);

    if (event->body)
        i++;

    ei_x_encode_list_header(ebuf, i + 1);

    if (uuid) {
		char *unique_id = switch_event_get_header(event, "unique-id");
		ei_x_encode_binary(ebuf, unique_id, strlen(unique_id));
    } else {
        ei_x_encode_atom(ebuf, "undefined");
    }

    for (hp = event->headers; hp; hp = hp->next) {
        ei_x_encode_tuple_header(ebuf, 2);
        ei_x_encode_binary(ebuf, hp->name, strlen(hp->name));
        switch_url_decode(hp->value);
        ei_x_encode_binary(ebuf, hp->value, strlen(hp->value));
    }

    if (event->body) {
        ei_x_encode_tuple_header(ebuf, 2);
        ei_x_encode_binary(ebuf, "body", strlen("body"));
        ei_x_encode_binary(ebuf, event->body, strlen(event->body));
    }

    ei_x_encode_empty_list(ebuf);
}

void close_socket(switch_socket_t ** sock) {
	if (*sock) {
		switch_socket_shutdown(*sock, SWITCH_SHUTDOWN_READWRITE);
		switch_socket_close(*sock);
		*sock = NULL;
	}
}

void close_socketfd(int *sockfd) {
	if (*sockfd) {
		shutdown(*sockfd, SHUT_RDWR);
		close(*sockfd);
	}
}

switch_socket_t *create_socket_with_port(switch_memory_pool_t *pool, switch_port_t port) {
	switch_sockaddr_t *sa;
	switch_socket_t *socket;

	if(switch_sockaddr_info_get(&sa, globals.ip, SWITCH_UNSPEC, port, 0, pool)) {
		return NULL;
	}

	if (switch_socket_create(&socket, switch_sockaddr_get_family(sa), SOCK_STREAM, SWITCH_PROTO_TCP, pool)) {
		return NULL;
	}

	if (switch_socket_opt_set(socket, SWITCH_SO_REUSEADDR, 1)) {
		return NULL;
	}

	if (switch_socket_bind(socket, sa)) {
		return NULL;
	}

	if (switch_socket_listen(socket, 5)){
		return NULL;
	}

	//	if (globals.nat_map && switch_nat_get_type()) {
	//		switch_nat_add_mapping(port, SWITCH_NAT_TCP, NULL, SWITCH_FALSE);
	//	}

	return socket;
}

switch_socket_t *create_socket(switch_memory_pool_t *pool) {
	return create_socket_with_port(pool, 0);

}

switch_status_t create_ei_cnode(const char *ip_addr, const char *name, struct ei_cnode_s *ei_cnode) {
    struct hostent *nodehost;
    char hostname[EI_MAXHOSTNAMELEN + 1] = "";
    char nodename[MAXNODELEN + 1];
    char cnodename[EI_MAXALIVELEN + 1];
    //EI_MAX_COOKIE_SIZE+1
    char *atsign;

    /* copy the erlang interface nodename into something we can modify */
    strncpy(cnodename, name, EI_MAXALIVELEN);

    if ((atsign = strchr(cnodename, '@'))) {
        /* we got a qualified node name, don't guess the host/domain */
        snprintf(nodename, MAXNODELEN + 1, "%s", globals.ei_nodename);
        /* truncate the alivename at the @ */
        *atsign = '\0';
    } else {
        if ((nodehost = gethostbyaddr(ip_addr, sizeof (ip_addr), AF_INET))) {
            memcpy(hostname, nodehost->h_name, EI_MAXHOSTNAMELEN);
        }

        if (zstr_buf(hostname) || !strncasecmp(globals.ip, "0.0.0.0", 7)) {
            gethostname(hostname, EI_MAXHOSTNAMELEN);
        }

        snprintf(nodename, MAXNODELEN + 1, "%s@%s", globals.ei_nodename, hostname);
    }

	if (globals.ei_shortname) {
		char *off;
		if ((off = strchr(nodename, '.'))) {
			*off = '\0';
		}
	}

    /* init the ec stuff */
    if (ei_connect_xinit(ei_cnode, hostname, cnodename, nodename, (Erl_IpAddr) ip_addr, globals.ei_cookie, 0) < 0) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to initialize the erlang interface connection structure\n");
        return SWITCH_STATUS_FALSE;
    }

    return SWITCH_STATUS_SUCCESS;
}

switch_status_t ei_compare_pids(const erlang_pid *pid1, const erlang_pid *pid2) {
    if ((!strcmp(pid1->node, pid2->node))
		&& pid1->creation == pid2->creation
		&& pid1->num == pid2->num
		&& pid1->serial == pid2->serial) {
        return SWITCH_STATUS_SUCCESS;
    } else {
        return SWITCH_STATUS_FALSE;
    }
}

void ei_link(ei_node_t *ei_node, erlang_pid * from, erlang_pid * to) {
    char msgbuf[2048];
    char *s;
    int index = 0;

    index = 5; /* max sizes: */
    ei_encode_version(msgbuf, &index); /*   1 */
    ei_encode_tuple_header(msgbuf, &index, 3);
    ei_encode_long(msgbuf, &index, ERL_LINK);
    ei_encode_pid(msgbuf, &index, from); /* 268 */
    ei_encode_pid(msgbuf, &index, to); /* 268 */

    /* 5 byte header missing */
    s = msgbuf;
    put32be(s, index - 4); /*   4 */
    put8(s, ERL_PASS_THROUGH); /*   1 */
    /* sum:  542 */

    if (write(ei_node->nodefd, msgbuf, index) == -1) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Failed to link to process on %s\n", ei_node->peer_nodename);
    }
}

void ei_encode_switch_event(ei_x_buff *ebuf, switch_event_t *event) {
    ei_x_encode_tuple_header(ebuf, 2);
    ei_x_encode_atom(ebuf, "event");
    ei_encode_switch_event_headers(ebuf, event);
}

int ei_helper_send(ei_node_t *ei_node, erlang_pid *to, ei_x_buff *buf) {
    int ret = 0;

    if (ei_node->nodefd) {
#ifdef EI_DEBUG
		ei_x_print_msg(buf, to, 1);
#endif
        ret = ei_send(ei_node->nodefd, to, buf->buff, buf->index);
    }

    return ret;
}

int ei_decode_atom_safe(char *buf, int *index, char *dst) {
    int type, size;

    ei_get_type(buf, index, &type, &size);

	if (type != ERL_ATOM_EXT) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unexpected erlang term type %d (size %d), needed atom\n", type, size);
        return -1;
	} else if (size > MAXATOMLEN) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Requested decoding of atom with size %d into a buffer of size %d\n", size, MAXATOMLEN);
        return -1;
	} else {
		return ei_decode_atom(buf, index, dst);
	}
}

int ei_decode_string_or_binary(char *buf, int *index, char **dst) {
    int type, size, res;
    long len;

    ei_get_type(buf, index, &type, &size);

    if (type != ERL_STRING_EXT && type != ERL_BINARY_EXT && type != ERL_NIL_EXT) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unexpected erlang term type %d (size %d), needed binary or string\n", type, size);
        return -1;
    }

	*dst = malloc(size + 1);

	if (type == ERL_NIL_EXT) {
		res = 0;
		**dst = '\0';
	} else if (type == ERL_BINARY_EXT) {
        res = ei_decode_binary(buf, index, *dst, &len);
        (*dst)[len] = '\0';
    } else {
        res = ei_decode_string(buf, index, *dst);
    }

    return res;
}

int ei_decode_string_or_binary_limited(char *buf, int *index, int maxsize, char *dst) {
    int type, size, res;
    long len;

    ei_get_type(buf, index, &type, &size);

    if (type != ERL_STRING_EXT && type != ERL_BINARY_EXT && type != ERL_NIL_EXT) {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unexpected erlang term type %d (size %d), needed binary or string\n", type, size);
        return -1;
    }

	if (size > maxsize) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Requested decoding of %s with size %d into a buffer of size %d\n",
						  type == ERL_BINARY_EXT ? "binary" : "string", size, maxsize);
		return -1;
	}

	if (type == ERL_NIL_EXT) {
		res = 0;
		*dst = '\0';
	} else if (type == ERL_BINARY_EXT) {
        res = ei_decode_binary(buf, index, dst, &len);
        dst[len] = '\0'; /* binaries aren't null terminated */
    } else {
        res = ei_decode_string(buf, index, dst);
    }

    return res;
}

switch_hash_t *create_default_filter() {
	switch_hash_t *filter;

	switch_core_hash_init(&filter);

	switch_core_hash_insert(filter, "Acquired-UUID", "1");
	switch_core_hash_insert(filter, "action", "1");
	switch_core_hash_insert(filter, "Action", "1");
	switch_core_hash_insert(filter, "alt_event_type", "1");
	switch_core_hash_insert(filter, "Answer-State", "1");
	switch_core_hash_insert(filter, "Application", "1");
	switch_core_hash_insert(filter, "Application-Data", "1");
	switch_core_hash_insert(filter, "Application-Name", "1");
	switch_core_hash_insert(filter, "Application-Response", "1");
	switch_core_hash_insert(filter, "att_xfer_replaced_by", "1");
	switch_core_hash_insert(filter, "Auth-Method", "1");
	switch_core_hash_insert(filter, "Auth-Realm", "1");
	switch_core_hash_insert(filter, "Auth-User", "1");
	switch_core_hash_insert(filter, "Bridge-A-Unique-ID", "1");
	switch_core_hash_insert(filter, "Bridge-B-Unique-ID", "1");
	switch_core_hash_insert(filter, "Call-Direction", "1");
	switch_core_hash_insert(filter, "Caller-Callee-ID-Name", "1");
	switch_core_hash_insert(filter, "Caller-Callee-ID-Number", "1");
	switch_core_hash_insert(filter, "Caller-Caller-ID-Name", "1");
	switch_core_hash_insert(filter, "Caller-Caller-ID-Number", "1");
	switch_core_hash_insert(filter, "Caller-Context", "1");
	switch_core_hash_insert(filter, "Caller-Controls", "1");
	switch_core_hash_insert(filter, "Caller-Destination-Number", "1");
	switch_core_hash_insert(filter, "Caller-Dialplan", "1");
	switch_core_hash_insert(filter, "Caller-Network-Addr", "1");
	switch_core_hash_insert(filter, "Caller-Unique-ID", "1");
	switch_core_hash_insert(filter, "Call-ID", "1");
	switch_core_hash_insert(filter, "Channel-Call-State", "1");
	switch_core_hash_insert(filter, "Channel-Call-UUID", "1");
	switch_core_hash_insert(filter, "Channel-Presence-ID", "1");
	switch_core_hash_insert(filter, "Channel-State", "1");
	switch_core_hash_insert(filter, "Chat-Permissions", "1");
	switch_core_hash_insert(filter, "Conference-Name", "1");
	switch_core_hash_insert(filter, "Conference-Profile-Name", "1");
	switch_core_hash_insert(filter, "Conference-Unique-ID", "1");
	switch_core_hash_insert(filter, "contact", "1");
	switch_core_hash_insert(filter, "Detected-Tone", "1");
	switch_core_hash_insert(filter, "dialog_state", "1");
	switch_core_hash_insert(filter, "direction", "1");
	switch_core_hash_insert(filter, "Distributed-From", "1");
	switch_core_hash_insert(filter, "DTMF-Digit", "1");
	switch_core_hash_insert(filter, "DTMF-Duration", "1");
	switch_core_hash_insert(filter, "Event-Date-Timestamp", "1");
	switch_core_hash_insert(filter, "Event-Name", "1");
	switch_core_hash_insert(filter, "Event-Subclass", "1");
	switch_core_hash_insert(filter, "expires", "1");
	switch_core_hash_insert(filter, "Expires", "1");
	switch_core_hash_insert(filter, "Ext-SIP-IP", "1");
	switch_core_hash_insert(filter, "File", "1");
	switch_core_hash_insert(filter, "FreeSWITCH-Hostname", "1");
	switch_core_hash_insert(filter, "from", "1");
	switch_core_hash_insert(filter, "Hunt-Destination-Number", "1");
	switch_core_hash_insert(filter, "ip", "1");
	switch_core_hash_insert(filter, "Message-Account", "1");
	switch_core_hash_insert(filter, "metadata", "1");
	switch_core_hash_insert(filter, "old_node_channel_uuid", "1");
	switch_core_hash_insert(filter, "Other-Leg-Callee-ID-Name", "1");
	switch_core_hash_insert(filter, "Other-Leg-Callee-ID-Number", "1");
	switch_core_hash_insert(filter, "Other-Leg-Caller-ID-Name", "1");
	switch_core_hash_insert(filter, "Other-Leg-Caller-ID-Number", "1");
	switch_core_hash_insert(filter, "Other-Leg-Destination-Number", "1");
	switch_core_hash_insert(filter, "Other-Leg-Direction", "1");
	switch_core_hash_insert(filter, "Other-Leg-Unique-ID", "1");
	switch_core_hash_insert(filter, "Other-Leg-Channel-Name", "1");
	switch_core_hash_insert(filter, "Participant-Type", "1");
	switch_core_hash_insert(filter, "Path", "1");
	switch_core_hash_insert(filter, "profile_name", "1");
	switch_core_hash_insert(filter, "Profiles", "1");
	switch_core_hash_insert(filter, "proto-specific-event-name", "1");
	switch_core_hash_insert(filter, "Raw-Application-Data", "1");
	switch_core_hash_insert(filter, "realm", "1");
	switch_core_hash_insert(filter, "Resigning-UUID", "1");
	switch_core_hash_insert(filter, "set", "1");
	switch_core_hash_insert(filter, "sip_auto_answer", "1");
	switch_core_hash_insert(filter, "sip_auth_method", "1");
	switch_core_hash_insert(filter, "sip_from_host", "1");
	switch_core_hash_insert(filter, "sip_from_user", "1");
	switch_core_hash_insert(filter, "sip_to_host", "1");
	switch_core_hash_insert(filter, "sip_to_user", "1");
	switch_core_hash_insert(filter, "sub-call-id", "1");
	switch_core_hash_insert(filter, "technology", "1");
	switch_core_hash_insert(filter, "to", "1");
	switch_core_hash_insert(filter, "Unique-ID", "1");
	switch_core_hash_insert(filter, "URL", "1");
	switch_core_hash_insert(filter, "username", "1");
	switch_core_hash_insert(filter, "variable_channel_is_moving", "1");
	switch_core_hash_insert(filter, "variable_collected_digits", "1");
	switch_core_hash_insert(filter, "variable_current_application", "1");
	switch_core_hash_insert(filter, "variable_current_application_data", "1");
	switch_core_hash_insert(filter, "variable_domain_name", "1");
	switch_core_hash_insert(filter, "variable_effective_caller_id_name", "1");
	switch_core_hash_insert(filter, "variable_effective_caller_id_number", "1");
	switch_core_hash_insert(filter, "variable_holding_uuid", "1");
	switch_core_hash_insert(filter, "variable_hold_music", "1");
	switch_core_hash_insert(filter, "variable_media_group_id", "1");
	switch_core_hash_insert(filter, "variable_originate_disposition", "1");
	switch_core_hash_insert(filter, "variable_origination_uuid", "1");
	switch_core_hash_insert(filter, "variable_playback_terminator_used", "1");
	switch_core_hash_insert(filter, "variable_presence_id", "1");
	switch_core_hash_insert(filter, "variable_record_ms", "1");
	switch_core_hash_insert(filter, "variable_recovered", "1");
	switch_core_hash_insert(filter, "variable_silence_hits_exhausted", "1");
	switch_core_hash_insert(filter, "variable_sip_auth_realm", "1");
	switch_core_hash_insert(filter, "variable_sip_from_host", "1");
	switch_core_hash_insert(filter, "variable_sip_from_user", "1");
	switch_core_hash_insert(filter, "variable_sip_from_tag", "1");
	switch_core_hash_insert(filter, "variable_sip_h_X-AUTH-IP", "1");
	switch_core_hash_insert(filter, "variable_sip_received_ip", "1");
	switch_core_hash_insert(filter, "variable_sip_to_host", "1");
	switch_core_hash_insert(filter, "variable_sip_to_user", "1");
	switch_core_hash_insert(filter, "variable_sip_to_tag", "1");
	switch_core_hash_insert(filter, "variable_sofia_profile_name", "1");
	switch_core_hash_insert(filter, "variable_transfer_history", "1");
	switch_core_hash_insert(filter, "variable_user_name", "1");
	switch_core_hash_insert(filter, "variable_endpoint_disposition", "1");
	switch_core_hash_insert(filter, "variable_originate_disposition", "1");
	switch_core_hash_insert(filter, "variable_bridge_hangup_cause", "1");
	switch_core_hash_insert(filter, "variable_hangup_cause", "1");
	switch_core_hash_insert(filter, "variable_last_bridge_proto_specific_hangup_cause", "1");
	switch_core_hash_insert(filter, "variable_proto_specific_hangup_cause", "1");
	switch_core_hash_insert(filter, "VM-Call-ID", "1");
	switch_core_hash_insert(filter, "VM-sub-call-id", "1");
	switch_core_hash_insert(filter, "whistle_application_name", "1");
	switch_core_hash_insert(filter, "whistle_application_response", "1");
	switch_core_hash_insert(filter, "whistle_event_name", "1");
	switch_core_hash_insert(filter, "sip_auto_answer_notify", "1");
	switch_core_hash_insert(filter, "eavesdrop_group", "1");
	switch_core_hash_insert(filter, "origination_caller_id_name", "1");
	switch_core_hash_insert(filter, "origination_caller_id_number", "1");
	switch_core_hash_insert(filter, "origination_callee_id_name", "1");
	switch_core_hash_insert(filter, "origination_callee_id_number", "1");
	switch_core_hash_insert(filter, "sip_auth_username", "1");
	switch_core_hash_insert(filter, "sip_auth_password", "1");
	switch_core_hash_insert(filter, "effective_caller_id_name", "1");
	switch_core_hash_insert(filter, "effective_caller_id_number", "1");
	switch_core_hash_insert(filter, "effective_callee_id_name", "1");
	switch_core_hash_insert(filter, "effective_callee_id_number", "1");

	/* Registration headers */
	switch_core_hash_insert(filter, "call-id", "1");
	switch_core_hash_insert(filter, "profile-name", "1");
	switch_core_hash_insert(filter, "from-user", "1");
	switch_core_hash_insert(filter, "from-host", "1");
	switch_core_hash_insert(filter, "presence-hosts", "1");
	switch_core_hash_insert(filter, "contact", "1");
	switch_core_hash_insert(filter, "rpid", "1");
	switch_core_hash_insert(filter, "status", "1");
	switch_core_hash_insert(filter, "expires", "1");
	switch_core_hash_insert(filter, "to-user", "1");
	switch_core_hash_insert(filter, "to-host", "1");
	switch_core_hash_insert(filter, "network-ip", "1");
	switch_core_hash_insert(filter, "network-port", "1");
	switch_core_hash_insert(filter, "username", "1");
	switch_core_hash_insert(filter, "realm", "1");
	switch_core_hash_insert(filter, "user-agent", "1");

	switch_core_hash_insert(filter, "Hangup-Cause", "1");
	switch_core_hash_insert(filter, "Unique-ID", "1");
	switch_core_hash_insert(filter, "variable_switch_r_sdp", "1");
	switch_core_hash_insert(filter, "variable_rtp_local_sdp_str", "1");
	switch_core_hash_insert(filter, "variable_sip_to_uri", "1");
	switch_core_hash_insert(filter, "variable_sip_from_uri", "1");
	switch_core_hash_insert(filter, "variable_sip_user_agent", "1");
	switch_core_hash_insert(filter, "variable_duration", "1");
	switch_core_hash_insert(filter, "variable_billsec", "1");
	switch_core_hash_insert(filter, "variable_progresssec", "1");
	switch_core_hash_insert(filter, "variable_progress_uepoch", "1");
	switch_core_hash_insert(filter, "variable_progress_media_uepoch", "1");
	switch_core_hash_insert(filter, "variable_start_uepoch", "1");
	switch_core_hash_insert(filter, "variable_digits_dialed", "1");
	switch_core_hash_insert(filter, "Member-ID", "1");
	switch_core_hash_insert(filter, "Floor", "1");
	switch_core_hash_insert(filter, "Video", "1");
	switch_core_hash_insert(filter, "Hear", "1");
	switch_core_hash_insert(filter, "Speak", "1");
	switch_core_hash_insert(filter, "Talking", "1");
	switch_core_hash_insert(filter, "Current-Energy", "1");
	switch_core_hash_insert(filter, "Energy-Level", "1");
	switch_core_hash_insert(filter, "Mute-Detect", "1");

	/* RTMP headers */
	switch_core_hash_insert(filter, "RTMP-Session-ID", "1");
	switch_core_hash_insert(filter, "RTMP-Profile", "1");
	switch_core_hash_insert(filter, "RTMP-Flash-Version", "1");
	switch_core_hash_insert(filter, "RTMP-SWF-URL", "1");
	switch_core_hash_insert(filter, "RTMP-TC-URL", "1");
	switch_core_hash_insert(filter, "RTMP-Page-URL", "1");
	switch_core_hash_insert(filter, "User", "1");
	switch_core_hash_insert(filter, "Domain", "1");

	/* Fax headers */
	switch_core_hash_insert(filter, "variable_fax_bad_rows", "1");
	switch_core_hash_insert(filter, "variable_fax_document_total_pages", "1");
	switch_core_hash_insert(filter, "variable_fax_document_transferred_pages", "1");
	switch_core_hash_insert(filter, "variable_fax_ecm_used", "1");
	switch_core_hash_insert(filter, "variable_fax_result_code", "1");
	switch_core_hash_insert(filter, "variable_fax_result_text", "1");
	switch_core_hash_insert(filter, "variable_fax_success", "1");
	switch_core_hash_insert(filter, "variable_fax_transfer_rate", "1");
	switch_core_hash_insert(filter, "variable_fax_local_station_id", "1");
	switch_core_hash_insert(filter, "variable_fax_remote_station_id", "1");
	switch_core_hash_insert(filter, "variable_fax_remote_country", "1");
	switch_core_hash_insert(filter, "variable_fax_remote_vendor", "1");
	switch_core_hash_insert(filter, "variable_fax_remote_model", "1");
	switch_core_hash_insert(filter, "variable_fax_image_resolution", "1");
	switch_core_hash_insert(filter, "variable_fax_file_image_resolution", "1");
	switch_core_hash_insert(filter, "variable_fax_image_size", "1");
	switch_core_hash_insert(filter, "variable_fax_image_pixel_size", "1");
	switch_core_hash_insert(filter, "variable_fax_file_image_pixel_size", "1");
	switch_core_hash_insert(filter, "variable_fax_longest_bad_row_run", "1");
	switch_core_hash_insert(filter, "variable_fax_encoding", "1");
	switch_core_hash_insert(filter, "variable_fax_encoding_name", "1");
	switch_core_hash_insert(filter, "variable_fax_header", "1");
	switch_core_hash_insert(filter, "variable_fax_ident", "1");
	switch_core_hash_insert(filter, "variable_fax_timezone", "1");
	switch_core_hash_insert(filter, "variable_fax_doc_id", "1");
	switch_core_hash_insert(filter, "variable_fax_doc_database", "1");

	/* Secure headers */
	/*
	  switch_core_hash_insert(filter, "variable_sdp_secure_savp_only", "1");
	  switch_core_hash_insert(filter, "variable_rtp_has_crypto", "1");
	  switch_core_hash_insert(filter, "variable_rtp_secure_media", "1");
	  switch_core_hash_insert(filter, "variable_rtp_secure_media_confirmed", "1");
	  switch_core_hash_insert(filter, "variable_rtp_secure_media_confirmed_audio", "1");
	  switch_core_hash_insert(filter, "variable_rtp_secure_media_confirmed_video", "1");
	  switch_core_hash_insert(filter, "variable_zrtp_secure_media", "1");
	  switch_core_hash_insert(filter, "variable_zrtp_secure_media_confirmed", "1");
	  switch_core_hash_insert(filter, "variable_zrtp_secure_media_confirmed_audio", "1");
	  switch_core_hash_insert(filter, "variable_zrtp_secure_media_confirmed_video", "1");
	  switch_core_hash_insert(filter, "sdp_secure_savp_only", "1");
	  switch_core_hash_insert(filter, "rtp_has_crypto", "1");
	  switch_core_hash_insert(filter, "rtp_secure_media", "1");
	  switch_core_hash_insert(filter, "rtp_secure_media_confirmed", "1");
	  switch_core_hash_insert(filter, "rtp_secure_media_confirmed_audio", "1");
	  switch_core_hash_insert(filter, "rtp_secure_media_confirmed_video", "1");
	  switch_core_hash_insert(filter, "zrtp_secure_media", "1");
	  switch_core_hash_insert(filter, "zrtp_secure_media_confirmed", "1");
	  switch_core_hash_insert(filter, "zrtp_secure_media_confirmed_audio", "1");
	  switch_core_hash_insert(filter, "zrtp_secure_media_confirmed_video", "1");
	*/

	/* Device Redirect headers */
	/*
	  switch_core_hash_insert(filter, "variable_last_bridge_hangup_cause", "1");
	  switch_core_hash_insert(filter, "variable_sip_redirected_by", "1");
	*/

	switch_core_hash_insert(filter, "intercepted_by", "1");

	// SMS
	switch_core_hash_insert(filter, "Message-ID", "1");
	switch_core_hash_insert(filter, "Delivery-Failure", "1");
	switch_core_hash_insert(filter, "Delivery-Result-Code", "1");

	return filter;
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

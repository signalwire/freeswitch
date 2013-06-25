/*
 * mod_rayo for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2013, Grasshopper
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
 * The Original Code is mod_rayo for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is Grasshopper
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Chris Rienzo <chris.rienzo@grasshopper.com>
 *
 * xmpp_streams.h -- XMPP in/out s2s and in c2s streams
 *
 */
#ifndef XMPP_STREAMS_H
#define XMPP_STREAMS_H

struct xmpp_stream;
struct xmpp_stream_context;

typedef void (* xmpp_stream_ready_callback)(struct xmpp_stream *stream);
typedef void (* xmpp_stream_recv_callback)(struct xmpp_stream *stream, iks *stanza);
typedef void (* xmpp_stream_destroy_callback)(struct xmpp_stream *stream);

extern struct xmpp_stream_context *xmpp_stream_context_create(const char *domain, const char *domain_secret, xmpp_stream_ready_callback ready, xmpp_stream_recv_callback recv, xmpp_stream_destroy_callback destroy);
extern void xmpp_stream_context_add_user(struct xmpp_stream_context *context, const char *user, const char *password);
extern void xmpp_stream_context_dump(struct xmpp_stream_context *context, switch_stream_handle_t *stream);
extern void xmpp_stream_context_destroy(struct xmpp_stream_context *context);
extern void xmpp_stream_context_send(struct xmpp_stream_context *context, const char *jid, iks *stanza);

extern switch_status_t xmpp_stream_context_listen(struct xmpp_stream_context *context, const char *addr, int port, int is_s2s, const char *acl);
extern switch_status_t xmpp_stream_context_connect(struct xmpp_stream_context *context, const char *peer_domain, const char *peer_address, int peer_port);

extern int xmpp_stream_is_s2s(struct xmpp_stream *stream);
extern int xmpp_stream_is_incoming(struct xmpp_stream *stream);
extern const char *xmpp_stream_get_jid(struct xmpp_stream *stream);
extern void xmpp_stream_set_private(struct xmpp_stream *stream, void *user_private);
extern void *xmpp_stream_get_private(struct xmpp_stream *stream);

#endif

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet
 */

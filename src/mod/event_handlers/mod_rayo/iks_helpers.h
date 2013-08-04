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
 * iks_helpers.h -- iksemel constants and helpers
 *
 */
#ifndef IKS_EXT_H
#define IKS_EXT_H

#include <iksemel.h>
#include <switch.h>

#define IKS_JABBER_SERVER_PORT 5269

#define IKS_NS_XMPP_DISCO "http://jabber.org/protocol/disco#info"
#define IKS_NS_XMPP_PING "urn:xmpp:ping"
#define IKS_NS_XMPP_STANZAS "urn:ietf:params:xml:ns:xmpp-stanzas"
#define IKS_NS_XMPP_STREAMS "http://etherx.jabber.org/streams"
#define IKS_NS_XMPP_DIALBACK "jabber:server:dialback"
#define IKS_NS_XMPP_TLS "urn:ietf:params:xml:ns:xmpp-tls"

struct xmpp_error {
	const char *name;
	const char *type;
};

#undef XMPP_ERROR
#define XMPP_ERROR(def_name, name, type) \
	extern const struct xmpp_error def_name##_val; \
	extern const struct xmpp_error *def_name;
#include "xmpp_errors.def"

/* See RFC-3920 XMPP core for error definitions */

extern iks *iks_new_presence(const char *name, const char *namespace, const char *from, const char *to);
extern iks *iks_new_error(iks *iq, const struct xmpp_error *err);
extern iks *iks_new_error_detailed(iks *iq, const struct xmpp_error *err, const char *detail_text);
extern iks *iks_new_error_detailed_printf(iks *iq, const struct xmpp_error *err, const char *detail_text_format, ...);
extern iks *iks_new_iq_result(iks *iq);
extern const char *iks_find_attrib_soft(iks *xml, const char *attrib);
extern const char *iks_find_attrib_default(iks *xml, const char *attrib, const char *def);
extern int iks_find_bool_attrib(iks *xml, const char *attrib);
extern int iks_find_int_attrib(iks *xml, const char *attrib);
extern double iks_find_decimal_attrib(iks *xml, const char *attrib);
extern const char *iks_node_type_to_string(int type);
extern const char *iks_net_error_to_string(int err);
extern iks *iks_insert_attrib_printf(iks *xml, const char *name, const char *fmt, ...);

extern char *iks_server_dialback_key(const char *secret, const char *receiving_server, const char *originating_server, const char *stream_id);

/** A function to validate attribute value */
typedef int (*iks_attrib_validation_function)(const char *);

#define ELEMENT_DECL(name) extern int VALIDATE_##name(iks *node);
#define ELEMENT(name) int VALIDATE_##name(iks *node) { int result = 1; if (!node) return 0;
#define ATTRIB(name, def, rule) result &= iks_attrib_is_##rule(iks_find_attrib_default(node, #name, #def));
#define STRING_ATTRIB(name, def, rule) result &= value_matches(iks_find_attrib_default(node, #name, #def), rule);
#define ELEMENT_END return result; }

extern int value_matches(const char *value, const char *rule);

extern int iks_attrib_is_bool(const char *value);
extern int iks_attrib_is_not_negative(const char *value);
extern int iks_attrib_is_positive(const char *value);
extern int iks_attrib_is_positive_or_neg_one(const char *value);
extern int iks_attrib_is_any(const char *value);
extern int iks_attrib_is_decimal_between_zero_and_one(const char *value);

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

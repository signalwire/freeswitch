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
 * iks_helpers.c -- iksemel helpers
 *
 */
#include "iks_helpers.h"
#include <switch.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>

#undef XMPP_ERROR
#define XMPP_ERROR(def_name, name, type) \
	const struct xmpp_error def_name##_val = { name, type }; \
	const struct xmpp_error *def_name = &def_name##_val;
#include "xmpp_errors.def"

/**
 * Create a <presence> event
 * @param name the event name
 * @param namespace the event namespace
 * @param from
 * @param to
 * @return the event XML node
 */
iks *iks_new_presence(const char *name, const char *namespace, const char *from, const char *to)
{
	iks *event = iks_new("presence");
	iks *x;
	/* iks makes copies of attrib name and value */
	iks_insert_attrib(event, "from", from);
	iks_insert_attrib(event, "to", to);
	x = iks_insert(event, name);
	if (!zstr(namespace)) {
		iks_insert_attrib(x, "xmlns", namespace);
	}
	return event;
}

/**
 * Create error response from request
 * @param req the request
 * @param from
 * @param to
 * @param err the XMPP stanza error
 * @return the error response
 */
iks *iks_new_error(iks *req, const struct xmpp_error *err)
{
	iks *response = iks_copy(req);
	iks *x;

	/* <iq> */
	iks_insert_attrib(response, "from", iks_find_attrib(req, "to"));
	iks_insert_attrib(response, "to", iks_find_attrib(req, "from"));
	iks_insert_attrib(response, "type", "error");

	/* <error> */
	x = iks_insert(response, "error");
	iks_insert_attrib(x, "type", err->type);

	/* e.g. <feature-not-implemented> */
	x = iks_insert(x, err->name);
	iks_insert_attrib(x, "xmlns", IKS_NS_XMPP_STANZAS);

	return response;
}

/**
 * Create error response from request
 * @param req the request
 * @param from
 * @param to
 * @param err the XMPP stanza error
 * @param detail_text optional text to include in message
 * @return the <iq> error response
 */
iks *iks_new_error_detailed(iks *req, const struct xmpp_error *err, const char *detail_text)
{
	iks *reply = iks_new_error(req, err);
	if (!zstr(detail_text)) {
		iks *error = iks_find(reply, "error");
		iks *text = iks_insert(error, "text");
		iks_insert_attrib(text, "xml:lang", "en");
		iks_insert_attrib(text, "xmlns", IKS_NS_XMPP_STANZAS);
		iks_insert_cdata(text, detail_text, strlen(detail_text));
	}
	return reply;
}

/**
 * Create error response from request
 * @param req the request
 * @param from
 * @param to
 * @param err the XMPP stanza error
 * @param detail_text_format format string
 * @param ...
 * @return the error response
 */
iks *iks_new_error_detailed_printf(iks *req, const struct xmpp_error *err, const char *detail_text_format, ...)
{
	iks *reply = NULL;
	char *data;
	va_list ap;
	int ret;

	va_start(ap, detail_text_format);
	ret = switch_vasprintf(&data, detail_text_format, ap);
	va_end(ap);

	if (ret == -1) {
		return NULL;
	}
	reply = iks_new_error_detailed(req, err, data);
	free(data);
	return reply;
}

/**
 * Create <iq> result response from request
 * @param iq the request
 * @return the result response
 */
iks *iks_new_iq_result(iks *iq)
{
	iks *response = iks_new("iq");
	iks_insert_attrib(response, "from", iks_find_attrib(iq, "to"));
	iks_insert_attrib(response, "to", iks_find_attrib(iq, "from"));
	iks_insert_attrib(response, "type", "result");
	iks_insert_attrib(response, "id", iks_find_attrib(iq, "id"));
	return response;
}

/**
 * Get attribute value of node, returning empty string if non-existent or not set.
 * @param xml the XML node to search
 * @param attrib the Attribute name
 * @return the attribute value
 */
const char *iks_find_attrib_soft(iks *xml, const char *attrib)
{
	char *value = iks_find_attrib(xml, attrib);
	return zstr(value) ? "" : value;
}

/**
 * Get attribute value of node, returning default value if missing.  The default value
 * is set in the node if missing.
 * @param xml the XML node to search
 * @param attrib the Attribute name
 * @return the attribute value
 */
const char *iks_find_attrib_default(iks *xml, const char *attrib, const char *def)
{
	char *value = iks_find_attrib(xml, attrib);
	if (!value) {
		iks_insert_attrib(xml, attrib, def);
		return def;
	}
	return value;
}

/**
 * Get attribute integer value of node
 * @param xml the XML node to search
 * @param attrib the Attribute name
 * @return the attribute value
 */
int iks_find_int_attrib(iks *xml, const char *attrib)
{
	return atoi(iks_find_attrib_soft(xml, attrib));
}

/**
 * Get attribute boolean value of node
 * @param xml the XML node to search
 * @param attrib the Attribute name
 * @return the attribute value
 */
int iks_find_bool_attrib(iks *xml, const char *attrib)
{
	return switch_true(iks_find_attrib_soft(xml, attrib));
}

/**
 * Get attribute double value of node
 * @param xml the XML node to search
 * @param attrib the Attribute name
 * @return the attribute value
 */
double iks_find_decimal_attrib(iks *xml, const char *attrib)
{
	return atof(iks_find_attrib_soft(xml, attrib));
}

/**
 * Convert iksemel XML node type to string
 * @param type the XML node type
 * @return the string value of type or "UNKNOWN"
 */
const char *iks_node_type_to_string(int type)
{
	switch(type) {
		case IKS_NODE_START: return "NODE_START";
		case IKS_NODE_NORMAL: return "NODE_NORMAL";
		case IKS_NODE_ERROR: return "NODE_ERROR";
		case IKS_NODE_STOP: return "NODE_START";
		default: return "NODE_UNKNOWN";
	}
}

/**
 * Convert iksemel error code to string
 * @param err the iksemel error code
 * @return the string value of error or "UNKNOWN"
 */
const char *iks_net_error_to_string(int err)
{
	switch (err) {
		case IKS_OK: return "OK";
		case IKS_NOMEM: return "NOMEM";
		case IKS_BADXML: return "BADXML";
		case IKS_HOOK: return "HOOK";
		case IKS_NET_NODNS: return "NET_NODNS";
		case IKS_NET_NOSOCK: return "NET_NOSOCK";
		case IKS_NET_NOCONN: return "NET_NOCONN";
		case IKS_NET_RWERR: return "NET_RWERR";
		case IKS_NET_NOTSUPP: return "NET_NOTSUPP";
		case IKS_NET_TLSFAIL: return "NET_TLSFAIL";
		case IKS_NET_DROPPED: return "NET_DROPPED";
		case IKS_NET_UNKNOWN: return "NET_UNKNOWN";
		default: return "UNKNOWN";
	}
}

/**
 * Insert attribute using format string
 * @param xml node to insert attribute into
 * @param name of attribute
 * @param fmt format string
 * @param ... format string args
 */
iks *iks_insert_attrib_printf(iks *xml, const char *name, const char *fmt, ...)
{
	iks *node;
	char *data;
	va_list ap;
	int ret;

	va_start(ap, fmt);
	ret = switch_vasprintf(&data, fmt, ap);
	va_end(ap);

	if (ret == -1) {
		return NULL;
	}
	node = iks_insert_attrib(xml, name, data);
	free(data);

	return node;
}

/**
 * @param value to match
 * @param rule to check
 * @return true if value is one of the comma-separated values in rule
 */
int value_matches(const char *value, const char *rule)
{
	if (rule && *rule && value && *value && !strchr(value, ',')) {
		const char *begin = strstr(rule, value);
		const char *end = begin + strlen(value);
		if (!begin) {
			return 0;
		}
		if ((begin == rule || *(begin - 1) == ',') && (*end == ',' || *end == '\0')) {
				return 1;
		}
		/* substring matched... try farther down the string */
		return value_matches(value, end);
	}
	return 0;
}

#define IKS_SHA256_HEX_DIGEST_LENGTH ((SHA256_DIGEST_LENGTH * 2) + 1)

/**
 * Convert hash to a hex string.
 * @param hash hash to convert
 * @param str buffer to store hash - this buffer must be hashlen * 2 + 1 in size.
 */
static void iks_hash_to_hex_string(unsigned char *hash, int hashlen, unsigned char *str)
{
	static const char *HEX = "0123456789abcdef";
	int i;

	/* convert to hex string with in-place algorithm */
	for (i = hashlen - 1; i >= 0; i--) {
		str[i * 2 + 1] = HEX[hash[i] & 0x0f];
		str[i * 2] = HEX[(hash[i] >> 4) & 0x0f];
	}
	str[hashlen * 2] = '\0';
}

/**
 * Generate SHA-256 hash of value as hex string
 * @param data to hash
 * @param datalen length of data to hash
 * @return hash as a hex string
 */
static void iks_sha256_hex_string(const unsigned char *data, int datalen, unsigned char *hash)
{
	/* hash data */
	SHA256(data, datalen, hash);
	iks_hash_to_hex_string(hash, SHA256_DIGEST_LENGTH, hash);
}

/**
 * Generate HMAC SHA-256
 * @param key the key
 * @param keylen length of key
 * @param message the message
 * @param messagelen length of message
 * @param hash buffer to store the hash - must be IKS_SHA256_HEX_DIGEST_LENGTH
 */
static void iks_hmac_sha256_hex_string(const unsigned char *key, int keylen, const unsigned char *message, int messagelen, unsigned char *hash)
{
	unsigned int hash_len = SHA256_DIGEST_LENGTH;
	HMAC(EVP_sha256(), key, keylen, message, messagelen, hash, &hash_len);
	iks_hash_to_hex_string(hash, SHA256_DIGEST_LENGTH, hash);
}

/**
 * Generate server dialback key.  free() the returned value
 * @param secret originating server shared secret
 * @param receiving_server domain
 * @param originating_server domain
 * @param stream_id stream ID
 * @return the dialback key
 */
char *iks_server_dialback_key(const char *secret, const char *receiving_server, const char *originating_server, const char *stream_id)
{
	if (!zstr(secret) && !zstr(receiving_server) && !zstr(originating_server) && !zstr(stream_id)) {
		unsigned char secret_hash[IKS_SHA256_HEX_DIGEST_LENGTH];
		unsigned char *message = NULL;
		unsigned char *dialback_key = malloc(sizeof(unsigned char *) * IKS_SHA256_HEX_DIGEST_LENGTH);
		iks_sha256_hex_string((unsigned char *)secret, strlen(secret), secret_hash);
		message = (unsigned char *)switch_mprintf("%s %s %s", receiving_server, originating_server, stream_id);
		iks_hmac_sha256_hex_string(secret_hash, strlen((char *)secret_hash), message, strlen((char *)message), dialback_key);
		free(message);
		return (char *)dialback_key;
	}
	return NULL;
}

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

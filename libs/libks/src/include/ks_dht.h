/*
Copyright (c) 2009-2011 by Juliusz Chroboczek

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifndef _KS_DHT_H
#define _KS_DHT_H

#include "ks.h"
#include "ks_bencode.h"

KS_BEGIN_EXTERN_C

typedef enum {
	KS_DHT_EVENT_NONE = 0,
	KS_DHT_EVENT_VALUES = 1,
	KS_DHT_EVENT_VALUES6 = 2,
	KS_DHT_EVENT_SEARCH_DONE = 3,
	KS_DHT_EVENT_SEARCH_DONE6 = 4
} ks_dht_event_t;


typedef enum {
	DHT_PARAM_AUTOROUTE = 1
} ks_dht_param_t;

typedef enum {
	KS_DHT_AF_INET4 = (1 << 0),
	KS_DHT_AF_INET6 = (1 << 1)
} ks_dht_af_flag_t;


typedef void (*dht_callback_t)(void *closure, ks_dht_event_t event, const unsigned char *info_hash, const void *data, size_t data_len);

typedef struct dht_handle_s dht_handle_t;

KS_DECLARE(int) dht_periodic(dht_handle_t *h, const void *buf, size_t buflen, ks_sockaddr_t *from);
KS_DECLARE(ks_status_t) ks_dht_init(dht_handle_t **handle, ks_dht_af_flag_t af_flags, const unsigned char *id, unsigned int port);
KS_DECLARE(void) ks_dht_set_param(dht_handle_t *h, ks_dht_param_t param, ks_bool_t val);
KS_DECLARE(ks_status_t) ks_dht_add_ip(dht_handle_t *h, char *ip, int port);
KS_DECLARE(void) ks_dht_start(dht_handle_t *h);
KS_DECLARE(int) dht_insert_node(dht_handle_t *h, const unsigned char *id, ks_sockaddr_t *sa);
KS_DECLARE(int) dht_ping_node(dht_handle_t *h, ks_sockaddr_t *sa);
KS_DECLARE(int) dht_search(dht_handle_t *h, const unsigned char *id, int port, int af, dht_callback_t callback, void *closure);
KS_DECLARE(int) dht_nodes(dht_handle_t *h, int af, int *good_return, int *dubious_return, int *cached_return, int *incoming_return);
KS_DECLARE(ks_status_t) ks_dht_one_loop(dht_handle_t *h, int timeout);
KS_DECLARE(ks_status_t) ks_dht_get_bind_addrs(dht_handle_t *h, const ks_sockaddr_t ***addrs, ks_size_t *addrlen);
KS_DECLARE(void) ks_dht_set_callback(dht_handle_t *h, dht_callback_t callback, void *closure);
KS_DECLARE(void) ks_dht_set_port(dht_handle_t *h, unsigned int port);
KS_DECLARE(void) dht_dump_tables(dht_handle_t *h, FILE *f);
KS_DECLARE(int) dht_get_nodes(dht_handle_t *h, struct sockaddr_in *sin, int *num, struct sockaddr_in6 *sin6, int *num6);
KS_DECLARE(int) dht_uninit(dht_handle_t **h);
KS_DECLARE(void) ks_dht_set_v(dht_handle_t *h, const unsigned char *v);
KS_DECLARE(int) ks_dht_calculate_mutable_storage_target(unsigned char *pk, unsigned char *salt, int salt_length, unsigned char *target, int target_length);
KS_DECLARE(int) ks_dht_generate_mutable_storage_args(struct bencode *data, int64_t sequence, int cas,
													 unsigned char *id, int id_len, /* querying nodes id */
													 const unsigned char *sk, const unsigned char *pk,
													 unsigned char *salt, unsigned long long salt_length,
													 unsigned char *token, unsigned long long token_length,
													 unsigned char *signature, unsigned long long *signature_length,
													 struct bencode **arguments);

/* This must be provided by the user. */
KS_DECLARE(int) dht_blacklisted(const ks_sockaddr_t *sa);
KS_DECLARE(void) dht_hash(void *hash_return, int hash_size, const void *v1, int len1, const void *v2, int len2, const void *v3, int len3);
KS_DECLARE(int) dht_random_bytes(void *buf, size_t size);

KS_DECLARE(int) ks_dht_send_message_mutable(dht_handle_t *h, unsigned char *sk, unsigned char *pk, char **node_id,
											char *message_id, int sequence, char *message, ks_time_t life);

KS_DECLARE(int) ks_dht_send_message_mutable_cjson(dht_handle_t *h, unsigned char *sk, unsigned char *pk, char **node_id,
												  char *message_id, int sequence, cJSON *message, ks_time_t life);

typedef void (ks_dht_store_entry_json_cb)(struct dht_handle_s *h, const cJSON *msg, void *obj);
KS_DECLARE(void) ks_dht_store_entry_json_cb_set(struct dht_handle_s *h, ks_dht_store_entry_json_cb *store_json_cb, void *arg);

KS_DECLARE(int) ks_dht_api_find_node(dht_handle_t *h, char *node_id_hex, char *target_hex, ks_bool_t ipv6);

KS_END_EXTERN_C

#endif /* _KS_DHT_H */

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

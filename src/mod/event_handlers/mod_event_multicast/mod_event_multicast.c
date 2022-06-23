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
 * Nick Lemberger <nlemberger@gmail.com>
 *
 *
 * mod_event_multicast.c -- Multicast Events
 *
 */
#include <switch.h>
#ifdef HAVE_OPENSSL
#include <openssl/evp.h>
#endif

#define MULTICAST_BUFFSIZE 65536
#define MAX_DST_HOSTS 16

/* magic byte sequence */
static unsigned char MAGIC[] = { 226, 132, 177, 197, 152, 198, 142, 211, 172, 197, 158, 208, 169, 208, 135, 197, 166, 207, 154, 196, 166 };
static char *MARKER = "1";

SWITCH_MODULE_LOAD_FUNCTION(mod_event_multicast_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_event_multicast_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_event_multicast_runtime);
SWITCH_MODULE_DEFINITION(mod_event_multicast, mod_event_multicast_load, mod_event_multicast_shutdown, mod_event_multicast_runtime);

static switch_memory_pool_t *module_pool = NULL;
static char *addr_type_names[] = { "IPv4 unicast", "IPv4 multicast", "IPv6 unicast", "IPv6 multicast", "Unknown" };

typedef enum {
	IPV4_UNICAST,
	IPV4_MULTICAST,
	IPV6_UNICAST,
	IPV6_MULTICAST,
	IP_UNKOWN_TYPE
} addr_type_t;

typedef struct {
	char *ipaddr;
	switch_sockaddr_t *sockaddr;
	addr_type_t addrtype;
} dst_sockaddr_t;

static struct {
	char *src_addr;
	char *src_addr6;
	char *dst_addrs;
	char *bindings;
	uint32_t key_count;
	switch_port_t port;
	int has_udp;
	int has_udp6;
	switch_sockaddr_t *src_sockaddr;
	switch_sockaddr_t *src_sockaddr6;
	switch_socket_t *udp_socket;
	switch_socket_t *udp_socket6;
	switch_hash_t *event_hash;
	uint8_t event_list[SWITCH_EVENT_ALL + 1];
	int running;
	int runtime_thread_has_to_finish;
	int runtime_processing;
	int num_dst_addrs;
	dst_sockaddr_t dst_sockaddrs[MAX_DST_HOSTS];
	uint8_t ttl;
	char *psk;
	switch_mutex_t *mutex;
	switch_thread_rwlock_t *runtime_rwlock;
	switch_hash_t *peer_hash;
	uint8_t loopback;
	uint8_t loopback6;
	char configuration_md5[SWITCH_MD5_DIGEST_STRING_SIZE];
} globals;

struct peer_status {
	switch_bool_t active;
	time_t lastseen;
};

SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_src_addr, globals.src_addr);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_src_addr6, globals.src_addr6);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_dst_addrs, globals.dst_addrs);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_bindings, globals.bindings);
#ifdef HAVE_OPENSSL
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_psk, globals.psk);
#endif
#define CONF_FILE "event_multicast.conf"
#define MULTICAST_EVENT "multicast::event"
#define MULTICAST_PEERUP "multicast::peerup"
#define MULTICAST_PEERDOWN "multicast::peerdown"

static switch_xml_t open_config_file(const char *file_path, switch_xml_t *cfg, char *md5) {
	char *configText;
	switch_xml_t xml;

	if (!(xml = switch_xml_open_cfg(CONF_FILE, cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", CONF_FILE);
	} else if (*cfg) {
		configText = switch_xml_toxml(*cfg, SWITCH_FALSE);
		switch_md5_string(md5, configText, strlen(configText));
		switch_safe_free(configText);
	} else {
		switch_xml_free(xml);
		xml = NULL;
	}

	return xml;
}

static switch_status_t load_config(switch_xml_t input_cfg)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_xml_t cfg, xml = NULL, settings, param;
	char *next, *cur;
	uint32_t count = 0;
	uint8_t custom = 0;

	globals.ttl = 1;
	globals.key_count = 0;
	globals.loopback = 0;
	globals.port = 0;
	globals.num_dst_addrs = 0;

	if (input_cfg) {
		cfg = input_cfg;
	} else {
		if (!(xml = open_config_file(CONF_FILE, &cfg, globals.configuration_md5))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", CONF_FILE);
			return SWITCH_STATUS_TERM;
		}
	}

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcasecmp(var, "address")) {
				set_global_dst_addrs(switch_strip_whitespace(val));
			} else if (!strcasecmp(var, "source_address")) {
				set_global_src_addr(switch_strip_whitespace(val));
			} else if (!strcasecmp(var, "source_address_ipv6")) {
				set_global_src_addr6(switch_strip_whitespace(val));
			} else if (!strcasecmp(var, "bindings")) {
				set_global_bindings(val);
			} else if (!strcasecmp(var, "port")) {
				globals.port = (switch_port_t) atoi(val);
			} else if (!strcasecmp(var, "psk")) {
#ifdef HAVE_OPENSSL
				set_global_psk(val);
#else
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Cannot use pre shared key encryption without OpenSSL support\n");
#endif
			} else if (!strcasecmp(var, "ttl")) {
				int ttl = atoi(val);
				if ((ttl && ttl <= 255) || !strcmp(val, "0")) {
					globals.ttl = (uint8_t) ttl;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid ttl '%s' specified, using default of 1\n", val);
				}
			} else if (!strcasecmp(var, "loopback")) {
				globals.loopback = switch_true(val);
			}
		}
	}

	switch_xml_free(xml);


	if (globals.bindings) {
		for (cur = globals.bindings; cur; count++) {
			switch_event_types_t type;

			if ((next = strchr(cur, ' '))) {
				*next++ = '\0';
			}

			if (custom) {
				switch_core_hash_insert(globals.event_hash, cur, MARKER);
			} else if (switch_name_event(cur, &type) == SWITCH_STATUS_SUCCESS) {
				globals.key_count++;
				if (type == SWITCH_EVENT_ALL) {
					uint32_t x = 0;
					for (x = 0; x < SWITCH_EVENT_ALL; x++) {
						globals.event_list[x] = 0;
					}
				}
				if (type <= SWITCH_EVENT_ALL) {
					globals.event_list[type] = 1;
				}
				if (type == SWITCH_EVENT_CUSTOM) {
					custom++;
				}
			}

			cur = next;
		}
	}

	if (zstr(globals.src_addr)) {
		set_global_src_addr("0.0.0.0");
	}

	if (zstr(globals.src_addr6)) {
		set_global_src_addr6("::");
	}

	if (!globals.key_count) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "No Event Bindings\n");
	}

	return status;
}

static void cleanup()
{
	/* Clean up sockets */
	if (globals.udp_socket) {
		switch_socket_shutdown(globals.udp_socket, SWITCH_SHUTDOWN_READWRITE);
		switch_socket_close(globals.udp_socket);
		globals.udp_socket = NULL;
	}

	if (globals.udp_socket6) {
		switch_socket_shutdown(globals.udp_socket6, SWITCH_SHUTDOWN_READWRITE);
		switch_socket_close(globals.udp_socket6);
		globals.udp_socket6 = NULL;
	}

	if (globals.event_hash) {
		switch_core_hash_destroy(&globals.event_hash);
	}

	if (globals.peer_hash) {
		switch_core_hash_destroy(&globals.peer_hash);
	}

	switch_safe_free(globals.src_addr);
	switch_safe_free(globals.src_addr6);
	switch_safe_free(globals.dst_addrs);
	switch_safe_free(globals.bindings);
	switch_safe_free(globals.psk);

	memset(globals.event_list, 0, SWITCH_EVENT_ALL + 1);
}

static switch_status_t initialize_sockets(switch_xml_t input_cfg)
{
	switch_status_t status;
	int i, dst_host_count = 0;
	char *dst_hosts[MAX_DST_HOSTS] = { 0 };
	switch_sockaddr_t *local_ip_sockaddr;
	switch_sockaddr_t *local_ip6_sockaddr;

	switch_core_hash_init(&globals.event_hash);
	switch_core_hash_init(&globals.peer_hash);

	if (load_config(input_cfg) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot Configure\n");
		switch_goto_status(SWITCH_STATUS_TERM, fail);
	}

	/* to attempt to avoid unicasting to ourself we need to know our IP address */
	switch_sockaddr_info_get(&local_ip_sockaddr, switch_core_get_variable("local_ip_v4"), SWITCH_INET, globals.port, 0, module_pool);
	switch_sockaddr_info_get(&local_ip6_sockaddr, switch_core_get_variable("local_ip_v6"), SWITCH_INET6, globals.port, 0, module_pool);

	/* set up the destination sockaddrs */
	dst_host_count = switch_separate_string(globals.dst_addrs, ',', dst_hosts, MAX_DST_HOSTS);
	for (i = 0; i < dst_host_count; i++) {
		char *ip_addr_groups[8] = { 0 };
		char *host_string;
		char ipv6_first_octet[3];

		memset(&globals.dst_sockaddrs[globals.num_dst_addrs].sockaddr, 0, sizeof(dst_sockaddr_t));

		if (globals.num_dst_addrs > MAX_DST_HOSTS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot add destination address: %s, exceeded maximum of %d\n", dst_hosts[i], MAX_DST_HOSTS);
			continue;
		}

		if (switch_sockaddr_info_get(&globals.dst_sockaddrs[globals.num_dst_addrs].sockaddr, dst_hosts[i], SWITCH_UNSPEC, globals.port, 0, module_pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot find address: %s\n", dst_hosts[i]);
			switch_goto_status(SWITCH_STATUS_TERM, fail);
		}

		/* best effort attempt to avoid unicasting to ourself */
		if (switch_cmp_addr(globals.dst_sockaddrs[globals.num_dst_addrs].sockaddr, local_ip_sockaddr, SWITCH_FALSE) || switch_cmp_addr(globals.dst_sockaddrs[globals.num_dst_addrs].sockaddr, local_ip6_sockaddr, SWITCH_FALSE)) {
			/* this address is on this box, cancel the destination sockaddr setup and move on to the next address */
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Local IP, not adding as peer: %s\n", dst_hosts[i]);
			globals.dst_sockaddrs[globals.num_dst_addrs].sockaddr = NULL;
			globals.dst_sockaddrs[globals.num_dst_addrs].ipaddr = NULL;
			continue;
		}

		/* flag this address with the address type */
		host_string = strdup(dst_hosts[i]);

		if (switch_sockaddr_get_family(globals.dst_sockaddrs[globals.num_dst_addrs].sockaddr) == SWITCH_INET) {
			globals.has_udp = 1;
			switch_separate_string(host_string, '.', ip_addr_groups, sizeof(ip_addr_groups) / sizeof(ip_addr_groups[0]));

			/* IPv4 multicast addresses start with numbers between 224 & 239 inclusive */
			if (switch_safe_atoi(ip_addr_groups[0], 0) >= 224 && switch_safe_atoi(ip_addr_groups[0], 0) <= 239) {
				globals.dst_sockaddrs[globals.num_dst_addrs].addrtype = IPV4_MULTICAST;
			} else {
				globals.dst_sockaddrs[globals.num_dst_addrs].addrtype = IPV4_UNICAST;
			}
		} else if (switch_sockaddr_get_family(globals.dst_sockaddrs[globals.num_dst_addrs].sockaddr) == SWITCH_INET6) {
			globals.has_udp6 = 1;
			switch_separate_string(host_string, ':', ip_addr_groups, 8);

			/* IPv6 multicast addresses have FF as the first octet */
			memcpy(ipv6_first_octet, ip_addr_groups[0], 2);
			ipv6_first_octet[2] = '\0';
			if (strcasecmp(ipv6_first_octet, "FF") == 0) {
				globals.dst_sockaddrs[globals.num_dst_addrs].addrtype = IPV6_MULTICAST;
			} else {
				globals.dst_sockaddrs[globals.num_dst_addrs].addrtype = IPV6_UNICAST;
			}
		} else {
			globals.dst_sockaddrs[globals.num_dst_addrs].addrtype = IP_UNKOWN_TYPE;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unkown address family for peer: %s\n", dst_hosts[i]);
		}

		/* store this address in our list of peers */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Added %s peer: %s", addr_type_names[globals.dst_sockaddrs[globals.num_dst_addrs].addrtype], dst_hosts[i]);
		globals.dst_sockaddrs[globals.num_dst_addrs].ipaddr = switch_core_strdup(module_pool, dst_hosts[i]);
		globals.num_dst_addrs++;
		switch_safe_free(host_string);
	}

	/* create IPv4 source socket */
	if (globals.has_udp == 1) {
		/* create IPv4 listen sockaddr*/
		if (switch_sockaddr_info_get(&globals.src_sockaddr, globals.src_addr, SWITCH_INET, globals.port, 0, module_pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot set IPv4 source address: %s\n", globals.src_addr);
			switch_goto_status(SWITCH_STATUS_TERM, fail);
		}

		/* create IPv4 socket */
		if (switch_socket_create(&globals.udp_socket, AF_INET, SOCK_DGRAM, 0, module_pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to create IPv4 Socket\n");
			switch_goto_status(SWITCH_STATUS_TERM, fail);
		}

		if (switch_socket_opt_set(globals.udp_socket, SWITCH_SO_REUSEADDR, 1) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "IPv4 unable to resue socket\n");
			switch_goto_status(SWITCH_STATUS_TERM, fail);
		}

		if (switch_socket_opt_set(globals.udp_socket, SWITCH_SO_NONBLOCK, TRUE) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "IPv4 unable to set nonblocking mode on socket\n");
			switch_goto_status(SWITCH_STATUS_TERM, fail);
		}

		/* no harm in doing this even without multicast destinations */
		if (switch_mcast_hops(globals.udp_socket, globals.ttl) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to set IPv4 multicast ttl to '%d'\n", globals.ttl);
			switch_goto_status(SWITCH_STATUS_TERM, fail);
		}

		if (switch_mcast_loopback(globals.udp_socket, globals.loopback) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to set IPv4 multicast loopback to '%d'\n", globals.loopback);
			switch_goto_status(SWITCH_STATUS_TERM, fail);
		}
		/* start listening on this IPv4 socket */
		if (switch_socket_bind(globals.udp_socket, globals.src_sockaddr) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to bind IPv4 Socket\n");
			switch_goto_status(SWITCH_STATUS_TERM, fail);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "IPv4 source set to: %s\n", globals.src_addr);
		}
	}

	/* create IPv6 source socket */
	if (globals.has_udp6 == 1) {
		/* create IPv6 listen sockaddr */
		if (switch_sockaddr_info_get(&globals.src_sockaddr6, globals.src_addr6, SWITCH_INET6, globals.port, 0, module_pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot set IPv6 source address: %s\n", globals.src_addr6);
			switch_goto_status(SWITCH_STATUS_TERM, fail);
		}

		/* create IPv6 socket */
		if (switch_socket_create(&globals.udp_socket6, AF_INET6, SOCK_DGRAM, 0, module_pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to create IPv6 Socket\n");
			switch_goto_status(SWITCH_STATUS_TERM, fail);
		}

		if (switch_socket_opt_set(globals.udp_socket6, SWITCH_SO_REUSEADDR, 1) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "IPv6 unable to reuse socket\n");
			switch_goto_status(SWITCH_STATUS_TERM, fail);
		}

		if (switch_socket_opt_set(globals.udp_socket6, SWITCH_SO_NONBLOCK, TRUE) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "IPv6 unable to set nonblocking mode on socket\n");
			switch_goto_status(SWITCH_STATUS_TERM, fail);
		}

		/* no harm in doing this even without multicast destinations */
		/* Bug in APR < v1.2.13, cannot set TTL on IPv6 multicast sockets */
		/*
		   if (switch_mcast_hops(globals.udp_socket6, globals.ttl) != SWITCH_STATUS_SUCCESS) {
		   switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to set IPv6 multicast ttl to '%d'\n", globals.ttl);
		   switch_goto_status(SWITCH_STATUS_TERM, fail);
		   }
		   */

		if (switch_mcast_loopback(globals.udp_socket6, globals.loopback6) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to set IPv6 multicast loopback to '%d'\n", globals.loopback6);
			switch_goto_status(SWITCH_STATUS_TERM, fail);
		}

		/* start listening on this IPv6 socket */
		if (switch_socket_bind(globals.udp_socket6, globals.src_sockaddr6) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to bind IPv6 Socket\n");
			switch_goto_status(SWITCH_STATUS_TERM, fail);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "IPv6 source set to: %s\n", globals.src_addr6);
		}
	}

	/* join multicast destinations */
	for (i = 0; i < globals.num_dst_addrs; i++) {
		if (globals.dst_sockaddrs[i].addrtype == IPV4_MULTICAST) {
			if (switch_mcast_join(globals.udp_socket, globals.dst_sockaddrs[i].sockaddr, NULL, NULL) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Multicast Error\n");
				switch_goto_status(SWITCH_STATUS_TERM, fail);
			}
		}

		if (globals.dst_sockaddrs[i].addrtype == IPV6_MULTICAST) {
			if (switch_mcast_join(globals.udp_socket6, globals.dst_sockaddrs[i].sockaddr, NULL, NULL) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Multicast Error\n");
				switch_goto_status(SWITCH_STATUS_TERM, fail);
			}
		}
	}

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;

fail:
	cleanup();

	return status;
}

static void event_handler(switch_event_t *event)
{
	uint8_t send = 0;

	if (globals.running != 1) {
		return;
	}

	if (event->subclass_name && (!strcmp(event->subclass_name, MULTICAST_EVENT) ||
				!strcmp(event->subclass_name, MULTICAST_PEERUP) || !strcmp(event->subclass_name, MULTICAST_PEERDOWN))) {
		char *event_name, *sender;
		if ((event_name = switch_event_get_header(event, "orig-event-name")) &&
				!strcasecmp(event_name, "HEARTBEAT") && (sender = switch_event_get_header(event, "orig-multicast-sender"))) {
			struct peer_status *p;
			time_t now = switch_epoch_time_now(NULL);

			if (!(p = switch_core_hash_find(globals.peer_hash, sender))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Host %s not already in hash\n", sender);
				p = switch_core_alloc(module_pool, sizeof(struct peer_status));
				p->active = SWITCH_FALSE;
				p->lastseen = 0;
				/*} else { */
				/*switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Host %s last seen %d seconds ago\n", sender, now - p->lastseen); */
			}

			if (!p->active) {
				switch_event_t *local_event;
				if (switch_event_create_subclass(&local_event, SWITCH_EVENT_CUSTOM, MULTICAST_PEERUP) == SWITCH_STATUS_SUCCESS) {
					char lastseen[21];
					switch_event_add_header_string(local_event, SWITCH_STACK_BOTTOM, "Peer", sender);
					if (p->lastseen) {
						switch_snprintf(lastseen, sizeof(lastseen), "%d", (int) p->lastseen);
					} else {
						switch_snprintf(lastseen, sizeof(lastseen), "%s", "Never");
					}
					switch_event_add_header_string(local_event, SWITCH_STACK_BOTTOM, "Lastseen", lastseen);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Peer %s has come up; last seen: %s\n", sender, lastseen);

					switch_event_fire(&local_event);
				}
			}
			p->active = SWITCH_TRUE;
			p->lastseen = now;

			switch_core_hash_insert(globals.peer_hash, sender, p);
		}

		/* ignore our own events to avoid ping pong */
		return;
	}

	if (event->event_id == SWITCH_EVENT_RELOADXML) {
		switch_bool_t config_changed = SWITCH_FALSE;
		char tempMD5[SWITCH_MD5_DIGEST_STRING_SIZE];
		switch_xml_t xml, cfg;

		if (!(xml = open_config_file(CONF_FILE, &cfg, tempMD5))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", CONF_FILE);
			return;
		}

		switch_mutex_lock(globals.mutex);

		config_changed = strcmp(globals.configuration_md5, tempMD5) ? SWITCH_TRUE : SWITCH_FALSE;
		/* If the config has changed, reload the configuration */
		if (config_changed) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Configuration changed, reloading\n");
			switch_thread_rwlock_wrlock(globals.runtime_rwlock);

			cleanup();

			/* Re-initialize the module */
			if (initialize_sockets(cfg) != SWITCH_STATUS_SUCCESS) {
				globals.runtime_processing = 0;
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unable to reinitialize sockets.\n");
				/* Invalidate current configuration md5 */
				switch_md5_string(globals.configuration_md5, "", strlen(""));
			} else {
				switch_md5_string(globals.configuration_md5, tempMD5, strlen(tempMD5));
				globals.runtime_processing = 1;
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Reloaded\n");
			}

			switch_thread_rwlock_unlock(globals.runtime_rwlock);
		}

		switch_mutex_unlock(globals.mutex);
		switch_xml_free(xml);
	}

	if (event->event_id == SWITCH_EVENT_HEARTBEAT) {
		switch_hash_index_t *cur;
		switch_ssize_t keylen;
		const void *key;
		void *value;
		time_t now = switch_epoch_time_now(NULL);
		struct peer_status *last;
		char *host;

		for (cur = switch_core_hash_first(globals.peer_hash); cur; cur = switch_core_hash_next(&cur)) {
			switch_core_hash_this(cur, &key, &keylen, &value);
			host = (char *) key;
			last = (struct peer_status *) value;
			if (last->active && (now - (last->lastseen)) > 60) {
				switch_event_t *local_event;

				last->active = SWITCH_FALSE;
				if (switch_event_create_subclass(&local_event, SWITCH_EVENT_CUSTOM, MULTICAST_PEERDOWN) == SWITCH_STATUS_SUCCESS) {
					char lastseen[21];
					switch_event_add_header_string(local_event, SWITCH_STACK_BOTTOM, "Peer", host);
					switch_snprintf(lastseen, sizeof(lastseen), "%d", (int) last->lastseen);
					switch_event_add_header_string(local_event, SWITCH_STACK_BOTTOM, "Lastseen", lastseen);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Peer %s has gone down; last seen: %s\n", host, lastseen);

					switch_event_fire(&local_event);
				}
			}
		}
	}

	switch_mutex_lock(globals.mutex);
	if (globals.event_list[(uint8_t) SWITCH_EVENT_ALL]) {
		send = 1;
	} else if ((globals.event_list[(uint8_t) event->event_id])) {
		if (event->event_id != SWITCH_EVENT_CUSTOM || (event->subclass_name && switch_core_hash_find(globals.event_hash, event->subclass_name))) {
			send = 1;
		}
	}
	switch_mutex_unlock(globals.mutex);

	if (send) {
		char *packet;

		switch (event->event_id) {
			case SWITCH_EVENT_LOG:
				return;
			default:
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Multicast-Sender", switch_core_get_switchname());
				if (switch_event_serialize(event, &packet, SWITCH_TRUE) == SWITCH_STATUS_SUCCESS) {
					size_t len;
					char *buf;
					int i;

#ifdef HAVE_OPENSSL
					int outlen, tmplen;
#if OPENSSL_VERSION_NUMBER >= 0x10100000
					EVP_CIPHER_CTX *ctx;
#else
					EVP_CIPHER_CTX ctx;
#endif
					char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
					switch_uuid_t uuid;

					switch_uuid_get(&uuid);
					switch_uuid_format(uuid_str, &uuid);
					len = strlen(packet) + SWITCH_UUID_FORMATTED_LENGTH + EVP_MAX_IV_LENGTH + strlen((char *) MAGIC);
#else
					len = strlen(packet) + strlen((char *) MAGIC);
#endif
					buf = malloc(len + 1);
					memset(buf, 0, len + 1);
					switch_assert(buf);

#ifdef HAVE_OPENSSL
					if (globals.psk) {
						switch_copy_string(buf, uuid_str, SWITCH_UUID_FORMATTED_LENGTH);

#if OPENSSL_VERSION_NUMBER >= 0x10100000
						ctx = EVP_CIPHER_CTX_new();
						EVP_EncryptInit(ctx, EVP_bf_cbc(), NULL, NULL);
						EVP_CIPHER_CTX_set_key_length(ctx, strlen(globals.psk));
						EVP_EncryptInit(ctx, NULL, (unsigned char *) globals.psk, (unsigned char *) uuid_str);
						EVP_EncryptUpdate(ctx, (unsigned char *) buf + SWITCH_UUID_FORMATTED_LENGTH,
								&outlen, (unsigned char *) packet, (int) strlen(packet));
						EVP_EncryptUpdate(ctx, (unsigned char *) buf + SWITCH_UUID_FORMATTED_LENGTH + outlen,
								&tmplen, (unsigned char *) MAGIC, (int) strlen((char *) MAGIC));
						outlen += tmplen;
						EVP_EncryptFinal(ctx, (unsigned char *) buf + SWITCH_UUID_FORMATTED_LENGTH + outlen, &tmplen);
						EVP_CIPHER_CTX_free(ctx);
#else
						EVP_CIPHER_CTX_init(&ctx);
						EVP_EncryptInit(&ctx, EVP_bf_cbc(), NULL, NULL);
						EVP_CIPHER_CTX_set_key_length(&ctx, strlen(globals.psk));
						EVP_EncryptInit(&ctx, NULL, (unsigned char *) globals.psk, (unsigned char *) uuid_str);
						EVP_EncryptUpdate(&ctx, (unsigned char *) buf + SWITCH_UUID_FORMATTED_LENGTH,
								&outlen, (unsigned char *) packet, (int) strlen(packet));
						EVP_EncryptUpdate(&ctx, (unsigned char *) buf + SWITCH_UUID_FORMATTED_LENGTH + outlen,
								&tmplen, (unsigned char *) MAGIC, (int) strlen((char *) MAGIC));
						outlen += tmplen;
						EVP_EncryptFinal(&ctx, (unsigned char *) buf + SWITCH_UUID_FORMATTED_LENGTH + outlen, &tmplen);
						EVP_CIPHER_CTX_cleanup(&ctx);
#endif
						outlen += tmplen;
						len = (size_t) outlen + SWITCH_UUID_FORMATTED_LENGTH;
						*(buf + SWITCH_UUID_FORMATTED_LENGTH + outlen) = '\0';
					} else {
#endif
						switch_copy_string(buf, packet, len);
						switch_copy_string(buf + strlen(packet), (char *) MAGIC, strlen((char *) MAGIC) + 1);
#ifdef HAVE_OPENSSL
					}
#endif

					for (i = 0; i < globals.num_dst_addrs; i++) {
						/* Send to IPv4 */
						if (globals.dst_sockaddrs[i].addrtype == IPV4_UNICAST || globals.dst_sockaddrs[i].addrtype == IPV4_MULTICAST) {
							switch_socket_sendto(globals.udp_socket, globals.dst_sockaddrs[i].sockaddr, 0, buf, &len);
						}

						/* Send to IPv4 */
						if (globals.dst_sockaddrs[i].addrtype == IPV6_UNICAST || globals.dst_sockaddrs[i].addrtype == IPV6_MULTICAST) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Sending message to IPv6: %s\n", globals.dst_sockaddrs[i].ipaddr);
							switch_socket_sendto(globals.udp_socket6, globals.dst_sockaddrs[i].sockaddr, 0, buf, &len);
						}
					}

					switch_safe_free(packet);
					switch_safe_free(buf);
				}
				break;
		}
	}
	return;
}

static switch_status_t process_packet(char* packet, size_t len)
{
	char *m;
	switch_event_t *local_event;

#ifdef HAVE_OPENSSL
	if (globals.psk) {
		char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
		char *tmp;
		int outl, tmplen;
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
		EVP_CIPHER_CTX *ctx;
#else
		EVP_CIPHER_CTX ctx;
#endif

		len -= SWITCH_UUID_FORMATTED_LENGTH;

		tmp = malloc(len);

		memset(tmp, 0, len);

		switch_copy_string(uuid_str, packet, SWITCH_UUID_FORMATTED_LENGTH);
		packet += SWITCH_UUID_FORMATTED_LENGTH;

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
		ctx = EVP_CIPHER_CTX_new();
		EVP_DecryptInit(ctx, EVP_bf_cbc(), NULL, NULL);
		EVP_CIPHER_CTX_set_key_length(ctx, strlen(globals.psk));
		EVP_DecryptInit(ctx, NULL, (unsigned char *)globals.psk, (unsigned char *)uuid_str);
		EVP_DecryptUpdate(ctx, (unsigned char *)tmp, &outl, (unsigned char *)packet, (int)len);
		EVP_DecryptFinal(ctx, (unsigned char *)tmp + outl, &tmplen);
		EVP_CIPHER_CTX_free(ctx);
#else
		EVP_CIPHER_CTX_init(&ctx);
		EVP_DecryptInit(&ctx, EVP_bf_cbc(), NULL, NULL);
		EVP_CIPHER_CTX_set_key_length(&ctx, strlen(globals.psk));
		EVP_DecryptInit(&ctx, NULL, (unsigned char *)globals.psk, (unsigned char *)uuid_str);
		EVP_DecryptUpdate(&ctx, (unsigned char *)tmp, &outl, (unsigned char *)packet, (int)len);
		EVP_DecryptFinal(&ctx, (unsigned char *)tmp + outl, &tmplen);
		EVP_CIPHER_CTX_cleanup(&ctx);
#endif

		*(tmp + outl + tmplen) = '\0';

		/*switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "decrypted event as %s\n----------\n of actual length %d (%d) %d\n", tmp, outl + tmplen, (int) len, (int) strlen(tmp)); */
		packet = tmp;

	}
#endif

	if ((m = strchr(packet, (int)MAGIC[0])) != 0) {
		/*switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Found start of magic string\n"); */
		if (!strncmp((char *)MAGIC, m, strlen((char *)MAGIC))) {
			/*switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Found entire magic string\n"); */
			*m = '\0';
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Inbound event packet is missing the complete validation string.\n");
			return SWITCH_STATUS_NOOP;
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Failed to validate inbound event packet, is your PSK correctly configured?\n");
		return SWITCH_STATUS_NOOP;
	}

	/*switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\nEVENT %d\n--------------------------------\n%s\n", (int) len, packet); */
	if (switch_event_create_subclass(&local_event, SWITCH_EVENT_CUSTOM, MULTICAST_EVENT) == SWITCH_STATUS_SUCCESS) {
		char *var, *val, *term = NULL, tmpname[128];
		switch_event_add_header_string(local_event, SWITCH_STACK_BOTTOM, "Multicast", "yes");
		var = packet;
		while (var && *var) {
			if ((val = strchr(var, ':')) != 0) {
				*val++ = '\0';
				while (*val == ' ') {
					val++;
				}
				if ((term = strchr(val, '\r')) != 0 || (term = strchr(val, '\n')) != 0) {
					*term = '\0';
					while (*term == '\r' || *term == '\n') {
						term++;
					}
				}
				switch_url_decode(val);
				switch_snprintf(tmpname, sizeof(tmpname), "Orig-%s", var);
				switch_event_add_header_string(local_event, SWITCH_STACK_BOTTOM, tmpname, val);
				var = term + 1;
			} else {
				/* This should be our magic packet, done processing incoming headers */
				break;
			}
		}

		if (var && strlen(var) > 1) {
			switch_event_add_body(local_event, "%s", var);
		}

		return switch_event_fire(&local_event);
	}

	return SWITCH_STATUS_NOOP;
}

SWITCH_STANDARD_API(multicast_peers)
{
	switch_hash_index_t *cur;
	switch_ssize_t keylen;
	const void *key;
	void *value;
	time_t now = switch_epoch_time_now(NULL);
	struct peer_status *last;
	char *host;
	int i;

	switch_mutex_lock(globals.mutex);

	/* Output current module state */
	stream->write_function(stream, "Module state: ");
	if (globals.running == 1 && globals.runtime_processing == 1) {
		stream->write_function(stream, "Active\n\n");
	} else {
		stream->write_function(stream, "Inactive\n\n");
	}

	stream->write_function(stream, "Configured peers:\n");
	for (i = 0; i < globals.num_dst_addrs; i++) {
		stream->write_function(stream, "\t%s: %s\n", addr_type_names[globals.dst_sockaddrs[i].addrtype], globals.dst_sockaddrs[i].ipaddr);
	}

	stream->write_function(stream, "\n\n");

	i = 0;
	for (cur = switch_core_hash_first(globals.peer_hash); cur; cur = switch_core_hash_next(&cur)) {
		switch_core_hash_this(cur, &key, &keylen, &value);
		host = (char *) key;
		last = (struct peer_status *) value;

		stream->write_function(stream, "Peer %s %s; last seen %d seconds ago\n", host, last->active ? "UP" : "DOWN", now - last->lastseen);
		i++;
	}

	if (i == 0) {
		stream->write_function(stream, "No multicast peers seen\n");
	}

	switch_mutex_unlock(globals.mutex);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_event_multicast_load)
{
	switch_api_interface_t *api_interface;
	switch_status_t status = SWITCH_STATUS_GENERR;

	memset(&globals, 0, sizeof(globals));

	switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, pool);
	module_pool = pool;

	switch_thread_rwlock_create(&globals.runtime_rwlock, pool);

	if (initialize_sockets(NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to initialize sockets.");
		switch_goto_status(SWITCH_STATUS_GENERR, fail);
	}

	/* Reserve the module specific events */
	if (switch_event_reserve_subclass(MULTICAST_EVENT) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass %s!\n", MULTICAST_EVENT);
		switch_goto_status(SWITCH_STATUS_GENERR, fail);
	}

	if (switch_event_reserve_subclass(MULTICAST_PEERUP) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass %s!\n", MULTICAST_PEERUP);
		switch_goto_status(SWITCH_STATUS_GENERR, fail);
	}

	if (switch_event_reserve_subclass(MULTICAST_PEERDOWN) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass %s!\n", MULTICAST_PEERDOWN);
		switch_goto_status(SWITCH_STATUS_GENERR, fail);
	}

	/* Bind to the event bus */
	if (switch_event_bind(modname, SWITCH_EVENT_ALL, SWITCH_EVENT_SUBCLASS_ANY, event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind to event bus!\n");
		switch_goto_status(SWITCH_STATUS_GENERR, fail);
	}

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_API(api_interface, "multicast_peers", "Show status of multicast peers", multicast_peers, "");

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;


fail:

	switch_event_free_subclass(MULTICAST_EVENT);
	switch_event_free_subclass(MULTICAST_PEERUP);
	switch_event_free_subclass(MULTICAST_PEERDOWN);

	return status;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_event_multicast_shutdown)
{
	globals.running = 0;
	switch_event_unbind_callback(event_handler);

	while (globals.runtime_thread_has_to_finish) {
		switch_yield(100 * 1000);
	}

	switch_mutex_lock(globals.mutex);

	switch_event_free_subclass(MULTICAST_EVENT);
	switch_event_free_subclass(MULTICAST_PEERUP);
	switch_event_free_subclass(MULTICAST_PEERDOWN);

	cleanup();

	switch_thread_rwlock_destroy(globals.runtime_rwlock);
	switch_mutex_unlock(globals.mutex);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_RUNTIME_FUNCTION(mod_event_multicast_runtime)
{
	char *buf;

	globals.runtime_thread_has_to_finish = 1;

	buf = (char *) malloc(MULTICAST_BUFFSIZE);
	switch_assert(buf);
	globals.running = 1;
	globals.runtime_processing = 1;
	while (globals.running == 1) {
		int rxdata = 0;
		size_t len = MULTICAST_BUFFSIZE - 1;
		switch_status_t status;

		switch_thread_rwlock_rdlock(globals.runtime_rwlock);

		if (globals.running != 1) {
			switch_thread_rwlock_unlock(globals.runtime_rwlock);
			break;
		} else if (globals.runtime_processing != 1) {
			switch_thread_rwlock_unlock(globals.runtime_rwlock);
			switch_yield(100 * 1000);
			continue;
		}

		memset(buf, 0, len + 1);

		/* If there's data in the IPv4 packet, process it */
		if (globals.has_udp == 1) {
			status = switch_socket_recv(globals.udp_socket, buf, &len);
			if (globals.running == 0 || (!SWITCH_STATUS_IS_BREAK(status) && status != SWITCH_STATUS_SUCCESS)) {
				switch_thread_rwlock_unlock(globals.runtime_rwlock);
				break;
			}

			/* Did we get data? */
			if (len && len > SWITCH_UUID_FORMATTED_LENGTH) {
				rxdata = 1;
				process_packet(buf, len);
			}
		}

		/* If there's data in the IPv6 packet, process it */
		len = MULTICAST_BUFFSIZE - 1;
		memset(buf, 0, len + 1);
		if (globals.has_udp6 == 1) {
			status = switch_socket_recv(globals.udp_socket6, buf, &len);
			if (globals.running == 0 || (!SWITCH_STATUS_IS_BREAK(status) && status != SWITCH_STATUS_SUCCESS)) {
				switch_thread_rwlock_unlock(globals.runtime_rwlock);
				break;
			}

			/* Did we get data? */
			if (len && len > SWITCH_UUID_FORMATTED_LENGTH) {
				rxdata = 1;
				process_packet(buf, len);
			}
		}

		/* Nonblocking sockets are required, re-run loop if we got data, else yield */
		if (rxdata == 1) {
			switch_thread_rwlock_unlock(globals.runtime_rwlock);
			continue;
		}

		switch_thread_rwlock_unlock(globals.runtime_rwlock);
		switch_yield(500000);
	}

	globals.running = 0;
	free(buf);
	globals.runtime_thread_has_to_finish = 0;
	return SWITCH_STATUS_TERM;
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

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

/* magic byte sequence */
static unsigned char MAGIC[] = { 226, 132, 177, 197, 152, 198, 142, 211, 172, 197, 158, 208, 169, 208, 135, 197, 166, 207, 154, 196, 166 };
static char *MARKER = "1";

SWITCH_MODULE_LOAD_FUNCTION(mod_event_multicast_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_event_multicast_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_event_multicast_runtime);
SWITCH_MODULE_DEFINITION(mod_event_multicast, mod_event_multicast_load, mod_event_multicast_shutdown, mod_event_multicast_runtime);

static switch_memory_pool_t *module_pool = NULL;

static struct {
	char *address;
	char *bindings;
	uint32_t key_count;
	switch_port_t port;
	switch_sockaddr_t *addr;
	switch_socket_t *udp_socket;
	switch_hash_t *event_hash;
	uint8_t event_list[SWITCH_EVENT_ALL + 1];
	int running;
	uint8_t ttl;
	char *psk;
	switch_mutex_t *mutex;
	switch_hash_t *peer_hash;
	int loopback;
} globals;

struct peer_status {
	switch_bool_t active;
	time_t lastseen;
};

SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_address, globals.address);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_bindings, globals.bindings);
#ifdef HAVE_OPENSSL
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_psk, globals.psk);
#endif
#define MULTICAST_EVENT "multicast::event"
#define MULTICAST_PEERUP "multicast::peerup"
#define MULTICAST_PEERDOWN "multicast::peerdown"
static switch_status_t load_config(void)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	char *cf = "event_multicast.conf";
	switch_xml_t cfg, xml, settings, param;
	char *next, *cur;
	uint32_t count = 0;
	uint8_t custom = 0;


	globals.ttl = 1;
	globals.key_count = 0;
	globals.loopback = 0;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}


	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcasecmp(var, "address")) {
				set_global_address(val);
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

	if (!globals.key_count) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "No Bindings\n");
	}

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
		switch_mutex_lock(globals.mutex);
		switch_core_hash_destroy(&globals.event_hash);
		globals.event_hash = NULL;
		if (globals.psk) {
			switch_safe_free(globals.psk);
			globals.psk = NULL;
		}
		switch_core_hash_init(&globals.event_hash);
		memset(globals.event_list, 0, SWITCH_EVENT_ALL + 1);
		if (load_config() != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Failed to reload config file\n");
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Event Multicast Reloaded\n");
		}
		switch_mutex_unlock(globals.mutex);
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
#ifdef HAVE_OPENSSL
				int outlen, tmplen;
				EVP_CIPHER_CTX ctx;
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

				switch_socket_sendto(globals.udp_socket, globals.addr, 0, buf, &len);
				switch_safe_free(packet);
				switch_safe_free(buf);
			}
			break;
		}
	}
	return;
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
	int i = 0;

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

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_event_multicast_load)
{
	switch_api_interface_t *api_interface;
	switch_status_t status = SWITCH_STATUS_GENERR;

	memset(&globals, 0, sizeof(globals));

	switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, pool);
	module_pool = pool;

	switch_core_hash_init(&globals.event_hash);
	switch_core_hash_init(&globals.peer_hash);

	globals.key_count = 0;

	if (load_config() != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot Configure\n");
		switch_goto_status(SWITCH_STATUS_TERM, fail);
	}

	if (switch_sockaddr_info_get(&globals.addr, globals.address, SWITCH_UNSPEC, globals.port, 0, module_pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot find address\n");
		switch_goto_status(SWITCH_STATUS_TERM, fail);
	}

	if (switch_socket_create(&globals.udp_socket, AF_INET, SOCK_DGRAM, 0, module_pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Socket Error\n");
		switch_goto_status(SWITCH_STATUS_TERM, fail);
	}

	if (switch_socket_opt_set(globals.udp_socket, SWITCH_SO_REUSEADDR, 1) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Socket Option Error\n");
		switch_goto_status(SWITCH_STATUS_TERM, fail);
	}

	if (switch_mcast_join(globals.udp_socket, globals.addr, NULL, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Multicast Error\n");
		switch_goto_status(SWITCH_STATUS_TERM, fail);
	}

	if (switch_mcast_hops(globals.udp_socket, globals.ttl) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to set ttl to '%d'\n", globals.ttl);
		switch_goto_status(SWITCH_STATUS_TERM, fail);
	}

	if (switch_mcast_loopback(globals.udp_socket, (uint8_t)globals.loopback) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to set loopback to '%d'\n", globals.loopback);
		switch_goto_status(SWITCH_STATUS_TERM, fail);
	}

	if (switch_socket_bind(globals.udp_socket, globals.addr) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Bind Error\n");
		switch_goto_status(SWITCH_STATUS_TERM, fail);
	}

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

	if (switch_event_bind(modname, SWITCH_EVENT_ALL, SWITCH_EVENT_SUBCLASS_ANY, event_handler, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		switch_goto_status(SWITCH_STATUS_GENERR, fail);
	}
#ifdef USE_NONBLOCK
	switch_socket_opt_set(globals.udp_socket, SWITCH_SO_NONBLOCK, TRUE);
#endif

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_API(api_interface, "multicast_peers", "Show status of multicast peers", multicast_peers, "");

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;


  fail:

	if (globals.udp_socket) {
		switch_socket_close(globals.udp_socket);
	}

	switch_event_free_subclass(MULTICAST_EVENT);
	switch_event_free_subclass(MULTICAST_PEERUP);
	switch_event_free_subclass(MULTICAST_PEERDOWN);

	return status;

}


SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_event_multicast_shutdown)
{
	globals.running = 0;
	switch_event_unbind_callback(event_handler);

	if (globals.udp_socket) {
		switch_socket_shutdown(globals.udp_socket, 2);
	}

	switch_event_free_subclass(MULTICAST_EVENT);
	switch_event_free_subclass(MULTICAST_PEERUP);
	switch_event_free_subclass(MULTICAST_PEERDOWN);

	switch_core_hash_destroy(&globals.event_hash);

	switch_safe_free(globals.address);
	switch_safe_free(globals.bindings);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_RUNTIME_FUNCTION(mod_event_multicast_runtime)
{
	switch_event_t *local_event;
	char *buf, *m;
	switch_sockaddr_t *addr;

	buf = (char *) malloc(MULTICAST_BUFFSIZE);
	switch_assert(buf);
	switch_sockaddr_info_get(&addr, NULL, SWITCH_UNSPEC, 0, 0, module_pool);
	globals.running = 1;
	while (globals.running == 1) {
		char *myaddr;
		size_t len = MULTICAST_BUFFSIZE;
		char *packet;
		switch_status_t status;
		memset(buf, 0, len);

		switch_sockaddr_ip_get(&myaddr, globals.addr);
		if ((status = switch_socket_recvfrom(addr, globals.udp_socket, 0, buf, &len)) != SWITCH_STATUS_SUCCESS || !len || !globals.running) {
			break;
		}
#ifdef USE_NONBLOCK
		if (!len) {
			if (SWITCH_STATUS_IS_BREAK(status)) {
				switch_yield(100000);
				continue;
			}

			break;
		}
#endif

		packet = buf;

#ifdef HAVE_OPENSSL
		if (globals.psk) {
			char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
			char *tmp;
			int outl, tmplen;
			EVP_CIPHER_CTX ctx;

			len -= SWITCH_UUID_FORMATTED_LENGTH;

			tmp = malloc(len);

			memset(tmp, 0, len);

			switch_copy_string(uuid_str, packet, SWITCH_UUID_FORMATTED_LENGTH);
			packet += SWITCH_UUID_FORMATTED_LENGTH;

			EVP_CIPHER_CTX_init(&ctx);
			EVP_DecryptInit(&ctx, EVP_bf_cbc(), NULL, NULL);
			EVP_CIPHER_CTX_set_key_length(&ctx, strlen(globals.psk));
			EVP_DecryptInit(&ctx, NULL, (unsigned char *) globals.psk, (unsigned char *) uuid_str);
			EVP_DecryptUpdate(&ctx, (unsigned char *) tmp, &outl, (unsigned char *) packet, (int) len);
			EVP_DecryptFinal(&ctx, (unsigned char *) tmp + outl, &tmplen);

			*(tmp + outl + tmplen) = '\0';

			/*switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "decrypted event as %s\n----------\n of actual length %d (%d) %d\n", tmp, outl + tmplen, (int) len, (int) strlen(tmp)); */
			packet = tmp;

		}
#endif
		if ((m = strchr(packet, (int) MAGIC[0])) != 0) {
			/*switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Found start of magic string\n"); */
			if (!strncmp((char *) MAGIC, m, strlen((char *) MAGIC))) {
				/*switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Found entire magic string\n"); */
				*m = '\0';
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Failed to find entire magic string\n");
				continue;
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Failed to find start of magic string\n");
			continue;
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
					break;
				}
			}

			if (var && strlen(var) > 1) {
				switch_event_add_body(local_event, "%s", var);
			}

			switch_event_fire(&local_event);

		}

	}


	if (globals.udp_socket) {
		switch_socket_close(globals.udp_socket);
		globals.udp_socket = NULL;
	}

	globals.running = 0;
	free(buf);
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

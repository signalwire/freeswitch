/* 
 * mod_graylog2 for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2014, Grasshopper
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
 * The Original Code is mod_graylog2 for FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is Grasshopper
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Christopher Rienzo <crienzo@grasshopper.com>
 * Michael McGuinness <mmcguinness@grasshopper.com>
 *
 * mod_graylog2.c -- Graylog2 GELF logger
 *
 */
#include <switch.h>

#include <sys/types.h>
#include <unistd.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_graylog2_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_graylog2_shutdown);
SWITCH_MODULE_DEFINITION(mod_graylog2, mod_graylog2_load, mod_graylog2_shutdown, NULL);

static struct {
	/** memory pool for this module */
	switch_memory_pool_t *pool;
	/** graylog2 server address */
	const char *server_host;
	/** graylog2 server port */
	switch_port_t server_port;
	/** minimum log level to allow */
	switch_log_level_t log_level;
	/** shutdown flag */
	int shutdown;
	/** prevents context shutdown until all threads are finished */
	switch_thread_rwlock_t *shutdown_rwlock;
	/** log delivery queue */
	switch_queue_t *log_queue;
} globals;

/**
 * Convert log level to graylog2 log level
 */
static int to_graylog2_level(switch_log_level_t level)
{
	switch (level) {
		case SWITCH_LOG_DEBUG: return 7;
		case SWITCH_LOG_INFO: return 6;
		case SWITCH_LOG_NOTICE: return 5;
		case SWITCH_LOG_WARNING: return 4;
		case SWITCH_LOG_ERROR: return 3;
		case SWITCH_LOG_CRIT: return 2;
		case SWITCH_LOG_ALERT: return 1;
		default: return 8;
	}
}

/**
 * Encode log as GELF
 */
static char *to_gelf(const switch_log_node_t *node, switch_log_level_t log_level)
{
	char *gelf_text = NULL;
	cJSON *gelf = cJSON_CreateObject();
	char *hostname;
	char timestamp[32];
	char *full_message = node->content;
	char short_message[151];
	char *short_message_end = NULL;
	char *parsed_full_message = NULL;
	char *field_name = NULL;
	switch_event_t *log_fields = NULL;

	cJSON_AddItemToObject(gelf, "version", cJSON_CreateString("1.1"));
	if ((hostname = switch_core_get_variable("hostname")) && !zstr(hostname)) {
		cJSON_AddItemToObject(gelf, "host", cJSON_CreateString(hostname));
	} else if ((hostname = switch_core_get_variable("local_ip_v4")) && !zstr(hostname)) {
		cJSON_AddItemToObject(gelf, "host", cJSON_CreateString(hostname));
	}
	switch_snprintf(timestamp, 32, "%"SWITCH_UINT64_T_FMT".%d", (uint64_t)(node->timestamp / 1000000), node->timestamp % 1000000);
	cJSON_AddItemToObject(gelf, "timestamp", cJSON_CreateString(timestamp));
	cJSON_AddItemToObject(gelf, "_microtimestamp", cJSON_CreateNumber(node->timestamp));
	cJSON_AddItemToObject(gelf, "level", cJSON_CreateNumber(to_graylog2_level(log_level)));
	cJSON_AddItemToObject(gelf, "_ident", cJSON_CreateString("freeswitch"));
	cJSON_AddItemToObject(gelf, "_pid", cJSON_CreateNumber((int)getpid()));
	if (!zstr(node->userdata)) {
		cJSON_AddItemToObject(gelf, "_uuid", cJSON_CreateString(node->userdata));
	}
	if (!zstr_buf(node->file)) {
		cJSON_AddItemToObject(gelf, "_file", cJSON_CreateString(node->file));
		cJSON_AddItemToObject(gelf, "_line", cJSON_CreateNumber(node->line));
	}
	if (!zstr_buf(node->func)) {
		cJSON_AddItemToObject(gelf, "_function", cJSON_CreateString(node->func));
	}

	/* skip initial space and new line */
	if (*full_message == ' ') {
		full_message++;
	}
	if (*full_message == '\n') {
		full_message++;
	}

	/* parse list of fields, if any */
	if (strncmp(full_message, "LOG_FIELDS", 10) == 0) {
		if (switch_event_create_brackets(full_message+10, '[', ']', ',', &log_fields, &parsed_full_message, SWITCH_TRUE) == SWITCH_STATUS_SUCCESS) {

			switch_event_header_t *hp;
			for (hp = log_fields->headers; hp; hp = hp->next) {
				if (!zstr(hp->name) && !zstr(hp->value)) {
					if (strncmp(hp->name, "@#", 2) == 0) {
						field_name = switch_mprintf("_%s", hp->name + 2);
						cJSON_AddItemToObject(gelf, field_name, cJSON_CreateNumber(strtod(hp->value, NULL)));
					} else {
						field_name = switch_mprintf("_%s", hp->name);
						cJSON_AddItemToObject(gelf, field_name, cJSON_CreateString(hp->value));
					}
					free(field_name);
				}
			}

			switch_event_destroy(&log_fields);
			full_message = parsed_full_message;
		}
	}

	cJSON_AddItemToObject(gelf, "full_message", cJSON_CreateString(full_message));

	switch_snprintf(short_message, sizeof(short_message) - 1, "%s", full_message);
	if ((short_message_end = strchr(short_message, '\n'))) {
		*short_message_end = '\0';
	}
	cJSON_AddItemToObject(gelf, "short_message", cJSON_CreateString(short_message));

	gelf_text = cJSON_PrintUnformatted(gelf);
	cJSON_Delete(gelf);
	
	switch_safe_free(parsed_full_message);
	
	return gelf_text;
}

/**
 * Open connection to graylog2 server
 */
static switch_socket_t *open_graylog2_socket(const char *host, switch_port_t port, switch_memory_pool_t *pool)
{
	switch_sockaddr_t *graylog2_addr = NULL;
	switch_socket_t *graylog2_sock = NULL;

	if (switch_sockaddr_info_get(&graylog2_addr, host, SWITCH_UNSPEC, port, 0, pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Bad address: %s:%d\n", host, port);
		return NULL;
	}

	if (switch_socket_create(&graylog2_sock, switch_sockaddr_get_family(graylog2_addr), SOCK_DGRAM, 0, pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to open UDP socket\n");
		return NULL;
	}

	if (switch_socket_connect(graylog2_sock, graylog2_addr) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to connect to: %s:%d\n", host, port);
		switch_socket_close(graylog2_sock);
		return NULL;
	}

	return graylog2_sock;
}

/**
 * Thread that delivers logs to graylog2 server
 * @param thread this thread
 * @param obj unused
 * @return NULL
 */
static void *SWITCH_THREAD_FUNC deliver_graylog2_thread(switch_thread_t *thread, void *obj)
{
	switch_socket_t *graylog2_sock = NULL;
	char *log;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "graylog2 delivery thread started\n");

	switch_thread_rwlock_rdlock(globals.shutdown_rwlock);

	graylog2_sock = open_graylog2_socket(globals.server_host, globals.server_port, globals.pool);
	if (graylog2_sock) {
		while (!globals.shutdown) {
			if (switch_queue_pop(globals.log_queue, (void *)&log) == SWITCH_STATUS_SUCCESS) {
				if (!zstr(log)) {
					switch_size_t len = strlen(log);
					if (len <= 8192) {
						switch_socket_send_nonblock(graylog2_sock, (void *)log, &len);
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Skipping log with length > 8192 bytes\n");
					}
				}
				switch_safe_free(log);
			}
		}
	}

	globals.shutdown = 1;

	/* clean up remaining logs */
	while(switch_queue_trypop(globals.log_queue, (void *)&log) == SWITCH_STATUS_SUCCESS) {
		switch_safe_free(log);
	}

	if (graylog2_sock) {
		switch_socket_close(graylog2_sock);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "graylog2 delivery thread finished\n");
	switch_thread_rwlock_unlock(globals.shutdown_rwlock);

	return NULL;
}

/**
 * Create a new graylog2 delivery thread
 * @param pool to use
 */
static void start_deliver_graylog2_thread(switch_memory_pool_t *pool)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	switch_threadattr_create(&thd_attr, pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, deliver_graylog2_thread, NULL, pool);
}

/**
 * Stop graylog2 delivery thread
 */
static void stop_deliver_graylog2_thread(void)
{
	globals.shutdown = 1;
	switch_queue_interrupt_all(globals.log_queue);
	switch_thread_rwlock_wrlock(globals.shutdown_rwlock);
}

/**
 * Handle log from core
 * @param node the log
 * @param level the log level
 */
static switch_status_t mod_graylog2_logger(const switch_log_node_t *node, switch_log_level_t level)
{
	if (!globals.shutdown && level <= globals.log_level && level != SWITCH_LOG_CONSOLE) {
		if (!zstr(node->content) && !zstr(node->content + 1)) {
			char *log = to_gelf(node, level);
			if (switch_queue_trypush(globals.log_queue, log) != SWITCH_STATUS_SUCCESS) {
				free(log);
			}
		}
	}
	return SWITCH_STATUS_SUCCESS;
}

/**
 * Configure module
 */
static switch_status_t do_config(void)
{
	switch_xml_t cfg, xml, settings;

	if (!(xml = switch_xml_open_cfg("graylog2.conf", &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of graylog2.conf failed\n");
		return SWITCH_STATUS_TERM;
	}

	/* set defaults */
	globals.log_level = SWITCH_LOG_WARNING;
	globals.server_host = "127.0.0.1";
	globals.server_port = 12201;

	if ((settings = switch_xml_child(cfg, "settings"))) {
		switch_xml_t param;
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *name = (char *) switch_xml_attr_soft(param, "name");
			char *value = (char *) switch_xml_attr_soft(param, "value");

			if (zstr(name)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Ignoring empty param\n");
				continue;
			}

			if (zstr(value)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Ignoring empty value for param \"%s\"\n", name); 
				continue;
			}

			if (!strcmp(name, "server-host")) {
				globals.server_host = switch_core_strdup(globals.pool, value);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\"%s\" = \"%s\"\n", name, value); 
			} else if (!strcasecmp(name, "server-port")) {
				int port = -1;
				if (switch_is_number(value)) {
					port = atoi(value);
				}
				if (port > 0 && port <= 65535) {
					globals.server_port = port;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\"%s\" = \"%s\"\n", name, value); 
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid port: \"%s\"\n", value); 
				}
			} else if (!strcasecmp(name, "loglevel")) {
				switch_log_level_t log_level = switch_log_str2level(value);
				if (log_level == SWITCH_LOG_INVALID) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Ignoring invalid log level: \"%s\"\n", value);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\"%s\" = \"%s\"\n", name, value); 
					globals.log_level = log_level;
				}
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Ignoring unknown param: \"%s\"\n", name);
			}
		}
	}
	switch_xml_free(xml);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_graylog2_load)
{
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	memset(&globals, 0, sizeof(globals));
	globals.pool = pool;

	if (do_config() != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_TERM;
	}

	switch_thread_rwlock_create(&globals.shutdown_rwlock, pool);
	switch_queue_create(&globals.log_queue, 25000, pool);

	start_deliver_graylog2_thread(globals.pool);
	switch_log_bind_logger(mod_graylog2_logger, SWITCH_LOG_DEBUG, SWITCH_FALSE);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_graylog2_shutdown)
{
	switch_log_unbind_logger(mod_graylog2_logger);
	stop_deliver_graylog2_thread();

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

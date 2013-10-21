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
 * Seven Du <dujinfang@gmail.com>
 *
 * switch_loadable_module.c -- Loadable Modules
 *
 */

#include <switch.h>

/* for apr_pstrcat */
#include <apr_strings.h>

/* for apr_env_get and apr_env_set */
#include <apr_env.h>

/* for apr file and directory handling */
#include <apr_file_io.h>

struct switch_loadable_module {
	char *key;
	char *filename;
	int perm;
	switch_loadable_module_interface_t *module_interface;
	switch_dso_lib_t lib;
	switch_module_load_t switch_module_load;
	switch_module_runtime_t switch_module_runtime;
	switch_module_shutdown_t switch_module_shutdown;
	switch_memory_pool_t *pool;
	switch_status_t status;
	switch_thread_t *thread;
	switch_bool_t shutting_down;
};

struct switch_loadable_module_container {
	switch_hash_t *module_hash;
	switch_hash_t *endpoint_hash;
	switch_hash_t *codec_hash;
	switch_hash_t *dialplan_hash;
	switch_hash_t *timer_hash;
	switch_hash_t *application_hash;
	switch_hash_t *chat_application_hash;
	switch_hash_t *api_hash;
	switch_hash_t *json_api_hash;
	switch_hash_t *file_hash;
	switch_hash_t *speech_hash;
	switch_hash_t *asr_hash;
	switch_hash_t *directory_hash;
	switch_hash_t *chat_hash;
	switch_hash_t *say_hash;
	switch_hash_t *management_hash;
	switch_hash_t *limit_hash;
	switch_hash_t *secondary_recover_hash;
	switch_mutex_t *mutex;
	switch_memory_pool_t *pool;
};

static struct switch_loadable_module_container loadable_modules;
static switch_status_t do_shutdown(switch_loadable_module_t *module, switch_bool_t shutdown, switch_bool_t unload, switch_bool_t fail_if_busy,
								   const char **err);
static switch_status_t switch_loadable_module_load_module_ex(char *dir, char *fname, switch_bool_t runtime, switch_bool_t global, const char **err);

static void *SWITCH_THREAD_FUNC switch_loadable_module_exec(switch_thread_t *thread, void *obj)
{


	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_core_thread_session_t *ts = obj;
	switch_loadable_module_t *module = ts->objs[0];
	int restarts;

	switch_assert(thread != NULL);
	switch_assert(module != NULL);

	for (restarts = 0; status != SWITCH_STATUS_TERM && !module->shutting_down; restarts++) {
		status = module->switch_module_runtime();
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Thread ended for %s\n", module->module_interface->module_name);

	if (ts->pool) {
		switch_memory_pool_t *pool = ts->pool;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Destroying Pool for %s\n", module->module_interface->module_name);
		switch_core_destroy_memory_pool(&pool);
	}
	switch_thread_exit(thread, 0);
	return NULL;
}


static void switch_loadable_module_runtime(void)
{
	switch_hash_index_t *hi;
	void *val;
	switch_loadable_module_t *module;

	switch_mutex_lock(loadable_modules.mutex);
	for (hi = switch_hash_first(NULL, loadable_modules.module_hash); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, NULL, NULL, &val);
		module = (switch_loadable_module_t *) val;

		if (module->switch_module_runtime) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Starting runtime thread for %s\n", module->module_interface->module_name);
			module->thread = switch_core_launch_thread(switch_loadable_module_exec, module, loadable_modules.pool);
		}
	}
	switch_mutex_unlock(loadable_modules.mutex);
}

static switch_status_t switch_loadable_module_process(char *key, switch_loadable_module_t *new_module)
{
	switch_event_t *event;
	int added = 0;

	new_module->key = switch_core_strdup(new_module->pool, key);

	switch_mutex_lock(loadable_modules.mutex);
	switch_core_hash_insert(loadable_modules.module_hash, key, new_module);

	if (new_module->module_interface->endpoint_interface) {
		const switch_endpoint_interface_t *ptr;
		for (ptr = new_module->module_interface->endpoint_interface; ptr; ptr = ptr->next) {
			if (!ptr->interface_name) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to load endpoint interface from %s due to no interface name.\n", key);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Adding Endpoint '%s'\n", ptr->interface_name);
				switch_core_hash_insert(loadable_modules.endpoint_hash, ptr->interface_name, (const void *) ptr);
				if (switch_event_create(&event, SWITCH_EVENT_MODULE_LOAD) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "type", "endpoint");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "name", ptr->interface_name);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "key", new_module->key);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "filename", new_module->filename);
					switch_event_fire(&event);
					added++;
				}
			}
		}
	}

	if (new_module->module_interface->codec_interface) {
		const switch_codec_implementation_t *impl;
		const switch_codec_interface_t *ptr;

		for (ptr = new_module->module_interface->codec_interface; ptr; ptr = ptr->next) {
			if (!ptr->interface_name) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to load codec interface from %s due to no interface name.\n", key);
			} else {
				unsigned load_interface = 1;
				for (impl = ptr->implementations; impl; impl = impl->next) {
					if (!impl->iananame) {
						load_interface = 0;
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT,
										  "Failed to load codec interface %s from %s due to no iana name in an implementation.\n", ptr->interface_name,
										  key);
						break;
					}
					if (impl->decoded_bytes_per_packet > SWITCH_RECOMMENDED_BUFFER_SIZE) {
						load_interface = 0;
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT,
										  "Failed to load codec interface %s from %s due to bytes per frame %d exceeding buffer size %d.\n", 
										  ptr->interface_name,
										  key, impl->decoded_bytes_per_packet, SWITCH_RECOMMENDED_BUFFER_SIZE);
						break;
					}
				}
				if (load_interface) {
					for (impl = ptr->implementations; impl; impl = impl->next) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,
										  "Adding Codec %s %d %s %dhz %dms %dbps\n",
										  impl->iananame, impl->ianacode,
										  ptr->interface_name, impl->actual_samples_per_second, impl->microseconds_per_packet / 1000, impl->bits_per_second);
						if (!switch_core_hash_find(loadable_modules.codec_hash, impl->iananame)) {
							switch_core_hash_insert(loadable_modules.codec_hash, impl->iananame, (const void *) ptr);
						}
					}
					if (switch_event_create(&event, SWITCH_EVENT_MODULE_LOAD) == SWITCH_STATUS_SUCCESS) {
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "type", "codec");
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "name", ptr->interface_name);
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "key", new_module->key);
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "filename", new_module->filename);
						switch_event_fire(&event);
						added++;
					}
				}
			}
		}
	}

	if (new_module->module_interface->dialplan_interface) {
		const switch_dialplan_interface_t *ptr;

		for (ptr = new_module->module_interface->dialplan_interface; ptr; ptr = ptr->next) {
			if (!ptr->interface_name) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to load dialplan interface from %s due to no interface name.\n", key);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Adding Dialplan '%s'\n", ptr->interface_name);
				if (switch_event_create(&event, SWITCH_EVENT_MODULE_LOAD) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "type", "dialplan");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "name", ptr->interface_name);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "key", new_module->key);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "filename", new_module->filename);
					switch_event_fire(&event);
					added++;
				}
				switch_core_hash_insert(loadable_modules.dialplan_hash, ptr->interface_name, (const void *) ptr);
			}
		}
	}

	if (new_module->module_interface->timer_interface) {
		const switch_timer_interface_t *ptr;

		for (ptr = new_module->module_interface->timer_interface; ptr; ptr = ptr->next) {
			if (!ptr->interface_name) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to load timer interface from %s due to no interface name.\n", key);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Adding Timer '%s'\n", ptr->interface_name);
				if (switch_event_create(&event, SWITCH_EVENT_MODULE_LOAD) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "type", "timer");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "name", ptr->interface_name);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "key", new_module->key);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "filename", new_module->filename);
					switch_event_fire(&event);
					added++;
				}
				switch_core_hash_insert(loadable_modules.timer_hash, ptr->interface_name, (const void *) ptr);
			}
		}
	}

	if (new_module->module_interface->application_interface) {
		const switch_application_interface_t *ptr;

		for (ptr = new_module->module_interface->application_interface; ptr; ptr = ptr->next) {
			if (!ptr->interface_name) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to load application interface from %s due to no interface name.\n", key);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Adding Application '%s'\n", ptr->interface_name);
				if (switch_event_create(&event, SWITCH_EVENT_MODULE_LOAD) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "type", "application");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "name", ptr->interface_name);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "description", switch_str_nil(ptr->short_desc));
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "syntax", switch_str_nil(ptr->syntax));
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "key", new_module->key);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "filename", new_module->filename);
					switch_event_fire(&event);
					added++;
				}
				switch_core_hash_insert(loadable_modules.application_hash, ptr->interface_name, (const void *) ptr);
			}
		}
	}

	if (new_module->module_interface->chat_application_interface) {
		const switch_chat_application_interface_t *ptr;

		for (ptr = new_module->module_interface->chat_application_interface; ptr; ptr = ptr->next) {
			if (!ptr->interface_name) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to load application interface from %s due to no interface name.\n", key);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Adding Chat Application '%s'\n", ptr->interface_name);
				if (switch_event_create(&event, SWITCH_EVENT_MODULE_LOAD) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "type", "application");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "name", ptr->interface_name);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "description", switch_str_nil(ptr->short_desc));
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "syntax", switch_str_nil(ptr->syntax));
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "key", new_module->key);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "filename", new_module->filename);
					switch_event_fire(&event);
					added++;
				}
				switch_core_hash_insert(loadable_modules.chat_application_hash, ptr->interface_name, (const void *) ptr);
			}
		}
	}

	if (new_module->module_interface->api_interface) {
		const switch_api_interface_t *ptr;

		for (ptr = new_module->module_interface->api_interface; ptr; ptr = ptr->next) {
			if (!ptr->interface_name) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to load api interface from %s due to no interface name.\n", key);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Adding API Function '%s'\n", ptr->interface_name);
				if (switch_event_create(&event, SWITCH_EVENT_MODULE_LOAD) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "type", "api");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "name", ptr->interface_name);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "description", switch_str_nil(ptr->desc));
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "syntax", switch_str_nil(ptr->syntax));
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "key", new_module->key);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "filename", new_module->filename);
					switch_event_fire(&event);
					added++;
				}
				switch_core_hash_insert(loadable_modules.api_hash, ptr->interface_name, (const void *) ptr);
			}
		}
	}

	if (new_module->module_interface->json_api_interface) {
		const switch_json_api_interface_t *ptr;

		for (ptr = new_module->module_interface->json_api_interface; ptr; ptr = ptr->next) {
			if (!ptr->interface_name) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to load JSON api interface from %s due to no interface name.\n", key);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Adding JSON API Function '%s'\n", ptr->interface_name);
				if (switch_event_create(&event, SWITCH_EVENT_MODULE_LOAD) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "type", "json_api");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "name", ptr->interface_name);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "description", switch_str_nil(ptr->desc));
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "syntax", switch_str_nil(ptr->syntax));
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "key", new_module->key);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "filename", new_module->filename);
					switch_event_fire(&event);
					added++;
				}
				switch_core_hash_insert(loadable_modules.json_api_hash, ptr->interface_name, (const void *) ptr);
			}
		}
	}

	if (new_module->module_interface->file_interface) {
		const switch_file_interface_t *ptr;

		for (ptr = new_module->module_interface->file_interface; ptr; ptr = ptr->next) {
			if (!ptr->interface_name) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to load file interface from %s due to no interface name.\n", key);
			} else if (!ptr->extens) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to load file interface from %s due to no file extensions.\n", key);
			} else {
				int i;
				for (i = 0; ptr->extens[i]; i++) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Adding File Format '%s'\n", ptr->extens[i]);
					if (switch_event_create(&event, SWITCH_EVENT_MODULE_LOAD) == SWITCH_STATUS_SUCCESS) {
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "type", "file");
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "name", ptr->extens[i]);
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "key", new_module->key);
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "filename", new_module->filename);
						switch_event_fire(&event);
						added++;
					}
					switch_core_hash_insert(loadable_modules.file_hash, ptr->extens[i], (const void *) ptr);
				}
			}
		}
	}

	if (new_module->module_interface->speech_interface) {
		const switch_speech_interface_t *ptr;

		for (ptr = new_module->module_interface->speech_interface; ptr; ptr = ptr->next) {
			if (!ptr->interface_name) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to load speech interface from %s due to no interface name.\n", key);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Adding Speech interface '%s'\n", ptr->interface_name);
				if (switch_event_create(&event, SWITCH_EVENT_MODULE_LOAD) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "type", "speech");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "name", ptr->interface_name);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "key", new_module->key);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "filename", new_module->filename);
					switch_event_fire(&event);
					added++;
				}
				switch_core_hash_insert(loadable_modules.speech_hash, ptr->interface_name, (const void *) ptr);
			}
		}
	}

	if (new_module->module_interface->asr_interface) {
		const switch_asr_interface_t *ptr;

		for (ptr = new_module->module_interface->asr_interface; ptr; ptr = ptr->next) {
			if (!ptr->interface_name) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to load asr interface from %s due to no interface name.\n", key);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Adding ASR interface '%s'\n", ptr->interface_name);
				if (switch_event_create(&event, SWITCH_EVENT_MODULE_LOAD) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "type", "asr");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "name", ptr->interface_name);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "key", new_module->key);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "filename", new_module->filename);
					switch_event_fire(&event);
					added++;
				}
				switch_core_hash_insert(loadable_modules.asr_hash, ptr->interface_name, (const void *) ptr);
			}
		}
	}

	if (new_module->module_interface->directory_interface) {
		const switch_directory_interface_t *ptr;

		for (ptr = new_module->module_interface->directory_interface; ptr; ptr = ptr->next) {
			if (!ptr->interface_name) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to load directory interface from %s due to no interface name.\n", key);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Adding Directory interface '%s'\n", ptr->interface_name);
				if (switch_event_create(&event, SWITCH_EVENT_MODULE_LOAD) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "type", "directory");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "name", ptr->interface_name);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "key", new_module->key);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "filename", new_module->filename);
					switch_event_fire(&event);
					added++;
				}
				switch_core_hash_insert(loadable_modules.directory_hash, ptr->interface_name, (const void *) ptr);
			}
		}
	}

	if (new_module->module_interface->chat_interface) {
		const switch_chat_interface_t *ptr;

		for (ptr = new_module->module_interface->chat_interface; ptr; ptr = ptr->next) {
			if (!ptr->interface_name) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to load chat interface from %s due to no interface name.\n", key);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Adding Chat interface '%s'\n", ptr->interface_name);
				if (switch_event_create(&event, SWITCH_EVENT_MODULE_LOAD) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "type", "chat");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "name", ptr->interface_name);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "key", new_module->key);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "filename", new_module->filename);
					switch_event_fire(&event);
					added++;
				}
				switch_core_hash_insert(loadable_modules.chat_hash, ptr->interface_name, (const void *) ptr);
			}
		}
	}

	if (new_module->module_interface->say_interface) {
		const switch_say_interface_t *ptr;

		for (ptr = new_module->module_interface->say_interface; ptr; ptr = ptr->next) {
			if (!ptr->interface_name) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to load say interface from %s due to no interface name.\n", key);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Adding Say interface '%s'\n", ptr->interface_name);
				if (switch_event_create(&event, SWITCH_EVENT_MODULE_LOAD) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "type", "say");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "name", ptr->interface_name);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "key", new_module->key);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "filename", new_module->filename);
					switch_event_fire(&event);
					added++;
				}
				switch_core_hash_insert(loadable_modules.say_hash, ptr->interface_name, (const void *) ptr);
			}
		}
	}

	if (new_module->module_interface->management_interface) {
		const switch_management_interface_t *ptr;

		for (ptr = new_module->module_interface->management_interface; ptr; ptr = ptr->next) {
			if (!ptr->relative_oid) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to load management interface from %s due to no interface name.\n", key);
			} else {
				if (switch_core_hash_find(loadable_modules.management_hash, ptr->relative_oid)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT,
									  "Failed to load management interface %s. OID %s already exists\n", key, ptr->relative_oid);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,
									  "Adding Management interface '%s' OID[%s.%s]\n", key, FREESWITCH_OID_PREFIX, ptr->relative_oid);
					switch_core_hash_insert(loadable_modules.management_hash, ptr->relative_oid, (const void *) ptr);
					if (switch_event_create(&event, SWITCH_EVENT_MODULE_LOAD) == SWITCH_STATUS_SUCCESS) {
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "type", "management");
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "name", ptr->relative_oid);
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "key", new_module->key);
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "filename", new_module->filename);
						switch_event_fire(&event);
						added++;
					}
				}

			}
		}
	}
	if (new_module->module_interface->limit_interface) {
		const switch_limit_interface_t *ptr;

		for (ptr = new_module->module_interface->limit_interface; ptr; ptr = ptr->next) {
			if (!ptr->interface_name) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to load limit interface from %s due to no interface name.\n", key);
			} else {
				if (switch_core_hash_find(loadable_modules.limit_hash, ptr->interface_name)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT,
									  "Failed to load limit interface %s. Name %s already exists\n", key, ptr->interface_name);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,
									  "Adding Limit interface '%s'\n", ptr->interface_name);
					switch_core_hash_insert(loadable_modules.limit_hash, ptr->interface_name, (const void *) ptr);
					if (switch_event_create(&event, SWITCH_EVENT_MODULE_LOAD) == SWITCH_STATUS_SUCCESS) {
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "type", "limit");
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "name", ptr->interface_name);
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "key", new_module->key);
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "filename", new_module->filename);
						switch_event_fire(&event);
						added++;
					}
				}

			}
		}
	}

	if (!added) {
		if (switch_event_create(&event, SWITCH_EVENT_MODULE_LOAD) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "type", "generic");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "name", new_module->key);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "key", new_module->key);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "filename", new_module->filename);
			switch_event_fire(&event);
			added++;
		}
	}

	switch_mutex_unlock(loadable_modules.mutex);
	return SWITCH_STATUS_SUCCESS;

}

#define CHAT_MAX_MSG_QUEUE 101
#define CHAT_QUEUE_SIZE 5000

static struct {
	switch_queue_t *msg_queue[CHAT_MAX_MSG_QUEUE];
	switch_thread_t *msg_queue_thread[CHAT_MAX_MSG_QUEUE];
	int msg_queue_len;
	switch_mutex_t *mutex;
	switch_memory_pool_t *pool;
	int running;
} chat_globals;

static int IDX = 0;


static switch_status_t do_chat_send(switch_event_t *message_event)

{
	switch_chat_interface_t *ci;
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_hash_index_t *hi;
	switch_event_t *dup = NULL;
	const void *var;
	void *val;
	const char *proto;
	const char *replying;
	const char *dest_proto;
	int do_skip = 0;

	/*

	const char *from; 
	const char *to;
	const char *subject;
	const char *body;
	const char *type;
	const char *hint;
	*/		



	dest_proto = switch_event_get_header(message_event, "dest_proto");

	if (!dest_proto) {
		return SWITCH_STATUS_FALSE;
	}

	/*

	from = switch_event_get_header(message_event, "from");
	to = switch_event_get_header(message_event, "to");
	subject = switch_event_get_header(message_event, "subject");
	body = switch_event_get_body(message_event);
	type = switch_event_get_header(message_event, "type");
	hint = switch_event_get_header(message_event, "hint");
	*/

	if (!(proto = switch_event_get_header(message_event, "proto"))) {
		proto = "global";
		switch_event_add_header_string(message_event, SWITCH_STACK_BOTTOM, "proto", proto);
	}


	replying = switch_event_get_header(message_event, "replying");
	
	if (!switch_true(replying) && !switch_stristr("global", proto) && !switch_true(switch_event_get_header(message_event, "skip_global_process"))) {
		switch_mutex_lock(loadable_modules.mutex);
		for (hi = switch_hash_first(NULL, loadable_modules.chat_hash); hi; hi = switch_hash_next(hi)) {
			switch_hash_this(hi, &var, NULL, &val);
			
			if ((ci = (switch_chat_interface_t *) val)) {
				if (ci->chat_send && !strncasecmp(ci->interface_name, "GLOBAL_", 7)) {
					status = ci->chat_send(message_event);
					if (status == SWITCH_STATUS_SUCCESS) {
						/* The event was handled by an extension in the chatplan, 
						 * so the event will be duplicated, modified and queued again, 
						 * but it won't be processed by the chatplan again.
						 * So this copy of the event can be destroyed by the caller.
						 */ 
						switch_mutex_unlock(loadable_modules.mutex);
						return SWITCH_STATUS_SUCCESS;
					} else if (status == SWITCH_STATUS_BREAK) {
						/* The event went through the chatplan, but no extension matched
						 * to handle the sms messsage. It'll be attempted to be delivered
						 * directly, and unless that works the sms delivery will have failed.
						 */
						do_skip = 1;
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Chat Interface Error [%s]!\n", dest_proto);
						break;
					}
				}
			}
		}
		switch_mutex_unlock(loadable_modules.mutex);
	}
	
	if (!do_skip && !switch_stristr("GLOBAL", dest_proto)) {
		if ((ci = switch_loadable_module_get_chat_interface(dest_proto)) && ci->chat_send) {
			status = ci->chat_send(message_event);
			UNPROTECT_INTERFACE(ci);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid chat interface [%s]!\n", dest_proto);
			status = SWITCH_STATUS_FALSE;
		}
	}


	switch_event_dup(&dup, message_event);

	if ( switch_true(switch_event_get_header(message_event, "blocking")) ) {
		if (status == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(dup, SWITCH_STACK_BOTTOM, "Delivery-Failure", "false");
		} else {
			switch_event_add_header_string(dup, SWITCH_STACK_BOTTOM, "Delivery-Failure", "true");
		}
	} else {
		switch_event_add_header_string(dup, SWITCH_STACK_BOTTOM, "Nonblocking-Delivery", "true");
	}

	switch_event_fire(&dup);
	return status;
}

static switch_status_t chat_process_event(switch_event_t **eventp)
{
	switch_event_t *event;
	switch_status_t status;

	switch_assert(eventp);

	event = *eventp;
	*eventp = NULL;

	status = do_chat_send(event);
	switch_event_destroy(&event);

	return status;
}


void *SWITCH_THREAD_FUNC chat_thread_run(switch_thread_t *thread, void *obj)
{
	void *pop;
	switch_queue_t *q = (switch_queue_t *) obj;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Chat Thread Started\n");


	while(switch_queue_pop(q, &pop) == SWITCH_STATUS_SUCCESS && pop) {
		switch_event_t *event = (switch_event_t *) pop;
		chat_process_event(&event);
		switch_cond_next();
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Chat Thread Ended\n");

	return NULL;	
}


static void chat_thread_start(int idx)
{

	if (idx >= CHAT_MAX_MSG_QUEUE || (idx < chat_globals.msg_queue_len && chat_globals.msg_queue_thread[idx])) {
		return;
	}

	switch_mutex_lock(chat_globals.mutex);
	
	if (idx >= chat_globals.msg_queue_len) {
		int i;
		chat_globals.msg_queue_len = idx + 1;

		for (i = 0; i < chat_globals.msg_queue_len; i++) {
			if (!chat_globals.msg_queue[i]) {
				switch_threadattr_t *thd_attr = NULL;

				switch_queue_create(&chat_globals.msg_queue[i], CHAT_QUEUE_SIZE, chat_globals.pool);

				switch_threadattr_create(&thd_attr, chat_globals.pool);
				switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
				switch_thread_create(&chat_globals.msg_queue_thread[i], 
									 thd_attr, 
									 chat_thread_run, 
									 chat_globals.msg_queue[i], 
									 chat_globals.pool);
			}
		}
	}

	switch_mutex_unlock(chat_globals.mutex);
}


static void chat_queue_message(switch_event_t **eventp)
{
	int idx = 0;
	switch_event_t *event;

	switch_assert(eventp);

	event = *eventp;
	*eventp = NULL;
	
	if (chat_globals.running == 0) {
		chat_process_event(&event);
		return;
	}

 again:

	switch_mutex_lock(chat_globals.mutex);
	idx = IDX;
	IDX++; 
	if (IDX >= chat_globals.msg_queue_len) IDX = 0;
	switch_mutex_unlock(chat_globals.mutex);
	
	chat_thread_start(idx);

	if (switch_queue_trypush(chat_globals.msg_queue[idx], event) != SWITCH_STATUS_SUCCESS) {
		if (chat_globals.msg_queue_len < CHAT_MAX_MSG_QUEUE) {
			chat_thread_start(idx + 1);
			goto again;
		} else {
			switch_queue_push(chat_globals.msg_queue[idx], event);
		}
	}
}


SWITCH_DECLARE(switch_status_t) switch_core_execute_chat_app(switch_event_t *message, const char *app, const char *data)
{
	switch_chat_application_interface_t *cai;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	char *expanded;

	if (!(cai = switch_loadable_module_get_chat_application_interface(app)) || !cai->chat_application_function) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid chat application interface [%s]!\n", app);
		return SWITCH_STATUS_FALSE;
	}

	if (switch_test_flag(message, EF_NO_CHAT_EXEC)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Message is not allowed to execute apps\n");
		switch_goto_status(SWITCH_STATUS_FALSE, end);
	}

	if (data && !strcmp(data, "__undef")) {
		data = NULL;
	}

	expanded = switch_event_expand_headers(message, data);
	
	status = cai->chat_application_function(message, expanded);

	if (expanded != data) {
		free(expanded);
	}

 end:

	UNPROTECT_INTERFACE(cai);

	return status;

}



SWITCH_DECLARE(switch_status_t) switch_core_chat_send_args(const char *dest_proto, const char *proto, const char *from, const char *to,
														   const char *subject, const char *body, const char *type, const char *hint, switch_bool_t blocking)
{
	switch_event_t *message_event;
	switch_status_t status;

	if (switch_event_create(&message_event, SWITCH_EVENT_MESSAGE) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header_string(message_event, SWITCH_STACK_BOTTOM, "proto", proto);
		switch_event_add_header_string(message_event, SWITCH_STACK_BOTTOM, "from", from);
		switch_event_add_header_string(message_event, SWITCH_STACK_BOTTOM, "to", to);
		switch_event_add_header_string(message_event, SWITCH_STACK_BOTTOM, "subject", subject);
		switch_event_add_header_string(message_event, SWITCH_STACK_BOTTOM, "type", type);
		switch_event_add_header_string(message_event, SWITCH_STACK_BOTTOM, "hint", hint);
		switch_event_add_header_string(message_event, SWITCH_STACK_BOTTOM, "skip_global_process", "true");
		if (blocking) {
			switch_event_add_header_string(message_event, SWITCH_STACK_BOTTOM, "blocking", "true");
		}
		
		if (body) {
			switch_event_add_body(message_event, "%s", body);
		}
	} else {
		abort();
	}	

	if (dest_proto) {
		switch_event_add_header_string(message_event, SWITCH_STACK_BOTTOM, "dest_proto", dest_proto);
	}

	
	if (blocking) {
		status = chat_process_event(&message_event);
	} else {
		chat_queue_message(&message_event);
		status = SWITCH_STATUS_SUCCESS;
	}

	return status;
	
}


SWITCH_DECLARE(switch_status_t) switch_core_chat_send(const char *dest_proto, switch_event_t *message_event)
{
	switch_event_t *dup;

	switch_event_dup(&dup, message_event);

	if (dest_proto) {
		switch_event_add_header_string(dup, SWITCH_STACK_BOTTOM, "dest_proto", dest_proto);
	}

	chat_queue_message(&dup);
	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(switch_status_t) switch_core_chat_deliver(const char *dest_proto, switch_event_t **message_event)
{

	if (dest_proto) {
		switch_event_add_header_string(*message_event, SWITCH_STACK_BOTTOM, "dest_proto", dest_proto);
	}

	chat_queue_message(message_event);

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t switch_loadable_module_unprocess(switch_loadable_module_t *old_module)
{
	switch_event_t *event;
	int removed = 0;

	switch_mutex_lock(loadable_modules.mutex);

	if (old_module->module_interface->endpoint_interface) {
		const switch_endpoint_interface_t *ptr;

		for (ptr = old_module->module_interface->endpoint_interface; ptr; ptr = ptr->next) {
			if (ptr->interface_name) {

				switch_core_session_hupall_endpoint(ptr, SWITCH_CAUSE_MANAGER_REQUEST);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Write lock interface '%s' to wait for existing references.\n",
								  ptr->interface_name);
				if (switch_thread_rwlock_trywrlock_timeout(ptr->rwlock, 10) == SWITCH_STATUS_SUCCESS) {
					switch_thread_rwlock_unlock(ptr->rwlock);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Giving up on '%s' waiting for existing references.\n", ptr->interface_name);
				}

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Deleting Endpoint '%s'\n", ptr->interface_name);
				if (switch_event_create(&event, SWITCH_EVENT_MODULE_UNLOAD) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "type", "endpoint");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "name", ptr->interface_name);
					switch_event_fire(&event);
					removed++;
				}
				switch_core_hash_delete(loadable_modules.endpoint_hash, ptr->interface_name);
			}
		}
	}

	if (old_module->module_interface->codec_interface) {
		const switch_codec_implementation_t *impl;
		const switch_codec_interface_t *ptr;

		for (ptr = old_module->module_interface->codec_interface; ptr; ptr = ptr->next) {
			if (ptr->interface_name) {
				unsigned load_interface = 1;
				for (impl = ptr->implementations; impl; impl = impl->next) {
					if (!impl->iananame) {
						load_interface = 0;
						break;
					}
				}
				if (load_interface) {
					for (impl = ptr->implementations; impl; impl = impl->next) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,
										  "Deleting Codec %s %d %s %dhz %dms\n",
										  impl->iananame, impl->ianacode,
										  ptr->interface_name, impl->actual_samples_per_second, impl->microseconds_per_packet / 1000);
						switch_core_session_hupall_matching_var("read_codec", impl->iananame, SWITCH_CAUSE_MANAGER_REQUEST);
						switch_core_session_hupall_matching_var("write_codec", impl->iananame, SWITCH_CAUSE_MANAGER_REQUEST);
						if (switch_core_hash_find(loadable_modules.codec_hash, impl->iananame)) {
							switch_core_hash_delete(loadable_modules.codec_hash, impl->iananame);
						}
					}
					if (switch_event_create(&event, SWITCH_EVENT_MODULE_UNLOAD) == SWITCH_STATUS_SUCCESS) {
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "type", "codec");
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "name", ptr->interface_name);
						switch_event_fire(&event);
						removed++;
					}
				}
			}
		}
	}

	if (old_module->module_interface->dialplan_interface) {
		const switch_dialplan_interface_t *ptr;

		for (ptr = old_module->module_interface->dialplan_interface; ptr; ptr = ptr->next) {
			if (ptr->interface_name) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Deleting Dialplan '%s'\n", ptr->interface_name);
				if (switch_event_create(&event, SWITCH_EVENT_MODULE_UNLOAD) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "type", "dialplan");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "name", ptr->interface_name);
					switch_event_fire(&event);
					removed++;
				}
				switch_core_hash_delete(loadable_modules.dialplan_hash, ptr->interface_name);
			}
		}
	}

	if (old_module->module_interface->timer_interface) {
		const switch_timer_interface_t *ptr;

		for (ptr = old_module->module_interface->timer_interface; ptr; ptr = ptr->next) {
			if (ptr->interface_name) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Deleting Timer '%s'\n", ptr->interface_name);
				if (switch_event_create(&event, SWITCH_EVENT_MODULE_UNLOAD) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "type", "timer");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "name", ptr->interface_name);
					switch_event_fire(&event);
					removed++;
				}
				switch_core_hash_delete(loadable_modules.timer_hash, ptr->interface_name);
			}
		}
	}

	if (old_module->module_interface->application_interface) {
		const switch_application_interface_t *ptr;
		for (ptr = old_module->module_interface->application_interface; ptr; ptr = ptr->next) {
			if (ptr->interface_name) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Deleting Application '%s'\n", ptr->interface_name);
				switch_core_session_hupall_matching_var(SWITCH_CURRENT_APPLICATION_VARIABLE, ptr->interface_name, SWITCH_CAUSE_MANAGER_REQUEST);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Write lock interface '%s' to wait for existing references.\n",
								  ptr->interface_name);
				if (switch_thread_rwlock_trywrlock_timeout(ptr->rwlock, 10) == SWITCH_STATUS_SUCCESS) {
					switch_thread_rwlock_unlock(ptr->rwlock);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Giving up on '%s' waiting for existing references.\n", ptr->interface_name);
				}

				if (switch_event_create(&event, SWITCH_EVENT_MODULE_UNLOAD) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "type", "application");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "name", ptr->interface_name);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "description", switch_str_nil(ptr->short_desc));
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "syntax", switch_str_nil(ptr->syntax));
					switch_event_fire(&event);
					removed++;
				}
				switch_core_hash_delete(loadable_modules.application_hash, ptr->interface_name);
			}
		}
	}

	if (old_module->module_interface->chat_application_interface) {
		const switch_chat_application_interface_t *ptr;
		for (ptr = old_module->module_interface->chat_application_interface; ptr; ptr = ptr->next) {
			if (ptr->interface_name) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Deleting Application '%s'\n", ptr->interface_name);
				switch_core_session_hupall_matching_var(SWITCH_CURRENT_APPLICATION_VARIABLE, ptr->interface_name, SWITCH_CAUSE_MANAGER_REQUEST);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Write lock interface '%s' to wait for existing references.\n",
								  ptr->interface_name);
				if (switch_thread_rwlock_trywrlock_timeout(ptr->rwlock, 10) == SWITCH_STATUS_SUCCESS) {
					switch_thread_rwlock_unlock(ptr->rwlock);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Giving up on '%s' waiting for existing references.\n", ptr->interface_name);
				}

				if (switch_event_create(&event, SWITCH_EVENT_MODULE_UNLOAD) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "type", "application");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "name", ptr->interface_name);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "description", switch_str_nil(ptr->short_desc));
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "syntax", switch_str_nil(ptr->syntax));
					switch_event_fire(&event);
					removed++;
				}
				switch_core_hash_delete(loadable_modules.chat_application_hash, ptr->interface_name);
			}
		}
	}

	if (old_module->module_interface->api_interface) {
		const switch_api_interface_t *ptr;

		for (ptr = old_module->module_interface->api_interface; ptr; ptr = ptr->next) {
			if (ptr->interface_name) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Deleting API Function '%s'\n", ptr->interface_name);

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Write lock interface '%s' to wait for existing references.\n",
								  ptr->interface_name);

				if (switch_thread_rwlock_trywrlock_timeout(ptr->rwlock, 10) == SWITCH_STATUS_SUCCESS) {
					switch_thread_rwlock_unlock(ptr->rwlock);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Giving up on '%s' waiting for existing references.\n", ptr->interface_name);
				}


				if (switch_event_create(&event, SWITCH_EVENT_MODULE_UNLOAD) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "type", "api");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "name", ptr->interface_name);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "description", switch_str_nil(ptr->desc));
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "syntax", switch_str_nil(ptr->syntax));
					switch_event_fire(&event);
					removed++;
				}
				switch_core_hash_delete(loadable_modules.api_hash, ptr->interface_name);
			}
		}
	}

	if (old_module->module_interface->json_api_interface) {
		const switch_json_api_interface_t *ptr;

		for (ptr = old_module->module_interface->json_api_interface; ptr; ptr = ptr->next) {
			if (ptr->interface_name) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Deleting API Function '%s'\n", ptr->interface_name);

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Write lock interface '%s' to wait for existing references.\n",
								  ptr->interface_name);

				if (switch_thread_rwlock_trywrlock_timeout(ptr->rwlock, 10) == SWITCH_STATUS_SUCCESS) {
					switch_thread_rwlock_unlock(ptr->rwlock);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Giving up on '%s' waiting for existing references.\n", ptr->interface_name);
				}


				if (switch_event_create(&event, SWITCH_EVENT_MODULE_UNLOAD) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "type", "api");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "name", ptr->interface_name);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "description", switch_str_nil(ptr->desc));
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "syntax", switch_str_nil(ptr->syntax));
					switch_event_fire(&event);
					removed++;
				}
				switch_core_hash_delete(loadable_modules.json_api_hash, ptr->interface_name);
			}
		}
	}

	if (old_module->module_interface->file_interface) {
		const switch_file_interface_t *ptr;

		for (ptr = old_module->module_interface->file_interface; ptr; ptr = ptr->next) {
			if (ptr->interface_name) {
				int i;

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Write lock interface '%s' to wait for existing references.\n",
								  ptr->interface_name);

				if (switch_thread_rwlock_trywrlock_timeout(ptr->rwlock, 10) == SWITCH_STATUS_SUCCESS) {
					switch_thread_rwlock_unlock(ptr->rwlock);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Giving up on '%s' waiting for existing references.\n", ptr->interface_name);
				}

				for (i = 0; ptr->extens[i]; i++) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Deleting File Format '%s'\n", ptr->extens[i]);
					if (switch_event_create(&event, SWITCH_EVENT_MODULE_UNLOAD) == SWITCH_STATUS_SUCCESS) {
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "type", "file");
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "name", ptr->extens[i]);
						switch_event_fire(&event);
						removed++;
					}
					switch_core_hash_delete(loadable_modules.file_hash, ptr->extens[i]);
				}
			}
		}
	}

	if (old_module->module_interface->speech_interface) {
		const switch_speech_interface_t *ptr;

		for (ptr = old_module->module_interface->speech_interface; ptr; ptr = ptr->next) {

			if (ptr->interface_name) {

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Write lock interface '%s' to wait for existing references.\n",
								  ptr->interface_name);

				if (switch_thread_rwlock_trywrlock_timeout(ptr->rwlock, 10) == SWITCH_STATUS_SUCCESS) {
					switch_thread_rwlock_unlock(ptr->rwlock);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Giving up on '%s' waiting for existing references.\n", ptr->interface_name);
				}

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Deleting Speech interface '%s'\n", ptr->interface_name);
				if (switch_event_create(&event, SWITCH_EVENT_MODULE_UNLOAD) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "type", "speech");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "name", ptr->interface_name);
					switch_event_fire(&event);
					removed++;
				}
				switch_core_hash_delete(loadable_modules.speech_hash, ptr->interface_name);
			}
		}
	}

	if (old_module->module_interface->asr_interface) {
		const switch_asr_interface_t *ptr;

		for (ptr = old_module->module_interface->asr_interface; ptr; ptr = ptr->next) {
			if (ptr->interface_name) {

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Write lock interface '%s' to wait for existing references.\n",
								  ptr->interface_name);

				if (switch_thread_rwlock_trywrlock_timeout(ptr->rwlock, 10) == SWITCH_STATUS_SUCCESS) {
					switch_thread_rwlock_unlock(ptr->rwlock);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Giving up on '%s' waiting for existing references.\n", ptr->interface_name);
				}

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Deleting Asr interface '%s'\n", ptr->interface_name);
				if (switch_event_create(&event, SWITCH_EVENT_MODULE_UNLOAD) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "type", "asr");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "name", ptr->interface_name);
					switch_event_fire(&event);
					removed++;
				}
				switch_core_hash_delete(loadable_modules.asr_hash, ptr->interface_name);
			}
		}
	}

	if (old_module->module_interface->directory_interface) {
		const switch_directory_interface_t *ptr;

		for (ptr = old_module->module_interface->directory_interface; ptr; ptr = ptr->next) {
			if (ptr->interface_name) {

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Write lock interface '%s' to wait for existing references.\n",
								  ptr->interface_name);

				if (switch_thread_rwlock_trywrlock_timeout(ptr->rwlock, 10) == SWITCH_STATUS_SUCCESS) {
					switch_thread_rwlock_unlock(ptr->rwlock);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Giving up on '%s' waiting for existing references.\n", ptr->interface_name);
				}

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Deleting Directory interface '%s'\n", ptr->interface_name);
				if (switch_event_create(&event, SWITCH_EVENT_MODULE_UNLOAD) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "type", "directory");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "name", ptr->interface_name);
					switch_event_fire(&event);
					removed++;
				}
				switch_core_hash_delete(loadable_modules.directory_hash, ptr->interface_name);
			}
		}
	}


	if (old_module->module_interface->chat_interface) {
		const switch_chat_interface_t *ptr;

		for (ptr = old_module->module_interface->chat_interface; ptr; ptr = ptr->next) {
			if (ptr->interface_name) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Write lock interface '%s' to wait for existing references.\n",
								  ptr->interface_name);

				if (switch_thread_rwlock_trywrlock_timeout(ptr->rwlock, 10) == SWITCH_STATUS_SUCCESS) {
					switch_thread_rwlock_unlock(ptr->rwlock);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Giving up on '%s' waiting for existing references.\n", ptr->interface_name);
				}

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Deleting Chat interface '%s'\n", ptr->interface_name);
				if (switch_event_create(&event, SWITCH_EVENT_MODULE_UNLOAD) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "type", "chat");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "name", ptr->interface_name);
					switch_event_fire(&event);
					removed++;
				}
				switch_core_hash_delete(loadable_modules.chat_hash, ptr->interface_name);
			}
		}
	}

	if (old_module->module_interface->say_interface) {
		const switch_say_interface_t *ptr;

		for (ptr = old_module->module_interface->say_interface; ptr; ptr = ptr->next) {
			if (ptr->interface_name) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Write lock interface '%s' to wait for existing references.\n",
								  ptr->interface_name);

				if (switch_thread_rwlock_trywrlock_timeout(ptr->rwlock, 10) == SWITCH_STATUS_SUCCESS) {
					switch_thread_rwlock_unlock(ptr->rwlock);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Giving up on '%s' waiting for existing references.\n", ptr->interface_name);
				}
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Deleting Say interface '%s'\n", ptr->interface_name);
				if (switch_event_create(&event, SWITCH_EVENT_MODULE_UNLOAD) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "type", "say");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "name", ptr->interface_name);
					switch_event_fire(&event);
					removed++;
				}
				switch_core_hash_delete(loadable_modules.say_hash, ptr->interface_name);
			}
		}
	}

	if (old_module->module_interface->management_interface) {
		const switch_management_interface_t *ptr;

		for (ptr = old_module->module_interface->management_interface; ptr; ptr = ptr->next) {
			if (ptr->relative_oid) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,
								  "Deleting Management interface '%s' OID[%s.%s]\n", old_module->key, FREESWITCH_OID_PREFIX, ptr->relative_oid);
				switch_core_hash_delete(loadable_modules.management_hash, ptr->relative_oid);
				if (switch_event_create(&event, SWITCH_EVENT_MODULE_UNLOAD) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "type", "management");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "name", ptr->relative_oid);
					switch_event_fire(&event);
					removed++;
				}
			}
		}
	}

	if (old_module->module_interface->limit_interface) {
		const switch_limit_interface_t *ptr;

		for (ptr = old_module->module_interface->limit_interface; ptr; ptr = ptr->next) {
			if (ptr->interface_name) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,
								  "Deleting Limit interface '%s'\n", ptr->interface_name);
				switch_core_hash_delete(loadable_modules.limit_hash, ptr->interface_name);
				if (switch_event_create(&event, SWITCH_EVENT_MODULE_UNLOAD) == SWITCH_STATUS_SUCCESS) {
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "type", "limit");
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "name", ptr->interface_name);
					switch_event_fire(&event);
					removed++;
				}
			}
		}
	}

	if (!removed) {
		if (switch_event_create(&event, SWITCH_EVENT_MODULE_UNLOAD) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "type", "generic");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "name", old_module->key);
			switch_event_fire(&event);
			removed++;
		}
	}
	switch_mutex_unlock(loadable_modules.mutex);

	return SWITCH_STATUS_SUCCESS;

}


static switch_status_t switch_loadable_module_load_file(char *path, char *filename, switch_bool_t global, switch_loadable_module_t **new_module)
{
	switch_loadable_module_t *module = NULL;
	switch_dso_lib_t dso = NULL;
	apr_status_t status = SWITCH_STATUS_SUCCESS;
	switch_loadable_module_function_table_t *interface_struct_handle = NULL;
	switch_loadable_module_function_table_t *mod_interface_functions = NULL;
	char *struct_name = NULL;
	switch_module_load_t load_func_ptr = NULL;
	int loading = 1;
	switch_loadable_module_interface_t *module_interface = NULL;
	char *derr = NULL;
	const char *err = NULL;
	switch_memory_pool_t *pool = NULL;
	switch_bool_t load_global = global;

	switch_assert(path != NULL);

	switch_core_new_memory_pool(&pool);
	*new_module = NULL;

	struct_name = switch_core_sprintf(pool, "%s_module_interface", filename);

#ifdef WIN32
	dso = switch_dso_open("FreeSwitch.dll", load_global, &derr);
#elif defined (MACOSX) || defined(DARWIN)
	{
		char *lib_path = switch_mprintf("%s/libfreeswitch.dylib", SWITCH_GLOBAL_dirs.lib_dir);
		dso = switch_dso_open(lib_path, load_global, &derr);
		switch_safe_free(lib_path);
	}
#else
	dso = switch_dso_open(NULL, load_global, &derr);
#endif
	if (!derr && dso) {
		interface_struct_handle = switch_dso_data_sym(dso, struct_name, &derr);
	}

	switch_safe_free(derr)

		if (!interface_struct_handle) {
		dso = switch_dso_open(path, load_global, &derr);
	}

	while (loading) {
		if (derr) {
			err = derr;
			break;
		}

		if (!interface_struct_handle) {
			interface_struct_handle = switch_dso_data_sym(dso, struct_name, &derr);
		}

		if (derr) {
			err = derr;
			break;
		}

		if (interface_struct_handle && interface_struct_handle->switch_api_version != SWITCH_API_VERSION) {
			err = "Trying to load an out of date module, please rebuild the module.";
			break;
		}

		if (!load_global && interface_struct_handle && switch_test_flag(interface_struct_handle, SMODF_GLOBAL_SYMBOLS)) {
			load_global = SWITCH_TRUE;
			switch_dso_destroy(&dso);
			interface_struct_handle = NULL;
			dso = switch_dso_open(path, load_global, &derr);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Loading module with global namespace at request of module\n");
			continue;
		}

		if (interface_struct_handle) {
			mod_interface_functions = interface_struct_handle;
			load_func_ptr = mod_interface_functions->load;
		}

		if (load_func_ptr == NULL) {
			err = "Cannot locate symbol 'switch_module_load' please make sure this is a valid module.";
			break;
		}

		status = load_func_ptr(&module_interface, pool);

		if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_NOUNLOAD) {
			err = "Module load routine returned an error";
			module_interface = NULL;
			break;
		}

		if (!module_interface) {
			err = "Module failed to initialize its module_interface. Is this a valid module?";
			break;
		}

		if ((module = switch_core_alloc(pool, sizeof(switch_loadable_module_t))) == 0) {
			err = "Could not allocate memory\n";
			abort();
		}

		if (status == SWITCH_STATUS_NOUNLOAD) {
			module->perm++;
		}

		loading = 0;
	}


	if (err) {

		if (dso) {
			switch_dso_destroy(&dso);
		}

		if (pool) {
			switch_core_destroy_memory_pool(&pool);
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Error Loading module %s\n**%s**\n", path, err);
		switch_safe_free(derr);
		return SWITCH_STATUS_GENERR;
	}

	module->pool = pool;
	module->filename = switch_core_strdup(module->pool, path);
	module->module_interface = module_interface;
	module->switch_module_load = load_func_ptr;

	if (mod_interface_functions) {
		module->switch_module_shutdown = mod_interface_functions->shutdown;
		module->switch_module_runtime = mod_interface_functions->runtime;
	}

	module->lib = dso;

	*new_module = module;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Successfully Loaded [%s]\n", module_interface->module_name);

	switch_core_set_signal_handlers();

	return SWITCH_STATUS_SUCCESS;

}
SWITCH_DECLARE(switch_status_t) switch_loadable_module_load_module(char *dir, char *fname, switch_bool_t runtime, const char **err)
{
	return switch_loadable_module_load_module_ex(dir, fname, runtime, SWITCH_FALSE, err);
}

static switch_status_t switch_loadable_module_load_module_ex(char *dir, char *fname, switch_bool_t runtime, switch_bool_t global, const char **err)
{
	switch_size_t len = 0;
	char *path;
	char *file, *dot;
	switch_loadable_module_t *new_module = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

#ifdef WIN32
	const char *ext = ".dll";
#else
	const char *ext = ".so";
#endif

	*err = "";

	if ((file = switch_core_strdup(loadable_modules.pool, fname)) == 0) {
		*err = "allocation error";
		return SWITCH_STATUS_FALSE;
	}

	if (switch_is_file_path(file)) {
		path = switch_core_strdup(loadable_modules.pool, file);
		file = (char *) switch_cut_path(file);
		if ((dot = strchr(file, '.'))) {
			*dot = '\0';
		}
	} else {
		if ((dot = strchr(file, '.'))) {
			*dot = '\0';
		}
		len = strlen(dir);
		len += strlen(file);
		len += 8;
		path = (char *) switch_core_alloc(loadable_modules.pool, len);
		switch_snprintf(path, len, "%s%s%s%s", dir, SWITCH_PATH_SEPARATOR, file, ext);
	}


	if (switch_core_hash_find_locked(loadable_modules.module_hash, file, loadable_modules.mutex)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Module %s Already Loaded!\n", file);
		*err = "Module already loaded";
		status = SWITCH_STATUS_FALSE;
	} else if ((status = switch_loadable_module_load_file(path, file, global, &new_module)) == SWITCH_STATUS_SUCCESS) {
		if ((status = switch_loadable_module_process(file, new_module)) == SWITCH_STATUS_SUCCESS && runtime) {
			if (new_module->switch_module_runtime) {
				new_module->thread = switch_core_launch_thread(switch_loadable_module_exec, new_module, new_module->pool);
			}
		} else if (status != SWITCH_STATUS_SUCCESS) {
			*err = "module load routine returned an error";
		}
	} else {
		*err = "module load file routine returned an error";
	}


	return status;

}

SWITCH_DECLARE(switch_status_t) switch_loadable_module_exists(const char *mod)
{
	switch_status_t status;

	if (zstr(mod)) {
		return SWITCH_STATUS_FALSE;
	}

	switch_mutex_lock(loadable_modules.mutex);
	if (switch_core_hash_find(loadable_modules.module_hash, mod)) {
		status = SWITCH_STATUS_SUCCESS;
	} else {
		status = SWITCH_STATUS_FALSE;
	}
	switch_mutex_unlock(loadable_modules.mutex);

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_loadable_module_unload_module(char *dir, char *fname, switch_bool_t force, const char **err)
{
	switch_loadable_module_t *module = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (force) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Spin the barrel and pull the trigger.......!\n");
	}

	switch_mutex_lock(loadable_modules.mutex);
	if ((module = switch_core_hash_find(loadable_modules.module_hash, fname))) {
		if (module->perm) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Module is not unloadable.\n");
			*err = "Module is not unloadable";
			status = SWITCH_STATUS_NOUNLOAD;
			goto unlock;
		} else {
			/* Prevent anything from using the module while it's shutting down */
			switch_core_hash_delete(loadable_modules.module_hash, fname);
			switch_mutex_unlock(loadable_modules.mutex);
			if ((status = do_shutdown(module, SWITCH_TRUE, SWITCH_TRUE, !force, err)) != SWITCH_STATUS_SUCCESS) {
				/* Something went wrong in the module's shutdown function, add it again */
				switch_core_hash_insert_locked(loadable_modules.module_hash, fname, module, loadable_modules.mutex);
			}
			goto end;
		}
	} else {
		*err = "No such module!";
		status = SWITCH_STATUS_FALSE;
	}
unlock:
	switch_mutex_unlock(loadable_modules.mutex);
  end:
	if (force) {
		switch_yield(1000000);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "PHEW!\n");
	}

	return status;

}

SWITCH_DECLARE(switch_status_t) switch_loadable_module_enumerate_available(const char *dir_path, switch_modulename_callback_func_t callback, void *user_data)
{
	switch_dir_t *dir = NULL;
	switch_status_t status;
	char buffer[256];
	const char *fname;
	const char *fname_ext;
	char *fname_base;

#ifdef WIN32
	const char *ext = ".dll";
#else
	const char *ext = ".so";
#endif

	if ((status = switch_dir_open(&dir, dir_path, loadable_modules.pool)) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	while((fname = switch_dir_next_file(dir, buffer, sizeof(buffer)))) {
		if ((fname_ext = strrchr(fname, '.'))) {
			if (!strcmp(fname_ext, ext)) {
				if (!(fname_base = switch_mprintf("%.*s", (int)(fname_ext-fname), fname))) {
					status = SWITCH_STATUS_GENERR;
					goto end;
				}
				callback(user_data, fname_base);
				switch_safe_free(fname_base)
			}
		}
	}


  end:
	switch_dir_close(dir);
	return status;
}

SWITCH_DECLARE(switch_status_t) switch_loadable_module_enumerate_loaded(switch_modulename_callback_func_t callback, void *user_data)
{
	switch_hash_index_t *hi;
	void *val;
	switch_loadable_module_t *module;

	switch_mutex_lock(loadable_modules.mutex);
	for (hi = switch_hash_first(NULL, loadable_modules.module_hash); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, NULL, NULL, &val);
		module = (switch_loadable_module_t *) val;

		callback(user_data, module->module_interface->module_name);
	}
	switch_mutex_unlock(loadable_modules.mutex);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_loadable_module_build_dynamic(char *filename,
																	 switch_module_load_t switch_module_load,
																	 switch_module_runtime_t switch_module_runtime,
																	 switch_module_shutdown_t switch_module_shutdown, switch_bool_t runtime)
{
	switch_loadable_module_t *module = NULL;
	switch_module_load_t load_func_ptr = NULL;
	int loading = 1;
	const char *err = NULL;
	switch_loadable_module_interface_t *module_interface = NULL;
	switch_memory_pool_t *pool;


	if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "OH OH no pool\n");
		abort();
	}

	if ((module = switch_core_alloc(pool, sizeof(switch_loadable_module_t))) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Couldn't allocate memory\n");
		abort();
	}



	while (loading) {
		switch_status_t status;
		load_func_ptr = (switch_module_load_t) switch_module_load;

		if (load_func_ptr == NULL) {
			err = "Cannot Load";
			break;
		}

		status = load_func_ptr(&module_interface, pool);

		if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_NOUNLOAD) {
			err = "Module load routine returned an error";
			module_interface = NULL;
			break;
		}

		if ((module = switch_core_alloc(pool, sizeof(switch_loadable_module_t))) == 0) {
			err = "Could not allocate memory\n";
			abort();
		}

		if (status == SWITCH_STATUS_NOUNLOAD) {
			module->perm++;
		}

		loading = 0;
	}

	if (err) {
		switch_core_destroy_memory_pool(&pool);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Error Loading module %s\n**%s**\n", filename, err);
		return SWITCH_STATUS_GENERR;
	}

	module->pool = pool;
	module->filename = switch_core_strdup(module->pool, filename);
	module->module_interface = module_interface;
	module->switch_module_load = load_func_ptr;

	if (switch_module_shutdown) {
		module->switch_module_shutdown = switch_module_shutdown;
	}
	if (switch_module_runtime) {
		module->switch_module_runtime = switch_module_runtime;
	}
	if (runtime && module->switch_module_runtime) {
		module->thread = switch_core_launch_thread(switch_loadable_module_exec, module, module->pool);
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Successfully Loaded [%s]\n", module_interface->module_name);
	return switch_loadable_module_process((char *) module->filename, module);
}

#ifdef WIN32
static void switch_loadable_module_path_init()
{
	char *path = NULL, *working = NULL;
	apr_dir_t *perl_dir_handle = NULL;

	apr_env_get(&path, "path", loadable_modules.pool);
	apr_filepath_get(&working, APR_FILEPATH_NATIVE, loadable_modules.pool);

	if (apr_dir_open(&perl_dir_handle, ".\\perl", loadable_modules.pool) == APR_SUCCESS) {
		apr_dir_close(perl_dir_handle);
		apr_env_set("path", apr_pstrcat(loadable_modules.pool, path, ";", working, "\\perl", NULL), loadable_modules.pool);
	}
}
#endif

SWITCH_DECLARE(switch_status_t) switch_loadable_module_init(switch_bool_t autoload)
{

	apr_finfo_t finfo = { 0 };
	apr_dir_t *module_dir_handle = NULL;
	apr_int32_t finfo_flags = APR_FINFO_DIRENT | APR_FINFO_TYPE | APR_FINFO_NAME;
	char *cf = "modules.conf";
	char *pcf = "post_load_modules.conf";
	switch_xml_t cfg, xml;
	unsigned char all = 0;
	unsigned int count = 0;
	const char *err;


#ifdef WIN32
	const char *ext = ".dll";
	const char *EXT = ".DLL";
#elif defined (MACOSX) || defined (DARWIN)
	const char *ext = ".dylib";
	const char *EXT = ".DYLIB";
#else
	const char *ext = ".so";
	const char *EXT = ".SO";
#endif

	memset(&loadable_modules, 0, sizeof(loadable_modules));
	switch_core_new_memory_pool(&loadable_modules.pool);


#ifdef WIN32
	switch_loadable_module_path_init();
#endif

	switch_core_hash_init(&loadable_modules.module_hash, loadable_modules.pool);
	switch_core_hash_init_nocase(&loadable_modules.endpoint_hash, loadable_modules.pool);
	switch_core_hash_init_nocase(&loadable_modules.codec_hash, loadable_modules.pool);
	switch_core_hash_init_nocase(&loadable_modules.timer_hash, loadable_modules.pool);
	switch_core_hash_init_nocase(&loadable_modules.application_hash, loadable_modules.pool);
	switch_core_hash_init_nocase(&loadable_modules.chat_application_hash, loadable_modules.pool);
	switch_core_hash_init_nocase(&loadable_modules.api_hash, loadable_modules.pool);
	switch_core_hash_init_nocase(&loadable_modules.json_api_hash, loadable_modules.pool);
	switch_core_hash_init(&loadable_modules.file_hash, loadable_modules.pool);
	switch_core_hash_init_nocase(&loadable_modules.speech_hash, loadable_modules.pool);
	switch_core_hash_init_nocase(&loadable_modules.asr_hash, loadable_modules.pool);
	switch_core_hash_init_nocase(&loadable_modules.directory_hash, loadable_modules.pool);
	switch_core_hash_init_nocase(&loadable_modules.chat_hash, loadable_modules.pool);
	switch_core_hash_init_nocase(&loadable_modules.say_hash, loadable_modules.pool);
	switch_core_hash_init_nocase(&loadable_modules.management_hash, loadable_modules.pool);
	switch_core_hash_init_nocase(&loadable_modules.limit_hash, loadable_modules.pool);
	switch_core_hash_init_nocase(&loadable_modules.dialplan_hash, loadable_modules.pool);
	switch_core_hash_init(&loadable_modules.secondary_recover_hash, loadable_modules.pool);
	switch_mutex_init(&loadable_modules.mutex, SWITCH_MUTEX_NESTED, loadable_modules.pool);

	if (!autoload) return SWITCH_STATUS_SUCCESS;

	switch_loadable_module_load_module("", "CORE_SOFTTIMER_MODULE", SWITCH_FALSE, &err);
	switch_loadable_module_load_module("", "CORE_PCM_MODULE", SWITCH_FALSE, &err);


	if ((xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_xml_t mods, ld;
		if ((mods = switch_xml_child(cfg, "modules"))) {
			for (ld = switch_xml_child(mods, "load"); ld; ld = ld->next) {
				switch_bool_t global = SWITCH_FALSE;
				const char *val = switch_xml_attr_soft(ld, "module");
				const char *path = switch_xml_attr_soft(ld, "path");
				const char *critical = switch_xml_attr_soft(ld, "critical");
				const char *sglobal = switch_xml_attr_soft(ld, "global");
				if (zstr(val) || (strchr(val, '.') && !strstr(val, ext) && !strstr(val, EXT))) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Invalid extension for %s\n", val);
					continue;
				}
				global = switch_true(sglobal);
				
				if (path && zstr(path)) {
					path = SWITCH_GLOBAL_dirs.mod_dir;
				}
				if (switch_loadable_module_load_module_ex((char *) path, (char *) val, SWITCH_FALSE, global, &err) == SWITCH_STATUS_GENERR) {
					if (critical && switch_true(critical)) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to load critical module '%s', abort()\n", val);
						abort();
					}
				}
				count++;
			}
		}
		switch_xml_free(xml);

	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "open of %s failed\n", cf);
	}

	if ((xml = switch_xml_open_cfg(pcf, &cfg, NULL))) {
		switch_xml_t mods, ld;

		if ((mods = switch_xml_child(cfg, "modules"))) {
			for (ld = switch_xml_child(mods, "load"); ld; ld = ld->next) {
				switch_bool_t global = SWITCH_FALSE;
				const char *val = switch_xml_attr_soft(ld, "module");
				const char *path = switch_xml_attr_soft(ld, "path");
				const char *sglobal = switch_xml_attr_soft(ld, "global");
				if (zstr(val) || (strchr(val, '.') && !strstr(val, ext) && !strstr(val, EXT))) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Invalid extension for %s\n", val);
					continue;
				}
				global = switch_true(sglobal);

				if (path && zstr(path)) {
					path = SWITCH_GLOBAL_dirs.mod_dir;
				}
				switch_loadable_module_load_module_ex((char *) path, (char *) val, SWITCH_FALSE, global, &err);
				count++;
			}
		}
		switch_xml_free(xml);

	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "open of %s failed\n", pcf);
	}

	if (!count) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "No modules loaded, assuming 'load all'\n");
		all = 1;
	}

	if (all) {
		if (apr_dir_open(&module_dir_handle, SWITCH_GLOBAL_dirs.mod_dir, loadable_modules.pool) != APR_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Can't open directory: %s\n", SWITCH_GLOBAL_dirs.mod_dir);
			return SWITCH_STATUS_GENERR;
		}

		while (apr_dir_read(&finfo, finfo_flags, module_dir_handle) == APR_SUCCESS) {
			const char *fname = finfo.fname;

			if (finfo.filetype != APR_REG) {
				continue;
			}

			if (!fname) {
				fname = finfo.name;
			}

			if (!fname) {
				continue;
			}

			if (zstr(fname) || (!strstr(fname, ext) && !strstr(fname, EXT))) {
				continue;
			}

			switch_loadable_module_load_module((char *) SWITCH_GLOBAL_dirs.mod_dir, (char *) fname, SWITCH_FALSE, &err);
		}
		apr_dir_close(module_dir_handle);
	}

	switch_loadable_module_runtime();

	memset(&chat_globals, 0, sizeof(chat_globals));
	chat_globals.running = 1;
	chat_globals.pool = loadable_modules.pool;
	switch_mutex_init(&chat_globals.mutex, SWITCH_MUTEX_NESTED, chat_globals.pool);

	chat_thread_start(1);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t do_shutdown(switch_loadable_module_t *module, switch_bool_t shutdown, switch_bool_t unload, switch_bool_t fail_if_busy,
								   const char **err)
{
	int32_t flags = switch_core_flags();
	switch_assert(module != NULL);

	if (fail_if_busy && module->module_interface->rwlock && switch_thread_rwlock_trywrlock(module->module_interface->rwlock) != SWITCH_STATUS_SUCCESS) {
		if (err) {
			*err = "Module in use.";
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Module %s is in use, cannot unload.\n", module->module_interface->module_name);
		return SWITCH_STATUS_FALSE;
	}

	module->shutting_down = SWITCH_TRUE;

	if (shutdown) {
		switch_loadable_module_unprocess(module);
		if (module->switch_module_shutdown) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Stopping: %s\n", module->module_interface->module_name);
			module->status = module->switch_module_shutdown();
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "%s has no shutdown routine\n", module->module_interface->module_name);
		}
	}

	if (fail_if_busy && module->module_interface->rwlock) {
		switch_thread_rwlock_unlock(module->module_interface->rwlock);
	}

	if (unload && module->status != SWITCH_STATUS_NOUNLOAD && !(flags & SCF_VG)) {
		switch_memory_pool_t *pool;
		switch_status_t st;

		if (module->thread) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "%s stopping runtime thread.\n", module->module_interface->module_name);
			switch_thread_join(&st, module->thread);
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "%s unloaded.\n", module->module_interface->module_name);
		switch_dso_destroy(&module->lib);
		if ((pool = module->pool)) {
			module = NULL;
			switch_core_destroy_memory_pool(&pool);
		}
	}

	return SWITCH_STATUS_SUCCESS;

}

SWITCH_DECLARE(void) switch_loadable_module_shutdown(void)
{
	switch_hash_index_t *hi;
	void *val;
	switch_loadable_module_t *module;
	int i;

	if (!loadable_modules.module_hash) {
		return;
	}

	chat_globals.running = 0;

	for (i = 0; i < chat_globals.msg_queue_len; i++) {	
		switch_queue_push(chat_globals.msg_queue[i], NULL);
	}

	for (i = 0; i < chat_globals.msg_queue_len; i++) {
		switch_status_t st;
		switch_thread_join(&st, chat_globals.msg_queue_thread[i]);
	}


	for (hi = switch_hash_first(NULL, loadable_modules.module_hash); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, NULL, NULL, &val);
		module = (switch_loadable_module_t *) val;
		if (!module->perm) {
			do_shutdown(module, SWITCH_TRUE, SWITCH_FALSE, SWITCH_FALSE, NULL);
		}
	}

	switch_yield(1000000);

	for (hi = switch_hash_first(NULL, loadable_modules.module_hash); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, NULL, NULL, &val);
		module = (switch_loadable_module_t *) val;
		if (!module->perm) {
			do_shutdown(module, SWITCH_FALSE, SWITCH_TRUE, SWITCH_FALSE, NULL);
		}
	}

	switch_core_hash_destroy(&loadable_modules.module_hash);
	switch_core_hash_destroy(&loadable_modules.endpoint_hash);
	switch_core_hash_destroy(&loadable_modules.codec_hash);
	switch_core_hash_destroy(&loadable_modules.timer_hash);
	switch_core_hash_destroy(&loadable_modules.application_hash);
	switch_core_hash_destroy(&loadable_modules.chat_application_hash);
	switch_core_hash_destroy(&loadable_modules.api_hash);
	switch_core_hash_destroy(&loadable_modules.json_api_hash);
	switch_core_hash_destroy(&loadable_modules.file_hash);
	switch_core_hash_destroy(&loadable_modules.speech_hash);
	switch_core_hash_destroy(&loadable_modules.asr_hash);
	switch_core_hash_destroy(&loadable_modules.directory_hash);
	switch_core_hash_destroy(&loadable_modules.chat_hash);
	switch_core_hash_destroy(&loadable_modules.say_hash);
	switch_core_hash_destroy(&loadable_modules.management_hash);
	switch_core_hash_destroy(&loadable_modules.limit_hash);
	switch_core_hash_destroy(&loadable_modules.dialplan_hash);

	switch_core_destroy_memory_pool(&loadable_modules.pool);
}

SWITCH_DECLARE(switch_endpoint_interface_t *) switch_loadable_module_get_endpoint_interface(const char *name)
{
	switch_endpoint_interface_t *ptr;

	switch_mutex_lock(loadable_modules.mutex);
	ptr = switch_core_hash_find(loadable_modules.endpoint_hash, name);
	if (ptr) {
		PROTECT_INTERFACE(ptr);
	}
	switch_mutex_unlock(loadable_modules.mutex);


	return ptr;
}

SWITCH_DECLARE(switch_codec_interface_t *) switch_loadable_module_get_codec_interface(const char *name)
{
	char altname[256] = "";
	switch_codec_interface_t *codec;
	switch_size_t x;

	switch_mutex_lock(loadable_modules.mutex);
	if (!(codec = switch_core_hash_find(loadable_modules.codec_hash, name))) {
		for (x = 0; x < strlen(name); x++) {
			altname[x] = (char) toupper((int) name[x]);
		}
		if (!(codec = switch_core_hash_find(loadable_modules.codec_hash, altname))) {
			for (x = 0; x < strlen(name); x++) {
				altname[x] = (char) tolower((int) name[x]);
			}
			codec = switch_core_hash_find(loadable_modules.codec_hash, altname);
		}
	}
	switch_mutex_unlock(loadable_modules.mutex);

	if (codec) {
		PROTECT_INTERFACE(codec);
	}

	return codec;
}

#define HASH_FUNC(_kind_) SWITCH_DECLARE(switch_##_kind_##_interface_t *) switch_loadable_module_get_##_kind_##_interface(const char *name)	\
	{																	\
		switch_##_kind_##_interface_t *i;								\
		if ((i = switch_core_hash_find_locked(loadable_modules._kind_##_hash, name, loadable_modules.mutex))) {	\
			PROTECT_INTERFACE(i);										\
		}																\
		return i;														\
	}

HASH_FUNC(dialplan)
HASH_FUNC(timer)
HASH_FUNC(application)
HASH_FUNC(chat_application)
HASH_FUNC(api)
HASH_FUNC(json_api)
HASH_FUNC(file)
HASH_FUNC(speech)
HASH_FUNC(asr)
HASH_FUNC(directory)
HASH_FUNC(chat)
HASH_FUNC(limit)


SWITCH_DECLARE(switch_say_interface_t *) switch_loadable_module_get_say_interface(const char *name)
{
	return switch_core_hash_find_locked(loadable_modules.say_hash, name, loadable_modules.mutex);
}

SWITCH_DECLARE(switch_management_interface_t *) switch_loadable_module_get_management_interface(const char *relative_oid)
{
	return switch_core_hash_find_locked(loadable_modules.management_hash, relative_oid, loadable_modules.mutex);
}

#ifdef DEBUG_CODEC_SORTING
static void do_print(const switch_codec_implementation_t **array, int arraylen)
{
	int i;

	for(i = 0; i < arraylen; i++) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, 
						  "DEBUG %d %s:%d %d\n", i, array[i]->iananame, array[i]->ianacode, array[i]->microseconds_per_packet / 1000);
	}

}
#endif

/* helper only -- bounds checking enforced by caller */
static void do_swap(const switch_codec_implementation_t **array, int a, int b)
{
	const switch_codec_implementation_t *tmp = array[b];
	array[b] = array[a];
	array[a] = tmp;
}

static void switch_loadable_module_sort_codecs(const switch_codec_implementation_t **array, int arraylen)
{
	int i = 0, sorted_ptime = 0;

#ifdef DEBUG_CODEC_SORTING
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "--BEFORE\n");
	do_print(array, arraylen);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "--BEFORE\n");
#endif

	for (i = 0; i < arraylen; i++) {
		int this_ptime = array[i]->microseconds_per_packet / 1000;
		
		if (!strcasecmp(array[i]->iananame, "ilbc")) {
			this_ptime = 20;
		}

		if (!sorted_ptime) {
			sorted_ptime = this_ptime;
#ifdef DEBUG_CODEC_SORTING
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "sorted1 = %d\n", sorted_ptime);
#endif
		}

		if (i > 0 && strcasecmp(array[i]->iananame, array[i-1]->iananame) && this_ptime != sorted_ptime) {
			int j;
			int swapped = 0;

#ifdef DEBUG_CODEC_SORTING
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "%d != %d\n", this_ptime, sorted_ptime);
#endif
			for(j = i; j < arraylen; j++) {
				int check_ptime = array[j]->microseconds_per_packet / 1000;

				if (!strcasecmp(array[i]->iananame, "ilbc")) {
					check_ptime = 20;
				}

				if (check_ptime == sorted_ptime) {
#ifdef DEBUG_CODEC_SORTING
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "swap %d %d ptime %d\n", i, j, check_ptime);
#endif
					do_swap(array, i, j);
					swapped = 1;
					break;
				}
			}

			if (!swapped) {
				sorted_ptime = this_ptime;
#ifdef DEBUG_CODEC_SORTING
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "sorted2 = %d\n", sorted_ptime);
#endif
			}
		}
	}

#ifdef DEBUG_CODEC_SORTING
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "--AFTER\n");
	do_print(array, arraylen);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "--AFTER\n");
#endif

}


SWITCH_DECLARE(int) switch_loadable_module_get_codecs(const switch_codec_implementation_t **array, int arraylen)
{
	switch_hash_index_t *hi;
	void *val;
	switch_codec_interface_t *codec_interface;
	int i = 0;
	const switch_codec_implementation_t *imp;

	switch_mutex_lock(loadable_modules.mutex);
	for (hi = switch_hash_first(NULL, loadable_modules.codec_hash); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, NULL, NULL, &val);
		codec_interface = (switch_codec_interface_t *) val;
		
		/* Look for the default ptime of the codec because it's the safest choice */
		for (imp = codec_interface->implementations; imp; imp = imp->next) {
			uint32_t default_ptime = switch_default_ptime(imp->iananame, imp->ianacode);

			if (imp->microseconds_per_packet / 1000 == (int)default_ptime) {
				array[i++] = imp;
				goto found;
			}
		}
		/* oh well we will use what we have */
		array[i++] = codec_interface->implementations;

	  found:

		if (i > arraylen) {
			break;
		}
	}

	switch_mutex_unlock(loadable_modules.mutex);

	switch_loadable_module_sort_codecs(array, i);

	return i;

}

SWITCH_DECLARE(char *) switch_parse_codec_buf(char *buf, uint32_t *interval, uint32_t *rate, uint32_t *bit)
{
	char *cur, *next = NULL, *name, *p;

	name = next = cur = buf;

	for (;;) {
		if (!next) {
			break;
		}

		if ((p = strchr(next, '@'))) {
			*p++ = '\0';
		}
		next = p;

		if (cur != name) {
			if (strchr(cur, 'i')) {
				*interval = atoi(cur);
			} else if ((strchr(cur, 'k') || strchr(cur, 'h'))) {
				*rate = atoi(cur);
			} else if (strchr(cur, 'b')) {
				*bit = atoi(cur);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Bad syntax for codec string. Missing qualifier [h|k|i|b] for part [%s]!\n", cur);
			}
		}
		cur = next;
	}
	
	return name;
}

SWITCH_DECLARE(int) switch_loadable_module_get_codecs_sorted(const switch_codec_implementation_t **array, int arraylen, char **prefs, int preflen)
{
	int x, i = 0, j = 0;
	switch_codec_interface_t *codec_interface;
	const switch_codec_implementation_t *imp;

	switch_mutex_lock(loadable_modules.mutex);

	for (x = 0; x < preflen; x++) {
		char *name, buf[256], jbuf[256];
		uint32_t interval = 0, rate = 0, bit = 0;

		switch_copy_string(buf, prefs[x], sizeof(buf));
		name = switch_parse_codec_buf(buf, &interval, &rate, &bit);

		for(j = 0; j < x; j++) {
			char *jname;
			uint32_t jinterval = 0, jrate = 0, jbit = 0;
			uint32_t ointerval = interval, orate = rate;

			if (ointerval == 0) {
				ointerval = switch_default_ptime(name, 0);
			}
			
			if (orate == 0) {
				orate = switch_default_rate(name, 0);
			}

			switch_copy_string(jbuf, prefs[j], sizeof(jbuf));
			jname = switch_parse_codec_buf(jbuf, &jinterval, &jrate, &jbit);

			if (jinterval == 0) {
				jinterval = switch_default_ptime(jname, 0);
			}

			if (jrate == 0) {
				jrate = switch_default_rate(jname, 0);
			}

			if (!strcasecmp(name, jname) && ointerval == jinterval && orate == jrate) {
				goto next_x;
			}
		}

		if ((codec_interface = switch_loadable_module_get_codec_interface(name)) != 0) {
			/* If no specific codec interval is requested opt for the default above all else because lots of stuff assumes it */
			for (imp = codec_interface->implementations; imp; imp = imp->next) {
				uint32_t default_ptime = switch_default_ptime(imp->iananame, imp->ianacode);
				uint32_t default_rate = switch_default_rate(imp->iananame, imp->ianacode);
				
				if (imp->codec_type != SWITCH_CODEC_TYPE_VIDEO) {
					
					if ((!interval && (uint32_t) (imp->microseconds_per_packet / 1000) != default_ptime) ||
						(interval && (uint32_t) (imp->microseconds_per_packet / 1000) != interval)) {
						continue;
					}

					if (((!rate && (uint32_t) imp->samples_per_second != default_rate) || (rate && (uint32_t) imp->samples_per_second != rate))) {
						continue;
					}

					if (bit && (uint32_t) imp->bits_per_second != bit) {
						continue;
					}

				}


				array[i++] = imp;
				goto found;

			}

			/* Either looking for a specific interval or there was no interval specified and there wasn't one at the default ptime available */
			for (imp = codec_interface->implementations; imp; imp = imp->next) {
				if (imp->codec_type != SWITCH_CODEC_TYPE_VIDEO) {

					if (interval && (uint32_t) (imp->microseconds_per_packet / 1000) != interval) {
						continue;
					}

					if (rate && (uint32_t) imp->samples_per_second != rate) {
						continue;
					}

					if (bit && (uint32_t) imp->bits_per_second != bit) {
						continue;
					}
					
				}

				array[i++] = imp;
				goto found;

			}

		  found:

			UNPROTECT_INTERFACE(codec_interface);

			if (i > arraylen) {
				break;
			}

		}

	next_x:

		continue;
	}

	switch_mutex_unlock(loadable_modules.mutex);

	switch_loadable_module_sort_codecs(array, i);

	return i;
}

SWITCH_DECLARE(switch_status_t) switch_api_execute(const char *cmd, const char *arg, switch_core_session_t *session, switch_stream_handle_t *stream)
{
	switch_api_interface_t *api;
	switch_status_t status;
	char *arg_used;
	char *cmd_used;

	switch_assert(stream != NULL);
	switch_assert(stream->data != NULL);
	switch_assert(stream->write_function != NULL);

	if (strcasecmp(cmd, "console_complete")) {
		cmd_used = switch_strip_whitespace(cmd);
		arg_used = switch_strip_whitespace(arg);
	} else {
		cmd_used = (char *) cmd;
		arg_used = (char *) arg;
	}
			

	if (!stream->param_event) {
		switch_event_create(&stream->param_event, SWITCH_EVENT_API);
	}

	if (stream->param_event) {
		if (cmd_used && *cmd_used) {
			switch_event_add_header_string(stream->param_event, SWITCH_STACK_BOTTOM, "API-Command", cmd_used);
		}
		if (arg_used && *arg_used) {
			switch_event_add_header_string(stream->param_event, SWITCH_STACK_BOTTOM, "API-Command-Argument", arg_used);
		}
	}


	if (cmd_used && (api = switch_loadable_module_get_api_interface(cmd_used)) != 0) {
		if ((status = api->function(arg_used, session, stream)) != SWITCH_STATUS_SUCCESS) {
			stream->write_function(stream, "COMMAND RETURNED ERROR!\n");
		}
		UNPROTECT_INTERFACE(api);
	} else {
		status = SWITCH_STATUS_FALSE;
		stream->write_function(stream, "INVALID COMMAND!\n");
	}

	if (stream->param_event) {
		switch_event_fire(&stream->param_event);
	}

	if (cmd_used != cmd) {
		switch_safe_free(cmd_used);
	}
	
	if (arg_used != arg) {
		switch_safe_free(arg_used);
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_json_api_execute(cJSON *json, switch_core_session_t *session, cJSON **retval)
{
	switch_json_api_interface_t *json_api;
	switch_status_t status;
	cJSON *function, *json_reply = NULL;

	switch_assert(json);

	function = cJSON_GetObjectItem(json, "command");

	if (function && function->valuestring 
		&& cJSON_GetObjectItem(json, "data") && (json_api = switch_loadable_module_get_json_api_interface(function->valuestring)) != 0) {
		if ((status = json_api->function(json, session, &json_reply)) != SWITCH_STATUS_SUCCESS) {
			cJSON_AddItemToObject(json, "status", cJSON_CreateString("error"));
			cJSON_AddItemToObject(json, "message", cJSON_CreateString("The command returned an error"));
		} else {
			cJSON_AddItemToObject(json, "status", cJSON_CreateString("success"));
		}
		
		if (!json_reply) {
			json_reply = cJSON_CreateNull();
		}

		if (retval) {
			*retval = json_reply;
		} else {
			cJSON_AddItemToObject(json, "response", json_reply);
		}
		
		UNPROTECT_INTERFACE(json_api);
	} else {
		status = SWITCH_STATUS_FALSE;
		cJSON_AddItemToObject(json, "status", cJSON_CreateString("error"));
		cJSON_AddItemToObject(json, "message", cJSON_CreateString("Invalid request or non-existant command"));
		cJSON_AddItemToObject(json, "response", cJSON_CreateNull());
	}

	return status;
}


SWITCH_DECLARE(switch_loadable_module_interface_t *) switch_loadable_module_create_module_interface(switch_memory_pool_t *pool, const char *name)
{
	switch_loadable_module_interface_t *mod;

	mod = switch_core_alloc(pool, sizeof(switch_loadable_module_interface_t));
	switch_assert(mod != NULL);

	mod->pool = pool;

	mod->module_name = switch_core_strdup(mod->pool, name);
	switch_thread_rwlock_create(&mod->rwlock, mod->pool);
	return mod;
}

#define ALLOC_INTERFACE(_TYPE_)	{									\
		switch_##_TYPE_##_interface_t *i, *ptr;							\
		i = switch_core_alloc(mod->pool, sizeof(switch_##_TYPE_##_interface_t)); \
		switch_assert(i != NULL);												\
		for (ptr = mod->_TYPE_##_interface; ptr && ptr->next; ptr = ptr->next); \
		if (ptr) {														\
			ptr->next = i;												\
		} else {														\
			mod->_TYPE_##_interface = i;								\
		}																\
		switch_thread_rwlock_create(&i->rwlock, mod->pool);				\
		switch_mutex_init(&i->reflock, SWITCH_MUTEX_NESTED, mod->pool);	\
		i->parent = mod;												\
		return i; }


SWITCH_DECLARE(void *) switch_loadable_module_create_interface(switch_loadable_module_interface_t *mod, switch_module_interface_name_t iname)
{

	switch (iname) {
	case SWITCH_ENDPOINT_INTERFACE:
		ALLOC_INTERFACE(endpoint)

	case SWITCH_TIMER_INTERFACE:
		ALLOC_INTERFACE(timer)

	case SWITCH_DIALPLAN_INTERFACE:
		ALLOC_INTERFACE(dialplan)

	case SWITCH_CODEC_INTERFACE:
		ALLOC_INTERFACE(codec)

	case SWITCH_APPLICATION_INTERFACE:
		ALLOC_INTERFACE(application)

	case SWITCH_CHAT_APPLICATION_INTERFACE:
		ALLOC_INTERFACE(chat_application)

	case SWITCH_API_INTERFACE:
		ALLOC_INTERFACE(api)

	case SWITCH_JSON_API_INTERFACE:
		ALLOC_INTERFACE(json_api)

	case SWITCH_FILE_INTERFACE:
		ALLOC_INTERFACE(file)

	case SWITCH_SPEECH_INTERFACE:
		ALLOC_INTERFACE(speech)

	case SWITCH_DIRECTORY_INTERFACE:
		ALLOC_INTERFACE(directory)

	case SWITCH_CHAT_INTERFACE:
		ALLOC_INTERFACE(chat)

	case SWITCH_SAY_INTERFACE:
		ALLOC_INTERFACE(say)

	case SWITCH_ASR_INTERFACE:
		ALLOC_INTERFACE(asr)

	case SWITCH_MANAGEMENT_INTERFACE:
		ALLOC_INTERFACE(management)

	case SWITCH_LIMIT_INTERFACE:
		ALLOC_INTERFACE(limit)

	default:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid Module Type!\n");
		return NULL;
	}
}

struct switch_say_file_handle {
	char *ext;
	int cnt;
	struct switch_stream_handle stream;
	switch_event_t *param_event;
};
	
SWITCH_DECLARE(char *) switch_say_file_handle_get_variable(switch_say_file_handle_t *sh, const char *var)
{
	char *ret = NULL;

	if (sh->param_event) {
		ret = switch_event_get_header(sh->param_event, var);
	}

	return ret;
	
}

SWITCH_DECLARE(char *) switch_say_file_handle_get_path(switch_say_file_handle_t *sh)
{
	return (char *) sh->stream.data;
}

SWITCH_DECLARE(char *) switch_say_file_handle_detach_path(switch_say_file_handle_t *sh)
{
	char *path;
	
	switch_assert(sh);
	path = (char *) sh->stream.data;
	sh->stream.data = NULL;
	return path;
}


SWITCH_DECLARE(void) switch_say_file_handle_destroy(switch_say_file_handle_t **sh)
{
	switch_assert(sh);
	
	switch_safe_free((*sh)->stream.data);
	switch_safe_free((*sh)->ext);

	if ((*sh)->param_event) {
		switch_event_destroy(&(*sh)->param_event);
	}
	free(*sh);
	*sh = NULL;
}

SWITCH_DECLARE(switch_status_t) switch_say_file_handle_create(switch_say_file_handle_t **sh, const char *ext, switch_event_t **var_event)
{
	switch_assert(sh);

	if (zstr(ext)) {
		ext = "wav";
	}

	*sh = malloc(sizeof(**sh));
	memset(*sh, 0, sizeof(**sh));

	SWITCH_STANDARD_STREAM((*sh)->stream);
	
	if (var_event) {
		(*sh)->param_event = *var_event;
		*var_event = NULL;
	}

	(*sh)->ext = strdup(ext);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(void) switch_say_file(switch_say_file_handle_t *sh, const char *fmt, ...)
{
	char buf[256] = "";
	int ret;
	va_list ap;

	va_start(ap, fmt);
	
	if ((ret = switch_vsnprintf(buf, sizeof(buf), fmt, ap)) > 0) {
		if (!sh->cnt++) {
			sh->stream.write_function(&sh->stream, "file_string://%s.%s", buf, sh->ext);
		} else if (strstr(buf, "://")) {
			sh->stream.write_function(&sh->stream, "!%s", buf);
		} else {
			sh->stream.write_function(&sh->stream, "!%s.%s", buf, sh->ext);
		}

	}
	
	va_end(ap);
}

SWITCH_DECLARE(switch_core_recover_callback_t) switch_core_get_secondary_recover_callback(const char *key)
{
	switch_core_recover_callback_t cb;

	switch_mutex_lock(loadable_modules.mutex);
	cb = (switch_core_recover_callback_t) (intptr_t) switch_core_hash_find(loadable_modules.secondary_recover_hash, key);
	switch_mutex_unlock(loadable_modules.mutex);

	return cb;
}


SWITCH_DECLARE(switch_status_t) switch_core_register_secondary_recover_callback(const char *key, switch_core_recover_callback_t cb)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	switch_assert(cb);

	switch_mutex_lock(loadable_modules.mutex);
	if (switch_core_hash_find(loadable_modules.secondary_recover_hash, key)) {
		status = SWITCH_STATUS_FALSE;
	} else {
		switch_core_hash_insert(loadable_modules.secondary_recover_hash, key, (void *)(intptr_t) cb);
	}
	switch_mutex_unlock(loadable_modules.mutex);

	return status;
}


SWITCH_DECLARE(void) switch_core_unregister_secondary_recover_callback(const char *key)
{
	switch_mutex_lock(loadable_modules.mutex);
	switch_core_hash_delete(loadable_modules.secondary_recover_hash, key);
	switch_mutex_unlock(loadable_modules.mutex);
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

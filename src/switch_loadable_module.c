/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2010, Anthony Minessale II <anthm@freeswitch.org>
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
	switch_hash_t *api_hash;
	switch_hash_t *file_hash;
	switch_hash_t *speech_hash;
	switch_hash_t *asr_hash;
	switch_hash_t *directory_hash;
	switch_hash_t *chat_hash;
	switch_hash_t *say_hash;
	switch_hash_t *management_hash;
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
										  "Failed to load codec interface %s from %s due to bytes per frame exceeding buffer size.\n", ptr->interface_name,
										  key);
						break;
					}
				}
				if (load_interface) {
					for (impl = ptr->implementations; impl; impl = impl->next) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE,
										  "Adding Codec '%s' (%s) %dhz %dms\n",
										  impl->iananame, ptr->interface_name, impl->actual_samples_per_second, impl->microseconds_per_packet / 1000);
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
				}
				switch_core_hash_insert(loadable_modules.application_hash, ptr->interface_name, (const void *) ptr);
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
				}
				switch_core_hash_insert(loadable_modules.api_hash, ptr->interface_name, (const void *) ptr);
			}
		}
	}

	if (new_module->module_interface->file_interface) {
		const switch_file_interface_t *ptr;

		for (ptr = new_module->module_interface->file_interface; ptr; ptr = ptr->next) {
			if (!ptr->interface_name) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to load file interface from %s due to no interface name.\n", key);
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
					}
				}

			}
		}
	}

	switch_mutex_unlock(loadable_modules.mutex);
	return SWITCH_STATUS_SUCCESS;

}


static switch_status_t switch_loadable_module_unprocess(switch_loadable_module_t *old_module)
{
	switch_event_t *event;

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
										  "Deleting Codec '%s' (%s) %dhz %dms\n",
										  impl->iananame, ptr->interface_name, impl->actual_samples_per_second, impl->microseconds_per_packet / 1000);
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
				}
				switch_core_hash_delete(loadable_modules.application_hash, ptr->interface_name);
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
				}
				switch_core_hash_delete(loadable_modules.api_hash, ptr->interface_name);
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
				}
			}
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
	switch_memory_pool_t *pool;
	switch_bool_t load_global = global;

	switch_assert(path != NULL);

	switch_core_new_memory_pool(&pool);
	*new_module = NULL;

	struct_name = switch_core_sprintf(pool, "%s_module_interface", filename);

#ifdef WIN32
	dso = switch_dso_open("FreeSwitch.dll", load_global, &derr);
#elif defined (MACOSX) || defined(DARWIN)
	dso = switch_dso_open(SWITCH_PREFIX_DIR "/lib/libfreeswitch.dylib", load_global, &derr);
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

	switch_mutex_lock(loadable_modules.mutex);
	if (switch_core_hash_find(loadable_modules.module_hash, file)) {
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
	switch_mutex_unlock(loadable_modules.mutex);

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
			goto end;
		} else {
			if ((status = do_shutdown(module, SWITCH_TRUE, SWITCH_TRUE, !force, err) != SWITCH_STATUS_SUCCESS)) {
				goto end;
			}
		}
		switch_core_hash_delete(loadable_modules.module_hash, fname);
	} else {
		*err = "No such module!";
		status = SWITCH_STATUS_FALSE;
	}
  end:
	switch_mutex_unlock(loadable_modules.mutex);

	if (force) {
		switch_yield(1000000);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "PHEW!\n");
	}

	return status;

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

SWITCH_DECLARE(switch_status_t) switch_loadable_module_init()
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
	switch_core_hash_init_nocase(&loadable_modules.api_hash, loadable_modules.pool);
	switch_core_hash_init(&loadable_modules.file_hash, loadable_modules.pool);
	switch_core_hash_init_nocase(&loadable_modules.speech_hash, loadable_modules.pool);
	switch_core_hash_init_nocase(&loadable_modules.asr_hash, loadable_modules.pool);
	switch_core_hash_init_nocase(&loadable_modules.directory_hash, loadable_modules.pool);
	switch_core_hash_init_nocase(&loadable_modules.chat_hash, loadable_modules.pool);
	switch_core_hash_init_nocase(&loadable_modules.say_hash, loadable_modules.pool);
	switch_core_hash_init_nocase(&loadable_modules.management_hash, loadable_modules.pool);
	switch_core_hash_init_nocase(&loadable_modules.dialplan_hash, loadable_modules.pool);
	switch_mutex_init(&loadable_modules.mutex, SWITCH_MUTEX_NESTED, loadable_modules.pool);

	switch_loadable_module_load_module("", "CORE_SOFTTIMER_MODULE", SWITCH_FALSE, &err);
	switch_loadable_module_load_module("", "CORE_PCM_MODULE", SWITCH_FALSE, &err);

	if ((xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_xml_t mods, ld;
		if ((mods = switch_xml_child(cfg, "modules"))) {
			for (ld = switch_xml_child(mods, "load"); ld; ld = ld->next) {
				switch_bool_t global = SWITCH_FALSE;
				const char *val = switch_xml_attr_soft(ld, "module");
				const char *sglobal = switch_xml_attr_soft(ld, "global");
				if (zstr(val) || (strchr(val, '.') && !strstr(val, ext) && !strstr(val, EXT))) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Invalid extension for %s\n", val);
					continue;
				}
				global = switch_true(sglobal);
				switch_loadable_module_load_module_ex((char *) SWITCH_GLOBAL_dirs.mod_dir, (char *) val, SWITCH_FALSE, global, &err);
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
				const char *sglobal = switch_xml_attr_soft(ld, "global");
				if (zstr(val) || (strchr(val, '.') && !strstr(val, ext) && !strstr(val, EXT))) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Invalid extension for %s\n", val);
					continue;
				}
				global = switch_true(sglobal);
				switch_loadable_module_load_module_ex((char *) SWITCH_GLOBAL_dirs.mod_dir, (char *) val, SWITCH_FALSE, global, &err);
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

	if (!loadable_modules.module_hash) {
		return;
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
	switch_core_hash_destroy(&loadable_modules.api_hash);
	switch_core_hash_destroy(&loadable_modules.file_hash);
	switch_core_hash_destroy(&loadable_modules.speech_hash);
	switch_core_hash_destroy(&loadable_modules.asr_hash);
	switch_core_hash_destroy(&loadable_modules.directory_hash);
	switch_core_hash_destroy(&loadable_modules.chat_hash);
	switch_core_hash_destroy(&loadable_modules.say_hash);
	switch_core_hash_destroy(&loadable_modules.management_hash);
	switch_core_hash_destroy(&loadable_modules.dialplan_hash);

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
	HASH_FUNC(api)
	HASH_FUNC(file)
	HASH_FUNC(speech)
	HASH_FUNC(asr)
	HASH_FUNC(directory)
	HASH_FUNC(chat)


	SWITCH_DECLARE(switch_say_interface_t *) switch_loadable_module_get_say_interface(const char *name)
{
	return switch_core_hash_find_locked(loadable_modules.say_hash, name, loadable_modules.mutex);
}

SWITCH_DECLARE(switch_management_interface_t *) switch_loadable_module_get_management_interface(const char *relative_oid)
{
	return switch_core_hash_find_locked(loadable_modules.management_hash, relative_oid, loadable_modules.mutex);
}

SWITCH_DECLARE(int) switch_loadable_module_get_codecs(const switch_codec_implementation_t **array, int arraylen)
{
	switch_hash_index_t *hi;
	void *val;
	switch_codec_interface_t *codec_interface;
	int i = 0, lock = 0;
	const switch_codec_implementation_t *imp;

	switch_mutex_lock(loadable_modules.mutex);
	for (hi = switch_hash_first(NULL, loadable_modules.codec_hash); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, NULL, NULL, &val);
		codec_interface = (switch_codec_interface_t *) val;
		/* Look for a 20ms implementation because it's the safest choice */
		for (imp = codec_interface->implementations; imp; imp = imp->next) {
			if (lock && imp->microseconds_per_packet != lock) {
				continue;
			}

			if (imp->microseconds_per_packet / 1000 == 20) {
				array[i++] = imp;
				goto found;
			}
		}
		/* oh well we will use what we have */
		array[i++] = codec_interface->implementations;

	  found:

		if (!lock && i > 0)
			lock = array[i - 1]->microseconds_per_packet;

		if (i > arraylen) {
			break;
		}
	}

	switch_mutex_unlock(loadable_modules.mutex);

	return i;

}

SWITCH_DECLARE(int) switch_loadable_module_get_codecs_sorted(const switch_codec_implementation_t **array, int arraylen, char **prefs, int preflen)
{
	int x, i = 0, lock = 0;
	switch_codec_interface_t *codec_interface;
	const switch_codec_implementation_t *imp;

	switch_mutex_lock(loadable_modules.mutex);

	for (x = 0; x < preflen; x++) {
		char *cur, *last = NULL, *next = NULL, *name, *p, buf[256];
		uint32_t interval = 0, rate = 0;

		switch_copy_string(buf, prefs[x], sizeof(buf));
		last = name = next = cur = buf;

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
					interval = atoi(cur);
				} else if ((strchr(cur, 'k') || strchr(cur, 'h'))) {
					rate = atoi(cur);
				}
			}
			cur = next;
		}

		if ((codec_interface = switch_loadable_module_get_codec_interface(name)) != 0) {
			/* If no specific codec interval is requested opt for 20ms above all else because lots of stuff assumes it */
			for (imp = codec_interface->implementations; imp; imp = imp->next) {

				if (imp->codec_type != SWITCH_CODEC_TYPE_VIDEO) {
					if (lock && imp->microseconds_per_packet != lock) {
						continue;
					}

					if ((!interval && (uint32_t) (imp->microseconds_per_packet / 1000) != 20) ||
						(interval && (uint32_t) (imp->microseconds_per_packet / 1000) != interval)) {
						continue;
					}

					if (((!rate && (uint32_t) imp->samples_per_second != 8000) || (rate && (uint32_t) imp->samples_per_second != rate))) {
						continue;
					}
				}


				array[i++] = imp;
				goto found;

			}

			/* Either looking for a specific interval or there was no interval specified and there wasn't one @20ms available */
			for (imp = codec_interface->implementations; imp; imp = imp->next) {
				if (imp->codec_type != SWITCH_CODEC_TYPE_VIDEO) {

					if (lock && imp->microseconds_per_packet != lock) {
						continue;
					}

					if (interval && (uint32_t) (imp->microseconds_per_packet / 1000) != interval) {
						continue;
					}

					if (rate && (uint32_t) imp->samples_per_second != rate) {
						continue;
					}
				}

				array[i++] = imp;
				goto found;

			}

		  found:

			if (!lock && i > 0) {
				lock = array[i - 1]->microseconds_per_packet;
			}

			UNPROTECT_INTERFACE(codec_interface);

			if (i > arraylen) {
				break;
			}

		}
	}

	switch_mutex_unlock(loadable_modules.mutex);


	return i;
}

SWITCH_DECLARE(switch_status_t) switch_api_execute(const char *cmd, const char *arg, switch_core_session_t *session, switch_stream_handle_t *stream)
{
	switch_api_interface_t *api;
	switch_status_t status;

	switch_assert(stream != NULL);
	switch_assert(stream->data != NULL);
	switch_assert(stream->write_function != NULL);

	if (!stream->param_event) {
		switch_event_create(&stream->param_event, SWITCH_EVENT_API);
	}

	if (stream->param_event) {
		if (cmd) {
			switch_event_add_header_string(stream->param_event, SWITCH_STACK_BOTTOM, "API-Command", cmd);
		}
		if (arg) {
			switch_event_add_header_string(stream->param_event, SWITCH_STACK_BOTTOM, "API-Command-Argument", arg);
		}
	}


	if (cmd && (api = switch_loadable_module_get_api_interface(cmd)) != 0) {
		if ((status = api->function(arg, session, stream)) != SWITCH_STATUS_SUCCESS) {
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

	case SWITCH_API_INTERFACE:
		ALLOC_INTERFACE(api)

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

	default:
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid Module Type!\n");
		return NULL;
	}
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

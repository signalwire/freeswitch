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
 * Bret McDanel <trixter AT 0xdecafbad.com>
 * John Wehle (john@feith.com)
 * Raymond Chandler <intralanman@gmail.com>
 * Kristin King <kristin.king@quentustech.com>
 * Emmanuel Schmidbauer <e.schmidbauer@gmail.com>
 *
 * mod_voicemail.c -- Voicemail Module
 *
 */
#include <switch.h>

#ifdef _MSC_VER					/* compilers are stupid sometimes */
#define TRY_CODE(code) for(;;) {status = code; if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_BREAK) { goto end; } break;}
#else
#define TRY_CODE(code) do { status = code; if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_BREAK) { goto end; } break;} while(status)
#endif


#define xml_safe_free(_x) if (_x) switch_xml_free(_x); _x = NULL

SWITCH_MODULE_LOAD_FUNCTION(mod_voicemail_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_voicemail_shutdown);
SWITCH_MODULE_DEFINITION(mod_voicemail, mod_voicemail_load, mod_voicemail_shutdown, NULL);
#define VM_EVENT_MAINT "vm::maintenance"

#define VM_MAX_GREETINGS 9
#define VM_EVENT_QUEUE_SIZE 50000

static switch_status_t voicemail_inject(const char *data, switch_core_session_t *session);

static const char *global_cf = "voicemail.conf";
static struct {
	switch_hash_t *profile_hash;
	int debug;
	int message_query_exact_match;
	int32_t threads;
	int32_t running;
	switch_queue_t *event_queue;
	switch_mutex_t *mutex;
	switch_memory_pool_t *pool;
} globals;

typedef enum {
	VM_DATE_FIRST,
	VM_DATE_LAST,
	VM_DATE_NEVER
} date_location_t;

typedef enum {
	PFLAG_DESTROY = 1 << 0
} vm_flags_t;

typedef enum {
	VM_MOVE_NEXT,
	VM_MOVE_PREV,
	VM_MOVE_SAME
} msg_move_t;

typedef enum {
	MWI_REASON_UNKNOWN = 0,
	MWI_REASON_NEW = 1,
	MWI_REASON_DELETE = 2,
	MWI_REASON_SAVE = 3,
	MWI_REASON_PURGE = 4,
	MWI_REASON_READ = 5
} mwi_reason_t;

struct mwi_reason_table {
	const char *name;
	int state;
};

static struct mwi_reason_table MWI_REASON_CHART[] = {
	{"UNKNOWN", MWI_REASON_UNKNOWN},
	{"NEW", MWI_REASON_NEW},
	{"DELETE", MWI_REASON_DELETE},
	{"SAVE", MWI_REASON_SAVE},
	{"PURGE", MWI_REASON_PURGE},
	{"READ", MWI_REASON_READ},
	{NULL, 0}
};

#define VM_PROFILE_CONFIGITEM_COUNT 100

struct vm_profile {
	char *name;
	char *dbname;
	char *odbc_dsn;
	char *play_new_messages_lifo;
	char *play_saved_messages_lifo;
	char terminator_key[2];
	char play_new_messages_key[2];
	char play_saved_messages_key[2];

	char login_keys[16];
	char main_menu_key[2];
	char skip_greet_key[2];
	char skip_info_key[2];
	char config_menu_key[2];
	char record_greeting_key[2];
	char choose_greeting_key[2];
	char record_name_key[2];
	char change_pass_key[2];

	char record_file_key[2];
	char listen_file_key[2];
	char save_file_key[2];
	char delete_file_key[2];
	char undelete_file_key[2];
	char email_key[2];
	char callback_key[2];
	char pause_key[2];
	char restart_key[2];
	char ff_key[2];
	char rew_key[2];
	char prev_msg_key[2];
	char next_msg_key[2];
	char repeat_msg_key[2];
	char urgent_key[2];
	char operator_key[2];
	char vmain_key[2];
	char forward_key[2];
	char prepend_key[2];
	char file_ext[10];
	char *record_title;
	char *record_comment;
	char *record_copyright;
	char *operator_ext;
	char *vmain_ext;
	char *tone_spec;
	char *storage_dir;
	switch_bool_t storage_dir_shared;
	char *callback_dialplan;
	char *callback_context;
	char *email_body;
	char *email_headers;
	char *notify_email_body;
	char *notify_email_headers;
	char *web_head;
	char *web_tail;
	char *email_from;
	char *date_fmt;
	char *convert_cmd;
	char *convert_ext;
	date_location_t play_date_announcement;
	uint32_t digit_timeout;
	uint32_t max_login_attempts;
	uint32_t min_record_len;
	uint32_t max_record_len;
	uint32_t max_retries;
	switch_mutex_t *mutex;
	uint32_t record_threshold;
	uint32_t record_silence_hits;
	uint32_t record_sample_rate;
	switch_bool_t auto_playback_recordings;
	switch_bool_t db_password_override;
	switch_bool_t allow_empty_password_auth;
	switch_thread_rwlock_t *rwlock;
	switch_memory_pool_t *pool;
	uint32_t flags;

	switch_xml_config_item_t config[VM_PROFILE_CONFIGITEM_COUNT];
	switch_xml_config_string_options_t config_str_pool;
};
typedef struct vm_profile vm_profile_t;

const char * mwi_reason2str(mwi_reason_t state)
{
	uint8_t x;
	const char *str = "UNKNOWN";

	for (x = 0; x < (sizeof(MWI_REASON_CHART) / sizeof(struct mwi_reason_table)) - 1; x++) {
		if (MWI_REASON_CHART[x].state == state) {
			str = MWI_REASON_CHART[x].name;
			break;
		}
	}

	return str;
}

switch_cache_db_handle_t *vm_get_db_handle(vm_profile_t *profile)
{

	switch_cache_db_handle_t *dbh = NULL;
	char *dsn;
	
	if (!zstr(profile->odbc_dsn)) {
		dsn = profile->odbc_dsn;
	} else {
		dsn = profile->dbname;
	}

	if (switch_cache_db_get_db_handle_dsn(&dbh, dsn) != SWITCH_STATUS_SUCCESS) {
		dbh = NULL;
	}
	
	return dbh;
}


static switch_status_t vm_execute_sql(vm_profile_t *profile, char *sql, switch_mutex_t *mutex)
{
	switch_cache_db_handle_t *dbh = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (mutex) {
		switch_mutex_lock(mutex);
	}

	if (!(dbh = vm_get_db_handle(profile))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB\n");
		goto end;
	}

	status = switch_cache_db_execute_sql(dbh, sql, NULL);

  end:

	switch_cache_db_release_db_handle(&dbh);

	if (mutex) {
		switch_mutex_unlock(mutex);
	}

	return status;
}

char *vm_execute_sql2str(vm_profile_t *profile, switch_mutex_t *mutex, char *sql, char *resbuf, size_t len)
{
	switch_cache_db_handle_t *dbh = NULL;

	char *ret = NULL;

	if (mutex) {
		switch_mutex_lock(mutex);
	}

	if (!(dbh = vm_get_db_handle(profile))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB\n");
		goto end;
	}

	ret = switch_cache_db_execute_sql2str(dbh, sql, resbuf, len, NULL);

end:

	switch_cache_db_release_db_handle(&dbh);

	if (mutex) {
		switch_mutex_unlock(mutex);
	}

	return ret;

}


static switch_bool_t vm_execute_sql_callback(vm_profile_t *profile, switch_mutex_t *mutex, char *sql, switch_core_db_callback_func_t callback,
											 void *pdata)
{
	switch_bool_t ret = SWITCH_FALSE;
	char *errmsg = NULL;
	switch_cache_db_handle_t *dbh = NULL;

	if (mutex) {
		switch_mutex_lock(mutex);
	}

	if (!(dbh = vm_get_db_handle(profile))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB\n");
		goto end;
	}

	switch_cache_db_execute_sql_callback(dbh, sql, callback, pdata, &errmsg);

	if (errmsg) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL ERR: [%s] %s\n", sql, errmsg);
		free(errmsg);
	}

  end:

	switch_cache_db_release_db_handle(&dbh);

	if (mutex) {
		switch_mutex_unlock(mutex);
	}

	return ret;
}



static char vm_sql[] =
	"CREATE TABLE voicemail_msgs (\n"
	"   created_epoch INTEGER,\n"
	"   read_epoch    INTEGER,\n"
	"   username      VARCHAR(255),\n"
	"   domain        VARCHAR(255),\n"
	"   uuid          VARCHAR(255),\n"
	"   cid_name      VARCHAR(255),\n"
	"   cid_number    VARCHAR(255),\n"
	"   in_folder     VARCHAR(255),\n"
	"   file_path     VARCHAR(255),\n"
	"   message_len   INTEGER,\n" "   flags         VARCHAR(255),\n" "   read_flags    VARCHAR(255),\n" "   forwarded_by  VARCHAR(255)\n" ");\n";

static char vm_pref_sql[] =
	"CREATE TABLE voicemail_prefs (\n"
	"   username        VARCHAR(255),\n"
	"   domain          VARCHAR(255),\n"
	"   name_path       VARCHAR(255),\n" "   greeting_path   VARCHAR(255),\n" "   password        VARCHAR(255)\n" ");\n";

static char *vm_index_list[] = {
	"create index voicemail_msgs_idx1 on voicemail_msgs(created_epoch)",
	"create index voicemail_msgs_idx2 on voicemail_msgs(username)",
	"create index voicemail_msgs_idx3 on voicemail_msgs(domain)",
	"create index voicemail_msgs_idx4 on voicemail_msgs(uuid)",
	"create index voicemail_msgs_idx5 on voicemail_msgs(in_folder)",
	"create index voicemail_msgs_idx6 on voicemail_msgs(read_flags)",
	"create index voicemail_msgs_idx7 on voicemail_msgs(forwarded_by)",
	"create index voicemail_msgs_idx8 on voicemail_msgs(read_epoch)",
	"create index voicemail_msgs_idx9 on voicemail_msgs(flags)",
	"create index voicemail_prefs_idx1 on voicemail_prefs(username)",
	"create index voicemail_prefs_idx2 on voicemail_prefs(domain)",
	NULL
};

static void free_profile(vm_profile_t *profile)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Destroying Profile %s\n", profile->name);
	switch_core_destroy_memory_pool(&profile->pool);
}

static void destroy_profile(const char *profile_name, switch_bool_t block)
{
	vm_profile_t *profile = NULL;
	switch_mutex_lock(globals.mutex);
	if ((profile = switch_core_hash_find(globals.profile_hash, profile_name))) {
		switch_core_hash_delete(globals.profile_hash, profile_name);
	}
	switch_mutex_unlock(globals.mutex);

	if (!profile) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[%s] Invalid Profile\n", profile_name);
		return;
	}

	if (block) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[%s] Waiting for write lock\n", profile->name);
		switch_thread_rwlock_wrlock(profile->rwlock);
	} else {
		if (switch_thread_rwlock_trywrlock(profile->rwlock) != SWITCH_STATUS_SUCCESS) {
			/* Lock failed, set the destroy flag so it'll be destroyed whenever its not in use anymore */
			switch_set_flag(profile, PFLAG_DESTROY);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[%s] Profile is in use, memory will be freed whenever its no longer in use\n",
							  profile->name);
			return;
		}
	}

	free_profile(profile);
}


/* Static buffer, 2 bytes */
static switch_xml_config_string_options_t config_dtmf = { NULL, 2, "[0-9#\\*]" };
static switch_xml_config_string_options_t config_dtmf_optional = { NULL, 2, "[0-9#\\*]?" };
static switch_xml_config_string_options_t config_login_keys = { NULL, 16, "[0-9#\\*]*" };
static switch_xml_config_string_options_t config_file_ext = { NULL, 10, NULL };
static switch_xml_config_int_options_t config_int_0_10000 = { SWITCH_TRUE, 0, SWITCH_TRUE, 10000 };
static switch_xml_config_int_options_t config_int_0_1000 = { SWITCH_TRUE, 0, SWITCH_TRUE, 1000 };
static switch_xml_config_int_options_t config_int_digit_timeout = { SWITCH_TRUE, 0, SWITCH_TRUE, 30000 };
static switch_xml_config_int_options_t config_int_max_logins = { SWITCH_TRUE, 0, SWITCH_TRUE, 10 };
static switch_xml_config_int_options_t config_int_ht_0 = { SWITCH_TRUE, 0 };

static switch_xml_config_enum_item_t config_play_date_announcement[] = {
	{"first", VM_DATE_FIRST},
	{"last", VM_DATE_LAST},
	{"never", VM_DATE_NEVER},
	{NULL, 0}
};


static switch_status_t vm_config_email_callback(switch_xml_config_item_t *item, const char *newvalue, switch_config_callback_type_t callback_type,
												switch_bool_t changed)
{
	vm_profile_t *profile = (vm_profile_t *) item->data;

	switch_assert(profile);

	if (callback_type == CONFIG_LOAD || callback_type == CONFIG_RELOAD) {
		char *email_headers = NULL, *email_body = NULL;
		if (newvalue) {
			switch_stream_handle_t stream;
			SWITCH_STANDARD_STREAM(stream);
			if (switch_stream_write_file_contents(&stream, newvalue) == SWITCH_STATUS_SUCCESS) {
				email_headers = switch_core_strdup(profile->pool, stream.data);
				if ((email_body = strstr(email_headers, "\n\n"))) {
					*email_body = '\0';
					email_body += 2;
				} else if ((email_body = strstr(email_headers, "\r\n\r\n"))) {
					*email_body = '\0';
					email_body += 4;
				}
			}

			free(stream.data);
		}

		if (email_headers) {
			profile->email_headers = email_headers;
		}
		if (email_body) {
			profile->email_body = email_body;
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t vm_config_notify_callback(switch_xml_config_item_t *item, const char *newvalue, switch_config_callback_type_t callback_type,
												 switch_bool_t changed)
{
	vm_profile_t *profile = (vm_profile_t *) item->data;

	switch_assert(profile);

	if (callback_type == CONFIG_LOAD || callback_type == CONFIG_RELOAD) {
		char *email_headers = NULL, *email_body = NULL;
		if (newvalue) {
			switch_stream_handle_t stream;
			SWITCH_STANDARD_STREAM(stream);
			if (switch_stream_write_file_contents(&stream, newvalue) == SWITCH_STATUS_SUCCESS) {
				email_headers = switch_core_strdup(profile->pool, stream.data);
				if ((email_body = strstr(email_headers, "\n\n"))) {
					*email_body = '\0';
					email_body += 2;
				} else if ((email_body = strstr(email_headers, "\r\n\r\n"))) {
					*email_body = '\0';
					email_body += 4;
				}
			}

			free(stream.data);
		}

		if (email_headers) {
			profile->notify_email_headers = email_headers;
		}
		if (email_body) {
			profile->notify_email_body = email_body;
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t vm_config_web_callback(switch_xml_config_item_t *item, const char *newvalue, switch_config_callback_type_t callback_type,
											  switch_bool_t changed)
{
	vm_profile_t *profile = (vm_profile_t *) item->data;

	switch_assert(profile);

	if (callback_type == CONFIG_LOAD || callback_type == CONFIG_RELOAD) {
		char *web_head = NULL, *web_tail = NULL;
		if (newvalue) {
			switch_stream_handle_t stream;
			SWITCH_STANDARD_STREAM(stream);
			if (switch_stream_write_file_contents(&stream, newvalue) == SWITCH_STATUS_SUCCESS) {
				web_head = switch_core_strdup(profile->pool, stream.data);

				if ((web_tail = strstr(web_head, "<!break>\n"))) {
					*web_tail = '\0';
					web_tail += 9;
				} else if ((web_tail = strstr(web_head, "<!break>\r\n"))) {
					*web_tail = '\0';
					web_tail += 10;
				}
			}

			free(stream.data);
		}

		if (web_head) {
			profile->web_head = web_head;
		}

		if (web_tail) {
			profile->web_tail = web_tail;
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t vm_config_validate_samplerate(switch_xml_config_item_t *item, const char *newvalue, switch_config_callback_type_t callback_type,
													 switch_bool_t changed)
{
	if ((callback_type == CONFIG_LOAD || callback_type == CONFIG_RELOAD) && newvalue) {
		int val = *(int *) item->ptr;
		if (val != 0 && !switch_is_valid_rate(val)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Invalid samplerate %s\n", newvalue);
			return SWITCH_STATUS_FALSE;
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

/*!
 * \brief Sets the profile's configuration instructions 
 */
vm_profile_t *profile_set_config(vm_profile_t *profile)
{
	int i = 0;

	profile->config_str_pool.pool = profile->pool;

	/*
	   SWITCH _CONFIG_SET_ITEM(item, "key", type, flags, 
	   pointer, default, options, help_syntax, help_description)
	 */

	/* DTMFs */
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "terminator-key", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->terminator_key, "#", &config_dtmf, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "play-new-messages-key", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->play_new_messages_key, "1", &config_dtmf, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "play-saved-messages-key", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->play_saved_messages_key, "2", &config_dtmf, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "play-new-messages-lifo", SWITCH_CONFIG_BOOL, CONFIG_RELOADABLE,
						   &profile->play_new_messages_lifo, SWITCH_FALSE, NULL, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "play-saved-messages-lifo", SWITCH_CONFIG_BOOL, CONFIG_RELOADABLE,
						   &profile->play_saved_messages_lifo, SWITCH_FALSE, NULL, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "login-keys", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->login_keys, "0", &config_login_keys, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "main-menu-key", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->main_menu_key, "0", &config_dtmf, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "skip-greet-key", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->skip_greet_key, "#", &config_dtmf, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "skip-info-key", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->skip_info_key, "*", &config_dtmf, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "config-menu-key", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->config_menu_key, "5", &config_dtmf, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "record-greeting-key", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->record_greeting_key, "1", &config_dtmf, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "choose-greeting-key", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->choose_greeting_key, "2", &config_dtmf, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "record-name-key", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->record_name_key, "3", &config_dtmf, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "change-pass-key", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->change_pass_key, "6", &config_dtmf, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "record-file-key", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->record_file_key, "3", &config_dtmf, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "listen-file-key", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->listen_file_key, "1", &config_dtmf, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "save-file-key", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->save_file_key, "2", &config_dtmf, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "delete-file-key", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->delete_file_key, "7", &config_dtmf, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "undelete-file-key", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->undelete_file_key, "8", &config_dtmf, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "email-key", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE, &profile->email_key, "4", &config_dtmf, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "callback-key", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->callback_key, "5", &config_dtmf, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "pause-key", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE, &profile->pause_key, "0", &config_dtmf, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "restart-key", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->restart_key, "1", &config_dtmf, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "ff-key", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE, &profile->ff_key, "6", &config_dtmf, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "rew-key", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE, &profile->rew_key, "4", &config_dtmf, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "previous-message-key", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->prev_msg_key, "1", &config_dtmf, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "next-message-key", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->next_msg_key, "3", &config_dtmf, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "repeat-message-key", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->repeat_msg_key, "0", &config_dtmf, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "urgent-key", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->urgent_key, "*", &config_dtmf, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "operator-key", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->operator_key, "", &config_dtmf_optional, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "vmain-key", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE, &profile->vmain_key, "", &config_dtmf_optional, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "vmain-extension", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->vmain_ext, "", &profile->config_str_pool, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "forward-key", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->forward_key, "8", &config_dtmf, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "prepend-key", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->prepend_key, "1", &config_dtmf, NULL, NULL);

	/* Other settings */
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "file-extension", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->file_ext, "wav", &config_file_ext, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "record-title", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->record_title, "FreeSWITCH Voicemail", &profile->config_str_pool, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "record-comment", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->record_comment, "FreeSWITCH Voicemail", &profile->config_str_pool, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "record-copyright", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->record_copyright, "http://www.freeswitch.org", &profile->config_str_pool, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "operator-extension", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->operator_ext, "", &profile->config_str_pool, NULL, NULL);

	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "tone-spec", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->tone_spec, "%(1000, 0, 640)", &profile->config_str_pool, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "storage-dir", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->storage_dir, "", &profile->config_str_pool, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "storage-dir-shared", SWITCH_CONFIG_BOOL, CONFIG_RELOADABLE,
						   &profile->storage_dir_shared, SWITCH_FALSE, NULL, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "callback-dialplan", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->callback_dialplan, "XML", &profile->config_str_pool, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "callback-context", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->callback_context, "default", &profile->config_str_pool, NULL, NULL);

	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "notify-email-body", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->notify_email_body, NULL, &profile->config_str_pool, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "notify-email-headers", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->notify_email_headers, NULL, &profile->config_str_pool, NULL, NULL);

	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "play-date-announcement", SWITCH_CONFIG_ENUM, CONFIG_RELOADABLE,
						   &profile->play_date_announcement, VM_DATE_FIRST, &config_play_date_announcement, NULL, NULL);

	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "convert-cmd", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->convert_cmd, NULL, &profile->config_str_pool, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "convert-ext", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->convert_ext, NULL, &profile->config_str_pool, NULL, NULL);


	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "digit-timeout", SWITCH_CONFIG_INT, CONFIG_RELOADABLE,
						   &profile->digit_timeout, 10000, &config_int_digit_timeout, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "max-login-attempts", SWITCH_CONFIG_INT, CONFIG_RELOADABLE,
						   &profile->max_login_attempts, 3, &config_int_max_logins, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "min-record-len", SWITCH_CONFIG_INT, CONFIG_RELOADABLE,
						   &profile->min_record_len, 3, &config_int_0_10000, "seconds", NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "max-record-len", SWITCH_CONFIG_INT, CONFIG_RELOADABLE,
						   &profile->max_record_len, 300, &config_int_0_1000, "seconds", NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "max-retries", SWITCH_CONFIG_INT, CONFIG_RELOADABLE,
						   &profile->max_retries, 3, &config_int_ht_0, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "record-silence-threshold", SWITCH_CONFIG_INT, CONFIG_RELOADABLE,
						   &profile->record_threshold, 200, &config_int_0_10000, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "record-silence-hits", SWITCH_CONFIG_INT, CONFIG_RELOADABLE,
						   &profile->record_silence_hits, 2, &config_int_0_1000, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM_CALLBACK(profile->config[i++], "record-sample-rate", SWITCH_CONFIG_INT, CONFIG_RELOADABLE,
									&profile->record_sample_rate, 0, NULL, vm_config_validate_samplerate, NULL, NULL);

	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "email_headers", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->email_headers, NULL, &profile->config_str_pool, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "email_body", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->email_body, NULL, &profile->config_str_pool, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "email_email-from", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->email_from, NULL, &profile->config_str_pool, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "email_date-fmt", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->date_fmt, "%A, %B %d %Y, %I:%M %p", &profile->config_str_pool, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "odbc-dsn", SWITCH_CONFIG_STRING, 0, &profile->odbc_dsn, NULL, &profile->config_str_pool, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "dbname", SWITCH_CONFIG_STRING, 0, &profile->dbname, NULL, &profile->config_str_pool, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM_CALLBACK(profile->config[i++], "email_template-file", SWITCH_CONFIG_CUSTOM, CONFIG_RELOADABLE,
									NULL, NULL, profile, vm_config_email_callback, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM_CALLBACK(profile->config[i++], "email_notify-template-file", SWITCH_CONFIG_CUSTOM, CONFIG_RELOADABLE,
									NULL, NULL, profile, vm_config_notify_callback, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM_CALLBACK(profile->config[i++], "web-template-file", SWITCH_CONFIG_CUSTOM, CONFIG_RELOADABLE,
									NULL, NULL, profile, vm_config_web_callback, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "db-password-override", SWITCH_CONFIG_BOOL, CONFIG_RELOADABLE,
						   &profile->db_password_override, SWITCH_FALSE, NULL, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "allow-empty-password-auth", SWITCH_CONFIG_BOOL, CONFIG_RELOADABLE,
						   &profile->allow_empty_password_auth, SWITCH_TRUE, NULL, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "auto-playback-recordings", SWITCH_CONFIG_BOOL, CONFIG_RELOADABLE, &profile->auto_playback_recordings, SWITCH_FALSE, NULL, NULL, NULL); 

	switch_assert(i < VM_PROFILE_CONFIGITEM_COUNT);

	return profile;

}

static vm_profile_t *load_profile(const char *profile_name)
{
	vm_profile_t *profile = NULL;
	switch_xml_t x_profiles, x_profile, cfg, xml, x_email, param;
	switch_event_t *event = NULL;
	switch_cache_db_handle_t *dbh = NULL;

	if (!(xml = switch_xml_open_cfg(global_cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", global_cf);
		return profile;
	}
	if (!(x_profiles = switch_xml_child(cfg, "profiles"))) {
		goto end;
	}

	if ((x_profile = switch_xml_find_child(x_profiles, "profile", "name", profile_name))) {
		switch_memory_pool_t *pool;
		int x;
		switch_size_t count;
		char *errmsg;

		if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Pool Failure\n");
			goto end;
		}

		if (!(profile = switch_core_alloc(pool, sizeof(vm_profile_t)))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Alloc Failure\n");
			switch_core_destroy_memory_pool(&pool);
			goto end;
		}

		profile->pool = pool;
		profile_set_config(profile);

		/* Add the params to the event structure */
		count = switch_event_import_xml(switch_xml_child(x_profile, "param"), "name", "value", &event);

		/* Take care of the custom config structure */
		if ((x_email = switch_xml_child(x_profile, "email"))) {
			if ((param = switch_xml_child(x_email, "body"))) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "email_body", param->txt);
			}
			if ((param = switch_xml_child(x_email, "headers"))) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "email_headers", param->txt);
			}

			for (param = switch_xml_child(x_email, "param"); param; param = param->next) {
				char *var, *val;
				char buf[2048];

				if ((var = (char *) switch_xml_attr_soft(param, "name")) && (val = (char *) switch_xml_attr_soft(param, "value"))) {
					switch_snprintf(buf, 2048, "email_%s", var);
					switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, buf, val);
				}
			}
		}


		if (switch_xml_config_parse_event(event, (int)count, SWITCH_FALSE, profile->config) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to process configuration\n");
			switch_core_destroy_memory_pool(&pool);
			goto end;
		}

		switch_thread_rwlock_create(&profile->rwlock, pool);
		profile->name = switch_core_strdup(pool, profile_name);

		if (zstr(profile->dbname)) {
			profile->dbname = switch_core_sprintf(profile->pool, "voicemail_%s", profile_name);
		}

		if (!(dbh = vm_get_db_handle(profile))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Cannot open DB!\n");
			goto end;
		}

		switch_cache_db_test_reactive(dbh, "select count(forwarded_by) from voicemail_msgs", NULL,
									  "alter table voicemail_msgs add forwarded_by varchar(255)");
		switch_cache_db_test_reactive(dbh, "select count(forwarded_by) from voicemail_msgs", "drop table voicemail_msgs", vm_sql);

		switch_cache_db_test_reactive(dbh, "select count(username) from voicemail_prefs", "drop table voicemail_prefs", vm_pref_sql);
		switch_cache_db_test_reactive(dbh, "select count(password) from voicemail_prefs", NULL, "alter table voicemail_prefs add password varchar(255)");

		for (x = 0; vm_index_list[x]; x++) {
			errmsg = NULL;
			switch_cache_db_execute_sql(dbh, vm_index_list[x], &errmsg);
			switch_safe_free(errmsg);
		}

		switch_cache_db_release_db_handle(&dbh);

		switch_mutex_init(&profile->mutex, SWITCH_MUTEX_NESTED, profile->pool);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Added Profile %s\n", profile->name);
		switch_core_hash_insert(globals.profile_hash, profile->name, profile);
	}

  end:

	switch_cache_db_release_db_handle(&dbh);

	if (xml) {
		switch_xml_free(xml);
	}
	if (event) {
		switch_event_destroy(&event);
	}
	return profile;
}


static vm_profile_t *get_profile(const char *profile_name)
{
	vm_profile_t *profile = NULL;

	switch_mutex_lock(globals.mutex);
	if (!(profile = switch_core_hash_find(globals.profile_hash, profile_name))) {
		profile = load_profile(profile_name);
	}
	if (profile) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG10, "[%s] rwlock\n", profile->name);

		switch_thread_rwlock_rdlock(profile->rwlock);
	}
	switch_mutex_unlock(globals.mutex);

	return profile;
}

static void profile_rwunlock(vm_profile_t *profile)
{
	switch_thread_rwlock_unlock(profile->rwlock);
	if (switch_test_flag(profile, PFLAG_DESTROY)) {
		if (switch_thread_rwlock_trywrlock(profile->rwlock) == SWITCH_STATUS_SUCCESS) {
			switch_thread_rwlock_unlock(profile->rwlock);
			free_profile(profile);
		}
	}
}


static switch_status_t load_config(void)
{
	switch_xml_t cfg, xml, settings, param, x_profiles, x_profile;

	if (!(xml = switch_xml_open_cfg(global_cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", global_cf);
		return SWITCH_STATUS_TERM;
	}

	switch_mutex_lock(globals.mutex);
	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcasecmp(var, "debug")) {
				globals.debug = atoi(val);
			} else if (!strcasecmp(var, "message-query-exact-match")) {
				globals.message_query_exact_match = switch_true(val);
			}
		}
	}

	if ((x_profiles = switch_xml_child(cfg, "profiles"))) {
		for (x_profile = switch_xml_child(x_profiles, "profile"); x_profile; x_profile = x_profile->next) {
			load_profile(switch_xml_attr_soft(x_profile, "name"));
		}
	}
	switch_mutex_unlock(globals.mutex);

	switch_xml_free(xml);
	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t cancel_on_dtmf(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen)
{
	switch (itype) {
	case SWITCH_INPUT_TYPE_DTMF:
		{
			switch_dtmf_t *dtmf = (switch_dtmf_t *) input;
			if (buf && buflen) {
				char *bp = (char *) buf;
				bp[0] = dtmf->digit;
				bp[1] = '\0';
			}
			return SWITCH_STATUS_BREAK;
		}
		break;
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}


struct call_control {
	vm_profile_t *profile;
	switch_file_handle_t *fh;
	char buf[4];
	int noexit;
	int playback_controls_active;
};
typedef struct call_control cc_t;

static switch_status_t control_playback(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen)
{
	switch (itype) {
	case SWITCH_INPUT_TYPE_DTMF:
		{
			switch_dtmf_t *dtmf = (switch_dtmf_t *) input;
			cc_t *cc = (cc_t *) buf;
			switch_file_handle_t *fh = cc->fh;
			uint32_t pos = 0;

			if (!cc->noexit
				&& (dtmf->digit == *cc->profile->delete_file_key || dtmf->digit == *cc->profile->save_file_key
					|| dtmf->digit == *cc->profile->prev_msg_key || dtmf->digit == *cc->profile->next_msg_key 
					|| dtmf->digit == *cc->profile->repeat_msg_key
					|| dtmf->digit == *cc->profile->terminator_key || dtmf->digit == *cc->profile->skip_info_key
					|| dtmf->digit == *cc->profile->forward_key)) {
				*cc->buf = dtmf->digit;
				return SWITCH_STATUS_BREAK;
			}

			if (!cc->playback_controls_active
				&& (dtmf->digit == *cc->profile->email_key)) {
				*cc->buf = dtmf->digit;
				return SWITCH_STATUS_BREAK;
			}

			if (!(fh && fh->file_interface && switch_test_flag(fh, SWITCH_FILE_OPEN))) {
				return SWITCH_STATUS_SUCCESS;
			}

			if (dtmf->digit == *cc->profile->pause_key) {
				if (switch_test_flag(fh, SWITCH_FILE_PAUSE)) {
					switch_clear_flag(fh, SWITCH_FILE_PAUSE);
				} else {
					switch_set_flag(fh, SWITCH_FILE_PAUSE);
				}
				return SWITCH_STATUS_SUCCESS;
			}

			if (dtmf->digit == *cc->profile->restart_key) {
				unsigned int seekpos = 0;
				fh->speed = 0;
				switch_core_file_seek(fh, &seekpos, 0, SEEK_SET);
				return SWITCH_STATUS_SUCCESS;
			}

			if (dtmf->digit == *cc->profile->ff_key) {
				int samps = 24000;
				switch_core_file_seek(fh, &pos, samps, SEEK_CUR);
				return SWITCH_STATUS_SUCCESS;
			}

			if (dtmf->digit == *cc->profile->rew_key) {
				int samps = -48000;
				int target_pos = fh->offset_pos + samps;
				if (target_pos < 1) {
					/* too close to beginning of the file, just restart instead of rewind */
					unsigned int seekpos = 0;
					fh->speed = 0;
					switch_core_file_seek(fh, &seekpos, 0, SEEK_SET);
					return SWITCH_STATUS_SUCCESS;
				} else {
					switch_core_file_seek(fh, &pos, samps, SEEK_CUR);
					return SWITCH_STATUS_SUCCESS;
				}
			}
		}
		break;
	default:
		break;
	}

	return SWITCH_STATUS_SUCCESS;
}

struct prefs_callback {
	char name_path[255];
	char greeting_path[255];
	char password[255];
};
typedef struct prefs_callback prefs_callback_t;

static int prefs_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	prefs_callback_t *cbt = (prefs_callback_t *) pArg;

	switch_copy_string(cbt->name_path, argv[2], sizeof(cbt->name_path));
	switch_copy_string(cbt->greeting_path, argv[3], sizeof(cbt->greeting_path));
	switch_copy_string(cbt->password, argv[4], sizeof(cbt->password));

	return 0;
}


typedef enum {
	VM_CHECK_START,
	VM_CHECK_AUTH,
	VM_CHECK_MENU,
	VM_CHECK_CONFIG,
	VM_CHECK_PLAY_MESSAGES,
	VM_CHECK_FOLDER_SUMMARY,
	VM_CHECK_LISTEN
} vm_check_state_t;


#define VM_INVALID_EXTENSION_MACRO "voicemail_invalid_extension"
#define VM_FORWARD_MESSAGE_ENTER_EXTENSION_MACRO "voicemail_forward_message_enter_extension"
#define VM_ACK_MACRO "voicemail_ack"
#define VM_SAY_DATE_MACRO "voicemail_say_date"
#define VM_PLAY_GREETING_MACRO "voicemail_play_greeting"
#define VM_SAY_MESSAGE_NUMBER_MACRO "voicemail_say_message_number"
#define VM_SAY_NUMBER_MACRO "voicemail_say_number"
#define VM_SAY_PHONE_NUMBER_MACRO "voicemail_say_phone_number"
#define VM_SAY_NAME_MACRO "voicemail_say_name"

#define VM_FORWARD_PREPEND_MACRO "voicemail_forward_prepend"
#define VM_RECORD_MESSAGE_MACRO "voicemail_record_message"
#define VM_CHOOSE_GREETING_MACRO "voicemail_choose_greeting"
#define VM_CHOOSE_GREETING_FAIL_MACRO "voicemail_choose_greeting_fail"
#define VM_CHOOSE_GREETING_SELECTED_MACRO "voicemail_greeting_selected"
#define VM_RECORD_GREETING_MACRO "voicemail_record_greeting"
#define VM_RECORD_NAME_MACRO "voicemail_record_name"
#define VM_LISTEN_FILE_CHECK_MACRO "voicemail_listen_file_check"
#define VM_RECORD_FILE_CHECK_MACRO "voicemail_record_file_check"
#define VM_RECORD_URGENT_CHECK_MACRO "voicemail_record_urgent_check"
#define VM_MENU_MACRO "voicemail_menu"
#define VM_CONFIG_MENU_MACRO "voicemail_config_menu"
#define VM_ENTER_ID_MACRO "voicemail_enter_id"
#define VM_ENTER_PASS_MACRO "voicemail_enter_pass"
#define VM_FAIL_AUTH_MACRO "voicemail_fail_auth"
#define VM_CHANGE_PASS_SUCCESS_MACRO "voicemail_change_pass_success"
#define VM_CHANGE_PASS_FAIL_MACRO "voicemail_change_pass_fail"
#define VM_ABORT_MACRO "voicemail_abort"
#define VM_HELLO_MACRO "voicemail_hello"
#define VM_GOODBYE_MACRO "voicemail_goodbye"
#define VM_MESSAGE_COUNT_MACRO "voicemail_message_count"
#define VM_DISK_QUOTA_EXCEEDED_MACRO "voicemail_disk_quota_exceeded"
#define URGENT_FLAG_STRING "A_URGENT"
#define NORMAL_FLAG_STRING "B_NORMAL"

static switch_status_t vm_macro_get(switch_core_session_t *session,
									char *macro,
									char *macro_arg,
									char *buf, switch_size_t buflen, switch_size_t maxlen, char *term_chars, char *terminator_key, uint32_t timeout)
{
	switch_input_args_t args = { 0 }, *ap = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_size_t bslen;

	if (buf && buflen) {
		memset(buf, 0, buflen);
		args.input_callback = cancel_on_dtmf;
		args.buf = buf;
		args.buflen = (uint32_t) buflen;
		ap = &args;
	}

	status = switch_ivr_phrase_macro(session, macro, macro_arg, NULL, ap);

	if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_BREAK) {
		if (buf) {
			memset(buf, 0, buflen);
		}
		return status;
	}

	if (!buf) {
		return status;
	}

	bslen = strlen(buf);

	if (maxlen == 0 || maxlen > buflen - 1) {
		maxlen = buflen - 1;
	}

	if (bslen < maxlen) {
		status = switch_ivr_collect_digits_count(session, buf + bslen, buflen, maxlen - bslen, term_chars, terminator_key, timeout, 0, 0);
		if (status == SWITCH_STATUS_TIMEOUT) {
			status = SWITCH_STATUS_SUCCESS;
		}
	}

	return status;
}

struct callback {
	char *buf;
	size_t len;
	int matches;
};
typedef struct callback callback_t;

struct msg_cnt_callback {
	char *buf;
	size_t len;
	int matches;
	int total_new_messages;
	int total_new_urgent_messages;
	int total_saved_messages;
	int total_saved_urgent_messages;
};
typedef struct msg_cnt_callback msg_cnt_callback_t;


static int message_count_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	msg_cnt_callback_t *cbt = (msg_cnt_callback_t *) pArg;

	if (argc < 3 || zstr(argv[0]) || zstr(argv[1]) || zstr(argv[2])) {
		return -1;
	}

	if (atoi(argv[0]) == 1) {	/* UnRead */
		if (!strcasecmp(argv[1], "A_URGENT")) {	/* Urgent */
			cbt->total_new_urgent_messages = atoi(argv[2]);
		} else {				/* Normal */
			cbt->total_new_messages = atoi(argv[2]);
		}
	} else {					/* Already Read */
		if (!strcasecmp(argv[1], "A_URGENT")) {	/* Urgent */
			cbt->total_saved_urgent_messages = atoi(argv[2]);
		} else {				/* Normal */
			cbt->total_saved_messages = atoi(argv[2]);
		}
	}

	return 0;
}

static int sql2str_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	callback_t *cbt = (callback_t *) pArg;

	switch_copy_string(cbt->buf, argv[0], cbt->len);
	cbt->matches++;
	return 0;
}


static int unlink_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	if (argv[0]) {
		if (unlink(argv[0]) != 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Failed to delete file [%s]\n", argv[0]);
		}
	}
	return 0;
}


typedef enum {
	MSG_NONE,
	MSG_NEW,
	MSG_SAVED
} msg_type_t;


switch_status_t measure_file_len(const char *path, switch_size_t *message_len)
{

	switch_file_handle_t fh = { 0 };
	uint32_t pos = 0;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (switch_core_file_open(&fh, path, 0, 0, SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL) == SWITCH_STATUS_SUCCESS) {

		if (switch_core_file_seek(&fh, &pos, 0, SEEK_END) == SWITCH_STATUS_SUCCESS) {
			*message_len = pos / fh.samplerate;
			status = SWITCH_STATUS_SUCCESS;
		}
		switch_core_file_close(&fh);
	}

	return status;

}

static switch_status_t create_file(switch_core_session_t *session, vm_profile_t *profile,
								   char *macro_name, char *file_path, switch_size_t *message_len, switch_bool_t limit,
								   const char *exit_keys, char *key_pressed)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_file_handle_t fh = { 0 };
	switch_input_args_t args = { 0 };
	char term;
	char input[10] = "", key_buf[80] = "";
	cc_t cc = { 0 };
	switch_codec_implementation_t read_impl = { 0 };
	int got_file = 0;
	switch_bool_t skip_record_check = switch_true(switch_channel_get_variable(channel, "skip_record_check"));

	switch_core_session_get_read_impl(session, &read_impl);


	if (exit_keys) {
		*key_pressed = '\0';
	}

	while (switch_channel_ready(channel)) {
		uint32_t counter = 0;
		switch_snprintf(key_buf, sizeof(key_buf), "%s:%s:%s", profile->listen_file_key, profile->save_file_key, profile->record_file_key);

	  record_file:
		*message_len = 0;

		if (macro_name)
			TRY_CODE(switch_ivr_phrase_macro(session, macro_name, NULL, NULL, NULL));
		switch_channel_flush_dtmf(channel);
		TRY_CODE(switch_ivr_gentones(session, profile->tone_spec, 0, NULL));

		memset(&fh, 0, sizeof(fh));
		fh.thresh = profile->record_threshold;
		fh.silence_hits = profile->record_silence_hits;
		fh.samplerate = profile->record_sample_rate;

		memset(input, 0, sizeof(input));
		args.input_callback = cancel_on_dtmf;
		args.buf = input;
		args.buflen = sizeof(input);

		unlink(file_path);

		switch_ivr_record_file(session, &fh, file_path, &args, profile->max_record_len);

		if (switch_file_exists(file_path, switch_core_session_get_pool(session)) == SWITCH_STATUS_SUCCESS) {
			got_file = 1;
		}

		if (limit && (*message_len = fh.samples_out / (fh.samplerate ? fh.samplerate : 8000)) < profile->min_record_len) {
			if (unlink(file_path) != 0) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Failed to delete file [%s]\n", file_path);
			}
			got_file = 0;
			if (exit_keys && input[0] && strchr(exit_keys, input[0])) {
				*key_pressed = input[0];
				return SWITCH_STATUS_SUCCESS;
			}
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Message is less than minimum record length: %d, discarding it.\n",
							  profile->min_record_len);
			if (switch_channel_ready(channel) && counter < profile->max_retries) {
				TRY_CODE(switch_ivr_phrase_macro(session, VM_ACK_MACRO, "too-small", NULL, NULL));
				counter++;
				goto record_file;
			} else {
				status = SWITCH_STATUS_NOTFOUND;
				goto end;
			}
		} else {
			status = SWITCH_STATUS_SUCCESS;
		}

		if (profile->auto_playback_recordings) {
		  play_file:
			memset(&fh, 0, sizeof(fh));
			args.input_callback = control_playback;
			memset(&cc, 0, sizeof(cc));
			cc.profile = profile;
			cc.fh = &fh;
			args.buf = &cc;
			switch_ivr_play_file(session, &fh, file_path, &args);
		}
		while (switch_channel_ready(channel)) {
			*input = '\0';

			if (*cc.buf && *cc.buf != *profile->terminator_key) {
				*input = *cc.buf;
				*(input + 1) = '\0';
				status = SWITCH_STATUS_SUCCESS;
				*cc.buf = '\0';
			} else if (skip_record_check) {
				/* Skip the record check and simply return */
				goto end;
			} else {
				(void) vm_macro_get(session, VM_RECORD_FILE_CHECK_MACRO, key_buf, input, sizeof(input), 1, "", &term, profile->digit_timeout);
				if (!switch_channel_ready(channel)) goto end;
			}

			if (!strcmp(input, profile->listen_file_key)) {
				goto play_file;
			} else if (!strcmp(input, profile->record_file_key)) {
				goto record_file;
			} else {
				(void) switch_ivr_phrase_macro(session, VM_ACK_MACRO, "saved", NULL, NULL);
				goto end;
			}
		}
	}

  end:

	if (!got_file) {
		status = SWITCH_STATUS_NOTFOUND;
	}

	return status;
}


struct listen_callback {
	char created_epoch[255];
	char read_epoch[255];
	char user[255];
	char domain[255];
	char uuid[255];
	char cid_name[255];
	char cid_number[255];
	char in_folder[255];
	char file_path[255];
	char message_len[255];
	char flags[255];
	char read_flags[255];
	char forwarded_by[255];
	char *email;
	int index;
	int want;
	msg_type_t type;
	msg_move_t move;
	char *convert_cmd;
	char *convert_ext;
};
typedef struct listen_callback listen_callback_t;

static int listen_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	listen_callback_t *cbt = (listen_callback_t *) pArg;

	if (cbt->index++ != cbt->want) {
		return 0;
	}

	switch_copy_string(cbt->created_epoch, argv[0], 255);
	switch_copy_string(cbt->read_epoch, argv[1], 255);
	switch_copy_string(cbt->user, argv[2], 255);
	switch_copy_string(cbt->domain, argv[3], 255);
	switch_copy_string(cbt->uuid, argv[4], 255);
	switch_copy_string(cbt->cid_name, argv[5], 255);
	switch_copy_string(cbt->cid_number, argv[6], 255);
	switch_copy_string(cbt->in_folder, argv[7], 255);
	switch_copy_string(cbt->file_path, argv[8], 255);
	switch_copy_string(cbt->message_len, argv[9], 255);
	switch_copy_string(cbt->flags, argv[10], 255);
	switch_copy_string(cbt->read_flags, argv[11], 255);
	switch_copy_string(cbt->forwarded_by, argv[12], 255);

	return -1;
}

static char *resolve_id(const char *myid, const char *domain_name, const char *action)
{
	switch_xml_t xx_user;
	switch_event_t *params;
	char *ret = (char *) myid;

	switch_event_create(&params, SWITCH_EVENT_GENERAL);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "action", action);

	if (switch_xml_locate_user_merged("id:number-alias", myid, domain_name, NULL, &xx_user, params) == SWITCH_STATUS_SUCCESS) {
		ret = strdup(switch_xml_attr(xx_user, "id"));
		switch_xml_free(xx_user);
	}

	switch_event_destroy(&params);
	return ret;
}

static void message_count(vm_profile_t *profile, const char *id_in, const char *domain_name, const char *myfolder, int *total_new_messages,
						  int *total_saved_messages, int *total_new_urgent_messages, int *total_saved_urgent_messages)
{
	char msg_count[80] = "";
	msg_cnt_callback_t cbt = { 0 };
	char *sql;
	char *myid = NULL;


	cbt.buf = msg_count;
	cbt.len = sizeof(msg_count);

	cbt.total_new_messages = 0;
	cbt.total_new_urgent_messages = 0;
	cbt.total_saved_messages = 0;
	cbt.total_saved_urgent_messages = 0;

	myid = resolve_id(id_in, domain_name, "message-count");

	sql = switch_mprintf(
						 "select 1, read_flags, count(read_epoch) from voicemail_msgs where "
						 "username='%q' and domain='%q' and in_folder='%q' and read_epoch=0 "
						 "group by read_flags "
						 "union "
						 "select 0, read_flags, count(read_epoch) from voicemail_msgs where "
						 "username='%q' and domain='%q' and in_folder='%q' and read_epoch<>0 "
						 "group by read_flags;",

						 myid, domain_name, myfolder,
						 myid, domain_name, myfolder);

	vm_execute_sql_callback(profile, profile->mutex, sql, message_count_callback, &cbt);
	free(sql);

	*total_new_messages = cbt.total_new_messages + cbt.total_new_urgent_messages;
	*total_new_urgent_messages = cbt.total_new_urgent_messages;
	*total_saved_messages = cbt.total_saved_messages + cbt.total_saved_urgent_messages;
	*total_saved_urgent_messages = cbt.total_saved_urgent_messages;

	if (myid != id_in) {
		free(myid);
	}
}

/* TODO Port this as switch_ core function */
switch_status_t vm_merge_media_files(const char** inputs, const char *output) {
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_file_handle_t fh_output = { 0 };
	int channels = 1;
	int rate = 8000; /* TODO Make this configurable */
	int j = 0;

	if (switch_core_file_open(&fh_output, output, channels, rate, SWITCH_FILE_FLAG_WRITE | SWITCH_FILE_DATA_SHORT, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't open %s\n", output);
		goto end;
	}

	for (j = 0; inputs[j] != NULL && j < 128 && status == SWITCH_STATUS_SUCCESS; j++) {
		switch_file_handle_t fh_input = { 0 };
		char buf[2048];
		switch_size_t len = sizeof(buf) / 2;

		if (switch_core_file_open(&fh_input, inputs[j], channels, rate, SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't open %s\n", inputs[j]);
			status = SWITCH_STATUS_GENERR;
			break;
		}

		while (switch_core_file_read(&fh_input, buf, &len) == SWITCH_STATUS_SUCCESS) {
			if (switch_core_file_write(&fh_output, buf, &len) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Write error\n");
				status = SWITCH_STATUS_GENERR;
				break;
			}
		}

		if (fh_input.file_interface) {
			switch_core_file_close(&fh_input);
		}
	}

	if (fh_output.file_interface) {
		switch_core_file_close(&fh_output);
	}
end:
	return status;
}

#define VM_STARTSAMPLES 1024 * 32

static char *vm_merge_file(switch_core_session_t *session, vm_profile_t *profile, const char *announce, const char *orig)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_file_handle_t lrfh = { 0 }, *rfh = NULL, lfh = {
	0}, *fh = NULL;
	char *tmp_path;
	switch_uuid_t uuid;
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	char *ret = NULL;
	int16_t *abuf = NULL;
	switch_size_t olen = 0;
	int asis = 0;
	switch_codec_implementation_t read_impl = { 0 };
	switch_core_session_get_read_impl(session, &read_impl);

	switch_uuid_get(&uuid);
	switch_uuid_format(uuid_str, &uuid);

	lfh.channels = read_impl.number_of_channels;
	lfh.native_rate = read_impl.actual_samples_per_second;

	tmp_path = switch_core_session_sprintf(session, "%s%smsg_%s.%s", SWITCH_GLOBAL_dirs.temp_dir, SWITCH_PATH_SEPARATOR, uuid_str, profile->file_ext);

	if (switch_core_file_open(&lfh,
							  tmp_path,
							  lfh.channels,
							  read_impl.actual_samples_per_second, SWITCH_FILE_FLAG_WRITE | SWITCH_FILE_DATA_SHORT, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failed to open file %s\n", tmp_path);
		goto end;
	}

	fh = &lfh;


	if (switch_core_file_open(&lrfh,
							  announce,
							  lfh.channels,
							  read_impl.actual_samples_per_second, SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failed to open file %s\n", announce);
		goto end;
	}

	rfh = &lrfh;

	switch_zmalloc(abuf, VM_STARTSAMPLES * sizeof(*abuf));

	if (switch_test_flag(fh, SWITCH_FILE_NATIVE)) {
		asis = 1;
	}

	while (switch_channel_ready(channel)) {
		olen = VM_STARTSAMPLES;

		if (!asis) {
			olen /= 2;
		}

		if (switch_core_file_read(rfh, abuf, &olen) != SWITCH_STATUS_SUCCESS || !olen) {
			break;
		}

		switch_core_file_write(fh, abuf, &olen);

	}

	if (rfh) {
		switch_core_file_close(rfh);
		rfh = NULL;
	}

	if (switch_core_file_open(&lrfh,
							  orig,
							  lfh.channels,
							  read_impl.actual_samples_per_second, SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failed to open file %s\n", orig);
		goto end;
	}

	rfh = &lrfh;

	while (switch_channel_ready(channel)) {
		olen = VM_STARTSAMPLES;

		if (!asis) {
			olen /= 2;
		}

		if (switch_core_file_read(rfh, abuf, &olen) != SWITCH_STATUS_SUCCESS || !olen) {
			break;
		}

		switch_core_file_write(fh, abuf, &olen);

	}

	if (unlink(announce) != 0) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Failed to delete file [%s]\n", announce);
	}
	ret = tmp_path;

  end:

	if (fh) {
		switch_core_file_close(fh);
		fh = NULL;
	}

	if (rfh) {
		switch_core_file_close(rfh);
		rfh = NULL;
	}

	switch_safe_free(abuf);

	return ret;

}


static switch_status_t listen_file(switch_core_session_t *session, vm_profile_t *profile, listen_callback_t *cbt)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_input_args_t args = { 0 };
	char term;
	char input[10] = "", key_buf[80] = "";
	switch_file_handle_t fh = { 0 };
	cc_t cc = { 0 };
	char *forward_file_path = NULL;
	switch_core_session_message_t msg = { 0 };
	char cid_buf[1024] = "";

	if (switch_channel_ready(channel)) {
		switch_snprintf(cid_buf, sizeof(cid_buf), "%s|%s", cbt->cid_name, cbt->cid_number);

		msg.from = __FILE__;
		msg.string_arg = cid_buf;
		msg.message_id = SWITCH_MESSAGE_INDICATE_DISPLAY;

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Sending display update [%s] to %s\n", 
						  cid_buf, switch_channel_get_name(channel));
		switch_core_session_receive_message(session, &msg);
		
		if (!zstr(cbt->cid_number) && (switch_true(switch_channel_get_variable(channel, "vm_announce_cid")))) {
			TRY_CODE(switch_ivr_phrase_macro(session, VM_SAY_PHONE_NUMBER_MACRO, cbt->cid_number, NULL, NULL));
		}
		
		args.input_callback = cancel_on_dtmf;
		
		switch_snprintf(key_buf, sizeof(key_buf), "%s:%s:%s:%s:%s:%s%s%s", profile->repeat_msg_key, profile->save_file_key,
						profile->delete_file_key, profile->email_key, profile->callback_key,
						profile->forward_key, cbt->email ? ":" : "", cbt->email ? cbt->email : "");


		switch_snprintf(input, sizeof(input), "%s:%d", cbt->type == MSG_NEW ? "new" : "saved", cbt->want + 1);
		memset(&cc, 0, sizeof(cc));
		cc.profile = profile;
		args.buf = &cc;
		args.input_callback = control_playback;
		TRY_CODE(switch_ivr_phrase_macro(session, VM_SAY_MESSAGE_NUMBER_MACRO, input, NULL, &args));

	  play_file:

		if (!*cc.buf && (profile->play_date_announcement == VM_DATE_FIRST)) {
			cc.fh = NULL;
			TRY_CODE(switch_ivr_phrase_macro(session, VM_SAY_DATE_MACRO, cbt->created_epoch, NULL, &args));
		}

		if (!*cc.buf || *cc.buf == *cc.profile->skip_info_key) {
			*cc.buf = '\0';
			memset(&fh, 0, sizeof(fh));
			cc.fh = &fh;
			cc.playback_controls_active = 1;
			if (switch_file_exists(cbt->file_path, switch_core_session_get_pool(session)) == SWITCH_STATUS_SUCCESS) {
				TRY_CODE(switch_ivr_play_file(session, &fh, cbt->file_path, &args));
			}
			cc.playback_controls_active = 0;
		}

		if (!*cc.buf && (profile->play_date_announcement == VM_DATE_LAST)) {
			cc.fh = NULL;
			TRY_CODE(switch_ivr_phrase_macro(session, VM_SAY_DATE_MACRO, cbt->created_epoch, NULL, &args));
		}

		if (switch_channel_ready(channel)) {
			if (*cc.buf && *cc.buf != *profile->terminator_key) {
				*input = *cc.buf;
				*(input + 1) = '\0';
				status = SWITCH_STATUS_SUCCESS;
				*cc.buf = '\0';
			} else {
				TRY_CODE(vm_macro_get(session, VM_LISTEN_FILE_CHECK_MACRO, key_buf, input, sizeof(input), 1, "", &term, profile->digit_timeout));
			}
			if (!strcmp(input, profile->prev_msg_key)) {
				cbt->move = VM_MOVE_PREV;
			} else if (!strcmp(input, profile->repeat_msg_key)) {
				cbt->move = VM_MOVE_SAME;
			} else if (!strcmp(input, profile->next_msg_key)) {
				cbt->move = VM_MOVE_NEXT;
			} else if (!strcmp(input, profile->listen_file_key)) {
				*cc.buf = '\0';
				goto play_file;
			} else if (!strcmp(input, profile->callback_key)) {
				const char *callback_dialplan;
				const char *callback_context;

				if (!(callback_dialplan = switch_channel_get_variable(channel, "voicemail_callback_dialplan"))) {
					callback_dialplan = profile->callback_dialplan;
				}

				if (!(callback_context = switch_channel_get_variable(channel, "voicemail_callback_context"))) {
					callback_context = profile->callback_context;
				}

				switch_core_session_execute_exten(session, cbt->cid_number, callback_dialplan, callback_context);
			} else if (!strcmp(input, profile->forward_key)) {
				char *cmd = NULL;
				char *new_file_path = NULL;
				char vm_cc[256] = "";
				char macro_buf[80] = "";
				int ok = 0;

				switch_xml_t x_param, x_params, x_user = NULL;
				switch_event_t *my_params = NULL;
				switch_bool_t vm_enabled = SWITCH_TRUE;

				switch_snprintf(key_buf, sizeof(key_buf), "%s:%s", profile->prepend_key, profile->forward_key);
				TRY_CODE(vm_macro_get(session, VM_FORWARD_PREPEND_MACRO, key_buf, input, sizeof(input), 1, "", &term, profile->digit_timeout));
				if (!strcmp(input, profile->prepend_key)) {
					switch_uuid_t uuid;
					char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
					switch_size_t message_len = 0;
					char *new_path = NULL;

					switch_uuid_get(&uuid);
					switch_uuid_format(uuid_str, &uuid);

					forward_file_path =
						switch_core_session_sprintf(session, "%s%smsg_%s.wav", SWITCH_GLOBAL_dirs.temp_dir, SWITCH_PATH_SEPARATOR, uuid_str);
					TRY_CODE(create_file(session, profile, VM_RECORD_MESSAGE_MACRO, forward_file_path, &message_len, SWITCH_TRUE, NULL, NULL));
					if ((new_path = vm_merge_file(session, profile, forward_file_path, cbt->file_path))) {
						switch_ivr_sleep(session, 1500, SWITCH_TRUE, NULL);
						forward_file_path = new_path;
					} else {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error merging files\n");
						TRY_CODE(switch_ivr_phrase_macro(session, VM_ACK_MACRO, "deleted", NULL, NULL));
						goto end;
					}

					new_file_path = forward_file_path;
				} else {
					new_file_path = cbt->file_path;
				}

				while (!ok) {

					xml_safe_free(x_user);

					switch_snprintf(macro_buf, sizeof(macro_buf), "phrase:%s:%s", VM_FORWARD_MESSAGE_ENTER_EXTENSION_MACRO, profile->terminator_key);
					vm_cc[0] = '\0';

					TRY_CODE(switch_ivr_read
							 (session, 0, sizeof(vm_cc), macro_buf, NULL, vm_cc, sizeof(vm_cc), profile->digit_timeout, profile->terminator_key, 0));

					cmd = switch_core_session_sprintf(session, "%s@%s@%s %s %s '%s'", vm_cc, cbt->domain, profile->name, 
													  new_file_path, cbt->cid_number, cbt->cid_name);

					switch_event_create(&my_params, SWITCH_EVENT_REQUEST_PARAMS);
					switch_assert(my_params);

					status = switch_xml_locate_user_merged("id:number-alias", vm_cc, cbt->domain, NULL, &x_user, my_params);
					switch_event_destroy(&my_params);
				
					if (status != SWITCH_STATUS_SUCCESS) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, 
										  "Failed to forward message - Cannot locate user %s@%s\n", vm_cc, cbt->domain);
						TRY_CODE(switch_ivr_phrase_macro(session, VM_INVALID_EXTENSION_MACRO, vm_cc, NULL, NULL));
						continue;
					} else {
						x_params = switch_xml_child(x_user, "params");

						for (x_param = switch_xml_child(x_params, "param"); x_param; x_param = x_param->next) {
							const char *var = switch_xml_attr_soft(x_param, "name");
							const char *val = switch_xml_attr_soft(x_param, "value");
							if (zstr(var) || zstr(val)) {
								continue; /* Ignore empty entires */
							}

							if (!strcasecmp(var, "vm-enabled")) {
								vm_enabled = !switch_false(val);
							}
						}

						if (!vm_enabled) {
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Failed to forward message - Voicemail is disabled for user %s@%s\n", vm_cc, cbt->domain);
							TRY_CODE(switch_ivr_phrase_macro(session, VM_INVALID_EXTENSION_MACRO, vm_cc, NULL, NULL));
							continue;
						} else {
							if (voicemail_inject(cmd, session) == SWITCH_STATUS_SUCCESS) {
								switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Forwarded message to %s\n", vm_cc);
								TRY_CODE(switch_ivr_phrase_macro(session, VM_ACK_MACRO, "saved", NULL, NULL));
								cbt->move = VM_MOVE_SAME;
							} else {
								switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failed to forward message to %s\n", vm_cc);
								TRY_CODE(switch_ivr_phrase_macro(session, VM_INVALID_EXTENSION_MACRO, vm_cc, NULL, NULL));
								continue;
							}
						}
					}

					xml_safe_free(x_user);

					break;
				}

			} else if (!strcmp(input, profile->delete_file_key) || (!strcmp(input, profile->email_key) && !zstr(cbt->email))) {
				char *sql = switch_mprintf("update voicemail_msgs set flags='delete' where uuid='%s'", cbt->uuid);
				vm_execute_sql(profile, sql, profile->mutex);
				switch_safe_free(sql);
				if (!strcmp(input, profile->email_key) && !zstr(cbt->email)) {
					switch_event_t *event;
					char *from;
					char *headers, *header_string;
					char *body;
					int priority = 3;
					switch_size_t retsize;
					switch_time_exp_t tm;
					char date[80] = "";
					char tmp[50] = "";
					int total_new_messages = 0;
					int total_saved_messages = 0;
					int total_new_urgent_messages = 0;
					int total_saved_urgent_messages = 0;
					int32_t message_len = 0;
					char *p;
					switch_time_t l_duration = 0;
					switch_core_time_duration_t duration;
					char duration_str[80];
					char *formatted_cid_num = NULL;
					if (!strcasecmp(cbt->read_flags, URGENT_FLAG_STRING)) {
						priority = 1;
					}

					message_count(profile, cbt->user, cbt->domain, cbt->in_folder, &total_new_messages, &total_saved_messages,
								  &total_new_urgent_messages, &total_saved_urgent_messages);

					switch_time_exp_lt(&tm, switch_time_make(atol(cbt->created_epoch), 0));
					switch_strftime(date, &retsize, sizeof(date), profile->date_fmt, &tm);

					formatted_cid_num = switch_format_number(cbt->cid_number);

					switch_snprintf(tmp, sizeof(tmp), "%d", total_new_messages);
					switch_channel_set_variable(channel, "voicemail_total_new_messages", tmp);
					switch_snprintf(tmp, sizeof(tmp), "%d", total_saved_messages);
					switch_channel_set_variable(channel, "voicemail_total_saved_messages", tmp);
					switch_snprintf(tmp, sizeof(tmp), "%d", total_new_urgent_messages);
					switch_channel_set_variable(channel, "voicemail_urgent_new_messages", tmp);
					switch_snprintf(tmp, sizeof(tmp), "%d", total_saved_urgent_messages);
					switch_channel_set_variable(channel, "voicemail_urgent_saved_messages", tmp);
					switch_channel_set_variable(channel, "voicemail_current_folder", cbt->in_folder);
					switch_channel_set_variable(channel, "voicemail_account", cbt->user);
					switch_channel_set_variable(channel, "voicemail_domain", cbt->domain);
					switch_channel_set_variable(channel, "voicemail_caller_id_number", cbt->cid_number);
					switch_channel_set_variable(channel, "voicemail_formatted_caller_id_number", formatted_cid_num);
					switch_channel_set_variable(channel, "voicemail_caller_id_name", cbt->cid_name);
					switch_channel_set_variable(channel, "voicemail_file_path", cbt->file_path);
					switch_channel_set_variable(channel, "voicemail_read_flags", cbt->read_flags);
					switch_channel_set_variable(channel, "voicemail_time", date);
					switch_snprintf(tmp, sizeof(tmp), "%d", priority);
					switch_channel_set_variable(channel, "voicemail_priority", tmp);
					message_len = atoi(cbt->message_len);
					switch_safe_free(formatted_cid_num);

					l_duration = switch_time_make(atol(cbt->message_len), 0);
					switch_core_measure_time(l_duration, &duration);
					duration.day += duration.yr * 365;
					duration.hr += duration.day * 24;

					switch_snprintf(duration_str, sizeof(duration_str), "%.2u:%.2u:%.2u", duration.hr, duration.min, duration.sec);

					switch_channel_set_variable(channel, "voicemail_message_len", duration_str);
					switch_channel_set_variable(channel, "voicemail_email", cbt->email);

					if (zstr(profile->email_headers)) {
						from = switch_core_session_sprintf(session, "%s@%s", cbt->user, cbt->domain);
					} else {
						from = switch_channel_expand_variables(channel, profile->email_from);
					}

					if (zstr(profile->email_headers)) {
						headers = switch_core_session_sprintf(session,
															  "From: FreeSWITCH mod_voicemail <%s@%s>\nSubject: Voicemail from %s %s\nX-Priority: %d",
															  cbt->user, cbt->domain, cbt->cid_name, cbt->cid_number, priority);
					} else {
						headers = switch_channel_expand_variables(channel, profile->email_headers);
					}

					p = headers + (strlen(headers) - 1);
					if (*p == '\n') {
						if (*(p - 1) == '\r') {
							p--;
						}
						*p = '\0';
					}

					header_string = switch_core_session_sprintf(session, "%s\nX-Voicemail-Length: %u", headers, message_len);

					if (switch_event_create(&event, SWITCH_EVENT_GENERAL) == SWITCH_STATUS_SUCCESS) {
						/* this isn't done?  it was in the other place
						 * switch_channel_event_set_data(channel, event);
						 */
						switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Message-Type", "forwarded-voicemail");
						switch_event_fire(&event);
					}

					if (profile->email_body) {
						body = switch_channel_expand_variables(channel, profile->email_body);
					} else {
						body = switch_mprintf("%u second Voicemail from %s %s", message_len, cbt->cid_name, cbt->cid_number);
					}

					switch_simple_email(cbt->email, from, header_string, body, cbt->file_path, cbt->convert_cmd, cbt->convert_ext);
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Sending message to %s\n", cbt->email);
					switch_safe_free(body);
					TRY_CODE(switch_ivr_phrase_macro(session, VM_ACK_MACRO, "emailed", NULL, NULL));
				} else {
					TRY_CODE(switch_ivr_phrase_macro(session, VM_ACK_MACRO, "deleted", NULL, NULL));
				}
			} else {
				char *sql = switch_mprintf("update voicemail_msgs set flags='save' where uuid='%s'", cbt->uuid);
				vm_execute_sql(profile, sql, profile->mutex);
				switch_safe_free(sql);
				TRY_CODE(switch_ivr_phrase_macro(session, VM_ACK_MACRO, "saved", NULL, NULL));
			}
		}
	}

  end:

	if (forward_file_path) {
		if (unlink(forward_file_path) != 0) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Failed to delete file [%s]\n", forward_file_path);
		}
	}

	return status;
}


static void update_mwi(vm_profile_t *profile, const char *id, const char *domain_name, const char *myfolder, mwi_reason_t reason)
{
	const char *yn = "no";
	const char *update_reason = mwi_reason2str(reason); 
	int total_new_messages = 0;
	int total_saved_messages = 0;
	int total_new_urgent_messages = 0;
	int total_saved_urgent_messages = 0;
	switch_event_t *event;
	switch_event_t *message_event;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Update MWI: Processing for %s@%s in %s\n", id, domain_name, myfolder);

	message_count(profile, id, domain_name, myfolder, &total_new_messages, &total_saved_messages, &total_new_urgent_messages,
				  &total_saved_urgent_messages);

	if (switch_event_create(&event, SWITCH_EVENT_MESSAGE_WAITING) != SWITCH_STATUS_SUCCESS) {
		return;
	}

	if (total_new_messages || total_new_urgent_messages) {
		yn = "yes";
	}
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "MWI-Messages-Waiting", yn);
	switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Update-Reason", update_reason);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "MWI-Message-Account", "%s@%s", id, domain_name);
	/* 
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "MWI-Voice-Message", "%d/%d (%d/%d)", total_new_messages, total_saved_messages,
							total_new_urgent_messages, total_saved_urgent_messages);
	*/
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "MWI-Voice-Message", "%d/%d", total_new_messages, total_saved_messages);

	switch_event_fire(&event);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Update MWI: Messages Waiting %s\n", yn);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Update MWI: Update Reason %s\n", update_reason);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Update MWI: Message Account %s@%s\n", id, domain_name);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Update MWI: Voice Message %d/%d\n", total_new_messages, total_saved_messages);

	switch_event_create_subclass(&message_event, SWITCH_EVENT_CUSTOM, VM_EVENT_MAINT);
	switch_event_add_header_string(message_event, SWITCH_STACK_BOTTOM, "VM-Action", "mwi-update");
	switch_event_add_header_string(message_event, SWITCH_STACK_BOTTOM, "VM-User", id);
	switch_event_add_header_string(message_event, SWITCH_STACK_BOTTOM, "VM-Domain", domain_name);
	switch_event_add_header(message_event, SWITCH_STACK_BOTTOM, "VM-Total-New", "%d", total_new_messages);
	switch_event_add_header(message_event, SWITCH_STACK_BOTTOM, "VM-Total-Saved", "%d", total_saved_messages);
	switch_event_add_header(message_event, SWITCH_STACK_BOTTOM, "VM-Total-New-Urgent", "%d", total_new_urgent_messages);
	switch_event_add_header(message_event, SWITCH_STACK_BOTTOM, "VM-Total-Saved-Urgent", "%d", total_saved_urgent_messages);

	switch_event_fire(&message_event);
}

static void voicemail_check_main(switch_core_session_t *session, vm_profile_t *profile, const char *domain_name, const char *id, int auth, const char *uuid_in)
{
	vm_check_state_t vm_check_state = VM_CHECK_START;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_caller_profile_t *caller_profile = switch_channel_get_caller_profile(channel);
	switch_xml_t x_user = NULL, x_params, x_param;
	switch_status_t status;
	char pass_buf[80] = "", *mypass = NULL, id_buf[80] = "", *myfolder = NULL;
	const char *thepass = NULL, *myid = id, *thehash = NULL, *vmhash = NULL;
	char term = 0;
	uint32_t timeout, attempts = 0, retries = 0;
	int failed = 0;
	msg_type_t play_msg_type = MSG_NONE;
	char *dir_path = NULL, *file_path = NULL, *tmp_file_path = NULL;
	int total_new_messages = 0;
	int total_saved_messages = 0;
	int total_new_urgent_messages = 0;
	int total_saved_urgent_messages = 0;
	int heard_auto_new = 0;
	char *vm_email = NULL, *email_addr = NULL;
	char *convert_cmd = profile->convert_cmd;
	char *convert_ext = profile->convert_ext;
	char *vm_storage_dir = NULL;
	char *storage_dir = NULL;
	char global_buf[2] = "";
	switch_input_args_t args = { 0 };
	const char *caller_id_number = NULL;
	int auth_only = 0, authed = 0;
	switch_event_t *event;

	if (!(caller_id_number = switch_channel_get_variable(channel, "effective_caller_id_number"))) {
		caller_id_number = caller_profile->caller_id_number;
	}

	if (auth == 2) {
		auth_only = 1;
		auth = 0;
	} else {
		auth_only = switch_true(switch_channel_get_variable(channel, "vm_auth_only"));
	}

	timeout = profile->digit_timeout;
	attempts = profile->max_login_attempts;
	switch_ivr_phrase_macro(session, VM_HELLO_MACRO, NULL, NULL, &args);
	*global_buf = '\0';

	while (switch_channel_ready(channel)) {
		switch_ivr_sleep(session, 100, SWITCH_TRUE, NULL);

		switch (vm_check_state) {
		case VM_CHECK_START:
			{
				total_new_messages = 0;
				total_saved_messages = 0;
				total_new_urgent_messages = 0;
				total_saved_urgent_messages = 0;
				heard_auto_new = 0;
				play_msg_type = MSG_NONE;
				attempts = profile->max_login_attempts;
				retries = profile->max_retries;
				myid = id;
				mypass = NULL;
				myfolder = "inbox";
				vm_check_state = VM_CHECK_AUTH;
				xml_safe_free(x_user);
			}
			break;
		case VM_CHECK_FOLDER_SUMMARY:
			{
				int informed = 0;
				char msg_count[80] = "";
				switch_input_args_t folder_args = { 0 };
				switch_event_t *params;
				const char *vm_auto_play = switch_channel_get_variable(channel, "vm_auto_play");
				int auto_play = 1;

				if (vm_auto_play && !switch_true(vm_auto_play)) {
					auto_play = 0;
				}
				
				folder_args.input_callback = cancel_on_dtmf;
				folder_args.buf = &global_buf;
				folder_args.buflen = sizeof(global_buf);

				switch_channel_set_variable(channel, "voicemail_current_folder", myfolder);
				message_count(profile, myid, domain_name, myfolder, &total_new_messages, &total_saved_messages,
							  &total_new_urgent_messages, &total_saved_urgent_messages);


				switch_event_create_subclass(&params, SWITCH_EVENT_CUSTOM, VM_EVENT_MAINT);
				switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "VM-Action", "folder-summary");
				switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "VM-User", myid);
				switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "VM-Domain", domain_name);
				switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "VM-Folder", myfolder);
				switch_event_add_header(params, SWITCH_STACK_BOTTOM, "VM-Total-New-Messages", "%u", total_new_messages);
				switch_event_add_header(params, SWITCH_STACK_BOTTOM, "VM-Total-Saved-Messages", "%u", total_saved_messages);
				switch_event_add_header(params, SWITCH_STACK_BOTTOM, "VM-Total-New-Urgent-Messages", "%u", total_new_urgent_messages);
				switch_event_add_header(params, SWITCH_STACK_BOTTOM, "VM-Total-Saved-Urgent-Messages", "%u", total_saved_urgent_messages);
				switch_event_fire(&params);

				if (total_new_urgent_messages > 0) {
					switch_snprintf(msg_count, sizeof(msg_count), "%d:urgent-new", total_new_urgent_messages);
					TRY_CODE(switch_ivr_phrase_macro(session, VM_MESSAGE_COUNT_MACRO, msg_count, NULL, &folder_args));
					informed++;
					if (auto_play && !zstr_buf(global_buf)) {
						vm_check_state = VM_CHECK_MENU;
						continue;
					}
				}

				if (total_new_messages > 0 && total_new_messages != total_new_urgent_messages) {
					switch_snprintf(msg_count, sizeof(msg_count), "%d:new", total_new_messages);
					TRY_CODE(switch_ivr_phrase_macro(session, VM_MESSAGE_COUNT_MACRO, msg_count, NULL, &folder_args));
					informed++;
					if (auto_play && !zstr_buf(global_buf)) {
						vm_check_state = VM_CHECK_MENU;
						continue;
					}
				}
				
				if (auto_play && !heard_auto_new && total_new_messages + total_new_urgent_messages > 0) {
					heard_auto_new = 1;
					play_msg_type = MSG_NEW;
					vm_check_state = VM_CHECK_PLAY_MESSAGES;
					continue;
				}

				if (!informed) {
					switch_snprintf(msg_count, sizeof(msg_count), "0:new");
					TRY_CODE(switch_ivr_phrase_macro(session, VM_MESSAGE_COUNT_MACRO, msg_count, NULL, &folder_args));
					switch_snprintf(msg_count, sizeof(msg_count), "%d:saved", total_saved_messages);
					TRY_CODE(switch_ivr_phrase_macro(session, VM_MESSAGE_COUNT_MACRO, msg_count, NULL, &folder_args));
					informed++;
				}

				vm_check_state = VM_CHECK_MENU;
			}
			break;
		case VM_CHECK_PLAY_MESSAGES:
			{
				listen_callback_t cbt;
				char sql[512];
				int cur_message, total_messages;

				message_count(profile, myid, domain_name, myfolder, &total_new_messages, &total_saved_messages,
							  &total_new_urgent_messages, &total_saved_urgent_messages);
				memset(&cbt, 0, sizeof(cbt));
				cbt.email = vm_email ? vm_email : email_addr;
				cbt.convert_cmd = convert_cmd;
				cbt.convert_ext = convert_ext;
				cbt.move = VM_MOVE_NEXT;
				switch (play_msg_type) {
				case MSG_NEW:
					{
						switch_snprintf(sql, sizeof(sql),
										"select created_epoch, read_epoch, username, domain, uuid, cid_name, cid_number, in_folder, file_path, message_len, flags, read_flags, forwarded_by from voicemail_msgs where username='%s' and domain='%s' and read_epoch=0"
										" order by read_flags, created_epoch %s", myid, domain_name,
										profile->play_new_messages_lifo ? "desc" : "asc");
						total_messages = total_new_messages;
						heard_auto_new = 1;
					}
					break;
				case MSG_SAVED:
				default:
					{
						switch_snprintf(sql, sizeof(sql),
										"select created_epoch, read_epoch, username, domain, uuid, cid_name, cid_number, in_folder, file_path, message_len, flags, read_flags, forwarded_by from voicemail_msgs where username='%s' and domain='%s' and read_epoch !=0"
										" order by read_flags, created_epoch %s", myid, domain_name,
										profile->play_saved_messages_lifo ? "desc" : "asc");
						total_messages = total_saved_messages;
						heard_auto_new = 1;
					}
					break;
				}
				for (cur_message = 0; cur_message < total_messages; cur_message++) {
					cbt.index = 0;
					cbt.want = cur_message;
					cbt.type = play_msg_type;
					cbt.move = VM_MOVE_NEXT;
					vm_execute_sql_callback(profile, profile->mutex, sql, listen_callback, &cbt);
					if (!zstr(uuid_in) && strcmp(cbt.uuid, uuid_in)) {
						continue;
					}
					status = listen_file(session, profile, &cbt);
					if (cbt.move == VM_MOVE_PREV) {
						if (cur_message <= 0) {
							cur_message = -1;
						} else {
							cur_message -= 2;
						}
					} else if (cbt.move == VM_MOVE_SAME) {
						cur_message -= 1;
					}
					
					if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_BREAK) {
						break;
					}
				}
				switch_snprintf(sql, sizeof(sql), "update voicemail_msgs set read_epoch=%ld where read_epoch=0 and "
								"username='%s' and domain='%s' and flags='save'",
								(long) switch_epoch_time_now(NULL), myid, domain_name);
				vm_execute_sql(profile, sql, profile->mutex);
				switch_snprintfv(sql, sizeof(sql), "select file_path from voicemail_msgs where username='%q' and domain='%q' and flags='delete'", myid,
								domain_name);
				vm_execute_sql_callback(profile, profile->mutex, sql, unlink_callback, NULL);
				switch_snprintfv(sql, sizeof(sql), "delete from voicemail_msgs where username='%q' and domain='%q' and flags='delete'", myid, domain_name);
				vm_execute_sql(profile, sql, profile->mutex);
				vm_check_state = VM_CHECK_FOLDER_SUMMARY;

				update_mwi(profile, myid, domain_name, myfolder, MWI_REASON_PURGE);
			}
			break;
		case VM_CHECK_CONFIG:
			{
				char *sql = NULL;
				char input[10] = "";
				char key_buf[80] = "";
				callback_t cbt = { 0 };
				char msg_count[80] = "";
				cc_t cc = { 0 };
				switch_size_t message_len = 0;

				cbt.buf = msg_count;
				cbt.len = sizeof(msg_count);
				sql = switch_mprintf("select count(*) from voicemail_prefs where username='%q' and domain = '%q'", myid, domain_name);
				vm_execute_sql_callback(profile, profile->mutex, sql, sql2str_callback, &cbt);
				switch_safe_free(sql);
				if (*msg_count == '\0' || !atoi(msg_count)) {
					sql = switch_mprintf("insert into voicemail_prefs values('%q','%q','','','')", myid, domain_name);
					vm_execute_sql(profile, sql, profile->mutex);
					switch_safe_free(sql);
				}

				switch_snprintf(key_buf, sizeof(key_buf), "%s:%s:%s:%s:%s",
								profile->record_greeting_key,
								profile->choose_greeting_key, profile->record_name_key, profile->change_pass_key, profile->main_menu_key);


				TRY_CODE(vm_macro_get(session, VM_CONFIG_MENU_MACRO, key_buf, input, sizeof(input), 1, "", &term, timeout));
				if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_BREAK) {
					goto end;
				}

				if (!strcmp(input, profile->main_menu_key)) {
					vm_check_state = VM_CHECK_MENU;
				} else if (!strcmp(input, profile->choose_greeting_key)) {
					int num;
					switch_input_args_t greeting_args = { 0 };
					greeting_args.input_callback = cancel_on_dtmf;

					TRY_CODE(vm_macro_get(session, VM_CHOOSE_GREETING_MACRO, key_buf, input, sizeof(input), 1, "", &term, timeout));


					num = atoi(input);
					file_path = switch_mprintf("%s%sgreeting_%d.%s", dir_path, SWITCH_PATH_SEPARATOR, num, profile->file_ext);
					if (num < 1 || num > VM_MAX_GREETINGS) {
						status = SWITCH_STATUS_FALSE;
					} else {
						switch_file_handle_t fh = { 0 };
						memset(&fh, 0, sizeof(fh));
						greeting_args.input_callback = control_playback;
						memset(&cc, 0, sizeof(cc));
						cc.profile = profile;
						cc.fh = &fh;
						cc.noexit = 1;
						greeting_args.buf = &cc;
						status = switch_ivr_play_file(session, &fh, file_path, &greeting_args);
					}
					if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_BREAK) {
						TRY_CODE(switch_ivr_phrase_macro(session, VM_CHOOSE_GREETING_FAIL_MACRO, NULL, NULL, NULL));
					} else {
						switch_event_t *params;

						TRY_CODE(switch_ivr_phrase_macro(session, VM_CHOOSE_GREETING_SELECTED_MACRO, input, NULL, NULL));
						sql =
							switch_mprintf("update voicemail_prefs set greeting_path='%s' where username='%s' and domain='%s'", file_path, myid,
										   domain_name);
						vm_execute_sql(profile, sql, profile->mutex);
						switch_safe_free(sql);

						switch_event_create_subclass(&params, SWITCH_EVENT_CUSTOM, VM_EVENT_MAINT);
						switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "VM-Action", "change-greeting");
						switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "VM-Greeting-Path", file_path);
						switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "VM-User", myid);
						switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "VM-Domain", domain_name);
						switch_channel_event_set_data(channel, params);
						switch_event_fire(&params);
					}
					switch_safe_free(file_path);
				} else if (!strcmp(input, profile->record_greeting_key)) {
					int num;
					TRY_CODE(vm_macro_get(session, VM_CHOOSE_GREETING_MACRO, key_buf, input, sizeof(input), 1, "", &term, timeout));

					num = atoi(input);
					if (num < 1 || num > VM_MAX_GREETINGS) {
						TRY_CODE(switch_ivr_phrase_macro(session, VM_CHOOSE_GREETING_FAIL_MACRO, NULL, NULL, NULL));
					} else {
						switch_event_t *params;
						file_path = switch_mprintf("%s%sgreeting_%d.%s", dir_path, SWITCH_PATH_SEPARATOR, num, profile->file_ext);
						tmp_file_path = switch_mprintf("%s%sgreeting_%d_TMP.%s", dir_path, SWITCH_PATH_SEPARATOR, num, profile->file_ext);
						unlink(tmp_file_path);

						TRY_CODE(create_file(session, profile, VM_RECORD_GREETING_MACRO, file_path, &message_len, SWITCH_TRUE, NULL, NULL));
						switch_file_rename(tmp_file_path, file_path, switch_core_session_get_pool(session));
						
						sql =
							switch_mprintf("update voicemail_prefs set greeting_path='%s' where username='%s' and domain='%s'", file_path, myid,
										   domain_name);
						vm_execute_sql(profile, sql, profile->mutex);
						switch_safe_free(sql);
						switch_safe_free(file_path);
						switch_safe_free(tmp_file_path);

						switch_event_create_subclass(&params, SWITCH_EVENT_CUSTOM, VM_EVENT_MAINT);
						switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "VM-Action", "record-greeting");
						switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "VM-Greeting-Path", file_path);
						switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "VM-User", myid);
						switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "VM-Domain", domain_name);
						switch_channel_event_set_data(channel, params);
						switch_event_fire(&params);
					}

				} else if (!strcmp(input, profile->change_pass_key)) {
					char buf[256] = "";
					char macro[256] = "";
					switch_event_t *params;
					switch_xml_t xx_user, xx_domain, xx_domain_root;
					int fail = 0;
					int ok = 0;

					while (!ok) {
						fail = 0;
						switch_snprintf(macro, sizeof(macro), "phrase:%s:%s", VM_ENTER_PASS_MACRO, profile->terminator_key);
						TRY_CODE(switch_ivr_read(session, 0, 255, macro, NULL, buf, sizeof(buf), 10000, profile->terminator_key, 0));
					

						switch_event_create_subclass(&params, SWITCH_EVENT_CUSTOM, VM_EVENT_MAINT);
						switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "VM-Action", "change-password");
						switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "VM-User-Password", buf);
						switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "VM-User", myid);
						switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "VM-Domain", domain_name);
						switch_channel_event_set_data(channel, params);
						
						if (switch_xml_locate_user("id", myid, domain_name, switch_channel_get_variable(channel, "network_addr"),
												   &xx_domain_root, &xx_domain, &xx_user, NULL, params) == SWITCH_STATUS_SUCCESS) {
							switch_xml_t x_result;
							
							if ((x_result = switch_xml_child(xx_user, "result"))) {
								if (!switch_true(switch_xml_attr_soft(x_result, "success"))) {
									fail = 1;
								}
							}
							
							switch_xml_free(xx_domain_root);
						}
						
						if (fail) {
							/* add feedback for user - let him/her know that the password they tried to change to is not allowed */
							switch_ivr_phrase_macro(session, VM_CHANGE_PASS_FAIL_MACRO, NULL, NULL, NULL);
						} else {
							sql = switch_mprintf("update voicemail_prefs set password='%s' where username='%s' and domain='%s'", buf, myid, domain_name);
							vm_execute_sql(profile, sql, profile->mutex);
							switch_safe_free(file_path);
							switch_safe_free(sql);
							ok = 1;
							/* add feedback for user - let him/her know that password change was successful */
							switch_ivr_phrase_macro(session, VM_CHANGE_PASS_SUCCESS_MACRO, NULL, NULL, NULL);
						}
					
						switch_event_destroy(&params);
					}

				} else if (!strcmp(input, profile->record_name_key)) {
					switch_event_t *params;
					file_path = switch_mprintf("%s%srecorded_name.%s", dir_path, SWITCH_PATH_SEPARATOR, profile->file_ext);
					tmp_file_path = switch_mprintf("%s%srecorded_name_TMP.%s", dir_path, SWITCH_PATH_SEPARATOR, profile->file_ext);
					unlink(tmp_file_path);
					TRY_CODE(create_file(session, profile, VM_RECORD_NAME_MACRO, file_path, &message_len, SWITCH_FALSE, NULL, NULL));
					switch_file_rename(tmp_file_path, file_path, switch_core_session_get_pool(session));
					sql = switch_mprintf("update voicemail_prefs set name_path='%s' where username='%s' and domain='%s'", file_path, myid, domain_name);
					vm_execute_sql(profile, sql, profile->mutex);
					switch_safe_free(file_path);
					switch_safe_free(tmp_file_path);
					switch_safe_free(sql);

					switch_event_create_subclass(&params, SWITCH_EVENT_CUSTOM, VM_EVENT_MAINT);
					switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "VM-Action", "record-name");
					switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "VM-Name-Path", file_path);
					switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "VM-User", myid);
					switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "VM-Domain", domain_name);
					switch_channel_event_set_data(channel, params);
					switch_event_fire(&params);
				}
				continue;
			}
			break;
		case VM_CHECK_MENU:
			{
				char input[10] = "";
				char key_buf[80] = "";
				play_msg_type = MSG_NONE;

				if (!retries) {
					goto end;
				}

				retries--;

				if (!zstr_buf(global_buf)) {
					switch_set_string(input, global_buf);
					*global_buf = '\0';
					status = SWITCH_STATUS_SUCCESS;
				} else {
					switch_snprintf(key_buf, sizeof(key_buf), "%s:%s:%s:%s",
									profile->play_new_messages_key, profile->play_saved_messages_key, profile->config_menu_key, profile->terminator_key);

					status = vm_macro_get(session, VM_MENU_MACRO, key_buf, input, sizeof(input), 1, "", &term, timeout);
				}

				if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_BREAK) {
					goto end;
				}

				if (!strcmp(input, profile->play_new_messages_key)) {
					play_msg_type = MSG_NEW;
				} else if (!strcmp(input, profile->play_saved_messages_key)) {
					play_msg_type = MSG_SAVED;
				} else if (!strcmp(input, profile->terminator_key)) {
					goto end;
				} else if (!strcmp(input, profile->config_menu_key)) {
					vm_check_state = VM_CHECK_CONFIG;
				}

				if (play_msg_type) {
					vm_check_state = VM_CHECK_PLAY_MESSAGES;
					retries = profile->max_retries;
				}

				continue;
			}
			break;
		case VM_CHECK_AUTH:
			{
				prefs_callback_t cbt = { {0}
				};
				char sql[512] = "";

				if (!attempts) {
					failed = 1;
					goto end;
				}

				attempts--;

				if (!myid) {
					status = vm_macro_get(session, VM_ENTER_ID_MACRO, profile->terminator_key, id_buf, sizeof(id_buf), 0,
										  profile->terminator_key, &term, timeout);
					if (status != SWITCH_STATUS_SUCCESS) {
						goto end;
					}

					if (*id_buf == '\0') {
						continue;
					} else {
						myid = id_buf;
					}
				}

				if (!x_user) {
					switch_event_t *params;
					int ok = 1;

					switch_event_create(&params, SWITCH_EVENT_GENERAL);
					switch_assert(params);
					switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "action", "voicemail-lookup");
					switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "destination_number", caller_profile->destination_number);
					switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "caller_id_number", caller_id_number);

					if (switch_xml_locate_user_merged("id:number-alias", myid, domain_name, switch_channel_get_variable(channel, "network_addr"),
											   &x_user, params) != SWITCH_STATUS_SUCCESS) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Can't find user [%s@%s]\n", myid, domain_name);
						ok = 0;
					} else {
						switch_bool_t vm_enabled = SWITCH_TRUE;

						x_params = switch_xml_child(x_user, "params");

						for (x_param = switch_xml_child(x_params, "param"); x_param; x_param = x_param->next) {
							const char *var = switch_xml_attr_soft(x_param, "name");
							const char *val = switch_xml_attr_soft(x_param, "value");

							if (zstr(var) || zstr(val)) {
								continue; /* Ignore empty entires */
							}

							if (!strcasecmp(var, "vm-enabled")) {
								vm_enabled = !switch_false(val);
							}
						}

						if (!vm_enabled) {
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "User [%s@%s] have voicemail disabled\n", myid, domain_name);
							ok = 0;
						}
						myid = switch_core_session_strdup(session, switch_xml_attr(x_user, "id"));
					}

					switch_event_destroy(&params);

					if (!ok) {
						goto end;
					}
				}

				thepass = thehash = NULL;
				switch_snprintfv(sql, sizeof(sql), "select * from voicemail_prefs where username='%q' and domain='%q'", myid, domain_name);
				vm_execute_sql_callback(profile, profile->mutex, sql, prefs_callback, &cbt);

				x_params = switch_xml_child(x_user, "variables");
				for (x_param = switch_xml_child(x_params, "variable"); x_param; x_param = x_param->next) {
					const char *var = switch_xml_attr_soft(x_param, "name");
					const char *val = switch_xml_attr_soft(x_param, "value");

					if (!strcasecmp(var, "timezone")) {
						switch_channel_set_variable(channel, var, val);
					}
				}

				x_params = switch_xml_child(x_user, "params");
				for (x_param = switch_xml_child(x_params, "param"); x_param; x_param = x_param->next) {
					const char *var = switch_xml_attr_soft(x_param, "name");
					const char *val = switch_xml_attr_soft(x_param, "value");

					if (!strcasecmp(var, "a1-hash")) {
						thehash = switch_core_session_strdup(session, val);
					} else if (!strcasecmp(var, "vm-a1-hash")) {
						vmhash = switch_core_session_strdup(session, val);
					} else if (!auth && !thepass && !strcasecmp(var, "password")) {
						thepass = switch_core_session_strdup(session, val);
					} else if (!auth && !strcasecmp(var, "vm-password")) {
						if (!zstr(val) && !strcasecmp(val, "user-choose")) {
							if (zstr(cbt.password)) {
								if (profile->allow_empty_password_auth) {
									auth = 1;
								}
							} else {
								thepass = switch_core_session_strdup(session, val);
							}
						} else {
							thepass = switch_core_session_strdup(session, val);
						}
					} else if (!strcasecmp(var, "vm-mailto")) {
						vm_email = switch_core_session_strdup(session, val);
						email_addr = switch_core_session_strdup(session, val);
					} else if (!strcasecmp(var, "email-addr")) {
						email_addr = switch_core_session_strdup(session, val);
					} else if (!strcasecmp(var, "vm-convert-cmd")) {
						convert_cmd = switch_core_session_strdup(session, val);
					} else if (!strcasecmp(var, "vm-convert-ext")) {
						convert_ext = switch_core_session_strdup(session, val);
					} else if (!strcasecmp(var, "vm-storage-dir")) {
						vm_storage_dir = switch_core_session_strdup(session, val);
					} else if (!strcasecmp(var, "vm-domain-storage-dir")) {
						storage_dir = switch_core_session_strdup(session, val);
					} else if (!strcasecmp(var, "storage-dir")) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING,
										  "Using deprecated 'storage-dir' directory variable: Please use 'vm-domain-storage-dir'.\n");
						storage_dir = switch_core_session_strdup(session, val);
					} else if (!strcasecmp(var, "timezone")) {
						switch_channel_set_variable(channel, var, val);
					}

				}

				if (!mypass) {
					if (auth) {
						mypass = "OK";
					} else {
						status = vm_macro_get(session, VM_ENTER_PASS_MACRO, profile->terminator_key,
											  pass_buf, sizeof(pass_buf), 0, profile->terminator_key, &term, timeout);
						if (status != SWITCH_STATUS_SUCCESS) {
							goto end;
						}
						if (*pass_buf == '\0') {
							continue;
						} else {
							mypass = pass_buf;
						}
					}
				}

				if (vmhash) {
					thehash = vmhash;
				}

				if (!auth && !thepass && !zstr(cbt.password)) {
					thepass = cbt.password;
				}

				if (!auth) {
					if (!zstr(cbt.password) && !strcmp(cbt.password, mypass)) {
						auth++;
					} else if (!thepass && profile->allow_empty_password_auth) {
						auth++;
					}

					if (!auth && (!profile->db_password_override || (profile->db_password_override && zstr(cbt.password))) && (thepass || thehash) && mypass) {
						if (thehash) {
							char digest[SWITCH_MD5_DIGEST_STRING_SIZE] = { 0 };
							char *lpbuf = switch_mprintf("%s:%s:%s", myid, domain_name, mypass);
							switch_md5_string(digest, (void *) lpbuf, strlen(lpbuf));
							if (!strcmp(digest, thehash)) {
								auth++;
							}
							switch_safe_free(lpbuf);
						}

						if (!auth && thepass && !strcmp(thepass, mypass)) {
							auth++;
						}
					}
				}

				switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, VM_EVENT_MAINT);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "VM-Action", "authentication");
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "VM-Auth-Result", auth ? "success" : "fail");
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "VM-User", myid);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "VM-Domain", domain_name);
				switch_channel_event_set_data(channel, event);
				switch_event_fire(&event);

				xml_safe_free(x_user);

				if (auth) {
					if (!dir_path) {
						if (!zstr(vm_storage_dir)) {
							/* check for absolute or relative path */
							if (switch_is_file_path(vm_storage_dir)) {
								dir_path = switch_core_session_strdup(session, vm_storage_dir);
							} else {
								dir_path = switch_core_session_sprintf(session, "%s%svoicemail%s%s", SWITCH_GLOBAL_dirs.storage_dir,
																	   SWITCH_PATH_SEPARATOR, SWITCH_PATH_SEPARATOR, vm_storage_dir);
							}
						} else if (!zstr(storage_dir)) {
							dir_path = switch_core_session_sprintf(session, "%s%s%s", storage_dir, SWITCH_PATH_SEPARATOR, myid);
						} else if (!zstr(profile->storage_dir)) {
							if (profile->storage_dir_shared) {
								dir_path =
									switch_core_session_sprintf(session, "%s%s%s%s%s%s%s", profile->storage_dir, SWITCH_PATH_SEPARATOR, domain_name,
																SWITCH_PATH_SEPARATOR, "voicemail",
																SWITCH_PATH_SEPARATOR, myid);
							} else {
								dir_path =
									switch_core_session_sprintf(session, "%s%s%s%s%s", profile->storage_dir, SWITCH_PATH_SEPARATOR, domain_name,
																SWITCH_PATH_SEPARATOR, myid);
							}
						} else {
							dir_path = switch_core_session_sprintf(session, "%s%svoicemail%s%s%s%s%s%s", SWITCH_GLOBAL_dirs.storage_dir,
																   SWITCH_PATH_SEPARATOR,
																   SWITCH_PATH_SEPARATOR,
																   profile->name, SWITCH_PATH_SEPARATOR, domain_name, SWITCH_PATH_SEPARATOR, myid);
						}

						if (switch_dir_make_recursive(dir_path, SWITCH_DEFAULT_DIR_PERMS, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error creating %s\n", dir_path);
							goto end;
						}
					}

					authed = 1;

					if (auth_only) goto end;

					vm_check_state = VM_CHECK_FOLDER_SUMMARY;
				} else {
					goto failed;
				}

				continue;

			  failed:

				xml_safe_free(x_user);

				switch_ivr_phrase_macro(session, VM_FAIL_AUTH_MACRO, NULL, NULL, NULL);
				myid = id;
				mypass = NULL;
				continue;
			}
			break;
		default:
			break;
		}
	}

  end:

	switch_safe_free(file_path);

	if (tmp_file_path) {
		unlink(tmp_file_path);
		free(tmp_file_path);
		tmp_file_path = NULL;
	}

	if (switch_channel_ready(channel) && (!auth_only || !authed)) {
		if (failed) {
			switch_ivr_phrase_macro(session, VM_ABORT_MACRO, NULL, NULL, NULL);
		}
		switch_ivr_phrase_macro(session, VM_GOODBYE_MACRO, NULL, NULL, NULL);
	}

	if (auth_only) {
		if (authed) {
			switch_channel_set_variable(channel, "user_pin_authenticated", "true");
			switch_channel_set_variable(channel, "user_pin_authenticated_user", myid);
			if (!zstr(myid) && !zstr(domain_name)) {
				char *account = switch_core_session_sprintf(session, "%s@%s", myid, domain_name);
				switch_ivr_set_user(session, account);
			}
		} else {
			switch_channel_hangup(channel, SWITCH_CAUSE_USER_CHALLENGE);
		}
	}

	xml_safe_free(x_user);
}


static switch_status_t deliver_vm(vm_profile_t *profile,
								  switch_xml_t x_user,
								  const char *domain_name,
								  const char *path,
								  uint32_t message_len,
								  const char *read_flags,
								  switch_event_t *params,
								  switch_memory_pool_t *pool,
								  const char *caller_id_name, 
								  const char *caller_id_number, 
								  const char *forwarded_by, 
								  switch_bool_t copy, const char *use_uuid, switch_core_session_t *session)
{
	char *file_path = NULL, *dir_path = NULL;
	const char *myid = switch_xml_attr(x_user, "id");
	switch_uuid_t uuid;
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	const char *filename;
	switch_xml_t x_param, x_params;
	const char *vm_cc = NULL, *vm_cc_tmp = NULL;
	char *vm_email = NULL;
	char *vm_email_from = NULL;
	char *vm_notify_email = NULL;
	char *vm_timezone = NULL;
	int send_mail = 0;
	int send_main = 0;
	int send_notify = 0;
	int insert_db = 1;
	int email_attach = 0;
	char *vm_storage_dir = NULL;
	char *storage_dir = NULL;
	char *myfolder = "inbox";
	int priority = 3;
	const char *tmp;
	switch_event_t *local_event = NULL;
	switch_status_t ret = SWITCH_STATUS_SUCCESS;
	char *convert_cmd = profile->convert_cmd;
	char *convert_ext = profile->convert_ext;
	
	if (!params) {
		switch_event_create(&local_event, SWITCH_EVENT_REQUEST_PARAMS);
		params = local_event;
	}

	if ((tmp = switch_event_get_header(params, "effective_caller_id_name"))) {
		caller_id_name = tmp;
	}

	if ((tmp = switch_event_get_header(params, "effective_caller_id_number"))) {
		caller_id_number = tmp;
	}

	if (!use_uuid) {
		switch_uuid_get(&uuid);
		switch_uuid_format(uuid_str, &uuid);
		use_uuid = uuid_str;
	}

	if ((filename = strrchr(path, *SWITCH_PATH_SEPARATOR))) {
		filename++;
	} else {
		filename = path;
	}

	x_params = switch_xml_child(x_user, "variables");
	for (x_param = switch_xml_child(x_params, "variable"); x_param; x_param = x_param->next) {
		const char *var = switch_xml_attr_soft(x_param, "name");
		const char *val = switch_xml_attr_soft(x_param, "value");

		if (!strcasecmp(var, "timezone")) {
			vm_timezone = switch_core_strdup(pool, val);
		}
	}

	x_params = switch_xml_child(x_user, "params");

	for (x_param = switch_xml_child(x_params, "param"); x_param; x_param = x_param->next) {
		const char *var = switch_xml_attr_soft(x_param, "name");
		const char *val = switch_xml_attr_soft(x_param, "value");

		if (!strcasecmp(var, "vm-cc")) {
			vm_cc = switch_core_strdup(pool, val);
		} else if (!strcasecmp(var, "vm-mailto")) {
			vm_email = switch_core_strdup(pool, val);
		} else if (!strcasecmp(var, "vm-notify-mailto")) {
			vm_notify_email = switch_core_strdup(pool, val);
		} else if (!strcasecmp(var, "vm-mailfrom")) {
			vm_email_from = switch_core_strdup(pool, val);
		} else if (!strcasecmp(var, "vm-email-all-messages") && (send_main = switch_true(val))) {
			send_mail++;
		} else if (!strcasecmp(var, "vm-storage-dir")) {
			vm_storage_dir = switch_core_strdup(pool, val);
		} else if (!strcasecmp(var, "vm-domain-storage-dir")) {
			storage_dir = switch_core_strdup(pool, val);
		} else if (!strcasecmp(var, "storage-dir")) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
							  "Using deprecated 'storage-dir' directory variable: Please use 'vm-domain-storage-dir'.\n");
			storage_dir = switch_core_strdup(pool, val);
		} else if (!strcasecmp(var, "vm-notify-email-all-messages") && (send_notify = switch_true(val))) {
			send_mail++;
		} else if (!strcasecmp(var, "vm-keep-local-after-email")) {
			insert_db = switch_true(val);
		} else if (!strcasecmp(var, "vm-attach-file")) {
			email_attach = switch_true(val);
		} else if (!strcasecmp(var, "vm-convert-cmd")) {
			convert_cmd = switch_core_strdup(pool, val);
		} else if (!strcasecmp(var, "vm-convert-ext")) {
			convert_ext = switch_core_strdup(pool, val);
		} else if (!strcasecmp(var, "timezone")) {
			vm_timezone = switch_core_strdup(pool, val);
		}
		/*switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Send mail is %d, var is %s\n", send_mail, var); */
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Deliver VM to %s@%s\n", myid, domain_name);

	if (!zstr(vm_storage_dir)) {
		/* check for absolute or relative path */
		if (switch_is_file_path(vm_storage_dir)) {
			dir_path = strdup(vm_storage_dir);
		} else {
			dir_path = switch_mprintf("%s%svoicemail%s%s", SWITCH_GLOBAL_dirs.storage_dir,
												   SWITCH_PATH_SEPARATOR, SWITCH_PATH_SEPARATOR, vm_storage_dir);
		}
	} else if (!zstr(storage_dir)) {
		dir_path = switch_mprintf("%s%s%s", storage_dir, SWITCH_PATH_SEPARATOR, myid);
	} else if (!zstr(profile->storage_dir)) {
		if (profile->storage_dir_shared) {
			dir_path = switch_mprintf("%s%s%s%s%s%s%s", profile->storage_dir, SWITCH_PATH_SEPARATOR, domain_name, SWITCH_PATH_SEPARATOR, "voicemail", SWITCH_PATH_SEPARATOR, myid);
		} else {
			dir_path = switch_mprintf("%s%s%s%s%s", profile->storage_dir, SWITCH_PATH_SEPARATOR, domain_name, SWITCH_PATH_SEPARATOR, myid);
		}
	} else {
		dir_path = switch_mprintf("%s%svoicemail%s%s%s%s%s%s", SWITCH_GLOBAL_dirs.storage_dir,
								  SWITCH_PATH_SEPARATOR,
								  SWITCH_PATH_SEPARATOR, profile->name, SWITCH_PATH_SEPARATOR, domain_name, SWITCH_PATH_SEPARATOR, myid);
	}

	if (switch_dir_make_recursive(dir_path, SWITCH_DEFAULT_DIR_PERMS, pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error creating %s\n", dir_path);
		ret = SWITCH_STATUS_FALSE;
		goto failed;
	}

	if (copy) {
		file_path = switch_mprintf("%s%smsg_%s_broadcast_%s", dir_path, SWITCH_PATH_SEPARATOR, use_uuid, filename);

		if (strlen(file_path) >= 250 /* Max size of the SQL field */) {
			char *ext;
			switch_safe_free(file_path);

			if (!(ext = strrchr(filename, '.'))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Filename doesn't include a file format %s\n", filename);
				ret = SWITCH_STATUS_FALSE;
				goto failed;
			}

			ext++;

			file_path = switch_mprintf("%s%smsg_%s_broadcast_%" SWITCH_TIME_T_FMT ".%s", dir_path, SWITCH_PATH_SEPARATOR, use_uuid, switch_micro_time_now(), ext);
		}

		switch_file_copy(path, file_path, SWITCH_FPROT_FILE_SOURCE_PERMS, pool);
	} else {
		file_path = (char *) path;
	}

	if (!message_len) {
		size_t len = 0;
		if (measure_file_len(file_path, &len) == SWITCH_STATUS_SUCCESS) {
			message_len = (uint32_t)len;
		}
	}

	if (insert_db && switch_file_exists(file_path, pool) == SWITCH_STATUS_SUCCESS) {
		char *usql;
		switch_event_t *message_event;

		switch_event_create_subclass(&message_event, SWITCH_EVENT_CUSTOM, VM_EVENT_MAINT);
		switch_event_add_header_string(message_event, SWITCH_STACK_BOTTOM, "VM-Action", "leave-message");
		switch_event_add_header_string(message_event, SWITCH_STACK_BOTTOM, "VM-User", myid);
		switch_event_add_header_string(message_event, SWITCH_STACK_BOTTOM, "VM-Domain", domain_name);
		switch_event_add_header_string(message_event, SWITCH_STACK_BOTTOM, "VM-Caller-ID-Name", caller_id_name);
		switch_event_add_header_string(message_event, SWITCH_STACK_BOTTOM, "VM-Caller-ID-Number", caller_id_number);
		switch_event_add_header_string(message_event, SWITCH_STACK_BOTTOM, "VM-File-Path", file_path);
		switch_event_add_header_string(message_event, SWITCH_STACK_BOTTOM, "VM-Flags", read_flags);
		switch_event_add_header_string(message_event, SWITCH_STACK_BOTTOM, "VM-Folder", myfolder);
		switch_event_add_header_string(message_event, SWITCH_STACK_BOTTOM, "VM-UUID", use_uuid);
		switch_event_add_header(message_event, SWITCH_STACK_BOTTOM, "VM-Message-Len", "%u", message_len);
		switch_event_add_header(message_event, SWITCH_STACK_BOTTOM, "VM-Timestamp", "%lu", (unsigned long) switch_epoch_time_now(NULL));

		switch_event_fire(&message_event);

		usql = switch_mprintf("insert into voicemail_msgs(created_epoch, read_epoch, username, domain, uuid, cid_name, "
							  "cid_number, in_folder, file_path, message_len, flags, read_flags, forwarded_by) "
							  "values(%ld,0,'%q','%q','%q','%q','%q','%q','%q','%u','','%q','%q')", (long) switch_epoch_time_now(NULL),
							  myid, domain_name, use_uuid, caller_id_name, caller_id_number,
							  myfolder, file_path, message_len, read_flags, switch_str_nil(forwarded_by));

		vm_execute_sql(profile, usql, profile->mutex);
		switch_safe_free(usql);

		update_mwi(profile, myid, domain_name, myfolder, MWI_REASON_NEW);
	}

	if (send_mail && (!zstr(vm_email) || !zstr(vm_notify_email)) && switch_file_exists(file_path, pool) == SWITCH_STATUS_SUCCESS) {
		switch_event_t *event;
		char *from;
		char *body;
		char *headers;
		char *header_string;
		char tmpvar[50] = "";
		int total_new_messages = 0;
		int total_saved_messages = 0;
		int total_new_urgent_messages = 0;
		int total_saved_urgent_messages = 0;
		char *p;
		switch_time_t l_duration = 0;
		switch_core_time_duration_t duration;
		char duration_str[80];
		switch_time_exp_t tm;
		char date[80] = "";
		switch_size_t retsize;
		char *formatted_cid_num = NULL;

		message_count(profile, myid, domain_name, myfolder, &total_new_messages, &total_saved_messages,
					  &total_new_urgent_messages, &total_saved_urgent_messages);

		if (zstr(vm_timezone) || (switch_strftime_tz(vm_timezone, profile->date_fmt, date, sizeof(date), 0) != SWITCH_STATUS_SUCCESS)) {
			switch_time_exp_lt(&tm, switch_micro_time_now());
			switch_strftime(date, &retsize, sizeof(date), profile->date_fmt, &tm);
		}

		formatted_cid_num = switch_format_number(caller_id_number);
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "voicemail_current_folder", myfolder);
		switch_snprintf(tmpvar, sizeof(tmpvar), "%d", total_new_messages);
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "voicemail_total_new_messages", tmpvar);
		switch_snprintf(tmpvar, sizeof(tmpvar), "%d", total_saved_messages);
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "voicemail_total_saved_messages", tmpvar);
		switch_snprintf(tmpvar, sizeof(tmpvar), "%d", total_new_urgent_messages);
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "voicemail_urgent_new_messages", tmpvar);
		switch_snprintf(tmpvar, sizeof(tmpvar), "%d", total_saved_urgent_messages);
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "voicemail_urgent_saved_messages", tmpvar);
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "voicemail_account", myid);
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "voicemail_domain", domain_name);
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "voicemail_caller_id_number", caller_id_number);
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "voicemail_formatted_caller_id_number", formatted_cid_num);		
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "voicemail_caller_id_name", caller_id_name);
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "voicemail_file_path", file_path);
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "voicemail_read_flags", read_flags);
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "voicemail_time", date);
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "voicemail_uuid", use_uuid);

		switch_safe_free(formatted_cid_num);

		switch_snprintf(tmpvar, sizeof(tmpvar), "%d", priority);
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "voicemail_priority", tmpvar);
		if (vm_email) {
			switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "voicemail_email", vm_email);
		}
		if (vm_email_from) {
			switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "voicemail_email_from", vm_email_from);
		}
		if (vm_notify_email) {
			switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "voicemail_notify_email", vm_notify_email);
		}
		l_duration = switch_time_make(message_len, 0);
		switch_core_measure_time(l_duration, &duration);
		duration.day += duration.yr * 365;
		duration.hr += duration.day * 24;
		switch_snprintf(duration_str, sizeof(duration_str), "%.2u:%.2u:%.2u", duration.hr, duration.min, duration.sec);

		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "voicemail_message_len", duration_str);

		if (!zstr(vm_email_from)) {
			from = switch_core_strdup(pool, vm_email_from);
		} else if (zstr(profile->email_from)) {
			from = switch_core_sprintf(pool, "%s@%s", myid, domain_name);
		} else {
			from = switch_event_expand_headers(params, profile->email_from);
		}


		if (send_main) {
			if (zstr(profile->email_headers)) {
				headers = switch_mprintf("From: FreeSWITCH mod_voicemail <%s@%s>\n"
										 "Subject: Voicemail from %s %s\nX-Priority: %d", myid, domain_name, caller_id_name, caller_id_number, priority);
			} else {
				headers = switch_event_expand_headers(params, profile->email_headers);
			}

			p = headers + (strlen(headers) - 1);

			if (*p == '\n') {
				if (*(p - 1) == '\r') {
					p--;
				}
				*p = '\0';
			}

			header_string = switch_core_sprintf(pool, "%s\nX-Voicemail-Length: %u", headers, message_len);

			switch_event_dup(&event, params);

			if (event) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Message-Type", "voicemail");
				switch_event_fire(&event);
			}

			if (profile->email_body) {
				body = switch_event_expand_headers(params, profile->email_body);
			} else {
				body = switch_mprintf("%u second Voicemail from %s %s", message_len, caller_id_name, caller_id_number);
			}

			if (email_attach) {
				switch_simple_email(vm_email, from, header_string, body, file_path, convert_cmd, convert_ext);
			} else {
				switch_simple_email(vm_email, from, header_string, body, NULL, NULL, NULL);
			}

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Sending message to %s\n", vm_email);

			if (body != profile->email_body) {
				switch_safe_free(body);
			}

			if (headers != profile->email_headers) {
				switch_safe_free(headers);
			}
		}


		if (send_notify) {
			if (zstr(vm_notify_email)) {
				vm_notify_email = vm_email;
			}
	
			if (zstr(profile->notify_email_headers)) {
				headers = switch_mprintf("From: FreeSWITCH mod_voicemail <%s@%s>\n"
										 "Subject: Voicemail from %s %s\nX-Priority: %d", myid, domain_name, caller_id_name, caller_id_number, priority);
			} else {
				headers = switch_event_expand_headers(params, profile->notify_email_headers);
			}

			p = headers + (strlen(headers) - 1);

			if (*p == '\n') {
				if (*(p - 1) == '\r') {
					p--;
				}
				*p = '\0';
			}

			header_string = switch_core_sprintf(pool, "%s\nX-Voicemail-Length: %u", headers, message_len);

			switch_event_dup(&event, params);

			if (event) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Message-Type", "voicemail-notify");
				switch_event_fire(&event);
			}

			if (profile->notify_email_body) {
				body = switch_event_expand_headers(params, profile->notify_email_body);
			} else {
				body = switch_mprintf("%u second Voicemail from %s %s", message_len, caller_id_name, caller_id_number);
			}

			switch_simple_email(vm_notify_email, from, header_string, body, NULL, NULL, NULL);

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Sending notify message to %s\n", vm_notify_email);

			if (body != profile->notify_email_body) {
				switch_safe_free(body);
			}

			if (headers != profile->notify_email_headers) {
				switch_safe_free(headers);
			}
		}
	}

	if (session) {
		switch_channel_t *channel = switch_core_session_get_channel(session);
		if (channel && (vm_cc_tmp = switch_channel_get_variable(channel, "vm_cc"))) {
			vm_cc = vm_cc_tmp;
		}
	}

	if (vm_cc) {
		char *vm_cc_dup;
		int vm_cc_num = 0;
		char *vm_cc_list[256] = { 0 };
		int vm_cc_i;

		vm_cc_dup = strdup(vm_cc);
		vm_cc_num = switch_separate_string(vm_cc_dup, ',', vm_cc_list, (sizeof(vm_cc_list) / sizeof(vm_cc_list[0])));

		for (vm_cc_i=0; vm_cc_i<vm_cc_num; vm_cc_i++) {
			char *cmd, *val;
			const char *vm_cc_current = vm_cc_list[vm_cc_i];

			val = strdup(caller_id_name);
			switch_url_decode(val);

			cmd = switch_mprintf("%s %s %s '%s' %s@%s %s",
								 vm_cc_current, file_path, caller_id_number,
								 val, myid, domain_name, read_flags);

			free(val);

			if (voicemail_inject(cmd, session) == SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Sent Carbon Copy to %s\n", vm_cc_current);
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failed to Carbon Copy to %s\n", vm_cc_current);
			}
			switch_safe_free(cmd);
		}

		switch_safe_free(vm_cc_dup);
	}


  failed:

	if (!insert_db && file_path && switch_file_exists(file_path, pool) == SWITCH_STATUS_SUCCESS) {
		if (unlink(file_path) != 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Failed to delete file [%s]\n", file_path);
		}
	}

	switch_event_destroy(&local_event);

	switch_safe_free(dir_path);

	if (file_path != path) {
		switch_safe_free(file_path);
	}

	return ret;

}

static switch_status_t voicemail_inject(const char *data, switch_core_session_t *session)
{
	vm_profile_t *profile;
	char *dup = NULL, *user = NULL, *domain = NULL, *profile_name = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	int isgroup = 0, isall = 0;
	int argc = 0;
	char *argv[6] = { 0 };
	char *box, *path, *cid_num, *cid_name;
	switch_memory_pool_t *pool = NULL;
	char *forwarded_by = NULL;
	char *read_flags = NORMAL_FLAG_STRING;
	char *dup_domain = NULL;
	
	if (zstr(data)) {
		status = SWITCH_STATUS_FALSE;
		goto end;
	}

	dup = strdup(data);
	switch_assert(dup);

	if ((argc = switch_separate_string(dup, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) < 2) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Not enough args [%s]\n", data);
		status = SWITCH_STATUS_FALSE;
		goto end;
	}

	box = argv[0];
	path = argv[1];
	cid_num = argv[2] ? argv[2] : "anonymous";
	cid_name = argv[3] ? argv[3] : "anonymous";
	forwarded_by = argv[4];
	if (!zstr(argv[5])) {
		read_flags = argv[5];
	}

	user = box;

	if ((domain = strchr(user, '@'))) {
		*domain++ = '\0';

		if ((profile_name = strchr(domain, '@'))) {
			*profile_name++ = '\0';
		} else {
			profile_name = domain;
		}
	}

	if (switch_stristr("group=", user)) {
		user += 6;
		isgroup++;
	} else if (switch_stristr("domain=", user)) {
		user += 7;
		domain = user;
		profile_name = domain;
		isall++;
	}

	if (zstr(domain)) {
		if ((dup_domain = switch_core_get_domain(SWITCH_TRUE))) {
			domain = dup_domain;
		}
		profile_name = domain;
	}

	if (!(user && domain)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid syntax [%s][%s]\n", switch_str_nil(user), switch_str_nil(domain));
		status = SWITCH_STATUS_FALSE;
		goto end;
	}

	if (!(profile = get_profile(profile_name))) {
		if (!(profile = get_profile(domain))) {
			profile = get_profile("default");
		}
	}

	if (!profile) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't find profile\n");
		status = SWITCH_STATUS_FALSE;
	} else {
		switch_xml_t x_domain, xml_root;
		switch_event_t *my_params = NULL;
		switch_xml_t ut;

		switch_event_create(&my_params, SWITCH_EVENT_REQUEST_PARAMS);
		switch_assert(my_params);

		if (isgroup) {
			switch_event_add_header_string(my_params, SWITCH_STACK_BOTTOM, "group", user);
		} else {
			if (isall) {
				switch_event_add_header_string(my_params, SWITCH_STACK_BOTTOM, "user", "_all_");
			} else {
				switch_event_add_header_string(my_params, SWITCH_STACK_BOTTOM, "user", user);
			}
		}

		if (forwarded_by) {
			switch_event_add_header_string(my_params, SWITCH_STACK_BOTTOM, "Forwarded-By", forwarded_by);
		}
		switch_event_add_header_string(my_params, SWITCH_STACK_BOTTOM, "domain", domain);
		switch_event_add_header_string(my_params, SWITCH_STACK_BOTTOM, "purpose", "publish-vm");

		if (switch_xml_locate_domain(domain, my_params, &xml_root, &x_domain) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Cannot locate domain %s\n", domain);
			status = SWITCH_STATUS_FALSE;
			switch_event_destroy(&my_params);
			profile_rwunlock(profile);
			goto end;
		}

		switch_event_destroy(&my_params);

		switch_core_new_memory_pool(&pool);


		if (isgroup) {
			switch_xml_t group = NULL, groups = NULL, users = NULL;
			if ((groups = switch_xml_child(x_domain, "groups"))) {
				if ((group = switch_xml_find_child_multi(groups, "group", "name", user, NULL))) {
					if ((users = switch_xml_child(group, "users"))) {
						for (ut = switch_xml_child(users, "user"); ut; ut = ut->next) {
							const char *type = switch_xml_attr_soft(ut, "type");

							if (!strcasecmp(type, "pointer")) {
								const char *uname = switch_xml_attr_soft(ut, "id");
								switch_xml_t ux;

								if (switch_xml_locate_user_in_domain(uname, x_domain, &ux, NULL) == SWITCH_STATUS_SUCCESS) {
									switch_xml_merge_user(ux, x_domain, group);
									switch_event_create(&my_params, SWITCH_EVENT_REQUEST_PARAMS);
									status =
										deliver_vm(profile, ux, domain, path, 0, read_flags, my_params, pool, cid_name, cid_num, forwarded_by,
												   SWITCH_TRUE, session ? switch_core_session_get_uuid(session) : NULL, NULL);
									switch_event_destroy(&my_params);
								}
								continue;
							}

							switch_xml_merge_user(ut, x_domain, group);
							switch_event_create(&my_params, SWITCH_EVENT_REQUEST_PARAMS);
							status = deliver_vm(profile, ut, domain, path, 0, read_flags, 
												my_params, pool, cid_name, cid_num, forwarded_by, SWITCH_TRUE, 
												session ? switch_core_session_get_uuid(session) : NULL, NULL);
							switch_event_destroy(&my_params);
						}
					}
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Cannot locate group %s\n", user);
				}
			}
		} else if (isall) {
			switch_xml_t group = NULL, groups = NULL, users = NULL;
			if ((groups = switch_xml_child(x_domain, "groups"))) {
				for (group = switch_xml_child(groups, "group"); group; group = group->next) {
					if ((users = switch_xml_child(group, "users"))) {
						for (ut = switch_xml_child(users, "user"); ut; ut = ut->next) {
							const char *type = switch_xml_attr_soft(ut, "type");

							if (!strcasecmp(type, "pointer")) {
								continue;
							}

							switch_xml_merge_user(ut, x_domain, group);
							switch_event_create(&my_params, SWITCH_EVENT_REQUEST_PARAMS);
							status = deliver_vm(profile, ut, domain, path, 0, read_flags, 
												my_params, pool, cid_name, cid_num, forwarded_by, SWITCH_TRUE,
												session ? switch_core_session_get_uuid(session) : NULL, NULL);
							switch_event_destroy(&my_params);
						}
					}
				}
			}

		} else {
			switch_xml_t x_group = NULL;

			if ((status = switch_xml_locate_user_in_domain(user, x_domain, &ut, &x_group)) == SWITCH_STATUS_SUCCESS) {
				switch_xml_merge_user(ut, x_domain, x_group);
				switch_event_create(&my_params, SWITCH_EVENT_REQUEST_PARAMS);
				status = deliver_vm(profile, ut, domain, path, 0, read_flags, 
									my_params, pool, cid_name, cid_num, forwarded_by, SWITCH_TRUE,
									session ? switch_core_session_get_uuid(session) : NULL, NULL);
				switch_event_destroy(&my_params);
			} else {
				status = SWITCH_STATUS_FALSE;
			}
		}
		profile_rwunlock(profile);

		switch_core_destroy_memory_pool(&pool);

		switch_xml_free(xml_root);
	}

  end:

	switch_safe_free(dup);
	switch_safe_free(dup_domain);

	return status;
}

static switch_status_t voicemail_leave_main(switch_core_session_t *session, vm_profile_t *profile, const char *domain_name, const char *id)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char sql[256];
	prefs_callback_t cbt;
	switch_uuid_t tmp_uuid;
	char tmp_uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1]; 
	char *file_path = NULL;
	char *dir_path = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_caller_profile_t *caller_profile = switch_channel_get_caller_profile(channel);
	switch_file_handle_t fh = { 0 };
	switch_input_args_t args = { 0 };
	char *vm_email = NULL;
	char *vm_notify_email = NULL;
	cc_t cc = { 0 };
	char *read_flags = NORMAL_FLAG_STRING;
	const char *operator_ext = switch_channel_get_variable(channel, "vm_operator_extension");
	char buf[2];
	char key_buf[80];
	char *greet_path = NULL;
	const char *voicemail_greeting_number = NULL;
	switch_size_t message_len = 0;
	switch_time_exp_t tm;
	char date[80] = "";
	switch_size_t retsize;
	switch_time_t ts = switch_micro_time_now();
	char *vm_storage_dir = NULL;
	char *storage_dir = NULL;
	char *record_macro = VM_RECORD_MESSAGE_MACRO;
	int send_main = 0;
	int send_notify = 0;
	const char *read_id = NULL;
	const char *caller_id_name = NULL;
	const char *caller_id_number = NULL;
	switch_xml_t x_user = NULL, x_params = NULL, x_param = NULL;
	switch_event_t *vars = NULL;
	const char *vtmp, *vm_ext = NULL;
	int disk_quota = 0;
	switch_bool_t skip_greeting = switch_true(switch_channel_get_variable(channel, "skip_greeting"));
	switch_bool_t skip_instructions = switch_true(switch_channel_get_variable(channel, "skip_instructions"));
	switch_bool_t skip_record_urgent_check = switch_true(switch_channel_get_variable(channel, "skip_record_urgent_check"));
	switch_bool_t vm_enabled = SWITCH_TRUE;

	switch_channel_set_variable(channel, "skip_greeting", NULL);
	switch_channel_set_variable(channel, "skip_instructions", NULL);
	switch_channel_set_variable(channel, "skip_record_urgent_check", NULL);

	memset(&cbt, 0, sizeof(cbt));

	if (id) {
		int ok = 1;
		switch_event_t *locate_params = NULL;
		const char *email_addr = NULL;

		switch_event_create(&locate_params, SWITCH_EVENT_REQUEST_PARAMS);
		switch_assert(locate_params);
		switch_event_add_header_string(locate_params, SWITCH_STACK_BOTTOM, "action", "voicemail-lookup");

		if (switch_xml_locate_user_merged("id:number-alias", id, domain_name, switch_channel_get_variable(channel, "network_addr"),
										  &x_user, locate_params) == SWITCH_STATUS_SUCCESS) {
			id = switch_core_session_strdup(session, switch_xml_attr(x_user, "id"));

			if ((x_params = switch_xml_child(x_user, "params"))) {
				for (x_param = switch_xml_child(x_params, "param"); x_param; x_param = x_param->next) {
					const char *var = switch_xml_attr_soft(x_param, "name");
					const char *val = switch_xml_attr_soft(x_param, "value");

					if (!strcasecmp(var, "vm-mailto")) {
						vm_email = switch_core_session_strdup(session, val);
					} else if (!strcasecmp(var, "vm-notify-mailto")) {
						vm_notify_email = switch_core_session_strdup(session, val);
					} else if (!strcasecmp(var, "vm-skip-instructions")) {
						skip_instructions = switch_true(val);
					} else if (!strcasecmp(var, "email-addr")) {
						email_addr = switch_core_session_strdup(session, val);
					} else if (!strcasecmp(var, "vm-storage-dir")) {
						vm_storage_dir = switch_core_session_strdup(session, val);
					} else if (!strcasecmp(var, "vm-domain-storage-dir")) {
						storage_dir = switch_core_session_strdup(session, val);
					} else if (!strcasecmp(var, "storage-dir")) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING,
										  "Using deprecated 'storage-dir' directory variable: Please use 'vm-domain-storage-dir'.\n");
						storage_dir = switch_core_session_strdup(session, val);
					} else if (!strcasecmp(var, "vm-disk-quota")) {
						disk_quota = atoi(val);
					} else if (!strcasecmp(var, "vm-alternate-greet-id")) {
						read_id = switch_core_session_strdup(session, val);
					} else if (!strcasecmp(var, "vm-enabled")) {
						vm_enabled = !switch_false(val);
					} else if (!strcasecmp(var, "vm-message-ext")) {
						vm_ext = switch_core_session_strdup(session, val);
					} else if (!strcasecmp(var, "vm-operator-extension") && (zstr(operator_ext))) {
						operator_ext = switch_core_session_strdup(session, val);
					}
				}
			}

			if (!vm_enabled) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "User [%s@%s] have voicemail disabled\n", id, domain_name);
				ok = 0;
			}

			if (send_main && zstr(vm_email) && !zstr(email_addr)) {
				vm_email = switch_core_session_strdup(session, email_addr);
				if (zstr(vm_email)) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "No email address, not going to send email.\n");
					send_main = 0;
				}
			}

			if (send_notify && zstr(vm_notify_email)) {
				vm_notify_email = vm_email;
				if (zstr(vm_notify_email)) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "No notify email address, not going to notify.\n");
					send_notify = 0;
				}
			}

		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Can't find user [%s@%s]\n", id, domain_name);
			ok = 0;
		}

		switch_event_destroy(&locate_params);

		if (!ok) {
			goto end;
		}
	}

	if (!zstr(vm_storage_dir)) {
		/* check for absolute or relative path */
		if (switch_is_file_path(vm_storage_dir)) {
			dir_path = switch_core_session_strdup(session, vm_storage_dir);
		} else {
			dir_path = switch_core_session_sprintf(session, "%s%svoicemail%s%s", SWITCH_GLOBAL_dirs.storage_dir,
												   SWITCH_PATH_SEPARATOR, SWITCH_PATH_SEPARATOR, vm_storage_dir);
		}
	} else if (!zstr(storage_dir)) {
		dir_path = switch_core_session_sprintf(session, "%s%s%s", storage_dir, SWITCH_PATH_SEPARATOR, id);
	} else if (!zstr(profile->storage_dir)) {
		if (profile->storage_dir_shared) {
			dir_path = switch_core_session_sprintf(session, "%s%s%s%s%s%s%s", profile->storage_dir, SWITCH_PATH_SEPARATOR, domain_name, SWITCH_PATH_SEPARATOR, "voicemail", SWITCH_PATH_SEPARATOR, id);
		} else {
			dir_path = switch_core_session_sprintf(session, "%s%s%s%s%s", profile->storage_dir, SWITCH_PATH_SEPARATOR, domain_name, SWITCH_PATH_SEPARATOR, id);
		}
	} else {
		dir_path = switch_core_session_sprintf(session, "%s%svoicemail%s%s%s%s%s%s", SWITCH_GLOBAL_dirs.storage_dir,
											   SWITCH_PATH_SEPARATOR,
											   SWITCH_PATH_SEPARATOR, profile->name, SWITCH_PATH_SEPARATOR, domain_name, SWITCH_PATH_SEPARATOR, id);
	}

	if (switch_dir_make_recursive(dir_path, SWITCH_DEFAULT_DIR_PERMS, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error creating %s\n", dir_path);
		goto end;
	}

	switch_snprintfv(sql, sizeof(sql), "select * from voicemail_prefs where username='%q' and domain='%q'", id, domain_name);
	vm_execute_sql_callback(profile, profile->mutex, sql, prefs_callback, &cbt);

	if (!vm_ext) {
		vm_ext = profile->file_ext;
	}
	if ((vtmp = switch_channel_get_variable(channel, "vm_message_ext"))) {
		vm_ext = vtmp;
	}

	switch_uuid_get(&tmp_uuid);
	switch_uuid_format(tmp_uuid_str, &tmp_uuid);

	file_path = switch_mprintf("%s%smsg_%s.%s", dir_path, SWITCH_PATH_SEPARATOR, tmp_uuid_str, vm_ext);

	if ((voicemail_greeting_number = switch_channel_get_variable(channel, "voicemail_greeting_number"))) {
		int num = atoi(voicemail_greeting_number);
		if (num > 0 && num <= VM_MAX_GREETINGS) {
			greet_path = switch_mprintf("%s%sgreeting_%d.%s", dir_path, SWITCH_PATH_SEPARATOR, num, profile->file_ext);
		}
	} else if (!(greet_path = (char *) switch_channel_get_variable(channel, "voicemail_greeting_path"))) {
		greet_path = cbt.greeting_path;
	}

  greet:
	if (!skip_greeting) {
		memset(buf, 0, sizeof(buf));
		args.input_callback = cancel_on_dtmf;
		args.buf = buf;
		args.buflen = sizeof(buf);

		switch_ivr_sleep(session, 100, SWITCH_TRUE, NULL);

		if (switch_file_exists(greet_path, switch_core_session_get_pool(session)) == SWITCH_STATUS_SUCCESS) {
			memset(buf, 0, sizeof(buf));
			TRY_CODE(switch_ivr_play_file(session, NULL, greet_path, &args));
		} else {
			if (switch_file_exists(cbt.name_path, switch_core_session_get_pool(session)) == SWITCH_STATUS_SUCCESS) {
				memset(buf, 0, sizeof(buf));
				TRY_CODE(switch_ivr_play_file(session, NULL, cbt.name_path, &args));
			}
			if (*buf == '\0') {
				if (!read_id) {
					if (!(read_id = switch_channel_get_variable(channel, "voicemail_alternate_greet_id"))) {
						read_id = id;
					}
				}
				memset(buf, 0, sizeof(buf));
				TRY_CODE(switch_ivr_phrase_macro(session, VM_PLAY_GREETING_MACRO, read_id, NULL, &args));
			}
		}

		if (*buf != '\0') {
		  greet_key_press:
			if (switch_stristr(buf, profile->login_keys)) {
				voicemail_check_main(session, profile, domain_name, id, 0, NULL);
			} else if ((!zstr(profile->operator_ext) || !zstr(operator_ext)) && !zstr(profile->operator_key) && !strcasecmp(buf, profile->operator_key) ) {
				int argc;
				char *argv[4];
				char *mycmd;
				
				if ((!zstr(operator_ext) && (mycmd = switch_core_session_strdup(session, operator_ext))) ||
				    (!zstr(profile->operator_ext) && (mycmd = switch_core_session_strdup(session, profile->operator_ext)))) {
					argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
					if (argc >= 1 && argc <= 4) {
						switch_ivr_session_transfer(session, argv[0], argv[1], argv[2]);
						/* the application still runs after we leave it so we need to make sure that we don't do anything evil */
						goto end;
					}
				}
			} else if (!strcasecmp(buf, profile->vmain_key) && !zstr(profile->vmain_key)) {
				int argc;
				char *argv[4];
				char *mycmd;

				if (!zstr(profile->vmain_ext) && (mycmd = switch_core_session_strdup(session, profile->vmain_ext))) {
					argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
					if (argc >= 1 && argc <= 4) {
						switch_ivr_session_transfer(session, argv[0], argv[1], argv[2]);
						/* the application still runs after we leave it so we need to make sure that we don't do anything evil */
						goto end;
					}
				}
			} else if (*profile->skip_greet_key && !strcasecmp(buf, profile->skip_greet_key)) {
				skip_instructions = SWITCH_TRUE;
			} else {
				goto greet;
			}
		}
	}

	if (skip_instructions) {
		record_macro = NULL;
	}

	if (disk_quota) {
		callback_t callback = { 0 };
		char sqlstmt[256];
		char disk_usage[256] = "";

		callback.buf = disk_usage;
		callback.len = sizeof(disk_usage);

		switch_snprintfv(sqlstmt, sizeof(sqlstmt), "select sum(message_len) from voicemail_msgs where username='%q' and domain='%q'", id, domain_name);
		vm_execute_sql_callback(profile, profile->mutex, sqlstmt, sql2str_callback, &callback);

		if (atoi(disk_usage) >= disk_quota) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Voicemail disk quota is exceeded for %s\n", id);
			TRY_CODE(switch_ivr_phrase_macro(session, VM_DISK_QUOTA_EXCEEDED_MACRO, NULL, NULL, NULL));
			goto end;
		}
	}


	memset(&fh, 0, sizeof(fh));
	args.input_callback = control_playback;
	memset(&cc, 0, sizeof(cc));
	cc.profile = profile;
	cc.fh = &fh;
	cc.noexit = 1;
	args.buf = &cc;

	if (!(caller_id_name = switch_channel_get_variable(channel, "effective_caller_id_name"))) {
		caller_id_name = caller_profile->caller_id_name;
	}

	if (!(caller_id_number = switch_channel_get_variable(channel, "effective_caller_id_number"))) {
		caller_id_number = caller_profile->caller_id_number;
	}

	switch_channel_set_variable_printf(channel, "RECORD_ARTIST", "%s (%s)", caller_id_name, caller_id_number);

	switch_time_exp_lt(&tm, ts);
	switch_strftime_nocheck(date, &retsize, sizeof(date), "%Y-%m-%d %T", &tm);
	switch_channel_set_variable(channel, "RECORD_DATE", date);
	switch_channel_set_variable(channel, "RECORD_SOFTWARE", "FreeSWITCH");
	switch_channel_set_variable(channel, "RECORD_TITLE", profile->record_title);
	switch_channel_set_variable(channel, "RECORD_COMMENT", profile->record_comment);
	switch_channel_set_variable(channel, "RECORD_COPYRIGHT", profile->record_copyright);

	switch_snprintf(key_buf, sizeof(key_buf), "%s:%s", profile->operator_key, profile->vmain_key);
	memset(buf, 0, sizeof(buf));

	status = create_file(session, profile, record_macro, file_path, &message_len, SWITCH_TRUE, key_buf, buf);

	if (status == SWITCH_STATUS_NOTFOUND) {
		goto end;
	}

	if (buf[0]) {
		goto greet_key_press;
	}

	if ((status == SWITCH_STATUS_SUCCESS || status == SWITCH_STATUS_BREAK) && switch_channel_ready(channel)) {
		char input[10] = "", term = 0;

		switch_snprintf(key_buf, sizeof(key_buf), "%s:%s", profile->urgent_key, profile->terminator_key);
		if (!skip_record_urgent_check) {
			(void) vm_macro_get(session, VM_RECORD_URGENT_CHECK_MACRO, key_buf, input, sizeof(input), 1, "", &term, profile->digit_timeout);
			if (!switch_channel_ready(channel)) goto deliver;
			if (*profile->urgent_key == *input) {
				read_flags = URGENT_FLAG_STRING;
				(void) switch_ivr_phrase_macro(session, VM_ACK_MACRO, "marked-urgent", NULL, NULL);
			} else {
				(void) switch_ivr_phrase_macro(session, VM_ACK_MACRO, "saved", NULL, NULL);
			}
		}
	}

 deliver:
	if (x_user) {
		switch_channel_get_variables(channel, &vars);
		status = deliver_vm(profile, x_user, domain_name, file_path, (uint32_t)message_len, read_flags, vars,
							switch_core_session_get_pool(session), caller_id_name, caller_id_number, NULL, SWITCH_FALSE,
							session ? switch_core_session_get_uuid(session) : NULL, session);
		switch_event_destroy(&vars);
		if (status == SWITCH_STATUS_SUCCESS) {
			switch_core_time_duration_t duration;
			char duration_str[80];
			switch_time_t l_duration = switch_time_make(message_len, 0);

			switch_core_measure_time(l_duration, &duration);
			duration.day += duration.yr * 365;
			duration.hr += duration.day * 24;

			switch_snprintf(duration_str, sizeof(duration_str), "%.2u:%.2u:%.2u", duration.hr, duration.min, duration.sec);

			switch_channel_set_variable(channel, "voicemail_account", id);
			switch_channel_set_variable(channel, "voicemail_domain", domain_name);
			switch_channel_set_variable(channel, "voicemail_file_path", file_path);
			switch_channel_set_variable(channel, "voicemail_read_flags", read_flags);
			switch_channel_set_variable(channel, "voicemail_message_len", duration_str);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failed to deliver message\n");
			TRY_CODE(switch_ivr_phrase_macro(session, VM_ACK_MACRO, "deleted", NULL, NULL));
		}

	}

  end:

	xml_safe_free(x_user);

	switch_safe_free(file_path);

	if (switch_channel_ready(channel) && vm_enabled) {
		status = switch_ivr_phrase_macro(session, VM_GOODBYE_MACRO, NULL, NULL, NULL);
	}

	return status;
}


#define VM_DESC "voicemail"
#define VM_USAGE "[check] [auth] <profile_name> <domain_name> [<id>] [uuid]"

SWITCH_STANDARD_APP(voicemail_function)
{
	char *argv[6] = { 0 };
	char *mydata = NULL;
	vm_profile_t *profile = NULL;
	const char *profile_name = NULL;
	const char *domain_name = NULL;
	const char *id = NULL;
	const char *auth_var = NULL;
	const char *uuid = NULL;
	int x = 0, check = 0, auth = 0;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	if (!zstr(data)) {
		mydata = switch_core_session_strdup(session, data);
		switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	for (;;) {
		if (argv[x] && !strcasecmp(argv[x], "check")) {
			check++;
			x++;
		} else if (argv[x] && !strcasecmp(argv[x], "auth_only")) {
			auth = 2;
			x++;
		} else if (argv[x] && !strcasecmp(argv[x], "auth")) {
			auth++;
			x++;
		} else {
			break;
		}
	}

	if (argv[x]) {
		profile_name = argv[x++];
	}

	if (argv[x]) {
		domain_name = argv[x++];
	}

	if (argv[x]) {
		id = argv[x++];
	}

	if ((auth_var = switch_channel_get_variable(channel, "voicemail_authorized")) && switch_true(auth_var)) {
		auth = 1;
	}

	if (zstr(profile_name)) {
		profile_name = switch_channel_get_variable(channel, "voicemail_profile_name");
	}

	if (zstr(domain_name)) {
		domain_name = switch_channel_get_variable(channel, "voicemail_domain_name");
	}

	if (zstr(id)) {
		id = switch_channel_get_variable(channel, "voicemail_id");
	}

	if (zstr(profile_name) || zstr(domain_name)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error Usage: %s\n", VM_USAGE);
		return;
	}

	if (!(profile = get_profile(profile_name))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error invalid profile %s\n", profile_name);
		return;
	}

	if (check || auth == 2) {
		if (argv[x]) {
			uuid = argv[x++];
		}
		voicemail_check_main(session, profile, domain_name, id, auth, uuid);
	} else {
		voicemail_leave_main(session, profile, domain_name, id);
	}

	profile_rwunlock(profile);

}

#define BOXCOUNT_SYNTAX "[profile/]<user>@<domain>[|[new|saved|new-urgent|saved-urgent|all]]"
SWITCH_STANDARD_API(boxcount_api_function)
{
	char *dup;
	const char *how = "new";
	int total_new_messages = 0;
	int total_saved_messages = 0;
	int total_new_urgent_messages = 0;
	int total_saved_urgent_messages = 0;
	vm_profile_t *profile;
	char *id, *domain, *p, *profilename = NULL;

	if (zstr(cmd)) {
		stream->write_function(stream, "%d", 0);
		return SWITCH_STATUS_SUCCESS;
	}

	id = dup = strdup(cmd);

	if ((p = strchr(dup, '/'))) {
		*p++ = '\0';
		id = p;
		profilename = dup;
	}

	if (!strncasecmp(id, "sip:", 4)) {
		id += 4;
	}

	if (zstr(id)) {
		stream->write_function(stream, "%d", 0);
		goto done;
	}

	if ((domain = strchr(id, '@'))) {
		*domain++ = '\0';
		if ((p = strchr(domain, '|'))) {
			*p++ = '\0';
			how = p;
		}

		if (!zstr(profilename)) {
			if ((profile = get_profile(profilename))) {
				message_count(profile, id, domain, "inbox", &total_new_messages, &total_saved_messages,
							  &total_new_urgent_messages, &total_saved_urgent_messages);
				profile_rwunlock(profile);
			} else {
				stream->write_function(stream, "-ERR No such profile\n");
				goto done;
			}
		} else {
			/* Kept for backwards-compatibility */
			switch_hash_index_t *hi;
			switch_mutex_lock(globals.mutex);
			if ((hi = switch_core_hash_first(globals.profile_hash))) {
				void *val;
				switch_core_hash_this(hi, NULL, NULL, &val);
				profile = (vm_profile_t *) val;
				total_new_messages = total_saved_messages = 0;
				message_count(profile, id, domain, "inbox", &total_new_messages, &total_saved_messages,
							  &total_new_urgent_messages, &total_saved_urgent_messages);
			}
			switch_mutex_unlock(globals.mutex);
		}
	}

	if (!strcasecmp(how, "saved")) {
		stream->write_function(stream, "%d", total_saved_messages);
	} else if (!strcasecmp(how, "new-urgent")) {
		stream->write_function(stream, "%d", total_new_urgent_messages);
	} else if (!strcasecmp(how, "new-saved")) {
		stream->write_function(stream, "%d", total_saved_urgent_messages);
	} else if (!strcasecmp(how, "all")) {
		stream->write_function(stream, "%d:%d:%d:%d", total_new_messages, total_saved_messages, total_new_urgent_messages, total_saved_urgent_messages);
	} else {
		stream->write_function(stream, "%d", total_new_messages);
	}

  done:
	switch_safe_free(dup);
	return SWITCH_STATUS_SUCCESS;
}

#define PREFS_SYNTAX "[profile/]<user>@<domain>[|[name_path|greeting_path|password]]"
SWITCH_STANDARD_API(prefs_api_function)
{
	char *dup = NULL;
	const char *how = "greeting_path";
	vm_profile_t *profile = NULL;
	char *id, *domain, *p, *profilename = NULL;
	char sql[256];
	prefs_callback_t cbt = { {0} };

	if (zstr(cmd)) {
		stream->write_function(stream, "%d", 0);
		goto done;
	}

	id = dup = strdup(cmd);

	if ((p = strchr(dup, '/'))) {
		*p++ = '\0';
		id = p;
		profilename = dup;
	}

	if (!strncasecmp(id, "sip:", 4)) {
		id += 4;
	}

	if (zstr(id)) {
		stream->write_function(stream, "%d", 0);
		goto done;
	}

	if ((domain = strchr(id, '@'))) {
		*domain++ = '\0';
		if ((p = strchr(domain, '|'))) {
			*p++ = '\0';
			how = p;
		}

		if (!zstr(profilename) && !(profile = get_profile(profilename))) {
			stream->write_function(stream, "-ERR No such profile\n");
			goto done;
		}
		if (!profile && !(profile = get_profile("default"))) {
			stream->write_function(stream, "-ERR profile 'default' doesn't exist\n");
			goto done;
		}
	} else {
		stream->write_function(stream, "-ERR No domain specified\n");
		goto done;

	}

	switch_snprintfv(sql, sizeof(sql), "select * from voicemail_prefs where username='%q' and domain='%q'", id, domain);
	vm_execute_sql_callback(profile, profile->mutex, sql, prefs_callback, &cbt);

	if (!strcasecmp(how, "greeting_path")) {
		stream->write_function(stream, "%s", cbt.greeting_path);
	} else if (!strcasecmp(how, "name_path")) {
		stream->write_function(stream, "%s", cbt.name_path);
	} else if (!strcasecmp(how, "password")) {
		stream->write_function(stream, "%s", cbt.password);
	} else {
		stream->write_function(stream, "%s:%s:%s", cbt.greeting_path, cbt.name_path, cbt.password);
	}
	profile_rwunlock(profile);
  done:
	switch_safe_free(dup);
	return SWITCH_STATUS_SUCCESS;
}

#define parse_profile() {\
		total_new_messages = total_saved_messages = 0;					\
		message_count(profile, id, domain, "inbox", &total_new_messages, &total_saved_messages,	\
					  &total_new_urgent_messages, &total_saved_urgent_messages); \
		if (total_new_messages || total_saved_messages) {				\
			if (switch_event_create(&new_event, SWITCH_EVENT_MESSAGE_WAITING) == SWITCH_STATUS_SUCCESS) { \
				const char *yn = "no";									\
				if (total_new_messages || total_new_urgent_messages) {	\
					yn = "yes";											\
				}														\
				switch_event_add_header_string(new_event, SWITCH_STACK_BOTTOM, "MWI-Messages-Waiting", yn);	\
				switch_event_add_header_string(new_event, SWITCH_STACK_BOTTOM, "MWI-Message-Account", account); \
				switch_event_add_header(new_event, SWITCH_STACK_BOTTOM, "MWI-Voice-Message", "%d/%d (%d/%d)", \
										+total_new_messages, total_saved_messages, total_new_urgent_messages, total_saved_urgent_messages); \
			}															\
		}																\
	}


static void actual_message_query_handler(switch_event_t *event)
{
	char *account = switch_event_get_header(event, "message-account");
	switch_event_t *new_event = NULL;
	char *dup = NULL;
	int total_new_messages = 0;
	int total_saved_messages = 0;
	int total_new_urgent_messages = 0;
	int total_saved_urgent_messages = 0;

	if (account) {
		switch_hash_index_t *hi;
		void *val;
		vm_profile_t *profile;
		char *id, *domain;

		dup = strdup(account);

		switch_split_user_domain(dup, &id, &domain);

		if (!id || !domain) {
			free(dup);
			return;
		}

		profile = NULL;

		if (globals.message_query_exact_match) {
			if ((profile = (vm_profile_t *) switch_core_hash_find(globals.profile_hash, domain))) {
				parse_profile();
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, 
								  "Cound not find a profile for domain: [%s] Returning 0 messages\nWhen message-query-exact-match is enabled you must have a dedicated vm profile per distinct domain name you wish to use.\n", domain);
			}
		} else {
			for (hi = switch_core_hash_first(globals.profile_hash); hi; hi = switch_core_hash_next(&hi)) {
				switch_core_hash_this(hi, NULL, NULL, &val);
				profile = (vm_profile_t *) val;
				parse_profile();

				if (new_event) {
					break;
				}
			}
			switch_safe_free(hi);
		}

		switch_safe_free(dup);

	}

	if (!new_event) {
		if (switch_event_create(&new_event, SWITCH_EVENT_MESSAGE_WAITING) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(new_event, SWITCH_STACK_BOTTOM, "MWI-Messages-Waiting", "no");
			switch_event_add_header_string(new_event, SWITCH_STACK_BOTTOM, "MWI-Message-Account", account);
		}
	}

	if (new_event) {
		switch_event_header_t *hp;

		for (hp = event->headers; hp; hp = hp->next) {
			if (!strncasecmp(hp->name, "vm-", 3)) {
				switch_event_add_header_string(new_event, SWITCH_STACK_BOTTOM, hp->name + 3, hp->value);
			}
		}

		switch_event_fire(&new_event);
	}


}

static int EVENT_THREAD_RUNNING = 0;
static int EVENT_THREAD_STARTED = 0;

void *SWITCH_THREAD_FUNC vm_event_thread_run(switch_thread_t *thread, void *obj)
{
	void *pop;
	int done = 0;

	switch_mutex_lock(globals.mutex);
	if (!EVENT_THREAD_RUNNING) {
		EVENT_THREAD_RUNNING++;
		globals.threads++;
	} else {
		done = 1;
	}
	switch_mutex_unlock(globals.mutex);

	if (done) {
		return NULL;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Event Thread Started\n");

	while (globals.running == 1) {
		int count = 0;

		if (switch_queue_trypop(globals.event_queue, &pop) == SWITCH_STATUS_SUCCESS) {
			switch_event_t *event = (switch_event_t *) pop;

			if (!pop) {
				break;
			}
			actual_message_query_handler(event);
			switch_event_destroy(&event);
			count++;
		}

		if (!count) {
			switch_yield(100000);
		}
	}

	while (switch_queue_trypop(globals.event_queue, &pop) == SWITCH_STATUS_SUCCESS && pop) {
		switch_event_t *event = (switch_event_t *) pop;
		switch_event_destroy(&event);
	}


	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Event Thread Ended\n");

	switch_mutex_lock(globals.mutex);
	globals.threads--;
	EVENT_THREAD_RUNNING = EVENT_THREAD_STARTED = 0;
	switch_mutex_unlock(globals.mutex);

	return NULL;
}

void vm_event_thread_start(void)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;
	int done = 0;

	switch_mutex_lock(globals.mutex);
	if (!EVENT_THREAD_STARTED) {
		EVENT_THREAD_STARTED++;
	} else {
		done = 1;
	}
	switch_mutex_unlock(globals.mutex);

	if (done) {
		return;
	}

	switch_threadattr_create(&thd_attr, globals.pool);
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_threadattr_priority_set(thd_attr, SWITCH_PRI_IMPORTANT);
	switch_thread_create(&thread, thd_attr, vm_event_thread_run, NULL, globals.pool);
}

void vm_event_handler(switch_event_t *event)
{
	switch_event_t *cloned_event;

	switch_event_dup(&cloned_event, event);
	switch_assert(cloned_event);
	switch_queue_push(globals.event_queue, cloned_event);

	if (!EVENT_THREAD_STARTED) {
		vm_event_thread_start();
	}
}

struct holder {
	vm_profile_t *profile;
	switch_memory_pool_t *pool;
	switch_stream_handle_t *stream;
	switch_xml_t xml;
	switch_xml_t x_item;
	switch_xml_t x_channel;
	int items;
	const char *user;
	const char *domain;
	const char *host;
	const char *port;
	const char *uri;
};


static int del_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	if (argc > 8) {
		if (unlink(argv[8]) != 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Failed to delete file [%s]\n", argv[8]);
		}
	}
	return 0;
}

static int play_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	switch_file_t *fd;
	struct holder *holder = (struct holder *) pArg;
	char *fname, *ext;
	switch_size_t flen;
	uint8_t chunk[1024];
	const char *mime_type = "audio/inline", *new_type;

	if ((fname = strrchr(argv[8], '/'))) {
		fname++;
	} else {
		fname = argv[8];
	}

	if ((ext = strrchr(fname, '.'))) {
		ext++;
		if ((new_type = switch_core_mime_ext2type(ext))) {
			mime_type = new_type;
		}
	}

	if (switch_file_open(&fd, argv[8], SWITCH_FOPEN_READ, SWITCH_FPROT_UREAD | SWITCH_FPROT_UWRITE, holder->pool) == SWITCH_STATUS_SUCCESS) {
		flen = switch_file_get_size(fd);
		holder->stream->write_function(holder->stream, "Content-type: %s\n", mime_type);
		holder->stream->write_function(holder->stream, "Content-length: %ld\n\n", (long) flen);
		for (;;) {
			switch_status_t status;

			flen = sizeof(chunk);
			status = switch_file_read(fd, chunk, &flen);
			if (status != SWITCH_STATUS_SUCCESS || flen == 0) {
				break;
			}

			holder->stream->raw_write_function(holder->stream, chunk, flen);
		}
		switch_file_close(fd);
	}
	return 0;
}

static void do_play(vm_profile_t *profile, char *user_in, char *domain, char *file, switch_stream_handle_t *stream)
{
	char *sql;
	struct holder holder;
	char *user;

	user = resolve_id(user_in, domain, "web-vm");

	sql = switch_mprintf("update voicemail_msgs set read_epoch=%ld where username='%s' and domain='%s' and file_path like '%%%s'",
						 (long) switch_epoch_time_now(NULL), user, domain, file);

	vm_execute_sql(profile, sql, profile->mutex);
	free(sql);

	sql = switch_mprintf("select created_epoch, read_epoch, username, domain, uuid, cid_name, cid_number, in_folder, file_path, message_len, flags, read_flags, forwarded_by from voicemail_msgs where username='%s' and domain='%s' and file_path like '%%%s' order by created_epoch",
						 user, domain, file);
	memset(&holder, 0, sizeof(holder));
	holder.profile = profile;
	holder.stream = stream;
	switch_core_new_memory_pool(&holder.pool);
	vm_execute_sql_callback(profile, profile->mutex, sql, play_callback, &holder);
	switch_core_destroy_memory_pool(&holder.pool);
	switch_safe_free(sql);
}


static void do_del(vm_profile_t *profile, char *user_in, char *domain, char *file, switch_stream_handle_t *stream)
{
	char *myfolder = "inbox";
	char *sql;
	struct holder holder;
	char *ref = NULL;
	char *user;

	user = resolve_id(user_in, domain, "web-vm");

	if (stream->param_event) {
		ref = switch_event_get_header(stream->param_event, "http-referer");
	}

	sql = switch_mprintf("select created_epoch, read_epoch, username, domain, uuid, cid_name, cid_number, in_folder, file_path, message_len, flags, read_flags, forwarded_by from voicemail_msgs where username='%s' and domain='%s' and file_path like '%%%s' order by created_epoch",
						 user, domain, file);
	memset(&holder, 0, sizeof(holder));
	holder.profile = profile;
	holder.stream = stream;
	vm_execute_sql_callback(profile, profile->mutex, sql, del_callback, &holder);

	switch_safe_free(sql);
	sql = switch_mprintf("delete from voicemail_msgs where username='%s' and domain='%s' and file_path like '%%%s'", user, domain, file);
	vm_execute_sql(profile, sql, profile->mutex);
	free(sql);

	update_mwi(profile, user, domain, myfolder, MWI_REASON_DELETE);

	if (ref) {
		stream->write_function(stream, "Content-type: text/html\n\n<h2>Message Deleted</h2>\n" "<META http-equiv=\"refresh\" content=\"1;URL=%s\">", ref);
	}
}


static int web_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct holder *holder = (struct holder *) pArg;
	char *del, *get, *fname, *ext;
	switch_time_exp_t tm;
	char create_date[80] = "";
	char read_date[80] = "";
	char rss_date[80] = "";
	switch_size_t retsize;
	switch_time_t l_created = 0;
	switch_time_t l_read = 0;
	switch_time_t l_duration = 0;
	switch_core_time_duration_t duration;
	char duration_str[80];
	const char *fmt = "%a, %e %b %Y %T %z";
	char heard[80];
	char title_b4[128] = "";
	char title_aft[128 * 3 + 1] = "";

	if (argc > 0) {
		l_created = switch_time_make(atol(argv[0]), 0);
	}

	if (argc > 1) {
		l_read = switch_time_make(atol(argv[1]), 0);
	}

	if (argc > 9) {
		l_duration = switch_time_make(atol(argv[9]), 0);
	}

	if ((fname = strrchr(argv[8], '/'))) {
		fname++;
	} else {
		fname = argv[8];
	}

	if ((ext = strrchr(fname, '.'))) {
		ext++;
	}

	switch_core_measure_time(l_duration, &duration);
	duration.day += duration.yr * 365;
	duration.hr += duration.day * 24;

	switch_snprintf(duration_str, sizeof(duration_str), "%.2u:%.2u:%.2u", duration.hr, duration.min, duration.sec);

	if (l_created) {
		switch_time_exp_lt(&tm, l_created);
		switch_strftime_nocheck(create_date, &retsize, sizeof(create_date), fmt, &tm);
		switch_strftime_nocheck(rss_date, &retsize, sizeof(create_date), "%D %T", &tm);
	}

	if (l_read) {
		switch_time_exp_lt(&tm, l_read);
		switch_strftime_nocheck(read_date, &retsize, sizeof(read_date), fmt, &tm);
	}

	switch_snprintf(heard, sizeof(heard), *read_date == '\0' ? "never" : read_date);

	get = switch_mprintf("http://%s:%s%s/get/%s", holder->host, holder->port, holder->uri, fname);
	del = switch_mprintf("http://%s:%s%s/del/%s", holder->host, holder->port, holder->uri, fname);

	holder->stream->write_function(holder->stream, "<font face=tahoma><div class=title><b>Message from %s %s</b></div><hr noshade size=1>\n",
								   argv[5], argv[6]);
	holder->stream->write_function(holder->stream, "Priority: %s<br>\n" "Created: %s<br>\n" "Last Heard: %s<br>\n" "Duration: %s<br>\n",
								   //"<a href=%s>Delete This Message</a><br><hr noshade size=1>", 
								   strcmp(argv[10], URGENT_FLAG_STRING) ? "normal" : "urgent", create_date, heard, duration_str);

	switch_snprintf(title_b4, sizeof(title_b4), "%s <%s> %s", argv[5], argv[6], rss_date);
	switch_url_encode(title_b4, title_aft, sizeof(title_aft));

	holder->stream->write_function(holder->stream,
								   "<br><object width=550 height=15 \n"
								   "type=\"application/x-shockwave-flash\" \n"
								   "data=\"http://%s:%s/pub/slim.swf?song_url=%s&player_title=%s\">\n"
								   "<param name=movie value=\"http://%s:%s/pub/slim.swf?song_url=%s&player_title=%s\"></object><br><br>\n"
								   "[<a href=%s>delete</a>] [<a href=%s>download</a>] [<a href=tel:%s>call</a>] <br><br><br></font>\n",
								   holder->host, holder->port, get, title_aft, holder->host, holder->port, get, title_aft, del, get, argv[6]);

	free(get);
	free(del);

	return 0;
}

static int rss_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	struct holder *holder = (struct holder *) pArg;
	switch_xml_t x_tmp, x_link;
	char *tmp, *del, *get;
	switch_time_exp_t tm;
	char create_date[80] = "";
	char read_date[80] = "";
	char rss_date[80] = "";
	switch_size_t retsize;
	const char *mime_type = "audio/inline", *new_type;
	char *ext;
	char *fname;
	switch_size_t flen;
	switch_file_t *fd;
	switch_time_t l_created = 0;
	switch_time_t l_read = 0;
	switch_time_t l_duration = 0;
	switch_core_time_duration_t duration;
	char duration_str[80];
	const char *fmt = "%a, %e %b %Y %T %z";
	char heard[80];

	if (argc > 0) {
		l_created = switch_time_make(atol(argv[0]), 0);
	}

	if (argc > 1) {
		l_read = switch_time_make(atol(argv[1]), 0);
	}

	if (argc > 9) {
		l_duration = switch_time_make(atol(argv[9]), 0);
	}

	switch_core_measure_time(l_duration, &duration);
	duration.day += duration.yr * 365;
	duration.hr += duration.day * 24;

	switch_snprintf(duration_str, sizeof(duration_str), "%.2u:%.2u:%.2u", duration.hr, duration.min, duration.sec);

	if (l_created) {
		switch_time_exp_lt(&tm, l_created);
		switch_strftime_nocheck(create_date, &retsize, sizeof(create_date), fmt, &tm);
		switch_strftime_nocheck(rss_date, &retsize, sizeof(create_date), fmt, &tm);
	}

	if (l_read) {
		switch_time_exp_lt(&tm, l_read);
		switch_strftime_nocheck(read_date, &retsize, sizeof(read_date), fmt, &tm);
	}

	holder->x_item = switch_xml_add_child_d(holder->x_channel, "item", holder->items++);

	x_tmp = switch_xml_add_child_d(holder->x_item, "title", 0);
	tmp = switch_mprintf("Message from %s %s on %s", argv[5], argv[6], create_date);
	switch_xml_set_txt_d(x_tmp, tmp);
	free(tmp);

	x_tmp = switch_xml_add_child_d(holder->x_item, "description", 0);

	switch_snprintf(heard, sizeof(heard), *read_date == '\0' ? "never" : read_date);

	if ((fname = strrchr(argv[8], '/'))) {
		fname++;
	} else {
		fname = argv[8];
	}

	get = switch_mprintf("http://%s:%s%s/get/%s", holder->host, holder->port, holder->uri, fname);
	del = switch_mprintf("http://%s:%s%s/del/%s", holder->host, holder->port, holder->uri, fname);
	x_link = switch_xml_add_child_d(holder->x_item, "fsvm:rmlink", 0);
	switch_xml_set_txt_d(x_link, del);

	tmp = switch_mprintf("<![CDATA[Priority: %s<br>"
						 "Last Heard: %s<br>Duration: %s<br>"
						 "<a href=%s>Delete This Message</a><br>"
						 "]]>", strcmp(argv[10], URGENT_FLAG_STRING) ? "normal" : "urgent", heard, duration_str, del);

	switch_xml_set_txt_d(x_tmp, tmp);
	free(tmp);
	free(del);

	x_tmp = switch_xml_add_child_d(holder->x_item, "pubDate", 0);
	switch_xml_set_txt_d(x_tmp, rss_date);

	x_tmp = switch_xml_add_child_d(holder->x_item, "itunes:duration", 0);
	switch_xml_set_txt_d(x_tmp, duration_str);

	x_tmp = switch_xml_add_child_d(holder->x_item, "guid", 0);
	switch_xml_set_txt_d(x_tmp, get);

	x_link = switch_xml_add_child_d(holder->x_item, "link", 0);
	switch_xml_set_txt_d(x_link, get);

	x_tmp = switch_xml_add_child_d(holder->x_item, "enclosure", 0);
	switch_xml_set_attr_d(x_tmp, "url", get);
	free(get);

	if (switch_file_open(&fd, argv[8], SWITCH_FOPEN_READ, SWITCH_FPROT_UREAD | SWITCH_FPROT_UWRITE, holder->pool) == SWITCH_STATUS_SUCCESS) {
		flen = switch_file_get_size(fd);
		tmp = switch_mprintf("%ld", (long) flen);
		switch_xml_set_attr_d(x_tmp, "length", tmp);
		free(tmp);
		switch_file_close(fd);
	}

	if ((ext = strrchr(fname, '.'))) {
		ext++;
		if ((new_type = switch_core_mime_ext2type(ext))) {
			mime_type = new_type;
		}
	}
	switch_xml_set_attr_d(x_tmp, "type", mime_type);

	return 0;
}


static void do_rss(vm_profile_t *profile, char *user, char *domain, char *host, char *port, char *uri, switch_stream_handle_t *stream)
{
	struct holder holder;
	switch_xml_t x_tmp;
	char *sql, *xmlstr;
	char *tmp = NULL;

	stream->write_function(stream, "Content-type: text/xml\n\n");
	memset(&holder, 0, sizeof(holder));
	holder.profile = profile;
	holder.stream = stream;
	holder.xml = switch_xml_new("rss");
	holder.user = user;
	holder.domain = domain;
	holder.host = host;
	holder.port = port;
	holder.uri = uri;

	switch_core_new_memory_pool(&holder.pool);
	switch_assert(holder.xml);

	switch_xml_set_attr_d(holder.xml, "xmlns:itunes", "http://www.itunes.com/dtds/podcast-1.0.dtd");
	switch_xml_set_attr_d(holder.xml, "xmlns:fsvm", "http://www.freeswitch.org/dtd/fsvm.dtd");
	switch_xml_set_attr_d(holder.xml, "version", "2.0");
	holder.x_channel = switch_xml_add_child_d(holder.xml, "channel", 0);

	x_tmp = switch_xml_add_child_d(holder.x_channel, "title", 0);
	tmp = switch_mprintf("FreeSWITCH Voicemail for %s@%s", user, domain);
	switch_xml_set_txt_d(x_tmp, tmp);
	free(tmp);

	x_tmp = switch_xml_add_child_d(holder.x_channel, "link", 0);
	switch_xml_set_txt_d(x_tmp, "http://www.freeswitch.org");

	x_tmp = switch_xml_add_child_d(holder.x_channel, "description", 0);
	switch_xml_set_txt_d(x_tmp, "http://www.freeswitch.org");

	x_tmp = switch_xml_add_child_d(holder.x_channel, "ttl", 0);
	switch_xml_set_txt_d(x_tmp, "15");

	sql = switch_mprintf("select created_epoch, read_epoch, username, domain, uuid, cid_name, cid_number, in_folder, file_path, message_len, flags, read_flags, forwarded_by from voicemail_msgs where username='%s' and domain='%s' order by read_flags, created_epoch", user, domain);
	vm_execute_sql_callback(profile, profile->mutex, sql, rss_callback, &holder);

	xmlstr = switch_xml_toxml(holder.xml, SWITCH_TRUE);

	stream->write_function(stream, "%s", xmlstr);

	switch_safe_free(sql);
	switch_safe_free(xmlstr);
	switch_xml_free(holder.xml);
	switch_core_destroy_memory_pool(&holder.pool);
}


static void do_web(vm_profile_t *profile, const char *user_in, const char *domain, const char *host, const char *port, const char *uri,
				   switch_stream_handle_t *stream)
{
	char buf[80] = "";
	struct holder holder;
	char *sql;
	callback_t cbt = { 0 };
	int ttl = 0;
	char *user;

	user = resolve_id(user_in, domain, "web-vm");

	stream->write_function(stream, "Content-type: text/html\n\n");
	memset(&holder, 0, sizeof(holder));
	holder.profile = profile;
	holder.stream = stream;
	holder.user = user;
	holder.domain = domain;
	holder.host = host;
	holder.port = port;
	holder.uri = uri;

	if (profile->web_head) {
		stream->raw_write_function(stream, (uint8_t *) profile->web_head, strlen(profile->web_head));
	}

	cbt.buf = buf;
	cbt.len = sizeof(buf);

	sql = switch_mprintf("select created_epoch, read_epoch, username, domain, uuid, cid_name, cid_number, in_folder, file_path, message_len, flags, read_flags, forwarded_by from voicemail_msgs where username='%s' and domain='%s' order by read_flags, created_epoch", user, domain);
	vm_execute_sql_callback(profile, profile->mutex, sql, web_callback, &holder);
	switch_safe_free(sql);

	sql = switch_mprintf("select count(*) from voicemail_msgs where username='%s' and domain='%s' order by read_flags", user, domain);
	vm_execute_sql_callback(profile, profile->mutex, sql, sql2str_callback, &cbt);
	switch_safe_free(sql);

	ttl = atoi(buf);
	stream->write_function(stream, "%d message%s<br>", ttl, ttl == 1 ? "" : "s");

	if (profile->web_tail) {
		stream->raw_write_function(stream, (uint8_t *) profile->web_tail, strlen(profile->web_tail));
	}

	if (user != user_in) {
		free(user);
	}
}

#define VM_INJECT_USAGE "[group=<group>[@domain]|domain=<domain>|<box>[@<domain>]] <sound_file> [<cid_num>] [<cid_name>]"
SWITCH_STANDARD_API(voicemail_inject_api_function)
{
	if (voicemail_inject(cmd, session) == SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "%s", "+OK\n");
	} else {
		stream->write_function(stream, "Error: %s\n", VM_INJECT_USAGE);
	}
	return SWITCH_STATUS_SUCCESS;
}


static int api_del_callback(void *pArg, int argc, char **argv, char **columnNames)
{

	unlink(argv[3]);
	
    return 0;
}


#define VM_DELETE_USAGE "<id>@<domain>[/profile] [<uuid>]"
SWITCH_STANDARD_API(voicemail_delete_api_function)
{
	char *sql;
	char *id = NULL, *domain = NULL, *uuid = NULL, *profile_name = "default";
	char *p, *e = NULL;
	vm_profile_t *profile;

	if (zstr(cmd)) {
		stream->write_function(stream, "Usage: %s\n", VM_DELETE_USAGE);
		return SWITCH_STATUS_SUCCESS;
	}

	id = strdup(cmd);
	
	if ((p = strchr(id, '@'))) {
		*p++ = '\0';
		domain = e = p;
	}

	if (domain && (p = strchr(domain, '/'))) {
		*p++ = '\0';
		profile_name = e = p;
	}

	if (e && (p = strchr(e, ' '))) {
		*p++ = '\0';
		uuid = p;
	}


	if (id && domain && profile_name && (profile = get_profile(profile_name))) {
		
		if (uuid) {
			sql = switch_mprintf("select username, domain, in_folder, file_path from voicemail_msgs where uuid='%q'", uuid);
		} else {
			sql = switch_mprintf("select username, domain, in_folder, file_path from voicemail_msgs where username='%q' and domain='%q'", id, domain);
		}
		
		vm_execute_sql_callback(profile, profile->mutex, sql, api_del_callback, profile);
		switch_safe_free(sql);
		
		if (uuid) {
			sql = switch_mprintf("delete from voicemail_msgs where uuid='%q'", uuid);
		} else {
			sql = switch_mprintf("delete from voicemail_msgs where username='%q' and domain='%q'", id, domain);
		}

		vm_execute_sql(profile, sql, profile->mutex);
		switch_safe_free(sql);
		
		update_mwi(profile, id, domain, "inbox", MWI_REASON_DELETE);
	
		stream->write_function(stream, "%s", "+OK\n");
		profile_rwunlock(profile);
	} else {
		stream->write_function(stream, "%s", "-ERR can't find user or profile.\n");
	}

	switch_safe_free(id);

	return SWITCH_STATUS_SUCCESS;
}




#define VM_READ_USAGE "<id>@<domain>[/profile] <read|unread> [<uuid>]"
SWITCH_STANDARD_API(voicemail_read_api_function)
{
	char *sql;
	char *id = NULL, *domain = NULL, *uuid = NULL, *profile_name = "default";
	char *ru = NULL, *p, *e = NULL;
	vm_profile_t *profile;
	int mread = -1;

	if (zstr(cmd)) {
		stream->write_function(stream, "Usage: %s\n", VM_READ_USAGE);
		return SWITCH_STATUS_SUCCESS;
	}

	id = strdup(cmd);
	
	if ((p = strchr(id, '@'))) {
		*p++ = '\0';
		domain = e = p;
	}

	if (domain && (p = strchr(domain, '/'))) {
		*p++ = '\0';
		profile_name = e = p;
	}

	if (e && (p = strchr(e, ' '))) {
		*p++ = '\0';
		ru = e = p;
	}

	if (e && (p = strchr(e, ' '))) {
		*p++ = '\0';
		uuid = p;
	}

	if (ru) {
		if (!strcasecmp(ru, "read")) {
			mread = 1;
		} else if (!strcasecmp(ru, "unread")) {
			mread = 0;
		} else {
			mread = -1;
		}
	}


	if (mread > -1 && id && domain && profile_name && (profile = get_profile(profile_name))) {
		
		if (mread) {
			if (uuid) {
				sql = switch_mprintf("update voicemail_msgs set read_epoch=%ld where uuid='%q'", (long) switch_epoch_time_now(NULL), uuid);
			} else {
				sql = switch_mprintf("update voicemail_msgs set read_epoch=%ld where domain='%q'", (long) switch_epoch_time_now(NULL), domain);
			}
		} else{
			if (uuid) {
				sql = switch_mprintf("update voicemail_msgs set read_epoch=0,flags='' where uuid='%q'", uuid);
			} else {
				sql = switch_mprintf("update voicemail_msgs set read_epoch=0,flags='' where domain='%q'", domain);
			}
		}

		vm_execute_sql(profile, sql, profile->mutex);
		switch_safe_free(sql);
		
		update_mwi(profile, id, domain, "inbox", MWI_REASON_READ);
	
		stream->write_function(stream, "%s", "+OK\n");

		profile_rwunlock(profile);
	} else {
		stream->write_function(stream, "%s", "-ERR can't find user or profile.\n");
	}

	switch_safe_free(id);

	return SWITCH_STATUS_SUCCESS;
}



static int api_list_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	switch_stream_handle_t *stream = (switch_stream_handle_t *) pArg;

	if (!strcasecmp(argv[10], "xml")) {
		stream->write_function(stream, " <message>\n");
		stream->write_function(stream, "  <created_epoch>%s</created_epoch>\n", argv[0]);
		stream->write_function(stream, "  <read_epoch>%s</read_epoch>\n", argv[1]);
		stream->write_function(stream, "  <username>%s</username>\n", argv[2]);
		stream->write_function(stream, "  <domain>%s</domain>\n", argv[3]);
		stream->write_function(stream, "  <folder>%s</folder>\n", argv[4]);
		stream->write_function(stream, "  <path>%s</path>\n", argv[5]);
		stream->write_function(stream, "  <uuid>%s</uuid>\n", argv[6]);
		stream->write_function(stream, "  <cid-name>%s</cid-name>\n", argv[7]);
		stream->write_function(stream, "  <cid-number>%s</cid-number>\n", argv[8]);
		stream->write_function(stream, "  <message-len>%s</message-len>\n", argv[9]);
		stream->write_function(stream, " </message>\n");
	} else {
		stream->write_function(stream, "%s:%s:%s:%s:%s:%s:%s:%s:%s:%s\n", argv[0], argv[1], argv[2], argv[3], argv[4], argv[5], argv[6], argv[7], argv[8], argv[9]);
	}
	
    return 0;
}


#define VM_LIST_USAGE "<id>@<domain>[/profile] [xml]"
SWITCH_STANDARD_API(voicemail_list_api_function)
{
	char *sql;
	char *id = NULL, *uuid = NULL, *domain = NULL, *format = "text", *profile_name = "default";
	char *p, *e = NULL;
	vm_profile_t *profile;

	if (zstr(cmd)) {
		stream->write_function(stream, "Usage: %s\n", VM_LIST_USAGE);
		return SWITCH_STATUS_SUCCESS;
	}

	id = strdup(cmd);
	
	if ((p = strchr(id, '@'))) {
		*p++ = '\0';
		domain = e = p;
	}

	if (domain && (p = strchr(domain, '/'))) {
		*p++ = '\0';
		profile_name = e = p;
	}

	if (e && (p = strchr(e, ' '))) {
		*p++ = '\0';
		format = e = p;
	}

	if (e && (p = strchr(e, ' '))) {
		*p++ = '\0';
		uuid = p;
	}

	if (id && domain && profile_name && (profile = get_profile(profile_name))) {
		if (uuid) {
			sql = switch_mprintf("select created_epoch, read_epoch, username, domain, in_folder, file_path, uuid, cid_name, cid_number, message_len, "
								 "'%q' from voicemail_msgs where username='%q' and domain='%q' and uuid='%q'", 
								 format, id, domain, uuid);
		} else {
			sql = switch_mprintf("select created_epoch, read_epoch, username, domain, in_folder, file_path, uuid, cid_name, cid_number, message_len, "
								 "'%q' from voicemail_msgs where username='%q' and domain='%q'", 
								 format, id, domain);
		}

		if (!strcasecmp(format, "xml")) {
			stream->write_function(stream, "<voicemail>\n");
		}

		vm_execute_sql_callback(profile, profile->mutex, sql, api_list_callback, stream);
		switch_safe_free(sql);
	
		if (!strcasecmp(format, "xml")) {
			stream->write_function(stream, "</voicemail>\n");
		}

		profile_rwunlock(profile);
	} else {
		stream->write_function(stream, "%s", "-ERR can't find user or profile.\n");
	}

	switch_safe_free(id);

	return SWITCH_STATUS_SUCCESS;
}

#define VOICEMAIL_SYNTAX "rss [<host> <port> <uri> <user> <domain>] | [load|unload|reload] <profile> [reloadxml]"
SWITCH_STANDARD_API(voicemail_api_function)
{
	char *mydata = NULL, *argv[6];
	char *host = NULL, *port = NULL, *uri = NULL;
	char *user = NULL, *domain = NULL;
	int ct = 0;
	vm_profile_t *profile = NULL;
	char *path_info = NULL;
	int rss = 0, xarg = 0;
	switch_hash_index_t *hi;
	void *val = NULL;
	switch_xml_t xml_root;
	const char *err;
	int argc = 0;

	if (session) {
		return SWITCH_STATUS_FALSE;
	}

	if (stream->param_event) {
		host = switch_event_get_header(stream->param_event, "http-host");
		port = switch_event_get_header(stream->param_event, "http-port");
		uri = switch_event_get_header(stream->param_event, "http-uri");
		user = switch_event_get_header(stream->param_event, "freeswitch-user");
		domain = switch_event_get_header(stream->param_event, "freeswitch-domain");
		path_info = switch_event_get_header(stream->param_event, "http-path-info");
	}

	if (!zstr(cmd)) {
		mydata = strdup(cmd);
		switch_assert(mydata);
		argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argc > 0) {
		if (!strcasecmp(argv[0], "rss")) {
			rss++;
			xarg++;
		} else if (argc > 1 && !strcasecmp(argv[0], "load")) {
			if (argc > 2 && !strcasecmp(argv[2], "reloadxml")) {
				if ((xml_root = switch_xml_open_root(1, &err))) {
					switch_xml_free(xml_root);
				}
				stream->write_function(stream, "Reload XML [%s]\n", err);
			}
			if ((profile = get_profile(argv[1]))) {
				profile_rwunlock(profile);
			}
			stream->write_function(stream, "+OK load complete\n");
			goto done;
		} else if (argc > 1 && !strcasecmp(argv[0], "unload")) {
			destroy_profile(argv[1], SWITCH_FALSE);
			stream->write_function(stream, "+OK unload complete\n");
			goto done;
		} else if (argc > 1 && !strcasecmp(argv[0], "reload")) {
			destroy_profile(argv[1], SWITCH_FALSE);
			if (argc > 2 && !strcasecmp(argv[2], "reloadxml")) {
				if ((xml_root = switch_xml_open_root(1, &err))) {
					switch_xml_free(xml_root);
				}
				stream->write_function(stream, "Reload XML [%s]\n", err);
			}
			if ((profile = get_profile(argv[1]))) {
				profile_rwunlock(profile);
			}
			stream->write_function(stream, "+OK reload complete\n");
			goto done;

		} else if (!strcasecmp(argv[0], "status")) {
			stream->write_function(stream, "============================\n");
			switch_mutex_lock(globals.mutex);
			for (hi = switch_core_hash_first(globals.profile_hash); hi; hi = switch_core_hash_next(&hi)) {
				switch_core_hash_this(hi, NULL, NULL, &val);
				profile = (vm_profile_t *) val;
				stream->write_function(stream, "Profile: %s\n", profile->name);
			}
			switch_mutex_unlock(globals.mutex);
			stream->write_function(stream, "============================\n");
			goto done;
		}
	}

	if (!host) {
		if (argc - rss < 5) {
			goto error;
		}
		host = argv[xarg++];
		port = argv[xarg++];
		uri = argv[xarg++];
		user = argv[xarg++];
		domain = argv[xarg++];
	}

	if (!(host && port && uri && user && domain)) {
		goto error;
	}

	if (!(profile = get_profile(domain))) {
		profile = get_profile("default");
	}

	if (!profile) {
		switch_hash_index_t *index;
		void *value;

		switch_mutex_lock(globals.mutex);
		for (index = switch_core_hash_first(globals.profile_hash); index; index = switch_core_hash_next(&index)) {
			switch_core_hash_this(index, NULL, NULL, &value);
			profile = (vm_profile_t *) value;
			if (profile) {
				switch_thread_rwlock_rdlock(profile->rwlock);
				break;
			}
		}
		switch_safe_free(index);
		switch_mutex_unlock(globals.mutex);
	}

	if (!profile) {
		stream->write_function(stream, "Can't find profile.\n");
		goto error;
	}

	if (path_info) {
		if (!strncasecmp(path_info, "get/", 4)) {
			do_play(profile, user, domain, path_info + 4, stream);
		} else if (!strncasecmp(path_info, "del/", 4)) {
			do_del(profile, user, domain, path_info + 4, stream);
		} else if (!strncasecmp(path_info, "web", 3)) {
			do_web(profile, user, domain, host, port, uri, stream);
		}
	}

	if (rss || (path_info && !strncasecmp(path_info, "rss", 3))) {
		do_rss(profile, user, domain, host, port, uri, stream);
	}

	profile_rwunlock(profile);
	goto done;

  error:
	if (host) {
		if (!ct) {
			stream->write_function(stream, "Content-type: text/html\n\n<h2>");
		}
	}
	stream->write_function(stream, "Error: %s\n", VOICEMAIL_SYNTAX);

  done:
	switch_safe_free(mydata);
	return SWITCH_STATUS_SUCCESS;
}

struct msg_get_callback {
	switch_event_t *my_params;
};
typedef struct msg_get_callback msg_get_callback_t;

static int message_get_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	msg_get_callback_t *cbt = (msg_get_callback_t *) pArg;

	switch_event_add_header(cbt->my_params, SWITCH_STACK_BOTTOM, "VM-Message-Received-Epoch", "%s", argv[0]);
	switch_event_add_header(cbt->my_params, SWITCH_STACK_BOTTOM, "VM-Message-Read-Epoch", "%s", argv[1]);
	/*	switch_event_add_header(cbt->my_params, SWITCH_STACK_BOTTOM, user, argv[2], 255); */
	/*	switch_event_add_header(cbt->my_params, SWITCH_STACK_BOTTOM, domain, argv[3], 255); */
	switch_event_add_header(cbt->my_params, SWITCH_STACK_BOTTOM, "VM-Message-UUID", "%s", argv[4]);
	switch_event_add_header(cbt->my_params, SWITCH_STACK_BOTTOM, "VM-Message-Caller-Name", "%s", argv[5]);
	switch_event_add_header(cbt->my_params, SWITCH_STACK_BOTTOM, "VM-Message-Caller-Number", "%s", argv[6]);
	switch_event_add_header(cbt->my_params, SWITCH_STACK_BOTTOM, "VM-Message-Folder", "%s", argv[7]);
	switch_event_add_header(cbt->my_params, SWITCH_STACK_BOTTOM, "VM-Message-File-Path", "%s", argv[8]);
	switch_event_add_header(cbt->my_params, SWITCH_STACK_BOTTOM, "VM-Message-Duration", "%s", argv[9]);
	switch_event_add_header(cbt->my_params, SWITCH_STACK_BOTTOM, "VM-Message-Flags", "%s", argv[10]);
	switch_event_add_header(cbt->my_params, SWITCH_STACK_BOTTOM, "VM-Message-Read-Flags", "%s", argv[11]);
	switch_event_add_header(cbt->my_params, SWITCH_STACK_BOTTOM, "VM-Message-Forwarded-By", "%s", argv[12]);

	return 0;
}

struct msg_lst_callback {
	char *buf;
	size_t len;
	switch_event_t *my_params;
};
typedef struct msg_lst_callback msg_lst_callback_t;

static int message_list_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	msg_lst_callback_t *cbt = (msg_lst_callback_t *) pArg;
	char *varname = NULL;
	/* Message # never start with 0 */
	varname = switch_mprintf("VM-List-Message-%ld-UUID", ++cbt->len);
	switch_event_add_header(cbt->my_params, SWITCH_STACK_BOTTOM, varname, "%s", argv[0]);
	switch_safe_free(varname);
	return 0;
}

static int message_purge_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	const char *profile_name = argv[0];
	const char *uuid = argv[1];
	const char *id = argv[2];
	const char *domain = argv[3];
	const char *file_path = argv[4];
	char *sql;
	vm_profile_t *profile = get_profile(profile_name);

	if (unlink(file_path) != 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Failed to delete file [%s]\n", file_path);
	} else {
		sql = switch_mprintf("DELETE FROM voicemail_msgs WHERE username='%q' AND domain='%q' AND uuid = '%q'", id, domain, uuid);
		vm_execute_sql(profile, sql, profile->mutex);
		switch_safe_free(sql);
	}
	profile_rwunlock(profile);

	return 0;
}

/* Preference API */
#define VM_FSDB_PREF_GREETING_SET_USAGE "<profile> <domain> <user> <slot> [file-path]"
SWITCH_STANDARD_API(vm_fsdb_pref_greeting_set_function)
{
	int slot = -1;
	const char *file_path = NULL;
	char *sql = NULL;
	char res[254] = "";

	char *id = NULL, *domain = NULL, *profile_name = NULL;
	vm_profile_t *profile = NULL;

	char *argv[6] = { 0 };
	char *mycmd = NULL;

	switch_memory_pool_t *pool;

	switch_core_new_memory_pool(&pool);

	if (!zstr(cmd)) {
		mycmd = switch_core_strdup(pool, cmd);
		switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argv[0])
		profile_name = argv[0];
	if (argv[1])
		domain = argv[1];
	if (argv[2])
		id = argv[2];
	if (argv[3])
		slot = atoi(argv[3]);
	if (argv[4])
		file_path = argv[4];

	if (!profile_name || !domain || !id || !slot) {
		stream->write_function(stream, "-ERR Missing Arguments\n");
		goto done;
	}

	if (!(profile = get_profile(profile_name))) {
		stream->write_function(stream, "-ERR Profile not found\n");
		goto done;
	} else {
		char *dir_path = switch_core_sprintf(pool, "%s%svoicemail%s%s%s%s%s%s", SWITCH_GLOBAL_dirs.storage_dir,
				SWITCH_PATH_SEPARATOR,
				SWITCH_PATH_SEPARATOR,
				profile->name, SWITCH_PATH_SEPARATOR, domain, SWITCH_PATH_SEPARATOR, id);
		char *final_file_path = switch_core_sprintf(pool, "%s%sgreeting_%d.%s", dir_path, SWITCH_PATH_SEPARATOR, slot, profile->file_ext);

		switch_dir_make_recursive(dir_path, SWITCH_DEFAULT_DIR_PERMS, pool);

		if (file_path) {
			if (switch_file_exists(file_path, pool) != SWITCH_STATUS_SUCCESS) {
				stream->write_function(stream, "-ERR Filename doesn't exist\n");
				profile_rwunlock(profile);
				goto done;
			}

			switch_file_rename(file_path, final_file_path, pool);
		}

		if (switch_file_exists(final_file_path, pool) == SWITCH_STATUS_SUCCESS) {

			sql = switch_mprintf("SELECT count(*) FROM voicemail_prefs WHERE username = '%q' AND domain = '%q'", id, domain);
			vm_execute_sql2str(profile, profile->mutex, sql, res, sizeof(res));
			switch_safe_free(sql);

			if (atoi(res) == 0) {
				sql = switch_mprintf("INSERT INTO voicemail_prefs (username, domain, greeting_path) VALUES('%q', '%q', '%q')", id, domain, final_file_path);
			} else {
				sql = switch_mprintf("UPDATE voicemail_prefs SET greeting_path = '%q' WHERE username = '%q' AND domain = '%q'", final_file_path, id, domain);
			}
			vm_execute_sql(profile, sql, profile->mutex);
			switch_safe_free(sql);
		} else {
			stream->write_function(stream, "-ERR Recording doesn't exist [%s]\n", final_file_path);
		}
		profile_rwunlock(profile);
	}

	stream->write_function(stream, "-OK\n");
done:
	switch_core_destroy_memory_pool(&pool);
	return SWITCH_STATUS_SUCCESS;
}

#define VM_FSDB_PREF_GREETING_GET_USAGE "<format> <profile> <domain> <user> [slot]"
SWITCH_STANDARD_API(vm_fsdb_pref_greeting_get_function)
{
	/* int slot = -1; not implemented yet */
	char *sql = NULL;
	char res[254] = "";

	char *id = NULL, *domain = NULL, *profile_name = NULL;
	vm_profile_t *profile = NULL;

	char *argv[6] = { 0 };
	char *mycmd = NULL;

	switch_memory_pool_t *pool;

	switch_core_new_memory_pool(&pool);

	if (!zstr(cmd)) {
		mycmd = switch_core_strdup(pool, cmd);
		switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argv[1])
		profile_name = argv[1];
	if (argv[2])
		domain = argv[2];
	if (argv[3])
		id = argv[3];
/*	if (argv[4])
		slot = atoi(argv[4]);
not implemented yet
*/

	if (!profile_name || !domain || !id) {
		stream->write_function(stream, "-ERR Missing Arguments\n");
		goto done;
	}

	if (!(profile = get_profile(profile_name))) {
		stream->write_function(stream, "-ERR Profile not found\n");
		goto done;
	}
        sql = switch_mprintf("select greeting_path from voicemail_prefs WHERE domain = '%q' AND username = '%q'", domain, id);

	vm_execute_sql2str(profile, profile->mutex, sql, res, sizeof(res));

	switch_safe_free(sql);

	profile_rwunlock(profile);

	/* TODO If no slot requested, returned currently selected and figure out the slot number from the file name. 
 	 * IF slot provided, check if file exist, check if it currently selected */
	if (zstr(res)) {
		stream->write_function(stream, "-ERR No greeting found\n");	
	} else {
		switch_event_t *my_params = NULL;
		char *ebuf = NULL;

		switch_event_create(&my_params, SWITCH_EVENT_REQUEST_PARAMS);
		switch_event_add_header(my_params, SWITCH_STACK_BOTTOM, "VM-Preference-Greeting-File-Path", "%s", res);
		switch_event_add_header(my_params, SWITCH_STACK_BOTTOM, "VM-Preference-Greeting-Slot", "%s", "Not Implemented yet");
		switch_event_add_header(my_params, SWITCH_STACK_BOTTOM, "VM-Preference-Greeting-Selected", "%s", "True");
		switch_event_serialize_json(my_params, &ebuf);
		switch_event_destroy(&my_params);

		stream->write_function(stream, "%s", ebuf);
		switch_safe_free(ebuf);

	}
done:
        switch_core_destroy_memory_pool(&pool);
        return SWITCH_STATUS_SUCCESS;
}

#define VM_FSDB_PREF_RECNAME_SET_USAGE "<profile> <domain> <user> <file-path>"
SWITCH_STANDARD_API(vm_fsdb_pref_recname_set_function)
{
	const char *file_path = NULL;

	char *sql = NULL;
	char res[254] = "";

	char *id = NULL, *domain = NULL, *profile_name = NULL;
	vm_profile_t *profile = NULL;

	char *argv[6] = { 0 };
	char *mycmd = NULL;

	switch_memory_pool_t *pool;

	switch_core_new_memory_pool(&pool);

	if (!zstr(cmd)) {
		mycmd = switch_core_strdup(pool, cmd);
		switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argv[0])
		profile_name = argv[0];
	if (argv[1])
		domain = argv[1];
	if (argv[2])
		id = argv[2];
	if (argv[3])
		file_path = argv[3];

	if (!profile_name || !domain || !id || !file_path) {
		stream->write_function(stream, "-ERR Missing Arguments\n");
		goto done;
	}

	if (!(profile = get_profile(profile_name))) {
		stream->write_function(stream, "-ERR Profile not found\n");
		goto done;
	}

	if (switch_file_exists(file_path, pool) != SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "-ERR Filename doesn't exist\n");
		profile_rwunlock(profile);
		goto done;
	}

	sql = switch_mprintf("SELECT count(*) FROM voicemail_prefs WHERE username = '%q' AND domain = '%q'", id, domain);
	vm_execute_sql2str(profile, profile->mutex, sql, res, sizeof(res));
	switch_safe_free(sql);

	{
		char *dir_path = switch_core_sprintf(pool, "%s%svoicemail%s%s%s%s%s%s", SWITCH_GLOBAL_dirs.storage_dir,
				SWITCH_PATH_SEPARATOR,
				SWITCH_PATH_SEPARATOR,
				profile->name, SWITCH_PATH_SEPARATOR, domain, SWITCH_PATH_SEPARATOR, id);
		char *final_file_path = switch_core_sprintf(pool, "%s%srecorded_name.%s", dir_path, SWITCH_PATH_SEPARATOR, profile->file_ext);

		switch_dir_make_recursive(dir_path, SWITCH_DEFAULT_DIR_PERMS, pool);

		if (switch_file_exists(file_path, pool) != SWITCH_STATUS_SUCCESS) {
			stream->write_function(stream, "-ERR Filename doesn't exist\n");
			profile_rwunlock(profile);
			goto done;
		}

		switch_file_rename(file_path, final_file_path, pool);

		if (atoi(res) == 0) {
			sql = switch_mprintf("INSERT INTO voicemail_prefs (username, domain, name_path) VALUES('%q', '%q', '%q')", id, domain, final_file_path);
		} else {
			sql = switch_mprintf("UPDATE voicemail_prefs SET name_path = '%q' WHERE username = '%q' AND domain = '%q'", final_file_path, id, domain);
		}
		vm_execute_sql(profile, sql, profile->mutex);
		switch_safe_free(sql);


	}
	profile_rwunlock(profile);
	stream->write_function(stream, "-OK\n");
done:
	switch_core_destroy_memory_pool(&pool);
	return SWITCH_STATUS_SUCCESS;
}

#define VM_FSDB_PREF_PASSWORD_SET_USAGE "<profile> <domain> <user> <password>"
SWITCH_STANDARD_API(vm_fsdb_pref_password_set_function)
{
	const char *password = NULL;

	char *sql = NULL;
	char res[254] = "";

	const char *id = NULL, *domain = NULL, *profile_name = NULL;
	vm_profile_t *profile = NULL;

	char *argv[6] = { 0 };
	char *mycmd = NULL;

	switch_memory_pool_t *pool;

	switch_core_new_memory_pool(&pool);

	if (!zstr(cmd)) {
		mycmd = switch_core_strdup(pool, cmd);
		switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argv[0])
		profile_name = argv[0];
	if (argv[1])
		domain = argv[1];
	if (argv[2])
		id = argv[2];
	if (argv[3])
		password = argv[3];

	if (!profile_name || !domain || !id || !password) {
		stream->write_function(stream, "-ERR Missing Arguments\n");
		goto done;
	}

	if (!(profile = get_profile(profile_name))) {
		stream->write_function(stream, "-ERR Profile not found\n");
		goto done;
	}

	sql = switch_mprintf("SELECT count(*) FROM voicemail_prefs WHERE username = '%q' AND domain = '%q'", id, domain);
	vm_execute_sql2str(profile, profile->mutex, sql, res, sizeof(res));
	switch_safe_free(sql);

	if (atoi(res) == 0) {
		sql = switch_mprintf("INSERT INTO voicemail_prefs (username, domain, password) VALUES('%q', '%q', '%q')", id, domain, password);
	} else {
		sql = switch_mprintf("UPDATE voicemail_prefs SET password = '%q' WHERE username = '%q' AND domain = '%q'", password, id, domain);
	}
	vm_execute_sql(profile, sql, profile->mutex);
	switch_safe_free(sql);
	profile_rwunlock(profile);

	stream->write_function(stream, "-OK\n");
done:
	switch_core_destroy_memory_pool(&pool);
	return SWITCH_STATUS_SUCCESS;
}



/* Message API */

#define VM_FSDB_MSG_LIST_USAGE "<format> <profile> <domain> <user> <folder> <filter> [msg-order = ASC | DESC]"
SWITCH_STANDARD_API(vm_fsdb_msg_list_function)
{
	char *sql;
	msg_lst_callback_t cbt = { 0 };
	char *ebuf = NULL;

	const char *id = NULL, *domain = NULL, *profile_name = NULL, *folder = NULL, *msg_type = NULL, *msg_order = NULL;
	vm_profile_t *profile = NULL;

	char *argv[7] = { 0 };
	char *mycmd = NULL;

	switch_memory_pool_t *pool;

	switch_core_new_memory_pool(&pool);

	if (!zstr(cmd)) {
		mycmd = switch_core_strdup(pool, cmd);
		switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argv[1])
		profile_name = argv[1];
	if (argv[2])
		domain = argv[2];
	if (argv[3])
		id = argv[3];
	if (argv[4])
		folder = argv[4]; /* TODO add Support */
	if (argv[5])
		msg_type = argv[5];
	if (argv[6])
		msg_order = argv[6];

	if (!profile_name || !domain || !id || !folder || !msg_type) {
		stream->write_function(stream, "-ERR Missing Arguments\n");
		goto done;
	}

	if (!msg_order) {
		msg_order = "ASC";
	} else if (strcasecmp(msg_order, "ASC") || strcasecmp(msg_order, "DESC")) {
		stream->write_function(stream, "-ERR Bad Argument: '%s'\n", msg_order);
		goto done;
	}

	if (!(profile = get_profile(profile_name))) {
		stream->write_function(stream, "-ERR Profile not found\n");
		goto done;
	}
	if (!strcasecmp(msg_type, "not-read")) {
		sql = switch_mprintf("SELECT uuid FROM voicemail_msgs WHERE username = '%q' AND domain = '%q' AND read_epoch = 0 ORDER BY read_flags, created_epoch %q", id, domain, msg_order);
	} else if (!strcasecmp(msg_type, "new")) {
		sql = switch_mprintf("SELECT uuid FROM voicemail_msgs WHERE username = '%q' AND domain = '%q' AND flags='' ORDER BY read_flags, created_epoch %q", id, domain, msg_order);
	} else if (!strcasecmp(msg_type, "save")) {
		sql = switch_mprintf("SELECT uuid FROM voicemail_msgs WHERE username = '%q' AND domain = '%q' AND flags='save' ORDER BY read_flags, created_epoch %q", id, domain, msg_order);
	} else {
		sql = switch_mprintf("SELECT uuid FROM voicemail_msgs WHERE username = '%q' AND domain = '%q' AND read_epoch != 0 ORDER BY read_flags, created_epoch %q", id, domain, msg_order);
	}
	memset(&cbt, 0, sizeof(cbt));

	switch_event_create(&cbt.my_params, SWITCH_EVENT_REQUEST_PARAMS);

	vm_execute_sql_callback(profile, profile->mutex, sql, message_list_callback, &cbt);

	profile_rwunlock(profile);

	switch_event_add_header(cbt.my_params, SWITCH_STACK_BOTTOM, "VM-List-Count", "%"SWITCH_SIZE_T_FMT, cbt.len);
	switch_event_serialize_json(cbt.my_params, &ebuf);
	switch_event_destroy(&cbt.my_params);

	switch_safe_free(sql);
	stream->write_function(stream, "%s", ebuf);
	switch_safe_free(ebuf);
done:
	switch_core_destroy_memory_pool(&pool);
	return SWITCH_STATUS_SUCCESS;
}

#define VM_FSDB_MSG_PURGE_USAGE "<profile> <domain> <user>"
SWITCH_STANDARD_API(vm_fsdb_msg_purge_function)
{
	char *sql;
	const char *id = NULL, *domain = NULL, *profile_name = NULL;
	vm_profile_t *profile = NULL;

	char *argv[6] = { 0 };
	char *mycmd = NULL;

	switch_memory_pool_t *pool;

	switch_core_new_memory_pool(&pool);

	if (!zstr(cmd)) {
		mycmd = switch_core_strdup(pool, cmd);
		switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argv[0])
		profile_name = argv[0];
	if (argv[1])
		domain = argv[1];
	if (argv[2])
		id = argv[2];

	if (!profile_name || !domain || !id) {
		stream->write_function(stream, "-ERR Missing Arguments\n");
		goto done;
	}

	if (!(profile = get_profile(profile_name))) {
		stream->write_function(stream, "-ERR Profile not found\n");
		goto done;
	}

	sql = switch_mprintf("SELECT '%q', uuid, username, domain, file_path FROM voicemail_msgs WHERE username = '%q' AND domain = '%q' AND flags = 'delete'", profile_name, id, domain);
	vm_execute_sql_callback(profile, profile->mutex, sql, message_purge_callback, NULL);
	update_mwi(profile, id, domain, "inbox", MWI_REASON_PURGE); /* TODO Make inbox value configurable */

	profile_rwunlock(profile);

	stream->write_function(stream, "-OK\n");

done:
	switch_core_destroy_memory_pool(&pool);
	return SWITCH_STATUS_SUCCESS;
}

#define VM_FSDB_MSG_DELETE_USAGE "<profile> <domain> <user> <uuid>"
SWITCH_STANDARD_API(vm_fsdb_msg_delete_function)
{
	char *sql;
	const char *uuid = NULL;

	const char *id = NULL, *domain = NULL, *profile_name = NULL;
	vm_profile_t *profile = NULL;

	char *argv[6] = { 0 };
	char *mycmd = NULL;

	switch_memory_pool_t *pool;

	switch_core_new_memory_pool(&pool);

	if (!zstr(cmd)) {
		mycmd = switch_core_strdup(pool, cmd);
		switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argv[0])
		profile_name = argv[0];
	if (argv[1])
		domain = argv[1];
	if (argv[2])
		id = argv[2];
	if (argv[3])
		uuid = argv[3];

	if (!profile_name || !domain || !id || !uuid) {
		stream->write_function(stream, "-ERR Missing Arguments\n");
		goto done;
	}

	if (!(profile = get_profile(profile_name))) {
		stream->write_function(stream, "-ERR Profile not found\n");
		goto done;
	}

	sql = switch_mprintf("UPDATE voicemail_msgs SET flags = 'delete' WHERE username = '%q' AND domain = '%q' AND uuid = '%q'", id, domain, uuid);
	vm_execute_sql(profile, sql, profile->mutex);
	profile_rwunlock(profile);

	stream->write_function(stream, "-OK\n");

done:
	switch_core_destroy_memory_pool(&pool);
	return SWITCH_STATUS_SUCCESS;
}

#define VM_FSDB_MSG_SAVE_USAGE "<profile> <domain> <user> <uuid>"
SWITCH_STANDARD_API(vm_fsdb_msg_save_function)
{
	char *sql;
	const char *uuid = NULL;

	const char *id = NULL, *domain = NULL, *profile_name = NULL;
	vm_profile_t *profile = NULL;

	char *argv[6] = { 0 };
	char *mycmd = NULL;

	switch_memory_pool_t *pool;

	switch_core_new_memory_pool(&pool);

	if (!zstr(cmd)) {
		mycmd = switch_core_strdup(pool, cmd);
		switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argv[0])
		profile_name = argv[0];
	if (argv[1])
		domain = argv[1];
	if (argv[2])
		id = argv[2];
	if (argv[3])
		uuid = argv[3];

	if (!profile_name || !domain || !id || !uuid) {
		stream->write_function(stream, "-ERR Missing Arguments\n");
		goto done;
	}

	if (!(profile = get_profile(profile_name))) {
		stream->write_function(stream, "-ERR Profile not found\n");
		goto done;
	}

	sql = switch_mprintf("UPDATE voicemail_msgs SET flags='save' WHERE username='%q' AND domain='%q' AND uuid = '%q'", id, domain, uuid);
	vm_execute_sql(profile, sql, profile->mutex);
	profile_rwunlock(profile);

	stream->write_function(stream, "-OK\n");
done:
	switch_core_destroy_memory_pool(&pool);
	return SWITCH_STATUS_SUCCESS;
}

#define VM_FSDB_MSG_UNDELETE_USAGE "<profile> <domain> <user> <uuid>"
SWITCH_STANDARD_API(vm_fsdb_msg_undelete_function)
{
	char *sql;
	const char *uuid = NULL;

	const char *id = NULL, *domain = NULL, *profile_name = NULL;
	vm_profile_t *profile = NULL;

	char *argv[6] = { 0 };
	char *mycmd = NULL;

	switch_memory_pool_t *pool;

	switch_core_new_memory_pool(&pool);

	if (!zstr(cmd)) {
		mycmd = switch_core_strdup(pool, cmd);
		switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argv[0])
		profile_name = argv[0];
	if (argv[1])
		domain = argv[1];
	if (argv[2])
		id = argv[2];
	if (argv[3])
		uuid = argv[3];

	if (!profile_name || !domain || !id || !uuid) {
		stream->write_function(stream, "-ERR Missing Arguments\n");
		goto done;
	}

	if (!(profile = get_profile(profile_name))) {
		stream->write_function(stream, "-ERR Profile not found\n");
		goto done;
	}

	sql = switch_mprintf("UPDATE voicemail_msgs SET flags='' WHERE username='%q' AND domain='%q' AND uuid = '%q'", id, domain, uuid);
	vm_execute_sql(profile, sql, profile->mutex);
	profile_rwunlock(profile);
	
	stream->write_function(stream, "-OK\n");
done:
	switch_core_destroy_memory_pool(&pool);
	return SWITCH_STATUS_SUCCESS;
}

#define VM_FSDB_AUTH_LOGIN_USAGE "<profile> <domain> <user> <password>"
SWITCH_STANDARD_API(vm_fsdb_auth_login_function)
{
	char *sql;
	char *password = NULL;

	const char *id = NULL, *domain = NULL, *profile_name = NULL;
	vm_profile_t *profile = NULL;

	char *argv[6] = { 0 };
	char *mycmd = NULL;

	char user_db_password[64] = { 0 };
	const char *user_xml_password = NULL;
	switch_bool_t authorized = SWITCH_FALSE;
	switch_event_t *params = NULL;
	switch_xml_t x_user = NULL;
	switch_bool_t vm_enabled = SWITCH_TRUE;

	switch_memory_pool_t *pool;

	switch_core_new_memory_pool(&pool);

	if (!zstr(cmd)) {
		mycmd = switch_core_strdup(pool, cmd);
		switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argv[0])
		profile_name = argv[0];
	if (argv[1])
		domain = argv[1];
	if (argv[2])
		id = argv[2];
	if (argv[3])
		password = argv[3];

	if (!profile_name || !domain || !id || !password) {
		stream->write_function(stream, "-ERR Missing Arguments\n");
		goto done;
	}

	if (!(profile = get_profile(profile_name))) {
		stream->write_function(stream, "-ERR Profile not found\n");
		goto done;
	}

	switch_event_create(&params, SWITCH_EVENT_GENERAL);
	if (switch_xml_locate_user_merged("id:number-alias", id, domain, NULL, &x_user, params) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Can't find user [%s@%s]\n", id, domain);
		stream->write_function(stream, "-ERR User not found\n");
	} else {
		switch_xml_t x_param, x_params;

		x_params = switch_xml_child(x_user, "params");

		for (x_param = switch_xml_child(x_params, "param"); x_param; x_param = x_param->next) {
			const char *var = switch_xml_attr_soft(x_param, "name");
			const char *val = switch_xml_attr_soft(x_param, "value");
			if (zstr(var) || zstr(val)) {
				continue; /* Ignore empty entires */
			}

			if (!strcasecmp(var, "vm-enabled")) {
				vm_enabled = !switch_false(val);
			}					
			if (!strcasecmp(var, "vm-password")) {
				user_xml_password = val;
			}  
		}

		sql = switch_mprintf("SELECT password FROM voicemail_prefs WHERE username = '%q' AND domain = '%q'", id, domain);
		vm_execute_sql2str(profile, profile->mutex, sql, user_db_password, sizeof(user_db_password));
		switch_safe_free(sql);

		if (vm_enabled == SWITCH_FALSE) {
			stream->write_function(stream, "%s", "-ERR Login Denied");
		} else {
			if (!zstr(user_db_password)) {
				if (!strcasecmp(user_db_password, password)) {
					authorized = SWITCH_TRUE;
				}
				if (!profile->db_password_override && !zstr(user_xml_password) && !strcasecmp(user_xml_password, password)) {
					authorized = SWITCH_TRUE;
				}
			} else {
				if (!zstr(user_xml_password)) {
					if (!strcasecmp(user_xml_password, password)) {
						authorized = SWITCH_TRUE;
					}
				}
			}
			if (profile->allow_empty_password_auth && zstr(user_db_password) && zstr(user_xml_password)) {
				authorized = SWITCH_TRUE;
			}
			if (authorized) {
				stream->write_function(stream, "%s", "-OK");
			} else {
				stream->write_function(stream, "%s", "-ERR");
			}
		}
	}

	xml_safe_free(x_user);

	profile_rwunlock(profile);
done:
	switch_core_destroy_memory_pool(&pool);
	return SWITCH_STATUS_SUCCESS;
}

#define VM_FSDB_MSG_FORWARD_USAGE "<profile> <domain> <user> <uuid> <dst_domain> <dst_user> [prepend_file_location]"
SWITCH_STANDARD_API(vm_fsdb_msg_forward_function)
{
	const char *id = NULL, *domain = NULL, *profile_name = NULL, *uuid = NULL, *dst_domain = NULL, *dst_id = NULL, *prepend_file_path = NULL;
	vm_profile_t *profile = NULL;
	char *argv[7] = { 0 };
	char *mycmd = NULL;
	msg_get_callback_t cbt = { 0 };
	char *sql;
	switch_memory_pool_t *pool;

	switch_core_new_memory_pool(&pool);

	if (!zstr(cmd)) {
		mycmd = switch_core_strdup(pool, cmd);
		switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argv[0])
		profile_name = argv[0];
	if (argv[1])
		domain = argv[1];
	if (argv[2])
		id = argv[2];
	if (argv[3])
		uuid = argv[3];
	if (argv[4])
		dst_domain = argv[4];
	if (argv[5])
		dst_id = argv[5];
	if (argv[6])
		prepend_file_path = argv[6];

	if (!profile_name || !domain || !id || !uuid || !dst_domain || !dst_id) {
		stream->write_function(stream, "-ERR Missing Arguments\n");
		goto done;
	}

	if (!(profile = get_profile(profile_name))) {
		stream->write_function(stream, "-ERR Profile not found\n");
		goto done;
	} else {
		const char *file_path = NULL;
		sql = switch_mprintf("select created_epoch, read_epoch, username, domain, uuid, cid_name, cid_number, in_folder, file_path, message_len, flags, read_flags, forwarded_by from voicemail_msgs WHERE username = '%q' AND domain = '%q' AND uuid = '%q' ORDER BY read_flags, created_epoch", id, domain, uuid);
		memset(&cbt, 0, sizeof(cbt));
		switch_event_create(&cbt.my_params, SWITCH_EVENT_REQUEST_PARAMS);
		vm_execute_sql_callback(profile, profile->mutex, sql, message_get_callback, &cbt);
		switch_safe_free(sql);
		file_path = switch_event_get_header(cbt.my_params, "VM-Message-File-Path");
		if (file_path && switch_file_exists(file_path, pool) == SWITCH_STATUS_SUCCESS) {
			const char *new_file_path = file_path;
			const char *cmd = NULL;


			if (prepend_file_path && switch_file_exists(prepend_file_path, pool) == SWITCH_STATUS_SUCCESS) {
				switch_uuid_t tmp_uuid;
				char tmp_uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
				const char *test[3] = { NULL };
				test[0] = prepend_file_path;
				test[1] = file_path;

				switch_uuid_get(&tmp_uuid);
				switch_uuid_format(tmp_uuid_str, &tmp_uuid);

				new_file_path = switch_core_sprintf(pool, "%s%smsg_%s.wav", SWITCH_GLOBAL_dirs.temp_dir, SWITCH_PATH_SEPARATOR, tmp_uuid_str);
				
				if (vm_merge_media_files(test, new_file_path) != SWITCH_STATUS_SUCCESS) {
					stream->write_function(stream, "-ERR Error Merging the file\n");
					switch_event_destroy(&cbt.my_params);
					profile_rwunlock(profile);
					goto done;
				}

			}
			cmd = switch_core_sprintf(pool, "%s@%s %s %s '%s'", dst_id, dst_domain, new_file_path, switch_event_get_header(cbt.my_params, "VM-Message-Caller-Number"), switch_event_get_header(cbt.my_params, "VM-Message-Caller-Name"));
			if (voicemail_inject(cmd, NULL) == SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Sent Carbon Copy to %s@%s\n", dst_id, dst_domain);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to Carbon Copy to %s@%s\n", dst_id, dst_domain);
				stream->write_function(stream, "-ERR Error Forwarding Message\n");
				switch_event_destroy(&cbt.my_params);
				profile_rwunlock(profile);
				goto done;
			}
			if (new_file_path != file_path) {
				/* TODO UNLINK new-file-path */
			}
		} else {
			stream->write_function(stream, "-ERR Cannot find source msg to forward: %s\n", file_path);
			switch_event_destroy(&cbt.my_params);
			profile_rwunlock(profile);
			goto done;
		}

		switch_event_destroy(&cbt.my_params);

		profile_rwunlock(profile);
	}
	stream->write_function(stream, "-OK\n");
done:
	switch_core_destroy_memory_pool(&pool);


	return SWITCH_STATUS_SUCCESS;
}

#define VM_FSDB_MSG_GET_USAGE "<format> <profile> <domain> <user> <uuid>"
SWITCH_STANDARD_API(vm_fsdb_msg_get_function)
{
	char *sql;
	msg_get_callback_t cbt = { 0 };
	char *ebuf = NULL;
	char *uuid = NULL;

	const char *id = NULL, *domain = NULL, *profile_name = NULL;
	vm_profile_t *profile = NULL;

	char *argv[6] = { 0 };
	char *mycmd = NULL;

	switch_memory_pool_t *pool;

	switch_core_new_memory_pool(&pool);

	if (!zstr(cmd)) {
		mycmd = switch_core_strdup(pool, cmd);
		switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argv[1])
		profile_name = argv[1];
	if (argv[2])
		domain = argv[2];
	if (argv[3])
		id = argv[3];
	if (argv[4])
		uuid = argv[4];

	if (!profile_name || !domain || !id || !uuid) {
		stream->write_function(stream, "-ERR Missing Arguments\n");
		goto done;
	}

	if (!(profile = get_profile(profile_name))) {
		stream->write_function(stream, "-ERR Profile not found\n");
		goto done;
	}

	sql = switch_mprintf("select created_epoch, read_epoch, username, domain, uuid, cid_name, cid_number, in_folder, file_path, message_len, flags, read_flags, forwarded_by from voicemail_msgs WHERE username = '%q' AND domain = '%q' AND uuid = '%q' ORDER BY read_flags, created_epoch", id, domain, uuid);

	memset(&cbt, 0, sizeof(cbt));

	switch_event_create(&cbt.my_params, SWITCH_EVENT_REQUEST_PARAMS);

	vm_execute_sql_callback(profile, profile->mutex, sql, message_get_callback, &cbt);

	profile_rwunlock(profile);

	switch_event_serialize_json(cbt.my_params, &ebuf);
	switch_event_destroy(&cbt.my_params);

	switch_safe_free(sql);
	stream->write_function(stream, "%s", ebuf);
	switch_safe_free(ebuf);
done:
	switch_core_destroy_memory_pool(&pool);
	return SWITCH_STATUS_SUCCESS;
}

#define VM_FSDB_MSG_EMAIL_USAGE "<profile> <domain> <user> <uuid> <email>"
SWITCH_STANDARD_API(vm_fsdb_msg_email_function)
{
	const char *id = NULL, *domain = NULL, *profile_name = NULL, *uuid = NULL, *email = NULL;
	vm_profile_t *profile = NULL;
	char *argv[7] = { 0 };
	char *mycmd = NULL;
	msg_get_callback_t cbt = { 0 };
	char *sql;
	switch_memory_pool_t *pool;

	switch_core_new_memory_pool(&pool);

	if (!zstr(cmd)) {
		mycmd = switch_core_strdup(pool, cmd);
		switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argv[0])
		profile_name = argv[0];
	if (argv[1])
		domain = argv[1];
	if (argv[2])
		id = argv[2];
	if (argv[3])
		uuid = argv[3];
	if (argv[4])
		email = argv[4];

	if (!profile_name || !domain || !id || !uuid || !email) {
		stream->write_function(stream, "-ERR Missing Arguments\n");
		goto done;
	}

	if (!(profile = get_profile(profile_name))) {
		stream->write_function(stream, "-ERR Profile not found\n");
		goto done;
	} else {
		char *from;
		char *headers, *header_string;
		char *body;
		int priority = 3;
		switch_size_t retsize;
		switch_time_exp_t tm;
		char date[80] = "";
		int total_new_messages = 0;
		int total_saved_messages = 0;
		int total_new_urgent_messages = 0;
		int total_saved_urgent_messages = 0;
		int32_t message_len = 0;
		char *p;
		switch_time_t l_duration = 0;
		switch_core_time_duration_t duration;
		char duration_str[80];
		char *formatted_cid_num = NULL;

		sql = switch_mprintf("select created_epoch, read_epoch, username, domain, uuid, cid_name, cid_number, in_folder, file_path, message_len, flags, read_flags, forwarded_by from voicemail_msgs WHERE username = '%q' AND domain = '%q' AND uuid = '%q' ORDER BY read_flags, created_epoch", id, domain, uuid);
		memset(&cbt, 0, sizeof(cbt));
		switch_event_create(&cbt.my_params, SWITCH_EVENT_GENERAL);
		vm_execute_sql_callback(profile, profile->mutex, sql, message_get_callback, &cbt);
		switch_safe_free(sql);

		if (!strcasecmp(switch_event_get_header(cbt.my_params, "VM-Message-Read-Flags"), URGENT_FLAG_STRING)) {
			priority = 1;
		}

		message_count(profile, id, domain, switch_event_get_header(cbt.my_params, "VM-Message-Folder"), &total_new_messages, &total_saved_messages,
				&total_new_urgent_messages, &total_saved_urgent_messages);

		switch_time_exp_lt(&tm, switch_time_make(atol(switch_event_get_header(cbt.my_params, "VM-Message-Received-Epoch")), 0));
		switch_strftime(date, &retsize, sizeof(date), profile->date_fmt, &tm);

		formatted_cid_num = switch_format_number(switch_event_get_header(cbt.my_params, "VM-Message-Caller-Number"));

		/* Legacy Mod_VoiceMail variable */
		switch_event_add_header_string(cbt.my_params, SWITCH_STACK_BOTTOM, "Message-Type", "forwarded-voicemail");
		switch_event_add_header(cbt.my_params, SWITCH_STACK_BOTTOM, "voicemail_total_new_messages", "%d", total_new_messages);
		switch_event_add_header(cbt.my_params, SWITCH_STACK_BOTTOM, "voicemail_total_saved_messages", "%d", total_saved_messages);
		switch_event_add_header(cbt.my_params, SWITCH_STACK_BOTTOM, "voicemail_urgent_new_messages", "%d", total_new_urgent_messages);
		switch_event_add_header(cbt.my_params, SWITCH_STACK_BOTTOM, "voicemail_urgent_saved_messages", "%d", total_saved_urgent_messages);
		switch_event_add_header_string(cbt.my_params, SWITCH_STACK_BOTTOM, "voicemail_current_folder", switch_event_get_header(cbt.my_params, "VM-Message-Folder"));
		switch_event_add_header_string(cbt.my_params, SWITCH_STACK_BOTTOM, "voicemail_account", id);
		switch_event_add_header_string(cbt.my_params, SWITCH_STACK_BOTTOM, "voicemail_domain", domain);
		switch_event_add_header_string(cbt.my_params, SWITCH_STACK_BOTTOM, "voicemail_caller_id_number", switch_event_get_header(cbt.my_params, "VM-Message-Caller-Number"));
		switch_event_add_header_string(cbt.my_params, SWITCH_STACK_BOTTOM, "voicemail_formatted_caller_id_number", formatted_cid_num);
		switch_event_add_header_string(cbt.my_params, SWITCH_STACK_BOTTOM, "voicemail_caller_id_name", switch_event_get_header(cbt.my_params, "VM-Message-Caller-Name"));
		switch_event_add_header_string(cbt.my_params, SWITCH_STACK_BOTTOM, "voicemail_file_path", switch_event_get_header(cbt.my_params, "VM-Message-File-Path"));
		switch_event_add_header_string(cbt.my_params, SWITCH_STACK_BOTTOM, "voicemail_read_flags", switch_event_get_header(cbt.my_params, "VM-Message-Read-Flags"));
		switch_event_add_header_string(cbt.my_params, SWITCH_STACK_BOTTOM, "voicemail_time", date);
		switch_event_add_header(cbt.my_params, SWITCH_STACK_BOTTOM, "voicemail_priority", "%d", priority);


		message_len = atoi(switch_event_get_header(cbt.my_params, "VM-Message-Duration"));
		switch_safe_free(formatted_cid_num);

		l_duration = switch_time_make(atol(switch_event_get_header(cbt.my_params, "VM-Message-Duration")), 0);
		switch_core_measure_time(l_duration, &duration);
		duration.day += duration.yr * 365;
		duration.hr += duration.day * 24;

		switch_snprintf(duration_str, sizeof(duration_str), "%.2u:%.2u:%.2u", duration.hr, duration.min, duration.sec);

		switch_event_add_header_string(cbt.my_params, SWITCH_STACK_BOTTOM, "voicemail_message_len", duration_str);
		switch_event_add_header_string(cbt.my_params, SWITCH_STACK_BOTTOM, "voicemail_email", email);

		if (zstr(profile->email_from)) {
			from = switch_core_sprintf(pool, "%s@%s", id, domain);
		} else {
			from = switch_event_expand_headers(cbt.my_params, profile->email_from);;
		}

		if (zstr(profile->email_headers)) {
			headers = switch_core_sprintf(pool,
					"From: FreeSWITCH mod_voicemail <%s@%s>\nSubject: Voicemail from %s %s\nX-Priority: %d",
					id, domain, switch_event_get_header(cbt.my_params, "VM-Message-Caller-Name"), 
					switch_event_get_header(cbt.my_params, "VM-Message-Caller-Number"), priority);
		} else {
			headers = switch_event_expand_headers(cbt.my_params, profile->email_headers);
		}

		p = headers + (strlen(headers) - 1);
		if (*p == '\n') {
			if (*(p - 1) == '\r') {
				p--;
			}
			*p = '\0';
		}

		header_string = switch_core_sprintf(pool, "%s\nX-Voicemail-Length: %u", headers, message_len);

		if (profile->email_body) {
			body = switch_event_expand_headers(cbt.my_params, profile->email_body);
		} else {
			body = switch_mprintf("%u second Voicemail from %s %s", message_len, switch_event_get_header(cbt.my_params, "VM-Message-Caller-Name"), switch_event_get_header(cbt.my_params, "VM-Message-Caller-Number"));
		}

		switch_simple_email(email, from, header_string, body, switch_event_get_header(cbt.my_params, "VM-Message-File-Path"), profile->convert_cmd, profile->convert_ext);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Sending message to %s\n", email);
		switch_safe_free(body);

		switch_event_fire(&cbt.my_params);


		profile_rwunlock(profile);
	}
	stream->write_function(stream, "-OK\n");
done:
	switch_core_destroy_memory_pool(&pool);

	return SWITCH_STATUS_SUCCESS;
}

#define VM_FSDB_MSG_COUNT_USAGE "<format> <profile> <domain> <user> <folder>"
SWITCH_STANDARD_API(vm_fsdb_msg_count_function)
{
	char *sql;
	msg_cnt_callback_t cbt = { 0 };
	switch_event_t *my_params = NULL;
	char *ebuf = NULL;

	const char *id = NULL, *domain = NULL, *profile_name = NULL, *folder = NULL;
	vm_profile_t *profile = NULL;

	char *argv[6] = { 0 };
	char *mycmd = NULL;

	switch_memory_pool_t *pool;

	switch_core_new_memory_pool(&pool);

	if (!zstr(cmd)) {
		mycmd = switch_core_strdup(pool, cmd);
		switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argv[1])
		profile_name = argv[1];
	if (argv[2])
		domain = argv[2];
	if (argv[3])
		id = argv[3];
	if (argv[4])
		folder = argv[4];

	if (!profile_name || !domain || !id || !folder) {
		stream->write_function(stream, "-ERR Missing Arguments\n");
		goto done;
	}

	if (!(profile = get_profile(profile_name))) {
		stream->write_function(stream, "-ERR Profile not found\n");
		goto done;
	}

	sql = switch_mprintf(
			"SELECT 1, read_flags, count(read_epoch) FROM voicemail_msgs WHERE username = '%q' AND domain = '%q' AND in_folder = '%q' AND flags = '' AND in_folder = '%q' GROUP BY read_flags "
			"UNION "
			"SELECT 0, read_flags, count(read_epoch) FROM voicemail_msgs WHERE username = '%q' AND domain = '%q' AND in_folder = '%q' AND flags = 'save' AND in_folder= '%q' GROUP BY read_flags;",
			id, domain, "inbox", folder,
			id, domain, "inbox", folder);


	vm_execute_sql_callback(profile, profile->mutex, sql, message_count_callback, &cbt);

	profile_rwunlock(profile);

	switch_event_create(&my_params, SWITCH_EVENT_REQUEST_PARAMS);
	switch_event_add_header(my_params, SWITCH_STACK_BOTTOM, "VM-Total-New-Messages", "%d", cbt.total_new_messages + cbt.total_new_urgent_messages);
	switch_event_add_header(my_params, SWITCH_STACK_BOTTOM, "VM-Total-New-Urgent-Messages", "%d", cbt.total_new_urgent_messages);
	switch_event_add_header(my_params, SWITCH_STACK_BOTTOM, "VM-Total-Saved-Messages", "%d", cbt.total_saved_messages + cbt.total_saved_urgent_messages);
	switch_event_add_header(my_params, SWITCH_STACK_BOTTOM, "VM-Total-Saved-Urgent-Messages", "%d", cbt.total_saved_urgent_messages);
	switch_event_serialize_json(my_params, &ebuf);
	switch_event_destroy(&my_params);

	switch_safe_free(sql);
	stream->write_function(stream, "%s", ebuf);
	switch_safe_free(ebuf);
done:
	switch_core_destroy_memory_pool(&pool);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_voicemail_load)
{
	switch_application_interface_t *app_interface;
	switch_api_interface_t *commands_api_interface;
	switch_status_t status;

	/* create/register custom event message type */
	if (switch_event_reserve_subclass(VM_EVENT_MAINT) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass %s!\n", VM_EVENT_MAINT);
		return SWITCH_STATUS_TERM;
	}

	memset(&globals, 0, sizeof(globals));
	globals.pool = pool;

	switch_core_hash_init(&globals.profile_hash);
	switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, globals.pool);

	switch_mutex_lock(globals.mutex);
	globals.running = 1;
	switch_mutex_unlock(globals.mutex);

	switch_queue_create(&globals.event_queue, VM_EVENT_QUEUE_SIZE, globals.pool);

	if ((status = load_config()) != SWITCH_STATUS_SUCCESS) {
		globals.running = 0;
		return status;
	}
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	if (switch_event_bind(modname, SWITCH_EVENT_MESSAGE_QUERY, SWITCH_EVENT_SUBCLASS_ANY, vm_event_handler, NULL)
		!= SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_GENERR;
	}

	SWITCH_ADD_APP(app_interface, "voicemail", "Voicemail", VM_DESC, voicemail_function, VM_USAGE, SAF_NONE);
	SWITCH_ADD_API(commands_api_interface, "voicemail", "voicemail", voicemail_api_function, VOICEMAIL_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "voicemail_inject", "voicemail_inject", voicemail_inject_api_function, VM_INJECT_USAGE);
	SWITCH_ADD_API(commands_api_interface, "vm_inject", "vm_inject", voicemail_inject_api_function, VM_INJECT_USAGE);
	SWITCH_ADD_API(commands_api_interface, "vm_boxcount", "vm_boxcount", boxcount_api_function, BOXCOUNT_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "vm_prefs", "vm_prefs", prefs_api_function, PREFS_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "vm_delete", "vm_delete", voicemail_delete_api_function, VM_DELETE_USAGE);
	SWITCH_ADD_API(commands_api_interface, "vm_read", "vm_read", voicemail_read_api_function, VM_READ_USAGE);
	SWITCH_ADD_API(commands_api_interface, "vm_list", "vm_list", voicemail_list_api_function, VM_LIST_USAGE);

	/* Auth API */
	SWITCH_ADD_API(commands_api_interface, "vm_fsdb_auth_login", "vm_fsdb_auth_login", vm_fsdb_auth_login_function, VM_FSDB_AUTH_LOGIN_USAGE);

	/* Message Targeted API */
	SWITCH_ADD_API(commands_api_interface, "vm_fsdb_msg_count", "vm_fsdb_msg_count", vm_fsdb_msg_count_function, VM_FSDB_MSG_COUNT_USAGE);
	SWITCH_ADD_API(commands_api_interface, "vm_fsdb_msg_list", "vm_fsdb_msg_list", vm_fsdb_msg_list_function, VM_FSDB_MSG_LIST_USAGE);
	SWITCH_ADD_API(commands_api_interface, "vm_fsdb_msg_get", "vm_fsdb_msg_get", vm_fsdb_msg_get_function, VM_FSDB_MSG_GET_USAGE);
	SWITCH_ADD_API(commands_api_interface, "vm_fsdb_msg_delete", "vm_fsdb_msg_delete", vm_fsdb_msg_delete_function, VM_FSDB_MSG_DELETE_USAGE);
	SWITCH_ADD_API(commands_api_interface, "vm_fsdb_msg_undelete", "vm_fsdb_msg_undelete", vm_fsdb_msg_undelete_function, VM_FSDB_MSG_UNDELETE_USAGE);
	SWITCH_ADD_API(commands_api_interface, "vm_fsdb_msg_email", "vm_fsdb_msg_email", vm_fsdb_msg_email_function, VM_FSDB_MSG_EMAIL_USAGE);
	SWITCH_ADD_API(commands_api_interface, "vm_fsdb_msg_purge", "vm_fsdb_msg_purge", vm_fsdb_msg_purge_function, VM_FSDB_MSG_PURGE_USAGE);
	SWITCH_ADD_API(commands_api_interface, "vm_fsdb_msg_save", "vm_fsdb_msg_save", vm_fsdb_msg_save_function, VM_FSDB_MSG_SAVE_USAGE);
	SWITCH_ADD_API(commands_api_interface, "vm_fsdb_msg_forward", "vm_fsdb_msg_forward", vm_fsdb_msg_forward_function, VM_FSDB_MSG_FORWARD_USAGE);

	/* Preferences */
	SWITCH_ADD_API(commands_api_interface, "vm_fsdb_pref_greeting_set", "vm_fsdb_pref_greeting_set", vm_fsdb_pref_greeting_set_function, VM_FSDB_PREF_GREETING_SET_USAGE);
	SWITCH_ADD_API(commands_api_interface, "vm_fsdb_pref_greeting_get", "vm_fsdb_pref_greeting_get", vm_fsdb_pref_greeting_get_function, VM_FSDB_PREF_GREETING_GET_USAGE);
	SWITCH_ADD_API(commands_api_interface, "vm_fsdb_pref_recname_set", "vm_fsdb_pref_recname_set", vm_fsdb_pref_recname_set_function, VM_FSDB_PREF_RECNAME_SET_USAGE);
	SWITCH_ADD_API(commands_api_interface, "vm_fsdb_pref_password_set", "vm_fsdb_pref_password_set", vm_fsdb_pref_password_set_function, VM_FSDB_PREF_PASSWORD_SET_USAGE);

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_voicemail_shutdown)
{
	switch_hash_index_t *hi = NULL;
	vm_profile_t *profile;
	void *val = NULL;
	const void *key;
	switch_ssize_t keylen;
	int sanity = 0;

	switch_mutex_lock(globals.mutex);
	if (globals.running == 1) {
		globals.running = 0;
	}
	switch_mutex_unlock(globals.mutex);

	switch_event_free_subclass(VM_EVENT_MAINT);
	switch_event_unbind_callback(vm_event_handler);

	while (globals.threads) {
		switch_cond_next();
		if (++sanity >= 60000) {
			break;
		}
	}

	switch_mutex_lock(globals.mutex);
	while ((hi = switch_core_hash_first_iter( globals.profile_hash, hi))) {
		switch_core_hash_this(hi, &key, &keylen, &val);
		profile = (vm_profile_t *) val;

		switch_core_hash_delete(globals.profile_hash, profile->name);

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Waiting for write lock (Profile %s)\n", profile->name);
		switch_thread_rwlock_wrlock(profile->rwlock);

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Destroying Profile %s\n", profile->name);

		switch_core_destroy_memory_pool(&profile->pool);
		profile = NULL;
	}
	switch_core_hash_destroy(&globals.profile_hash);
	switch_mutex_unlock(globals.mutex);
	switch_mutex_destroy(globals.mutex);

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

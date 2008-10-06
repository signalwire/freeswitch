/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
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
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthmct@yahoo.com>
 * Bret McDanel <trixter AT 0xdecafbad.com>
 * John Wehle (john@feith.com)
 * Raymond Chandler <intralanman@gmail.com>
 *
 * mod_voicemail.c -- Voicemail Module
 *
 */
#include <switch.h>

#ifdef SWITCH_HAVE_ODBC
#include <switch_odbc.h>
#endif

#ifdef _MSC_VER					/* compilers are stupid sometimes */
#define TRY_CODE(code) for(;;) {status = code; if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_BREAK) { goto end; } break;}
#else
#define TRY_CODE(code) do { status = code; if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_BREAK) { goto end; } break;} while(status)
#endif

SWITCH_MODULE_LOAD_FUNCTION(mod_voicemail_load);
SWITCH_MODULE_DEFINITION(mod_voicemail, mod_voicemail_load, NULL, NULL);

#define VM_MAX_GREETINGS 9

static switch_status_t voicemail_inject(const char *data);

static struct {
	switch_hash_t *profile_hash;
	int debug;
	switch_memory_pool_t *pool;
} globals;

typedef enum {
	VM_DATE_FIRST,
	VM_DATE_LAST,
	VM_DATE_NEVER
} date_location_t;

struct vm_profile {
	char *name;
	char *dbname;
	char *odbc_dsn;
	char *odbc_user;
	char *odbc_pass;
	char terminator_key[2];
	char play_new_messages_key[2];
	char play_saved_messages_key[2];

	char main_menu_key[2];
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
	date_location_t play_date_announcement;
	uint32_t digit_timeout;
	uint32_t max_login_attempts;
	uint32_t min_record_len;
	uint32_t max_record_len;
	switch_mutex_t *mutex;
	uint32_t record_threshold;
	uint32_t record_silence_hits;
	uint32_t record_sample_rate;
	switch_odbc_handle_t *master_odbc;
	switch_bool_t auto_playback_recordings;
};
typedef struct vm_profile vm_profile_t;

static switch_status_t vm_execute_sql(vm_profile_t *profile, char *sql, switch_mutex_t *mutex)
{
	switch_core_db_t *db;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (mutex) {
		switch_mutex_lock(mutex);
	}

	if (profile->odbc_dsn) {
#ifdef SWITCH_HAVE_ODBC
		SQLHSTMT stmt;
		if (switch_odbc_handle_exec(profile->master_odbc, sql, &stmt) != SWITCH_ODBC_SUCCESS) {
			char *err_str;
			err_str = switch_odbc_handle_get_error(profile->master_odbc, stmt);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ERR: [%s]\n[%s]\n", sql, switch_str_nil(err_str));
			switch_safe_free(err_str);
			status = SWITCH_STATUS_FALSE;
		}
		SQLFreeHandle(SQL_HANDLE_STMT, stmt);
#endif
	} else {
		if (!(db = switch_core_db_open_file(profile->dbname))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB %s\n", profile->dbname);
			status = SWITCH_STATUS_FALSE;
			goto end;
		}
		status = switch_core_db_persistant_execute(db, sql, 1);
		switch_core_db_close(db);
	}

  end:
	if (mutex) {
		switch_mutex_unlock(mutex);
	}
	return status;
}


static switch_bool_t vm_execute_sql_callback(vm_profile_t *profile, switch_mutex_t *mutex, char *sql, switch_core_db_callback_func_t callback, void *pdata)
{
	switch_bool_t ret = SWITCH_FALSE;
	switch_core_db_t *db;
	char *errmsg = NULL;

	if (mutex) {
		switch_mutex_lock(mutex);
	}

	if (profile->odbc_dsn) {
#ifdef SWITCH_HAVE_ODBC
		switch_odbc_handle_callback_exec(profile->master_odbc, sql, callback, pdata);
#endif
	} else {
		if (!(db = switch_core_db_open_file(profile->dbname))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB %s\n", profile->dbname);
			goto end;
		}

		switch_core_db_exec(db, sql, callback, pdata, &errmsg);

		if (errmsg) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "SQL ERR: [%s] %s\n", sql, errmsg);
			free(errmsg);
		}

		if (db) {
			switch_core_db_close(db);
		}
	}

  end:
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
	"   message_len   INTEGER,\n" 
	"   flags         VARCHAR(255),\n" 
	"   read_flags    VARCHAR(255)\n" 
	");\n";

static char vm_pref_sql[] =
	"CREATE TABLE voicemail_prefs (\n"
	"   username        VARCHAR(255),\n"
	"   domain          VARCHAR(255),\n" 
	"   name_path       VARCHAR(255),\n" 
	"   greeting_path   VARCHAR(255),\n" 
	"   password        VARCHAR(255)\n" 
	");\n";

static switch_status_t load_config(void)
{
	char *cf = "voicemail.conf";
	vm_profile_t *profile = NULL;
	switch_xml_t cfg, xml, settings, param, x_profile, x_profiles, x_email;

	memset(&globals, 0, sizeof(globals));
	switch_core_new_memory_pool(&globals.pool);
	switch_core_hash_init(&globals.profile_hash, globals.pool);

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcasecmp(var, "debug")) {
				globals.debug = atoi(val);
			}
		}
	}

	if (!(x_profiles = switch_xml_child(cfg, "profiles"))) {
		goto end;
	}

	for (x_profile = switch_xml_child(x_profiles, "profile"); x_profile; x_profile = x_profile->next) {
		char *name = (char *) switch_xml_attr_soft(x_profile, "name");
		char *odbc_dsn = NULL, *odbc_user = NULL, *odbc_pass = NULL;
		char *terminator_key = "#";
		char *play_new_messages_key = "1";
		char *play_saved_messages_key = "2";

		char *main_menu_key = "0";
		char *config_menu_key = "5";
		char *record_greeting_key = "1";
		char *choose_greeting_key = "2";
		char *record_name_key = "3";
		char *change_pass_key = "6";

		char *record_file_key = "3";
		char *listen_file_key = "1";
		char *save_file_key = "2";
		char *delete_file_key = "7";
		char *undelete_file_key = "8";
		char *email_key = "4";
		char *callback_key = "5";
		char *pause_key = "0";
		char *restart_key = "1";
		char *ff_key = "6";
		char *rew_key = "4";
		char *urgent_key = "*";
		char *operator_key = "";
		char *operator_ext = "";
		char *vmain_key = "";
		char *vmain_ext = "";
		char *forward_key = "8";
		char *prepend_key = "1";
		char *tone_spec = "%(1000, 0, 640)";
		char *file_ext = "wav";
		char *storage_dir = "";
		char *callback_dialplan = "XML";
		char *callback_context = "default";
		char *email_body = NULL;
		char *email_headers = NULL;
		char *notify_email_body = NULL;
		char *notify_email_headers = NULL;
		char *email_from = "";
		char *date_fmt = "%A, %B %d %Y, %I %M %p";
		date_location_t play_date_announcement = VM_DATE_FIRST;
		char *web_head = NULL;
		char *web_tail = NULL;
		uint32_t record_threshold = 200;
		uint32_t record_silence_hits = 2;
		uint32_t record_sample_rate = 0;

		char *record_title = "FreeSWITCH Voicemail";
		char *record_comment = "FreeSWITCH Voicemail";
		char *record_copyright = "http://www.freeswitch.org";

		switch_bool_t auto_playback_recordings = SWITCH_TRUE;

		switch_core_db_t *db;
		uint32_t timeout = 10000, max_login_attempts = 3, max_record_len = 300, min_record_len = 3;

		db = NULL;

		if ((x_email = switch_xml_child(x_profile, "email"))) {
			if ((param = switch_xml_child(x_email, "body"))) {
				email_body = switch_core_strdup(globals.pool, param->txt);
			}

			if ((param = switch_xml_child(x_email, "headers"))) {
				email_headers = switch_core_strdup(globals.pool, param->txt);
			}

			for (param = switch_xml_child(x_email, "param"); param; param = param->next) {
				char *var, *val;

				var = (char *) switch_xml_attr_soft(param, "name");
				val = (char *) switch_xml_attr_soft(param, "value");

				if (!strcasecmp(var, "date-fmt") && !switch_strlen_zero(val)) {
					date_fmt = val;
				} else if (!strcasecmp(var, "email-from") && !switch_strlen_zero(val)) {
					email_from = val;
				} else if (!strcasecmp(var, "template-file") && !switch_strlen_zero(val)) {
					switch_stream_handle_t stream = { 0 };
					int fd;
					char *dpath = NULL;
					char *path;

					if (switch_is_file_path(val)) {
						path = val;
					} else {
						dpath = switch_mprintf("%s%s%s", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, val);
						path = dpath;
					}

					if ((fd = open(path, O_RDONLY)) > -1) {
						char buf[2048];
						SWITCH_STANDARD_STREAM(stream);
						while (switch_fd_read_line(fd, buf, sizeof(buf))) {
							stream.write_function(&stream, "%s", buf);
						}
						close(fd);
						email_headers = stream.data;
						if ((email_body = strstr(email_headers, "\n\n"))) {
							*email_body = '\0';
							email_body += 2;
						} else if ((email_body = strstr(email_headers, "\r\n\r\n"))) {
							*email_body = '\0';
							email_body += 4;
						}
					}
					switch_safe_free(dpath);
				} else if (!strcasecmp(var, "notify-template-file") && !switch_strlen_zero(val)) {
					switch_stream_handle_t stream = { 0 };
					int fd;
					char *dpath = NULL;
					char *path;

					if (switch_is_file_path(val)) {
						path = val;
					} else {
						dpath = switch_mprintf("%s%s%s", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, val);
						path = dpath;
					}

					if ((fd = open(path, O_RDONLY)) > -1) {
						char buf[2048];
						SWITCH_STANDARD_STREAM(stream);
						while (switch_fd_read_line(fd, buf, sizeof(buf))) {
							stream.write_function(&stream, "%s", buf);
						}
						close(fd);
						notify_email_headers = stream.data;
						if ((notify_email_body = strstr(notify_email_headers, "\n\n"))) {
							*notify_email_body = '\0';
							notify_email_body += 2;
						} else if ((notify_email_body = strstr(notify_email_headers, "\r\n\r\n"))) {
							*notify_email_body = '\0';
							notify_email_body += 4;
						}
					}
					switch_safe_free(dpath);
				}
			}
		}

		for (param = switch_xml_child(x_profile, "param"); param; param = param->next) {
			char *var, *val;

			var = (char *) switch_xml_attr_soft(param, "name");
			val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcasecmp(var, "terminator-key") && !switch_strlen_zero(val)) {
				terminator_key = val;
			} else if (!strcasecmp(var, "web-template-file") && !switch_strlen_zero(val)) {
				switch_stream_handle_t stream = { 0 };
				int fd;
				char *dpath = NULL;
				char *path;

				if (switch_is_file_path(val)) {
					path = val;
				} else {
					dpath = switch_mprintf("%s%s%s", SWITCH_GLOBAL_dirs.conf_dir, SWITCH_PATH_SEPARATOR, val);
					path = dpath;
				}

				if ((fd = open(path, O_RDONLY)) > -1) {
					char buf[2048];
					SWITCH_STANDARD_STREAM(stream);
					while (switch_fd_read_line(fd, buf, sizeof(buf))) {
						stream.write_function(&stream, "%s", buf);
					}
					close(fd);
					web_head = stream.data;
					if ((web_tail = strstr(web_head, "<!break>\n"))) {
						*web_tail = '\0';
						web_tail += 9;
					} else if ((web_tail = strstr(web_head, "<!break>\r\n"))) {
						*web_tail = '\0';
						web_tail += 10;
					}
				}
				switch_safe_free(dpath);
			} else if (!strcasecmp(var, "play-new-messages-key") && !switch_strlen_zero(val)) {
				play_new_messages_key = val;
			} else if (!strcasecmp(var, "play-saved-messages-key") && !switch_strlen_zero(val)) {
				play_saved_messages_key = val;
			} else if (!strcasecmp(var, "main-menu-key") && !switch_strlen_zero(val)) {
				main_menu_key = val;
			} else if (!strcasecmp(var, "config-menu-key") && !switch_strlen_zero(val)) {
				config_menu_key = val;
			} else if (!strcasecmp(var, "record-greeting-key") && !switch_strlen_zero(val)) {
				record_greeting_key = val;
			} else if (!strcasecmp(var, "choose-greeting-key") && !switch_strlen_zero(val)) {
				choose_greeting_key = val;
			} else if (!strcasecmp(var, "record-name-key") && !switch_strlen_zero(val)) {
				record_name_key = val;
			} else if (!strcasecmp(var, "change-pass-key") && !switch_strlen_zero(val)) {
				change_pass_key = val;
			} else if (!strcasecmp(var, "listen-file-key") && !switch_strlen_zero(val)) {
				listen_file_key = val;
			} else if (!strcasecmp(var, "save-file-key") && !switch_strlen_zero(val)) {
				save_file_key = val;
			} else if (!strcasecmp(var, "delete-file-key") && !switch_strlen_zero(val)) {
				delete_file_key = val;
			} else if (!strcasecmp(var, "undelete-file-key") && !switch_strlen_zero(val)) {
				undelete_file_key = val;
			} else if (!strcasecmp(var, "email-key") && !switch_strlen_zero(val)) {
				email_key = val;
			} else if (!strcasecmp(var, "callback-key") && !switch_strlen_zero(val)) {
				callback_key = val;
			} else if (!strcasecmp(var, "pause-key") && !switch_strlen_zero(val)) {
				pause_key = val;
			} else if (!strcasecmp(var, "restart-key") && !switch_strlen_zero(val)) {
				restart_key = val;
			} else if (!strcasecmp(var, "ff-key") && !switch_strlen_zero(val)) {
				ff_key = val;
			} else if (!strcasecmp(var, "rew-key") && !switch_strlen_zero(val)) {
				rew_key = val;
			} else if (!strcasecmp(var, "urgent-key") && !switch_strlen_zero(val)) {
				urgent_key = val;
			} else if (!strcasecmp(var, "operator-key") && !switch_strlen_zero(val)) {
				operator_key = val;
			} else if (!strcasecmp(var, "operator-extension") && !switch_strlen_zero(val)) {
				operator_ext = val;
			} else if (!strcasecmp(var, "vmain-key") && !switch_strlen_zero(val)) {
				vmain_key = val;
			} else if (!strcasecmp(var, "forward-key") && !switch_strlen_zero(val)) {
				forward_key = val;
			} else if (!strcasecmp(var, "prepend-key") && !switch_strlen_zero(val)) {
				prepend_key = val;
			} else if (!strcasecmp(var, "vmain-extension") && !switch_strlen_zero(val)) {
				vmain_ext = val;
			} else if (!strcasecmp(var, "storage-dir") && !switch_strlen_zero(val)) {
				storage_dir = val;
			} else if (!strcasecmp(var, "callback-dialplan") && !switch_strlen_zero(val)) {
				callback_dialplan = val;
			} else if (!strcasecmp(var, "callback-context") && !switch_strlen_zero(val)) {
				callback_context = val;
			} else if (!strcasecmp(var, "file-extension") && !switch_strlen_zero(val)) {
				file_ext = val;
			} else if (!strcasecmp(var, "play-date-announcement") && !switch_strlen_zero(val)) {
				if (!strcmp("last", val)) {
					play_date_announcement = VM_DATE_LAST;
				} else if (!strcmp("never", val)) {
					play_date_announcement = VM_DATE_NEVER;
				}
			} else if (!strcasecmp(var, "record-title") && !switch_strlen_zero(val)) {
				record_title = val;
			} else if (!strcasecmp(var, "record-comment") && !switch_strlen_zero(val)) {
				record_comment = val;
			} else if (!strcasecmp(var, "record-copyright") && !switch_strlen_zero(val)) {
				record_copyright = val;
			} else if (!strcasecmp(var, "record-silence-threshold")) {
				int tmp = 0;
				if (!switch_strlen_zero(val)) {
					tmp = atoi(val);
				}
				if (tmp >= 0 && tmp <= 10000) {
					record_threshold = tmp;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "invalid threshold value [%s] must be between 0 and 10000 ms\n", val);
				}
			} else if (!strcasecmp(var, "record-sample-rate")) {
				int tmp = 0;
				if (!switch_strlen_zero(val)) {
					tmp = atoi(val);
				}
				if (tmp == 8000 || tmp == 16000 || tmp == 32000 || tmp == 11025 || tmp == 22050 || tmp == 44100) {
					record_sample_rate = tmp;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "invalid samplerate %s\n", val);
				}
			} else if (!strcasecmp(var, "record-silence-hits")) {
				int tmp = 0;
				if (!switch_strlen_zero(val)) {
					tmp = atoi(val);
				}
				if (tmp >= 0 && tmp <= 1000) {
					record_silence_hits = tmp;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "invalid threshold value [%s] must be between 0 and 1000 ms\n", val);
				}
			} else if (!strcasecmp(var, "tone-spec") && !switch_strlen_zero(val)) {
				tone_spec = val;
			} else if (!strcasecmp(var, "digit-timeout")) {
				int tmp = 0;
				if (!switch_strlen_zero(val)) {
					tmp = atoi(val);
				}
				if (tmp >= 1000 && tmp <= 30000) {
					timeout = tmp;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "invalid timeout value [%s] must be between 1000 and 30000 ms\n", val);
				}
			} else if (!strcasecmp(var, "max-login-attempts")) {
				int tmp = 0;
				if (!switch_strlen_zero(val)) {
					tmp = atoi(val);
				}
				if (tmp > 0 && tmp < 11) {
					max_login_attempts = tmp;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "invalid attempts [%s] must be between 1 and 10 ms\n", val);
				}
			} else if (!strcasecmp(var, "min-record-len")) {
				int tmp = 0;
				if (!switch_strlen_zero(val)) {
					tmp = atoi(val);
				}
				if (tmp > 0 && tmp < 10000) {
					min_record_len = tmp;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "invalid record length [%s] must be between 1 and 10000s\n", val);
				}
			} else if (!strcasecmp(var, "max-record-len")) {
				int tmp = 0;
				if (!switch_strlen_zero(val)) {
					tmp = atoi(val);
				}
				if (tmp > 0 && tmp < 10000) {
					max_record_len = tmp;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "invalid record length [%s] must be between 1 and 10000s\n", val);
				}
			} else if (!strcasecmp(var, "odbc-dsn") && !switch_strlen_zero(val)) {
#ifdef SWITCH_HAVE_ODBC
				odbc_dsn = switch_core_strdup(globals.pool, val);
				if ((odbc_user = strchr(odbc_dsn, ':'))) {
					*odbc_user++ = '\0';
					if ((odbc_pass = strchr(odbc_user, ':'))) {
						*odbc_pass++ = '\0';
					}
				}
#else
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ODBC IS NOT AVAILABLE!\n");
#endif
			} else if (!strcasecmp(var, "auto-playback-recordings")){
				auto_playback_recordings = switch_true(val);
			}
		}

		if (switch_strlen_zero(name)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No name specified.\n");
		} else {
			profile = switch_core_alloc(globals.pool, sizeof(*profile));
			profile->name = switch_core_strdup(globals.pool, name);

			if (!switch_strlen_zero(odbc_dsn) && !switch_strlen_zero(odbc_user) && !switch_strlen_zero(odbc_pass)) {
				profile->odbc_dsn = odbc_dsn;
				profile->odbc_user = odbc_user;
				profile->odbc_pass = odbc_pass;
			} else {
				profile->dbname = switch_core_sprintf(globals.pool, "voicemail_%s", name);
			}
			if (profile->odbc_dsn) {
#ifdef SWITCH_HAVE_ODBC
				if (!(profile->master_odbc = switch_odbc_handle_new(profile->odbc_dsn, profile->odbc_user, profile->odbc_pass))) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Cannot Open ODBC Database!\n");
					continue;

				}
				if (switch_odbc_handle_connect(profile->master_odbc) != SWITCH_ODBC_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Cannot Open ODBC Database!\n");
					continue;
				}

				if (switch_odbc_handle_exec(profile->master_odbc, "select count(message_len) from voicemail_msgs", NULL) != SWITCH_ODBC_SUCCESS) {
					switch_odbc_handle_exec(profile->master_odbc, "drop table voicemail_msgs", NULL);
					switch_odbc_handle_exec(profile->master_odbc, vm_sql, NULL);
				}

				if (switch_odbc_handle_exec(profile->master_odbc, "select count(username) from voicemail_prefs", NULL) != SWITCH_ODBC_SUCCESS) {
					switch_odbc_handle_exec(profile->master_odbc, "drop table voicemail_prefs", NULL);
					switch_odbc_handle_exec(profile->master_odbc, vm_pref_sql, NULL);
				}

				if (switch_odbc_handle_exec(profile->master_odbc, "select count(password) from voicemail_prefs", NULL) != SWITCH_ODBC_SUCCESS) {
					switch_odbc_handle_exec(profile->master_odbc, "alter table voicemail_prefs add password varchar(255)", NULL);
				}

				if (switch_odbc_handle_exec(profile->master_odbc, "select count(message_len) from voicemail_data", NULL) == SWITCH_ODBC_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Old table voicemail_data found, migrating data!\n");
					/* XXX: Old table found.. migrating data into new table */
					if (switch_odbc_handle_exec(profile->master_odbc,
												"insert into voicemail_msgs select created_epoch, read_epoch, user as username, domain, uuid,"
												"cid_name, cid_number, in_folder, file_path, message_len, flags, read_flags from voicemail_data",
												NULL) != SWITCH_ODBC_SUCCESS) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Failed to migrate old voicemail_data to voicemail_msgs!\n");
						continue;
					}
					switch_odbc_handle_exec(profile->master_odbc, "drop table voicemail_data", NULL);
				}
#endif
			} else {
				if ((db = switch_core_db_open_file(profile->dbname))) {
					char *errmsg;
					switch_core_db_test_reactive(db, "select count(message_len) from voicemail_msgs", "drop table voicemail_msgs", vm_sql);

					switch_core_db_exec(db, "select count(message_len) from voicemail_data", NULL, NULL, &errmsg);
					if (errmsg) {
						switch_core_db_free(errmsg);
						errmsg = NULL;
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Migrating data from voicemail_data to voicemail_msgs!\n");
						switch_core_db_exec(db, "insert into voicemail_msgs select created_epoch, read_epoch, user as username, domain, uuid,"
											"cid_name, cid_number, in_folder, file_path, message_len, flags, read_flags from voicemail_data",
											NULL, NULL, &errmsg);
						if (errmsg) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SQL ERR [%s]\n", errmsg);
							switch_core_db_free(errmsg);
							errmsg = NULL;
						}
						switch_core_db_exec(db, "drop table voicemail_data", NULL, NULL, &errmsg);
						if (errmsg) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SQL ERR [%s]\n", errmsg);
							switch_core_db_free(errmsg);
							errmsg = NULL;
						}
					}


					switch_core_db_test_reactive(db, "select count(username) from voicemail_prefs", "drop table voicemail_prefs", vm_pref_sql);
					switch_core_db_test_reactive(db, "select count(password) from voicemail_prefs", NULL, 
												 "alter table voicemail_prefs add password varchar(255)");
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Cannot Open SQL Database!\n");
					continue;
				}
				switch_core_db_close(db);
			}

			profile->web_head = web_head;
			profile->web_tail = web_tail;

			profile->email_body = email_body;
			profile->email_headers = email_headers;
			if (notify_email_headers) {
				profile->notify_email_body = notify_email_body;
				profile->notify_email_headers = notify_email_headers;
			} else {
				profile->notify_email_body = email_body;
				profile->notify_email_headers = email_headers;
			}
			profile->email_from = switch_core_strdup(globals.pool, email_from);
			profile->date_fmt = switch_core_strdup(globals.pool, date_fmt);
			profile->play_date_announcement = play_date_announcement;

			profile->digit_timeout = timeout;
			profile->max_login_attempts = max_login_attempts;
			profile->min_record_len = min_record_len;
			profile->max_record_len = max_record_len;
			*profile->terminator_key = *terminator_key;
			*profile->play_new_messages_key = *play_new_messages_key;
			*profile->play_saved_messages_key = *play_saved_messages_key;
			*profile->main_menu_key = *main_menu_key;
			*profile->config_menu_key = *config_menu_key;
			*profile->record_greeting_key = *record_greeting_key;
			*profile->choose_greeting_key = *choose_greeting_key;
			*profile->record_name_key = *record_name_key;
			*profile->change_pass_key = *change_pass_key;
			*profile->record_file_key = *record_file_key;
			*profile->listen_file_key = *listen_file_key;
			*profile->save_file_key = *save_file_key;
			*profile->delete_file_key = *delete_file_key;
			*profile->undelete_file_key = *undelete_file_key;
			*profile->email_key = *email_key;
			*profile->callback_key = *callback_key;
			*profile->pause_key = *pause_key;
			*profile->restart_key = *restart_key;
			*profile->ff_key = *ff_key;
			*profile->rew_key = *rew_key;
			*profile->urgent_key = *urgent_key;
			*profile->operator_key = *operator_key;
			*profile->vmain_key = *vmain_key;
			*profile->forward_key = *forward_key;
			*profile->prepend_key = *prepend_key;
			profile->record_threshold = record_threshold;
			profile->record_silence_hits = record_silence_hits;
			profile->record_sample_rate = record_sample_rate;

			profile->auto_playback_recordings = auto_playback_recordings;
			profile->operator_ext = switch_core_strdup(globals.pool, operator_ext);
			profile->vmain_ext = switch_core_strdup(globals.pool, vmain_ext);
			profile->storage_dir = switch_core_strdup(globals.pool, storage_dir);
			profile->tone_spec = switch_core_strdup(globals.pool, tone_spec);
			profile->callback_dialplan = switch_core_strdup(globals.pool, callback_dialplan);
			profile->callback_context = switch_core_strdup(globals.pool, callback_context);

			profile->record_title = switch_core_strdup(globals.pool, record_title);
			profile->record_comment = switch_core_strdup(globals.pool, record_comment);
			profile->record_copyright = switch_core_strdup(globals.pool, record_copyright);

			switch_copy_string(profile->file_ext, file_ext, sizeof(profile->file_ext));
			switch_mutex_init(&profile->mutex, SWITCH_MUTEX_NESTED, globals.pool);

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Added Profile %s\n", profile->name);
			switch_core_hash_insert(globals.profile_hash, profile->name, profile);
		}
	}

  end:
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
					|| dtmf->digit == *cc->profile->terminator_key)) {
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
				int samps = 24000;
				switch_core_file_seek(fh, &pos, fh->pos - samps, SEEK_SET);
				return SWITCH_STATUS_SUCCESS;
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
#define VM_ABORT_MACRO "voicemail_abort"
#define VM_HELLO_MACRO "voicemail_hello"
#define VM_GOODBYE_MACRO "voicemail_goodbye"
#define VM_MESSAGE_COUNT_MACRO "voicemail_message_count"
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
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "failed to delete file [%s]\n", argv[0]);
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

	if (switch_core_file_open(&fh,
							  path,
							  0,
							  0,
							  SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL) == SWITCH_STATUS_SUCCESS) {

		if (switch_core_file_seek(&fh, &pos, 0, SEEK_END) == SWITCH_STATUS_SUCCESS) {
			*message_len = pos;
			status = SWITCH_STATUS_SUCCESS;
		}
		switch_core_file_close(&fh);
	}
	
	return status;

}

static switch_status_t create_file(switch_core_session_t *session, vm_profile_t *profile,
								   char *macro_name, char *file_path, switch_size_t *message_len, switch_bool_t limit)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_file_handle_t fh = { 0 };
	switch_input_args_t args = { 0 };
	char term;
	char input[10] = "", key_buf[80] = "";
	cc_t cc = { 0 };
	switch_codec_t *read_codec;

	read_codec = switch_core_session_get_read_codec(session);

	while (switch_channel_ready(channel)) {

		switch_snprintf(key_buf, sizeof(key_buf), "%s:%s:%s", profile->listen_file_key, profile->save_file_key, profile->record_file_key);

	  record_file:
		*message_len = 0;
		args.input_callback = cancel_on_dtmf;
		TRY_CODE(switch_ivr_phrase_macro(session, macro_name, NULL, NULL, NULL));
		TRY_CODE(switch_ivr_gentones(session, profile->tone_spec, 0, NULL));

		memset(&fh, 0, sizeof(fh));
		fh.thresh = profile->record_threshold;
		fh.silence_hits = profile->record_silence_hits;
		fh.samplerate = profile->record_sample_rate;
		switch_ivr_record_file(session, &fh, file_path, &args, profile->max_record_len);
		if (limit && (*message_len = fh.sample_count / read_codec->implementation->actual_samples_per_second) < profile->min_record_len) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Message is less than minimum record length: %d, discarding it.\n",
							  profile->min_record_len);
			if (unlink(file_path) != 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "failed to delete file [%s]\n", file_path);
			}
			if (switch_channel_ready(channel)) {
				TRY_CODE(switch_ivr_phrase_macro(session, VM_ACK_MACRO, "too-small", NULL, NULL));
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
			} else {
				status = vm_macro_get(session, VM_RECORD_FILE_CHECK_MACRO, key_buf, input, sizeof(input), 1, "", &term, profile->digit_timeout);
			}

			if (!strcmp(input, profile->listen_file_key)) {
				goto play_file;
			} else if (!strcmp(input, profile->record_file_key)) {
				goto record_file;
			} else {
				TRY_CODE(switch_ivr_phrase_macro(session, VM_ACK_MACRO, "saved", NULL, NULL));
				goto end;
			}
		}
	}

  end:
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
	char *email;
	int index;
	int want;
	msg_type_t type;
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

	return -1;
}


static void message_count(vm_profile_t *profile, const char *myid, const char *domain_name, const char *myfolder,
						  int *total_new_messages, int *total_saved_messages, int *total_new_urgent_messages, int *total_saved_urgent_messages)
{
	char msg_count[80] = "";
	callback_t cbt = { 0 };
	char sql[256];

	cbt.buf = msg_count;
	cbt.len = sizeof(msg_count);

	switch_snprintf(sql, sizeof(sql),
					"select count(*) from voicemail_msgs where username='%s' and domain='%s' and in_folder='%s' and read_epoch=0",
					myid, domain_name, myfolder);
	vm_execute_sql_callback(profile, profile->mutex, sql, sql2str_callback, &cbt);
	*total_new_messages = atoi(msg_count);

	switch_snprintf(sql, sizeof(sql),
					"select count(*) from voicemail_msgs where username='%s' and domain='%s' and in_folder='%s' and read_epoch=0 and read_flags='%s'",
					myid, domain_name, myfolder, URGENT_FLAG_STRING);
	vm_execute_sql_callback(profile, profile->mutex, sql, sql2str_callback, &cbt);
	*total_new_urgent_messages = atoi(msg_count);

	switch_snprintf(sql, sizeof(sql),
					"select count(*) from voicemail_msgs where username='%s' and domain='%s' and in_folder='%s' and read_epoch!=0",
					myid, domain_name, myfolder);
	vm_execute_sql_callback(profile, profile->mutex, sql, sql2str_callback, &cbt);
	*total_saved_messages = atoi(msg_count);

	switch_snprintf(sql, sizeof(sql),
					"select count(*) from voicemail_msgs where username='%s' and domain='%s' and in_folder='%s' and read_epoch!=0 and read_flags='%s'",
					myid, domain_name, myfolder, URGENT_FLAG_STRING);
	vm_execute_sql_callback(profile, profile->mutex, sql, sql2str_callback, &cbt);
	*total_saved_urgent_messages = atoi(msg_count);
}

#define VM_STARTSAMPLES 1024 * 32

static char *vm_merge_file(switch_core_session_t *session, vm_profile_t *profile, const char *announce, const char *orig)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);	
	switch_codec_t *read_codec = switch_core_session_get_read_codec(session);
	switch_file_handle_t lrfh = { 0 }, *rfh = NULL, lfh = { 0 }, *fh = NULL;
	char *tmp_path;
	switch_uuid_t uuid;
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	char *ret = NULL;
	int16_t *abuf = NULL;
	switch_size_t olen = 0;
	int asis = 0;

	switch_uuid_get(&uuid);
	switch_uuid_format(uuid_str, &uuid);

	lfh.channels = read_codec->implementation->number_of_channels;
	lfh.native_rate = read_codec->implementation->actual_samples_per_second;

	tmp_path = switch_core_session_sprintf(session, "%s%smsg_%s.%s", SWITCH_GLOBAL_dirs.temp_dir, SWITCH_PATH_SEPARATOR, uuid_str, profile->file_ext);

	if (switch_core_file_open(&lfh,
							  tmp_path,
							  lfh.channels,
							  read_codec->implementation->actual_samples_per_second,
							  SWITCH_FILE_FLAG_WRITE | SWITCH_FILE_DATA_SHORT, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to open file %s\n", tmp_path);
		goto end;
	}

	fh = &lfh;
	

	if (switch_core_file_open(&lrfh,
							  announce,
							  lfh.channels,
							  read_codec->implementation->actual_samples_per_second,
							  SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to open file %s\n", announce);
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
							  read_codec->implementation->actual_samples_per_second,
							  SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to open file %s\n", orig);
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
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "failed to delete file [%s]\n", announce);
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


	if (switch_channel_ready(channel)) {

		args.input_callback = cancel_on_dtmf;

		switch_snprintf(key_buf, sizeof(key_buf), "%s:%s:%s:%s:%s:%s", profile->listen_file_key, profile->save_file_key, 
						profile->delete_file_key, profile->email_key, profile->callback_key, profile->forward_key);
						

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

		if (!*cc.buf) {
			memset(&fh, 0, sizeof(fh));
			cc.fh = &fh;
			TRY_CODE(switch_ivr_play_file(session, NULL, cbt->file_path, &args));
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

			if (!strcmp(input, profile->listen_file_key)) {
				goto play_file;
			} else if (!strcmp(input, profile->callback_key)) {
				switch_core_session_execute_exten(session, cbt->cid_number, profile->callback_dialplan, profile->callback_context);
			} else if (!strcmp(input, profile->forward_key)) {
				char *cmd = NULL;
				char *new_file_path = NULL;
				char vm_cc[256] = "";
				char macro_buf[80] = "";
				
				
				switch_snprintf(key_buf, sizeof(key_buf), "%s:%s", profile->prepend_key, profile->forward_key);
				TRY_CODE(vm_macro_get(session, VM_FORWARD_PREPEND_MACRO, key_buf, input, sizeof(input), 1, "", &term, profile->digit_timeout));
				if (!strcmp(input, profile->prepend_key)) {
					switch_uuid_t uuid;
					char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
					switch_size_t message_len = 0;
					char *new_path = NULL;
					
					switch_uuid_get(&uuid);
					switch_uuid_format(uuid_str, &uuid);

					forward_file_path = switch_core_session_sprintf(session, "%s%smsg_%s.wav", SWITCH_GLOBAL_dirs.temp_dir, SWITCH_PATH_SEPARATOR, uuid_str);
					TRY_CODE(create_file(session, profile, VM_RECORD_MESSAGE_MACRO, forward_file_path, &message_len, SWITCH_TRUE));
					if ((new_path = vm_merge_file(session, profile, forward_file_path, cbt->file_path))) {
						switch_ivr_sleep(session, 1500, NULL);
						forward_file_path = new_path;
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error merging files\n");
						TRY_CODE(switch_ivr_phrase_macro(session, VM_ACK_MACRO, "deleted", NULL, NULL));
						goto end;
					}

					new_file_path = forward_file_path;
				} else {
					new_file_path = cbt->file_path;
				}
				
			get_exten:
				
				switch_snprintf(macro_buf, sizeof(macro_buf), "phrase:%s:%s", VM_FORWARD_MESSAGE_ENTER_EXTENSION_MACRO, profile->terminator_key);
				vm_cc[0] = '\0';

				TRY_CODE(switch_ivr_read(session, 0, sizeof(vm_cc), macro_buf, NULL, vm_cc, sizeof(vm_cc), profile->digit_timeout, profile->terminator_key));
				
				cmd = switch_core_session_sprintf(session, "%s@%s %s %s %s", vm_cc, cbt->domain, new_file_path, cbt->cid_number, cbt->cid_name);

				if (voicemail_inject(cmd) == SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Sent Carbon Copy to %s\n", vm_cc);
					TRY_CODE(switch_ivr_phrase_macro(session, VM_ACK_MACRO, "saved", NULL, NULL));
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to Carbon Copy to %s\n", vm_cc);
					TRY_CODE(switch_ivr_phrase_macro(session, VM_INVALID_EXTENSION_MACRO, vm_cc, NULL, NULL));
					goto get_exten;
				}
			} else if (!strcmp(input, profile->delete_file_key) || !strcmp(input, profile->email_key)) {
				char *sql = switch_mprintf("update voicemail_msgs set flags='delete' where uuid='%s'", cbt->uuid);
				vm_execute_sql(profile, sql, profile->mutex);
				switch_safe_free(sql);
				if (!strcmp(input, profile->email_key) && !switch_strlen_zero(cbt->email)) {
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

					if (!strcasecmp(cbt->read_flags, URGENT_FLAG_STRING)) {
						priority = 1;
					}

					message_count(profile, cbt->user, cbt->domain, cbt->in_folder, &total_new_messages, &total_saved_messages,
								  &total_new_urgent_messages, &total_saved_urgent_messages);

					switch_time_exp_lt(&tm, switch_time_make (atol(cbt->created_epoch), 0));
					switch_strftime(date, &retsize, sizeof(date), profile->date_fmt, &tm);

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
					switch_channel_set_variable(channel, "voicemail_caller_id_name", cbt->cid_name);
					switch_channel_set_variable(channel, "voicemail_file_path", cbt->file_path);
					switch_channel_set_variable(channel, "voicemail_read_flags", cbt->read_flags);
					switch_channel_set_variable(channel, "voicemail_time", date);
					switch_snprintf(tmp, sizeof(tmp), "%d", priority);
					switch_channel_set_variable(channel, "voicemail_priority", tmp);
					message_len = atoi(cbt->message_len);

					l_duration = switch_time_make (atol(cbt->message_len), 0);
					switch_core_measure_time(l_duration, &duration);
					duration.day += duration.yr * 365;
					duration.hr += duration.day * 24;

					switch_snprintf(duration_str, sizeof(duration_str), "%.2u:%.2u:%.2u", duration.hr, duration.min, duration.sec);

					switch_channel_set_variable(channel, "voicemail_message_len", duration_str);
					switch_channel_set_variable(channel, "voicemail_email", cbt->email);

					if (switch_strlen_zero(profile->email_headers)) {
						from = switch_core_session_sprintf(session, "%s@%s", cbt->user, cbt->domain);
					} else {
						from = switch_channel_expand_variables(channel, profile->email_from);
					}

					if (switch_strlen_zero(profile->email_headers)) {
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
						/* this isnt done?  it was in the other place
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

					switch_simple_email(cbt->email, from, header_string, body, cbt->file_path);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Sending message to %s\n", cbt->email);
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
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "failed to delete file [%s]\n", forward_file_path);
		}
	}

	return status;
}


static void
update_mwi(vm_profile_t *profile, const char *id, const char *domain_name,
           const char *myfolder)
{
	char *mwi_id;
	const char *yn = "no";
	int total_new_messages = 0;
	int total_saved_messages = 0;
	int total_new_urgent_messages = 0;
	int total_saved_urgent_messages = 0;
	switch_event_t *event;

	message_count(profile, id, domain_name, myfolder, &total_new_messages, &total_saved_messages, &total_new_urgent_messages, &total_saved_urgent_messages);

	if (switch_event_create(&event, SWITCH_EVENT_MESSAGE_WAITING) != SWITCH_STATUS_SUCCESS) {
		return;
	}

	if (total_new_messages || total_new_urgent_messages) {
		yn = "yes";
	}
	mwi_id = switch_mprintf("%s@%s", id, domain_name);
	switch_assert(mwi_id);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "MWI-Messages-Waiting", "%s", yn);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "MWI-Message-Account", mwi_id);
	switch_event_add_header(event, SWITCH_STACK_BOTTOM, "MWI-Voice-Message", "%d/%d (%d/%d)", total_new_messages, total_saved_messages, total_new_urgent_messages, total_saved_urgent_messages);
	switch_event_fire(&event);
	switch_safe_free(mwi_id);
}


#define FREE_DOMAIN_ROOT() if (x_domain_root) switch_xml_free(x_domain_root); x_user = x_domain = x_domain_root = NULL


static void voicemail_check_main(switch_core_session_t *session, const char *profile_name, const char *domain_name, const char *id, int auth)
{
	vm_check_state_t vm_check_state = VM_CHECK_START;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_caller_profile_t *caller_profile = switch_channel_get_caller_profile(channel);
	vm_profile_t *profile;
	switch_xml_t x_domain = NULL, x_domain_root = NULL, x_user = NULL, x_params, x_param;
	switch_status_t status;
	char pass_buf[80] = "", *mypass = NULL, id_buf[80] = "", *myfolder = NULL;
	const char *thepass = NULL, *myid = id;
	char term = 0;
	uint32_t timeout, attempts = 0;
	int failed = 0;
	msg_type_t play_msg_type = MSG_NONE;
	char *dir_path = NULL, *file_path = NULL;
	int total_new_messages = 0;
	int total_saved_messages = 0;
	int total_new_urgent_messages = 0;
	int total_saved_urgent_messages = 0;
	int heard_auto_saved = 0, heard_auto_new = 0;
	char *vm_email = NULL;
	char *vm_storage_dir = NULL;
	char global_buf[2] = "";
	switch_input_args_t args = { 0 };
	const char *caller_id_name = NULL;
	const char *caller_id_number = NULL;
	
	if (!(caller_id_name = switch_channel_get_variable(channel, "effective_caller_id_name"))) {
		caller_id_name = caller_profile->caller_id_name;
	}

	if (!(caller_id_number = switch_channel_get_variable(channel, "effective_caller_id_number"))) {
		caller_id_number = caller_profile->caller_id_number;
	}

	if (!(profile = switch_core_hash_find(globals.profile_hash, profile_name))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error invalid profile %s\n", profile_name);
		return;
	}

	timeout = profile->digit_timeout;
	attempts = profile->max_login_attempts;
	status = switch_ivr_phrase_macro(session, VM_HELLO_MACRO, NULL, NULL, &args);
	*global_buf = '\0';

	while (switch_channel_ready(channel)) {
		switch_ivr_sleep(session, 100, NULL);

		switch (vm_check_state) {
		case VM_CHECK_START:
			{
				total_new_messages = 0;
				total_saved_messages = 0;
				total_new_urgent_messages = 0;
				total_saved_urgent_messages = 0;
				heard_auto_saved = 0;
				heard_auto_new = 0;
				play_msg_type = MSG_NONE;
				attempts = profile->max_login_attempts;
				myid = id;
				mypass = NULL;
				myfolder = "inbox";
				vm_check_state = VM_CHECK_AUTH;
				FREE_DOMAIN_ROOT();
			}
			break;
		case VM_CHECK_FOLDER_SUMMARY:
			{
				int informed = 0;
				char msg_count[80] = "";
				switch_input_args_t folder_args = { 0 };

				folder_args.input_callback = cancel_on_dtmf;
				folder_args.buf = &global_buf;
				folder_args.buflen = sizeof(global_buf);

				switch_channel_set_variable(channel, "voicemail_current_folder", myfolder);
				message_count(profile, myid, domain_name, myfolder, &total_new_messages, &total_saved_messages,
							  &total_new_urgent_messages, &total_saved_urgent_messages);

				if (total_new_urgent_messages > 0) {
					switch_snprintf(msg_count, sizeof(msg_count), "%d:urgent-new", total_new_urgent_messages);
					TRY_CODE(switch_ivr_phrase_macro(session, VM_MESSAGE_COUNT_MACRO, msg_count, NULL, &folder_args));
					informed++;
					if (!switch_strlen_zero_buf(global_buf)) {
						vm_check_state = VM_CHECK_MENU;
						continue;
					}
				}
				if (total_new_messages > 0 && total_new_messages != total_new_urgent_messages) {
					switch_snprintf(msg_count, sizeof(msg_count), "%d:new", total_new_messages);
					TRY_CODE(switch_ivr_phrase_macro(session, VM_MESSAGE_COUNT_MACRO, msg_count, NULL, &folder_args));
					informed++;
					if (!switch_strlen_zero_buf(global_buf)) {
						vm_check_state = VM_CHECK_MENU;
						continue;
					}
				}

				if (!heard_auto_new && total_new_messages + total_new_urgent_messages > 0) {
					heard_auto_new = 1;
					play_msg_type = MSG_NEW;
					vm_check_state = VM_CHECK_PLAY_MESSAGES;
					continue;
				}

				if (!informed) {
					switch_snprintf(msg_count, sizeof(msg_count), "0:new");
					TRY_CODE(switch_ivr_phrase_macro(session, VM_MESSAGE_COUNT_MACRO, msg_count, NULL, &folder_args));
					informed++;
				}

				vm_check_state = VM_CHECK_MENU;
			}
			break;
		case VM_CHECK_PLAY_MESSAGES:
			{
				listen_callback_t cbt;
				char sql[256];
				int cur_message, total_messages;

				message_count(profile, myid, domain_name, myfolder, &total_new_messages, &total_saved_messages,
							  &total_new_urgent_messages, &total_saved_urgent_messages);
				memset(&cbt, 0, sizeof(cbt));
				cbt.email = vm_email;
				switch (play_msg_type) {
				case MSG_NEW:
					{
						switch_snprintf(sql, sizeof(sql),
										"select * from voicemail_msgs where username='%s' and domain='%s' and read_epoch=0 order by read_flags", myid,
										domain_name);
						total_messages = total_new_messages;
						heard_auto_new = heard_auto_saved = 1;
					}
					break;
				case MSG_SAVED:
				default:
					{
						switch_snprintf(sql, sizeof(sql),
										"select * from voicemail_msgs where username='%s' and domain='%s' and read_epoch !=0 order by read_flags", myid,
										domain_name);
						total_messages = total_saved_messages;
						heard_auto_new = heard_auto_saved = 1;
					}
					break;
				}
				for (cur_message = 0; cur_message < total_messages; cur_message++) {
					cbt.index = 0;
					cbt.want = cur_message;
					cbt.type = play_msg_type;
					vm_execute_sql_callback(profile, profile->mutex, sql, listen_callback, &cbt);
					status = listen_file(session, profile, &cbt);
					if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_BREAK) {
						break;
					}
				}
				switch_snprintf(sql, sizeof(sql), "update voicemail_msgs set read_epoch=%ld where username='%s' and domain='%s' and flags='save'",
								(long) switch_timestamp(NULL), myid, domain_name);
				vm_execute_sql(profile, sql, profile->mutex);
				switch_snprintf(sql, sizeof(sql), "select file_path from voicemail_msgs where username='%s' and domain='%s' and flags='delete'", myid,
								domain_name);
				vm_execute_sql_callback(profile, profile->mutex, sql, unlink_callback, NULL);
				switch_snprintf(sql, sizeof(sql), "delete from voicemail_msgs where username='%s' and domain='%s' and flags='delete'", myid, domain_name);
				vm_execute_sql(profile, sql, profile->mutex);
				vm_check_state = VM_CHECK_FOLDER_SUMMARY;

				update_mwi(profile, id, domain_name, myfolder);
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
								profile->choose_greeting_key, 
								profile->record_name_key, 
								profile->change_pass_key, 
								profile->main_menu_key);
				

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
						status = switch_ivr_play_file(session, NULL, file_path, &greeting_args);
					}
					if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_BREAK) {
						TRY_CODE(switch_ivr_phrase_macro(session, VM_CHOOSE_GREETING_FAIL_MACRO, NULL, NULL, NULL));
					} else {
						TRY_CODE(switch_ivr_phrase_macro(session, VM_CHOOSE_GREETING_SELECTED_MACRO, input, NULL, NULL));
						sql =
							switch_mprintf("update voicemail_prefs set greeting_path='%s' where username='%s' and domain='%s'", file_path, myid,
										   domain_name);
						vm_execute_sql(profile, sql, profile->mutex);
						switch_safe_free(sql);
					}
					switch_safe_free(file_path);
				} else if (!strcmp(input, profile->record_greeting_key)) {
					int num;
					TRY_CODE(vm_macro_get(session, VM_CHOOSE_GREETING_MACRO, key_buf, input, sizeof(input), 1, "", &term, timeout));

					num = atoi(input);
					if (num < 1 || num > VM_MAX_GREETINGS) {
						TRY_CODE(switch_ivr_phrase_macro(session, VM_CHOOSE_GREETING_FAIL_MACRO, NULL, NULL, NULL));
					} else {
						file_path = switch_mprintf("%s%sgreeting_%d.%s", dir_path, SWITCH_PATH_SEPARATOR, num, profile->file_ext);
						TRY_CODE(create_file(session, profile, VM_RECORD_GREETING_MACRO, file_path, &message_len, SWITCH_TRUE));
						sql =
							switch_mprintf("update voicemail_prefs set greeting_path='%s' where username='%s' and domain='%s'", file_path, myid,
										   domain_name);
						vm_execute_sql(profile, sql, profile->mutex);
						switch_safe_free(sql);
						switch_safe_free(file_path);
					}

				} else if (!strcmp(input, profile->change_pass_key)) {
					char buf[256] = "";
					char macro[256] = "";

					switch_snprintf(macro, sizeof(macro), "phrase:%s:%s", VM_ENTER_PASS_MACRO, profile->terminator_key);
					TRY_CODE(switch_ivr_read(session, 0, 255, macro, NULL, buf, sizeof(buf), 10000, profile->terminator_key));
					sql = switch_mprintf("update voicemail_prefs set password='%s' where username='%s' and domain='%s'", buf, myid, domain_name);
					vm_execute_sql(profile, sql, profile->mutex);
					switch_safe_free(file_path);
					switch_safe_free(sql);

				} else if (!strcmp(input, profile->record_name_key)) {
					file_path = switch_mprintf("%s%srecorded_name.%s", dir_path, SWITCH_PATH_SEPARATOR, profile->file_ext);
					TRY_CODE(create_file(session, profile, VM_RECORD_NAME_MACRO, file_path, &message_len, SWITCH_FALSE));
					sql = switch_mprintf("update voicemail_prefs set name_path='%s' where username='%s' and domain='%s'", file_path, myid, domain_name);
					vm_execute_sql(profile, sql, profile->mutex);
					switch_safe_free(file_path);
					switch_safe_free(sql);
				}
				continue;
			}
			break;
		case VM_CHECK_MENU:
			{
				char input[10] = "";
				char key_buf[80] = "";
				play_msg_type = MSG_NONE;
				if (!switch_strlen_zero_buf(global_buf)) {
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
				}

				continue;
			}
			break;
		case VM_CHECK_AUTH:
			{
				prefs_callback_t cbt = {{0}};
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
					switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "mailbox", myid);
					switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "destination_number", caller_profile->destination_number);
					switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "caller_id_number", caller_id_number);
					
					
					if (switch_xml_locate_user("id", myid, domain_name, switch_channel_get_variable(channel, "network_addr"),
											   &x_domain_root, &x_domain, &x_user, params) != SWITCH_STATUS_SUCCESS) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "can't find user [%s@%s]\n", myid, domain_name);
						ok = 0;
					}
					
					switch_event_destroy(&params);

					if (!ok) {
						goto end;
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

				x_params = switch_xml_child(x_user, "params");

				thepass = NULL;
				switch_snprintf(sql, sizeof(sql), "select * from voicemail_prefs where username='%s' and domain='%s'", myid, domain_name);
				vm_execute_sql_callback(profile, profile->mutex, sql, prefs_callback, &cbt);
				for (x_param = switch_xml_child(x_params, "param"); x_param; x_param = x_param->next) {
					const char *var = switch_xml_attr_soft(x_param, "name");
					const char *val = switch_xml_attr_soft(x_param, "value");

					if (!strcasecmp(var, "password")) {
						thepass = val;
					} else if (!strcasecmp(var, "vm-password")) {
						thepass = val;
					} else if (!strcasecmp(var, "vm-mailto")) {
						vm_email = switch_core_session_strdup(session, val);
					} else if (!strcasecmp(var, "storage-dir")) {
						vm_storage_dir = switch_core_session_strdup(session, val);
					} else if (!switch_strlen_zero(cbt.password) && !thepass) {
						thepass = cbt.password;
					}
				}

				if (!auth) {
					if (switch_strlen_zero(cbt.password) && !strcmp(cbt.password, mypass)) {
						auth++;
					} else if (!thepass) {
						auth++;
					}

					if (!auth && (thepass && mypass && !strcmp(thepass, mypass))) {
						auth++;
					}
				}

				FREE_DOMAIN_ROOT();
				
				if (auth) {
					if (!dir_path) {
						if (!switch_strlen_zero(vm_storage_dir)) {
							dir_path = switch_core_session_sprintf(session, "%s%s%s", vm_storage_dir, SWITCH_PATH_SEPARATOR, myid);
						} else if ( !switch_strlen_zero(profile->storage_dir) ) {
							dir_path = switch_core_session_sprintf(session, "%s%s%s", profile->storage_dir, SWITCH_PATH_SEPARATOR, myid);
						} else {
							dir_path = switch_core_session_sprintf(session, "%s%svoicemail%s%s%s%s%s%s", SWITCH_GLOBAL_dirs.storage_dir,
																   SWITCH_PATH_SEPARATOR,
																   SWITCH_PATH_SEPARATOR,
																   profile->name, SWITCH_PATH_SEPARATOR, domain_name, SWITCH_PATH_SEPARATOR, myid);
						}

						if (switch_dir_make_recursive(dir_path, SWITCH_DEFAULT_DIR_PERMS, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error creating %s\n", dir_path);
							goto end;
						}
					}

					vm_check_state = VM_CHECK_FOLDER_SUMMARY;
				} else {
					goto failed;
				}
				
				continue;

			  failed:
				
				FREE_DOMAIN_ROOT();

				status = switch_ivr_phrase_macro(session, VM_FAIL_AUTH_MACRO, NULL, NULL, NULL);
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

	if (switch_channel_ready(channel)) {
		if (failed) {
			status = switch_ivr_phrase_macro(session, VM_ABORT_MACRO, NULL, NULL, NULL);
		}
		status = switch_ivr_phrase_macro(session, VM_GOODBYE_MACRO, NULL, NULL, NULL);
	}

	if (x_domain_root) {
		switch_xml_free(x_domain_root);
		x_domain_root = NULL;
	}
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
								switch_bool_t copy)
{
	char *file_path = NULL, *dir_path = NULL;
	const char *myid = switch_xml_attr(x_user, "id");
	const char *mybox = switch_xml_attr(x_user, "mailbox");
	switch_uuid_t uuid;
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	const char *filename;
	switch_xml_t x_param, x_params;
	char *vm_email = NULL;
	char *vm_notify_email = NULL;
	char *email_addr = NULL;
	char *vm_timezone = NULL;
	int send_mail = 0;
	int send_main = 0;
	int send_notify = 0;
	int insert_db = 1;
	int email_attach = 0;
	char *vm_storage_dir = NULL;
	char *myfolder = "inbox";
	int priority = 3;
	const char *tmp;
	switch_event_t *local_event = NULL;
	switch_status_t ret = SWITCH_STATUS_SUCCESS;
	
	if (mybox) {
		myid = mybox;
	}
	
	if (params) {
		switch_event_create(&local_event, SWITCH_EVENT_REQUEST_PARAMS);
		params = local_event;
	}
	
	if ((tmp = switch_event_get_header(params, "effective_caller_id_name"))) {
		caller_id_name = tmp;
	}

	if ((tmp = switch_event_get_header(params, "effective_caller_id_number"))) {
		caller_id_number = tmp;
	}
	
	switch_uuid_get(&uuid);
	switch_uuid_format(uuid_str, &uuid);

	if ((filename = strrchr(path, *SWITCH_PATH_SEPARATOR))) {
		filename++;
	} else {
		filename = path;
	}

	x_params = switch_xml_child(x_user, "params");

	for (x_param = switch_xml_child(x_params, "param"); x_param; x_param = x_param->next) {
		const char *var = switch_xml_attr_soft(x_param, "name");
		const char *val = switch_xml_attr_soft(x_param, "value");

		if (!strcasecmp(var, "vm-mailto")) {
			vm_email = switch_core_strdup(pool, val);
		} else if (!strcasecmp(var, "vm-notify-mailto")) {
			vm_notify_email = switch_core_strdup(pool, val);
		} else if (!strcasecmp(var, "email-addr")) {
			email_addr = switch_core_strdup(pool, val);
		} else if (!strcasecmp(var, "vm-email-all-messages")) {
			send_main = switch_true(val);
			send_mail++;
		} else if (!strcasecmp(var, "vm-notify-email-all-messages")) {
			send_notify = switch_true(val);
			send_mail++;
		} else if (!strcasecmp(var, "vm-keep-local-after-email")) {
			insert_db = switch_true(val);
		} else if (!strcasecmp(var, "vm-attach-file")) {
			email_attach = switch_true(val);
		} else if (!strcasecmp(var, "timezone")) {
			vm_timezone = switch_core_strdup(pool, val);
		}
		/*switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "send mail is %d, var is %s\n", send_mail, var);*/
	}
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "send mail is %d and that's my final answer\n", send_mail);
	if (!switch_strlen_zero(vm_storage_dir)) {
		dir_path = switch_mprintf("%s%s%s", vm_storage_dir, SWITCH_PATH_SEPARATOR, myid);
	} else if (!switch_strlen_zero(profile->storage_dir)) {
		dir_path = switch_mprintf("%s%s%s", profile->storage_dir, SWITCH_PATH_SEPARATOR, myid);
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

	file_path = switch_mprintf("%s%smsg_%s_broadcast_%s", dir_path, SWITCH_PATH_SEPARATOR, uuid_str, filename);

	if (copy) {
		switch_file_copy(path, file_path, SWITCH_FPROT_FILE_SOURCE_PERMS, pool);
	} else {
		file_path = (char *)path;
	}

	if (!message_len) {
		size_t len = 0;
		if (measure_file_len(file_path, &len) == SWITCH_STATUS_SUCCESS) {
			message_len = len;
		}
	}

	if (insert_db && switch_file_exists(file_path, pool) == SWITCH_STATUS_SUCCESS) {
		char *usql;
		
		usql = switch_mprintf("insert into voicemail_msgs values(%ld,0,'%q','%q','%q','%q','%q','%q','%q','%u','','%q')", (long) switch_timestamp(NULL),
							  myid, domain_name, uuid_str, caller_id_name, caller_id_number,
							  myfolder, file_path, message_len, read_flags);

		vm_execute_sql(profile, usql, profile->mutex);
		switch_safe_free(usql);

		update_mwi(profile, myid, domain_name, myfolder);
	}

	if (send_mail && !switch_strlen_zero(vm_email) && switch_file_exists(file_path, pool) == SWITCH_STATUS_SUCCESS) {
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

		message_count(profile, myid, domain_name, myfolder, &total_new_messages, &total_saved_messages,
					  &total_new_urgent_messages, &total_saved_urgent_messages);

		switch_time_exp_lt(&tm, switch_timestamp_now());
		switch_strftime(date, &retsize, sizeof(date), profile->date_fmt, &tm);

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
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "voicemail_caller_id_name", caller_id_name);
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "voicemail_file_path", file_path);
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "voicemail_read_flags", read_flags);
		
		if (!switch_strlen_zero(vm_timezone)) {
			char tz_date[80] = "";
			if ((switch_strftime_tz(vm_timezone, profile->date_fmt, tz_date, sizeof(tz_date)) == SWITCH_STATUS_SUCCESS) && !switch_strlen_zero_buf(tz_date)) {
				switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "voicemail_time", tz_date);
			} else {
				switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "voicemail_time", date);
			}
		}

		switch_snprintf(tmpvar, sizeof(tmpvar), "%d", priority);
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "voicemail_priority", tmpvar);
		if (vm_email) {
			switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "voicemail_email", vm_email);
		}
		if (vm_notify_email) {
			switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "voicemail_notify_email", vm_notify_email);
		}
		l_duration = switch_time_make (message_len, 0);
		switch_core_measure_time(l_duration, &duration);
		duration.day += duration.yr * 365;
		duration.hr += duration.day * 24;
		switch_snprintf(duration_str, sizeof(duration_str), "%.2u:%.2u:%.2u", duration.hr, duration.min, duration.sec);

		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "voicemail_message_len", duration_str);

		if (switch_strlen_zero(profile->email_from)) {
			from = switch_core_sprintf(pool, "%s@%s", myid, domain_name);
		} else {
			from = switch_event_expand_headers(params, profile->email_headers);
		}


		if (send_main) {
			if (switch_strlen_zero(profile->email_headers)) {
				headers = switch_mprintf(
										 "From: FreeSWITCH mod_voicemail <%s@%s>\n"
										 "Subject: Voicemail from %s %s\nX-Priority: %d",
										 myid, domain_name, caller_id_name, caller_id_number, priority);
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
				switch_simple_email(vm_email, from, header_string, body, file_path);
			} else {
				switch_simple_email(vm_email, from, header_string, body, NULL);
			}

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Sending message to %s\n", vm_email);
			switch_safe_free(body);

			if (headers != profile->email_headers) {
				switch_safe_free(headers);
			}
		}


		if (send_notify) {
			if (switch_strlen_zero(profile->notify_email_headers)) {
				headers = switch_mprintf(
										 "From: FreeSWITCH mod_voicemail <%s@%s>\n"
										 "Subject: Voicemail from %s %s\nX-Priority: %d",
										 myid, domain_name, caller_id_name, caller_id_number, priority);
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

			switch_simple_email(vm_notify_email, from, header_string, body, NULL);

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Sending notify message to %s\n", vm_notify_email);
			switch_safe_free(body);

			if (headers != profile->notify_email_headers) {
				switch_safe_free(headers);
			}
		}

		if (!insert_db) {
			if (unlink(file_path) != 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "failed to delete file [%s]\n", file_path);
			}
		}
	}

 failed:

	switch_event_destroy(&local_event);

	switch_safe_free(dir_path);

	if (file_path != path) {
		switch_safe_free(file_path);
	}

	return ret;

}

static switch_status_t voicemail_inject(const char *data)
{
	vm_profile_t *profile;
	char *dup = NULL, *user = NULL, *domain = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	int istag = 0, isall = 0;
	int argc = 0;
	char *argv[4];
	char *box, *path, *cid_num, *cid_name;
	switch_memory_pool_t *pool = NULL;

	if (switch_strlen_zero(data)) {
		status = SWITCH_STATUS_FALSE;
		goto end;
	}
	
	dup = strdup(data);
	switch_assert(dup);

	if ((argc = switch_separate_string(dup, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) < 2) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "not enough args [%s]\n", data);
		status = SWITCH_STATUS_FALSE;
		goto end;
	}

	box = argv[0];
	path = argv[1];
	cid_num = argv[2] ? argv[2] : "anonymous";
	cid_name = argv[3] ? argv[3] : "anonymous";

	user = box;
	
	if ((domain = strchr(user, '@'))) {
		*domain++ = '\0';
	} else {
		domain = user;
	}

	if (switch_stristr("tag=", user)) {
		user += 4;
		istag++;
	} else if (user == domain) {
		isall++;
	}
	
	if (!(user && domain)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "invalid syntax [%s][%s]\n", switch_str_nil(user), switch_str_nil(domain));
		status = SWITCH_STATUS_FALSE;
		goto end;
	}

	if (!(profile = switch_core_hash_find(globals.profile_hash, domain))) {
		profile = switch_core_hash_find(globals.profile_hash, "default");
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
		switch_event_add_header_string(my_params, SWITCH_STACK_BOTTOM, "domain", domain);
		switch_event_add_header_string(my_params, SWITCH_STACK_BOTTOM, "purpose", "publish-vm");
		
		if (switch_xml_locate_domain(domain, my_params, &xml_root, &x_domain) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Cannot locate domain %s\n", domain);
			status = SWITCH_STATUS_FALSE;
			switch_event_destroy(&my_params);
			goto end;
		}
		
		switch_event_destroy(&my_params);

		switch_core_new_memory_pool(&pool);
		
		if (!isall && !istag) {
			if ((ut = switch_xml_find_child(x_domain, "user", "id", user))) {
				switch_event_create(&my_params, SWITCH_EVENT_REQUEST_PARAMS);
				status = deliver_vm(profile, ut, domain, path, 0, "B", my_params, pool, cid_name, cid_num, SWITCH_TRUE);
				switch_event_destroy(&my_params);
			} else {
				status = SWITCH_STATUS_FALSE;
			}
		} else {
			for (ut = switch_xml_child(x_domain, "user"); ut; ut = ut->next) {
				const char *tag;

				if (isall || (istag && (tag=switch_xml_attr(ut, "vm-tag")) && !strcasecmp(tag, user))) {
					switch_event_create(&my_params, SWITCH_EVENT_REQUEST_PARAMS);
					status = deliver_vm(profile, ut, domain, path, 0, "B", my_params, pool, cid_name, cid_num, SWITCH_TRUE);
					switch_event_destroy(&my_params);
				}
			}
		}
		
		switch_core_destroy_memory_pool(&pool);

		switch_xml_free(xml_root);
	}

 end:

	switch_safe_free(dup);

	return status;
}

static switch_status_t voicemail_leave_main(switch_core_session_t *session, const char *profile_name, const char *domain_name, const char *id)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char sql[256];
	prefs_callback_t cbt;
	vm_profile_t *profile;
	char *uuid = switch_core_session_get_uuid(session);
	char *file_path = NULL;
	char *dir_path = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_caller_profile_t *caller_profile = switch_channel_get_caller_profile(channel);
	switch_file_handle_t fh = { 0 };
	switch_input_args_t args = { 0 };
	char *vm_email = NULL;
	char *vm_notify_email = NULL;
	int send_mail = 0;
	cc_t cc = { 0 };
	char *read_flags = NORMAL_FLAG_STRING;
	int priority = 3;
	int email_attach = 1;
	char buf[2];
	char *greet_path = NULL;
	const char *voicemail_greeting_number = NULL;
	switch_size_t message_len = 0;
	switch_time_exp_t tm;
	char date[80] = "";
	switch_size_t retsize;
	switch_time_t ts = switch_timestamp_now();
	char *dbuf = NULL;
	char *vm_storage_dir = NULL;
	int send_main = 0;
	int send_notify = 0;
	int insert_db = 1;
	const char *caller_id_name = NULL;
	const char *caller_id_number = NULL;
	switch_xml_t x_domain = NULL, x_domain_root = NULL, x_user = NULL, x_params = NULL, x_param = NULL;
	switch_event_t *vars = NULL;
	const char *vm_cc = NULL, *vtmp, *vm_ext = NULL;
	
	if (!(caller_id_name = switch_channel_get_variable(channel, "effective_caller_id_name"))) {
		caller_id_name = caller_profile->caller_id_name;
	}

	if (!(caller_id_number = switch_channel_get_variable(channel, "effective_caller_id_number"))) {
		caller_id_number = caller_profile->caller_id_number;
	}

	memset(&cbt, 0, sizeof(cbt));
	if (!(profile = switch_core_hash_find(globals.profile_hash, profile_name))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error invalid profile %s\n", profile_name);
		return SWITCH_STATUS_FALSE;
	}

	if (id) {
		int ok = 1;
		switch_event_t *params = NULL;
		const char *email_addr = NULL;

		switch_event_create(&params, SWITCH_EVENT_REQUEST_PARAMS);
		switch_assert(params);
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "mailbox", id);

		if (switch_xml_locate_user("id", id, domain_name, switch_channel_get_variable(channel, "network_addr"),
								   &x_domain_root, &x_domain, &x_user, params) == SWITCH_STATUS_SUCCESS) {
			if ((x_params = switch_xml_child(x_user, "params"))) {
				for (x_param = switch_xml_child(x_params, "param"); x_param; x_param = x_param->next) {
					const char *var = switch_xml_attr_soft(x_param, "name");
					const char *val = switch_xml_attr_soft(x_param, "value");

					if (!strcasecmp(var, "vm-mailto")) {
						vm_email = switch_core_session_strdup(session, val);
					} else if (!strcasecmp(var, "vm-notify-mailto")) {
						vm_notify_email = switch_core_session_strdup(session, val);
					} else if (!strcasecmp(var, "email-addr")) {
						email_addr = switch_core_session_strdup(session, val);
					} else if (!strcasecmp(var, "vm-email-all-messages")) {
						send_main = switch_true(val);
						send_mail++;
					} else if (!strcasecmp(var, "storage-dir")) {
						vm_storage_dir = switch_core_session_strdup(session, val);
					} else if (!strcasecmp(var, "vm-notify-email-all-messages")) {
						send_notify = switch_true(val);
						send_mail++;
					} else if (!strcasecmp(var, "vm-keep-local-after-email")) {
						insert_db = switch_true(val);
					} else if (!strcasecmp(var, "vm-attach-file")) {
						email_attach = switch_true(val);
					}
				}
			}

			if (send_main && switch_strlen_zero(vm_email) && !switch_strlen_zero(email_addr)) {
				vm_email = switch_core_session_strdup(session, email_addr);
				if (switch_strlen_zero(vm_email)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "No email address, not going to send email.\n");
					send_main = 0;
				}
			}

			if (send_notify && switch_strlen_zero(vm_notify_email)) {
				vm_notify_email = vm_email;
				if (switch_strlen_zero(vm_notify_email)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "No notify email address, not going to notify.\n");
					send_notify = 0;
				}
			}
			
			if (send_mail && (!(send_main || send_notify))) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Falling back to leaving message locally due to too many misconfigurations.\n");
				send_mail = 0;
				insert_db = 1;
			}

			if (send_notify && !send_main) {
				insert_db = 1;
			}
			
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "can't find user [%s@%s]\n", id, domain_name);
			ok = 0;
		}

		switch_event_destroy(&params);
		
		if (!ok) {
			goto end;
		}
	}

	if (!switch_strlen_zero(vm_storage_dir)) {
		dir_path = switch_core_session_sprintf(session, "%s%s%s", vm_storage_dir, SWITCH_PATH_SEPARATOR, id);
	} else if (!switch_strlen_zero(profile->storage_dir)) {
		dir_path = switch_core_session_sprintf(session, "%s%s%s", profile->storage_dir, SWITCH_PATH_SEPARATOR, id);
	} else {
		dir_path = switch_core_session_sprintf(session, "%s%svoicemail%s%s%s%s%s%s", SWITCH_GLOBAL_dirs.storage_dir,
											   SWITCH_PATH_SEPARATOR,
											   SWITCH_PATH_SEPARATOR, profile->name, SWITCH_PATH_SEPARATOR, domain_name, SWITCH_PATH_SEPARATOR, id);
	}

	if (switch_dir_make_recursive(dir_path, SWITCH_DEFAULT_DIR_PERMS, switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error creating %s\n", dir_path);
		goto end;
	}

	switch_snprintf(sql, sizeof(sql), "select * from voicemail_prefs where username='%s' and domain='%s'", id, domain_name);
	vm_execute_sql_callback(profile, profile->mutex, sql, prefs_callback, &cbt);

	vm_ext = profile->file_ext;
	if ((vtmp = switch_channel_get_variable(channel, "vm_message_ext"))) {
		vm_ext = vtmp;
	}

	file_path = switch_mprintf("%s%smsg_%s.%s", dir_path, SWITCH_PATH_SEPARATOR, uuid, vm_ext);

	if ((voicemail_greeting_number = switch_channel_get_variable(channel, "voicemail_greeting_number"))) {
		int num = atoi(voicemail_greeting_number);
		if (num > 0 && num <= VM_MAX_GREETINGS) {
			greet_path = switch_mprintf("%s%sgreeting_%d.%s", dir_path, SWITCH_PATH_SEPARATOR, num, profile->file_ext);
		}
	} else {
		greet_path = cbt.greeting_path;
	}

  greet:
	memset(buf, 0, sizeof(buf));
	args.input_callback = cancel_on_dtmf;
	args.buf = buf;
	args.buflen = sizeof(buf);

	if (!switch_strlen_zero(greet_path)) {
		memset(buf, 0, sizeof(buf));
		TRY_CODE(switch_ivr_play_file(session, NULL, greet_path, &args));
	} else {
		if (!switch_strlen_zero(cbt.name_path)) {
			memset(buf, 0, sizeof(buf));
			TRY_CODE(switch_ivr_play_file(session, NULL, cbt.name_path, &args));
		}
		if (*buf == '\0') {
			const char *read_id;
			if (!(read_id = switch_channel_get_variable(channel, "voicemail_alternate_greet_id"))) {
				read_id = id;
			}
			memset(buf, 0, sizeof(buf));
			TRY_CODE(switch_ivr_phrase_macro(session, VM_PLAY_GREETING_MACRO, read_id, NULL, &args));
		}
	}

	if (*buf != '\0') {
		if (!strcasecmp(buf, profile->main_menu_key)) {
			voicemail_check_main(session, profile_name, domain_name, id, 0);
		} else if (!strcasecmp(buf, profile->operator_key) && !switch_strlen_zero(profile->operator_key)) {
			int argc;
			char *argv[4];
			char *mycmd;

			if (!switch_strlen_zero(profile->operator_ext) && (mycmd = switch_core_session_strdup(session, profile->operator_ext))) {
				argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
				if (argc >= 1 && argc <= 4) {
					switch_ivr_session_transfer(session, argv[0], argv[1], argv[2]);
					/* the application still runs after we leave it so we need to make sure that we dont do anything evil */
					send_mail = 0;
					goto end;
				}
			}
		} else if (!strcasecmp(buf, profile->vmain_key) && !switch_strlen_zero(profile->vmain_key)) {
			int argc;
			char *argv[4];
			char *mycmd;

			if (!switch_strlen_zero(profile->vmain_ext) && (mycmd = switch_core_session_strdup(session, profile->vmain_ext))) {
				argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
				if (argc >= 1 && argc <= 4) {
					switch_ivr_session_transfer(session, argv[0], argv[1], argv[2]);
					/* the application still runs after we leave it so we need to make sure that we dont do anything evil */
					send_mail = 0;
					goto end;
				}
			}
		} else {
			goto greet;
		}
	}

	memset(&fh, 0, sizeof(fh));
	args.input_callback = control_playback;
	memset(&cc, 0, sizeof(cc));
	cc.profile = profile;
	cc.fh = &fh;
	cc.noexit = 1;
	args.buf = &cc;

	dbuf = switch_mprintf("%s (%s)", caller_id_name, caller_id_number);
	switch_channel_set_variable(channel, "RECORD_ARTIST", dbuf);
	free(dbuf);

	switch_time_exp_lt(&tm, ts);
	switch_strftime(date, &retsize, sizeof(date), "%Y-%m-%d %T", &tm);
	switch_channel_set_variable(channel, "RECORD_DATE", date);
	switch_channel_set_variable(channel, "RECORD_SOFTWARE", "FreeSWITCH");
	switch_channel_set_variable(channel, "RECORD_TITLE", profile->record_title);
	switch_channel_set_variable(channel, "RECORD_COMMENT", profile->record_comment);
	switch_channel_set_variable(channel, "RECORD_COPYRIGHT", profile->record_copyright);

	status = create_file(session, profile, VM_RECORD_MESSAGE_MACRO, file_path, &message_len, SWITCH_TRUE);

	if ((status == SWITCH_STATUS_NOTFOUND)) {
		goto end;
	}

	if ((status == SWITCH_STATUS_SUCCESS || status == SWITCH_STATUS_BREAK) && switch_channel_ready(channel)) {
		char input[10] = "", key_buf[80] = "", term = 0;

		switch_snprintf(key_buf, sizeof(key_buf), "%s:%s", profile->urgent_key, profile->terminator_key);

		vm_macro_get(session, VM_RECORD_URGENT_CHECK_MACRO, key_buf, input, sizeof(input), 1, "", &term, profile->digit_timeout);
		if (*profile->urgent_key == *input) {
			read_flags = URGENT_FLAG_STRING;
			priority = 1;
			TRY_CODE(switch_ivr_phrase_macro(session, VM_ACK_MACRO, "marked-urgent", NULL, NULL));
		} else {
			TRY_CODE(switch_ivr_phrase_macro(session, VM_ACK_MACRO, "saved", NULL, NULL));
		}
	}
	
	switch_channel_get_variables(channel, &vars);
	status = deliver_vm(profile, x_user, domain_name, file_path, message_len, read_flags, vars, 
						switch_core_session_get_pool(session), caller_id_name, caller_id_number, SWITCH_FALSE);
	switch_event_destroy(&vars);
	if (status == SWITCH_STATUS_SUCCESS) {
		if ((vm_cc = switch_channel_get_variable(channel, "vm_cc"))) {
			char *cmd = switch_core_session_sprintf(session, "%s %s %s %s", vm_cc, file_path, caller_id_number, caller_id_name);
			if (voicemail_inject(cmd) == SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Sent Carbon Copy to %s\n", vm_cc);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to Carbon Copy to %s\n", vm_cc);
			}
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to deliver message\n");
		TRY_CODE(switch_ivr_phrase_macro(session, VM_ACK_MACRO, "deleted", NULL, NULL));
	}

 end:

	if (x_domain_root) {
		switch_xml_free(x_domain_root);
		x_domain_root = NULL;
	}

	switch_safe_free(file_path);

	if (switch_channel_ready(channel)) {
		status = switch_ivr_phrase_macro(session, VM_GOODBYE_MACRO, NULL, NULL, NULL);
	}

	return status;
}


#define VM_DESC "voicemail"
#define VM_USAGE "[check|auth] <profile_name> <domain_name> [<id>]"

SWITCH_STANDARD_APP(voicemail_function)
{
	int argc = 0;
	char *argv[6] = { 0 };
	char *mydata = NULL;
	const char *profile_name = NULL;
	const char *domain_name = NULL;
	const char *id = NULL;
	const char *auth_var = NULL;
	int x = 0, check = 0, auth = 0;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	if (!switch_strlen_zero(data)) {
		mydata = switch_core_session_strdup(session, data);
		argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	for (;;) {
		if (argv[x] && !strcasecmp(argv[x], "check")) {
			check++;
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

	if (switch_strlen_zero(profile_name)) {
		profile_name = switch_channel_get_variable(channel, "voicemail_profile_name");
	}

	if (switch_strlen_zero(domain_name)) {
		domain_name = switch_channel_get_variable(channel, "voicemail_domain_name");
	}

	if (switch_strlen_zero(id)) {
		id = switch_channel_get_variable(channel, "voicemail_id");
	}

	if (switch_strlen_zero(profile_name) || switch_strlen_zero(domain_name)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Usage: %s\n", VM_USAGE);
		return;
	}

	if (check) {
		voicemail_check_main(session, profile_name, domain_name, id, auth);
	} else {
		voicemail_leave_main(session, profile_name, domain_name, id);
	}
}

#define BOXCOUNT_SYNTAX "<user>@<domain>[|[new|saved|new-urgent|saved-urgent|all]]"
SWITCH_STANDARD_API(boxcount_api_function)
{
	char *dup;
	char *how = "new";
	int total_new_messages = 0;
	int total_saved_messages = 0;
	int total_new_urgent_messages = 0;
	int total_saved_urgent_messages = 0;

	if (cmd) {
		switch_hash_index_t *hi;
		void *val;
		vm_profile_t *profile;
		char *id, *domain, *p;

		dup = strdup(cmd);
		id = dup;

		if (!strncasecmp(cmd, "sip:", 4)) {
			id += 4;
		}

		if (!id) {
			stream->write_function(stream, "%d", 0);
			free(dup);
			return SWITCH_STATUS_SUCCESS;
		}

		if ((domain = strchr(id, '@'))) {
			*domain++ = '\0';
			if ((p = strchr(domain, '|'))) {
				*p++ = '\0';
				how = p;
			}

			for (hi = switch_hash_first(NULL, globals.profile_hash); hi; hi = switch_hash_next(hi)) {
				switch_hash_this(hi, NULL, NULL, &val);
				profile = (vm_profile_t *) val;
				total_new_messages = total_saved_messages = 0;
				message_count(profile, id, domain, "inbox", &total_new_messages, &total_saved_messages,
							  &total_new_urgent_messages, &total_saved_urgent_messages);
			}
		}

		switch_safe_free(id);
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

	return SWITCH_STATUS_SUCCESS;
}

static void message_query_handler(switch_event_t *event)
{
	char *account = switch_event_get_header(event, "message-account");
	int created = 0;
	switch_event_t *new_event = NULL;
	char *dup = NULL;

	if (account) {
		switch_hash_index_t *hi;
		int total_new_messages = 0;
		int total_saved_messages = 0;
		int total_new_urgent_messages = 0;
		int total_saved_urgent_messages = 0;
		void *val;
		vm_profile_t *profile;
		char *id, *domain;

		dup = strdup(account);
		id = dup;

		if (!strncasecmp(account, "sip:", 4)) {
			id += 4;
		}

		if (!id) {
			free(dup);
			return;
		}

		if ((domain = strchr(id, '@'))) {
			*domain++ = '\0';
			for (hi = switch_hash_first(NULL, globals.profile_hash); hi; hi = switch_hash_next(hi)) {
				switch_hash_this(hi, NULL, NULL, &val);
				profile = (vm_profile_t *) val;
				total_new_messages = total_saved_messages = 0;
				message_count(profile, id, domain, "inbox", &total_new_messages, &total_saved_messages,
							  &total_new_urgent_messages, &total_saved_urgent_messages);
				if (total_new_messages || total_saved_messages) {
					if (switch_event_create(&new_event, SWITCH_EVENT_MESSAGE_WAITING) == SWITCH_STATUS_SUCCESS) {
						const char *yn = "no";
						if (total_new_messages || total_new_urgent_messages) {
							yn = "yes";
						}
						switch_event_add_header_string(new_event, SWITCH_STACK_BOTTOM, "MWI-Messages-Waiting", yn);
						switch_event_add_header_string(new_event, SWITCH_STACK_BOTTOM, "MWI-Message-Account", account);
						switch_event_add_header(new_event, SWITCH_STACK_BOTTOM, "MWI-Voice-Message", "%d/%d (%d/%d)",
												total_new_messages, total_saved_messages, total_new_urgent_messages, total_saved_urgent_messages);
						created++;
					}
				}
			}
		}

		switch_safe_free(dup);

	}

	if (!created) {
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

#define VOICEMAIL_SYNTAX "rss [<host> <port> <uri> <user> <domain>]"

struct holder {
	vm_profile_t *profile;
	switch_memory_pool_t *pool;
	switch_stream_handle_t *stream;
	switch_xml_t xml;
	switch_xml_t x_item;
	switch_xml_t x_channel;
	int items;
	char *user;
	char *domain;
	char *host;
	char *port;
	char *uri;
};


static int del_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	if (argc > 8) {
		if (unlink(argv[8]) != 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "failed to delete file [%s]\n", argv[8]);
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

static void do_play(vm_profile_t *profile, char *user, char *domain, char *file, switch_stream_handle_t *stream)
{
	char *sql;
	struct holder holder;

	sql = switch_mprintf("update voicemail_msgs set read_epoch=%ld where username='%s' and domain='%s' and file_path like '%%%s'",
						 (long) switch_timestamp(NULL), user, domain, file);

	vm_execute_sql(profile, sql, profile->mutex);
	free(sql);

	sql = switch_mprintf("select * from voicemail_msgs where username='%s' and domain='%s' and file_path like '%%%s'", user, domain, file);
	memset(&holder, 0, sizeof(holder));
	holder.profile = profile;
	holder.stream = stream;
	switch_core_new_memory_pool(&holder.pool);
	vm_execute_sql_callback(profile, profile->mutex, sql, play_callback, &holder);
	switch_core_destroy_memory_pool(&holder.pool);
	switch_safe_free(sql);
}


static void do_del(vm_profile_t *profile, char *user, char *domain, char *file, switch_stream_handle_t *stream)
{
	char *myfolder = "inbox";
	char *sql;
	struct holder holder;
	char *ref = NULL;

	if (stream->param_event) {
		ref = switch_event_get_header(stream->param_event, "http-referer");
	}

	sql = switch_mprintf("select * from voicemail_msgs where username='%s' and domain='%s' and file_path like '%%%s'", user, domain, file);
	memset(&holder, 0, sizeof(holder));
	holder.profile = profile;
	holder.stream = stream;
	vm_execute_sql_callback(profile, profile->mutex, sql, del_callback, &holder);

	switch_safe_free(sql);
	sql = switch_mprintf("delete from voicemail_msgs where username='%s' and domain='%s' and file_path like '%%%s'", user, domain, file);
	vm_execute_sql(profile, sql, profile->mutex);
	free(sql);

	update_mwi(profile, user, domain, myfolder);

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
	char title_aft[128 * 3] = "";

	if (argc > 0) {
		l_created = switch_time_make (atol(argv[0]), 0);
	}

	if (argc > 1) {
		l_read = switch_time_make (atol(argv[1]), 0);
	}

	if (argc > 9) {
		l_duration = switch_time_make (atol(argv[9]), 0);
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
		switch_strftime(create_date, &retsize, sizeof(create_date), fmt, &tm);
		switch_strftime(rss_date, &retsize, sizeof(create_date), "%D %T", &tm);
	}

	if (l_read) {
		switch_time_exp_lt(&tm, l_read);
		switch_strftime(read_date, &retsize, sizeof(read_date), fmt, &tm);
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
		l_created = switch_time_make (atol(argv[0]), 0);
	}

	if (argc > 1) {
		l_read = switch_time_make (atol(argv[1]), 0);
	}

	if (argc > 9) {
		l_duration = switch_time_make (atol(argv[9]), 0);
	}

	switch_core_measure_time(l_duration, &duration);
	duration.day += duration.yr * 365;
	duration.hr += duration.day * 24;

	switch_snprintf(duration_str, sizeof(duration_str), "%.2u:%.2u:%.2u", duration.hr, duration.min, duration.sec);

	if (l_created) {
		switch_time_exp_lt(&tm, l_created);
		switch_strftime(create_date, &retsize, sizeof(create_date), fmt, &tm);
		switch_strftime(rss_date, &retsize, sizeof(create_date), fmt, &tm);
	}

	if (l_read) {
		switch_time_exp_lt(&tm, l_read);
		switch_strftime(read_date, &retsize, sizeof(read_date), fmt, &tm);
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

	sql = switch_mprintf("select * from voicemail_msgs where username='%s' and domain='%s' order by read_flags", user, domain);
	vm_execute_sql_callback(profile, profile->mutex, sql, rss_callback, &holder);

	xmlstr = switch_xml_toxml(holder.xml, SWITCH_TRUE);

	stream->write_function(stream, "%s", xmlstr);

	switch_safe_free(sql);
	switch_safe_free(xmlstr);
	switch_xml_free(holder.xml);
	switch_core_destroy_memory_pool(&holder.pool);
}


static void do_web(vm_profile_t *profile, char *user, char *domain, char *host, char *port, char *uri, switch_stream_handle_t *stream)
{
	char buf[80] = "";
	struct holder holder;
	char *sql;
	callback_t cbt = { 0 };
	int ttl = 0;

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

	sql = switch_mprintf("select * from voicemail_msgs where username='%s' and domain='%s' order by read_flags", user, domain);
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
}

#define VM_INJECT_USAGE "[tag=]<box> <sound_file> [<cid_num>] [<cid_name>]"
SWITCH_STANDARD_API(voicemail_inject_api_function)
{
	if (voicemail_inject(cmd) == SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "%s", "+OK\n");
	} else {
		stream->write_function(stream, "Error: %s\n", VM_INJECT_USAGE);
	}
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(voicemail_api_function)
{
	int argc = 0;
	char *mydata = NULL, *argv[6];
	char *host = NULL, *port = NULL, *uri = NULL;
	char *user = NULL, *domain = NULL;
	int ct = 0;
	vm_profile_t *profile = NULL;
	char *path_info = NULL;
	int rss = 0, xarg = 0;

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

	if (!switch_strlen_zero(cmd)) {
		mydata = strdup(cmd);
		switch_assert(mydata);
		argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}

	if (argc > 0) {
		if (!strcasecmp(argv[0], "rss")) {
			rss++;
			xarg++;
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

	profile = switch_core_hash_find(globals.profile_hash, domain);

	if (!profile) {
		profile = switch_core_hash_find(globals.profile_hash, "default");
	}

	if (!profile) {
		switch_hash_index_t *hi;
		void *val;

		for (hi = switch_hash_first(NULL, globals.profile_hash); hi; hi = switch_hash_next(hi)) {
			switch_hash_this(hi, NULL, NULL, &val);
			profile = (vm_profile_t *) val;
			if (profile) {
				break;
			}
		}
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


SWITCH_MODULE_LOAD_FUNCTION(mod_voicemail_load)
{
	switch_application_interface_t *app_interface;
	switch_api_interface_t *commands_api_interface;
	switch_status_t status;

	if ((status = load_config()) != SWITCH_STATUS_SUCCESS) {
		return status;
	}
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	SWITCH_ADD_APP(app_interface, "voicemail", "Voicemail", VM_DESC, voicemail_function, VM_USAGE, SAF_NONE);

	if (switch_event_bind(modname, SWITCH_EVENT_MESSAGE_QUERY, SWITCH_EVENT_SUBCLASS_ANY, message_query_handler, NULL)
		!= SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
		return SWITCH_STATUS_GENERR;
	}

	SWITCH_ADD_API(commands_api_interface, "voicemail", "voicemail", voicemail_api_function, VOICEMAIL_SYNTAX);
	SWITCH_ADD_API(commands_api_interface, "voicemail_inject", "voicemail_inject", voicemail_inject_api_function, VM_INJECT_USAGE);
	SWITCH_ADD_API(commands_api_interface, "vm_boxcount", "vm_boxcount", boxcount_api_function, BOXCOUNT_SYNTAX);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_NOUNLOAD;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */

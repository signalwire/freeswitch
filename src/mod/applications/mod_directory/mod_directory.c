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
 * Marc Olivier Chouinard <mochouinard at moctel dot com>
 * Emmanuel Schmidbauer <e.schmidbauer@gmail.com>
 *
 *
 * mod_directory.c -- Search by Name Directory IVR
 *
 */
#include <switch.h>

/* Prototypes */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_directory_shutdown);
SWITCH_MODULE_LOAD_FUNCTION(mod_directory_load);

SWITCH_MODULE_DEFINITION(mod_directory, mod_directory_load, mod_directory_shutdown, NULL);

static const char *global_cf = "directory.conf";

static char dir_sql[] =
	"CREATE TABLE directory_search (\n"
	"   hostname         VARCHAR(255),\n"
	"   uuid             VARCHAR(255),\n"
	"   extension        VARCHAR(255),\n"
	"   full_name        VARCHAR(255),\n"
	"   full_name_digit  VARCHAR(255),\n"
	"   first_name       VARCHAR(255),\n"
	"   first_name_digit VARCHAR(255),\n"
	"   last_name        VARCHAR(255),\n"
	"   last_name_digit  VARCHAR(255),\n" "   name_visible          INTEGER,\n" "   exten_visible         INTEGER\n" ");\n";

#define DIR_RESULT_ITEM "directory_result_item"
#define DIR_RESULT_SAY_NAME "directory_result_say_name"
#define DIR_RESULT_AT "directory_result_at"
#define DIR_RESULT_MENU "directory_result_menu"
#define DIR_INTRO "directory_intro"
#define DIR_MIN_SEARCH_DIGITS "directory_min_search_digits"
#define DIR_RESULT_COUNT "directory_result_count"
#define DIR_RESULT_COUNT_TOO_LARGE "directory_result_count_too_large"
#define DIR_RESULT_LAST "directory_result_last"

static switch_xml_config_string_options_t config_dtmf = { NULL, 2, "[0-9#\\*]" };
static switch_xml_config_int_options_t config_int_digit_timeout = { SWITCH_TRUE, 0, SWITCH_TRUE, 30000 };
static switch_xml_config_int_options_t config_int_ht_0 = { SWITCH_TRUE, 0 };

static struct {
	switch_hash_t *profile_hash;
	const char *hostname;
	int integer;
	int debug;
	char *dbname;
	switch_mutex_t *mutex;
	switch_memory_pool_t *pool;
	char odbc_dsn[1024];
} globals;

#define DIR_PROFILE_CONFIGITEM_COUNT 100

struct dir_profile {
	char *name;

	char next_key[2];
	char prev_key[2];
	char select_name_key[2];
	char new_search_key[2];

	char terminator_key[2];
	char switch_order_key[2];

	char *search_order;

	uint32_t min_search_digits;
	uint32_t max_menu_attempt;
	uint32_t digit_timeout;
	uint32_t max_result;
	switch_bool_t use_number_alias;
	switch_mutex_t *mutex;

	switch_thread_rwlock_t *rwlock;
	switch_memory_pool_t *pool;

	switch_xml_config_item_t config[DIR_PROFILE_CONFIGITEM_COUNT];
	switch_xml_config_string_options_t config_str_pool;
	uint32_t flags;
};

typedef struct dir_profile dir_profile_t;

typedef enum {
	PFLAG_DESTROY = 1 << 0
} dir_flags_t;

static int digit_matching_keypad(char letter)
{
	int result = -1;
	switch (toupper(letter)) {
	case 'A':
	case 'B':
	case 'C':
		result = 2;
		break;
	case 'D':
	case 'E':
	case 'F':
		result = 3;
		break;
	case 'G':
	case 'H':
	case 'I':
		result = 4;
		break;
	case 'J':
	case 'K':
	case 'L':
		result = 5;
		break;
	case 'M':
	case 'N':
	case 'O':
		result = 6;
		break;
	case 'P':
	case 'Q':
	case 'R':
	case 'S':
		result = 7;
		break;
	case 'T':
	case 'U':
	case 'V':
		result = 8;
		break;
	case 'W':
	case 'X':
	case 'Y':
	case 'Z':
		result = 9;
		break;
	}


	return result;

}

char *string_to_keypad_digit(const char *in)
{
	const char *s = NULL;
	char *dst = NULL;
	char *d = NULL;

	if (in) {
		s = in;
		dst = strdup(in);
		d = dst;

		while (*s) {
			char c;
			if ((c = (char)digit_matching_keypad(*s++)) > 0) {
				*d++ = c + 48;
			}
		}
		if (*d) {
			*d = '\0';
		}
	}
	return dst;
}

switch_cache_db_handle_t *directory_get_db_handle(void)
{
	switch_cache_db_handle_t *dbh = NULL;
	char *dsn;
	
	if (!zstr(globals.odbc_dsn)) {
		dsn = globals.odbc_dsn;
	} else {
		dsn = globals.dbname;
	}

	if (switch_cache_db_get_db_handle_dsn(&dbh, dsn) != SWITCH_STATUS_SUCCESS) {
		dbh = NULL;
	}
	
	return dbh;

}

static switch_status_t directory_execute_sql(char *sql, switch_mutex_t *mutex)
{
	switch_cache_db_handle_t *dbh = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (mutex) {
		switch_mutex_lock(mutex);
	}

	if (!(dbh = directory_get_db_handle())) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Opening DB\n");
		goto end;
	}

	if (globals.debug > 1) switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "sql: %s\n", sql);

	status = switch_cache_db_execute_sql(dbh, sql, NULL);

end:

	switch_cache_db_release_db_handle(&dbh);

	if (mutex) {
		switch_mutex_unlock(mutex);
	}

	return status;
}

typedef enum {
	ENTRY_MOVE_NEXT,
	ENTRY_MOVE_PREV
} entry_move_t;

typedef enum {
	SEARCH_BY_FIRST_NAME,
	SEARCH_BY_LAST_NAME,
	SEARCH_BY_FIRST_AND_LAST_NAME,
	SEARCH_BY_FULL_NAME
} search_by_t;

struct search_params {
	char digits[255];
	char transfer_to[255];
	char domain[255];
	char profile[255];
	search_by_t search_by;
	int timeout;
	int try_again;
};
typedef struct search_params search_params_t;

struct listing_callback {
	char extension[255];
	char fullname[255];
	char first_name[255];
	char last_name[255];
	char transfer_to[255];
	int name_visible;
	int exten_visible;
	int new_search;
	int index;
	int want;
	entry_move_t move;
	search_params_t *params;
};
typedef struct listing_callback listing_callback_t;

static int listing_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	listing_callback_t *cbt = (listing_callback_t *) pArg;
	if (cbt->index++ != cbt->want) {
		return 0;
	}
	switch_copy_string(cbt->extension, argv[0], 255);
	switch_copy_string(cbt->fullname, argv[1], 255);
	switch_copy_string(cbt->last_name, argv[2], 255);
	switch_copy_string(cbt->first_name, argv[3], 255);
	cbt->name_visible = atoi(argv[4]);
	cbt->exten_visible = atoi(argv[5]);
	return -1;
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

static switch_bool_t directory_execute_sql_callback(switch_mutex_t *mutex, char *sql, switch_core_db_callback_func_t callback, void *pdata)
{
	switch_bool_t ret = SWITCH_FALSE;
	switch_cache_db_handle_t *dbh = NULL;
	char *errmsg = NULL;

	if (mutex) {
		switch_mutex_lock(mutex);
	}

	if (!(dbh = directory_get_db_handle())) {
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

#define DIR_DESC "directory"
#define DIR_USAGE "<profile_name> <domain_name> [<context_name>] | [<dialplan_name> <context_name>]"

static void free_profile(dir_profile_t *profile)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Destroying Profile %s\n", profile->name);
	switch_core_destroy_memory_pool(&profile->pool);
}

static void profile_rwunlock(dir_profile_t *profile)
{
	switch_thread_rwlock_unlock(profile->rwlock);
	if (switch_test_flag(profile, PFLAG_DESTROY)) {
		if (switch_thread_rwlock_tryrdlock(profile->rwlock) == SWITCH_STATUS_SUCCESS) {
			free_profile(profile);
		}
	}
}

dir_profile_t *profile_set_config(dir_profile_t *profile)
{
	int i = 0;

	profile->config_str_pool.pool = profile->pool;

	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "next-key", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE, &profile->next_key, "6", &config_dtmf, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "prev-key", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE, &profile->prev_key, "4", &config_dtmf, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "terminator-key", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->terminator_key, "#", &config_dtmf, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "switch-order-key", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->switch_order_key, "*", &config_dtmf, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "select-name-key", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->select_name_key, "1", &config_dtmf, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "new-search-key", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->new_search_key, "3", &config_dtmf, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "search-order", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE,
						   &profile->search_order, "last_name", &profile->config_str_pool, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "digit-timeout", SWITCH_CONFIG_INT, CONFIG_RELOADABLE,
						   &profile->digit_timeout, 3000, &config_int_digit_timeout, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "min-search-digits", SWITCH_CONFIG_INT, CONFIG_RELOADABLE,
						   &profile->min_search_digits, 3, &config_int_ht_0, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "max-menu-attempts", SWITCH_CONFIG_INT, CONFIG_RELOADABLE,
						   &profile->max_menu_attempt, 3, &config_int_ht_0, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "max-result", SWITCH_CONFIG_INT, CONFIG_RELOADABLE,
						   &profile->max_result, 5, &config_int_ht_0, NULL, NULL);
	SWITCH_CONFIG_SET_ITEM(profile->config[i++], "use-number-alias", SWITCH_CONFIG_BOOL, CONFIG_RELOADABLE,
						   &profile->use_number_alias, SWITCH_FALSE, NULL, NULL, NULL);

	return profile;

}

static dir_profile_t *load_profile(const char *profile_name)
{
	dir_profile_t *profile = NULL;
	switch_xml_t x_profiles, x_profile, cfg, xml = NULL;
	switch_event_t *event = NULL;

	if (!(xml = switch_xml_open_cfg(global_cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", global_cf);
		return profile;
	}
	if (!(x_profiles = switch_xml_child(cfg, "profiles"))) {
		goto end;
	}

	if ((x_profile = switch_xml_find_child(x_profiles, "profile", "name", profile_name))) {
		switch_memory_pool_t *pool;
		int count;

		if (switch_core_new_memory_pool(&pool) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Pool Failure\n");
			goto end;
		}

		if (!(profile = switch_core_alloc(pool, sizeof(dir_profile_t)))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Alloc Failure\n");
			switch_core_destroy_memory_pool(&pool);
			goto end;
		}

		profile->pool = pool;
		profile_set_config(profile);

		/* Add the params to the event structure */
		count = (int)switch_event_import_xml(switch_xml_child(x_profile, "param"), "name", "value", &event);

		if (switch_xml_config_parse_event(event, count, SWITCH_FALSE, profile->config) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to process configuration\n");
			switch_core_destroy_memory_pool(&pool);
			goto end;
		}

		switch_thread_rwlock_create(&profile->rwlock, pool);
		profile->name = switch_core_strdup(pool, profile_name);

		switch_mutex_init(&profile->mutex, SWITCH_MUTEX_NESTED, profile->pool);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Added Profile %s\n", profile->name);
		switch_core_hash_insert(globals.profile_hash, profile->name, profile);
	}

  end:
	switch_xml_free(xml);

	return profile;
}

static switch_status_t load_config(switch_bool_t reload)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_xml_t cfg, xml = NULL, settings, param, x_profiles, x_profile;
	switch_cache_db_handle_t *dbh = NULL;

	if (!(xml = switch_xml_open_cfg(global_cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", global_cf);
		return SWITCH_STATUS_TERM;
	}

	switch_mutex_lock(globals.mutex);
	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcasecmp(var, "odbc-dsn") && !zstr(val)) {
				if (switch_odbc_available() || switch_pgsql_available()) {
					switch_set_string(globals.odbc_dsn, val);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ODBC IS NOT AVAILABLE!\n");
				}
			} else if (!strcasecmp(var, "dbname") && !zstr(val)) {
				globals.dbname = switch_core_strdup(globals.pool, val);
			}

			if (!strcasecmp(var, "debug")) {
				globals.debug = atoi(val);
			}
		}
	}

	if ((x_profiles = switch_xml_child(cfg, "profiles"))) {
		for (x_profile = switch_xml_child(x_profiles, "profile"); x_profile; x_profile = x_profile->next) {
			load_profile(switch_xml_attr_soft(x_profile, "name"));
		}
	}

	if (zstr(globals.odbc_dsn) && zstr(globals.dbname)) {
		globals.dbname = switch_core_sprintf(globals.pool, "directory");
	}

	dbh = directory_get_db_handle();
	if (dbh) {
		if (!reload) {
			switch_cache_db_test_reactive(dbh, "delete from directory_search where uuid != '' and name_visible != '' ", "drop table directory_search", dir_sql);
		}
		switch_cache_db_release_db_handle(&dbh);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Cannot open DB!2\n");
		status = SWITCH_STATUS_TERM;
		goto end;
	}
end:
	switch_mutex_unlock(globals.mutex);

	switch_xml_free(xml);
	return status;
}

static dir_profile_t *get_profile(const char *profile_name)
{
	dir_profile_t *profile = NULL;

	switch_mutex_lock(globals.mutex);
	if (!(profile = switch_core_hash_find(globals.profile_hash, profile_name))) {
		profile = load_profile(profile_name);
	}
	if (profile) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[%s] rwlock\n", profile->name);

		switch_thread_rwlock_rdlock(profile->rwlock);
	}
	switch_mutex_unlock(globals.mutex);

	return profile;
}

char *generate_sql_entry_for_user(switch_core_session_t *session, switch_xml_t ut, switch_bool_t use_number_alias) {
	char *sql = NULL;

	int name_visible = 1;
	int exten_visible = 1;
	const char *id = switch_xml_attr_soft(ut, "id");
	const char *number_alias = switch_xml_attr_soft(ut, "number-alias");
	const char *extension = NULL;
	char *fullName = NULL;
	char *caller_name = NULL;
	char *caller_name_override = NULL;
	char *firstName = NULL;
	char *lastName = NULL;
	char *fullNameDigit = NULL;
	char *firstNameDigit = NULL;
	char *lastNameDigit = NULL;
	switch_xml_t x_params, x_param, x_vars, x_var;

	/* Check all the user params */
	if ((x_params = switch_xml_child(ut, "params"))) {
		for (x_param = switch_xml_child(x_params, "param"); x_param; x_param = x_param->next) {
			const char *var = switch_xml_attr_soft(x_param, "name");
			const char *val = switch_xml_attr_soft(x_param, "value");
			if (!strcasecmp(var, "directory-visible")) {
				name_visible = switch_true(val);
			}
			if (!strcasecmp(var, "directory-exten-visible")) {
				exten_visible = switch_true(val);
			}

		}
	}
	/* Check all the user variables */
	if ((x_vars = switch_xml_child(ut, "variables"))) {
		for (x_var = switch_xml_child(x_vars, "variable"); x_var; x_var = x_var->next) {
			const char *var = switch_xml_attr_soft(x_var, "name");
			const char *val = switch_xml_attr_soft(x_var, "value");
			if (!strcasecmp(var, "effective_caller_id_name")) {
				caller_name = switch_core_session_strdup(session, val);
			}
			if (!strcasecmp(var, "directory_full_name")) {
				caller_name_override = switch_core_session_strdup(session, val);
			}
		}
	}
	if (caller_name_override) {
		fullName = caller_name_override;
	} else {
		fullName = caller_name;
	}
	if (zstr(fullName)) {
		goto end;
	}
	firstName = switch_core_session_strdup(session, fullName);

	if ((lastName = strrchr(firstName, ' '))) {
		*lastName++ = '\0';
	} else {
		lastName = switch_core_session_strdup(session, firstName);
	}

	/* switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "FullName %s firstName [%s] lastName [%s]\n", fullName, firstName, lastName); */
	if (use_number_alias == SWITCH_TRUE && !zstr(number_alias)) {
		extension = number_alias;
	} else {
		extension = id;
	}

	/* Generate Digits key mapping */
	fullNameDigit = string_to_keypad_digit(fullName);
	lastNameDigit = string_to_keypad_digit(lastName);
	firstNameDigit = string_to_keypad_digit(firstName);

	/* add user into DB */
	sql = switch_mprintf("insert into directory_search values('%q','%q','%q','%q','%q','%q','%q','%q','%q','%d','%d')",
			globals.hostname, switch_core_session_get_uuid(session), extension, fullName, fullNameDigit, firstName, firstNameDigit,
			lastName, lastNameDigit, name_visible, exten_visible);

	switch_safe_free(fullNameDigit);
	switch_safe_free(lastNameDigit);
	switch_safe_free(firstNameDigit);

end:

	return sql;
}
static switch_status_t populate_database(switch_core_session_t *session, dir_profile_t *profile, const char *domain_name)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	char *sql = NULL;
	char *sqlvalues = NULL;
	char *sqltmp = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	const char *group_selection = switch_channel_get_variable(channel, "directory_group_selection");
	int count = 0;

	switch_xml_t xml_root = NULL, x_domain;
	switch_xml_t ut;

	switch_event_t *xml_params = NULL;
	switch_xml_t group = NULL, groups = NULL, users = NULL;
	switch_event_create(&xml_params, SWITCH_EVENT_REQUEST_PARAMS);
	switch_assert(xml_params);

	if (switch_xml_locate_domain(domain_name, xml_params, &xml_root, &x_domain) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Cannot locate domain %s\n", domain_name);
		status = SWITCH_STATUS_FALSE;
		goto end;
	}

	if ((groups = switch_xml_child(x_domain, "groups"))) {
		for (group = switch_xml_child(groups, "group"); group; group = group->next) {
			const char *gname = switch_xml_attr_soft(group, "name");

			if (group_selection && strcasecmp(gname, group_selection)) {
				continue;
			}

			if ((users = switch_xml_child(group, "users"))) {
				for (ut = switch_xml_child(users, "user"); ut; ut = ut->next) {
					const char *uname = switch_xml_attr_soft(ut, "id");
					const char *type = switch_xml_attr_soft(ut, "type");

					if (!strcasecmp(type, "pointer")) {
						switch_xml_t ux;

						if (switch_xml_locate_user_merged("id", uname, domain_name, NULL, &ux, NULL) != SWITCH_STATUS_SUCCESS) {
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Can't find user [%s@%s] from pointer\n", uname, domain_name);
						} else {
							sql = generate_sql_entry_for_user(session, ux, profile->use_number_alias);
							switch_xml_free(ux);
						}
						
					} else {
						sql = generate_sql_entry_for_user(session, ut, profile->use_number_alias);
					}

					if (sql) {
						if (sqlvalues) {
							sqltmp = sqlvalues;
							sqlvalues = switch_mprintf("%s;%s", sqltmp, sql);
							switch_safe_free(sqltmp);
							switch_safe_free(sql);
						} else {
							sqlvalues = sql;
							sql = NULL;
						}
					}
					
					if (++count >= 100) {
						count = 0;
						sql = switch_mprintf("BEGIN;%s;COMMIT;", sqlvalues);
						directory_execute_sql(sql, globals.mutex);
						switch_safe_free(sql);
						switch_safe_free(sqlvalues);
					}
				}
			}
		}
	}
	if (sqlvalues) {
		sql = switch_mprintf("BEGIN;%s;COMMIT;", sqlvalues);
		directory_execute_sql(sql, globals.mutex);
	}
  end:
	switch_safe_free(sql);
	switch_safe_free(sqlvalues);
	switch_event_destroy(&xml_params);
	switch_xml_free(xml_root);

	return status;
}

struct cb_result {
	char digits[255];
	char digit;
	dir_profile_t *profile;
};

typedef struct cb_result cbr_t;

static switch_status_t on_dtmf(switch_core_session_t *session, void *input, switch_input_type_t itype, void *buf, unsigned int buflen)
{
	switch (itype) {
	case SWITCH_INPUT_TYPE_DTMF:
		{
			switch_dtmf_t *dtmf = (switch_dtmf_t *) input;
			cbr_t *cbr = (cbr_t *) buf;
			cbr->digit = dtmf->digit;
			if (dtmf->digit == *cbr->profile->terminator_key || dtmf->digit == *cbr->profile->switch_order_key) {
				return SWITCH_STATUS_BREAK;
			}

			if (strlen(cbr->digits) < sizeof(cbr->digits) - 2) {
				int at = (int)strlen(cbr->digits);
				cbr->digits[at++] = dtmf->digit;
				cbr->digits[at] = '\0';
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "DTMF buffer is full\n");
				return SWITCH_STATUS_BREAK;
			}
		}
		break;
	default:
		break;
	}

	return SWITCH_STATUS_BREAK;
}

static switch_status_t listen_entry(switch_core_session_t *session, dir_profile_t *profile, listing_callback_t *cbt)
{
	char buf[2] = "";
	char macro[256] = "";
	char recorded_name[256] = "";

	/* Try to use the recorded name from voicemail if it exist */
	if (switch_loadable_module_exists("mod_voicemail") == SWITCH_STATUS_SUCCESS) {
		char *cmd = NULL;
		switch_stream_handle_t stream = { 0 };
		SWITCH_STANDARD_STREAM(stream);

		cmd = switch_core_session_sprintf(session, "%s/%s@%s|name_path", cbt->params->profile, cbt->extension, cbt->params->domain);
		switch_api_execute("vm_prefs", cmd, session, &stream);
		if (strncmp("-ERR", stream.data, 4)) {
			switch_copy_string(recorded_name, (char *) stream.data, sizeof(recorded_name));
		}
		switch_safe_free(stream.data);
	}

	if (zstr_buf(buf)) {
		switch_snprintf(macro, sizeof(macro), "phrase:%s:%d", DIR_RESULT_ITEM, cbt->want + 1);
		switch_ivr_read(session, 0, 1, macro, NULL, buf, sizeof(buf), 1, profile->terminator_key, 0);
	}

	if (!zstr_buf(recorded_name) && zstr_buf(buf)) {
		switch_ivr_read(session, 0, 1, recorded_name, NULL, buf, sizeof(buf), 1, profile->terminator_key, 0);

	}
	if (zstr_buf(recorded_name) && zstr_buf(buf)) {
		switch_snprintf(macro, sizeof(macro), "phrase:%s:%s", DIR_RESULT_SAY_NAME, cbt->fullname);
		switch_ivr_read(session, 0, 1, macro, NULL, buf, sizeof(buf), 1, profile->terminator_key, 0);
	}
	if (cbt->exten_visible && zstr_buf(buf)) {
		switch_snprintf(macro, sizeof(macro), "phrase:%s:%s", DIR_RESULT_AT, cbt->extension);
		switch_ivr_read(session, 0, 1, macro, NULL, buf, sizeof(buf), 1, profile->terminator_key, 0);
	}
	if (zstr_buf(buf)) {
		switch_snprintf(macro, sizeof(macro), "phrase:%s:%c,%c,%c,%c", DIR_RESULT_MENU, *profile->select_name_key, *profile->next_key, *profile->prev_key,
						*profile->new_search_key);
		switch_ivr_read(session, 0, 1, macro, NULL, buf, sizeof(buf), profile->digit_timeout, profile->terminator_key, 0);
	}

	if (!zstr_buf(buf)) {
		if (buf[0] == *profile->select_name_key) {
			switch_copy_string(cbt->transfer_to, cbt->extension, 255);
		}
		if (buf[0] == *profile->new_search_key) {
			cbt->new_search = 1;
		}
		if (buf[0] == *profile->prev_key) {
			cbt->move = ENTRY_MOVE_PREV;
		}
	} else {
		return SWITCH_STATUS_TIMEOUT;
	}

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t gather_name_digit(switch_core_session_t *session, dir_profile_t *profile, search_params_t *params)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	cbr_t cbr;
	int loop = 1;

	switch_input_args_t args = { 0 };
	args.input_callback = on_dtmf;
	args.buf = &cbr;

	while (switch_channel_ready(channel) && loop) {
		char macro[255];
		loop = 0;
		memset(&cbr, 0, sizeof(cbr));
		cbr.profile = profile;
		params->timeout = 0;

		/* Gather the user Name */

		switch_snprintf(macro, sizeof(macro), "%s:%c", (params->search_by == SEARCH_BY_LAST_NAME ? "last_name" : "first_name"), *profile->switch_order_key);
		switch_ivr_phrase_macro(session, DIR_INTRO, macro, NULL, &args);

		while (switch_channel_ready(channel)) {
			if (cbr.digit == *profile->terminator_key) {
				status = SWITCH_STATUS_BREAK;
				break;
			}
			if (cbr.digit == *profile->switch_order_key) {
				if (params->search_by == SEARCH_BY_LAST_NAME) {
					params->search_by = SEARCH_BY_FIRST_NAME;
				} else {
					params->search_by = SEARCH_BY_LAST_NAME;
				}
				loop = 1;
				break;
			}

			if (switch_ivr_collect_digits_callback(session, &args, profile->digit_timeout, 0) == SWITCH_STATUS_TIMEOUT) {
				params->timeout = 1;
				break;
			}

		}
	}
	switch_copy_string(params->digits, cbr.digits, 255);

	return status;
}

switch_status_t navigate_entrys(switch_core_session_t *session, dir_profile_t *profile, search_params_t *params)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	char *sql = NULL, *sql_where = NULL;
	char entry_count[80] = "";
	callback_t cbt = { 0 };
	int result_count;
	char macro[256] = "";
	listing_callback_t listing_cbt;
	int cur_entry = 0;
	cbt.buf = entry_count;
	cbt.len = sizeof(entry_count);

	if (params->search_by == SEARCH_BY_FIRST_AND_LAST_NAME) {
		sql_where = switch_mprintf("hostname = '%q' and uuid = '%q' and name_visible = 1 and (%s like '%q%%' or %s like '%q%%')",
				globals.hostname, switch_core_session_get_uuid(session), "last_name_digit", params->digits, "first_name_digit", params->digits);
	} else if (params->search_by == SEARCH_BY_FULL_NAME) {
		sql_where = switch_mprintf("hostname = '%q' and uuid = '%q' and name_visible = 1 and full_name_digit like '%%%q%%'",
				globals.hostname, switch_core_session_get_uuid(session), "last_name_digit", params->digits, "first_name_digit", params->digits);
	} else {
		sql_where = switch_mprintf("hostname = '%q' and uuid = '%q' and name_visible = 1 and %s like '%q%%'",
				globals.hostname, switch_core_session_get_uuid(session), (params->search_by == SEARCH_BY_LAST_NAME ? "last_name_digit" : "first_name_digit"), params->digits);
	}

	sql = switch_mprintf("select count(*) from directory_search where %s group by last_name, first_name, extension", sql_where);

	directory_execute_sql_callback(globals.mutex, sql, sql2str_callback, &cbt);
	switch_safe_free(sql);

	result_count = atoi(entry_count);

	if (result_count == 0) {
		switch_snprintf(macro, sizeof(macro), "%d", result_count);
		switch_ivr_phrase_macro(session, DIR_RESULT_COUNT, macro, NULL, NULL);
		params->try_again = 1;
		status = SWITCH_STATUS_BREAK;
		goto end;
	} else if (profile->max_result != 0 && (uint32_t)result_count > profile->max_result) {
		switch_ivr_phrase_macro(session, DIR_RESULT_COUNT_TOO_LARGE, NULL, NULL, NULL);
		params->try_again = 1;
		status = SWITCH_STATUS_BREAK;
		goto end;

	} else {
		switch_snprintf(macro, sizeof(macro), "%d", result_count);
		switch_ivr_phrase_macro(session, DIR_RESULT_COUNT, macro, NULL, NULL);
	}

	memset(&listing_cbt, 0, sizeof(listing_cbt));
	listing_cbt.params = params;

	sql = switch_mprintf("select extension, full_name, last_name, first_name, name_visible, exten_visible from directory_search where %s group by extension, full_name, last_name, first_name, name_visible, exten_visible order by last_name, first_name", sql_where);

	for (cur_entry = 0; cur_entry < result_count; cur_entry++) {
		listing_cbt.index = 0;
		listing_cbt.want = cur_entry;
		listing_cbt.move = ENTRY_MOVE_NEXT;
		directory_execute_sql_callback(globals.mutex, sql, listing_callback, &listing_cbt);
		status = listen_entry(session, profile, &listing_cbt);
		if (!zstr(listing_cbt.transfer_to)) {
			switch_copy_string(params->transfer_to, listing_cbt.transfer_to, 255);
			break;
		}
		if (listing_cbt.new_search) {
			params->try_again = 1;
			goto end;

		}
		if (listing_cbt.move == ENTRY_MOVE_NEXT) {
			if (cur_entry == result_count - 1) {
				switch_snprintf(macro, sizeof(macro), "%d", result_count);
				switch_ivr_phrase_macro(session, DIR_RESULT_LAST, macro, NULL, NULL);
				cur_entry -= 1;
			}
		}
		if (listing_cbt.move == ENTRY_MOVE_PREV) {
			if (cur_entry <= 0) {
				cur_entry = -1;
			} else {
				cur_entry -= 2;
			}
		}
		if (status == SWITCH_STATUS_TIMEOUT) {
			goto end;
		}
		if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_BREAK) {
			goto end;
		}
	}

  end:
	switch_safe_free(sql);
	switch_safe_free(sql_where);
	return status;

}

SWITCH_STANDARD_APP(directory_function)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	int argc = 0;
	char *argv[6] = { 0 };
	char *mydata = NULL;
	const char *profile_name = NULL;
	const char *domain_name = NULL;
	const char *context_name = NULL;
	const char *dialplan_name = NULL;
	const char *search_by = NULL;
	dir_profile_t *profile = NULL;
	int x = 0;
	char *sql = NULL;
	search_params_t s_param;
	int attempts = 3;
	char macro[255];

	if (zstr(data)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Missing profile name\n");
		return;
	}

	mydata = switch_core_session_strdup(session, data);
	if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0])))) < 2) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Not enough args [%s]\n", data);
		return;
	}

	if (argv[x]) {
		profile_name = argv[x++];
	}

	if (argv[x]) {
		domain_name = argv[x++];
	}

	if (argv[x]) {
		if (!(argv[x+1])) {
			context_name = argv[x++];
		} else {
			dialplan_name = argv[x++];
		}
	}

	if (argv[x]) {
		context_name = argv[x++];
	}

	if (!(profile = get_profile(profile_name))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error invalid profile %s\n", profile_name);
		return;
	}

	if (!context_name) {
		context_name = domain_name;
	}

	if (!dialplan_name) {
		dialplan_name = "XML";
	}

	populate_database(session, profile, domain_name);

	memset(&s_param, 0, sizeof(s_param));
	s_param.try_again = 1;
	switch_copy_string(s_param.profile, profile_name, 255);
	switch_copy_string(s_param.domain, domain_name, 255);

	if (!(search_by = switch_channel_get_variable(channel, "directory_search_order"))) { 
		search_by = profile->search_order;
	}

	if (!strcasecmp(search_by, "first_name")) {
		s_param.search_by = SEARCH_BY_FIRST_NAME;
	} else if (!strcasecmp(search_by, "first_and_last_name")) {
		s_param.search_by = SEARCH_BY_FIRST_AND_LAST_NAME;
	} else {
		s_param.search_by = SEARCH_BY_LAST_NAME;
	}

	attempts = profile->max_menu_attempt;
	s_param.try_again = 1;
	while (switch_channel_ready(channel) && (s_param.try_again && attempts-- > 0)) {
		s_param.try_again = 0;
		gather_name_digit(session, profile, &s_param);

		if (zstr(s_param.digits)) {
			s_param.try_again = 1;
			continue;
		}

		if (strlen(s_param.digits) < profile->min_search_digits) {
			switch_snprintf(macro, sizeof(macro), "%d", profile->min_search_digits);
			switch_ivr_phrase_macro(session, DIR_MIN_SEARCH_DIGITS, macro, NULL, NULL);
			s_param.try_again = 1;
			continue;
		}

		navigate_entrys(session, profile, &s_param);
	}

	if (!zstr(s_param.transfer_to)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Directory transfering call to : %s\n", s_param.transfer_to);
		switch_ivr_session_transfer(session, s_param.transfer_to, dialplan_name, context_name);
	}

	/* Delete all sql entry for this call */
	sql = switch_mprintf("delete from directory_search where hostname = '%q' and uuid = '%q'", globals.hostname, switch_core_session_get_uuid(session));
	directory_execute_sql(sql, globals.mutex);
	switch_safe_free(sql);
	profile_rwunlock(profile);
}

SWITCH_MODULE_LOAD_FUNCTION(mod_directory_load)
{
	switch_application_interface_t *app_interface;
	switch_status_t status;

	memset(&globals, 0, sizeof(globals));
	globals.pool = pool;

	switch_core_hash_init(&globals.profile_hash);
	switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, globals.pool);

	if ((status = load_config(SWITCH_FALSE)) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	globals.hostname = switch_core_get_switchname();

	SWITCH_ADD_APP(app_interface, "directory", "directory", DIR_DESC, directory_function, DIR_USAGE, SAF_NONE);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

/*
   Called when the system shuts down
   Macro expands to: switch_status_t mod_directory_shutdown() */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_directory_shutdown)
{
	switch_hash_index_t *hi = NULL;
	dir_profile_t *profile;
	void *val = NULL;
	const void *key;
	switch_ssize_t keylen;
	char *sql = NULL;

	switch_mutex_lock(globals.mutex);

	while ((hi = switch_core_hash_first_iter(globals.profile_hash, hi))) {
		switch_core_hash_this(hi, &key, &keylen, &val);
		profile = (dir_profile_t *) val;

		switch_core_hash_delete(globals.profile_hash, profile->name);

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Waiting for write lock (Profile %s)\n", profile->name);
		switch_thread_rwlock_wrlock(profile->rwlock);

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Destroying Profile %s\n", profile->name);
		switch_core_destroy_memory_pool(&profile->pool);
		profile = NULL;
	}

	sql = switch_mprintf("delete from directory_search where hostname = '%q'", globals.hostname);
	directory_execute_sql(sql, globals.mutex);
	switch_safe_free(sql);

	switch_mutex_unlock(globals.mutex);
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet
 */

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
 * Raymond Chandler <intralanman@gmail.com>
 * Rupa Schomaker <rupa@rupa.com>
 * Emmanuel Schmidbauer <e.schmidbauer@gmail.com>
 *
 * mod_lcr.c -- Least Cost Routing Module
 *
 */

#include <switch.h>

#define LCR_SYNTAX "lcr <digits> [<lcr profile>] [caller_id] [intrastate] [as xml]"
#define LCR_ADMIN_SYNTAX "lcr_admin show profiles"

#define LCR_HEADERS_COUNT 7

#define LCR_HEADERS_DIGITS 0
#define LCR_HEADERS_CARRIER 1
#define LCR_HEADERS_RATE 2
#define LCR_HEADERS_DIALSTRING 3
#define LCR_HEADERS_CODEC 4
#define LCR_HEADERS_CID 5
#define LCR_HEADERS_LIMIT 6

static char headers[LCR_HEADERS_COUNT][32] = {
	"Digit Match",
	"Carrier",
	"Rate",
	"Dialstring",
	"Codec",
	"CID Regexp",
	"Limit",
};

/* sql for random function */
static char *db_random = NULL;

struct lcr_obj {
	char *carrier_name;
	char *gw_prefix;
	char *gw_suffix;
	char *digit_str;
	char *prefix;
	char *suffix;
	char *dialstring;
	float rate;
	char *rate_str;
	float user_rate;
	char *user_rate_str;
	size_t lstrip;
	size_t tstrip;
	size_t digit_len;
	char *codec;
	char *cid;
	char *limit_realm;
	char *limit_id;
	int limit_max;
	switch_event_t *fields;
	struct lcr_obj *prev;
	struct lcr_obj *next;
};

struct max_obj {
	size_t carrier_name;
	size_t digit_str;
	size_t rate;
	size_t codec;
	size_t cid;
	size_t dialstring;
	size_t limit;
};

typedef struct lcr_obj lcr_obj_t;
typedef lcr_obj_t *lcr_route;

typedef struct max_obj max_obj_t;
typedef max_obj_t *max_len;

struct profile_obj {
	char *name;
	uint16_t id;
	char *order_by;
	char *custom_sql;
	char *export_fields_str;
	int export_fields_cnt;
	char **export_fields;
	char *limit_type;
	switch_bool_t custom_sql_has_percent;
	switch_bool_t custom_sql_has_vars;
	switch_bool_t profile_has_intrastate;
	switch_bool_t profile_has_intralata;
	switch_bool_t profile_has_npanxx;

	switch_bool_t reorder_by_rate;
	switch_bool_t quote_in_list;
	switch_bool_t single_bridge;
	switch_bool_t info_in_headers;
	switch_bool_t enable_sip_redir;
};
typedef struct profile_obj profile_t;

struct callback_obj {
	lcr_route head;
	switch_hash_t *dedup_hash;
	int matches;
	switch_memory_pool_t *pool;
	char *lookup_number;
	char *lrn_number;
	char *cid;
	switch_bool_t intrastate;
	switch_bool_t intralata;
	profile_t *profile;
	switch_core_session_t *session;
	switch_event_t *event;
	float max_rate;
};
typedef struct callback_obj callback_t;

static struct {
	switch_memory_pool_t *pool;
	char *dbname;
	char *odbc_dsn;
	switch_mutex_t *mutex;
	switch_hash_t *profile_hash;
	profile_t *default_profile;
	void *filler1;
} globals;


SWITCH_MODULE_LOAD_FUNCTION(mod_lcr_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_lcr_shutdown);
SWITCH_MODULE_DEFINITION(mod_lcr, mod_lcr_load, mod_lcr_shutdown, NULL);

static void lcr_destroy(lcr_route route)
{
	while(route) {
		switch_event_destroy(&route->fields);
		route=route->next;
	}
}

static const char *do_cid(switch_memory_pool_t *pool, const char *cid, const char *number, switch_core_session_t *session)
{
	switch_regex_t *re = NULL;
	int proceed = 0, ovector[30];
	char *substituted = NULL;
	uint32_t len = 0;
	char *src = NULL;
	char *dst = NULL;
	char *tmp_regex = NULL;
	char *src_regex = NULL;
	char *dst_regex = NULL;
	switch_channel_t *channel = NULL;

	if (!zstr(cid)) {
		len = (uint32_t)strlen(cid);
	} else {
		goto done;
	}

	src = switch_core_strdup(pool, cid);
	/* check that this is a valid regexp and split the string */

	if ((src[0] == '/') && src[len-1] == '/') {
		/* strip leading / trailing slashes */
		src[len-1] = '\0';
		src++;

		/* break on first / */
		if((dst = strchr(src, '/'))) {
			*dst = '\0';
			dst++;
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Invalid destination part in regexp: %s\n", src);
			goto done;
		}
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "src: %s, dst: %s\n", src, dst);
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Not a valid regexp: %s\n", src);
		goto done;
	}

	/* if a session is provided, check if the source part of the regex has any channel variables, then expand them */
	if (session) {
		channel = switch_core_session_get_channel(session);
		switch_assert(channel);

		if (switch_string_var_check_const(src) || switch_string_has_escaped_data(src)) {
			tmp_regex = switch_channel_expand_variables(channel, src);
			src_regex = switch_core_strdup(pool, tmp_regex);
			if ( tmp_regex != src ) {
				switch_safe_free(tmp_regex);
			}
			src = src_regex;
		}

		if (switch_string_var_check_const(dst) || switch_string_has_escaped_data(dst)) {
			tmp_regex = switch_channel_expand_variables(channel, dst);
			dst_regex = switch_core_strdup(pool, tmp_regex);
			if ( tmp_regex != dst ) {
				switch_safe_free(tmp_regex);
			}
			dst = dst_regex;
		}

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "expanded src: %s, dst: %s\n", src, dst);
	}

	if ((proceed = switch_regex_perform(number, src, &re, ovector, sizeof(ovector) / sizeof(ovector[0])))) {
		len = (uint32_t) (strlen(src) + strlen(dst) + 10) * proceed; /* guestimate size */
		if (!(substituted = switch_core_alloc(pool, len))) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Memory Error!\n");
			goto done;
		}
		memset(substituted, 0, len);
		switch_perform_substitution(re, proceed, dst, number, substituted, len, ovector);
	} else {
		goto done;
	}

	switch_regex_safe_free(re);

	return substituted;

done:
	switch_regex_safe_free(re);
	switch_safe_free(tmp_regex);
	return number;
}

static char *get_bridge_data(switch_memory_pool_t *pool, char *dialed_number, char *caller_id, lcr_route cur_route, profile_t *profile, switch_core_session_t *session, char *data)
{
	size_t lstrip;
	size_t  tstrip;
	char *destination_number = NULL;
	char *codec = NULL;
	char *cid = NULL;
	char *header = NULL;
	char *user_rate = NULL;
	char *export_fields = NULL;
	char *expanded = NULL;

	destination_number = switch_core_strdup(pool, dialed_number);

	tstrip = ((cur_route->digit_len - cur_route->tstrip) + 1);
	lstrip = cur_route->lstrip;

	if (cur_route->tstrip > 0) {
		if (strlen(destination_number) > tstrip) {
			destination_number[tstrip] = '\0';
		}
		else {
			destination_number[0] = '\0';
				   }
	}
	if (cur_route->lstrip > 0) {
		if (strlen(destination_number) > lstrip) {
			destination_number += lstrip;
		}
		else {
			destination_number[0] = '\0';
		}
	}
	codec = "";
	if (!zstr(cur_route->codec)) {
		codec = switch_core_sprintf(pool, ",absolute_codec_string=%s", cur_route->codec);
	}

	cid = "";
	if (!zstr(cur_route->cid)) {
		cid = switch_core_sprintf(pool, ",origination_caller_id_number=%s",
								  do_cid(pool, cur_route->cid, caller_id, session));
	}

	header = "";
	if (profile->info_in_headers) {
		header = switch_core_sprintf(pool, ",sip_h_X-LCR-INFO=lcr_rate=%s;lcr_carrier=%s",
									  cur_route->rate_str,
									  cur_route->carrier_name);
	}

	if (zstr(cur_route->user_rate_str)) {
		user_rate = "";
	} else {
		user_rate = switch_core_sprintf(pool, ",lcr_user_rate=%s", cur_route->user_rate_str);
	}

	export_fields = "";
	if (profile->export_fields_cnt > 0) {
		int i = 0;
		char *val = NULL;
		for (i = 0; i < profile->export_fields_cnt; i++) {
			val = switch_event_get_header(cur_route->fields, profile->export_fields[i]);
			if (val) {
				export_fields = switch_core_sprintf(pool, "%s,%s=%s",
													export_fields,
													profile->export_fields[i],
													val);
			}
		}
	}

	if (profile->enable_sip_redir) {
		data = switch_core_sprintf(pool, "%s%s%s%s%s"
									, cur_route->gw_prefix, cur_route->prefix
									, destination_number, cur_route->suffix, cur_route->gw_suffix);
	} else {
		data = switch_core_sprintf(pool, "[lcr_carrier=%s,lcr_rate=%s%s%s%s%s%s]%s%s%s%s%s"
									, cur_route->carrier_name, cur_route->rate_str
									, user_rate, codec, cid, header, export_fields
									, cur_route->gw_prefix, cur_route->prefix
									, destination_number, cur_route->suffix, cur_route->gw_suffix);
	}

	if (session && (switch_string_var_check_const(data) || switch_string_has_escaped_data(data))) {
		expanded = switch_channel_expand_variables(switch_core_session_get_channel(session), data);
		if (expanded == data ) {
			expanded = NULL;
		} else {
			data = switch_core_strdup( pool, expanded );
		}
	}

	switch_safe_free(expanded);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Returning Dialstring %s\n", data);

	return data;
}

static profile_t *locate_profile(const char *profile_name)
{
	profile_t *profile = NULL;

	if (zstr(profile_name)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "profile_name is empty\n");
		if (globals.default_profile) {
			profile = globals.default_profile;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "using default_profile\n");
		} else if ((profile = switch_core_hash_find(globals.profile_hash, "default"))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "no default set, using profile named \"default\"\n");
		}
	} else if (!(profile = switch_core_hash_find(globals.profile_hash, profile_name))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error invalid profile %s\n", profile_name);
	}

	return profile;
}


static void init_max_lens(max_len maxes)
{
	maxes->digit_str = strlen(headers[LCR_HEADERS_DIGITS]);
	maxes->carrier_name = strlen(headers[LCR_HEADERS_CARRIER]);
	maxes->dialstring = strlen(headers[LCR_HEADERS_DIALSTRING]);
	maxes->rate = 8;
	maxes->codec = strlen(headers[LCR_HEADERS_CODEC]);
	maxes->cid = strlen(headers[LCR_HEADERS_CID]);
	maxes->limit = strlen(headers[LCR_HEADERS_LIMIT]);
}

static switch_status_t process_max_lengths(max_obj_t *maxes, lcr_route routes, char *destination_number)
{
	lcr_route current = NULL;

	if (routes == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "no routes\n");
		return SWITCH_STATUS_FALSE;
	}
	if (maxes == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "no maxes\n");
		return SWITCH_STATUS_FALSE;
	}

	init_max_lens(maxes);

	for (current = routes; current; current = current->next) {
		size_t this_len;

		if (current->carrier_name) {
			this_len = strlen(current->carrier_name);
			if (this_len > maxes->carrier_name) {
				maxes->carrier_name = this_len;
			}
		}
		if (current->dialstring) {
			this_len = strlen(current->dialstring);
			if (this_len > maxes->dialstring) {
				maxes->dialstring = this_len;
			}
		}
		if (current->digit_str) {
			if (current->digit_len > maxes->digit_str) {
				maxes->digit_str = current->digit_len;
			}
		}
		if (current->rate_str) {
			this_len = strlen(current->rate_str);
			if (this_len > maxes->rate) {
				maxes->rate = this_len;
			}
		}
		if (current->codec) {
			this_len= strlen(current->codec);
			if (this_len > maxes->codec) {
				maxes->codec = this_len;
			}
		}
		if (current->cid) {
			this_len = strlen(current->cid);
			if (this_len > maxes->cid) {
				maxes->cid = this_len;
			}
		}

		if (current->limit_realm && current->limit_id) {
			this_len = strlen(current->limit_realm) + strlen(current->limit_id) + 5;
			if (this_len > maxes->limit) {
				maxes->limit = this_len;
			}
		}
	}
	return SWITCH_STATUS_SUCCESS;
}

static switch_cache_db_handle_t *lcr_get_db_handle(void)
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

static switch_bool_t db_check(char *sql)
{
	switch_bool_t ret = SWITCH_FALSE;
	switch_cache_db_handle_t *dbh = NULL;

	if (globals.odbc_dsn && (dbh = lcr_get_db_handle())) {
		if (switch_cache_db_execute_sql(dbh, sql, NULL) == SWITCH_STATUS_SUCCESS) {
			ret = SWITCH_TRUE;
		}
	}

	switch_cache_db_release_db_handle(&dbh);
	return ret;
}

/* try each type of random until we suceed */
static switch_bool_t set_db_random()
{
	if (db_check("SELECT rand();") == SWITCH_TRUE) {
		db_random = "rand()";
		return SWITCH_TRUE;
	}
	if (db_check("SELECT random();") == SWITCH_TRUE) {
		db_random = "random()";
		return SWITCH_TRUE;
	}

	return SWITCH_FALSE;
}

/* make a new string with digits only */
static char *string_digitsonly(switch_memory_pool_t *pool, const char *str)
{
	char *p, *np, *newstr;
	size_t len;

	p = (char *)str;

	len = strlen(str);
	newstr = switch_core_alloc(pool, len+1);
	np = newstr;

	while(*p) {
		if (switch_isdigit(*p)) {
			*np = *p;
			np++;
		}
		p++;
	}
	*np = '\0';

	return newstr;
}

/* escape sql */
#ifdef _WAITING_FOR_ESCAPE
static char *escape_sql(const char *sql)
{
	return switch_string_replace(sql, "'", "''");
}
#endif

/* expand the digits */
static char *expand_digits(switch_memory_pool_t *pool, char *digits, switch_bool_t quote)
{
	switch_stream_handle_t dig_stream = { 0 };
	char *ret;
	char *digits_copy;
	int n;
	int digit_len;
	SWITCH_STANDARD_STREAM(dig_stream);

	digit_len = (int)strlen(digits);
	digits_copy = switch_core_strdup(pool, digits);

	for (n = digit_len; n > 0; n--) {
		digits_copy[n] = '\0';
		dig_stream.write_function(&dig_stream, "%s%s%s%s",
									(n==digit_len ? "" : ", "),
									(quote ? "'" : ""),
									digits_copy,
									(quote ? "'" : ""));
	}

	ret = switch_core_strdup(pool, dig_stream.data);
	switch_safe_free(dig_stream.data);
	return ret;
}

/* format the custom sql */
static char *format_custom_sql(const char *custom_sql, callback_t *cb_struct, const char *digits)
{
	char *replace = NULL;
	switch_channel_t *channel;

	/* first replace %s with digits to maintain backward compat */
	if (cb_struct->profile->custom_sql_has_percent == SWITCH_TRUE) {
		replace = switch_string_replace(custom_sql, "%q", digits);
		custom_sql = replace;
	}

	/* expand the vars */
	if (cb_struct->profile->custom_sql_has_vars == SWITCH_TRUE) {
		if (cb_struct->session) {
			channel = switch_core_session_get_channel(cb_struct->session);
			switch_assert(channel);
			custom_sql = switch_channel_expand_variables(channel, custom_sql);
			if ( custom_sql != replace ) {
				switch_safe_free(replace);
			}
		} else if (cb_struct->event) {
			/* use event system to expand vars */
			custom_sql = switch_event_expand_headers(cb_struct->event, custom_sql);
			if ( custom_sql != replace ) {
				switch_safe_free(replace);
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(cb_struct->session), SWITCH_LOG_CRIT,
								"mod_lcr called without a valid session while using a custom_sql that has channel variables.\n");
		}
	}

	return (char *) custom_sql;
}

static switch_status_t lcr_execute_sql_callback(char *sql, switch_core_db_callback_func_t callback, void *pdata)
{
	switch_status_t retval = SWITCH_STATUS_GENERR;
	switch_cache_db_handle_t *dbh = NULL;

	if (globals.odbc_dsn && (dbh = lcr_get_db_handle())) {
		if (switch_cache_db_execute_sql_callback(dbh, sql, callback, pdata, NULL) != SWITCH_STATUS_SUCCESS) {
			retval = SWITCH_STATUS_GENERR;
		} else {
			retval = SWITCH_STATUS_SUCCESS;
		}
	}
	switch_cache_db_release_db_handle(&dbh);
	return retval;
}

/* CF = compare field */
#define CF(x) !strcmp(x, columnNames[i])

static int route_add_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	lcr_route additional = NULL;
	lcr_route current = NULL;
	callback_t *cbt = (callback_t *) pArg;
	char *key = NULL;
	char *key2 = NULL;
	int i = 0;
	int r = 0;
	char *data = NULL;
	switch_bool_t lcr_skipped = SWITCH_TRUE; /* assume we'll throw it away, paranoid about leak */

	switch_memory_pool_t *pool = cbt->pool;

	additional = switch_core_alloc(pool, sizeof(lcr_obj_t));
	switch_event_create(&additional->fields, SWITCH_EVENT_REQUEST_PARAMS);

	for (i = 0; i < argc ; i++) {
		if (CF("lcr_digits")) {
			additional->digit_len = strlen(argv[i]);
			additional->digit_str = switch_core_strdup(pool, switch_str_nil(argv[i]));
		} else if (CF("lcr_prefix")) {
			additional->prefix = switch_core_strdup(pool, switch_str_nil(argv[i]));
		} else if (CF("lcr_suffix")) {
			additional->suffix = switch_core_strdup(pool, switch_str_nil(argv[i]));
		} else if (CF("lcr_carrier_name")) {
			additional->carrier_name = switch_core_strdup(pool, switch_str_nil(argv[i]));
		} else if (CF("lcr_rate_field")) {
			if (!argv[i] || zstr(argv[i])) {
				/* maybe we want to consider saying which carriers have null rate fields... maybe they can run the query and find out */
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "rate field is null, skipping\n");
				/* kill prev/next pointers */
				/* additional->prev = NULL; */
				goto end;
			}
			additional->rate = (float)atof(switch_str_nil(argv[i]));
			additional->rate_str = switch_core_sprintf(pool, "%0.5f", additional->rate);
		} else if (CF("lcr_gw_prefix")) {
			additional->gw_prefix = switch_core_strdup(pool, switch_str_nil(argv[i]));
		} else if (CF("lcr_gw_suffix")) {
			additional->gw_suffix = switch_core_strdup(pool, switch_str_nil(argv[i]));
		} else if (CF("lcr_lead_strip")) {
			additional->lstrip = atoi(switch_str_nil(argv[i]));
		} else if (CF("lcr_trail_strip")) {
			additional->tstrip = atoi(switch_str_nil(argv[i]));
		} else if (CF("lcr_codec")) {
			additional->codec = switch_core_strdup(pool, switch_str_nil(argv[i]));
		} else if (CF("lcr_cid")) {
			additional->cid = switch_core_strdup(pool, switch_str_nil(argv[i]));
		} else if (CF("lcr_user_rate")) {
			additional->user_rate = (float)atof(switch_str_nil(argv[i]));
			additional->user_rate_str = switch_core_sprintf(pool, "%0.5f", additional->user_rate);
		} else if (CF("lcr_limit_realm")) {
			additional->limit_realm = switch_core_strdup(pool, switch_str_nil(argv[i]));
		} else if (CF("lcr_limit_id")) {
			additional->limit_id = switch_core_strdup(pool, switch_str_nil(argv[i]));
		} else if (CF("lcr_limit_max")) {
			additional->limit_max = (int)(float)atof(switch_str_nil(argv[i]));
		}

		/* add all fields to the fields event */
		switch_event_add_header_string(additional->fields, SWITCH_STACK_BOTTOM, columnNames[i], argv[i]);
	}

	cbt->matches++;

	additional->dialstring = get_bridge_data(pool, cbt->lookup_number, cbt->cid, additional, cbt->profile, cbt->session, data);

	if (cbt->head == NULL) {
		if (cbt->max_rate && (cbt->max_rate < additional->rate)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Skipping [%s] because [%f] is higher than the max_rate of [%f]\n",
							  additional->carrier_name, additional->rate, cbt->max_rate);
			lcr_skipped = SWITCH_FALSE;
			r = 0; goto end;
		}
		key = switch_core_sprintf(pool, "%s:%s", additional->gw_prefix, additional->gw_suffix);
		if (cbt->profile->single_bridge) {
			key2 = switch_core_sprintf(pool, "%s", additional->carrier_name);
		}
		additional->next = cbt->head;
		cbt->head = additional;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Adding %s to head of list\n", additional->carrier_name);
		if (switch_core_hash_insert(cbt->dedup_hash, key, additional) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error inserting into dedup hash\n");
			r = -1; goto end;
		}
		if (cbt->profile->single_bridge) {
			if (switch_core_hash_insert(cbt->dedup_hash, key2, additional) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error inserting into dedup hash\n");
				r = -1; goto end;
			}
		}
		lcr_skipped = SWITCH_FALSE;
		r = 0; goto end;
	}


	for (current = cbt->head; current; current = current->next) {
		if (cbt->max_rate && (cbt->max_rate < additional->rate)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Skipping [%s] because [%f] is higher than the max_rate of [%f]\n",
							  additional->carrier_name, additional->rate, cbt->max_rate);
			break;
		}

		key = switch_core_sprintf(pool, "%s:%s", additional->gw_prefix, additional->gw_suffix);
		if (switch_core_hash_find(cbt->dedup_hash, key)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
								"Ignoring Duplicate route for termination point (%s)\n",
								key);
			break;
		}

		if (cbt->profile->single_bridge) {
			key2 = switch_core_sprintf(pool, "%s", additional->carrier_name);
			if (switch_core_hash_find(cbt->dedup_hash, key2)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
								"Ignoring duplicate carrier gateway for single bridge. (%s)\n",
								key2);
				break;
			}
		}

		if (!cbt->profile->reorder_by_rate) {
			/* use db order */
			if (current->next == NULL) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Adding %s to end of list\n", additional->carrier_name);
				current->next = additional;
				additional->prev = current;
				if (switch_core_hash_insert(cbt->dedup_hash, key, additional) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error inserting into dedup hash\n");
					r = -1; goto end;
				}
				if (cbt->profile->single_bridge) {
					if (switch_core_hash_insert(cbt->dedup_hash, key2, additional) != SWITCH_STATUS_SUCCESS) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error inserting into dedup hash\n");
						r = -1; goto end;
					}
				}
				lcr_skipped = SWITCH_FALSE;
				break;
			}
		} else {
			if (current->rate > additional->rate) {
				/* insert myself here */
				if (current->prev != NULL) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Adding %s before %s\n",
										additional->carrier_name, current->carrier_name);
					current->prev->next = additional;
				} else {
					/* put this one at the head */
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Inserting %s to list head before %s\n",
										additional->carrier_name, current->carrier_name);
					cbt->head = additional;
				}
				additional->next = current;
				current->prev = additional;
				if (switch_core_hash_insert(cbt->dedup_hash, key, additional) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error inserting into dedup hash\n");
					r = -1; goto end;
				}
				if (cbt->profile->single_bridge) {
					if (switch_core_hash_insert(cbt->dedup_hash, key2, additional) != SWITCH_STATUS_SUCCESS) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error inserting into dedup hash\n");
						r = -1; goto end;
					}
				}
				lcr_skipped = SWITCH_FALSE;
				break;
			} else if (current->next == NULL) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "adding %s to end of list after %s\n",
									additional->carrier_name, current->carrier_name);
				current->next = additional;
				additional->prev = current;
				if (switch_core_hash_insert(cbt->dedup_hash, key, additional) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error inserting into dedup hash\n");
					r = -1; goto end;
				}
				if (cbt->profile->single_bridge) {
					if (switch_core_hash_insert(cbt->dedup_hash, key2, additional) != SWITCH_STATUS_SUCCESS) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error inserting into dedup hash\n");
						r = -1; goto end;
					}
				}
				lcr_skipped = SWITCH_FALSE;
				break;
			}
		}
	}

 end:

	/* lcr was not added to any lists, so destroy lcr object here */
	if (lcr_skipped == SWITCH_TRUE) {
		/* complain loudly if we're asked to destroy a route that is
		   added to the route list */
		if (additional && additional->prev != NULL) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
							  "additional->prev != NULL\n");
		}
		if (current && current->next == additional) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
							  "current->next == additional\n");
		}
		lcr_destroy(additional);
	}

	switch_safe_free(data);

	return r;

}

static int intrastatelata_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	int count = 0;
	callback_t *cbt = (callback_t *) pArg;

	count = atoi(argv[1]);
	if (count == 1) {
		if (!strcmp(argv[0], "state")) {
			cbt->intrastate = SWITCH_TRUE;
		} else if (!strcmp(argv[0], "lata")) {
			cbt->intralata = SWITCH_TRUE;
		}
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Type: %s, Count: %d\n", argv[0], count);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t is_intrastatelata(callback_t *cb_struct)
{
	char *sql = NULL;

	/* extract npa nxx - make some assumptions about format:
	   e164 format without the +
	   NANP only (so 11 digits starting with 1)
	 */
	if (!cb_struct->lookup_number || strlen(cb_struct->lookup_number) != 11 || *cb_struct->lookup_number != '1' ||
		!switch_is_number(cb_struct->lookup_number)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(cb_struct->session), SWITCH_LOG_DEBUG,
						  "%s doesn't appear to be a NANP number\n", cb_struct->lookup_number);
		/* dest doesn't appear to be NANP number */
		return SWITCH_STATUS_GENERR;
	}
	if (!cb_struct->cid || strlen(cb_struct->cid) != 11 || *cb_struct->cid != '1' || !switch_is_number(cb_struct->cid)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(cb_struct->session), SWITCH_LOG_DEBUG,
						  "%s doesn't appear to be a NANP number\n", cb_struct->cid);
		/* cid not NANP */
		return SWITCH_STATUS_GENERR;
	}

	/* assume that if the area code (plus leading 1) are the same we're intrastate */
	/* probably a bad assumption */
	/*
	if (!strncmp(cb_struct->lookup_number, cb_struct->cid, 4)) {
		cb_struct->intrastate = SWITCH_TRUE;
		return SWITCH_STATUS_SUCCESS;
	}
	*/

	sql = switch_core_sprintf(cb_struct->pool,
								"SELECT 'state', count(DISTINCT state) FROM npa_nxx_company_ocn WHERE (npa=%3.3s AND nxx=%3.3s) OR (npa=%3.3s AND nxx=%3.3s)"
								" UNION "
								"SELECT 'lata', count(DISTINCT lata) FROM npa_nxx_company_ocn WHERE (npa=%3.3s AND nxx=%3.3s) OR (npa=%3.3s AND nxx=%3.3s)",
								cb_struct->lookup_number+1, cb_struct->lookup_number+4,
								cb_struct->cid+1, cb_struct->cid+4,
								cb_struct->lookup_number+1, cb_struct->lookup_number+4,
								cb_struct->cid+1, cb_struct->cid+4);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(cb_struct->session), SWITCH_LOG_DEBUG, "SQL: %s\n", sql);

	return(lcr_execute_sql_callback(sql, intrastatelata_callback, cb_struct));

}

static switch_status_t lcr_do_lookup(callback_t *cb_struct)
{
	switch_stream_handle_t sql_stream = { 0 };
	char *digits = cb_struct->lookup_number;
	char *digits_copy = NULL;
	char *digits_expanded = NULL;
	char *lrn_digits_expanded = NULL;
	profile_t *profile = cb_struct->profile;
	switch_status_t lookup_status;
	switch_channel_t *channel;
	char *id_str;
	char *safe_sql = NULL;
	char *rate_field = NULL;
	char *user_rate_field = NULL;

	switch_assert(cb_struct->lookup_number != NULL);

	digits_copy = string_digitsonly(cb_struct->pool, digits);
	if (zstr(digits_copy)) {
		return SWITCH_STATUS_GENERR;
	}

	/* allocate the dedup hash */
	if (switch_core_hash_init(&cb_struct->dedup_hash) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(cb_struct->session), SWITCH_LOG_ERROR, "Error initializing the dedup hash\n");
		return SWITCH_STATUS_GENERR;
	}

	digits_expanded = expand_digits(cb_struct->pool, digits_copy, cb_struct->profile->quote_in_list);
	if (cb_struct->lrn_number) {
		lrn_digits_expanded = expand_digits(cb_struct->pool, cb_struct->lrn_number, cb_struct->profile->quote_in_list);
	} else {
		lrn_digits_expanded = switch_core_strdup(cb_struct->pool, digits_expanded);
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(cb_struct->session), SWITCH_LOG_DEBUG, "Has NPA NXX: [%u == %u]\n", profile->profile_has_npanxx, SWITCH_TRUE);
	if (profile->profile_has_npanxx == SWITCH_TRUE) {
		is_intrastatelata(cb_struct);
	}

	/* set our rate field based on env and profile */
	if (cb_struct->intralata == SWITCH_TRUE && profile->profile_has_intralata == SWITCH_TRUE) {
		rate_field = switch_core_strdup(cb_struct->pool, "intralata_rate");
		user_rate_field = switch_core_strdup(cb_struct->pool, "user_intralata_rate");
	} else if (cb_struct->intrastate == SWITCH_TRUE && profile->profile_has_intrastate == SWITCH_TRUE) {
		rate_field = switch_core_strdup(cb_struct->pool, "intrastate_rate");
		user_rate_field = switch_core_strdup(cb_struct->pool, "user_intrastate_rate");
	} else {
		rate_field = switch_core_strdup(cb_struct->pool, "rate");
		user_rate_field = switch_core_strdup(cb_struct->pool, "user_rate");
	}
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(cb_struct->session), SWITCH_LOG_DEBUG, "intra routing [state:%d lata:%d] so rate field is [%s]\n",
					  cb_struct->intrastate, cb_struct->intralata, rate_field);

	/* set some channel vars if we have a session */
	if (cb_struct->session) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(cb_struct->session), SWITCH_LOG_DEBUG, "we have a session\n");
		if ((channel = switch_core_session_get_channel(cb_struct->session))) {
			const char *max_rate = switch_channel_get_variable(channel, "max_rate");
			if (!zstr(max_rate)) {
				cb_struct->max_rate = (float)atof(max_rate);
			}
			switch_channel_set_variable_var_check(channel, "lcr_rate_field", rate_field, SWITCH_FALSE);
			switch_channel_set_variable_var_check(channel, "lcr_user_rate_field", user_rate_field, SWITCH_FALSE);
			switch_channel_set_variable_var_check(channel, "lcr_query_digits", digits_copy, SWITCH_FALSE);
			id_str = switch_core_sprintf(cb_struct->pool, "%d", cb_struct->profile->id);
			switch_channel_set_variable_var_check(channel, "lcr_query_profile", id_str, SWITCH_FALSE);
			switch_channel_set_variable_var_check(channel, "lcr_query_expanded_digits", digits_expanded, SWITCH_FALSE);
			switch_channel_set_variable_var_check(channel, "lcr_query_expanded_lrn_digits", lrn_digits_expanded, SWITCH_FALSE);
			if ( cb_struct->lrn_number ) {
				switch_channel_set_variable_var_check(channel, "lcr_lrn", cb_struct->lrn_number, SWITCH_FALSE);
			}
		}
	}
	if (cb_struct->event) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(cb_struct->session), SWITCH_LOG_DEBUG, "we have an event\n");
		switch_event_add_header_string(cb_struct->event, SWITCH_STACK_BOTTOM, "lcr_rate_field", rate_field);
		switch_event_add_header_string(cb_struct->event, SWITCH_STACK_BOTTOM, "lcr_user_rate_field", user_rate_field);
		switch_event_add_header_string(cb_struct->event, SWITCH_STACK_BOTTOM, "lcr_query_digits", digits_copy);
		id_str = switch_core_sprintf(cb_struct->pool, "%d", cb_struct->profile->id);
		switch_event_add_header_string(cb_struct->event, SWITCH_STACK_BOTTOM, "lcr_query_profile", id_str);
		switch_event_add_header_string(cb_struct->event, SWITCH_STACK_BOTTOM, "lcr_query_expanded_digits", digits_expanded);
		switch_event_add_header_string(cb_struct->event, SWITCH_STACK_BOTTOM, "lcr_query_expanded_lrn_digits", lrn_digits_expanded);
		if ( cb_struct->lrn_number ) {
			switch_event_add_header_string(cb_struct->event, SWITCH_STACK_BOTTOM, "lcr_lrn", cb_struct->lrn_number);
		}
	}

	/* set up the query to be executed */
	/* format the custom_sql */
	safe_sql = format_custom_sql(profile->custom_sql, cb_struct, digits_copy);
	if (!safe_sql) {
		switch_core_hash_destroy(&cb_struct->dedup_hash);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(cb_struct->session), SWITCH_LOG_ERROR, "Unable to format SQL\n");
		return SWITCH_STATUS_GENERR;
	}
	SWITCH_STANDARD_STREAM(sql_stream);
	sql_stream.write_function(&sql_stream, safe_sql);
	if (safe_sql != profile->custom_sql) {
		/* channel_expand_variables returned the same string to us, no need to free */
		switch_safe_free(safe_sql);
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(cb_struct->session), SWITCH_LOG_DEBUG, "SQL: %s\n", (char *)sql_stream.data);

	lookup_status = lcr_execute_sql_callback((char *)sql_stream.data, route_add_callback, cb_struct);

	switch_safe_free(sql_stream.data);
	switch_core_hash_destroy(&cb_struct->dedup_hash);

	return lookup_status;
}

static switch_bool_t test_profile(char *lcr_profile)
{
	callback_t routes = { 0 };
	switch_memory_pool_t *pool = NULL;
	switch_event_t *event = NULL;
	switch_bool_t r;

	switch_core_new_memory_pool(&pool);
	switch_event_create(&event, SWITCH_EVENT_MESSAGE);
	routes.event = event;
	routes.pool = pool;

	if (!(routes.profile = locate_profile(lcr_profile))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unknown profile: %s\n", lcr_profile);
		return SWITCH_FALSE;
	}

	routes.lookup_number = "15555551212";
	routes.cid = "18005551212";
	lcr_destroy(routes.head);

	r = (lcr_do_lookup(&routes) == SWITCH_STATUS_SUCCESS) ? SWITCH_TRUE : SWITCH_FALSE;
	switch_event_destroy(&event);

	return r;
}

static switch_status_t lcr_load_config()
{
	char *cf = "lcr.conf";
	switch_stream_handle_t sql_stream = { 0 };
	switch_xml_t cfg, xml, settings, param, x_profile, x_profiles;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	profile_t *profile = NULL;
	switch_cache_db_handle_t *dbh = NULL;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
		return SWITCH_STATUS_TERM;
	}

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = NULL;
			char *val = NULL;
			var = (char *) switch_xml_attr_soft(param, "name");
			val = (char *) switch_xml_attr_soft(param, "value");
			if (!strcasecmp(var, "odbc-dsn") && !zstr(val)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "odbc_dsn is %s\n", val);
				switch_safe_free(globals.odbc_dsn);
				globals.odbc_dsn = strdup(val);
			}
		}
	}

	/* initialize sql here, 'cause we need to verify custom_sql for each profile below */
	if (globals.odbc_dsn) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG
						  , "dsn is \"%s\"\n"
						  , globals.odbc_dsn
						  );
		if (!(dbh = lcr_get_db_handle())) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Cannot Open ODBC Database!\n");
			switch_goto_status(SWITCH_STATUS_FALSE, done);
		}
	}

	if (set_db_random() == SWITCH_TRUE) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Database RANDOM function set to %s\n", db_random);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to determine database RANDOM function\n");
	};

	switch_core_hash_init(&globals.profile_hash);
	if ((x_profiles = switch_xml_child(cfg, "profiles"))) {
		for (x_profile = switch_xml_child(x_profiles, "profile"); x_profile; x_profile = x_profile->next) {
			char *name = (char *) switch_xml_attr_soft(x_profile, "name");
			char *comma = ", ";
			switch_stream_handle_t order_by = { 0 };
			switch_stream_handle_t *thisorder = NULL;
			char *reorder_by_rate = NULL;
			char *quote_in_list = NULL;
			char *single_bridge = NULL;
			char *info_in_headers = NULL;
			char *enable_sip_redir = NULL;
			char *id_s = NULL;
			char *custom_sql = NULL;
			char *export_fields = NULL;
			char *limit_type = NULL;
			int argc, x = 0;
			char *argv[32] = { 0 };

			SWITCH_STANDARD_STREAM(order_by);

			for (param = switch_xml_child(x_profile, "param"); param; param = param->next) {
				char *var, *val;

				var = (char *) switch_xml_attr_soft(param, "name");
				val = (char *) switch_xml_attr_soft(param, "value");

				if (!strcasecmp(var, "order_by")  && !zstr(val)) {
					thisorder = &order_by;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "param val is %s\n", val);
					if ((argc = switch_separate_string(val, ',', argv, (sizeof(argv) / sizeof(argv[0]))))) {
						for (x=0; x<argc; x++) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "arg #%d/%d is %s\n", x, argc, argv[x]);
							if (!zstr(argv[x])) {
								if (!strcasecmp(argv[x], "quality")) {
									thisorder->write_function(thisorder, "%s quality DESC", comma);
								} else if (!strcasecmp(argv[x], "reliability")) {
									thisorder->write_function(thisorder, "%s reliability DESC", comma);
								} else if (!strcasecmp(argv[x], "rate")) {
									thisorder->write_function(thisorder, "%s ${lcr_rate_field}", comma);
								} else {
									thisorder->write_function(thisorder, "%s %s", comma, argv[x]);
								}
							} else {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "arg #%d is empty\n", x);
							}
						}
					} else {
						if (!strcasecmp(val, "quality")) {
							thisorder->write_function(thisorder, "%s quality DESC", comma);
						} else if (!strcasecmp(val, "reliability")) {
							thisorder->write_function(thisorder, "%s reliability DESC", comma);
						} else {
							thisorder->write_function(thisorder, "%s %s", comma, val);
						}
					}
				} else if (!strcasecmp(var, "id") && !zstr(val)) {
					id_s = val;
				} else if (!strcasecmp(var, "custom_sql") && !zstr(val)) {
					custom_sql = val;
				} else if (!strcasecmp(var, "reorder_by_rate") && !zstr(val)) {
					reorder_by_rate = val;
				} else if (!strcasecmp(var, "info_in_headers") && !zstr(val)) {
					info_in_headers = val;
				} else if (!strcasecmp(var, "quote_in_list") && !zstr(val)) {
					quote_in_list = val;
				} else if (!strcasecmp(var, "single_bridge") && !zstr(val)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Single bridge mode set to %s.\n", val);
					single_bridge = val;
				} else if (!strcasecmp(var, "export_fields") && !zstr(val)) {
					export_fields = val;
				} else if (!strcasecmp(var, "limit_type") && !zstr(val)) {
					limit_type = val;
				} else if (!strcasecmp(var, "enable_sip_redir") && !zstr(val)) {
					enable_sip_redir = val;
				}
			}

			if (zstr(name)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No name specified.\n");
			} else {
				profile = switch_core_alloc(globals.pool, sizeof(*profile));
				memset(profile, 0, sizeof(profile_t));
				profile->name = switch_core_strdup(globals.pool, name);

				if (!zstr((char *)order_by.data)) {
					profile->order_by = switch_core_strdup(globals.pool, (char *)order_by.data);
				} else {
					/* default to rate */
					profile->order_by = ", ${lcr_rate_field}";
				}

				if (!zstr(id_s)) {
					profile->id = (uint16_t)atoi(id_s);
				}

				/* SWITCH_STANDARD_STREAM doesn't use pools.  but we only have to free sql_stream.data */
				SWITCH_STANDARD_STREAM(sql_stream);
				if (zstr(custom_sql)) {
					/* use default sql */

					/* Checking for codec field, adding if needed */
					if (db_check("SELECT codec FROM carrier_gateway LIMIT 1") == SWITCH_TRUE) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "codec field defined.\n");
					} else {
						if (db_check("ALTER TABLE carrier_gateway add codec varchar(255);") == SWITCH_TRUE) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "adding codec field your lcr carrier_gateway database schema.\n");
						} else {
							switch_goto_status(SWITCH_STATUS_FALSE, done);
						}
					}

					/* Checking for cid field, adding if needed */
					if (db_check("SELECT cid FROM lcr LIMIT 1") == SWITCH_TRUE) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "cid field defined.\n");
					} else {
						if (db_check("ALTER TABLE lcr add cid varchar(32);") == SWITCH_TRUE) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "adding cid field to your lcr database schema.\n");
						} else {
							switch_goto_status(SWITCH_STATUS_FALSE, done);
						}
					}

					if (db_check("SELECT lrn FROM lcr LIMIT 1") != SWITCH_TRUE) {
						if (db_check("ALTER TABLE lcr ADD lrn BOOLEAN NOT NULL DEFAULT false")) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "adding lrn field to your lcr database schema.\n");
						} else {
							switch_goto_status(SWITCH_STATUS_FALSE, done);
						}
					}

					sql_stream.write_function(&sql_stream,
											  "SELECT l.digits AS lcr_digits, c.carrier_name AS lcr_carrier_name, l.${lcr_rate_field} AS lcr_rate_field, \
													  cg.prefix AS lcr_gw_prefix, cg.suffix AS lcr_gw_suffix, l.lead_strip AS lcr_lead_strip, \
													  l.trail_strip AS lcr_trail_strip, l.prefix AS lcr_prefix, l.suffix AS lcr_suffix, \
													  cg.codec AS lcr_codec, l.cid AS lcr_cid ");
					sql_stream.write_function(&sql_stream, "FROM lcr l JOIN carriers c ON l.carrier_id=c.id \
															JOIN carrier_gateway cg ON c.id=cg.carrier_id \
															WHERE c.enabled = '1' AND cg.enabled = '1' AND l.enabled = '1' AND (\
															 (digits IN (${lcr_query_expanded_digits})     AND lrn = false) OR	\
															 (digits IN (${lcr_query_expanded_lrn_digits}) AND lrn = true)");

					sql_stream.write_function(&sql_stream, ") AND CURRENT_TIMESTAMP BETWEEN date_start AND date_end ");
					if (profile->id > 0) {
						sql_stream.write_function(&sql_stream, "AND lcr_profile=%d ", profile->id);
					}

					sql_stream.write_function(&sql_stream, "ORDER BY digits DESC%s",
															profile->order_by);
					if (db_random) {
						sql_stream.write_function(&sql_stream, ", %s", db_random);
					}
					sql_stream.write_function(&sql_stream, ";");

					custom_sql = sql_stream.data;
				}


				profile->profile_has_intralata = db_check("SELECT intralata_rate FROM lcr LIMIT 1");
				if (profile->profile_has_intralata != SWITCH_TRUE) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
									  "no \"intralata_rate\" field found in the \"lcr\" table, routing by intralata rates will be disabled until the field is added and mod_lcr is reloaded\n"
									  );
				}
				profile->profile_has_intrastate = db_check("SELECT intrastate_rate FROM lcr LIMIT 1");
				if (profile->profile_has_intrastate != SWITCH_TRUE) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
									  "no \"intrastate_rate\" field found in the \"lcr\" table, routing by intrastate rates will be disabled until the field is added and mod_lcr is reloaded\n"
									  );
				}

				profile->profile_has_npanxx = db_check("SELECT npa, nxx, state FROM npa_nxx_company_ocn LIMIT 1");
				if (profile->profile_has_npanxx != SWITCH_TRUE) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
									  "no \"npa_nxx_company_ocn\" table found in the \"lcr\" database, automatic intrastate detection will be disabled until the table is added and mod_lcr is reloaded\n"
									  );
				}

				if (switch_string_var_check_const(custom_sql) || switch_string_has_escaped_data(custom_sql)) {
					profile->custom_sql_has_vars = SWITCH_TRUE;
				}
				if (strstr(custom_sql, "%")) {
					profile->custom_sql_has_percent = SWITCH_TRUE;
				}
				profile->custom_sql = switch_core_strdup(globals.pool, (char *)custom_sql);

				if (!zstr(reorder_by_rate)) {
					profile->reorder_by_rate = switch_true(reorder_by_rate);
				}

				if (!zstr(info_in_headers)) {
					profile->info_in_headers = switch_true(info_in_headers);
				}

				if (!zstr(enable_sip_redir)) {
					profile->enable_sip_redir = switch_true(enable_sip_redir);
				}

				if (!zstr(quote_in_list)) {
					profile->quote_in_list = switch_true(quote_in_list);
				}

				if (!zstr(single_bridge)) {
					profile->single_bridge = switch_true(single_bridge);
				}

				if (!zstr(export_fields)) {
					int argc2 = 0;
					char *argv2[50] = { 0 };
					char **argvdup = NULL;
					char *dup = switch_core_strdup(globals.pool, export_fields);
					argc2 = switch_separate_string(dup, ',', argv2, (sizeof(argv2) / sizeof(argv2[0])));
					profile->export_fields_str = switch_core_strdup(globals.pool, export_fields);
					profile->export_fields_cnt = argc2;
					argvdup = switch_core_alloc(globals.pool, sizeof(argv2));
					memcpy(argvdup, argv2, sizeof(argv2));
					profile->export_fields = argvdup;
				}

				if (!zstr(limit_type)) {
					if (!strcasecmp(limit_type, "hash")) {
						profile->limit_type = "hash";
					} else {
						profile->limit_type = "db";
					}
				} else {
					profile->limit_type = "db";
				}

				switch_core_hash_insert(globals.profile_hash, profile->name, profile);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Loaded lcr profile %s.\n", profile->name);
				/* test the profile */
				if (profile->custom_sql_has_vars) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "custom_sql has channel vars, skipping verification and assuming valid profile: %s.\n", profile->name);
					if (!strcasecmp(profile->name, "default")) {
						globals.default_profile = profile;
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Setting user defined default profile: %s.\n", profile->name);
					}
				} else if (test_profile(profile->name) == SWITCH_TRUE) {
					if (!strcasecmp(profile->name, "default")) {
						globals.default_profile = profile;
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Setting user defined default profile: %s.\n", profile->name);
					}
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Removing INVALID Profile %s.\n", profile->name);
					switch_core_hash_delete(globals.profile_hash, profile->name);
				}

			}
			switch_safe_free(order_by.data);
			switch_safe_free(sql_stream.data);
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "No lcr profiles defined.\n");
	}

	/* define default profile  */
	if (!globals.default_profile) {
		profile = switch_core_alloc(globals.pool, sizeof(*profile));
		memset(profile, 0, sizeof(profile_t));
		profile->name = "global_default";
		profile->order_by = ", rate";
		globals.default_profile = profile;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Setting system defined default profile.");
	}

done:
	switch_cache_db_release_db_handle(&dbh);
	switch_xml_free(xml);
	return status;
}

/* fake chan_lcr */
switch_endpoint_interface_t *lcr_endpoint_interface;
static switch_call_cause_t lcr_outgoing_channel(switch_core_session_t *session,
												  switch_event_t *var_event,
												  switch_caller_profile_t *outbound_profile,
												  switch_core_session_t **new_session, switch_memory_pool_t **pool, switch_originate_flag_t flags,
												  switch_call_cause_t *cancel_cause);
switch_io_routines_t lcr_io_routines = {
	/*.outgoing_channel */ lcr_outgoing_channel
};

static switch_call_cause_t lcr_outgoing_channel(switch_core_session_t *session,
												  switch_event_t *var_event,
												  switch_caller_profile_t *outbound_profile,
												  switch_core_session_t **new_session, switch_memory_pool_t **new_pool, switch_originate_flag_t flags,
												  switch_call_cause_t *cancel_cause)
{
	switch_call_cause_t cause = SWITCH_CAUSE_NONE;
	char *dest = NULL;
	switch_originate_flag_t myflags = SOF_NONE;
	const char *cid_name_override = NULL;
	const char *cid_num_override = NULL;
	switch_channel_t *new_channel = NULL;
	unsigned int timelimit = 60;
	const char *skip, *var;

	switch_memory_pool_t *pool = NULL;
	callback_t routes = { 0 };
	lcr_route cur_route = { 0 };
	char *lcr_profile = NULL;
	switch_event_t *event = NULL;
	const char *intrastate = NULL;
	const char *intralata = NULL;
	switch_core_session_t *mysession = NULL, *locked_session = NULL;
	switch_channel_t *channel = NULL;
	int argc;
	char *argv[32] = { 0 };
	char *mydata = NULL;

	switch_core_new_memory_pool(&pool);
	routes.pool = pool;

	if (!outbound_profile->destination_number) {
		goto done;
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Entering lcr endpoint for %s\n", outbound_profile->destination_number);

	mydata = switch_core_strdup(pool, outbound_profile->destination_number);

	if ((argc = switch_separate_string(mydata, '/', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		if (argc > 1) {
			lcr_profile = switch_core_strdup(pool, argv[0]);
			dest        = switch_core_strdup(pool, argv[1]);
		}
	}

	if (!dest) {
		dest = outbound_profile->destination_number;
	}

	if (var_event && (skip = switch_event_get_header(var_event, "lcr_recurse_variables")) && switch_false(skip)) {
		if ((var = switch_event_get_header(var_event, SWITCH_CALL_TIMEOUT_VARIABLE)) || (var = switch_event_get_header(var_event, "leg_timeout"))) {
			timelimit = atoi(var);
		}
		var_event = NULL;
	}

	if (session) {
		mysession = session;
		channel = switch_core_session_get_channel(session);
		if ((var = switch_channel_get_variable(channel, SWITCH_CALL_TIMEOUT_VARIABLE)) || (var = switch_event_get_header(var_event, "leg_timeout"))) {
			timelimit = atoi(var);
		}
		routes.session = session;
		intrastate = switch_channel_get_variable(channel, "intrastate");
		intralata = switch_channel_get_variable(channel, "intralata");
		cid_name_override = switch_channel_get_variable(channel, "origination_caller_id_name");
		cid_num_override = switch_channel_get_variable(channel, "origination_caller_id_number");
		if (zstr(cid_name_override)) {
			cid_name_override = switch_channel_get_variable(channel, "effective_caller_id_name");
		}
		if (zstr(cid_num_override)) {
			cid_num_override = switch_channel_get_variable(channel, "effective_caller_id_number");
		}
		if ((var = switch_channel_get_variable(channel, SWITCH_CALL_TIMEOUT_VARIABLE)) || (var = switch_event_get_header(var_event, "leg_timeout"))) {
			timelimit = atoi(var);
		}
	} else if (var_event) {
		char *session_uuid = switch_event_get_header(var_event, "ent_originate_aleg_uuid");
		if (session_uuid) {
			mysession = locked_session = switch_core_session_locate(session_uuid);
		}
		cid_name_override = switch_event_get_header(var_event, "origination_caller_id_name");
		cid_num_override = switch_event_get_header(var_event, "origination_caller_id_number");
		if (zstr(cid_name_override)) {
			cid_name_override = switch_event_get_header(var_event, "effective_caller_id_name");
		}
		if (zstr(cid_num_override)) {
			cid_num_override = switch_event_get_header(var_event, "caller_id_number");
		}
		if ((var = switch_event_get_header(var_event, SWITCH_CALL_TIMEOUT_VARIABLE)) || (var = switch_event_get_header(var_event, "leg_timeout"))) {
			timelimit = atoi(var);
		}

		intrastate = switch_event_get_header(var_event, "intrastate");
		intralata = switch_event_get_header(var_event, "intralata");
		//switch_event_dup(&event, var_event);
		routes.event = var_event;
	} else {
		switch_event_create(&event, SWITCH_EVENT_MESSAGE);
		routes.event = event;
	}
	routes.lookup_number = dest;
	routes.cid = (char *) cid_num_override;

	if ((flags & SOF_FORKED_DIAL)) {
		myflags |= SOF_NOBLOCK;
	}

	if (!(routes.profile = locate_profile(lcr_profile))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unknown profile: %s\n", lcr_profile);
		goto done;
	}

	if (!zstr(intralata) && !strcasecmp((char *)intralata, "true")) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Select routes based on intralata rates\n");
		routes.intralata = SWITCH_FALSE;
	} else if (!zstr(intrastate) && !strcasecmp((char *)intrastate, "true")) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Select routes based on intrastate rates\n");
		routes.intrastate = SWITCH_TRUE;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Select routes based on interstate rates\n");
		routes.intrastate = SWITCH_FALSE;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "LCR Lookup on %s\n", dest);

	if (lcr_do_lookup(&routes) == SWITCH_STATUS_SUCCESS) {
		if (channel) {
			if (zstr(switch_channel_get_variable(channel, "import"))) {
				switch_channel_set_variable(channel, "import", "lcr_carrier,lcr_rate,lcr_user_rate");
			} else {
				const char *tmp = switch_channel_get_variable(channel, "import");
				if (!strstr(tmp, "lcr_carrier,lcr_rate,lcr_user_rate")) {
					switch_channel_set_variable_printf(channel, "import", "%s,lcr_carrier,lcr_rate,lcr_user_rate", tmp);
				}
			}
		}

		for (cur_route = routes.head; cur_route; cur_route = cur_route->next) {
			switch_bool_t pop_limit = SWITCH_FALSE;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Trying route: %s\n", cur_route->dialstring);
			if (mysession && cur_route->limit_realm && cur_route->limit_id) {
				if (switch_limit_incr(routes.profile->limit_type, mysession, cur_route->limit_realm, cur_route->limit_id, cur_route->limit_max, 0) == SWITCH_STATUS_SUCCESS) {
					pop_limit = SWITCH_TRUE;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Limit exceeded for route %s, session: %s\n", cur_route->dialstring, mysession ? "true" : "false");
					continue;
				}
			}
			if (switch_ivr_originate(session, new_session, &cause, cur_route->dialstring, timelimit, NULL,
									 cid_name_override, cid_num_override, NULL, var_event, myflags, cancel_cause) == SWITCH_STATUS_SUCCESS) {
				const char *context;
				switch_caller_profile_t *cp;

				new_channel = switch_core_session_get_channel(*new_session);

				if ((context = switch_channel_get_variable(new_channel, "lcr_context"))) {
					if ((cp = switch_channel_get_caller_profile(new_channel))) {
						cp->context = switch_core_strdup(cp->pool, context);
					}
				}
				switch_core_session_rwunlock(*new_session);
				break;
			}

			/* did not connect, release limit */
			if (pop_limit) {
				switch_limit_release(routes.profile->limit_type, mysession, cur_route->limit_realm, cur_route->limit_id);
			}
			if (cause == SWITCH_CAUSE_LOSE_RACE || cause == SWITCH_CAUSE_ORIGINATOR_CANCEL) {
				break;
			}
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "LCR lookup failed for %s\n", dest);
	}

  done:
	if (event) {
		switch_event_destroy(&event);
	}
	if (locked_session) {
		switch_core_session_rwunlock(locked_session);
	}
	lcr_destroy(routes.head);
	switch_core_destroy_memory_pool(&pool);

	if (cause == SWITCH_CAUSE_NONE) {
		cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
	}

	return cause;
}

SWITCH_STANDARD_DIALPLAN(lcr_dialplan_hunt)
{
	switch_caller_extension_t *extension = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	callback_t routes = { 0 };
	lcr_route cur_route = { 0 };
	switch_memory_pool_t *pool = NULL;
	switch_event_t *event = NULL;
	const char *intrastate = NULL;
	const char *intralata = NULL;
	const char *lrn = NULL;

	if (session) {
		pool = switch_core_session_get_pool(session);
		routes.session = session;
	} else {
		switch_core_new_memory_pool(&pool);
		switch_event_create(&event, SWITCH_EVENT_MESSAGE);
		routes.event = event;
	}
	routes.pool = pool;

	intrastate = switch_channel_get_variable(channel, "intrastate");
	intralata = switch_channel_get_variable(channel, "intralata");
	lrn = switch_channel_get_variable(channel, "lrn");
	routes.lrn_number = (char *) lrn;

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "intrastate channel var is [%s]\n", intrastate);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "intralata channel var is [%s]\n", intralata);

	if (!zstr(intralata) && !strcasecmp((char *)intralata, "true")) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Select routes based on intralata rates\n");
		routes.intralata = SWITCH_FALSE;
	} else if (!zstr(intrastate) && !strcasecmp((char *)intrastate, "true")) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Select routes based on intrastate rates\n");
		routes.intrastate = SWITCH_TRUE;
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Select routes based on interstate rates\n");
		routes.intrastate = SWITCH_FALSE;
	}

	if (!caller_profile) {
		caller_profile = switch_channel_get_caller_profile(channel);
	}

	if (!(routes.profile = locate_profile(caller_profile->context))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Unknown profile: %s\n", caller_profile->context);
		goto end;
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "LCR Lookup on %s using profile %s\n", caller_profile->destination_number, caller_profile->context);
	routes.lookup_number = caller_profile->destination_number;

	if (caller_profile) {
		routes.cid = (char *) switch_channel_get_variable(channel, "effective_caller_id_number");
		if (!routes.cid) {
			routes.cid = (char *) caller_profile->caller_id_number;
		}
	}

	if (caller_profile && lcr_do_lookup(&routes) == SWITCH_STATUS_SUCCESS) {
		if ((extension = switch_caller_extension_new(session, caller_profile->destination_number, caller_profile->destination_number)) == 0) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "memory error!\n");
			goto end;
		}

		switch_channel_set_variable(channel, SWITCH_CONTINUE_ON_FAILURE_VARIABLE, "true");
		switch_channel_set_variable(channel, SWITCH_HANGUP_AFTER_BRIDGE_VARIABLE, "true");
		if (zstr(switch_channel_get_variable(channel, "import"))) {
			switch_channel_set_variable(channel, "import", "lcr_carrier,lcr_rate,lcr_user_rate");
		} else {
			const char *tmp = switch_channel_get_variable(channel, "import");
			if (!strstr(tmp, "lcr_carrier,lcr_rate,lcr_user_rate")) {
				switch_channel_set_variable_printf(channel, "import", "%s,lcr_carrier,lcr_rate,lcr_user_rate", tmp);
			}
		}

		for (cur_route = routes.head; cur_route; cur_route = cur_route->next) {
			char *app = NULL;
			char *argc = NULL;
			if (cur_route->limit_realm && cur_route->limit_id) {
				app = "limit_execute";
				argc = switch_core_sprintf(pool, "%s %s %s %d bridge %s",
											routes.profile->limit_type,
											cur_route->limit_realm,
											cur_route->limit_id,
											cur_route->limit_max,
											cur_route->dialstring);
			} else {
				app = "bridge";
				argc = cur_route->dialstring;
			}
			switch_caller_extension_add_application(session, extension, app, argc);
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "LCR lookup failed for %s using profile %s\n", caller_profile ? caller_profile->destination_number : "unknown", caller_profile ? caller_profile->context : "unknown");
	}

end:
	lcr_destroy(routes.head);
	if (event) {
		switch_event_destroy(&event);
	}
	if (!session) {
		switch_core_destroy_memory_pool(&pool);
	}
	return extension;
}

void str_repeat(size_t how_many, char *what, switch_stream_handle_t *str_stream)
{
	size_t i;

	/*switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "repeating %d of '%s'\n", (int)how_many, what);*/

	for (i=0; i<how_many; i++) {
		str_stream->write_function(str_stream, "%s", what);
	}
}

SWITCH_STANDARD_APP(lcr_app_function)
{
	int argc = 0;
	char *argv[32] = { 0 };
	char *mydata = NULL;
	char *dest = NULL;
	char vbuf[1024] = "";
	uint32_t cnt = 1;
	char *lcr_profile = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_caller_profile_t *caller_profile = NULL;
	callback_t routes = { 0 };
	lcr_route cur_route = { 0 };
	switch_memory_pool_t *pool;
	switch_event_t *event;
	const char *intra = NULL;
	const char *lrn = NULL;

	if (!(mydata = switch_core_session_strdup(session, data))) {
		return;
	}

	if (session) {
		pool = switch_core_session_get_pool(session);
		routes.session = session;
	} else {
		switch_core_new_memory_pool(&pool);
		switch_event_create(&event, SWITCH_EVENT_MESSAGE);
		routes.event = event;
	}

	routes.pool = pool;


	lrn = switch_channel_get_variable(channel, "lrn");
	routes.lrn_number = (char *) lrn;

	intra = switch_channel_get_variable(channel, "intrastate");
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "intrastate channel var is [%s]\n", zstr(intra) ? "undef" : intra);
	if (zstr(intra) || strcasecmp((char *)intra, "true")) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Select routes based on interstate rates\n");
		routes.intrastate = SWITCH_FALSE;
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Select routes based on intrastate rates\n");
		routes.intrastate = SWITCH_TRUE;
	}

	if (!caller_profile) {
		if (!(caller_profile = switch_channel_get_caller_profile(channel))) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Unable to locate caller_profile\n");
		}
	}

	if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		dest = argv[0];

		if (argc > 1) {
			lcr_profile = argv[1];
		}

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "LCR Lookup on %s using profile %s\n", dest, lcr_profile);
		routes.lookup_number = dest;
		if (caller_profile) {
			routes.cid = (char *) switch_channel_get_variable(channel, "effective_caller_id_number");
			if (!routes.cid) {
				routes.cid = (char *) caller_profile->caller_id_number;
			}
		}


		if (!(routes.profile = locate_profile(lcr_profile))) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Unknown profile: %s\n", lcr_profile);
			goto end;
		}
		if (lcr_do_lookup(&routes) == SWITCH_STATUS_SUCCESS) {
			switch_stream_handle_t dig_stream = { 0 };

			SWITCH_STANDARD_STREAM(dig_stream);

			for (cur_route = routes.head; cur_route; cur_route = cur_route->next) {
				switch_snprintf(vbuf, sizeof(vbuf), "lcr_route_%d", cnt);
				switch_channel_set_variable(channel, vbuf, cur_route->dialstring);
				switch_snprintf(vbuf, sizeof(vbuf), "lcr_rate_%d", cnt);
				switch_channel_set_variable(channel, vbuf, cur_route->rate_str);
				switch_snprintf(vbuf, sizeof(vbuf), "lcr_carrier_%d", cnt);
				switch_channel_set_variable(channel, vbuf, cur_route->carrier_name);
				switch_snprintf(vbuf, sizeof(vbuf), "lcr_codec_%d", cnt);
				switch_channel_set_variable(channel, vbuf, cur_route->codec);
				if (cur_route->next) {
					if (routes.profile->enable_sip_redir) {
						dig_stream.write_function(&dig_stream, "%s,", cur_route->dialstring);
					} else {
						dig_stream.write_function(&dig_stream, "%s|", cur_route->dialstring);
					}
				} else {
					dig_stream.write_function(&dig_stream, "%s", cur_route->dialstring);
				}
				cnt++;
			}

			switch_snprintf(vbuf, sizeof(vbuf), "%d", cnt - 1);
			switch_channel_set_variable(channel, "lcr_route_count", vbuf);
			switch_channel_set_variable(channel, "lcr_auto_route", (char *)dig_stream.data);
			if (zstr(switch_channel_get_variable(channel, "import"))) {
				switch_channel_set_variable(channel, "import", "lcr_carrier,lcr_rate,lcr_user_rate");
			} else {
				const char *tmp = switch_channel_get_variable(channel, "import");
				if (!strstr(tmp, "lcr_carrier,lcr_rate,lcr_user_rate")) {
					switch_channel_set_variable_printf(channel, "import", "%s,lcr_carrier,lcr_rate,lcr_user_rate", tmp);
				}
			}
			free(dig_stream.data);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "LCR lookup failed for %s\n", dest);
		}
	}

end:
	lcr_destroy(routes.head);
	if (routes.event) {
		switch_event_destroy(&event);
	}
	if (!session) {
		switch_core_destroy_memory_pool(&pool);
	}
}

static void write_data(switch_stream_handle_t *stream, switch_bool_t as_xml, const char *key, const char *data, int indent, int maxlen) {
	if (!data) {
		data = "";
	}
	if (as_xml) {
		str_repeat(indent*2, " ", stream);
		stream->write_function(stream, "<%s>%s</%s>\n", key, data, key);
	} else {
		stream->write_function(stream, " | %s", data);
		str_repeat((maxlen - strlen(data)), " ", stream);
	}
}

SWITCH_STANDARD_API(dialplan_lcr_function)
{
	char *argv[32]                 = { 0 };
	int argc;
	char *mydata                  = NULL;
	char *lcr_profile             = NULL;
	lcr_route current             = NULL;
	max_obj_t maximum_lengths     = { 0 };
	callback_t cb_struct          = { 0 };
	switch_memory_pool_t *pool    = NULL;
	switch_event_t *event;
	switch_status_t lookup_status = SWITCH_STATUS_SUCCESS;
	switch_bool_t as_xml          = SWITCH_FALSE;
	char *event_str               = NULL;
	switch_xml_t event_xml        = NULL;
	int rowcount                  = 0;
	char *data                    = NULL;

	if (zstr(cmd)) {
		goto usage;
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG
					  , "data passed to lcr is [%s]\n", cmd
					  );

	if (session) {
		pool = switch_core_session_get_pool(session);
		cb_struct.session = session;
	} else {
		switch_core_new_memory_pool(&pool);
		switch_event_create(&event, SWITCH_EVENT_MESSAGE);
		cb_struct.event = event;
	}
	cb_struct.pool = pool;

	mydata = switch_core_strdup(pool, cmd);

	if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		switch_assert(argv[0] != NULL);
		cb_struct.lookup_number = switch_core_strdup(pool, argv[0]);
		if (argc > 1) {
			lcr_profile = argv[1];
		}
		if (argc > 2) {
			int i;
			for (i = 2; i < argc; i++) {
				if (!strcasecmp(argv[i], "intrastate")) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Select routes based on intrastate rates\n");
					cb_struct.intrastate = SWITCH_TRUE;
				} else if (!strcasecmp(argv[i], "lrn")) {
					i++;
					if (argv[i]) {
						cb_struct.lrn_number = argv[i];
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "LRN number [%s]\n", cb_struct.lrn_number);
					} else {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "you must pass a LRN number to use lrn\n");
					}
				} else if (!strcasecmp(argv[i], "as")) {
					i++;
					if (argv[i] && !strcasecmp(argv[i], "xml")) {
							as_xml = SWITCH_TRUE;
						} else {
							goto usage;
						}
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Set Caller ID to [%s]\n", argv[i]);
					/* the only other option we have right now is caller id */
					cb_struct.cid = switch_core_strdup(pool, argv[i]);
				}
			}
		}
		if (zstr(cb_struct.cid)) {
			cb_struct.cid = "18005551212";
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING
							  , "Using default CID [%s]\n", cb_struct.cid
							  );
		}


		if (!(cb_struct.profile = locate_profile(lcr_profile))) {
			stream->write_function(stream, "-ERR Unknown profile: %s\n", lcr_profile);
			goto end;
		}

		lookup_status = lcr_do_lookup(&cb_struct);

		if (cb_struct.head != NULL) {
			size_t len;

			if (as_xml) {
				stream->write_function(stream, "<result>\n");
			} else {
				process_max_lengths(&maximum_lengths, cb_struct.head, cb_struct.lookup_number);

				stream->write_function(stream, " | %s", headers[LCR_HEADERS_DIGITS]);
				if ((len = (maximum_lengths.digit_str - strlen(headers[LCR_HEADERS_DIGITS]))) > 0) {
					str_repeat(len, " ", stream);
				}

				stream->write_function(stream, " | %s", headers[LCR_HEADERS_CARRIER]);
				if ((len = (maximum_lengths.carrier_name - strlen(headers[LCR_HEADERS_CARRIER]))) > 0) {
					str_repeat(len, " ", stream);
				}

				stream->write_function(stream, " | %s", headers[LCR_HEADERS_RATE]);
				if ((len = (maximum_lengths.rate - strlen(headers[LCR_HEADERS_RATE]))) > 0) {
					str_repeat(len, " ", stream);
				}

				stream->write_function(stream, " | %s", headers[LCR_HEADERS_CODEC]);
				if ((len = (maximum_lengths.codec - strlen(headers[LCR_HEADERS_CODEC]))) > 0) {
					str_repeat(len, " ", stream);
				}

				stream->write_function(stream, " | %s", headers[LCR_HEADERS_CID]);
				if ((len = (maximum_lengths.cid - strlen(headers[LCR_HEADERS_CID]))) > 0) {
					str_repeat(len, " ", stream);
				}

				stream->write_function(stream, " | %s", headers[LCR_HEADERS_LIMIT]);
				if ((len = (maximum_lengths.limit - strlen(headers[LCR_HEADERS_LIMIT]))) > 0) {
					str_repeat(len, " ", stream);
				}

				stream->write_function(stream, " | %s", headers[LCR_HEADERS_DIALSTRING]);
				if ((len = (maximum_lengths.dialstring - strlen(headers[LCR_HEADERS_DIALSTRING]))) > 0) {
					str_repeat(len, " ", stream);
				}

				stream->write_function(stream, " |\n");
			}
			current = cb_struct.head;
			while (current) {

				//dialstring =
				get_bridge_data(pool, cb_struct.lookup_number, cb_struct.cid, current, cb_struct.profile, cb_struct.session, data);
				rowcount++;

				if (as_xml) {
					stream->write_function(stream, "  <row id=\"%d\">\n", rowcount);
				}

				write_data(stream, as_xml, "prefix", current->digit_str, 2, (int)maximum_lengths.digit_str);
				write_data(stream, as_xml, "carrier_name", current->carrier_name, 2, (int)maximum_lengths.carrier_name);
				write_data(stream, as_xml, "rate", current->rate_str, 2, (int)maximum_lengths.rate);
				if (current->codec) {
					write_data(stream, as_xml, "codec", current->codec, 2, (int)maximum_lengths.codec);
				} else {
					write_data(stream, as_xml, "codec", "", 2, (int)maximum_lengths.codec);
				}

				if (current->cid) {
					write_data(stream, as_xml, "cid", current->cid, 2, (int)maximum_lengths.cid);
				} else {
					write_data(stream, as_xml, "cid", "", 2, (int)maximum_lengths.cid);
				}

				if (current->limit_realm && current->limit_id) {
					char *str = NULL;
					str = switch_core_sprintf(pool, "%s %s %d", current->limit_realm, current->limit_id, current->limit_max);

					write_data(stream, as_xml, "limit", str, 2, (int)maximum_lengths.limit);
				} else {
					write_data(stream, as_xml, "limit", "", 2, (int)maximum_lengths.limit);
				}

				write_data(stream, as_xml, "dialstring", current->dialstring, 2, (int)maximum_lengths.dialstring);
				if (as_xml) {
					event_xml = switch_event_xmlize(current->fields, SWITCH_VA_NONE);
					event_str = switch_xml_toxml(event_xml, SWITCH_FALSE);
					stream->write_function(stream, "%s", event_str);
					switch_xml_free(event_xml);
					switch_safe_free(event_str);
				}

				if (as_xml) {
					stream->write_function(stream, "  </row>\n");
				} else {
					stream->write_function(stream, " |\n");
				}
				current = current->next;
			}
			if (as_xml) {
				stream->write_function(stream, "</result>\n");
			}
		} else {
			if (lookup_status == SWITCH_STATUS_SUCCESS) {
				if (as_xml) {
					stream->write_function(stream, "<result row_count=\"0\">\n</results>\n");
				} else {
					stream->write_function(stream, "No Routes To Display\n");
				}
			} else {
				stream->write_function(stream, "-ERR Error looking up routes\n");
			}
		}
	}

end:
	switch_safe_free(data);
	lcr_destroy(cb_struct.head);
	if (!session) {
		if (pool) {
			switch_core_destroy_memory_pool(&pool);
		}
	}
	return SWITCH_STATUS_SUCCESS;
usage:
	stream->write_function(stream, "USAGE: %s\n", LCR_SYNTAX);
	goto end;
}

SWITCH_STANDARD_API(dialplan_lcr_admin_function)
{
	char *argv[32] = { 0 };
	int argc;
	char *mydata = NULL;
	switch_hash_index_t *hi;
	void *val;
	profile_t *profile;

	if (zstr(cmd)) {
		goto usage;
	}

	mydata = strdup(cmd);

	if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		if (argc < 2) {
			goto usage;
		}
		switch_assert(argv[0]);
		if (!strcasecmp(argv[0], "show") && !strcasecmp(argv[1], "profiles")) {
			for (hi = switch_core_hash_first(globals.profile_hash); hi; hi = switch_core_hash_next(&hi)) {
				switch_core_hash_this(hi, NULL, NULL, &val);
				profile = (profile_t *) val;

				stream->write_function(stream, "Name:\t\t%s\n", profile->name);
				if (zstr(profile->custom_sql)) {
					stream->write_function(stream, " ID:\t\t%d\n", profile->id);
					stream->write_function(stream, " order by:\t%s\n", profile->order_by);
				} else {
					stream->write_function(stream, " custom sql:\t%s\n", profile->custom_sql);
					stream->write_function(stream, " has %%:\t\t%s\n", profile->custom_sql_has_percent ? "true" : "false");
					stream->write_function(stream, " has vars:\t%s\n", profile->custom_sql_has_vars ? "true" : "false");
				}
				stream->write_function(stream, " has intrastate:\t%s\n", profile->profile_has_intrastate ? "true" : "false");
				stream->write_function(stream, " has intralata:\t%s\n", profile->profile_has_intralata ? "true" : "false");
				stream->write_function(stream, " has npanxx:\t%s\n", profile->profile_has_npanxx ? "true" : "false");
				stream->write_function(stream, " Reorder rate:\t%s\n", profile->reorder_by_rate ? "enabled" : "disabled");
				stream->write_function(stream, " Info in headers:\t%s\n", profile->info_in_headers ? "enabled" : "disabled");
				stream->write_function(stream, " Quote IN() List:\t%s\n", profile->quote_in_list ? "enabled" : "disabled");
				stream->write_function(stream, " Single Bridge:\t%s\n", profile->single_bridge ? "enabled" : "disabled");
				stream->write_function(stream, " Sip Redirection Mode:\t%s\n", profile->enable_sip_redir ? "enabled" : "disabled");
				stream->write_function(stream, " Import fields:\t%s\n", profile->export_fields_str ? profile->export_fields_str : "(null)");
				stream->write_function(stream, " Limit type:\t%s\n", profile->limit_type);
				stream->write_function(stream, "\n");
			}
		} else {
			goto usage;
		}
	}
	switch_safe_free(mydata);
	return SWITCH_STATUS_SUCCESS;
usage:
	switch_safe_free(mydata);
	stream->write_function(stream, "-ERR %s\n", LCR_ADMIN_SYNTAX);
	return SWITCH_STATUS_SUCCESS;

}

SWITCH_MODULE_LOAD_FUNCTION(mod_lcr_load)
{
	switch_api_interface_t *dialplan_lcr_api_interface;
	switch_api_interface_t *dialplan_lcr_api_admin_interface;
	switch_application_interface_t *app_interface;
	switch_dialplan_interface_t *dp_interface;

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	globals.pool = pool;

	if (switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, globals.pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to initialize mutex\n");
	}
	if (lcr_load_config() != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to load lcr config file\n");
		return SWITCH_STATUS_FALSE;
	}

	SWITCH_ADD_API(dialplan_lcr_api_interface, "lcr", "Least Cost Routing Module", dialplan_lcr_function, LCR_SYNTAX);
	SWITCH_ADD_API(dialplan_lcr_api_admin_interface, "lcr_admin", "Least Cost Routing Module Admin", dialplan_lcr_admin_function, LCR_ADMIN_SYNTAX);
	SWITCH_ADD_APP(app_interface, "lcr", "Perform an LCR lookup", "Perform an LCR lookup",
				   lcr_app_function, "<number>", SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);
	SWITCH_ADD_DIALPLAN(dp_interface, "lcr", lcr_dialplan_hunt);

	lcr_endpoint_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_ENDPOINT_INTERFACE);
	lcr_endpoint_interface->interface_name = "lcr";
	lcr_endpoint_interface->io_routines = &lcr_io_routines;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_lcr_shutdown)
{

	switch_core_hash_destroy(&globals.profile_hash);

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

/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2009, Anthony Minessale II <anthm@freeswitch.org>
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
 *
 * mod_lcr.c -- Least Cost Routing Module
 *
 */

#include <switch.h>
#include <switch_odbc.h>


#define LCR_SYNTAX "lcr <digits> [<lcr profile>] [caller_id]"
#define LCR_ADMIN_SYNTAX "lcr_admin show profiles"

/* SQL Query places */
#define LCR_DIGITS_PLACE 0
#define LCR_CARRIER_PLACE 1
#define LCR_RATE_PLACE 2
#define LCR_GW_PREFIX_PLACE 3
#define LCR_GW_SUFFIX_PLACE 4
#define LCR_LSTRIP_PLACE 5
#define LCR_TSTRIP_PLACE 6
#define LCR_PREFIX_PLACE 7
#define LCR_SUFFIX_PLACE 8
#define LCR_CODEC_PLACE 9
#define LCR_CID_PLACE 10

#define LCR_QUERY_COLS_REQUIRED 9
#define LCR_QUERY_COLS 11

#define LCR_HEADERS_COUNT 6

#define LCR_HEADERS_DIGITS 0
#define LCR_HEADERS_CARRIER 1
#define LCR_HEADERS_RATE 2
#define LCR_HEADERS_DIALSTRING 3
#define LCR_HEADERS_CODEC 4
#define LCR_HEADERS_CID 5

#define LCR_
static char headers[LCR_HEADERS_COUNT][32] = {
	"Digit Match",
	"Carrier",
	"Rate",
	"Dialstring",
	"Codec",
	"CID Regexp"
};

/* sql for random function */
static char *db_random = NULL;

struct odbc_obj {
	switch_odbc_handle_t *handle;
	SQLHSTMT stmt;
	SQLCHAR *colbuf;
	int32_t cblen;
	SQLCHAR *code;
	int32_t codelen;
};

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
	size_t lstrip;
	size_t tstrip;
	size_t digit_len;
	char *codec;
	char *cid;
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
};

typedef struct odbc_obj  odbc_obj_t;
typedef odbc_obj_t *odbc_handle;

typedef struct lcr_obj lcr_obj_t;
typedef lcr_obj_t *lcr_route;

typedef struct max_obj max_obj_t;
typedef max_obj_t *max_len;

struct profile_obj {
	char *name;
	uint16_t id;
	char *order_by;
	char *pre_order;
	char *custom_sql;
	switch_bool_t custom_sql_has_percent;
	switch_bool_t custom_sql_has_vars;
	
	switch_bool_t reorder_by_rate;
	switch_bool_t quote_in_list;
	switch_bool_t info_in_headers;
};
typedef struct profile_obj profile_t;

struct callback_obj {
	lcr_route head;
	switch_hash_t *dedup_hash;
	int matches;
	switch_memory_pool_t *pool;
	char *lookup_number;
	char *cid;
	profile_t *profile;
	switch_core_session_t *session;
	switch_event_t *event;
};
typedef struct callback_obj callback_t;

static struct {
	switch_memory_pool_t *pool;
	char *dbname;
	char *odbc_dsn;
	switch_mutex_t *mutex;
	switch_mutex_t *db_mutex;
	switch_odbc_handle_t *master_odbc;
	switch_hash_t *profile_hash;
	profile_t *default_profile;
	void *filler1;
} globals;


SWITCH_MODULE_LOAD_FUNCTION(mod_lcr_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_lcr_shutdown);
SWITCH_MODULE_DEFINITION(mod_lcr, mod_lcr_load, mod_lcr_shutdown, NULL);

static const char *do_cid(switch_memory_pool_t *pool, const char *cid, const char *number)
{
	switch_regex_t *re = NULL;
	int proceed = 0, ovector[30];
	char *substituted = NULL;
	uint32_t len = 0;
	char *src = NULL;
	char *dst = NULL;
	
	if (!switch_strlen_zero(cid)) {
		len = strlen(cid);
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
		dst = strchr(src, '/');
		*dst = '\0';
		dst++;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "src: %s, dst: %s\n", src, dst);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Not a valid regexp: %s\n", src);
		goto done;
	}
	
	if ((proceed = switch_regex_perform(number, src, &re, ovector, sizeof(ovector) / sizeof(ovector[0])))) {
		len = (uint32_t) (strlen(src) + strlen(dst) + 10) * proceed; /* guestimate size */
		if (!(substituted = switch_core_alloc(pool, len))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error!\n");
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
	return number;
}

static char *get_bridge_data(switch_memory_pool_t *pool, char *dialed_number, char *caller_id, lcr_route cur_route, profile_t *profile)
{
	size_t lstrip;
	size_t  tstrip;
	char *data = NULL;
	char *destination_number = NULL;
	char *orig_destination_number = NULL; 
	char *codec = NULL;
	char *cid = NULL;
	char *header = NULL;

	orig_destination_number = destination_number = switch_core_strdup(pool, dialed_number);
	
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
	if (!switch_strlen_zero(cur_route->codec)) {
		codec = switch_core_sprintf(pool, ",absolute_codec_string=%s", cur_route->codec);
	}
	
	cid = "";
	if (!switch_strlen_zero(cur_route->cid)) {
		cid = switch_core_sprintf(pool, ",origination_caller_id_number=%s", 
								  do_cid(pool, cur_route->cid, caller_id));
	}
	
	header = "";
	if (profile->info_in_headers) {
		header = switch_core_sprintf(pool, ",sip_h_X-LCR-INFO=lcr_rate=%s;lcr_carrier=%s",
									  cur_route->rate_str, 
									  cur_route->carrier_name);
	}
	
	data = switch_core_sprintf(pool, "[lcr_carrier=%s,lcr_rate=%s%s%s%s]%s%s%s%s%s"
								, cur_route->carrier_name, cur_route->rate_str
								, codec, cid, header
								, cur_route->gw_prefix, cur_route->prefix
								, destination_number, cur_route->suffix, cur_route->gw_suffix);
			
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Returning Dialstring %s\n", data);
	return data;
}

static profile_t *locate_profile(const char *profile_name)
{
	profile_t *profile = NULL;
	
	if (switch_strlen_zero(profile_name)) {
		profile = globals.default_profile;
	} else if (!(profile = switch_core_hash_find(globals.profile_hash, profile_name))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error invalid profile %s\n", profile_name);
	}
	
	return profile;
}


static void init_max_lens(max_len maxes)
{
	maxes->digit_str = (headers[LCR_HEADERS_DIGITS] == NULL ? 0 : strlen(headers[LCR_HEADERS_DIGITS]));
	maxes->carrier_name = (headers[LCR_HEADERS_CARRIER] == NULL ? 0 : strlen(headers[LCR_HEADERS_CARRIER]));
	maxes->dialstring = (headers[LCR_HEADERS_DIALSTRING] == NULL ? 0 : strlen(headers[LCR_HEADERS_DIALSTRING]));
	maxes->rate = 8;
	maxes->codec = (headers[LCR_HEADERS_CODEC] == NULL ? 0 : strlen(headers[LCR_HEADERS_CODEC]));
	maxes->cid = (headers[LCR_HEADERS_CID] == NULL ? 0 : strlen(headers[LCR_HEADERS_CID]));
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

		if (current->carrier_name != NULL) {
			this_len = strlen(current->carrier_name);
			if (this_len > maxes->carrier_name) {
				maxes->carrier_name = this_len;
			}
		}
		if (current->dialstring != NULL) {
			this_len = strlen(current->dialstring);
			if (this_len > maxes->dialstring) {
				maxes->dialstring = this_len;
			}
		}
		if (current->digit_str != NULL) {
			if (current->digit_len > maxes->digit_str) {
				maxes->digit_str = current->digit_len;
			} 
		}
		if (current->rate_str != NULL) {
			this_len = strlen(current->rate_str);
			if (this_len > maxes->rate) {
				maxes->rate = this_len;
			}
		}
		if (current->codec != NULL) {
			this_len= strlen(current->codec);
			if (this_len > maxes->codec) {
				maxes->codec = this_len;
			}
		}
		if (current->cid != NULL) {
			this_len = strlen(current->cid);
			if (this_len > maxes->cid) {
				maxes->cid = this_len;
			}
		}
	}
	return SWITCH_STATUS_SUCCESS;
}

static switch_bool_t db_check(char *sql)
{
	if (globals.odbc_dsn) {
		if (switch_odbc_handle_exec(globals.master_odbc, sql, NULL) == SWITCH_ODBC_SUCCESS) {
			return SWITCH_TRUE;
		}
	}
	
	return SWITCH_FALSE;
}

/* try each type of random until we suceed */
static switch_bool_t set_db_random()
{
	if (db_check("SELECT rand();") == SWITCH_TRUE) {
		db_random = "rand()";
		return SWITCH_TRUE;
	}
	if(db_check("SELECT random();") == SWITCH_TRUE) {
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

	digit_len = strlen(digits);
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
	char * tmpSQL = NULL;
	char * newSQL = NULL;
	switch_channel_t *channel;
	
	/* first replace %s with digits to maintain backward compat */
	if (cb_struct->profile->custom_sql_has_percent == SWITCH_TRUE) {
		tmpSQL = switch_string_replace(custom_sql, "%q", digits);
		newSQL = tmpSQL;
	}
	
	/* expand the vars */
	if (cb_struct->profile->custom_sql_has_vars == SWITCH_TRUE) {
		if (cb_struct->session) {
			channel = switch_core_session_get_channel(cb_struct->session);
			switch_assert(channel);
			/*
			newSQL = switch_channel_expand_variables_escape(channel, 
															tmpSQL ? tmpSQL : custom_sql,
															escape_sql);
			*/
			newSQL = switch_channel_expand_variables(channel, 
														tmpSQL ? tmpSQL : custom_sql);
		} else if (cb_struct->event) {
			/* use event system to expand vars */
			newSQL = switch_event_expand_headers(cb_struct->event, tmpSQL ? tmpSQL : custom_sql);
			
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT,
								"mod_lcr called without a valid session while using a custom_sql that has channel variables.\n");
		}
	}
	
	if (tmpSQL != newSQL) {
		switch_safe_free(tmpSQL);
	}
	
	if (newSQL == NULL) {
		return (char *) custom_sql;
	} else {
		return newSQL;
	}
}

static switch_bool_t lcr_execute_sql_callback(char *sql, switch_core_db_callback_func_t callback, void *pdata)
{
	switch_bool_t retval = SWITCH_FALSE;
	
	switch_mutex_lock(globals.db_mutex);
	if (globals.odbc_dsn) {
		if (switch_odbc_handle_callback_exec(globals.master_odbc, sql, callback, pdata)
				== SWITCH_ODBC_FAIL) {
			retval = SWITCH_FALSE;
		} else {
			retval = SWITCH_TRUE;
		}
	}
	switch_mutex_unlock(globals.db_mutex);
	return retval;
}

static int route_add_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	lcr_route additional = NULL;
	lcr_route current = NULL;
	callback_t *cbt = (callback_t *) pArg;
	char *key = NULL;
	
	switch_memory_pool_t *pool = cbt->pool;
	
	
	if (argc < LCR_QUERY_COLS_REQUIRED) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT,
							"Unexpected number of columns returned for SQL.  Returned column count: %d. "
							"If using a custom sql for this profile, verify it is correct.  Otherwise file a bug report.\n",
							argc);
		return SWITCH_STATUS_GENERR;
	}

	if (switch_strlen_zero(argv[LCR_GW_PREFIX_PLACE]) && switch_strlen_zero(argv[LCR_GW_SUFFIX_PLACE]) ) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
								"There's no way to dial this Gateway: Carrier: \"%s\" Prefix: \"%s\", Suffix \"%s\"\n",
								switch_str_nil(argv[LCR_CARRIER_PLACE]),
								switch_str_nil(argv[LCR_GW_PREFIX_PLACE]), switch_str_nil(argv[LCR_GW_SUFFIX_PLACE]));
		return SWITCH_STATUS_SUCCESS;
	}

	cbt->matches++;
	additional = switch_core_alloc(pool, sizeof(lcr_obj_t));

	additional->digit_len = strlen(argv[LCR_DIGITS_PLACE]);
	additional->digit_str = switch_core_strdup(pool, switch_str_nil(argv[LCR_DIGITS_PLACE]));
	additional->suffix = switch_core_strdup(pool, switch_str_nil(argv[LCR_SUFFIX_PLACE]));
	additional->prefix = switch_core_strdup(pool, switch_str_nil(argv[LCR_PREFIX_PLACE]));
	additional->carrier_name = switch_core_strdup(pool, switch_str_nil(argv[LCR_CARRIER_PLACE]));
	additional->rate = (float)atof(switch_str_nil(argv[LCR_RATE_PLACE]));
	additional->rate_str = switch_core_sprintf(pool, "%0.5f", additional->rate);
	additional->gw_prefix = switch_core_strdup(pool, switch_str_nil(argv[LCR_GW_PREFIX_PLACE]));
	additional->gw_suffix = switch_core_strdup(pool, switch_str_nil(argv[LCR_GW_SUFFIX_PLACE]));
	additional->lstrip = atoi(switch_str_nil(argv[LCR_LSTRIP_PLACE]));
	additional->tstrip = atoi(switch_str_nil(argv[LCR_TSTRIP_PLACE]));
	if (argc > LCR_CODEC_PLACE) {
		additional->codec = switch_core_strdup(pool, switch_str_nil(argv[LCR_CODEC_PLACE]));
	}
	if (argc > LCR_CID_PLACE) {
		additional->cid = switch_core_strdup(pool, switch_str_nil(argv[LCR_CID_PLACE]));
	}
	additional->dialstring = get_bridge_data(pool, cbt->lookup_number, cbt->cid, additional, cbt->profile);

	if (cbt->head == NULL) {
		key = switch_core_sprintf(pool, "%s:%s", additional->gw_prefix, additional->gw_suffix);
		additional->next = cbt->head;
		cbt->head = additional;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Adding %s to head of list\n", additional->carrier_name);
		if (switch_core_hash_insert(cbt->dedup_hash, key, additional) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error inserting into dedup hash\n");
			return SWITCH_STATUS_GENERR;
		}
		return SWITCH_STATUS_SUCCESS;
	}

	
	for (current = cbt->head; current; current = current->next) {
	
		key = switch_core_sprintf(pool, "%s:%s", additional->gw_prefix, additional->gw_suffix);
		if (switch_core_hash_find(cbt->dedup_hash, key)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
								"Ignoring Duplicate route for termination point (%s)\n",
								key);
			break;
		}

		if (!cbt->profile->reorder_by_rate) {
			/* use db order */
			if (current->next == NULL) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Adding %s to end of list\n", additional->carrier_name);
				current->next = additional;
				additional->prev = current;
				if (switch_core_hash_insert(cbt->dedup_hash, key, additional) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error inserting into dedup hash\n");
					return SWITCH_STATUS_GENERR;
				}
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
					return SWITCH_STATUS_GENERR;
				}
				break;
			} else if (current->next == NULL) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "adding %s to end of list after %s\n",
									additional->carrier_name, current->carrier_name);
				current->next = additional;
				additional->prev = current;
				if (switch_core_hash_insert(cbt->dedup_hash, key, additional) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error inserting into dedup hash\n");
					return SWITCH_STATUS_GENERR;
				}
				break;
			}
		}
	}
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t lcr_do_lookup(callback_t *cb_struct)
{
	switch_stream_handle_t sql_stream = { 0 };
	char *digits = cb_struct->lookup_number;
	char *digits_copy;
	char *digits_expanded;
	profile_t *profile = cb_struct->profile;
	switch_bool_t lookup_status;
	switch_channel_t *channel;
	char *id_str;
	char *safe_sql;
	
	switch_assert(cb_struct->lookup_number != NULL);

	digits_copy = string_digitsonly(cb_struct->pool, digits);
	if (switch_strlen_zero(digits_copy)) {
		return SWITCH_STATUS_GENERR;
	}
	
	/* allocate the dedup hash */
	if (switch_core_hash_init(&cb_struct->dedup_hash, cb_struct->pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error initializing the dedup hash\n");
		return SWITCH_STATUS_GENERR;
	}
	
	digits_expanded = expand_digits(cb_struct->pool, digits_copy, cb_struct->profile->quote_in_list);
	
	/* set some channel vars if we have a session */
	if (cb_struct->session) {
		if ((channel = switch_core_session_get_channel(cb_struct->session))) {
			switch_channel_set_variable_var_check(channel, "lcr_query_digits", digits_copy, SWITCH_FALSE);
			id_str = switch_core_sprintf(cb_struct->pool, "%d", cb_struct->profile->id);
			switch_channel_set_variable_var_check(channel, "lcr_query_profile", id_str, SWITCH_FALSE);
			switch_channel_set_variable_var_check(channel, "lcr_query_expanded_digits", digits_expanded, SWITCH_FALSE);
		}
	}
	if (cb_struct->event) {
		switch_event_add_header_string(cb_struct->event, SWITCH_STACK_BOTTOM, "lcr_query_digits", digits_copy);
		id_str = switch_core_sprintf(cb_struct->pool, "%d", cb_struct->profile->id);
		switch_event_add_header_string(cb_struct->event, SWITCH_STACK_BOTTOM, "lcr_query_profile", id_str);
		switch_event_add_header_string(cb_struct->event, SWITCH_STACK_BOTTOM, "lcr_query_expanded_digits", digits_expanded);
	}

	/* set up the query to be executed */

	/* format the custom_sql */
	safe_sql = format_custom_sql(profile->custom_sql, cb_struct, digits_copy);
	if (!safe_sql) {
		switch_event_safe_destroy(&cb_struct->event);
		switch_core_hash_destroy(&cb_struct->dedup_hash);
		return SWITCH_STATUS_GENERR;
	}
	SWITCH_STANDARD_STREAM(sql_stream);
	sql_stream.write_function(&sql_stream, safe_sql);
	if (safe_sql != profile->custom_sql) {
		/* channel_expand_variables returned the same string to us, no need to free */
		switch_safe_free(safe_sql);
	}
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "SQL: %s\n", (char *)sql_stream.data);    
	
	lookup_status = lcr_execute_sql_callback((char *)sql_stream.data, route_add_callback, cb_struct);

	switch_safe_free(sql_stream.data);
	switch_event_safe_destroy(&cb_struct->event);
	switch_core_hash_destroy(&cb_struct->dedup_hash);

	if (lookup_status) {
		return SWITCH_STATUS_SUCCESS;
	} else {
		return SWITCH_STATUS_GENERR;
	}
}

static switch_bool_t test_profile(char *lcr_profile)
{
	callback_t routes = { 0 };
	switch_memory_pool_t *pool = NULL;
	switch_event_t *event = NULL;

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
	return (lcr_do_lookup(&routes) == SWITCH_STATUS_SUCCESS) ?
	        SWITCH_TRUE : SWITCH_FALSE;
}

static switch_status_t lcr_load_config()
{
	char *cf = "lcr.conf";
	switch_stream_handle_t sql_stream = { 0 };
	switch_xml_t cfg, xml, settings, param, x_profile, x_profiles;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	char *odbc_user = NULL;
	char *odbc_pass = NULL;
	profile_t *profile = NULL;

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
			if (!strcasecmp(var, "odbc-dsn") && !switch_strlen_zero(val)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "odbc_dsn is %s\n", val);
				globals.odbc_dsn = switch_core_strdup(globals.pool, val);
				if ((odbc_user = strchr(globals.odbc_dsn, ':'))) {
					*odbc_user++ = '\0';
					if ((odbc_pass = strchr(odbc_user, ':'))) {
						*odbc_pass++ = '\0';
					}
				}
			}
		}
	}
	
	/* initialize sql here, 'cause we need to verify custom_sql for each profile below */
	if (globals.odbc_dsn) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO
						  , "dsn is \"%s\", user is \"%s\", and password is \"%s\"\n"
						  , globals.odbc_dsn, odbc_user, odbc_pass
						  );
		if (!(globals.master_odbc = switch_odbc_handle_new(globals.odbc_dsn, odbc_user, odbc_pass))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Cannot Open ODBC Database!\n");
			status = SWITCH_STATUS_FALSE;
			goto done;
		}
		if (switch_odbc_handle_connect(globals.master_odbc) != SWITCH_ODBC_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Cannot Open ODBC Database!\n");
			status = SWITCH_STATUS_FALSE;
			goto done;
		}
	}
	
	if (set_db_random() == SWITCH_TRUE) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Database RANDOM function set to %s\n", db_random);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to determine database RANDOM function\n");
	};

	switch_core_hash_init(&globals.profile_hash, globals.pool);
	if ((x_profiles = switch_xml_child(cfg, "profiles"))) {
		for (x_profile = switch_xml_child(x_profiles, "profile"); x_profile; x_profile = x_profile->next) {
			char *name = (char *) switch_xml_attr_soft(x_profile, "name");
			char *comma = ", ";
			switch_stream_handle_t order_by = { 0 };
			switch_stream_handle_t pre_order = { 0 };
			switch_stream_handle_t *thisorder = NULL;
			char *reorder_by_rate = NULL;
			char *quote_in_list = NULL;
			char *info_in_headers = NULL;
			char *id_s = NULL;
			char *custom_sql = NULL;
			int argc, x = 0;
			char *argv[4] = { 0 };
			
			SWITCH_STANDARD_STREAM(order_by);
			SWITCH_STANDARD_STREAM(pre_order);

			for (param = switch_xml_child(x_profile, "param"); param; param = param->next) {
				char *var, *val;

				var = (char *) switch_xml_attr_soft(param, "name");
				val = (char *) switch_xml_attr_soft(param, "value");
				
				if ((!strcasecmp(var, "order_by") || !strcasecmp(var, "pre_order"))  && !switch_strlen_zero(val)) {
					if (!strcasecmp(var, "order_by")) {
						thisorder = &order_by;
					} else if (!strcasecmp(var, "pre_order")) {
						thisorder = &pre_order;
						comma = ""; /* don't want leading comma */
					}
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "param val is %s\n", val);
					if ((argc = switch_separate_string(val, ',', argv, (sizeof(argv) / sizeof(argv[0]))))) {
						for (x=0; x<argc; x++) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "arg #%d/%d is %s\n", x, argc, argv[x]);
							if (!switch_strlen_zero(argv[x])) {
								if (!strcasecmp(argv[x], "quality")) {
									thisorder->write_function(thisorder, "%s quality DESC", comma);
								} else if (!strcasecmp(argv[x], "reliability")) {
									thisorder->write_function(thisorder, "%s reliability DESC", comma);
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
				} else if (!strcasecmp(var, "id") && !switch_strlen_zero(val)) {
					id_s = val;
				} else if (!strcasecmp(var, "custom_sql") && !switch_strlen_zero(val)) {
					custom_sql = val;
				} else if (!strcasecmp(var, "reorder_by_rate") && !switch_strlen_zero(val)) {
					reorder_by_rate = val;
				} else if (!strcasecmp(var, "info_in_headers") && !switch_strlen_zero(val)) {
					info_in_headers = val;
				} else if (!strcasecmp(var, "quote_in_list") && !switch_strlen_zero(val)) {
					quote_in_list = val;
				}
			}
			
			if (switch_strlen_zero(name)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No name specified.\n");
			} else {
				profile = switch_core_alloc(globals.pool, sizeof(*profile));
				memset(profile, 0, sizeof(profile_t));
				profile->name = switch_core_strdup(globals.pool, name);
				
				if (!switch_strlen_zero((char *)order_by.data)) {
					profile->order_by = switch_core_strdup(globals.pool, (char *)order_by.data);
				} else {
					/* default to rate */
					profile->order_by = ", rate";
				}
				if (!switch_strlen_zero((char *)pre_order.data)) {
					profile->pre_order = switch_core_strdup(globals.pool, (char *)pre_order.data);
				} else {
					/* default to rate */
					profile->pre_order = "";
				}

				if (!switch_strlen_zero(id_s)) {
					profile->id = (uint16_t)atoi(id_s);
				}
				
				/* SWITCH_STANDARD_STREAM doesn't use pools.  but we only have to free sql_stream.data */
				SWITCH_STANDARD_STREAM(sql_stream);
				if (switch_strlen_zero(custom_sql)) {
					/* use default sql */
					sql_stream.write_function(&sql_stream, 
											  "SELECT l.digits, c.carrier_name, l.rate, cg.prefix AS gw_prefix, cg.suffix AS gw_suffix, l.lead_strip, l.trail_strip, l.prefix, l.suffix "
											  );
					if (db_check("SELECT codec from carrier_gateway limit 1") == SWITCH_TRUE) {
						sql_stream.write_function(&sql_stream, ", cg.codec ");
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "codec field defined.\n");
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "codec field not defined, please update your lcr carrier_gateway database schema.\n");
					}
					if (db_check("SELECT cid from lcr limit 1") == SWITCH_TRUE) {
						sql_stream.write_function(&sql_stream, ", l.cid ");
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "cid field defined.\n");
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "cid field not defined, please update your lcr database schema.\n");
					}
					sql_stream.write_function(&sql_stream, "FROM lcr l JOIN carriers c ON l.carrier_id=c.id JOIN carrier_gateway cg ON c.id=cg.carrier_id WHERE c.enabled = '1' AND cg.enabled = '1' AND l.enabled = '1' AND digits IN (");
					sql_stream.write_function(&sql_stream, "${lcr_query_expanded_digits}");
					sql_stream.write_function(&sql_stream, ") AND CURRENT_TIMESTAMP BETWEEN date_start AND date_end ");
					if (profile->id > 0) {
						sql_stream.write_function(&sql_stream, "AND lcr_profile=%d ", profile->id);
					}
					sql_stream.write_function(&sql_stream, "ORDER BY %s%s digits DESC%s", 
															profile->pre_order,
															switch_strlen_zero(profile->pre_order)? "" : ",",
															profile->order_by);
					if (db_random) {
						sql_stream.write_function(&sql_stream, ", %s", db_random);
					}
					sql_stream.write_function(&sql_stream, ";");
					
					custom_sql = sql_stream.data;
				}
				
				if (switch_string_var_check_const(custom_sql) || switch_string_has_escaped_data(custom_sql)) {
					profile->custom_sql_has_vars = SWITCH_TRUE;
				}
				if (strstr(custom_sql, "%")) {
					profile->custom_sql_has_percent = SWITCH_TRUE;
				}
				profile->custom_sql = switch_core_strdup(globals.pool, (char *)custom_sql);
				
				if (!switch_strlen_zero(reorder_by_rate)) {
					profile->reorder_by_rate = switch_true(reorder_by_rate);
				}
				
				if (!switch_strlen_zero(info_in_headers)) {
					profile->info_in_headers = switch_true(info_in_headers);
				}
				
				if (!switch_strlen_zero(quote_in_list)) {
					profile->quote_in_list = switch_true(quote_in_list);
				}
				
				switch_core_hash_insert(globals.profile_hash, profile->name, profile);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Loaded lcr profile %s.\n", profile->name);
				/* test the profile */
				if (test_profile(profile->name) == SWITCH_TRUE) {
					if (!strcasecmp(profile->name, "default")) {
						globals.default_profile = profile;
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Setting user defined default profile: %s.\n", profile->name);
					}
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Removing INAVLID Profile %s.\n", profile->name);
					switch_core_hash_delete(globals.profile_hash, profile->name);
				}
				
			}
			switch_safe_free(order_by.data);
			switch_safe_free(pre_order.data);
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
		profile->pre_order = "";
		globals.default_profile = profile;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Setting system defined default profile.");
	}

	done:
	switch_xml_free(xml);
	return status;
}

SWITCH_STANDARD_DIALPLAN(lcr_dialplan_hunt)
{
	switch_caller_extension_t *extension = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	callback_t routes = { 0 };
	lcr_route cur_route = { 0 };
	char *lcr_profile = NULL;
	switch_memory_pool_t *pool = NULL;
	switch_event_t *event = NULL;

	if (session) {
		pool = switch_core_session_get_pool(session);
		routes.session = session;
	} else {
		switch_core_new_memory_pool(&pool);
		switch_event_create(&event, SWITCH_EVENT_MESSAGE);
		routes.event = event;
	}
	routes.pool = pool;
	if (!(routes.profile = locate_profile(lcr_profile))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unknown profile: %s\n", lcr_profile);
		goto end;
	}
	
	if (!caller_profile) {
		caller_profile = switch_channel_get_caller_profile(channel);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "LCR Lookup on %s\n", caller_profile->destination_number);
	routes.lookup_number = caller_profile->destination_number;
	routes.cid = (char *) caller_profile->caller_id_number;
	if (lcr_do_lookup(&routes) == SWITCH_STATUS_SUCCESS) {
		if ((extension = switch_caller_extension_new(session, caller_profile->destination_number, caller_profile->destination_number)) == 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "memory error!\n");
			goto end;
		}

		switch_channel_set_variable(channel, SWITCH_CONTINUE_ON_FAILURE_VARIABLE, "true");
		switch_channel_set_variable(channel, SWITCH_HANGUP_AFTER_BRIDGE_VARIABLE, "true");
		switch_channel_set_variable(channel, "import", "lcr_carrier,lcr_rate");

		for (cur_route = routes.head; cur_route; cur_route = cur_route->next) {
			switch_caller_extension_add_application(session, extension, "bridge", cur_route->dialstring);
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "LCR lookup failed for %s\n", caller_profile->destination_number);
	}

end:
	if (!session) {
		switch_core_destroy_memory_pool(&pool);
	}
	return extension;
}

void str_repeat(size_t how_many, char *what, switch_stream_handle_t *str_stream)
{
	size_t i;

	/*//switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "repeating %d of '%s'\n", how_many, what);*/

	for (i=0; i<how_many; i++) {
		str_stream->write_function(str_stream, "%s", what);
	}
}

SWITCH_STANDARD_APP(lcr_app_function)
{
	int argc = 0;
	char *argv[4] = { 0 };
	char *mydata = NULL;
	char *dest = NULL;
	char rbuf[1024] = "";
	char vbuf[1024] = "";
	char *rbp = rbuf;
	switch_size_t l = 0, rbl = sizeof(rbuf);
	uint32_t cnt = 1;
	char *lcr_profile = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_caller_profile_t *caller_profile = NULL;
	char *last_delim = "|";
	callback_t routes = { 0 };
	lcr_route cur_route = { 0 };
	switch_memory_pool_t *pool;
	switch_event_t *event;

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

	if (!caller_profile) {
		if (!(caller_profile = switch_channel_get_caller_profile(channel))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Unable to locate caller_profile\n");
		}
	}

	if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		dest = argv[0];
		if (argc > 1) {
			lcr_profile = argv[1];
		}
		
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "LCR Lookup on %s using profile %s\n", dest, lcr_profile);
		routes.lookup_number = dest;
		if (caller_profile) {
			routes.cid = (char *) switch_channel_get_variable(channel, "effective_caller_id_number");
			if (!routes.cid) {
				routes.cid = (char *) caller_profile->caller_id_number;
			}
		}
	
		if (!(routes.profile = locate_profile(lcr_profile))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unknown profile: %s\n", lcr_profile);
			goto end;
		}
		if (lcr_do_lookup(&routes) == SWITCH_STATUS_SUCCESS) {
			for (cur_route = routes.head; cur_route; cur_route = cur_route->next) {
				switch_snprintf(vbuf, sizeof(vbuf), "lcr_route_%d", cnt);
				switch_channel_set_variable(channel, vbuf, cur_route->dialstring);
				switch_snprintf(vbuf, sizeof(vbuf), "lcr_rate_%d", cnt);
				switch_channel_set_variable(channel, vbuf, cur_route->rate_str);
				switch_snprintf(vbuf, sizeof(vbuf), "lcr_carrier_%d", cnt);
				switch_channel_set_variable(channel, vbuf, cur_route->carrier_name);
				switch_snprintf(vbuf, sizeof(vbuf), "lcr_codec_%d", cnt);
				switch_channel_set_variable(channel, vbuf, cur_route->codec);
				cnt++;
				switch_snprintf(rbp, rbl, "%s|", cur_route->dialstring);
				last_delim = end_of_p(rbp);
				l = strlen(cur_route->dialstring) + 1;
				rbp += l;
				rbl -= l;
			}
			switch_snprintf(vbuf, sizeof(vbuf), "%d", cnt - 1);
			switch_channel_set_variable(channel, "lcr_route_count", vbuf);
			*(rbuf + strlen(rbuf) - 1) = '\0';
			switch_channel_set_variable(channel, "lcr_auto_route", rbuf);
			switch_channel_set_variable(channel, "import", "lcr_carrier,lcr_rate");
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "LCR lookup failed for %s\n", dest);
		}
	}
	
end:
	if (!session) {
		switch_core_destroy_memory_pool(&pool);
	}
}

SWITCH_STANDARD_API(dialplan_lcr_function)
{
	char *argv[4] = { 0 };
	int argc;
	char *mydata = NULL;
	char *dialstring = NULL;
	char *lcr_profile = NULL;
	lcr_route current = NULL;
	max_obj_t maximum_lengths = { 0 };
	callback_t cb_struct = { 0 };
	switch_memory_pool_t *pool;
	switch_event_t *event;
	switch_status_t lookup_status = SWITCH_STATUS_SUCCESS;

	if (switch_strlen_zero(cmd)) {
		goto usage;
	}

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
			cb_struct.cid = switch_core_strdup(pool, argv[2]);
		} else {
			cb_struct.cid = "18005551212";
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING
							  , "Using default CID [%s]\n", cb_struct.cid
							  );
		}
		if (!(cb_struct.profile = locate_profile(lcr_profile))) {
			stream->write_function(stream, "-ERR Unknown profile: %s\n", lcr_profile);
			goto end;
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO
						  , "data passed to lcr is [%s]\n", cmd
						  );
		lookup_status = lcr_do_lookup(&cb_struct);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO
						  , "lcr lookup returned [%d]\n"
						  , lookup_status
						  );
		if (cb_struct.head != NULL) {
			size_t len;

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

			stream->write_function(stream, " | %s", headers[LCR_HEADERS_DIALSTRING]);
			if ((len = (maximum_lengths.dialstring - strlen(headers[LCR_HEADERS_DIALSTRING]))) > 0) {
				str_repeat(len, " ", stream);
			}

			stream->write_function(stream, " |\n");

			current = cb_struct.head;
			while (current) {

				dialstring = get_bridge_data(pool, cb_struct.lookup_number, cb_struct.cid, current, cb_struct.profile);

				stream->write_function(stream, " | %s", current->digit_str);
				str_repeat((maximum_lengths.digit_str - current->digit_len), " ", stream);
				
				stream->write_function(stream, " | %s", current->carrier_name );
				str_repeat((maximum_lengths.carrier_name - strlen(current->carrier_name)), " ", stream);
				
				stream->write_function(stream, " | %s", current->rate_str );
				str_repeat((maximum_lengths.rate - strlen(current->rate_str)), " ", stream);

				if (current->codec) {
					stream->write_function(stream, " | %s", current->codec );
					str_repeat((maximum_lengths.codec - strlen(current->codec)), " ", stream);
				} else {
					stream->write_function(stream, " | ");
					str_repeat((maximum_lengths.codec), " ", stream);
				}

				if (current->cid) {
					stream->write_function(stream, " | %s", current->cid );
					str_repeat((maximum_lengths.cid - strlen(current->cid)), " ", stream);
				} else {
					stream->write_function(stream, " | ");
					str_repeat((maximum_lengths.cid), " ", stream);
				}
				
				stream->write_function(stream, " | %s", dialstring);
				str_repeat((maximum_lengths.dialstring - strlen(dialstring)), " ", stream);
				
				stream->write_function(stream, " |\n");
				
				current = current->next;
			}

		} else {
			if (lookup_status == SWITCH_STATUS_SUCCESS) {
				stream->write_function(stream, "No Routes To Display\n");
			} else {
				stream->write_function(stream, "Error looking up routes\n");
			}
		}
	}

end:
	if (!session) {
		switch_core_destroy_memory_pool(&pool);
	}
	return SWITCH_STATUS_SUCCESS;
 usage:
	stream->write_function(stream, "USAGE: %s\n", LCR_SYNTAX);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_API(dialplan_lcr_admin_function)
{
	char *argv[4] = { 0 };
	int argc;
	char *mydata = NULL;
	switch_hash_index_t *hi;
	void *val;
	profile_t *profile;

	if (switch_strlen_zero(cmd)) {
		goto usage;
	}

	mydata = strdup(cmd);

	if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		if (argc < 2) {
			goto usage;
		}
		switch_assert(argv[0]);
		if (!strcasecmp(argv[0], "show") && !strcasecmp(argv[1], "profiles")) {
			for (hi = switch_hash_first(NULL, globals.profile_hash); hi; hi = switch_hash_next(hi)) {
				switch_hash_this(hi, NULL, NULL, &val);
				profile = (profile_t *) val;
				
				stream->write_function(stream, "Name:\t\t%s\n", profile->name);
				if (switch_strlen_zero(profile->custom_sql)) {
					stream->write_function(stream, " ID:\t\t%d\n", profile->id);
					stream->write_function(stream, " order by:\t%s\n", profile->order_by);
					if (!switch_strlen_zero(profile->pre_order)) {
						stream->write_function(stream, " pre_order:\t%s\n", profile->pre_order);
					}
				} else {
					stream->write_function(stream, " custom sql:\t%s\n", profile->custom_sql);
					stream->write_function(stream, " has %%:\t\t%s\n", profile->custom_sql_has_percent ? "true" : "false");
					stream->write_function(stream, " has vars:\t%s\n", profile->custom_sql_has_vars ? "true" : "false");
				}
				stream->write_function(stream, " Reorder rate:\t%s\n", 
										profile->reorder_by_rate ? "enabled" : "disabled");
				stream->write_function(stream, " Info in headers:\t%s\n", 
										profile->info_in_headers ? "enabled" : "disabled");
				stream->write_function(stream, " Quote IN() List:\t%s\n", 
										profile->quote_in_list ? "enabled" : "disabled");
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

#ifndef SWITCH_HAVE_ODBC
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "You must have ODBC support in FreeSWITCH to use this module\n");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "\t./configure --enable-core-odbc-support\n");
	return SWITCH_STATUS_FALSE;
#endif

	globals.pool = pool;

	if (switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, globals.pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to initialize mutex\n");
	}
	if (switch_mutex_init(&globals.db_mutex, SWITCH_MUTEX_UNNESTED, globals.pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to initialize db_mutex\n");
	}

	if (lcr_load_config() != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to load lcr config file\n");
		return SWITCH_STATUS_FALSE;
	}

	SWITCH_ADD_API(dialplan_lcr_api_interface, "lcr", "Least Cost Routing Module", dialplan_lcr_function, LCR_SYNTAX);
	SWITCH_ADD_API(dialplan_lcr_api_admin_interface, "lcr_admin", "Least Cost Routing Module Admin", dialplan_lcr_admin_function, LCR_ADMIN_SYNTAX);
	SWITCH_ADD_APP(app_interface, "lcr", "Perform an LCR lookup", "Perform an LCR lookup",
				   lcr_app_function, "<number>", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_DIALPLAN(dp_interface, "lcr", lcr_dialplan_hunt);
	
	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_lcr_shutdown)
{

#ifdef SWITCH_HAVE_ODBC
	switch_odbc_handle_disconnect(globals.master_odbc);
	switch_odbc_handle_destroy(&globals.master_odbc);
#endif
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */

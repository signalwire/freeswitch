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
 * Raymond Chandler <intralanman@gmail.com>
 * Rupa Schomaker <rupa@rupa.com>
 *
 * mod_lcr.c -- Least Cost Routing Module
 *
 */

#include <switch.h>
#include <switch_odbc.h>


#define LCR_SYNTAX "lcr <digits> [<lcr profile>]"

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


#define LCR_DIALSTRING_PLACE 3
#define LCR_HEADERS_COUNT 4
char headers[LCR_HEADERS_COUNT][32] = {
	"Digit Match",
	"Carrier",
	"Rate",
	"Dialstring",
};

/* sql for random function */
char *db_random = NULL;

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
	struct lcr_obj *next;
};

struct max_obj {
	size_t carrier_name;
	size_t digit_str;
	size_t rate;
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
};
typedef struct profile_obj profile_t;

struct callback_obj {
	lcr_route head;
	int matches;
	char *lookup_number;
};
typedef struct callback_obj callback_t;

static struct {
	switch_memory_pool_t *pool;
	char *dbname;
	char *odbc_dsn;
	switch_mutex_t *mutex;
	switch_odbc_handle_t *master_odbc;
	switch_hash_t *profile_hash;
	profile_t *default_profile;
	void *filler1;
} globals;


SWITCH_MODULE_LOAD_FUNCTION(mod_lcr_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_lcr_shutdown);
SWITCH_MODULE_DEFINITION(mod_lcr, mod_lcr_load, mod_lcr_shutdown, NULL);

static char *get_bridge_data(const char *dialed_number, lcr_route cur_route)
{
	size_t lstrip;
	size_t  tstrip;
	char *data = NULL;
	char *destination_number = NULL;
	char *orig_destination_number = NULL; 

	orig_destination_number = destination_number = strdup(dialed_number);

	tstrip = ((cur_route->digit_len - cur_route->tstrip) + 1);
	lstrip = cur_route->lstrip;
	
	if (strlen(destination_number) > tstrip && cur_route->tstrip > 0) {
		destination_number[tstrip] = '\0';
	}
	if (strlen(destination_number) > lstrip && cur_route->lstrip > 0) {
		destination_number += lstrip;
	}
	
	data = switch_mprintf("%s%s%s%s%s", cur_route->gw_prefix, cur_route->prefix
						  , destination_number, cur_route->suffix, cur_route->gw_suffix);
		
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Returning Dialstring %s\n", data);
	switch_safe_free(orig_destination_number);
	return data;
}

void init_max_lens(max_len maxes)
{
	maxes->digit_str = (headers[LCR_DIGITS_PLACE] == NULL ? 0 : strlen(headers[LCR_DIGITS_PLACE]));
	maxes->carrier_name = (headers[LCR_CARRIER_PLACE] == NULL ? 0 : strlen(headers[LCR_CARRIER_PLACE]));
	maxes->dialstring = (headers[LCR_DIALSTRING_PLACE] == NULL ? 0 : strlen(headers[LCR_DIALSTRING_PLACE]));
	maxes->digit_str = (headers[LCR_DIGITS_PLACE] == NULL ? 0 : strlen(headers[LCR_DIGITS_PLACE]));
	maxes->rate = 8;
}

switch_status_t process_max_lengths(max_obj_t *maxes, lcr_route routes, char *destination_number)
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
	}
	return SWITCH_STATUS_SUCCESS;
}

/* try each type of random until we suceed */
static switch_bool_t set_db_random()
{
	char *sql = NULL;
	if (globals.odbc_dsn) {
		sql = "SELECT * FROM lcr ORDER BY rand() LIMIT 1";
		if(switch_odbc_handle_exec(globals.master_odbc, sql, NULL)
				== SWITCH_ODBC_SUCCESS) {
			db_random = "rand()";
			return SWITCH_TRUE;
		}
		sql = "SELECT * FROM lcr ORDER BY random() LIMIT 1";
		if(switch_odbc_handle_exec(globals.master_odbc, sql, NULL)
				== SWITCH_ODBC_SUCCESS) {
			db_random = "random()";
			return SWITCH_TRUE;
		}
	}
	return SWITCH_FALSE;
}

/* make a new string with digits only */
static char *string_digitsonly(const char *str) 
{
	char *p, *np, *newstr;
	size_t len;

	p = (char *)str;

	len = strlen(str);
	switch_zmalloc(newstr, len+1); /* worst case, same string */
	np = newstr;
	
	while(*p) {
		if(switch_isdigit(*p)) {
			*np = *p;
			np++;
		}
		p++;
	}
	*np = '\0';

	return newstr;
}

static switch_bool_t lcr_execute_sql_callback(char *sql, switch_core_db_callback_func_t callback, void *pdata)
{
	if (globals.odbc_dsn) {
		if(switch_odbc_handle_callback_exec(globals.master_odbc, sql, callback, pdata)
				== SWITCH_ODBC_FAIL) {
			return SWITCH_FALSE;
		} else {
			return SWITCH_TRUE;
		}
	}
	return SWITCH_FALSE;
}

int route_add_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	lcr_route additional = NULL;
	lcr_route current = NULL;
	callback_t *cbt = (callback_t *) pArg;
	char srate[10];
	
	cbt->matches++;

	switch_zmalloc(additional, sizeof(lcr_obj_t));

	additional->digit_len = strlen(argv[LCR_DIGITS_PLACE]);
	additional->digit_str = switch_safe_strdup(argv[LCR_DIGITS_PLACE]);
	additional->suffix = switch_safe_strdup(argv[LCR_SUFFIX_PLACE]);
	additional->prefix = switch_safe_strdup(argv[LCR_PREFIX_PLACE]);
	additional->carrier_name = switch_safe_strdup(argv[LCR_CARRIER_PLACE]);
	additional->rate = (float)atof(argv[LCR_RATE_PLACE]);
	switch_snprintf(srate, sizeof(srate), "%0.5f", additional->rate);
	additional->rate_str = switch_safe_strdup(srate);
	additional->gw_prefix = switch_safe_strdup(argv[LCR_GW_PREFIX_PLACE]);
	additional->gw_suffix = switch_safe_strdup(argv[LCR_GW_SUFFIX_PLACE]);
	additional->lstrip = atoi(argv[LCR_LSTRIP_PLACE]);
	additional->tstrip = atoi(argv[LCR_TSTRIP_PLACE]);
	additional->dialstring = get_bridge_data(cbt->lookup_number, additional);

	if (cbt->head == NULL) {
		additional->next = cbt->head;
		cbt->head = additional;

		return SWITCH_STATUS_SUCCESS;
	}

	for (current = cbt->head; current; current = current->next) {

	if (switch_strlen_zero(additional->gw_prefix) && switch_strlen_zero(additional->gw_suffix) ) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING
							  , "WTF?!? There's no way to dial this Gateway: Carrier: \"%s\" Prefix: \"%s\", Suffix \"%s\"\n"
							  , additional->carrier_name
							  , additional->gw_prefix, additional->gw_suffix
							  );
			break;
		}
			
		if (!strcmp(current->gw_prefix, additional->gw_prefix) && !strcmp(current->gw_suffix, additional->gw_suffix)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG
							  , "Ignoring Duplicate route for termination point (%s:%s)\n"
							  , additional->gw_prefix, additional->gw_suffix
							  );
			switch_safe_free(additional);
			break;
		}
			
		if (current->next == NULL) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "adding route to end of list\n");
			current->next = additional;
			break;
		}
	}
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t lcr_do_lookup(callback_t *cb_struct, char *digits, char* profile_name)
{
	/* instantiate the object/struct we defined earlier */
	switch_stream_handle_t sql_stream = { 0 };
	size_t n, digit_len = strlen(digits);
	char *digits_copy;
	profile_t *profile;
	switch_bool_t lookup_status;

	if (switch_strlen_zero(digits)) {
		return SWITCH_FALSE;
	}
	
	/* locate the profile */
	if(switch_strlen_zero(profile_name)) {
		profile = globals.default_profile;
	} else if (!(profile = switch_core_hash_find(globals.profile_hash, profile_name))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error invalid profile %s\n", profile_name);
		return SWITCH_STATUS_FALSE;
	}

	SWITCH_STANDARD_STREAM(sql_stream);

	/* set up the query to be executed */
	sql_stream.write_function(&sql_stream, 
							  "SELECT l.digits, c.carrier_name, l.rate, cg.prefix AS gw_prefix, cg.suffix AS gw_suffix, l.lead_strip, l.trail_strip, l.prefix, l.suffix "
							  );
	sql_stream.write_function(&sql_stream, "FROM lcr l JOIN carriers c ON l.carrier_id=c.id JOIN carrier_gateway cg ON c.id=cg.carrier_id WHERE c.enabled = '1' AND cg.enabled = '1' AND l.enabled = '1' AND digits IN (");
	digits_copy = string_digitsonly(digits);
	for (n = digit_len; n > 0; n--) {
		digits_copy[n] = '\0';
		sql_stream.write_function(&sql_stream, "%s%s", (n==digit_len ? "" : ", "), digits_copy);
	}
	switch_safe_free(digits_copy);
	sql_stream.write_function(&sql_stream, ") AND CURRENT_TIMESTAMP BETWEEN date_start AND date_end ");
	if(profile->id > 0) {
		sql_stream.write_function(&sql_stream, "AND lcr_profile=%d ", profile->id);
	}
	sql_stream.write_function(&sql_stream, "ORDER BY digits DESC%s", profile->order_by);
	if(db_random) {
		sql_stream.write_function(&sql_stream, ", %s", db_random);
	}
	sql_stream.write_function(&sql_stream, ";");
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "%s\n", (char *)sql_stream.data);    
	
	lookup_status = lcr_execute_sql_callback((char *)sql_stream.data, route_add_callback, cb_struct);
	switch_safe_free(sql_stream.data);
	
	if(lookup_status) {
		return SWITCH_STATUS_SUCCESS;
	} else {
		return SWITCH_STATUS_GENERR;
	}
}

static switch_status_t lcr_load_config()
{
	char *cf = "lcr.conf";
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
	
	/* define default profile  */
	profile = switch_core_alloc(globals.pool, sizeof(*profile));
	profile->name = "global_default";
	profile->order_by = ", rate";
	globals.default_profile = profile;
	
	switch_core_hash_init(&globals.profile_hash, globals.pool);
	if((x_profiles = switch_xml_child(cfg, "profiles"))) {
		for (x_profile = switch_xml_child(x_profiles, "profile"); x_profile; x_profile = x_profile->next) {
			char *name = (char *) switch_xml_attr_soft(x_profile, "name");
			switch_stream_handle_t order_by = { 0 };
			char *id_s = NULL;
			int argc, x = 0;
			char *argv[4] = { 0 };
			
			SWITCH_STANDARD_STREAM(order_by);
			for (param = switch_xml_child(x_profile, "param"); param; param = param->next) {
				char *var, *val;

				var = (char *) switch_xml_attr_soft(param, "name");
				val = (char *) switch_xml_attr_soft(param, "value");
				
				if (!strcasecmp(var, "order_by") && !switch_strlen_zero(val)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "param val is %s\n", val);
					if ((argc = switch_separate_string(val, ',', argv, (sizeof(argv) / sizeof(argv[0]))))) {
						for (x=0; x<argc; x++) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "arg #%d/%d is %s\n", x, argc, argv[x]);
							if(!switch_strlen_zero(argv[x])) {
								if (!strcasecmp(argv[x], "quality")) {
									order_by.write_function(&order_by, ", quality DESC");
								} else if(!strcasecmp(argv[x], "reliability")) {
									order_by.write_function(&order_by, ", reliability DESC");
								} else {
									order_by.write_function(&order_by, ", %s", argv[x]);
								}
							} else {
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "arg #%d is empty\n", x);
							}
						}
					} else {
						if (!strcasecmp(val, "quality")) {
							order_by.write_function(&order_by, ", quality DESC");
						} else if(!strcasecmp(val, "reliability")) {
							order_by.write_function(&order_by, ", reliability DESC");
						} else {
							order_by.write_function(&order_by, ", %s", val);
						}
					}
				} else if (!strcasecmp(var, "id") && !switch_strlen_zero(val)) {
					id_s = val;
				}
			}
			
			if(switch_strlen_zero(name)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No name specified.\n");
			} else {
				profile = switch_core_alloc(globals.pool, sizeof(*profile));
				profile->name = switch_core_strdup(globals.pool, name);
				
				if(!switch_strlen_zero((char *)order_by.data)) {
					profile->order_by = switch_core_strdup(globals.pool, (char *)order_by.data);
					switch_safe_free(order_by.data);
				} else {
					/* default to rate */
					profile->order_by = ", rate";
				}
				if(!switch_strlen_zero(id_s)) {
					profile->id = atoi(id_s);
				}
				
				switch_core_hash_insert(globals.profile_hash, profile->name, profile);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Loaded lcr profile %s.\n", profile->name);
			}
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "No lcr profiles defined.\n");
	}
	

	
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
 done:
	switch_xml_free(xml);
	return status;
}

static void destroy_list(lcr_route *head)
{
	lcr_route cur = NULL, top = *head;

	while (top) {
		cur = top;
		top = top->next;
		switch_safe_free(cur->digit_str);
		switch_safe_free(cur->suffix);
		switch_safe_free(cur->prefix);
		switch_safe_free(cur->carrier_name);
		switch_safe_free(cur->gw_prefix);
		switch_safe_free(cur->gw_suffix);
		switch_safe_free(cur->rate_str);
		switch_safe_free(cur);
	}
	*head = NULL;
}

SWITCH_STANDARD_DIALPLAN(lcr_dialplan_hunt)
{
	switch_caller_extension_t *extension = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	callback_t routes = { 0 };
	lcr_route cur_route = { 0 };
	char *bridge_data = NULL;
	char *lcr_profile = NULL;

	if (!caller_profile) {
		caller_profile = switch_channel_get_caller_profile(channel);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "LCR Lookup on %s\n", caller_profile->destination_number);
	routes.lookup_number = caller_profile->destination_number;
	if (lcr_do_lookup(&routes, caller_profile->destination_number, lcr_profile) == SWITCH_STATUS_SUCCESS) {
		if ((extension = switch_caller_extension_new(session, caller_profile->destination_number, caller_profile->destination_number)) == 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "memory error!\n");
			destroy_list(&routes.head);
			return NULL;
		}

		switch_channel_set_variable(channel, SWITCH_CONTINUE_ON_FAILURE_VARIABLE, "true");
		switch_channel_set_variable(channel, SWITCH_HANGUP_AFTER_BRIDGE_VARIABLE, "true");

		for (cur_route = routes.head; cur_route; cur_route = cur_route->next) {
			/*bridge_data = get_bridge_data(caller_profile->destination_number, cur_route);*/
			switch_caller_extension_add_application(session, extension, "bridge", cur_route->dialstring);
			switch_safe_free(bridge_data);
		}
		destroy_list(&routes.head);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "LCR lookup failed for %s\n", caller_profile->destination_number);
	}
	switch_safe_free(bridge_data);

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
	char *last_delim = "|";
	callback_t routes = { 0 };
	lcr_route cur_route = { 0 };

	if (!(mydata = switch_core_session_strdup(session, data))) {
		return;
	}

	if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		dest = argv[0];
		if(argc > 1) {
			lcr_profile = argv[1];
		}
		
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "LCR Lookup on %s\n", dest);
		routes.lookup_number = dest;
		if (lcr_do_lookup(&routes, dest, lcr_profile) == SWITCH_STATUS_SUCCESS) {
			for (cur_route = routes.head; cur_route; cur_route = cur_route->next) {
				switch_snprintf(vbuf, sizeof(vbuf), "lcr_route_%d", cnt++);
				switch_channel_set_variable(channel, vbuf, cur_route->dialstring);
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
			destroy_list(&routes.head);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "LCR lookup failed for %s\n", dest);
		}
	}
}

SWITCH_STANDARD_API(dialplan_lcr_function)
{
	char *argv[4] = { 0 };
	int argc;
	char *mydata = NULL;
	char *dialstring = NULL;
	char *destination_number = NULL;
	char *lcr_profile = NULL;
	lcr_route current = NULL;
	max_obj_t maximum_lengths = { 0 };
	callback_t cb_struct = { 0 };
	switch_status_t lookup_status = SWITCH_STATUS_SUCCESS;
	/*//switch_malloc(maximum_lengths, sizeof(max_obj_t)); */

	if (switch_strlen_zero(cmd)) {
		goto usage;
	}

	mydata = switch_safe_strdup(cmd);

	if ((argc = switch_separate_string(mydata, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		destination_number = strdup(argv[0]);
		if(argc > 1) {
			lcr_profile = argv[1];
		}
		cb_struct.lookup_number = destination_number;

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO
						  , "data passed to lcr is [%s]\n", cmd
						  );
		lookup_status = lcr_do_lookup(&cb_struct, destination_number, lcr_profile);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO
						  , "lcr lookup returned [%d]\n"
						  , lookup_status
						  );
		if (cb_struct.head != NULL) {
			size_t len;

			process_max_lengths(&maximum_lengths, cb_struct.head, destination_number);

			stream->write_function(stream, " | %s", headers[LCR_DIGITS_PLACE]);
			if ((len = (maximum_lengths.digit_str - strlen(headers[LCR_DIGITS_PLACE]))) > 0) {
				str_repeat(len, " ", stream);
			}

			stream->write_function(stream, " | %s", headers[LCR_CARRIER_PLACE]);
			if ((len = (maximum_lengths.carrier_name - strlen(headers[LCR_CARRIER_PLACE]))) > 0) {
				str_repeat(len, " ", stream);
			}

			stream->write_function(stream, " | %s", headers[LCR_RATE_PLACE]);
			if ((len = (maximum_lengths.rate - strlen(headers[LCR_RATE_PLACE]))) > 0) {
				str_repeat(len, " ", stream);
			}

			stream->write_function(stream, " | %s", headers[LCR_DIALSTRING_PLACE]);
			if ((len = (maximum_lengths.dialstring - strlen(headers[LCR_DIALSTRING_PLACE]))) > 0) {
				str_repeat(len, " ", stream);
			}

			stream->write_function(stream, " |\n");

			current = cb_struct.head;
			while (current) {

				dialstring = get_bridge_data(destination_number, current);

				stream->write_function(stream, " | %s", current->digit_str);
				str_repeat((maximum_lengths.digit_str - current->digit_len), " ", stream);
				
				stream->write_function(stream, " | %s", current->carrier_name );
				str_repeat((maximum_lengths.carrier_name - strlen(current->carrier_name)), " ", stream);
				
				stream->write_function(stream, " | %s", current->rate_str );
				str_repeat((maximum_lengths.rate - strlen(current->rate_str)), " ", stream);
								
				stream->write_function(stream, " | %s", dialstring);
				str_repeat((maximum_lengths.dialstring - strlen(dialstring)), " ", stream);
				
				stream->write_function(stream, " |\n");
				
				switch_safe_free(dialstring);
				current = current->next;
			}

			destroy_list(&cb_struct.head);
			switch_safe_free(dialstring);
		} else {
			if(lookup_status == SWITCH_STATUS_SUCCESS) {
				stream->write_function(stream, "No Routes To Display\n");
			} else {
				stream->write_function(stream, "Error looking up routes\n");
			}
		}
	}

	switch_safe_free(mydata);
	return SWITCH_STATUS_SUCCESS;
 usage:
	stream->write_function(stream, "USAGE: %s\n", LCR_SYNTAX);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_lcr_load)
{
	switch_api_interface_t *dialplan_lcr_api_interface;
	switch_application_interface_t *app_interface;
	switch_dialplan_interface_t *dp_interface;
	
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

#ifndef SWITCH_HAVE_ODBC
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "You must have ODBC support in FreeSWITCH to use this module\n");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "\t./configure --enable-core-odbc-support\n");
	return SWITCH_STATUS_FALSE;
#endif

	globals.pool = pool;
	

	if (lcr_load_config() != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to load lcr config file\n");
		return SWITCH_STATUS_FALSE;
	}
	if (switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, globals.pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to initialize mutex\n");
	}
	
	if(set_db_random() == SWITCH_TRUE) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Database RANDOM function set to %s\n", db_random);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unable to determine database RANDOM function\n");
	};

	SWITCH_ADD_API(dialplan_lcr_api_interface, "lcr", "Least Cost Routing Module", dialplan_lcr_function, LCR_SYNTAX);
	SWITCH_ADD_APP(app_interface, "lcr", "Perform an LCR lookup", "Perform an LCR lookup",
				   lcr_app_function, "<number>", SAF_SUPPORT_NOMEDIA);
	SWITCH_ADD_DIALPLAN(dp_interface, "lcr", lcr_dialplan_hunt);
	
	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_lcr_shutdown)
{
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

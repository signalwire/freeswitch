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
 *
 * The Initial Developer of this module is
 * Darren Schreiber <d@d-man.org>
 *
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Darren Schreiber <d@d-man.org>
 * Rupa Schomaker <rupa@rupa.com>
 * Emmanuel Schmidbauer <e.schmidbauer@gmail.com>
 *
 * mod_nibblebill.c - Nibble Billing
 * Purpose is to allow real-time debiting of credit or cash from a database while calls are in progress. I had the following goals:
 *
 * Debit credit/cash from accounts real-time 
 * Allow for billing at different rates during a single call 
 * Allow for warning callers when their balance is low (via audio, in-channel) 
 * Allow for disconnecting or re-routing calls when balance is depleted 
 * Allow for billing to function as listed above with multiple concurrent calls 
 * 
 * Thanks go to bandwidth.com for funding this work.
 *
 *
 * TODO: Fix what happens when the DB is not available
 * TODO: Fix what happens when the DB queries fail (right now, all are acting like success)
 * TODO: Add buffering abilities
 * TODO: Make error handling for database, such that when the database is down (or not installed) we just log to a text file
 * FUTURE: Possibly make the hooks not tied per-channel, and instead just do this as a supervision style application with one thread that watches all calls
 */

#include <switch.h>

typedef struct {
	switch_time_t lastts;		/* Last time we did any billing */
	double total;				/* Total amount billed so far */

	switch_time_t pausets;		/* Timestamp of when a pause action started. 0 if not paused */
	double bill_adjustments;	/* Adjustments to make to the next billing, based on pause/resume events */

	int lowbal_action_executed;	/* Set to 1 once lowbal_action has been executed */
} nibble_data_t;


typedef struct nibblebill_results {
	double balance;

	double percall_max;			/* Overrides global on a per-user level */
	double lowbal_amt;			/*  ditto */
} nibblebill_results_t;


/* Keep track of our config, event hooks and database connection variables, for this module only */
static struct {
	/* Memory */
	switch_memory_pool_t *pool;

	/* Event hooks */
	switch_event_node_t *node;

	/* Global mutex (don't touch a session when it's already being touched) */
	switch_mutex_t *mutex;

	/* Global billing config options */
	double percall_max_amt;		/* Per-call billing limit (safety check, for fraud) */
	char *percall_action;		/* Exceeded length of per-call action */
	double lowbal_amt;			/* When we warn them they are near depletion */
	char *lowbal_action;		/* Low balance action */
	double nobal_amt;			/* Minimum amount that must remain in the account */
	char *nobal_action;			/* Drop action */

	/* Other options */
	int global_heartbeat;		/* Supervise and bill every X seconds, 0 means off */

	/* Channel variable name options */
	char *var_name_rate;
	char *var_name_account;

	/* Database settings */
	char *dbname;
	char *odbc_dsn;
	char *db_table;
	char *db_column_cash;
	char *db_column_account;
	char *custom_sql_save;
	char *custom_sql_lookup;
	switch_odbc_handle_t *master_odbc;
} globals;

static void nibblebill_pause(switch_core_session_t *session);

/**************************
* Setup FreeSWITCH Macros *
**************************/
/* Define the module's load function */
SWITCH_MODULE_LOAD_FUNCTION(mod_nibblebill_load);

/* Define the module's shutdown function */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_nibblebill_shutdown);

/* Define the module's name, load function, shutdown function and runtime function */
SWITCH_MODULE_DEFINITION(mod_nibblebill, mod_nibblebill_load, mod_nibblebill_shutdown, NULL);

/* String setting functions */
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_db_table, globals.db_table);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_db_column_cash, globals.db_column_cash);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_db_column_account, globals.db_column_account);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_custom_sql_save, globals.custom_sql_save);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_custom_sql_lookup, globals.custom_sql_lookup);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_percall_action, globals.percall_action);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_lowbal_action, globals.lowbal_action);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_nobal_action, globals.nobal_action);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_var_name_rate, globals.var_name_rate);
SWITCH_DECLARE_GLOBAL_STRING_FUNC(set_global_var_name_account, globals.var_name_account);

static switch_cache_db_handle_t *nibblebill_get_db_handle(void)
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

static int nibblebill_callback(void *pArg, int argc, char **argv, char **columnNames)
{
	int i = 0;
	nibblebill_results_t *cbt = (nibblebill_results_t *) pArg;

	for (i = 0; i < argc; i++) {
		if (!strcasecmp(columnNames[i], "nibble_balance")) {
			cbt->balance = atof(argv[0]);
		}
	}
	
	return 0;
}

static switch_bool_t nibblebill_execute_sql_callback(char *sql, switch_core_db_callback_func_t callback, void *pdata)
{
	switch_bool_t retval = SWITCH_FALSE;
	switch_cache_db_handle_t *dbh = NULL;
	
	if (globals.odbc_dsn && (dbh = nibblebill_get_db_handle())) {
		if (switch_cache_db_execute_sql_callback(dbh, sql, callback, pdata, NULL) != SWITCH_STATUS_SUCCESS) {
			retval = SWITCH_FALSE;
		} else {
			retval = SWITCH_TRUE;
		}
	}
	switch_cache_db_release_db_handle(&dbh);
	return retval;
}

static switch_bool_t nibblebill_execute_sql(char *sql)
{
	switch_bool_t retval = SWITCH_FALSE;
	switch_cache_db_handle_t *dbh = NULL;

	if (globals.odbc_dsn && (dbh = nibblebill_get_db_handle())) {

		if ( switch_cache_db_execute_sql(dbh, sql, NULL ) != SWITCH_STATUS_SUCCESS ) {
			retval = SWITCH_FALSE;
		} else {
			retval = SWITCH_TRUE;
		}
	}
	switch_cache_db_release_db_handle(&dbh);

	return retval;
}


static switch_status_t nibblebill_load_config(void)
{
	char *cf = "nibblebill.conf";
	switch_cache_db_handle_t *dbh = NULL;
	switch_xml_t cfg, xml = NULL, param, settings;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (!(xml = switch_xml_open_cfg(cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", cf);
		status = SWITCH_STATUS_SUCCESS;	/* We don't fail because we can still write to a text file or buffer */
		goto setdefaults;
	}

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");
			
			if (!strcasecmp(var, "odbc-dsn") && !zstr(val)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "odbc_dsn is %s\n", val);
				switch_safe_free(globals.odbc_dsn);
				globals.odbc_dsn = strdup(val);
			} else if (!strcasecmp(var, "db_dsn") && !zstr(val)) {
				/* For backwards-compatibility */
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "this nibblebill param(db_dsn) is deprecated and will be removed in 1.4 - odbc_dsn is %s\n", val);
				switch_safe_free(globals.odbc_dsn);
				globals.odbc_dsn = strdup(val);
			} else if (!strcasecmp(var, "db_table")) {
				set_global_db_table(val);
			} else if (!strcasecmp(var, "db_column_cash")) {
				set_global_db_column_cash(val);
			} else if (!strcasecmp(var, "db_column_account")) {
				set_global_db_column_account(val);
			} else if (!strcasecmp(var, "custom_sql_save")) {
				set_global_custom_sql_save(val);
			} else if (!strcasecmp(var, "custom_sql_lookup")) {
				set_global_custom_sql_lookup(val);
			} else if (!strcasecmp(var, "percall_action")) {
				set_global_percall_action(val);
			} else if (!strcasecmp(var, "percall_max_amt")) {
				globals.percall_max_amt = atof(val);
			} else if (!strcasecmp(var, "lowbal_action")) {
				set_global_lowbal_action(val);
			} else if (!strcasecmp(var, "lowbal_amt")) {
				globals.lowbal_amt = atof(val);
			} else if (!strcasecmp(var, "nobal_action")) {
				set_global_nobal_action(val);
			} else if (!strcasecmp(var, "nobal_amt")) {
				globals.nobal_amt = atof(val);
			} else if (!strcasecmp(var, "var_name_rate")) {
				set_global_var_name_rate(val);
			} else if (!strcasecmp(var, "var_name_account")) {
				set_global_var_name_account(val);
			} else if (!strcasecmp(var, "global_heartbeat")) {
				globals.global_heartbeat = atoi(val);
			}
		}
	}
	

/* Set defaults for any variables still not set */
  setdefaults:
	if (zstr(globals.percall_action)) {
		set_global_percall_action("hangup");
	}
	if (zstr(globals.lowbal_action)) {
		set_global_lowbal_action("play ding");
	}
	if (zstr(globals.nobal_action)) {
		set_global_nobal_action("hangup");
	}
	if (zstr(globals.var_name_rate)) {
		set_global_var_name_rate("nibble_rate");
	}
	if (zstr(globals.var_name_account)) {
		set_global_var_name_account("nibble_account");
	}

	if (globals.odbc_dsn) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG
						  , "dsn is \"%s\"\n"
						  , globals.odbc_dsn
						  );
		if (!(dbh = nibblebill_get_db_handle())) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Cannot Open ODBC Database!\n");
			switch_goto_status(SWITCH_STATUS_FALSE, done);
		} else {
			switch_cache_db_release_db_handle(&dbh);
		}
	}

  done:
	if (xml) {
		switch_xml_free(xml);
	}
	return status;
}

void debug_event_handler(switch_event_t *event)
{
	if (!event) {
		return;
	}

	/* Print out all event headers, for fun */
	if (event->headers) {
		switch_event_header_t *event_header = NULL;
		for (event_header = event->headers; event_header; event_header = event_header->next) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Header info: %s => %s\n", event_header->name, event_header->value);
		}
	}
}

static switch_status_t exec_app(switch_core_session_t *session, const char *app_string)
{
	switch_status_t status;
	char *strings[2] = { 0 };
	char *dup;

	if (!app_string) {
		return SWITCH_STATUS_FALSE;
	}

	dup = strdup(app_string);
	switch_assert(dup);
	switch_separate_string(dup, ' ', strings, sizeof(strings) / sizeof(strings[0]));
	status = switch_core_session_execute_application(session, strings[0], strings[1]);
	free(dup);
	return status;
}

static void transfer_call(switch_core_session_t *session, char *destination)
{
	char *argv[4] = { 0 };
	const char *uuid;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char *mydup;

	if (!destination) {
		return;
	}

	mydup = strdup(destination);
	switch_assert(mydup);
	switch_separate_string(mydup, ' ', argv, (sizeof(argv) / sizeof(argv[0])));

	/* Find the uuid of our B leg. If it exists, transfer it first */
	if ((uuid = switch_channel_get_partner_uuid(channel))) {
		switch_core_session_t *b_session;

		/* Get info on the B leg */
		if ((b_session = switch_core_session_locate(uuid))) {
			/* Make sure we are in the media path on B leg */
			switch_ivr_media(uuid, SMF_REBRIDGE);

			/* Transfer the B leg */
			switch_ivr_session_transfer(b_session, argv[0], argv[1], argv[2]);
			switch_core_session_rwunlock(b_session);
		}
	}

	/* Make sure we are in the media path on A leg */
	uuid = switch_core_session_get_uuid(session);
	switch_ivr_media(uuid, SMF_REBRIDGE);

	/* Transfer the A leg */
	switch_ivr_session_transfer(session, argv[0], argv[1], argv[2]);
	free(mydup);
}

/* At this time, billing never succeeds if you don't have a database. */
static switch_bool_t bill_event(double billamount, const char *billaccount, switch_channel_t *channel)
{
	char *sql = NULL, *dsql = NULL;
	switch_bool_t status = SWITCH_FALSE;

	if (globals.custom_sql_save) {
		if (switch_string_var_check_const(globals.custom_sql_save) || switch_string_has_escaped_data(globals.custom_sql_save)) {
			switch_channel_set_variable_printf(channel, "nibble_bill", "%f", billamount, SWITCH_FALSE);
			sql = switch_channel_expand_variables(channel, globals.custom_sql_save);
			if (sql != globals.custom_sql_save) dsql = sql;
		} else {
			sql = globals.custom_sql_save;
		}
	} else {
		sql = dsql = switch_mprintf("UPDATE %s SET %s=%s-%f WHERE %s='%s'", globals.db_table, globals.db_column_cash, 
									globals.db_column_cash, billamount, globals.db_column_account, billaccount);
		
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Doing update query\n[%s]\n", sql);
	status = nibblebill_execute_sql(sql);
	switch_safe_free(dsql);

	return status;
}

static double get_balance(const char *billaccount, switch_channel_t *channel)
{
	char *dsql = NULL, *sql = NULL;
	nibblebill_results_t pdata;
	double balance = 0.0;

	memset(&pdata, 0, sizeof(pdata));

	if (globals.custom_sql_lookup) {
		if (switch_string_var_check_const(globals.custom_sql_lookup) || switch_string_has_escaped_data(globals.custom_sql_lookup)) {
			sql = switch_channel_expand_variables(channel, globals.custom_sql_lookup);
			if (sql != globals.custom_sql_lookup) dsql = sql;
		} else {
			sql = globals.custom_sql_lookup;
		}
	} else {
		sql = dsql = switch_mprintf("SELECT %s AS nibble_balance FROM %s WHERE %s='%s'", 
									globals.db_column_cash, globals.db_table, globals.db_column_account, billaccount);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Doing lookup query\n[%s]\n", sql);
	if (nibblebill_execute_sql_callback(sql, nibblebill_callback, &pdata) != SWITCH_TRUE) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error running this query: [%s]\n", sql);
		/* Return -1 for safety */
		balance = -1.0;
	} else {
		/* Successfully retrieved! */
		balance = pdata.balance;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Retrieved current balance for account %s (balance = %f)\n", billaccount, balance);
	}
	
	switch_safe_free(dsql);
	return balance;
}

/* This is where we actually charge the guy 
  This can be called anytime a call is in progress or at the end of a call before the session is destroyed */
static switch_status_t do_billing(switch_core_session_t *session)
{
	/* FS vars we will use */
	switch_channel_t *channel;
	switch_caller_profile_t *profile;

	/* Local vars */
	nibble_data_t *nibble_data;
	switch_time_t ts = switch_micro_time_now();
	double billamount;
	char date[80] = "";
	char *uuid;
	switch_size_t retsize;
	switch_time_exp_t tm;
	const char *billrate;
	const char *billincrement;
	const char *billaccount;
	double nobal_amt = globals.nobal_amt;
	double lowbal_amt = globals.lowbal_amt;
	double balance;

	if (!session) {
		/* Why are we here? */
		return SWITCH_STATUS_SUCCESS;
	}

	uuid = switch_core_session_get_uuid(session);

	/* Get channel var */
	if (!(channel = switch_core_session_get_channel(session))) {
		return SWITCH_STATUS_SUCCESS;
	}

	/* Variables kept in FS but relevant only to this module */
	billrate = switch_channel_get_variable(channel, globals.var_name_rate);
	billincrement = switch_channel_get_variable(channel, "nibble_increment");
	billaccount = switch_channel_get_variable(channel, globals.var_name_account);

	if (!zstr(switch_channel_get_variable(channel, "nobal_amt"))) {
		nobal_amt = atof(switch_channel_get_variable(channel, "nobal_amt"));
	}
	
	if (!zstr(switch_channel_get_variable(channel, "lowbal_amt"))) {
		lowbal_amt = atof(switch_channel_get_variable(channel, "lowbal_amt"));
	}
	
	/* Return if there's no billing information on this session */
	if (!billrate || !billaccount) {
		return SWITCH_STATUS_SUCCESS;
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Attempting to bill at $%s per minute to account %s\n", billrate,
					  billaccount);

	/* Get caller profile info from channel */
	profile = switch_channel_get_caller_profile(channel);

	if (!profile || !profile->times) {
		/* No caller profile (why would this happen?) */
		return SWITCH_STATUS_SUCCESS;
	}

	if (profile->times->answered < 1) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Not billing %s - call is not in answered state\n", billaccount);

		/* See if this person has enough money left to continue the call */
		balance = get_balance(billaccount, channel);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Comparing %f to hangup balance of %f\n", balance, nobal_amt);
		if (balance <= nobal_amt) {
			/* Not enough money - reroute call to nobal location */
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Balance of %f fell below allowed amount of %f! (Account %s)\n",
							  balance, nobal_amt, billaccount);

			transfer_call(session, globals.nobal_action);
		}

		return SWITCH_STATUS_SUCCESS;
	}

	/* Lock this session's data for this module while we tinker with it */
	if (globals.mutex) {
		switch_mutex_lock(globals.mutex);
	}

	/* Get our nibble data var. This will be NULL if it's our first call here for this session */
	nibble_data = (nibble_data_t *) switch_channel_get_private(channel, "_nibble_data_");

	/* Are we in paused mode? If so, we don't do anything here - go back! */
	if (nibble_data && (nibble_data->pausets > 0)) {
		if (globals.mutex) {
			switch_mutex_unlock(globals.mutex);
		}
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Received heartbeat, but we're paused - ignoring\n");
		return SWITCH_STATUS_SUCCESS;
	}

	/* Have we done any billing on this channel yet? If no, set up vars for doing so */
	if (!nibble_data) {
		nibble_data = switch_core_session_alloc(session, sizeof(*nibble_data));
		memset(nibble_data, 0, sizeof(*nibble_data));

		/* Setup new billing data (based on call answer time, in case this module started late with active calls) */
		nibble_data->lastts = profile->times->answered;	/* Set the initial answer time to match when the call was really answered */
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Beginning new billing on %s\n", uuid);
	}

	switch_time_exp_lt(&tm, nibble_data->lastts);
	switch_strftime_nocheck(date, &retsize, sizeof(date), "%Y-%m-%d %T", &tm);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%d seconds passed since last bill time of %s\n",
					  (int) ((ts - nibble_data->lastts) / 1000000), date);

	if ((ts - nibble_data->lastts) >= 0) {
		/* If billincrement is set we bill by it and not by time elapsed */
		if (!(switch_strlen_zero(billincrement))) {
			switch_time_t chargedunits = (ts - nibble_data->lastts) / 1000000 <= atol(billincrement) ? atol(billincrement) * 1000000 : (switch_time_t)(ceil((ts - nibble_data->lastts) / (atol(billincrement) * 1000000.0))) * atol(billincrement) * 1000000;
			billamount = (atof(billrate) / 1000000 / 60) * chargedunits - nibble_data->bill_adjustments;
			/* Account for the prepaid amount */
			nibble_data->lastts += chargedunits;
		} else {		
			/* Convert billrate into microseconds and multiply by # of microseconds that have passed since last *successful* bill */
			billamount = (atof(billrate) / 1000000 / 60) * ((ts - nibble_data->lastts)) - nibble_data->bill_adjustments;
			/* Update the last time we billed */
			nibble_data->lastts = ts;
		}

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Billing $%f to %s (Call: %s / %f so far)\n", billamount, billaccount,
						  uuid, nibble_data->total);

		/* DO ODBC BILLING HERE and reset counters if it's successful! */
		if (bill_event(billamount, billaccount, channel) == SWITCH_TRUE) {
			/* Increment total cost */
			nibble_data->total += billamount;

			/* Reset manual billing adjustments from pausing */
			nibble_data->bill_adjustments = 0;

			/* Update channel variable with current billing */
			switch_channel_set_variable_printf(channel, "nibble_total_billed", "%f", nibble_data->total);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Failed to log to database!\n");
		}
	} else {
		if (switch_strlen_zero(billincrement))
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Just tried to bill %s negative minutes! That should be impossible.\n", uuid);
	}

	/* Save this location */
	if (channel) {
		switch_channel_set_private(channel, "_nibble_data_", nibble_data);

		/* don't verify balance and transfer to nobal if we're done with call */
		if (switch_channel_get_state(channel) != CS_REPORTING && switch_channel_get_state(channel) != CS_HANGUP) {
			
			balance = get_balance(billaccount, channel);
			
			/* See if we've achieved low balance */
			if (!nibble_data->lowbal_action_executed && balance <= lowbal_amt) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Balance of %f fell below low balance amount of %f! (Account %s)\n",
								  balance, lowbal_amt, billaccount);

				if (exec_app(session, globals.lowbal_action) != SWITCH_STATUS_SUCCESS)
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Low balance action didn't execute\n");
				else
					nibble_data->lowbal_action_executed = 1;
			}

			/* See if this person has enough money left to continue the call */
			if (balance <= nobal_amt) {
				/* Not enough money - reroute call to nobal location */
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Balance of %f fell below allowed amount of %f! (Account %s)\n",
								  balance, nobal_amt, billaccount);

				/* IMPORTANT: Billing must be paused before the transfer occurs! This prevents infinite loops, since the transfer will result */
				/* in nibblebill checking the call again in the routing process for an allowed balance! */
				/* If you intend to give the user the option to re-up their balance, you must clear & resume billing once the balance is updated! */
				nibblebill_pause(session);
				transfer_call(session, globals.nobal_action);
			}
		}
	}

	/* Done changing - release lock */
	if (globals.mutex) {
		switch_mutex_unlock(globals.mutex);
	}

	/* Go check if this call is allowed to continue */
	return SWITCH_STATUS_SUCCESS;
}

/* You can turn on session heartbeat on a channel to have us check billing more often */
static void event_handler(switch_event_t *event)
{
	switch_core_session_t *session;
	char *uuid;

	if (!event) {
		/* We should never get here - it means an event came in without the event info */
		return;
	}

	/* Make sure everything is sane */
	if (!(uuid = switch_event_get_header(event, "Unique-ID"))) {
		/* Donde esta channel? */
		return;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Received request via %s!\n", switch_event_name(event->event_id));

	/* Display debugging info */
	if (switch_event_get_header(event, "nibble_debug")) {
		debug_event_handler(event);
	}

	/* Get session var */
	if (!(session = switch_core_session_locate(uuid))) {
		return;
	}

	/* Go bill */
	do_billing(session);

	switch_core_session_rwunlock(session);
}

static void nibblebill_pause(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_time_t ts = switch_micro_time_now();
	nibble_data_t *nibble_data;

	if (!channel) {
		return;
	}

	/* Lock this session's data for this module while we tinker with it */
	if (globals.mutex) {
		switch_mutex_lock(globals.mutex);
	}

	/* Get our nibble data var. This will be NULL if it's our first call here for this session */
	nibble_data = (nibble_data_t *) switch_channel_get_private(channel, "_nibble_data_");

	if (!nibble_data) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Can't pause - channel is not initialized for billing!\n");
		return;
	}

	/* Set pause counter if not already set */
	if (nibble_data->pausets == 0)
		nibble_data->pausets = ts;

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Paused billing timestamp!\n");

	/* Done checking - release lock */
	if (globals.mutex) {
		switch_mutex_unlock(globals.mutex);
	}
}

static void nibblebill_resume(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_time_t ts = switch_micro_time_now();
	nibble_data_t *nibble_data;
	const char *billrate;

	if (!channel) {
		return;
	}

	/* Get our nibble data var. This will be NULL if it's our first call here for this session */
	nibble_data = (nibble_data_t *) switch_channel_get_private(channel, "_nibble_data_");

	if (!nibble_data) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
						  "Can't resume - channel is not initialized for billing (This is expected at hangup time)!\n");
		return;
	}

	if (nibble_data->pausets == 0) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
						  "Can't resume - channel is not paused! (This is expected at hangup time)\n");
		return;
	}

	/* Lock this session's data for this module while we tinker with it */
	if (globals.mutex) {
		switch_mutex_lock(globals.mutex);
	}

	billrate = switch_channel_get_variable(channel, globals.var_name_rate);

	/* Calculate how much was "lost" to billings during pause - we do this here because you never know when the billrate may change during a call */
	nibble_data->bill_adjustments += (atof(billrate) / 1000000 / 60) * ((ts - nibble_data->pausets));
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Resumed billing! Subtracted %f from this billing cycle.\n",
					  (atof(billrate) / 1000000 / 60) * ((ts - nibble_data->pausets)));

	nibble_data->pausets = 0;

	/* Done checking - release lock */
	if (globals.mutex) {
		switch_mutex_unlock(globals.mutex);
	}
}

static void nibblebill_reset(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_time_t ts = switch_micro_time_now();
	nibble_data_t *nibble_data;

	if (!channel) {
		return;
	}

	/* Get our nibble data var. This will be NULL if it's our first call here for this session */
	nibble_data = (nibble_data_t *) switch_channel_get_private(channel, "_nibble_data_");

	if (!nibble_data) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Can't reset - channel is not initialized for billing!\n");
		return;
	}

	/* Lock this session's data for this module while we tinker with it */
	if (globals.mutex) {
		switch_mutex_lock(globals.mutex);
	}

	/* Update the last time we billed */
	nibble_data->lastts = ts;

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Reset last billing timestamp marker to right now!\n");

	/* Done checking - release lock */
	if (globals.mutex) {
		switch_mutex_unlock(globals.mutex);
	}
}

static double nibblebill_check(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	nibble_data_t *nibble_data;
	double amount = 0;

	if (!channel) {
		return -99999;
	}

	/* Get our nibble data var. This will be NULL if it's our first call here for this session */
	nibble_data = (nibble_data_t *) switch_channel_get_private(channel, "_nibble_data_");

	if (!nibble_data) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Can't check - channel is not initialized for billing!\n");
		return -99999;
	}

	/* Lock this session's data for this module while we tinker with it */
	if (globals.mutex) {
		switch_mutex_lock(globals.mutex);
	}

	amount = nibble_data->total;

	/* Done checking - release lock */
	if (globals.mutex) {
		switch_mutex_unlock(globals.mutex);
	}

	return amount;
}

static void nibblebill_adjust(switch_core_session_t *session, double amount)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	const char *billaccount;

	if (!channel) {
		return;
	}

	/* Variables kept in FS but relevant only to this module */

	billaccount = switch_channel_get_variable(channel, globals.var_name_account);

	/* Return if there's no billing information on this session */
	if (!billaccount) {
		return;
	}

	/* Add or remove amount from adjusted billing here. Note, we bill the OPPOSITE */
	if (bill_event(-amount, billaccount, channel) == SWITCH_TRUE) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Recorded adjustment to %s for $%f\n", billaccount, amount);
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failed to record adjustment to %s for $%f\n", billaccount, amount);
	}
}

#define APP_SYNTAX "pause | resume | reset | adjust <amount> | heartbeat <seconds> | check"
SWITCH_STANDARD_APP(nibblebill_app_function)
{
	int argc = 0;
	char *lbuf = NULL;
	char *argv[3] = { 0 };

	if (!zstr(data) && (lbuf = strdup(data))
		&& (argc = switch_separate_string(lbuf, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
		if (!strcasecmp(argv[0], "adjust") && argc == 2) {
			nibblebill_adjust(session, atof(argv[1]));
		} else if (!strcasecmp(argv[0], "flush")) {
			do_billing(session);
		} else if (!strcasecmp(argv[0], "pause")) {
			nibblebill_pause(session);
		} else if (!strcasecmp(argv[0], "resume")) {
			nibblebill_resume(session);
		} else if (!strcasecmp(argv[0], "check")) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Current billing is at $%f\n", nibblebill_check(session));
		} else if (!strcasecmp(argv[0], "reset")) {
			nibblebill_reset(session);
		} else if (!strcasecmp(argv[0], "heartbeat") && argc == 2) {
			switch_core_session_enable_heartbeat(session, atoi(argv[1]));
		}
	}
	switch_safe_free(lbuf);
}

/* We get here from the API only (theoretically) */
#define API_SYNTAX "<uuid> [pause | resume | reset | adjust <amount> | heartbeat <seconds> | check]"
SWITCH_STANDARD_API(nibblebill_api_function)
{
	switch_core_session_t *psession = NULL;
	char *mycmd = NULL, *argv[3] = { 0 };
	int argc = 0;

	if (!zstr(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
		if ((argc == 2 || argc == 3) && !zstr(argv[0])) {
			char *uuid = argv[0];
			if ((psession = switch_core_session_locate(uuid))) {
				if (!strcasecmp(argv[1], "adjust") && argc == 3) {
					nibblebill_adjust(psession, atof(argv[2]));
				} else if (!strcasecmp(argv[1], "flush")) {
					do_billing(psession);
				} else if (!strcasecmp(argv[1], "pause")) {
					nibblebill_pause(psession);
				} else if (!strcasecmp(argv[1], "resume")) {
					nibblebill_resume(psession);
				} else if (!strcasecmp(argv[1], "check")) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Current billing is at $%f\n", nibblebill_check(psession));
				} else if (!strcasecmp(argv[1], "reset")) {
					nibblebill_reset(psession);
				} else if (!strcasecmp(argv[1], "heartbeat") && argc == 3) {
					switch_core_session_enable_heartbeat(psession, atoi(argv[2]));
				}

				switch_core_session_rwunlock(psession);
			} else {
				stream->write_function(stream, "-ERR No Such Channel!\n");
			}
		} else {
			stream->write_function(stream, "-USAGE: %s\n", API_SYNTAX);
		}
	}
	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}

/* Check if session has variable "billrate" set. If it does, activate the heartbeat variable
 switch_core_session_enable_heartbeat(switch_core_session_t *session, uint32_t seconds)
 switch_core_session_sched_heartbeat(switch_core_session_t *session, uint32_t seconds)*/

static switch_status_t sched_billing(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	
	const char *billrate = NULL;
	const char *billaccount = NULL;
	
	if (!(channel = switch_core_session_get_channel(session))) {
		return SWITCH_STATUS_SUCCESS;
	}

	/* Variables kept in FS but relevant only to this module */
	billrate = switch_channel_get_variable(channel, globals.var_name_rate);
	billaccount = switch_channel_get_variable(channel, globals.var_name_account);

	/* Return if there's no billing information on this session */
	if (!billrate || !billaccount) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (globals.global_heartbeat > 0) {
		switch_core_session_enable_heartbeat(session, globals.global_heartbeat);
	}

	/* TODO: Check account balance here */

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t process_hangup(switch_core_session_t *session)
{
	const char* billaccount;
	switch_channel_t *channel = NULL;

	channel = switch_core_session_get_channel(session);
	
	/* Resume any paused billings, just in case */
	/*  nibblebill_resume(session); */

	/* Now go handle like normal billing */
	do_billing(session);

	billaccount = switch_channel_get_variable(channel, globals.var_name_account);
	if (billaccount) {
		switch_channel_set_variable_printf(channel, "nibble_current_balance", "%f", get_balance(billaccount, channel));
	}			
	
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t process_and_sched(switch_core_session_t *session) {
	process_hangup(session);
	sched_billing(session);
	return SWITCH_STATUS_SUCCESS;
}

switch_state_handler_table_t nibble_state_handler = {
	/* on_init */ NULL,
	/* on_routing */ process_hangup, 	/* Need to add a check here for anything in their account before routing */
	/* on_execute */ sched_billing, 	/* Turn on heartbeat for this session and do an initial account check */
	/* on_hangup */ process_hangup, 	/* On hangup - most important place to go bill */
	/* on_exch_media */ process_and_sched,
	/* on_soft_exec */ NULL,
	/* on_consume_med */ process_and_sched,
	/* on_hibernate */ NULL,
	/* on_reset */ NULL,
	/* on_park */ NULL,
	/* on_reporting */ NULL, 
	/* on_destroy */ NULL
};

SWITCH_MODULE_LOAD_FUNCTION(mod_nibblebill_load)
{
	switch_api_interface_t *api_interface;
	switch_application_interface_t *app_interface;

	/* Set every byte in this structure to 0 */
	memset(&globals, 0, sizeof(globals));
	globals.pool = pool;
	switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, globals.pool);

	nibblebill_load_config();

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	/* Add API and CLI commands */
	SWITCH_ADD_API(api_interface, "nibblebill", "Manage billing parameters for a channel/call", nibblebill_api_function, API_SYNTAX);

	/* Add dialplan applications */
	SWITCH_ADD_APP(app_interface, "nibblebill", "Handle billing for the current channel/call",
				   "Pause, resume, reset, adjust, flush, heartbeat commands to handle billing.", nibblebill_app_function, APP_SYNTAX,
				   SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);

	/* register state handlers for billing */
	switch_core_add_state_handler(&nibble_state_handler);

	/* bind to heartbeat events */
	if (switch_event_bind_removable(modname, SWITCH_EVENT_SESSION_HEARTBEAT, SWITCH_EVENT_SUBCLASS_ANY, event_handler, NULL, &globals.node) !=
		SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind event to monitor for session heartbeats!\n");
		return SWITCH_STATUS_GENERR;
	}

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_nibblebill_shutdown)
{
	switch_event_unbind(&globals.node);
	switch_core_remove_state_handler(&nibble_state_handler);
	switch_odbc_handle_disconnect(globals.master_odbc);
	
	switch_safe_free(globals.dbname);
	switch_safe_free(globals.odbc_dsn);
	switch_safe_free(globals.db_table);
	switch_safe_free(globals.db_column_cash);
	switch_safe_free(globals.db_column_account);
	switch_safe_free(globals.custom_sql_save);
	switch_safe_free(globals.custom_sql_lookup);
	switch_safe_free(globals.percall_action);
	switch_safe_free(globals.lowbal_action);
	switch_safe_free(globals.nobal_action);
	switch_safe_free(globals.var_name_rate);
	switch_safe_free(globals.var_name_account);

	return SWITCH_STATUS_UNLOAD;
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

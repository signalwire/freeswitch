/**
 * 
 * The Initial Developer of the Original Code is
 * Amaze Telecom <suppor@amazetelecom.com> 
 * 
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Amaze Telecom <suppor@amazetelecom.com>
 * 
 * mod_dialpeer.c  Dialpeer module.
 * 
 */

//-------------------------------------------------------------------------------------//
//  To include Header Guard
//-------------------------------------------------------------------------------------//

#ifndef DIALPEER_HEADER
#define DIALPEER_HEADER

//-------------------------------------------------------------------------------------//
//  Include Header Files.
//-------------------------------------------------------------------------------------//

#include <switch.h> 

//-------------------------------------------------------------------------------------//
//  Function Prototype
//-------------------------------------------------------------------------------------//

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_dialpeer_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_dialpeer_runtime);
SWITCH_MODULE_LOAD_FUNCTION(mod_dialpeer_load);

/* SWITCH_MODULE_DEFINITION(name, load, shutdown, runtime) 
 * Defines a switch_loadable_module_function_table_t and a static const char[] modname
 */
SWITCH_MODULE_DEFINITION(mod_dialpeer, mod_dialpeer_load, mod_dialpeer_shutdown, NULL);

/**
 * @var Loading module banner message display on console.
 */

char banner[] = "\n"
"/**\n"
" * \n"
" * The Initial Developer of the Original Code is\n"
" * Amaze Telecom <suppor@amazetelecom.com>.\n"
" * \n"
" * Portions created by the Initial Developer are Copyright (C)\n"
" * the Initial Developer. All Rights Reserved.\n"
" * \n"
" * Contributor(s):\n"
" * \n"
" * Amaze Telecom <suppor@amazetelecom.com>\n"
" * \n"
" * mod_dialpeer.c  Dialpeer module.\n"
" */\n";

static char *line = "++++++++++++++++++++++++++++++++++++++++++++++++++++++++";

/**
 * @var Declared Global variables
 */

#ifndef  	SWITCH_DIALPEER_SUCCESS
#define  	SWITCH_DIALPEER_SUCCESS            0         /*! ON SUCCESS */
#define  	SWITCH_DIALPEER_FAILED            -1         /*! ON FAILED */
#define  	SWITCH_DIALPEER_SQLITE_DBNAME     "amazeswitch"   /*! ON DBNAME */  
#endif

/**
 * @var dialpeer Application Configuration file name
 */

static const char *global_cf = "amazeswitch.conf";        /*! COnfig File Name */

/**
 * @struct Stored configuration in structure variables.
 */

static struct {
	char *odbc_dsn;                           /*! Dialpeer application odbc-dsn name */
	char *dbname;                             /*! Dialpeer application dbname */
	char *profile;                            /*! Dialpeer application hiredis profile name */
	switch_mutex_t *mutex;                    /*! Dialpeer mutex object */
	switch_memory_pool_t *pool;               /*! Dialpeer memory pool object */
} globals;

/**
 * @struct dialpeer handler structure
 */

struct switch_dial_peer_handler
{
	switch_core_session_t *session;             /*! Switch channel session */
	char *dp_id;                                /*! Dialpeer ID */
	char *dp_balancing_method;                  /*! Dialpeer Balancing Method */
	char *source_number;                        /*! Dialpeer Source Number */
	char *destination_number;                   /*! Dialpeer Destination Number */
	char *source_billing_number;                /*! Dialpeer Billing Source Number */
	char *destination_billing_number;           /*! Dialpeer Billing Destination Number */
	char *sip_equipment_type;                   /*! Dialpeer SIP Equipment Type */     
	char *sip_equipment_ids;                    /*! Dialpeer SIP Redirect Equipment ID */  
	char *termination_equipment_ids;            /*! Dialpeer SIP Termination Equipment ID */
	char *sip_term_destination_number;          /*! Dialpeer Sending Destination Number To termination application */
	char *sip_term_source_number;               /*! Dialpeer Sending Source Number To termination application */
	int flag;
};
typedef struct switch_dial_peer_handler switch_dial_peer_handler_st;

/**
 * @Struct dialpeer REGEX information structure
 */

struct regex_master
{
	char *allow_source;                           /*! allow source number regex string */
	char *disallow_source;                        /*! disallow source number regex string */
	char *allow_destination;                      /*! allow destination number regex string */
	char *disallow_destination;                   /*! disallow destination number regex string */
	char *change_source;                          /*! Source Number Manipulation Regex String*/
	char *change_destination;                     /*! Destination Number manipulation Regex String */
	char *change_bill_source;                     /*! Change Source Billing Number Regex String*/ 
	char *change_bill_destination;                /*! Change Destination Billing Number Regex String */
};
typedef struct regex_master regex_master_st;

/**
 * @function dialpeer application prototype
 */

SWITCH_STANDARD_APP(switch_dialpeer_app);            /*! dialpeer application declaration */

char* switch_strip_quotes(const char *in)
{
	char *t = (char *) in;
	char *r = (char *) in;
	
	if (t && *t == '"') {
		t++;
		r = strchr(t,'"');
		(*r) = '\0';
	}

	return t;
}

void switch_replace_character(char *str, char ochar, char rchar) 
{
    if (str) {
	char *p = str;
        for (; *str; ++str) {
		if (((*str)== ochar)) {
			*p++ = rchar;
		} else {
			*p++ = *str;
		}
        }
        *p = '\0';
    }
}

int switch_check_resource_limit(char *backend, switch_core_session_t *session, char *realm, char *id, int max , int interval, char *xfer_exten) 
{
    int ret = 0;

    switch_channel_t *channel = switch_core_session_get_channel(session);

	if (switch_limit_incr(backend, session,  realm, id, max, interval) != SWITCH_STATUS_SUCCESS) {
	    /* Limit exceeded */
	    if (*xfer_exten == '!') {
		    switch_channel_hangup(channel, switch_channel_str2cause(xfer_exten + 1));
	    } else {
 		    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-DP] :  Your resource limit is exceeded...!!!\n");
// 		    switch_channel_set_variable(channel, "transfer_string", "cg_cps_exceeded XML CG_CPS_EXCEEDED");
		    ret = -1;
		    goto end;
	    }
    }
    
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[AMAZE-DP] :  ********* LIMIT SET SUCCESSFULLY *********\n");
    
    end:
	return ret;	    
}


char *switch_disallow_regex(switch_core_session_t *session, switch_channel_t *channel, char *regex_str, char *source) 
{
	char *status = NULL;
	if(!zstr(regex_str)) {
		char *api_cmd = NULL;
		char *api_result = NULL;
		
		api_cmd = switch_mprintf("api_result=${regex(%s|%s}", source, regex_str);
		switch_core_session_execute_application(session, "set", api_cmd);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-DP] :  api_cmd : \n%s\n", api_cmd);
		switch_safe_free(api_cmd);

		api_result = (char *)switch_channel_get_variable(channel, "api_result");
		if(!zstr(api_result)) {
			char *switch_result = NULL;
			switch_result = switch_mprintf("%s", api_result);
			if(!strcmp(switch_result, "true")) {
			    
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-DP] :  Routing is Disallow on this number.\n");
				status = switch_mprintf("true");
				switch_safe_free(switch_result);
				switch_channel_set_variable(channel, "transfer_string", "REGEX_DISALLOW_NUMBER XML REGEX_DISALLOW_NUMBER");
				goto end;
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] :  [ %s ] is passed to regex string [ %s ].\n", source, regex_str);
				status = switch_mprintf("false");
			}
			switch_safe_free(switch_result);
			switch_channel_set_variable(channel, "api_result", NULL);
		}
	}
end :	
	return status;    
}

char *switch_allow_regex(switch_core_session_t *session, switch_channel_t *channel, char *regex_str, char *source) 
{
	char *status = NULL;
	if(!zstr(regex_str)) {
		char *api_cmd = NULL;
		char *api_result = NULL;
		
		api_cmd = switch_mprintf("api_result=${regex(%s|%s}", source, regex_str);
		switch_core_session_execute_application(session, "set", api_cmd);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-DP] :  api_cmd : %s\n",api_cmd);
		switch_safe_free(api_cmd);

		api_result = (char *)switch_channel_get_variable(channel, "api_result");

		if(!zstr(api_result)) {
			char *switch_result = NULL;
			
			switch_result = switch_mprintf("%s", api_result);
			if(!strcmp(switch_result, "false")) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-DP] :  validation false.\n");
				status = switch_mprintf("false");
				switch_safe_free(switch_result);
				switch_channel_set_variable(channel, "transfer_string", "DISALLOW_SOUCE XML DISALLOW_SOUCE");
				goto end;
			} else {
				status = switch_mprintf("true");
			}
			switch_safe_free(switch_result);
			switch_channel_set_variable(channel, "api_result", NULL);
		}
	}
end :	
	return status;    
}


char *switch_regex_manipulation(switch_core_session_t *session, switch_channel_t *channel, char *regex_str, char *source ) 
{
	if(!zstr(regex_str)) {
		char *string = NULL;
		char *token = NULL;
		const char *seprator = "|";
    	char *api_cmd = NULL;
		
		string = switch_mprintf("%s", regex_str);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-DP] :  regex_str : %s\n",regex_str);
		
		token = strtok(string, seprator);
		while( token != NULL) {
			char *ptr = NULL;
			char *api_result = NULL;
			
			ptr = switch_mprintf("%s", token);
			switch_replace_character(ptr, '/', '|');
			switch_replace_character(ptr, '\\', '%'); //replace $1 by %1
			ptr[strlen(ptr)-1]  = '\0'; //to remove $ from string end
			
			api_cmd = switch_mprintf("api_result=${regex(%s|%s}", source, ptr);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-DP] :  api_cmd : %s\n", api_cmd);
			switch_core_session_execute_application(session, "set", api_cmd);
			api_result = (char *)switch_channel_get_variable(channel, "api_result");
			
			if(!zstr(api_result)) {
				char *switch_result = NULL;
				switch_result = switch_mprintf("%s", api_result);
				source = strdup(switch_result);
				
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-DP] :  api_result : %s\n", api_result);
				switch_safe_free(switch_result);
				switch_channel_set_variable(channel, "api_result", NULL);
			}
			
			token = strtok(NULL, seprator);
			switch_safe_free(ptr);
			switch_safe_free(api_cmd);    
		}
	}
	return source;    
}


/**
 * @Function loading a configuartion from configuration file.
 * @return status ---> SWITCH_STATUS_SUCCESS on success and SWITCH_STATUS_TERM on failed.
 */

static switch_status_t load_config(void)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;     /*! Loading Config Status */
	switch_xml_t cfg, xml, settings, param;             /*! Config Params */

	/**
	 * @Section Open Configuration File To Read configuartion
	 */

	if (!(xml = switch_xml_open_cfg(global_cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[ AMAZE-DP ] : Open of %s failed\n", global_cf);
		status = SWITCH_STATUS_TERM;
		goto end;
	}

	/**
	 * @Section Load Configuartion into Global Structure
	 */

	switch_mutex_lock(globals.mutex);
	if ((settings = switch_xml_child(cfg, "settings"))) {
	  
		/**
		 * @Section Load config param into variable
		 */
		
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			// Load dbname in variable
			if (!strcasecmp(var, "dbname")) {                  /*! odbc DBName */
				globals.dbname = strdup(val);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-DP] : dbname [ %s ]\n", globals.dbname);
			} else if (!strcasecmp(var, "odbc-dsn")) {         /*! odbc dsn */
				globals.odbc_dsn = strdup(val);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-DP] : odbc-dsn [ %s ]\n", globals.odbc_dsn);
			} else if (!strcasecmp(var, "redis-profile")) {    /*! Redix Profile */   
				globals.profile = strdup(val);
				switch_log_printf(SWITCH_CHANNEL_LOG,SWITCH_LOG_DEBUG5, "[AMAZE-DP] : redis-profile [ %s ]\n", globals.profile);
			} 
		}
	}
	
	/**
	 * @var Default Database name
	 */
	
	if (!globals.dbname) {
		//use default db name if not specify in configure
		globals.dbname = strdup(SWITCH_DIALPEER_SQLITE_DBNAME);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-DP] : set default DB [ %s ]\n", globals.dbname);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-DP] : Loaded Configuration successfully\n");

	end:
	switch_mutex_unlock(globals.mutex);

	//free memory
	if (xml) {
		switch_xml_free(xml);
	}

	return status;
}

/**
 * @Function Get db handler from configuartion and connect db mysql. 
 * @return DB Handler.
 */

static switch_cache_db_handle_t *switch_get_db_handler(void)
{
	switch_cache_db_handle_t *dbh = NULL;        /*! MySQL odbc DSN Handler */
	char *dsn;

	/**
	 * @Section Load DSN and from config
	 */
	
	if (!zstr(globals.odbc_dsn)) {
		dsn = globals.odbc_dsn;
	} else {
		dsn = globals.dbname;
	}

	/**
	 * @Function get odbc dsn db handler
	 */
	
	if (switch_cache_db_get_db_handle_dsn(&dbh, dsn) != SWITCH_STATUS_SUCCESS) {
		dbh = NULL;
	}

	return dbh;
}

/**
 * @Function Execute SQL with Callback Function 
 * @Return sql raw wise result 
 */

static switch_bool_t switch_execute_sql_callback(switch_mutex_t *mutex, char *sql, switch_core_db_callback_func_t callback, void *pdata)
{
	switch_bool_t ret = SWITCH_FALSE;            /*! MySQL Query Status */
	char *errmsg = NULL;                         /*! MySQL Query ERROR Message */  
	switch_cache_db_handle_t *dbh = NULL;        /*! MySQL odbc DSN Handler */

	/**
	 * @Section Process Synchronisation
	 */
	
	if (mutex) {
		switch_mutex_lock(mutex);
	} else {
		switch_mutex_lock(globals.mutex);
	}

	/**
	 * @Function get db handler dsn object
	 */
	
	if (!(dbh = switch_get_db_handler())) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-DP] : Error Opening DB\n");
		goto end;
	}

	/**
	 * @Function Execute SQL in MYSQL and get result row wise in callback function
	 */
	
	switch_cache_db_execute_sql_callback(dbh, sql, callback, pdata, &errmsg);

	if (errmsg) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-DP] : SQL ERR: [%s] %s\n", sql, errmsg);
		free(errmsg);
	}

	end:

	/**
	 * @Function release dsn handler 
	 */
	
	switch_cache_db_release_db_handle(&dbh);

	/**
	 * @Section Process Synchronisation
	 */
	
	if (mutex) {
		switch_mutex_unlock(mutex);
	} else {
		switch_mutex_unlock(globals.mutex);
	}

	return ret;
}
// /*
// static switch_status_t switch_execute_sql(char *sql, switch_mutex_t *mutex)
// {
// 	switch_cache_db_handle_t *dbh = NULL;            /*! odbc dsn Handler */
// 	switch_status_t status = SWITCH_STATUS_FALSE;    /*! SQL Execution Status */
// 
// 	/**
// 	 * @Section Process Synchronisation
// 	 */
// 	
// 	if (mutex) {
// 		switch_mutex_lock(mutex);
// 	} else {
// 		switch_mutex_lock(globals.mutex);
// 	}
// 
// 	/**
// 	 * @Function get db handler dsn object
// 	 */
// 	
// 	if (!(dbh = switch_get_db_handler())) {
// 		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-DP] : Error Opening DB\n");
// 		goto end;
// 	}
// 
// 	
// 	/**
// 	 * @function Execute SQL in MySQL and get result.
// 	 */
// 	
// 	status = switch_cache_db_execute_sql(dbh, sql, NULL);
// 
// 	end:
// 
// 	/**
// 	 * @function release db handler.
// 	 */
// 
// 	switch_cache_db_release_db_handle(&dbh);
// 
// 	/**
// 	 * @Section Process Synchronisation.
// 	 */
// 	
// 	if (mutex) {
// 		switch_mutex_unlock(mutex);
// 	} else {
// 		switch_mutex_unlock(globals.mutex);
// 	}
// 
// 	return status;
// }*/

/**
 * @Function Execute MySQL Query and get result in string
 * @return resulting string
 */

static char *switch_execute_sql2str(switch_mutex_t *mutex, char *sql, char *resbuf, size_t len)
{
	char *ret = NULL;                         /*! MySQL Query Result */
	switch_cache_db_handle_t *dbh = NULL;     /*! MySQL odbc DSN Handler */

	/**
	 * @Section Process Synchronisation
	 */
	
	if (mutex) {
		switch_mutex_lock(mutex);
	} else {
		switch_mutex_lock(globals.mutex);
	}

	/**
	 * @Function get db handler dsn object
	 */
	
	if (!(dbh = switch_get_db_handler())) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-DP] : Error Opening DB\n");
		goto end;
	}
	
	/**
	 * @function Execute SQL in MySQL and get result.
	 */

	ret = switch_cache_db_execute_sql2str(dbh, sql, resbuf, len, NULL);

	end:
	
	/**
	 * @function release db handler.
	 */
	
	switch_cache_db_release_db_handle(&dbh);

	/**
	 * @Section Process Synchronisation.
	 */
	
	if (mutex) {
		switch_mutex_unlock(mutex);
	} else {
		switch_mutex_unlock(globals.mutex);
	}

	return ret;
}

/**
 * @function dialpeer source, destination numbers validation and manipulation regex strings.
 */

static int switch_regex_callback(void *ptr, int argc, char **argv, char **col) 
{
	regex_master_st *oe = (regex_master_st *)ptr;
	int index = 0;

	/**
	 * @Section Log on console  
	 */
		
	for(index = 0; index < argc ; index++) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-DP] : %s : %s\n", col[index], argv[index]);
	}

	/**
	 * @var store a regex string in structure variable.
	 */
	
	// Source Number Allow Regex String
	if(!strcmp(argv[0], "ALLOW_SOURCE")) {                          /*! Copy Allow Source Number Regex String in Structure */
		oe->allow_source = strdup(argv[1]);
	} else if(!strcmp(argv[0], "DISALLOW_SOURCE")) {                /*! Copy Disallow Source Number Regex String in Structure */ 
		oe->disallow_source = strdup(argv[1]);
	} else if(!strcmp(argv[0], "ALLOW_DESTINATION")) {              /*! Copy Allow Destination Number Regex String in Structure */
		oe->allow_destination = strdup(argv[1]);
	} else if(!strcmp(argv[0], "DISALLOW_DESTINATION")) {           /*! Copy Disallow Source Number Regex String in Structure */
		oe->disallow_destination = strdup(argv[1]);
	} else if(!strcmp(argv[0], "CHANGE_SOURCE")) {                  /*! Copy Change Source Number Regex String in Structure */
		oe->change_source = strdup(argv[1]);
	} else if(!strcmp(argv[0], "CHANGE_DESTINATION")) {             /*! Copy Change Destination Number Regex String in Structure */
		oe->change_destination = strdup(argv[1]);
	} else if(!strcmp(argv[0], "CHANGE_BILL_SOURCE")) {             /*! Copy Change Billing Source Number Regex String in Structure */
		oe->change_bill_source = strdup(argv[1]);
	}else if(!strcmp(argv[0], "CHANGE_BILL_DESTINATION")) {         /*! Copy Change Biiling Destination Number Regex String in Structure */  
		oe->change_bill_destination = strdup(argv[1]);
	}

	return SWITCH_DIALPEER_SUCCESS;
}

/**
 * @function getting dialpeer information from db and store in structure
 */

static int dialpeer_callback_function(void *ptr, int argc, char **argv, char **col) 
{
	switch_dial_peer_handler_st *dp = (switch_dial_peer_handler_st *)ptr;
	switch_core_session_t *session = dp->session;

	/**
	 * @var dialpeer variables declaration.
	 */
	
	int index = 0;                                                               /*! Count value */
	int dp_max_cps = atoi(argv[1]);                                              /*! Dialpeer Max CPS */
	int dp_max_calls = atoi(argv[2]);                                            /*! Dialpeer Max Calls */
// 	int dp_cgp_mc = atoi(argv[5]);                                               /*! Dialpeer Capacity Group Max CPS */
// 	int dp_cgp_cps = atoi(argv[6]);                                              /*! Dialpeer Capacity Group Max Call */
	char *dp_id = argv[0];                                                       /*! Dialpeer ID */
	char *dp_cgp_id = argv[4];                                                   /*! Dialpeer Capacity Group ID */
	char *dp_route_hunt = strdup(argv[6]);                                       /*! Dialpeer Route Hant status */
	char *sql = NULL;                                                            /*! Dialpeer SQL */
	char *regex_status = NULL;                                                   /*! Dialpeer Regex Status */
	char *source_number = strdup(dp->source_number);                             /*! Dialpeer Source Number */
	char *destination_number = strdup(dp->destination_number);                   /*! Dialpeer Destination Number */
	char *source_billing_number = strdup(dp->source_billing_number);             /*! Dialpeer Source Billing Number */
	char *destination_billing_number = strdup(dp->destination_billing_number);   /*! Dialpeer Destination Billing Number */
	char *capacity_group_mc_idname = NULL;                                       /*! Dialpeer Capacity Group MAX Call Key */
	char *dp_equip_type = NULL;                                                  /*! Dialpeer SIP Equipment Type */
	char result[50] = "";                                                        /*! Dialpeer SQL Result */  
	char *tmp_num = NULL;                                                        /*! Dialpeer Temp variable */ 
	regex_master_st regex_master;                                                /*! Dialpeer regex information structure variable */ 
	int dp_cgp_mc = 0;                                                           /*! Dialpeer Capacity Group Max CPS */
	int dp_cgp_cps = 0;                                                          /*! Dialpeer Capacity Group Max Call */

	/**
	 * @var Getting channel from session object.
	 */
	
	switch_channel_t *channel = switch_core_session_get_channel(session);
	
	// dial peer id 
	dp->dp_id = strdup(argv[0]);
	
	// dial peer balancing method
	dp->dp_balancing_method = strdup(argv[3]);
	dp->flag = 0; 
	
	
	if(!zstr(argv[5]) && atoi(argv[5]) > 0) {
		char msgbuf[10] = "";
		char *switchargv[3] = {0};                                          /* string tokenizing */
	  
		sql = switch_mprintf("SELECT concat(cpg_max_calls,',',cpg_max_calls_sec) as capacity_gp_data FROM  vca_capacity_group WHERE cpg_id = '%s' AND cpg_status='Y'",argv[5]);
		
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-DP] : Proceed To Get DIALPEER Equipment Capacity Group Data SQL :: \n%s\n",sql);
		switch_execute_sql2str(NULL, sql, msgbuf, sizeof(msgbuf));
		switch_safe_free(sql);
		
		if(zstr(msgbuf)) {
			dp_cgp_cps = 0;
			dp_cgp_mc = 0;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : Proceed To Get DIALPEER Equipment Capacity Group Data  [ %s ]\n",msgbuf);
			switch_separate_string(msgbuf, ',', switchargv, (sizeof(switchargv) / sizeof(switchargv[0])));
			dp_cgp_cps = atoi(switchargv[0]);           /*! DIALPEER EQUIPMENT CAPACITY GROUP MAX CALL */
			dp_cgp_mc = atoi(switchargv[1]);            /*! DIALPEER EQUIPMENT CAPACITY GROUP MAX CPS */
		}
	  
	} else {
		dp_cgp_cps = 0;
		dp_cgp_mc = 0;
	}
	
	
	
	/**
	 * @Section Setting channel variables for CDR report.
	 */
	
	switch_channel_set_variable(channel, "vox_dp_id", dp->dp_id);
	switch_channel_export_variable(channel, "vox_dp_id",dp->dp_id, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);

	/**
	 * @Section session validation.
	 */
	
	if(!session && zstr(dp_route_hunt)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-DP] : Hey... I am unable to get session...:)!!\n");
		return SWITCH_DIALPEER_SUCCESS;
	}
	
	/**
	 * @Section Query <CALLING STORE PROCEDURE TO Validate DIALPEER SCHEDULER >
	 */
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-DP] : proceed to validate DIALPEER [ %s ] in schedule time.\n", argv[0]);
	sql = switch_mprintf("CALL `GetDialPeerStatus`('%s');", argv[0]);
	switch_execute_sql2str(NULL, sql, result, sizeof(result));
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-DP] : Check DIALPEER Schedule Time SQL : \n%s\n", sql);
	switch_safe_free(sql);

	/**
	 * @Section Checking <dialpeer> is in cuurent schedule or not ? 
	 */
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : DIALPEER [ %s ] Schedule Status is [ %s ].\n", argv[0], result);
	if(zstr(result) || !strcmp(result, "FAIL")) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[AMAZE-DP] : DIALPEER [ %s ] is not in schedule time so skip it.\n", argv[0]);
		
		/**
		 * @Section Checking DP routing hunt is set or not --> <Y> means get another dialpeer and  <N> means Do not check another DP.
		 */
		
		if(!strcasecmp(dp_route_hunt,"Y") ) {
			return SWITCH_DIALPEER_FAILED;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : DIALPEER application checking for another dialpeer is there\n");
			return SWITCH_DIALPEER_SUCCESS;
		}
	}

	/**
	 * @Section Checking DP EQUIPMENT TYPE is valide --> <SIP Termination> and  <SIP Redirect>
	 */
	
	if(zstr(argv[5])) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-DP] : DIALPEER Termination Type is not set, So system will not allow this dial peer\n");
		return SWITCH_DIALPEER_SUCCESS;
	}
	
	//copy equipment type
	dp_equip_type = strdup(argv[5]);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-DP] : DIALPEER [ %s ] has Termination Equipment Type is [ %s ]\n", dp->dp_id, dp_equip_type);

	/**
	 * @Section Log on console <dialpeer information>
	 */
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-DP] : %s\n", line);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-DP] : ============ DIALPEER INFORMATION ============\n");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-DP] : %s\n", line);
	
	for(index = 0; index < argc ; index++) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-DP] : %s : %s\n", col[index], argv[index]);
	}

	/**
	 * @var dialpeer <regex structure variable>
	 */
	
	memset(&regex_master, 0, sizeof(regex_master));	

	/**
	 * @Query Getting Dialpeer <Regex strings> from MySQL Database. 
	 */
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-DP] : proceed To Get DIALPEER [ %s ] regex information.\n", dp->dp_id);
	sql = switch_mprintf("SELECT regex_type,IF((regex_type='CHANGE_DESTINATION' OR regex_type='CHANGE_SOURCE' OR regex_type='CHANGE_BILL_SOURCE' OR  regex_type='CHANGE_BILL_DESTINATION'), group_concat(regex_string SEPARATOR '|'), group_concat(regex_string SEPARATOR '\\\\|')) as regex_string FROM `vca_regex_master` WHERE regex_equip_id='%s' AND `regex_equip_type`='DIAL' GROUP BY regex_type",dp_id);
	
	/**
	 * @Section Log DIAL-PEER <Regex information> On console.
	 */
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-DP] : %s\n", line);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-DP] : ============ DIAL PEER REGEX INFORMATION ============\n");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-DP] : %s\n", line);
		
	switch_execute_sql_callback(globals.mutex, sql, switch_regex_callback, &regex_master);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-DP] : DIALPEER REGEX INFORMATION SQL : \n%s\n", sql);
	switch_safe_free(sql);

	/**
	 * @Section Source-Destination Numbers of DP
	 */
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : Source Number [ %s ] For DIALPEER [ %s ].\n", source_number, dp->dp_id);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : Destination Number [ %s ] For DIALPEER [ %s ]\n", destination_number, dp->dp_id);
	
	/**
	 * @Section Checking Source Number <disallow regex> on FreeSWITCH Server is regex is available for disallow source number.
	 */
	
	if(!zstr(regex_master.disallow_source)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-DP] : Proceed To Check DIALPEER [ %s ] Source Number [ %s ] Disallow Regex [ %s ]\n", dp->dp_id, source_number, regex_master.disallow_source);
		
		/**
		 * @Section Checking Source Number <disallow> ---> on failed <true> ---> on success <false>
		 */
		
		regex_status = switch_disallow_regex(session, channel, regex_master.disallow_source, source_number);
		if(zstr(regex_status)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[AMAZE-DP] : proceed for another DIALPEER if there is ?\n");
			return SWITCH_DIALPEER_SUCCESS;
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-DP] : DIALPEER [ %s ] Regex Status [ %s ]\n",dp->dp_id, regex_status);
		  
		/**
		 * @Section on-failed checking go for next DP or not ?
		 */
		
		// failed regex
		if(!strcmp(regex_status, "true")) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-DP] : souce disallow failed, source number is not allow from disallow regex\n");
			switch_safe_free(regex_status);
			
			// routing hunt is true means next dp if available
			if(!strcasecmp(dp_route_hunt,"Y") ) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[AMAZE-DP] : DIALPEER [ %s ] has routing is set to yes, so system will not search another dial peer and call should be hangup.\n", dp->dp_id);
				return SWITCH_DIALPEER_FAILED;
			} 
			
			// routing hunt is false so do not check for another DP.
			else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[AMAZE-DP] : proceed for another dial peer if there is ?\n");
				return SWITCH_DIALPEER_SUCCESS;
			}
		}
		
		// Free Memory
		switch_safe_free(regex_status);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : Source Number disallow Regex Validate Successfully.\n");
	} 
	
	// Dialpeer Source Number Disallow Regex is not set, so system will not chcek it
	else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[AMAZE-DP] : DIALPEER [ %s ] Source Number [ %s ] Disallow Regex is not set, So system will not check it.\n", dp->dp_id, source_number);
	}

	/**
	 * @Section Checking Source Number <allow regex> on FreeSWITCH Server is regex is available for allow source number.
	 */
	
	if(!zstr(regex_master.allow_source)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-DP] : Proceed To Check DIALPEER [ %s ] Source Number [ %s ] allow Regex [ %s ].\n", dp->dp_id, source_number, regex_master.allow_source);
	
		/**
		 * @Section Checking Source Number <allow> ---> on failed <false> ---> on success <true>
		 */
		
		regex_status = switch_allow_regex(session, channel, regex_master.allow_source, source_number);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-DP] : DIALPEER [ %s ] Regex Status [ %s ]\n",dp->dp_id, regex_status);
		
		// failed regex
		if(!strcmp(regex_status, "false")) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-DP] : Source Number [ %s ] Allow Regex [ %s ] is failed for DIALPEER [ %s ]\n",source_number, regex_master.allow_source, dp->dp_id);
			switch_safe_free(regex_status);
			
			// routing hunt is true means next dp if available
			if(!strcasecmp(dp_route_hunt,"Y") ) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[AMAZE-DP] : DIALPEER [ %s ] has routing is set to yes, so system will not search another dial peer and call should be hangup.\n", dp->dp_id);
				return SWITCH_DIALPEER_FAILED;
			} 
			
			// routing hunt is false so do not check for another DP.
			else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[AMAZE-DP] : proceed for another dial peer if there is ?\n");
				return SWITCH_DIALPEER_SUCCESS;
			}
		}
		
		//Free Memory
		switch_safe_free(regex_status);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : Source Number Allow Regex Validate Successfully.\n");
	} 
	
	// Dialpeer Source Number allow Regex is not set, so system will not chcek it
	else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[AMAZE-DP] : DIALPEER [ %s ] Source Number [ %s ] allow Regex is not set, So system will not check it.\n", dp->dp_id, source_number);
	}
	
	/**
	 * @Section dialpeer Destination Number <disallow Regex String> 
	 */
	
	if(!zstr(regex_master.disallow_destination)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-DP] : Proceed To Check DIALPEER [ %s ] Destination Number [ %s ] Disallow Regex [ %s ]\n", dp->dp_id, destination_number, regex_master.disallow_destination);
	
		/**
		 * @Section Checking Destination Number is match in disallow <regex> if <matched than do not allow it>
		 */
		
		regex_status = switch_disallow_regex(session, channel, regex_master.disallow_destination, destination_number);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-DP] : DIALPEER [ %s ] Regex Status [ %s ]\n",dp->dp_id, regex_status);
		
		/**
		 * @Section validate Destination Number <true means do not allow this number> and <false means allow this number>
		 */
		
		if(!strcmp(regex_status, "true")) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-DP] : Destination Number [ %s ] Disallow Regex [ %s ] is failed for DIALPEER [ %s ]\n", destination_number, regex_master.disallow_destination, dp->dp_id);
			switch_safe_free(regex_status);
			
			/**
			 * @Section checking reroute on next DP or not ? <dp_route_hunt='Y' reroute and 'N' meanse do not >
			 */
			
			if(!strcasecmp(dp_route_hunt,"Y") ) {// DP Route Hunt is Enable
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : DIALPEER [ %s ] Route Hunt is Enable.\n", dp->dp_id);
			  
				return SWITCH_DIALPEER_FAILED;
			} else { // DP Route Hunt is Disable
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : DIALPEER [ %s ] Route Hunt is Disable.\n", dp->dp_id);
				return SWITCH_DIALPEER_SUCCESS;
			}
		}
		switch_safe_free(regex_status);

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : Destination Number disallow Regex Validate Successfully.\n");
	} else {// DP Destination Number Disallow regex is not set, so system will not check it.
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[AMAZE-DP] : DIALPEER [ %s ] Destination Number [ %s ] Disallow Regex is not set, So system will not check it\n", dp->dp_id, destination_number);
	}
	
	/**
	 * @Section Destination Number <allow regex validation> <true means valid> and <false means failed>
	 */
	
	if(!zstr(regex_master.allow_destination)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-DP] : Proceed To Check DIALPEER [ %s ] Destination Number [ %s ] allow Regex [ %s ]\n", dp->dp_id, destination_number, regex_master.allow_destination);

		/**
		 * @Section validate Destination Number using FreeSWITCH regex.
		 */
		
		regex_status = switch_allow_regex(session, channel, regex_master.allow_destination, destination_number);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-DP] : DIALPEER [ %s ] Regex Status [ %s ]\n",dp->dp_id, regex_status);

		/**
		 * @Section regext-status <false means failed> and <true means validate>
		 */
		
		if(!strcmp(regex_status, "false")) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-DP] : DIALPEER Destination Number [ %s ] allow regex [ %s] failed.\n", destination_number, regex_master.allow_destination);
			switch_safe_free(regex_status);
			
			/**
			 * @Section checking reroute on next DP or not ? <dp_route_hunt='Y' reroute and 'N' meanse do not >
			 */
			
			if(!strcasecmp(dp_route_hunt,"Y") ) {// DP Route Hunt is Enable
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : DIALPEER [ %s ] Route Hunt is Enable.\n", dp->dp_id);
				return SWITCH_DIALPEER_FAILED;
			} else {// DP Route Hunt is Disable
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : DIALPEER [ %s ] Route Hunt is Disable.\n", dp->dp_id);
				return SWITCH_DIALPEER_SUCCESS;	
			}
		}
		
		// Free Memory
		switch_safe_free(regex_status);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : Destination Number allow Regex Validate Successfully.\n");
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[AMAZE-DP] : DIALPEER [ %s ] Destination Number [ %s ] allow Regex is not set, So system will not check it.\n", dp->dp_id, destination_number);
	}
	
	/**
	 * @Section Source Number Manipulation <DIALPEER>
	 */
	
	if(!zstr(regex_master.change_source)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : Source Billing Number [ %s ] For DIALPEER [ %s ]\n", source_billing_number, dp->dp_id);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : Destination Billing Number [ %s ] For DIALPEER [ %s ]\n", destination_billing_number, dp->dp_id);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-DP] : Proceed To DIALPEER [ %s ] Source Number [ %s ] Regex [ %s ] Manipulation.\n", dp->dp_id, source_number, regex_master.change_source);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : DIALPEER [ %s ] Before Regex Manipulation Source Number [ %s ]\n", dp->dp_id, source_number);

		tmp_num = strdup(source_number);
		
		/**
		 * @Section Source Number manipulate on Freeswitch Server and gives new number after regex manipulation
		 */
		
		source_number = switch_regex_manipulation(session, channel, regex_master.change_source, source_number);
		if(!strcmp(source_number, "true") || !strcmp(source_number, "false")) {
			source_number = strdup(tmp_num);
		} else {
			source_number = strdup(source_number);
		}
		
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : DIALPEER [ %s ] After Regex Manipulation Source Number : %s.\n", dp->dp_id,source_number);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-DP] : DIALPEER [ %s ] Source Number [ %s ] Manipulation Regex is not set, So system will not check it.\n", dp->dp_id, source_number);
	}
	dp->sip_term_source_number = strdup(source_number);// For Termination Purpose
	
	/**
	 * @Section Destination Number Manipulation <DIALPEER>
	 */
	
	if(!zstr(regex_master.change_destination)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-DP] : Proceed To DIALPEER [ %s ] Destination Number [ %s ] Regex [ %s ] Manipulation.\n", dp->dp_id, destination_number, regex_master.change_destination);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : DIALPEER [ %s ] Before regex manipulation destination_number : %s.\n", dp->dp_id,destination_number);

		tmp_num = strdup(destination_number);
		
		/**
		 * @Section Destination Number manipulate on Freeswitch Server and gives new number after regex manipulation
		 */
		
		destination_number = switch_regex_manipulation(session, channel, regex_master.change_destination, destination_number);
		if(!strcmp(destination_number, "true") || !strcmp(destination_number, "false")) {
			destination_number = strdup(tmp_num);
		} else {
			destination_number = strdup(destination_number);
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : DIALPEER [ %s ] After Regex Manipulation Destination Number : %s.\n", dp->dp_id,destination_number);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-DP] : DIALPEER [ %s ] Destination Number [ %s ] Manipulation Regex is not set, So system will not check it.\n", dp->dp_id, destination_number);
	}
	
	dp->sip_term_destination_number = strdup(destination_number);// For Termination Purpose
	
	/**
	 * @Section Source Billing Number Manipulation <DIALPEER>
	 */
	
	if(!zstr(regex_master.change_bill_source)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-DP] : Proceed To DIALPEER [ %s ] Source Billing Number [ %s ] Regex [ %s ] Manipulation.\n", dp->dp_id, source_billing_number, regex_master.change_bill_source);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : DIALPEER [ %s ] Before regex manipulation Source Billin Number [ %s ].\n", dp->dp_id, source_billing_number);

		tmp_num = strdup(source_billing_number);
		
		/**
		 * @Section Source Billing Number manipulate on Freeswitch Server and gives new number after regex manipulation
		 */
		
		source_billing_number = switch_regex_manipulation(session, channel, regex_master.change_bill_source, source_billing_number);
		if(!strcmp(source_billing_number, "true") || !strcmp(source_billing_number, "false")) {
			source_billing_number = strdup(tmp_num);
		} else {
			source_billing_number = strdup(source_billing_number);
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : DIALPEER [ %s ] After Regex Manipulation Source Billin Number [ %s ].\n", dp->dp_id, source_billing_number);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[AMAZE-DP] : Proceed To DIALPEER [ %s ] Source Billing Number [ %s ] Manipulation Regex is not set, So system will not check it.\n", dp->dp_id, source_billing_number);
	}
	
	/**
	 * @Section Destination Billing Number Manipulation <DIALPEER>
	 */
	
	if(!zstr(regex_master.change_bill_destination)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-DP] : Proceed To DIALPEER [ %s ] Destination Billing Number [ %s ] Regex [ %s ] Manipulation.\n", dp->dp_id, destination_billing_number, regex_master.change_bill_destination);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : DIALPEER [ %s ] Before regex manipulation Destination Billin Number [ %s ].\n", dp->dp_id, destination_billing_number);
		
		tmp_num = strdup(destination_billing_number);
		
		/**
		 * @Section Destination Billinfg Number manipulate on Freeswitch Server and gives new number after regex manipulation
		 */
		
		destination_billing_number = switch_regex_manipulation(session, channel, regex_master.change_bill_destination, destination_billing_number);
		if(!strcmp(destination_billing_number, "true") || !strcmp(destination_billing_number, "false")) {
			destination_billing_number = strdup(tmp_num);
		} else {
			destination_billing_number = strdup(destination_billing_number);
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : DIALPEER [ %s ] After Regex Manipulation Destination Billin Number [ %s ].\n", dp->dp_id, destination_billing_number);
		
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-DP] : Proceed To DIALPEER [ %s ] Destination Billing Number [ %s ] Manipulation Regex is not set, So system will not check it.\n", dp->dp_id, destination_billing_number);
	}
	
	/**
	 * @Section Setting Channel variables for CDR Report
	 */
        
        // ----------------- For Redirect to Dialpeer -------- date : 13-Sept-2016
	switch_channel_set_variable(channel, "vox_redirect_dp_source_billing_number", source_billing_number);
	switch_channel_set_variable(channel, "vox_redirect_dp_destination_billing_number", destination_billing_number);
	switch_channel_export_variable(channel, "vox_redirect_dp_source_billing_number", source_billing_number, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
	switch_channel_export_variable(channel, "vox_redirect_dp_destination_billing_number", destination_billing_number, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);

        //-----------------------------------------------------------------
        
        
	switch_channel_set_variable(channel, "vox_source_billing_number", source_billing_number);
	switch_channel_set_variable(channel, "vox_destination_billing_number", destination_billing_number);
	switch_channel_export_variable(channel, "vox_source_billing_number", source_billing_number, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
	switch_channel_export_variable(channel, "vox_destination_billing_number", destination_billing_number, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-DP] : Proceed To DIALPEER [ %s ] Resource Limit Varification.\n", dp->dp_id);
	
	/**
	 * @Section Check Dialpeer Capacity Group MAX CPS in redis server.
	 */
	
	if(dp_cgp_cps > 0) {
		char *idname = switch_mprintf("%s_cg_max_cps", dp_cgp_id);      /*! Capacity Group Max CPS */
		int retval = 0;                                                 /*! Function Return Value */
		char *hiredis_raw_response = NULL;                              /*! Hiredis Response */

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[AMAZE-DP] : ********* PROCEED TO SET LIMIT *********\n");
		
		/**
		 * @function check key value in redis server <KEY = [CAPACITY-GROUP-ID]_cg_max_cps >. 
		 */
		
		retval = switch_check_resource_limit("hiredis", session, globals.profile, idname,  dp_cgp_cps, 1, "CAPACITY_GP_CPS_EXCEEDED");
		hiredis_raw_response = switch_mprintf("%s",switch_channel_get_variable(channel, "vox_hiredis_response"));
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-DP] : hiredis_raw_response : %s\n",hiredis_raw_response); 
		
		/**
		 * @section maxcps validation
		 */
		
		if((strlen(hiredis_raw_response)>0 && !strcasecmp(hiredis_raw_response, "success") )) {
			if(retval != 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-DP] : DIALPEER [ %s ] Capacity Group [ %s ] CPS Limit Exceeded\n", dp->dp_id, dp_cgp_id);
				switch_channel_set_variable(channel, "termination_string", "oe_cps_exceeded XML OE_CPS_EXCEEDED");
				
				/**
				 * @Section Checking Route Hunt Status
				 */
				
				if(!strcasecmp(dp_route_hunt,"Y") ) {
					return SWITCH_DIALPEER_FAILED;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[AMAZE-DP] : proceed for another dial peer if there is ?\n");
					return SWITCH_DIALPEER_SUCCESS;
				}
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[AMAZE-DP] : Hiredis is unable to connect redis server [ %s ], Diap Peer Capacity Group [ %s ] CPS will not check.\n",hiredis_raw_response,dp_cgp_id );
		}
		switch_safe_free(idname);
		switch_safe_free(hiredis_raw_response);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : Diap Peer Capacity Group [ %s ] CPS Is Unlimited.\n", dp_cgp_id);
	}

	/**
	 * @Section Check Dialpeer Max CPS value is set or not.
	 */
	
	if(dp_max_cps > 0) {
		char *idname = switch_mprintf("%s_dp_max_cps", dp_id);
		int retval = 0;
		char *hiredis_raw_response = NULL;
		
		/**
		 * @function check key value in redis server <KEY = [DIALPEER-ID]_cg_max_cps >. 
		 */
		
		retval = switch_check_resource_limit("hiredis", session, globals.profile, idname,  dp_max_cps, 1, "CAPACITY_DP_CPS_EXCEEDED");
		hiredis_raw_response = switch_mprintf("%s",switch_channel_get_variable(channel, "vox_hiredis_response"));
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-DP] : hiredis_raw_response : %s\n",hiredis_raw_response); 

		/**
		 * @section maxcps validation
		 */
		
		if((strlen(hiredis_raw_response)>0 && !strcasecmp(hiredis_raw_response, "success") )) {
			if(retval != 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-DP] : Dial Peer [ %s ] CPS Limit Exceeded\n", dp_id);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : Proceed For Next DIALPEER If Allowed\n");
				
				/**
				 * @Section Decreament Capacity Group Max Call in Redis
				 */
				
				if(!zstr(capacity_group_mc_idname)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : Proceed To Decreament Capacity Group [ %s ] MAX Value.\n", dp_cgp_id);
					switch_channel_set_variable(channel, "termination_string", "DP_MAX_CPS_EXCEEDED XML DP_MAX_CPS_EXCEEDED");
				}
				
				/**
				 * @Section Checking Route Hunt Status
				 */

				if(!strcasecmp(dp_route_hunt,"Y") ) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : DIALPEER [ %s ] Route Hunt is Enable.\n", dp->dp_id);
					return SWITCH_DIALPEER_FAILED;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[AMAZE-DP] : proceed for another dial peer if there is ?\n");
					return SWITCH_DIALPEER_SUCCESS;
				}
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[ ORIGINATION ] : Hiredis is unable to connect redis server [ %s ], Diap Peer  [ %s ] MAX CPS will not check.\n",hiredis_raw_response, dp_id);
		}
		switch_safe_free(idname);
		switch_safe_free(hiredis_raw_response);
		
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : DIAP-PEER [ %s ] MAX CPS IS UNLIMITED.\n", dp_id);
	}
	
	if(dp_max_calls > 0 ) {
		char *hiredis_raw_response = NULL;                             /*! Hiredis Response */
		char *idname = switch_mprintf("%s_dp_max_call", dp_id);        /*! Dialpeer Max Call Key */
		
		/**
		 * @function check key value in redis server <KEY = [DIALPEER-ID]_dp_max_call >. 
		 */

		int retval = switch_check_resource_limit("hiredis", session, globals.profile, idname,  dp_max_calls, 0, "DIAL_PEER_MC_EXCEEDED");
		
		hiredis_raw_response = switch_mprintf("%s",switch_channel_get_variable(channel, "vox_hiredis_response"));
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-DP] : hiredis_raw_response : %s\n",hiredis_raw_response); 

		/**
		 * @section maxcps validation
		 */
		
		if((strlen(hiredis_raw_response)>0 && !strcasecmp(hiredis_raw_response, "success") )) {
			if(retval != 0) {
                                int limit_status = SWITCH_STATUS_SUCCESS;      /*! Limit Status */
                                /*! release hiredis resource */
                                limit_status = switch_limit_release("hiredis", session, globals.profile, idname);
                                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[ ORIGINATION ] : release [ %s ] [ %d ]\n", idname, limit_status);
                                switch_safe_free(idname);
                                        
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-DP] : DIALPEER MAX Call Limit Exceeded\n");
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : Proceed for next dial peer if available\n");
				
				/**
				 * @Section Decreament Dialpeer Max Call in Redis
				 */
				
				if(!zstr(capacity_group_mc_idname)) {    
                                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : Proceed To Decreament Dial Peer [ %s ] MAX CALL Value.\n", dp_id);
					switch_channel_set_variable(channel, "termination_string", "DP_MAX_CALL_EXCEEDED XML DP_MAX_CALL_EXCEEDED");
				}

				/**
				 * @Section Checking Route Hunt Status
				 */
				
				if(!strcasecmp(dp_route_hunt,"Y") ) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-DP] : Route Hunt Is Enable So Do Not Check For Another DIALPEER\n");
					return SWITCH_DIALPEER_FAILED;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[AMAZE-DP] : proceed for another dial peer if there is ?\n");
					return SWITCH_DIALPEER_SUCCESS;	
				}
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[ ORIGINATION ] : Hiredis is unable to connect redis server [ %s ], Diap Peer  [ %s ] MAX CALL will not check.\n",hiredis_raw_response, dp_id);
		}
		switch_safe_free(idname);
		switch_safe_free(hiredis_raw_response);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-DP] : Diap Peer [ %s ] MAX Call Is Unlimited.\n", dp_id);
	}
	
	/**
	 * @Section Checking Dialpeer Capacity Group Max Call in Redis
	 */
	
	if(dp_cgp_mc > 0 ) {
		char *idname = switch_mprintf("%s_cg_max_call", dp_cgp_id);       /*! Dialpeer Capacity Group Max Call Key */
		int retval = 0;                                                   /*! Function Return Value */ 
		char *hiredis_raw_response = NULL;                                /*! Hiredis Response */

		capacity_group_mc_idname = switch_mprintf(idname);
		
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[AMAZE-DP] : ********* PROCEED TO SET LIMIT *********\n");
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-DP] : idname : %s\n", idname);
		
		/**
		 * @function check key value in redis server <KEY = [DIALPEER-CAPACITY-GROUP-ID]_cg_max_call >. 
		 */
		
		retval = switch_check_resource_limit("hiredis", session, globals.profile, idname,  dp_cgp_mc, 0, "CG_MC_EXCEEDED");

		/*! Hiredis response */
		hiredis_raw_response = switch_mprintf("%s",switch_channel_get_variable(channel, "vox_hiredis_response"));
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-DP] : hiredis_raw_response : %s\n",hiredis_raw_response); 

		/**
		 * @section maxcps validation
		 */
		
		if((strlen(hiredis_raw_response)>0 && !strcasecmp(hiredis_raw_response, "success") )) {
			if(retval != 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-DP] : DIALPEER [ %s ] Capacity Group [ %s ] Max Call Limit Exceeded\n", dp->dp_id,dp_cgp_id);
				switch_channel_set_variable(channel, "termination_string", "cg_mc_exceeded XML CG_MC_EXCEEDED");
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : Proceed For Next DIALPEER If Allowed\n");
				
				/**
				 * @Section Checking Route Hunt Status
				 */
				
				if(!strcasecmp(dp_route_hunt,"Y") ) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : DIALPEER [ %s ] Route Hunt is Enable.\n", dp->dp_id);
					return SWITCH_DIALPEER_FAILED;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[AMAZE-DP] : proceed for another dial peer if there is ?\n");
					return SWITCH_DIALPEER_SUCCESS;
				}
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[AMAZE-DP] : Hiredis is unable to connect redis server [ %s ], DIALPEER [ %s ] Capacity Group [ %s ] MAX CALL is set for unlimited.\n",hiredis_raw_response, dp->dp_id, dp_cgp_id);
		}
		switch_safe_free(idname);
		switch_safe_free(hiredis_raw_response);
	}
	
	dp->sip_equipment_type = strdup(dp_equip_type);
	dp->flag = 1;
        
        // Execute SIP Redirction Equipment Application    
        if(!zstr(dp->sip_equipment_type) && !strcasecmp(dp->sip_equipment_type,"SIP_TERMINATION")) {
                char *sip_termination_data = NULL;
                
                sip_termination_data = switch_mprintf("%s,%s,%s,%s,SIP_TERMINATION", dp->sip_term_source_number, dp->sip_term_destination_number, dp->dp_id, dp->dp_balancing_method);
                
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : SIP Termination Equipment Data : \n%s\n", sip_termination_data);
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : Proceed To Execute Termination Application\n");
                
                switch_core_session_execute_application(session,"termination", sip_termination_data);
                switch_safe_free(sip_termination_data);
                
                if(!strcmp(switch_str_nil(switch_channel_get_variable(channel, "reroute_on_dialpeer")),"yes") ) {
                     switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : Proceed For Next Dial Peer of origination equipment mapped.\n");
                     return SWITCH_DIALPEER_SUCCESS;
                }
                
               switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : Termination Application Execution done.\n");
                
        } else if(!zstr(dp->sip_equipment_type) && !strcasecmp(dp->sip_equipment_type,"SIP_REDIRECT")) {
                char *sip_termination_data = NULL;
                
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : SIP Redirect Equipment [ redirection ] Application Called.\n");
                
                sip_termination_data = switch_mprintf("%s,%s,%s,%s,SIP_REDIRECT", dp->sip_term_source_number, dp->sip_term_destination_number, dp->dp_id, dp->dp_balancing_method);
                
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : SIP Redirection Equipment Data : \n%s\n", sip_termination_data);
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : Proceed To Execute Redirection Application\n");
                
                switch_core_session_execute_application(session, "redirection", sip_termination_data);
                switch_safe_free(sip_termination_data);
                
                if(!strcmp(switch_str_nil(switch_channel_get_variable(channel, "reroute_on_dialpeer")), "yes") ) {
                     switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : Proceed For Next Dial Peer of origination equipment mapped.\n");
                     return SWITCH_DIALPEER_SUCCESS;
                }
        }
        
	return SWITCH_DIALPEER_FAILED;
}

/*! DialPeer Application */
SWITCH_STANDARD_APP(switch_dialpeer_app)
{
	char *mydata = NULL;                             /*! dialpeer Application Data */
	char *originate_equip_id = NULL;                 /*! Origination Equipment Id */
	char *argv[6] = {0};                             /*! Stringization string */
	char *sql = NULL;                                /*! SQL */
	char *original_source_number = NULL;             /*! Original Source Number */
	char *original_destination_number = NULL;        /*! Original Destination Number */
	char *source_billing_number = NULL;              /*! Source Billing Number */
	char *destination_billing_number = NULL;         /*! Destination Billing Number */
	switch_dial_peer_handler_st dp_handler;      /*! dialpeer handler */
	
	switch_channel_t *channel = switch_core_session_get_channel(session);      /*! Channel */
            
        char *redirection_to_dialpeer = NULL;
        char *dp_id = NULL;

	/**
	 * @Section Checking Channel validation
	 */
	
	if(!channel) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-DP] : Hey I am unable to get channel..!!\n");
		switch_channel_set_variable(channel, "switch_hangup_code", "66");
		switch_channel_set_variable(channel, "switch_hangup_reason", "CHAN_NOT_IMPLEMENTED");
		switch_channel_set_variable(channel, "termination_string", "SWITCH_HANGUP XML SWITCH_HANGUP");
		goto end;
	}
	
	redirection_to_dialpeer = (char *)switch_str_nil(switch_channel_get_variable(channel, "redirection_to_dialpeer"));
	
	/*! Copy Data From Session Of Application */
	mydata = switch_core_session_strdup(session, data);
	
	/**
	 * @Section Data validation
	 */
	
	if(!mydata) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-DP] : dialpeer application is unable to get data from origination application, so system will not allow to dial this call.\n");
		switch_channel_set_variable(channel, "switch_hangup_code", "400");
		switch_channel_set_variable(channel, "switch_hangup_reason", "NORMAL_TEMPORARY_FAILURE");
		switch_channel_set_variable(channel, "termination_string", "SWITCH_HANGUP XML SWITCH_HANGUP");
		switch_channel_export_variable(channel, "cdr_call_flag", "1", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
		goto end;
	}
	
	switch_core_session_execute_exten(session, "EXPORT_VARS_ON_OTHERLEG" ,"XML" ,"EXPORT_VARS_ON_OTHERLEG");
	
	/**
	 * @Section Dialpeer Log on console
	 */
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-DP] : ORIGINATION_EQUIPMENT_ID,ORIGINAL_SOURCE_NUMBER, ORIGINATL_DESTINATION_NUMBER,BILLING_SOURCE_NUMBER,BILLING_DESTINATION_NUMBER\n");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : DIALPEER application received data [ %s ].\n", mydata);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : ++++++++++++ DIALPEER validation started ++++++++++++\n");

	/*! Separating Data in OE id , Source Number, Destination Number */  
	switch_separate_string(mydata, '_', argv, (sizeof(argv) / sizeof(argv[0])));
	
        
        if(!strcasecmp(redirection_to_dialpeer, "yes")) {
            
                if(zstr(argv[0]) || zstr(argv[1]) || zstr(argv[2]) || zstr(argv[3]) ) {
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-DP] : DIALPEER application is unable to validate data which received from origination application, so system will hangup this call.\n");
                        switch_channel_set_variable(channel, "switch_hangup_code", "400");
                        switch_channel_set_variable(channel, "switch_hangup_reason", "NORMAL_TEMPORARY_FAILURE");
                        switch_channel_set_variable(channel, "termination_string", "SWITCH_HANGUP XML SWITCH_HANGUP");
                        switch_channel_export_variable(channel, "cdr_call_flag", "1", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
                        goto end;
                }
                
        	originate_equip_id =(char *)switch_str_nil(switch_channel_get_variable(channel, "vox_origination_equipment_id")); /*! Copy Origination Equipment Id */
              
                dp_id = strdup(argv[4]);                 /*! Copy Original Source Number */
                original_source_number = strdup(argv[0]);                 /*! Copy Original Source Number */
                original_destination_number = strdup(argv[1]);            /*! Copy Original Destination Number */
                source_billing_number = strdup(argv[2]);                  /*! Copy Source Billing Number */     
                destination_billing_number = strdup(argv[3]);             /*! Copy Destination Billing Number */
        
            
        } else {
        
        
	/**
	 * @Section Validation Channel Data.
	 */
	
	if(zstr(argv[0]) || zstr(argv[1]) || zstr(argv[2]) || zstr(argv[3]) || zstr(argv[4])) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-DP] : DIALPEER application is unable to validate data which received from origination application, so system will hangup this call.\n");
		switch_channel_set_variable(channel, "switch_hangup_code", "400");
		switch_channel_set_variable(channel, "switch_hangup_reason", "NORMAL_TEMPORARY_FAILURE");
		switch_channel_set_variable(channel, "termination_string", "SWITCH_HANGUP XML SWITCH_HANGUP");
		switch_channel_export_variable(channel, "cdr_call_flag", "1", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
		goto end;
	}
	
	originate_equip_id = strdup(argv[0]);                     /*! Copy Origination Equipment Id */
	original_source_number = strdup(argv[1]);                 /*! Copy Original Source Number */
	original_destination_number = strdup(argv[2]);            /*! Copy Original Destination Number */
	source_billing_number = strdup(argv[3]);                  /*! Copy Source Billing Number */     
	destination_billing_number = strdup(argv[4]);             /*! Copy Destination Billing Number */

	/**
	 * @Section Application Data validation
	 */
	
        }
        
	if(zstr(originate_equip_id) || zstr(original_source_number) || zstr(original_destination_number) || zstr(source_billing_number) || zstr(destination_billing_number)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-DP] : DIALPEER application is unable to validate data which received from origination application, so system will hangup this call.\n");
		switch_channel_set_variable(channel, "switch_hangup_code", "400");
		switch_channel_set_variable(channel, "switch_hangup_reason", "NORMAL_TEMPORARY_FAILURE");
		switch_channel_set_variable(channel, "termination_string", "SWITCH_HANGUP XML SWITCH_HANGUP");
		switch_channel_export_variable(channel, "cdr_call_flag", "1", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
		goto end;
	}
	
	/**
	 * @Section Log on Console 
	 */
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-DP] : DIALPEER Received Request From Originate Equipment [ %s ]\n", originate_equip_id);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-DP] : DIALPEER Recievde Source Number [ %s ] From Origination Equipment [ %s ]\n", original_source_number, originate_equip_id);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-DP] : DIALPEER Recievde Destination Number [ %s ] From Origination Equipment [ %s ]\n", original_destination_number, originate_equip_id);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-DP] : DIALPEER Billing Source Number [ %s ] From Origination Equipment [ %s ]\n", source_billing_number, originate_equip_id);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-DP] : DIALPEER Billing Destination Number [ %s ] From Origination Equipment [ %s ]\n", destination_billing_number, originate_equip_id);

	/*! Initialize dialpeer object */
	memset(&dp_handler, 0, sizeof(dp_handler));
	
	/**
	 * @Section Copy Data in dialpeer Handler
	 */
	
	dp_handler.session = session;                                                      /*! Assigned Session */
	dp_handler.source_number = strdup(original_source_number);                         /*! Copy Source Number */
	dp_handler.destination_number = strdup(original_destination_number);               /*! Copy Destination Number */
	dp_handler.source_billing_number = strdup(source_billing_number);                  /*! Copy Source Billing Number */
	dp_handler.destination_billing_number = strdup(destination_billing_number);        /*! Copy Billing Destination Number */

	/**
	 * @Section Proceed to Get dialpeer Information
	 */
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-DP] : Proceed To Get DIALPEER Which Are Mapped With Origination Equipment [ %s ]\n", originate_equip_id);

        /*! Call From SIP Redirection application */    
          if(!strcasecmp(redirection_to_dialpeer, "yes")) {
                sql = switch_mprintf("SELECT distinct(c.dp_id), IF(isnull(c.dp_max_calls_sec),0,c.dp_max_calls_sec) as dp_max_calls_sec , IF(isnull(c.dp_max_calls),0,c.dp_max_calls) as dp_max_calls, c.dp_balancing_method, IF(isnull(c.cpg_id),0,c.cpg_id) as cpg_id, c.dp_equip_type,  c.dp_route_hunt FROM vca_dial_peer c WHERE  c.dp_status = 'Y' AND dp_id = '%s' And  UNIX_TIMESTAMP(NOW()) BETWEEN c.dp_start_date AND c.dp_end_date ORDER BY c.dp_priority, c.dp_id", dp_id);
              
              
          } else {    /*! Call From SIP Origination application */    
		sql = switch_mprintf("SELECT distinct(c.dp_id), IF(isnull(c.dp_max_calls_sec),0,c.dp_max_calls_sec) as dp_max_calls_sec , IF(isnull(c.dp_max_calls),0,c.dp_max_calls) as dp_max_calls, c.dp_balancing_method, IF(isnull(c.cpg_id),0,c.cpg_id) as cpg_id, c.dp_equip_type,  c.dp_route_hunt FROM vca_orig_route_mapping a, vca_dial_routing_group_mapping b, vca_dial_peer c WHERE a.rg_id=b.rg_id AND b.dp_id=c.dp_id AND  a.orig_id = '%s' AND c.dp_status = 'Y' And  UNIX_TIMESTAMP(NOW()) BETWEEN c.dp_start_date AND c.dp_end_date ORDER BY c.dp_priority, c.dp_id", originate_equip_id);
          }
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-DP] : Origination Equipments [ %s ] DIALPEER information SQL : \n%s\n",originate_equip_id, sql);
	switch_execute_sql_callback(globals.mutex, sql, dialpeer_callback_function, &dp_handler);
	
	/**
	 * @Section validate dialpeer information
	 */
	
	if(dp_handler.flag != 1) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-DP] : Failed To Verify DIALPEER, So System Will Not Allow Dial This Call.\n");
		switch_channel_set_variable(channel, "switch_hangup_code", "401");
		switch_channel_set_variable(channel, "switch_hangup_reason", "CALL_REJECTED");
		switch_channel_set_variable(channel, "termination_string", "SWITCH_HANGUP XML SWITCH_HANGUP");
		
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-DP] : System Is Unable To Get Data From SQL : \n\n%s\n\n", sql);
		
		switch_safe_free(sql);
		switch_channel_export_variable(channel, "cdr_call_flag", "1", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
		goto end;
	}
	switch_safe_free(sql);

	/**
	 * @Section Checking SIP Termination Equipment Type 
	 */
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : DIALPEER [ %s ] Equipment Type Is [ %s ]\n", dp_handler.dp_id, dp_handler.sip_equipment_type);
	
	/**
	 * @Section SIP Redirect Equipment  
	 */
        
        
//         if(!strcmp(switch_str_nil(switch_channel_get_variable(channel, "reroute_on_dialpeer")),"yes") && !strcmp(dp_handler.sip_equipment_type,"SIP_TERMINATION")) {
//                 switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : DELETE ACTIVE CALL RECORD FROM HERE\n");
//                 sql = switch_mprintf("DELETE FROM vca_active_call WHERE ac_uuid='%s'", switch_str_nil(switch_channel_get_variable(channel, "uuid")));
//                 switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-DP] : SIP Termination Equipment  Delete Active Call Record SQL :: \n%s\n", sql);
//                 switch_execute_sql(sql, globals.mutex);
//                 switch_safe_free(sql);
//         } else {
//              switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-DP] : Need To Delete SIP Redirect Equipment Active Call Here.\n");
//         }
                
        
        
        
// 	if(!strcmp(dp_handler.sip_equipment_type, "SIP_REDIRECT")) {
// 		char *transfer = NULL;         /*! transfer application data */
	  
		/**
		 * @Section Transfering Call to Termination Equipment 
		 */
		
		//SIP Redirect Termination Equipment
// 		transfer = switch_mprintf("%s,%s,%s,%s,SIP_REDIRECT XML PROCEED_TERMINATION_ROUTING", dp_handler.sip_term_source_number, dp_handler.sip_term_destination_number, dp_handler.dp_id, dp_handler.dp_balancing_method);
// 		switch_channel_set_variable(channel, "termination_string", transfer);

//                 switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : PROCEED FOR SIP REDIRECT IN DIALPEER [ %s ].\n", dp_handler.dp_id);  
// 		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : PROCEED FOR SIP REDIRECT IN DIALPEER [ %s ].\n", dp_handler.dp_id);  
// 		switch_safe_free(transfer);
// 	} 
	
	/**
	 * @Section SIP Termination Equipment
	 */
	
// 	else if(!strcmp(dp_handler.sip_equipment_type, "SIP_TERMINATION")) {
// 		char *transfer = NULL;       /*! transfer application data */
		
		/**
		 * @Section Transfering Call to Termination Equipment
		 */
		
		//SIP TERMINATION EQUIPMENT IDS 123#SOURCENUMBER,DESTINATION NUMBER
// 		transfer = switch_mprintf("%s,%s,%s,%s,SIP_TERMINATION XML PROCEED_TERMINATION_ROUTING", dp_handler.sip_term_source_number, dp_handler.sip_term_destination_number, dp_handler.dp_id, dp_handler.dp_balancing_method);
// 		switch_channel_set_variable(channel, "termination_string", transfer);
		
// 		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : PROCEED FOR SIP TERMINATION IN DIALPEER [ %s ].\n", dp_handler.dp_id);  
// 		switch_safe_free(transfer);
// 	} else {
// 		goto end;
		switch_channel_export_variable(channel, "cdr_call_flag", "1", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
// 	}
	
end :
	return ;
}

/**
 * @Function Load module in FreeSWITCH Kernel Memory
 */

SWITCH_MODULE_LOAD_FUNCTION(mod_dialpeer_load)
{
	switch_application_interface_t *app_interface;      /*! Application interface */
	switch_status_t status;                             /*! Loading module Status */

	/*! Initializing Global structure */
	memset(&globals, 0, sizeof(globals));
	globals.pool = pool;

	/*! Mutex Initializing */
	switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, globals.pool);

	/**
	 * @Section Loading Configuartion From Config File 
	 */
	
	if ((status = load_config()) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	/**
	 * @Section Checking mod_origination is loaded or not 
	 */
	
	if (switch_loadable_module_exists("mod_origination") != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-DP] : First Load mod_origination.\n");
		return SWITCH_STATUS_FALSE;
	} 
	
	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	/**
	 * @Section Log on Console 
	 */
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-DP] : Loading dialpeer Module.\n");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[AMAZE-DP] : ======================================\n");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s", banner);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[AMAZE-DP] : ======================================\n");
	
	/**
	 * @Section Application dialpeer define 
	 */
	SWITCH_ADD_APP(app_interface, "dialpeer", "dialpeer", "DP Switch", switch_dialpeer_app, NULL, SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);

	return SWITCH_STATUS_SUCCESS;
}

/**
 * @Function Shutdown Function 
 */

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_dialpeer_shutdown)
{
	return SWITCH_STATUS_SUCCESS;
}

// End Header Guard.
#endif



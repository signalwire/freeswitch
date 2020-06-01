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
 * mod_redirection.c  SIP Redirection Equipment Application
 * 
 */

//-------------------------------------------------------------------------------------//
//  To include Header Guard
//-------------------------------------------------------------------------------------//

#ifndef REDIRECTION_HEADER
#define REDIRECTION_HEADER

//-------------------------------------------------------------------------------------//
//  Include Header Files.
//-------------------------------------------------------------------------------------//

#include <switch.h>             /*! switch Header File */
#include <netinet/tcp.h>        /*! Network Header File */

//-------------------------------------------------------------------------------------//
//  Function Prototype
//-------------------------------------------------------------------------------------//

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_redirection_shutdown);          /*! Execute on shutdown */
SWITCH_MODULE_RUNTIME_FUNCTION(mod_redirection_runtime);            /*! Execute on runtime */
SWITCH_MODULE_LOAD_FUNCTION(mod_redirection_load);                  /*! Execute on Load time */
SWITCH_MODULE_DEFINITION(mod_redirection, mod_redirection_load, mod_redirection_shutdown, NULL);   /*! Module function Declaration */

/*! Module Banner */

char banner[] = "\n"
"/**\n"
" * \n"
" * The Initial Developer of the Original Code is\n"
" * Amaze Telecom <suppor@amazetelecom.com>\n"
" * \n"
" * Portions created by the Initial Developer are Copyright (C)\n"
" * the Initial Developer. All Rights Reserved.\n"
" * \n"
" * Contributor(s):\n"
" * \n"
" * Amaze Telecom <suppor@amazetelecom.com>\n"
" * \n"
" * mod_redirection.c SIP Redirection Equipment Application.\n"
" */\n";

static char *line = "++++++++++++++++++++++++++++++++++++++++++++++++++++++++";

/**
 * @var define global variables
 */

#ifndef  SWITCH_REDIRECTION_SUCCESS
#define  SWITCH_REDIRECTION_SUCCESS            0               /*! On Success */
#define  SWITCH_REDIRECTION_FAILED            -1               /*! On Failed */
#define  SWITCH_REDIRECTION_SUCCESS            0               /*! On Success */
#define  SWITCH_REDIRECTION_FAILED            -1               /*! On Failed */
#define  SWITCH_REDIRECTION_SQLITE_DBNAME     "amazeswitch"         /*! Default DB Name */
#endif

/**
 * @var Application Configuartion file name
 */
static const char *global_cf = "amazeswitch.conf";       /*! Configuartion File Name */  

/*! Configuartion Structure */
static struct {
	char *odbc_dsn;               /*! SIP Equipment odbc dsn */
	char *dbname;                 /*! SIP Equipment dbname */
	char *bind_ipaddress;         /*! SIP Equipment Bind IP Address */
	unsigned int bind_ipport;     /*! SIP Equipment Bind Port */
	char *profile;                /*! SIP Equipment Redis Profile */
	char *default_max_call_dur;   /*! MAX DEFAULT CALL DURATION */
	switch_mutex_t *mutex;        /*! SIP Equipment mutex */
	switch_memory_pool_t *pool;   /*! SIP Equipment Memory pool */
} globals;


/*! SIP Redirect Equipment Structure */
struct sip_redirect {
	char *sip_redirect_id;        /*! SIP Redirect Equipment ID */
	char *sip_redirect_zoneid;    /*! SIP Redirect Equipment Zone ID */
	char *call_timeout;           /*! SIP Redirect Equipment Call Timeout */
};
typedef struct sip_redirect sip_redirect_t;

// static switch_cache_db_handle_t *switch_get_db_handler(void);
static switch_bool_t switch_execute_sql_callback(switch_mutex_t *mutex, char *sql, switch_core_db_callback_func_t callback, void *pdata);
// static char *switch_execute_sql2str(switch_mutex_t *mutex, char *sql, char *resbuf, size_t len);

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
 		    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-RE] : Your resource limit is exceeded...!!!\n");
// 		    switch_channel_set_variable(channel, "transfer_string", "cg_cps_exceeded XML CG_CPS_EXCEEDED");
		    ret = -1;
		    goto end;
	    }
    }
    
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[AMAZE-RE] : ********* LIMIT SET SUCCESSFULLY *********\n");
    
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
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-RE] : api_cmd : \n%s\n", api_cmd);
		switch_safe_free(api_cmd);

		api_result = (char *)switch_channel_get_variable(channel, "api_result");
		if(!zstr(api_result)) {
			char *switch_result = NULL;
			switch_result = switch_mprintf("%s", api_result);
			if(!strcmp(switch_result, "true")) {
			    
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-RE] : Routing is Disallow on this number.\n");
				status = switch_mprintf("true");
				switch_safe_free(switch_result);
				switch_channel_set_variable(channel, "transfer_string", "REGEX_DISALLOW_NUMBER XML REGEX_DISALLOW_NUMBER");
				goto end;
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-RE] : [ %s ] is passed to regex string [ %s ].\n", source, regex_str);
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
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-RE] : api_cmd : %s\n",api_cmd);
		switch_safe_free(api_cmd);

		api_result = (char *)switch_channel_get_variable(channel, "api_result");

		if(!zstr(api_result)) {
			char *switch_result = NULL;
			
			switch_result = switch_mprintf("%s", api_result);
			if(!strcmp(switch_result, "false")) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-RE] : validation false.\n");
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
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-RE] : regex_str : %s\n",regex_str);
		
		token = strtok(string, seprator);
		while( token != NULL) {
			char *ptr = NULL;
			char *api_result = NULL;
			
			ptr = switch_mprintf("%s", token);
			switch_replace_character(ptr, '/', '|');
			switch_replace_character(ptr, '\\', '%'); //replace $1 by %1
			ptr[strlen(ptr)-1]  = '\0'; //to remove $ from string end
			
			api_cmd = switch_mprintf("api_result=${regex(%s|%s}", source, ptr);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-RE] : api_cmd : %s\n", api_cmd);
			switch_core_session_execute_application(session, "set", api_cmd);
			api_result = (char *)switch_channel_get_variable(channel, "api_result");
			
			if(!zstr(api_result)) {
				char *switch_result = NULL;
				switch_result = switch_mprintf("%s", api_result);
				source = strdup(switch_result);
				
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-RE] : api_result : %s\n", api_result);
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

static switch_status_t load_config(void)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;      /*! Loading Config Status */
	switch_xml_t cfg, xml, settings, param;              /*! Loading Config Param */  
	
	if (!(xml = switch_xml_open_cfg(global_cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-RE] : Open of %s failed\n", global_cf);
		status = SWITCH_STATUS_TERM;
		goto end;
	}

	switch_mutex_lock(globals.mutex);

	/**
	 * @Section Load Configuartion into Global Structure
	 */
	
	if ((settings = switch_xml_child(cfg, "settings"))) {
	  
		/**
		 * @Section Load config param into variable
		 */
		
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			if (!strcasecmp(var, "dbname")) {             /*! odbc DBName */
				globals.dbname = strdup(val);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-RE] : dbname [ %s ]\n", globals.dbname);
			} else if (!strcasecmp(var, "odbc-dsn")) {    /*! odbc dsn */   
				globals.odbc_dsn = strdup(val);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-RE] : odbc-dsn [ %s ]\n", globals.odbc_dsn);
			} else if (!strcasecmp(var, "bind-ipaddress")) {   /*! Bind Ip Address */
				globals.bind_ipaddress = strdup(val);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-RE] : bind-ipaddress [ %s ]\n", globals.bind_ipaddress);
			} else if (!strcasecmp(var, "bind-ipport")) {      /*! Bind Ip Port */
				globals.bind_ipport = atoi(val);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-RE] : bind-ipport [ %d ]\n", globals.bind_ipport);
			} else if (!strcasecmp(var, "redis-profile")) {     /*! Redis Profile */
				globals.profile = strdup(val);
				switch_log_printf(SWITCH_CHANNEL_LOG,SWITCH_LOG_DEBUG5, "[AMAZE-RE] : redis-profile [ %s ]\n", globals.profile);
			}
		}
	}
	
	
	/**
	 * @var Default Database name
	 */

	if (!globals.dbname) {
		//use default db name if not specify in configure
		globals.dbname = strdup(SWITCH_REDIRECTION_SQLITE_DBNAME);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-RE] : set default DB :%s\n", globals.dbname);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-RE] : Loaded Configuration successfully\n");

	end:
	switch_mutex_unlock(globals.mutex);

	//free memory
	if (xml) {
		switch_xml_free(xml);
	}

	return status;
}

/**
 * @function Get db handler from odbc dsn
 * @param void
 * @return odbc dsn
 */
static switch_cache_db_handle_t *switch_get_db_handler(void)
{
	switch_cache_db_handle_t *dbh = NULL;      /*! ODBC DSN HANDLER */
	char *dsn;                                 /*! ODBC DSN STRING */  

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
 * @function Execute sql callback function
 * @param mutex
 * @param sql query string
 * @param callback function pointer for call function
 * @param pdata argument passed in callback function
 */
static switch_bool_t switch_execute_sql_callback(switch_mutex_t *mutex, char *sql, switch_core_db_callback_func_t callback, void *pdata)
{
	switch_bool_t ret = SWITCH_FALSE;          /*! SQL Query Execution Status */
	char *errmsg = NULL;                       /*! SQL Query Execution ERROR String */
	switch_cache_db_handle_t *dbh = NULL;      /*! MySQL odbc dsn Handler */

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
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-RE] : Error Opening DB\n");
		goto end;
	}

	/**
	 * @Function Execute SQL in MYSQL and get result row wise in callback function
	 */
	
	switch_cache_db_execute_sql_callback(dbh, sql, callback, pdata, &errmsg);

	if (errmsg) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-RE] : SQL ERR: [%s] %s\n", sql, errmsg);
		free(errmsg);
	}

	end:

	/**
	 * @Function release dsn handler 
	 */
	
	switch_cache_db_release_db_handle(&dbh);

	if (mutex) {
		switch_mutex_unlock(mutex);
	} else {
		switch_mutex_unlock(globals.mutex);
	}

	return ret;
}

/**
 * @function Execute sql and get result in string
 * @param mutex 
 * @param sql 
 * @param resbuf result will store
 * @param len sizeof of data
 * @return query result in string
 */
static char *switch_execute_sql2str(switch_mutex_t *mutex, char *sql, char *resbuf, size_t len)
{
	char *ret = NULL;                         /*! SQL Query Execution result value */
	switch_cache_db_handle_t *dbh = NULL;     /*! odbc Handler */

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
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-RE] : Error Opening DB\n");
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
 * @function Get SIP Redirect Equipment information
 */

static int switch_redirect_callback(void *ptr, int argc, char **argv, char **col) 
{
	sip_redirect_t *SipRedirect = (sip_redirect_t *)ptr;      /*! SIP redirect Equipment Structure */
	int index = 0;                                            /*! Coloum COunt */  
	
	/**
	 * @Section Log SIP Redirect Equipment on console 
	 */
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[ TERMINATION ] : %s\n", line);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[ TERMINATION ] : ========= SIP REDIRECT EQUIPMENT INFORMATION =========\n");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[ TERMINATION ] : %s\n", line);

	for(index = 0; index < argc ; index++) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[ TERMINATION ] : %s : %s\n", col[index], argv[index]);
	}
	
	SipRedirect->sip_redirect_id = strdup(argv[0]);                /*! SIP Redirect Equipment ID */
	SipRedirect->sip_redirect_zoneid = strdup(argv[1]);            /*! SIP Redirect Equipment Zone ID */
	SipRedirect->call_timeout = strdup(argv[2]);                   /*! SIP Redirect Equipment Call Timeout */
	
	return SWITCH_REDIRECTION_SUCCESS;
}

SWITCH_STANDARD_APP(switch_redirection_app)
{
	char *mydata = NULL;                     /*! Session Data */
	// 	char *argv[25] = {0};                    /*! Session Application Argument */
	char *Pargv[6] = {0};                    /*! Session String Seperatoring */ 
	char *sql = NULL;                        /*! SQL */
	// 	char *sip_id_data = NULL;                /*! SIP ID data */
	char *source_number = NULL;              /*! Source Number */
	char *destination_number = NULL;         /*! Destination Number */
	char *term_source_number = NULL;         /*! SIP Redirection Source Number */    
	char *term_destination_number = NULL;    /*! SIP Redirection Destination Number */
	char *dp_balancing_method = NULL;        /*! SIP Redirection Balancing Method */
	char *dp_termination_type = "";          /*! SIP Redirection Equipment Type */
	// 	int vox_term_id = 0;                     /*! SIP Redirection ID */
	int dp_id = 0;                           /*! SIP Redirection Dialpeer ID */
	
	/*! Session Channel */
	switch_channel_t *channel = switch_core_session_get_channel(session);
	
	if(!channel) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-RE] : Hey I am unable to get channel..!!\n");
		switch_channel_set_variable(channel, "switch_hangup_code", "401");
		switch_channel_set_variable(channel, "switch_hangup_reason", "CALL_REJECTED");
		switch_channel_set_variable(channel, "dialout_string", "SWITCH_HANGUP XML SWITCH_HANGUP");
		goto end;
	}
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-RE] : redirection application is started to execute.\n");
	mydata = switch_core_session_strdup(session, data); /*! Copy Data From Session */
	if(!mydata) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-RE] : redirection application is not able to get data from session.\n");
		switch_channel_set_variable(channel, "switch_hangup_code", "401");
		switch_channel_set_variable(channel, "switch_hangup_reason", "CALL_REJECTED");
		switch_channel_set_variable(channel, "dialout_string", "SWITCH_HANGUP XML SWITCH_HANGUP");
		goto end;
	}
	
	/*! Application Log */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-RE] : BILLING_SOURCE_NUMBER,BILLING_DESTINATION_NUMBER, DIALPEER_ID,BALANCING_METHOD,TERMINATION_TYPE\n");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-RE] : Redirection application Get Data From DIALPEER [ %s ].\n", mydata);
	
	/**
	 * @Section Application Data Validation
	 */
	
	switch_separate_string(mydata, ',', Pargv, (sizeof(Pargv) / sizeof(Pargv[0])));
	if(zstr(Pargv[0]) || zstr(Pargv[1]) || zstr(Pargv[2]) || zstr(Pargv[3]) || zstr(Pargv[4]) ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-RE] : SIP Redirection Equipment is unable to get data.\n");
		switch_channel_set_variable(channel, "switch_hangup_code", "401");
		switch_channel_set_variable(channel, "switch_hangup_reason", "CALL_REJECTED");
		switch_channel_set_variable(channel, "dialout_string", "SWITCH_HANGUP XML SWITCH_HANGUP");
		goto end; 
	}
	
	/**
	 * @Section Application Log on console
	 */
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-RE] : Redirection Application Get Source Number [ %s ] From DIALPEER.\n", Pargv[0]);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-RE] : Redirection Application Get Destination Number [ %s ] From DIALPEER.\n", Pargv[1]);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-RE] : Redirection Application Get DIALPEER [ %s ] From DIALPEER.\n", Pargv[2]);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-RE] : Redirection Application Get DIALPEER Balancing Method [ %s ] From DIALPEER.\n", Pargv[3]);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-RE] : Redirection Application Get DIALPEER Redirection Type [ %s ] From DIALPEER.\n", Pargv[4]);
	
	term_source_number = strdup(Pargv[0]);         /*! Redirection Equipment Source Number */
	term_destination_number = strdup(Pargv[1]);    /*! Redirection Equipment Destination Number */
	dp_termination_type = strdup(Pargv[4]);        /*! Redirection Equipment Type */
	dp_id = atoi(Pargv[2]);                        /*! Redirection Equipment DialPeer ID */
	dp_balancing_method = strdup(Pargv[3]);        /*! Redirection Equipment Balancing Method */
	
	//for cdr
	switch_channel_export_variable(channel, "vox_termination_equipment_type", dp_termination_type, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
	
	if(!strcmp(dp_termination_type, "SIP_REDIRECT")) {
		
		char *sip_ids = NULL;                     /*! SIP Redirect Equipment ID */
// 		char dialstring[400] = "";                /*! SIP Redirection Equipments IP List */
// 		char *routing_str = NULL;                 /*! Sofia Bridge String */
// 		char resbuf[25] = "";                     /*! Query Result*/
		char *Pargv[25] = {0};                    /*! IPS String */
		int argc = 0;                             /*! Argument counts */ 
		int i = 0;                                /*! Loop Variable */
		switch_event_t *ovars;                    /*! Event Object */ 
		char *sip_from_call_id = NULL;
		sip_from_call_id = (char *)switch_channel_get_variable(channel, "vox_originator_sip_call_id");
		
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-RE] : sip_from_call_id [ %s ]\n", sip_from_call_id);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-RE] : SIP Redirection Type Is Redirect Server.\n");
		
		source_number = strdup(term_source_number);
		destination_number = strdup(term_destination_number);
		
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-RE] : source_number [ %s ]\n", source_number);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-RE] : destination_number [ %s ]\n", destination_number);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-RE] : dp_id [ %d ]\n", dp_id);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-RE] : Balancing Method [ %s ]\n", dp_balancing_method);
			
			//NOTE BALANCING METHOD IS PRIORITY BASE SO CONSIDER IT AS PRIORITY
			
		if(!strcmp(dp_balancing_method, "PRIORITY_BASED")) {
			char dialstring[400] = "";                /*! SIP Redirection Equipments IP List */
			char *routing_str = NULL;                 /*! Sofia Bridge String */
			char resbuf[25] = "";                     /*! Query Result*/
			
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-RE] : Proceed To Get SIP Redirect Equipment\n");
			
			sql = switch_mprintf("SELECT GROUP_CONCAT(sip_id) FROM  vca_dial_sip_equip_mapping  WHERE dp_id='%d' ORDER BY `dsen_priority`", dp_id);
			switch_execute_sql2str(NULL, sql, resbuf, sizeof(resbuf));
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-RE] : SIP Redirect Equipment ID LIST SQL : %s\n", sql);
			switch_safe_free(sql);
			
			if(zstr(resbuf)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-RE] : Dial peer [ %d ] do not mapped with any SIP Redirect Equipment.\n", dp_id);
				switch_channel_set_variable(channel, "switch_hangup_code", "401");
				switch_channel_set_variable(channel, "switch_hangup_reason", "CALL_REJECTED");
				switch_channel_set_variable(channel, "dialout_string", "SWITCH_HANGUP XML SWITCH_HANGUP");
				goto end;
			}
			
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-RE] : Proceed To Get SIP Redirect Equipment ID List Are [ %s ]\n", resbuf);
			
			/*! Separte Each SIP Equipment ID by ,*/
			argc = switch_separate_string(resbuf, ',', Pargv, (sizeof(Pargv) / sizeof(Pargv[0])));
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-RE] : SIP Redirect Equipments Count [ %d ]\n", argc);
			
			
			for( i = 0 ;i < argc ; i++) {
				int count = 0;
				char *ipargv[15] = {0};
				int j = 0;
				sip_redirect_t  SipRedirect;     /*! SIP Redirect Structure */
				
				if(!switch_channel_up(channel)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-RE] : Switch Origination Session Destroyed.\n");
					goto end;
				}
				
				sip_ids = switch_mprintf(Pargv[i]);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-RE] : Proceed for SIP Redirect Equipments  [ %s ]\n", sip_ids);
				
				if(zstr(sip_ids)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-RE] : Switch is unable to get SIP Redirect Equipment IDS.\n");
					goto end;
				}
				memset(&SipRedirect, 0, sizeof(SipRedirect));
				
				sql = switch_mprintf("SELECT a.sip_id, a.zone_id,a.sip_timeout FROM `vca_sip_equipment` a,`vca_sip_equipment_mapping` b WHERE a.sip_id = b.sip_id AND a.sip_status='Y' And UNIX_TIMESTAMP(NOW()) BETWEEN a.sip_start_date AND a.sip_end_date AND a.`sip_id` ='%s' limit 1", sip_ids);
				
				switch_execute_sql_callback(globals.mutex, sql, switch_redirect_callback, &SipRedirect);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-RE] : SIP Redirect Equipment [ %s ] Information SQL  :\n %s\n", sip_ids, sql);
				switch_safe_free(sql);
				
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-RE] : SIP Redirect Equipment [ %s ] SIP ID [ %s ]\n", sip_ids, SipRedirect.sip_redirect_id);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-RE] : SIP Redirect Equipment [ %s ] ZONE ID [  %s ]\n", sip_ids, SipRedirect.sip_redirect_zoneid);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-RE] : SIP Redirect Equipment [ %s ] Call Timeout [ %s ]\n", sip_ids, SipRedirect.call_timeout);                        
				
				switch_channel_export_variable(channel, "vox_termination_equipment_id", sip_ids, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
				
				switch_channel_export_variable(channel, "vox_redirection_equipment_id", sip_ids, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
				
				
				sql = switch_mprintf("SELECT group_concat(concat(b.sip_map_ip,':',b.sip_map_port) ORDER BY b.sip_map_priority SEPARATOR '|') as sip_redirect_dialstring  FROM `vca_sip_equipment` a,`vca_sip_equipment_mapping` b WHERE a.sip_id = b.sip_id AND a.sip_status='Y' And UNIX_TIMESTAMP(NOW()) BETWEEN a.sip_start_date AND a.sip_end_date AND a.`sip_id` ='%s'", sip_ids);
				
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-RE] : SIP Redirect Equipment [ %s ] SIGNALING IP:PORT SQL :\n %s\n", sip_ids, sql);
				
				/*! Execute SQL in MySQL Database*/
				switch_execute_sql2str(NULL, sql, dialstring, sizeof(dialstring));
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-RE] : SIP Redirect Equipment [ %s ] SIGNALING IP:PORT [ %s ]\n", sip_ids, dialstring);
				switch_safe_free(sql);
				
				count = switch_separate_string(dialstring, '|', ipargv, (sizeof(ipargv) / sizeof(ipargv[0])));
				
				for(j = 0; j < count; j++) {
					char *IpPort[2] = {0};
					char *sip_red_ipaddr = NULL;
					char *sip_red_port = NULL;
					
					switch_separate_string(ipargv[j], ':', IpPort, (sizeof(IpPort) / sizeof(IpPort[0])));
					sip_red_ipaddr = switch_mprintf("%s", IpPort[0]);
					sip_red_port = switch_mprintf("%s", IpPort[1]);
					
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-RE] : SIP Redirect Equipment [ %s ] Signaling IP:PORT [ %s:%s ]\n", sip_ids, sip_red_ipaddr, sip_red_port);
					
					if(zstr(sip_red_port) || zstr(sip_red_ipaddr)) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-RE] : SIP Redirect is unable to get IPPORT.\n");
						switch_channel_set_variable(channel, "switch_hangup_code", "401");
						switch_channel_set_variable(channel, "switch_hangup_reason", "CALL_REJECTED");
						switch_channel_set_variable(channel, "dialout_string", "SWITCH_HANGUP XML SWITCH_HANGUP");
						
						continue;
					}
					
					switch_event_create(&ovars, SWITCH_EVENT_REQUEST_PARAMS);
					switch_event_add_header(ovars, SWITCH_STACK_BOTTOM, "vox_termination_equipment_type", "%s", dp_termination_type);
					switch_event_add_header(ovars, SWITCH_STACK_BOTTOM, "vox_redirection_destination_number", "%s", destination_number);
					switch_event_add_header(ovars, SWITCH_STACK_BOTTOM, "vox_redirection_source_number", "%s", source_number);
// 					switch_event_add_header(ovars, SWITCH_STACK_BOTTOM, "sip_redirect_context", "SWITCH_REDIRECTION");
// 					switch_event_add_header(ovars, SWITCH_STACK_BOTTOM, "sip_redirect_dialplan", "XML");
// 					switch_event_add_header(ovars, SWITCH_STACK_BOTTOM, "sip_redirect_profile", "external");
					
					switch_channel_export_variable(channel, "sip_redirect_context", "SWITCH_REDIRECTION", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
					switch_channel_export_variable(channel, "sip_redirect_dialplan", "XML", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
					switch_channel_export_variable(channel, "sip_redirect_profile", "external", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
					
					/*! Sofia Routing Dial String */
					
					routing_str = switch_mprintf("{ignore_early_media='true'}sofia/external/%s@%s:%s;transport=udp",destination_number, sip_red_ipaddr, sip_red_port);
					
					switch_channel_process_export(channel, NULL, ovars, "vox_export_vars");
					routing_str = switch_channel_expand_variables(channel, routing_str);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-RE] : SIP Redirection Equipment Sofia String is [ %s ]\n", routing_str);
					switch_safe_free(sip_red_port);
					switch_safe_free(sip_red_ipaddr);
					
					/*! Sofia Routing Dial String*/
					if(!zstr(routing_str)) {
						switch_status_t status = SWITCH_STATUS_FALSE;                   /*! Call Status */
						switch_core_session_t *callee_session;                          /*! Callee Session */
						switch_call_cause_t cause = SWITCH_CAUSE_NORMAL_CLEARING;       /*! Callee Cause */
// 						switch_channel_t *callee_channel = NULL;                        /*! Callee Channel */
//						char *vox_origination_equipment_id = switch_str_nil((char *)switch_channel_get_variable(channel,"vox_origination_equipment_id"));    
//						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-RE] : vox_origination_equipment_id : %s\n", vox_origination_equipment_id);
						
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-RE] : SIP Redirect Bridge Application Data : \n%s\n", routing_str);
						status = switch_ivr_originate(session, &callee_session, &cause, routing_str, 3600, NULL, source_number, source_number, NULL, ovars, SOF_NONE, NULL, NULL);
						
						switch_safe_free(routing_str);
						switch_event_destroy(&ovars);

						/*! Bridge Call Statue */
						if(status != SWITCH_STATUS_SUCCESS) {
							char * sip_redirect_dialstring = NULL;
							char *SipRedirectingString[25] = {0};
							int redirect_count = 0;
							int cindex = 0;
							char *vox_redirect_dp_source_billing_number = (char *)switch_str_nil(switch_channel_get_variable(channel, "vox_redirect_dp_source_billing_number"));
							char *vox_redirect_dp_destination_billing_number = (char *)switch_str_nil(switch_channel_get_variable(channel, "vox_redirect_dp_destination_billing_number"));
							
							sip_redirect_dialstring = switch_mprintf("%s", switch_str_nil(switch_channel_get_variable(channel, "sip_redirect_dialstring")));
							
							//NOTE Variables For SIP Redirection 
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(callee_session), SWITCH_LOG_INFO, "[AMAZE-RE] : SIP Redirecting String [ %s ]\n", sip_redirect_dialstring);
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-RE] : Redirect SIP From URI sip_full_from [ %s ]\n", switch_str_nil(switch_channel_get_variable(channel, "sip_full_from")));
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-RE] : Redirect SIP To URI  sip_h_To  [ %s ]\n", switch_str_nil(switch_channel_get_variable(channel, "sip_full_to")));
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-RE] : Redirect sip_redirected_to  [ %s ]\n", switch_str_nil(switch_channel_get_variable(channel, "sip_redirected_to")));
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-RE] : Redirect sip_redirected_by  [ %s ]\n", switch_str_nil(switch_channel_get_variable(channel, "sip_redirected_by")));

							redirect_count = switch_separate_string(sip_redirect_dialstring, '|', SipRedirectingString, (sizeof(SipRedirectingString) / sizeof(SipRedirectingString[0])));
							
							for( cindex = 0; cindex < redirect_count ; cindex++) {
								char *gwname = NULL;
								char *dpname = NULL;
								char *sip_dp_name = NULL;
								char *sip_equip_name = NULL;
								
								switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(callee_session), SWITCH_LOG_INFO, "[AMAZE-RE] : contact uri is [ %s ]\n", SipRedirectingString[cindex]);
								
								gwname = strstr(SipRedirectingString[cindex], "gwname=");
								if(gwname && !zstr(gwname)) {
									sip_equip_name = switch_mprintf("%s", gwname+(strlen("gwname=")));
									switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"[AMAZE-RE] : SIP TERMINATION EQUIPMENT NAME [ %s ].", sip_equip_name);
								}
								gwname = NULL;

								//TODO Pending  
								if(sip_equip_name && !zstr(sip_equip_name)) {
									char *sql = NULL;
									char sip_equip_id[10] = "";
									char *sip_termination_data = NULL;
									
									switch_channel_export_variable(channel, "redirection_to_termination", "yes", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
									
									switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(callee_session), SWITCH_LOG_INFO, "[AMAZE-RE] : Proceed For SIP Termination Equipment [ %s ]\n", sip_equip_name);  
									
									sql = switch_mprintf("SELECT term_id FROM vca_term_equipment WHERE term_name='%s' AND term_status='Y'", sip_equip_name);
									switch_execute_sql2str(NULL, sql, sip_equip_id, sizeof(sip_equip_id));
									switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-RE] : SIP Termination Equipment ID SQL : %s\n", sql);
									switch_safe_free(sql);
									
									if(zstr(sip_equip_id)) {
										switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-RE] : System is unable to get Termination Equipment ID from Name, Proceed for Next SIP Redirect Equipment.\n");
										continue;                                
									}
									
									switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(callee_session), SWITCH_LOG_INFO, "[AMAZE-RE] : SIP Termination Equipment ID [ %s ]\n", sip_equip_id); 
									switch_channel_set_variable(channel, "reroute_on_dialpeer", NULL); 
									sip_termination_data = switch_mprintf("%s,%s,%s,NOBALANCING_METHOD,SIP_TERMINATION", source_number, destination_number, sip_equip_id);
									switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-RE] : Proceed To Execute Termination Application\n");
									switch_core_session_execute_application(session,"termination", sip_termination_data);
									switch_safe_free(sip_termination_data);
									if(!strcmp(switch_str_nil(switch_channel_get_variable(channel, "rerouting_is")),"disable")) {
										switch_channel_set_variable(channel, "reroute_on_dialpeer", "no"); 
										goto end; 
									}
									
//                              switch_safe_free(sip_equip_name);
//TODO check here need to next sip equipment or not ???

//                             if(!strcmp(switch_str_nil(switch_channel_get_variable(channel, "reroute_on_dialpeer")),"yes") ) {
//                                     switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-RE] : Proceed For Next SIP Termination Equipment.\n");
//                                     continue;
//                             } else {
//                                     goto end; 
//                             }
								}
	
								dpname = strstr(SipRedirectingString[cindex], "dpname=");
								if(dpname && !zstr(dpname)) {
									sip_dp_name = switch_mprintf("%s", dpname+(strlen("dpname=")));
									switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"[AMAZE-RE] : SIP DIALPEER EQUIPMENT NAME [ %s ].", sip_dp_name);
								}
								dpname = NULL;
								
								if(sip_dp_name && !zstr(sip_dp_name)) {
									char *sql = NULL;
									char result[250] = "";
									char *appData = NULL;
									char *sip_termination_data = NULL;
									switch_channel_export_variable(channel, "redirection_to_dialpeer", "yes", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
									sql = switch_mprintf("SELECT dp_id FROM  vca_dial_peer WHERE dp_status='Y' AND dp_name='%s' AND UNIX_TIMESTAMP(NOW()) BETWEEN dp_start_date AND dp_end_date", sip_dp_name);
									
									switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"[AMAZE-RE] : Proceed To Get Dial peer ID SQL : %s\n", sql);
									switch_execute_sql2str(NULL, sql, result, sizeof(result));
									switch_safe_free(sql);
									
									if(zstr(result)) {
										switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"[AMAZE-RE] : Proceed For Next Contact URI if available\n");
										switch_safe_free(sip_dp_name);
										continue;
									}
									
									appData = switch_mprintf(result);
									switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"[AMAZE-RE] : Dial Peer ID is [ %s ].\n", appData);
									switch_channel_set_variable(channel, "reroute_on_dialpeer", NULL); 
									
									sip_termination_data = switch_mprintf("%s_%s_%s_%s_%s",source_number, destination_number, vox_redirect_dp_source_billing_number, vox_redirect_dp_destination_billing_number, appData);
									switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"[AMAZE-RE] : Prceed To Execute Dial Peer Application\n");
									
									switch_core_session_execute_application(session,"dialpeer", sip_termination_data);
									switch_safe_free(sip_termination_data);
									switch_safe_free(appData);
									
									//TODO check here need to next sip equipment or not ???
									switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-RE] : dialpeer Application Execution is Done \n");
									switch_safe_free(appData);
									
									if(!strcmp(switch_str_nil(switch_channel_get_variable(channel, "rerouting_is")),"disable")) {
										switch_channel_set_variable(channel, "reroute_on_dialpeer", "no"); 
										goto end; 
									}
								}
								
								if(!sip_dp_name && !sip_equip_name) {
									switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(callee_session), SWITCH_LOG_INFO, "[AMAZE-RE] : System is unable to get Equipment Name From Contact header, So proceed for next redirect equipment.\n");                                                                 
									break;
								}
							}
						} else {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[ TERMINATION ] : SIP Redirect Bridge Application Done \n");
						}
					}                                                
				}
			}
			switch_safe_free(sip_ids);
			switch_channel_set_variable(channel, "reroute_on_dialpeer", "yes");
		}
	}
	
	end:
	
	switch_channel_export_variable(channel, "cdr_call_flag", "1", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
	
	return;
}
// Load module when FS start
SWITCH_MODULE_LOAD_FUNCTION(mod_redirection_load)
{
	switch_application_interface_t *app_interface;
	switch_status_t status;

	//initialize structure
	memset(&globals, 0, sizeof(globals));
	globals.pool = pool;

	//initialize mutex 
	switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, globals.pool);

	//load configuration
	if ((status = load_config()) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-RE] : Loading vox redirection Module.\n");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[AMAZE-RE] : ======================================\n");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s", banner);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[AMAZE-RE] : ======================================\n");
	
	//Define redirection application
	SWITCH_ADD_APP(app_interface, "redirection", "redirection", "redirection", switch_redirection_app, NULL, SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);
	
	
	return SWITCH_STATUS_SUCCESS;
}

//shutdown module or execute when module unloaded from FS or Stop FS.
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_redirection_shutdown)
{
  	switch_mutex_lock(globals.mutex);
	switch_safe_free(globals.odbc_dsn);
	switch_safe_free(globals.dbname);
	switch_safe_free(globals.profile);
	switch_mutex_unlock(globals.mutex);
	
	//unbind event.
	return SWITCH_STATUS_SUCCESS;
}

// End Header Guard.
#endif



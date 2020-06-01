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
 * mod_termination.c  Termination Equipment Routing
 * 
 */

//-------------------------------------------------------------------------------------//
//  To include Header Guard
//-------------------------------------------------------------------------------------//

#ifndef TERMINATION_HEADER
#define TERMINATION_HEADER

//-------------------------------------------------------------------------------------//
//  Include Header Files.
//-------------------------------------------------------------------------------------//

#include <switch.h>             /*! switch Header File */
#include <netinet/tcp.h>        /*! Network Header File */

//-------------------------------------------------------------------------------------//
//  Function Prototype
//-------------------------------------------------------------------------------------//

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_termination_shutdown);          /*! Execute on shutdown */
SWITCH_MODULE_RUNTIME_FUNCTION(mod_termination_runtime);            /*! Execute on runtime */
SWITCH_MODULE_LOAD_FUNCTION(mod_termination_load);                  /*! Execute on Load time */
SWITCH_MODULE_DEFINITION(mod_termination, mod_termination_load, mod_termination_shutdown, NULL);   /*! Module function Declaration */

/*! Module Banner */

char banner[] = "\n"
"/**\n"
" * \n"
" * The Initial Developer of the Original Code is\n"
" *Amaze Telecom <suppor@amazetelecom.com>\n"
" * \n"
" * Portions created by the Initial Developer are Copyright (C)\n"
" * the Initial Developer. All Rights Reserved.\n"
" * \n"
" * Contributor(s):\n"
" * \n"
" * Amaze Telecom <suppor@amazetelecom.com>\n"
" * \n"
" * mod_termination.c Termination Equipment routing\n"
" */\n";

static char *line = "++++++++++++++++++++++++++++++++++++++++++++++++++++++++";

/**
 * @var define global variables
 */

#ifndef  SWITCH_TERMINATOR_SUCCESS
#define  SWITCH_TERMINATOR_SUCCESS            0               /*! On Success */
#define  SWITCH_TERMINATOR_FAILED            -1               /*! On Failed */
#define  SWITCH_TERMINATOR_SQLITE_DBNAME     "amazeswitch"         /*! Default DB Name */
#define  SWITCH_SIP_PREVENT                  1                /*! SIP Prevent Reroute */
#define  SWITCH_ITUT_PREVENT                 2                /*! ITUT Prevent Reroute */ 
#define  SWITCH_SIP_REROUTE                  3                /*! SIP Reroute */
#define  SWITCH_ITUT_REROUTE                 4                /*! ITUT Reroute */
#define  SWITCH_SIP_RETRY                    5                /*! SIP Retry */
#define  SWITCH_ITUT_RETRY                   6                /*! ITUT Retry */  
#define OPENSIPS_DEFAULT_PORT               5060              /*! Opensips Default PORT */
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

/**
 * @struct sip_termination_equipment_handler Structure Of SIP Termination Equipment
 * 
 */

struct sip_termination_equipment_handler
{
	char *term_id;                           /*! SIP Termination Equipment ID */
	char *term_max_call_dur;                 /*! SIP Termination Equipment Max Call Duration */
	char *term_progress_timeout;             /*! SIP Termination Equipment Progress Timeout */
	char *term_progress_timeout_action;      /*! SIP Termination Equipment On Progress Timeout action */
	char *term_session_time;                 /*! SIP Termination Equipment Session Timeout */
	char *term_rtp_time;                     /*! SIP Termination Equipment RTP Timeout */
	char *term_media_proxy;                  /*! SIP Termination Equipment Proxy mode */
	char *term_privacy_method;               /*! SIP Termination Equipment privacy method */
	char *term_codec_policy;                 /*! SIP Termination Equipment Codec Policy */
	char *term_media_node;                   /*! SIP Termination Equipment Media Node */
	char *term_source_number_type;           /*! SIP Termination Equipment Source Number Type  */
	char *term_source_number_plan;           /*! SIP Termination Equipment Source Number Plan */
	char *term_signal_ip;                    /*! SIP Termination Equipment Signaling IP */
	char *term_signal_port;                  /*! SIP Termination Equipment Signaling Port */ 
	char *term_sip_call_reroute;             /*! SIP Termination Equipment SIP Call Reroute */
	char *term_itu_call_reroute;             /*! SIP Termination Equipment ITUT Call Reroute */
	char *term_sip_prevent_call_reroute;     /*! SIP Termination Equipment SIP Call Prevent */
	char *term_itu_prevent_call_reroute;     /*! SIP Termination Equipment ITUT Call Prevent */
	char *term_sip_call_retry;               /*! SIP Termination Equipment SIP Prevent Call Reroute */
	char *term_itu_call_retry;               /*! SIP Termination Equipment ITUT Prevent Call Reroute */ 
	char *term_zone_id;                      /*! SIP Termination Equipment Zone ID */

	int term_max_calls;                      /*! SIP Termination Equipment Max Call Limit */ 
	int term_max_calls_sec;                  /*! SIP Termination Equipment Max Call Duration */
	int term_max_retry;                      /*! SIP Termination Equipment Max Retry */
	int term_retry_time;                     /*! SIP Termination Equipment Max Retry Time */
	int term_group_id;                       /*! SIP Termination Equipment Group Code ID */
	int term_cpg_id;                         /*! SIP Termination Equipment Capacity Group ID */
	int cpg_max_calls;                       /*! SIP Termination Equipment Capacity Group Max Call */ 
	int cpg_max_calls_sec;                   /*! SIP Termination Equipment Capacity Group Max CPS */
};
typedef struct sip_termination_equipment_handler sip_termination_equipment_handler_st;

struct regex_master
{
	char *change_source;            /*! SIP Termination Equipment Source Number Manipulation Regex */
	char *change_destination;       /*! SIP Termination Equipment Destination Number Manipulation Regex */
};
typedef struct regex_master regex_master_st;

struct __sip_server
{
	char *opensips_default_ip;           /*! Opensips Default IP */
	char *sip_sig_node_id;                 /*! Signaling Node ID */  
	char *sip_media_node_id;               /*! Media Node ID */ 
};


/**
 * @function get SIP Termination Equipment info
 * 
 */
static int switch_termination_callback(void *ptr, int argc, char **argv, char **col);
static switch_cache_db_handle_t *switch_get_db_handler(void);
static switch_bool_t switch_execute_sql_callback(switch_mutex_t *mutex, char *sql, switch_core_db_callback_func_t callback, void *pdata);
static char *switch_execute_sql2str(switch_mutex_t *mutex, char *sql, char *resbuf, size_t len);

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
 		    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-TE] : Your resource limit is exceeded...!!!\n");
// 		    switch_channel_set_variable(channel, "transfer_string", "cg_cps_exceeded XML CG_CPS_EXCEEDED");
		    ret = -1;
		    goto end;
	    }
    }
    
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[AMAZE-TE] : ********* LIMIT SET SUCCESSFULLY *********\n");
    
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
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : api_cmd : \n%s\n", api_cmd);
		switch_safe_free(api_cmd);

		api_result = (char *)switch_channel_get_variable(channel, "api_result");
		if(!zstr(api_result)) {
			char *switch_result = NULL;
			switch_result = switch_mprintf("%s", api_result);
			if(!strcmp(switch_result, "true")) {
			    
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-TE] : Routing is Disallow on this number.\n");
				status = switch_mprintf("true");
				switch_safe_free(switch_result);
				switch_channel_set_variable(channel, "transfer_string", "REGEX_DISALLOW_NUMBER XML REGEX_DISALLOW_NUMBER");
				goto end;
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : [ %s ] is passed to regex string [ %s ].\n", source, regex_str);
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
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : api_cmd : %s\n",api_cmd);
		switch_safe_free(api_cmd);

		api_result = (char *)switch_channel_get_variable(channel, "api_result");

		if(!zstr(api_result)) {
			char *switch_result = NULL;
			
			switch_result = switch_mprintf("%s", api_result);
			if(!strcmp(switch_result, "false")) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-TE] : validation false.\n");
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
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : regex_str : %s\n",regex_str);
		
		token = strtok(string, seprator);
		while( token != NULL) {
			char *ptr = NULL;
			char *api_result = NULL;
			
			ptr = switch_mprintf("%s", token);
			switch_replace_character(ptr, '/', '|');
			switch_replace_character(ptr, '\\', '%'); //replace $1 by %1
			ptr[strlen(ptr)-1]  = '\0'; //to remove $ from string end
			
			api_cmd = switch_mprintf("api_result=${regex(%s|%s}", source, ptr);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : api_cmd : %s\n", api_cmd);
			switch_core_session_execute_application(session, "set", api_cmd);
			api_result = (char *)switch_channel_get_variable(channel, "api_result");
			
			if(!zstr(api_result)) {
				char *switch_result = NULL;
				switch_result = switch_mprintf("%s", api_result);
				source = strdup(switch_result);
				
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : api_result : %s\n", api_result);
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
 * @function Loading Configuartion From config file 
 */

static switch_status_t load_config(void)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;      /*! Loading Config Status */
	switch_xml_t cfg, xml, settings, param;              /*! Loading Config Param */  
	char result[50] = ""; 
	char *sql = NULL;
	
	/**
	 * @Section open Configuartion File 
	 */
	
	if (!(xml = switch_xml_open_cfg(global_cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-TE] : Open of %s failed\n", global_cf);
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
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : dbname [ %s ]\n", globals.dbname);
			} else if (!strcasecmp(var, "odbc-dsn")) {    /*! odbc dsn */   
				globals.odbc_dsn = strdup(val);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : odbc-dsn [ %s ]\n", globals.odbc_dsn);
			} else if (!strcasecmp(var, "bind-ipaddress")) {   /*! Bind Ip Address */
				globals.bind_ipaddress = strdup(val);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : bind-ipaddress [ %s ]\n", globals.bind_ipaddress);
			} else if (!strcasecmp(var, "bind-ipport")) {      /*! Bind Ip Port */
				globals.bind_ipport = atoi(val);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : bind-ipport [ %d ]\n", globals.bind_ipport);
			} else if (!strcasecmp(var, "redis-profile")) {     /*! Redis Profile */
				globals.profile = strdup(val);
				switch_log_printf(SWITCH_CHANNEL_LOG,SWITCH_LOG_DEBUG5, "[AMAZE-TE] : redis-profile [ %s ]\n", globals.profile);
			}
		}
	}
	
	// SQL To Get Default MAX Call Duartion 
	sql = switch_mprintf("SELECT cfg_value FROM vca_global_config WHERE cfg_key='max_call_duration' limit 1");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Get Default MAX Call Duration SQL :: \n%s\n",sql);
	switch_execute_sql2str(NULL, sql, result, sizeof(result));
	switch_safe_free(sql);
	
	if(!zstr(result)) {
		globals.default_max_call_dur = strdup(result);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Default Max Call Duration :: %s\n", globals.default_max_call_dur);
	}
	
	/**
	 * @var Default Database name
	 */

	if (!globals.dbname) {
		//use default db name if not specify in configure
		globals.dbname = strdup(SWITCH_TERMINATOR_SQLITE_DBNAME);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : set default DB :%s\n", globals.dbname);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : Loaded Configuration successfully\n");

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
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-TE] : Error Opening DB\n");
		goto end;
	}

	/**
	 * @Function Execute SQL in MYSQL and get result row wise in callback function
	 */
	
	switch_cache_db_execute_sql_callback(dbh, sql, callback, pdata, &errmsg);

	if (errmsg) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-TE] : SQL ERR: [%s] %s\n", sql, errmsg);
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
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-TE] : Error Opening DB\n");
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
 * @function execute SQL for UPDATE/INSERT/DELET Query
 * @param sql
 * @param mutex
 * @return void
 */
static switch_status_t switch_execute_sql(char *sql, switch_mutex_t *mutex)
{
	switch_cache_db_handle_t *dbh = NULL;            /*! odbc dsn Handler */
	switch_status_t status = SWITCH_STATUS_FALSE;    /*! SQL Execution Status */

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
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-TE] : Error Opening DB\n");
		goto end;
	}

	
	/**
	 * @function Execute SQL in MySQL and get result.
	 */
	
	status = switch_cache_db_execute_sql(dbh, sql, NULL);

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

	return status;
}

/**
 * @function regex callback
 * @param regex master object
 * @param argc result count
 * @param result array
 * @param result coloum
 * 
 */

static int switch_regex_callback(void *ptr, int argc, char **argv, char **col) 
{
	regex_master_st *term = (regex_master_st *)ptr;        /*! SIP Termination Equipment Regex structure pointer */
	int index = 0;                                         /*! Count Coloum */

	/**
	 * @Section Log on Console. 
	 */
	
	for(index = 0; index < argc ; index++) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : %s : %s\n", col[index], argv[index]);
	}

	if(!strcmp(argv[0], "CHANGE_SOURCE")) {                      /*! Change Source Number Regex*/
		term->change_source = strdup(argv[1]);                 
	} else if(!strcmp(argv[0], "CHANGE_DESTINATION")) {          /*! Change Destination Number Regex*/
		term->change_destination = strdup(argv[1]);
	}

	return SWITCH_TERMINATOR_SUCCESS;
}

/**
 * @function Get sip Termination Equipment Info
 * and store in sip equipment object
 */

static int switch_termination_callback(void *ptr, int argc, char **argv, char **col) 
{
	sip_termination_equipment_handler_st *sipHandler = (sip_termination_equipment_handler_st *)ptr;       /*! SIP Termination Structure */
	int index = 0;                                                                                        /*! Coloum Count */ 
	
	/**
	 * @Section Log on Console
	 */
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : %s\n", line);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : ========= SIP TERMINATION EQUIPMENT INFORMATION =========\n");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : %s\n", line);

	for(index = 0; index < argc ; index++) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : %s : %s\n", col[index], argv[index]);
	}
	
	/*! ------------------------------------------------------------------------ */
	
	sipHandler->term_id = strdup(argv[0]);                                /*! SIP Termination Equipment ID */
	sipHandler->term_max_call_dur = strdup(argv[1]);                      /*! SIP Termination Equipment Max Call Duration */
	sipHandler->term_max_calls = atoi(argv[2]);                           /*! SIP Termination Equipment MAX Calls */
	sipHandler->term_max_calls_sec = atoi(argv[3]);                       /*! SIP Termination Equipment MAX CPS */
	sipHandler->term_progress_timeout = strdup(argv[4]);                  /*! SIP Termination Equipment Progress Timeout */  
	sipHandler->term_progress_timeout_action = strdup(argv[5]);           /*! SIP Termination Equipment On Progress Timeout Action */
	sipHandler->term_session_time = strdup(argv[6]);                      /*! SIP Termination Equipment Session Timeout */
	sipHandler->term_rtp_time = strdup(argv[7]);                          /*! SIP Termination Equipment RTP Timeout */
	sipHandler->term_max_retry = atoi(argv[8]);                           /*! SIP Termination Equipment MAX Retry */
	sipHandler->term_retry_time = atoi(argv[9]);                          /*! SIP Termination Equipment MAX Retry Time */
	sipHandler->term_group_id = atoi(argv[10]);                           /*! SIP Termination Equipment COdec Group ID */
	sipHandler->term_cpg_id = atoi(argv[11]);                             /*! SIP Termination Equipment Capacity Group ID */
	sipHandler->term_media_proxy = strdup(argv[12]);                      /*! SIP Termination Equipment Media Proxy mode */
	sipHandler->term_privacy_method = strdup(argv[13]);                   /*! SIP Termination Equipment Privacy Method */ 
	sipHandler->term_codec_policy = strdup(argv[14]);                     /*! SIP Termination Equipment Codec Policy */
	sipHandler->term_source_number_type = strdup(argv[15]);               /*! SIP Termination Equipment Source Number Type */
	sipHandler->term_source_number_plan = strdup(argv[16]);               /*! SIP Termination Equipment Source Number Plan */
	sipHandler->term_signal_ip = strdup(argv[17]);                        /*! SIP Termination Equipment Signaling IP */
	sipHandler->term_signal_port = strdup(argv[18]);                      /*! SIP Termination Equipment Signaling Port */ 
	sipHandler->term_sip_call_reroute = strdup(argv[19]);                 /*! SIP Termination Equipment SIP Reroute Code Mapping */  
	sipHandler->term_itu_call_reroute = strdup(argv[20]);                 /*! SIP Termination Equipment ITUT Reroute Code Mapping */    
	sipHandler->term_sip_prevent_call_reroute = strdup(argv[21]);         /*! SIP Termination Equipment SIP Prevent Code Mapping */   
	sipHandler->term_itu_prevent_call_reroute = strdup(argv[22]);         /*! SIP Termination Equipment ITUT Prevent Code Mapping */  
	sipHandler->term_sip_call_retry = strdup(argv[23]);                   /*! SIP Termination Equipment SIP Retry Code Mapping */
	sipHandler->term_itu_call_retry = strdup(argv[24]);                   /*! SIP Termination Equipment ITUT Retry Code Mapping */
	
	//After Remove Capacity Group as mandatory  
	sipHandler->cpg_max_calls = 0;                           /*! SIP Termination Equipment Capacity Group MAX CALL */
	sipHandler->cpg_max_calls_sec = 0;                       /*! SIP Termination Equipment Capacity Group MAX CPS */ 
	
	return SWITCH_TERMINATOR_SUCCESS;
}



static int switch_capacitygp_callback(void *ptr, int argc, char **argv, char **col) 
{
	sip_termination_equipment_handler_st *sipHandler = (sip_termination_equipment_handler_st *)ptr;       /*! SIP Termination Structure */
	int index = 0;                                                                                        /*! Coloum Count */ 
	
	/**
	 * @Section Log on Console
	 */
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : %s\n", line);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : SIP TERMINATION EQUIPMENT CAPACITY GROUP INFORMATION \n");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : %s\n", line);

	for(index = 0; index < argc ; index++) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : %s : %s\n", col[index], argv[index]);
	}
	
	/*! ------------------------------------------------------------------------ */
	
	//After Remove Capacity Group as mandatory  
	sipHandler->cpg_max_calls = atoi(argv[0]);                           /*! SIP Termination Equipment Capacity Group MAX CALL */
	sipHandler->cpg_max_calls_sec = atoi(argv[1]);                       /*! SIP Termination Equipment Capacity Group MAX CPS */ 
	
	return SWITCH_TERMINATOR_SUCCESS;
}

static int switch_hangup_cause_to_sip(switch_call_cause_t cause)
{
        switch (cause) {
        case SWITCH_CAUSE_UNALLOCATED_NUMBER:
        case SWITCH_CAUSE_NO_ROUTE_TRANSIT_NET:
        case SWITCH_CAUSE_NO_ROUTE_DESTINATION:
                return 404;
        case SWITCH_CAUSE_USER_BUSY:
                return 486;
        case SWITCH_CAUSE_NO_USER_RESPONSE:
                return 408;
        case SWITCH_CAUSE_NO_ANSWER:
        case SWITCH_CAUSE_SUBSCRIBER_ABSENT:
                return 480;
        case SWITCH_CAUSE_CALL_REJECTED:
                return 603;
        case SWITCH_CAUSE_NUMBER_CHANGED:
        case SWITCH_CAUSE_REDIRECTION_TO_NEW_DESTINATION:
                return 410;
        case SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER:
        case SWITCH_CAUSE_INVALID_PROFILE:
                return 502;
        case SWITCH_CAUSE_INVALID_NUMBER_FORMAT:
        case SWITCH_CAUSE_INVALID_URL:
        case SWITCH_CAUSE_INVALID_GATEWAY:
                return 484;
        case SWITCH_CAUSE_FACILITY_REJECTED:
                return 501;
        case SWITCH_CAUSE_NORMAL_UNSPECIFIED:
                return 480;
        case SWITCH_CAUSE_REQUESTED_CHAN_UNAVAIL:
        case SWITCH_CAUSE_NORMAL_CIRCUIT_CONGESTION:
        case SWITCH_CAUSE_NETWORK_OUT_OF_ORDER:
        case SWITCH_CAUSE_NORMAL_TEMPORARY_FAILURE:
        case SWITCH_CAUSE_SWITCH_CONGESTION:
        case SWITCH_CAUSE_GATEWAY_DOWN:
                return 503;
        case SWITCH_CAUSE_OUTGOING_CALL_BARRED:
        case SWITCH_CAUSE_INCOMING_CALL_BARRED:
        case SWITCH_CAUSE_BEARERCAPABILITY_NOTAUTH:
                return 403;
        case SWITCH_CAUSE_BEARERCAPABILITY_NOTAVAIL:
                return 503;
        case SWITCH_CAUSE_BEARERCAPABILITY_NOTIMPL:
        case SWITCH_CAUSE_INCOMPATIBLE_DESTINATION:
                return 488;
        case SWITCH_CAUSE_FACILITY_NOT_IMPLEMENTED:
        case SWITCH_CAUSE_SERVICE_NOT_IMPLEMENTED:
                return 501;
        case SWITCH_CAUSE_RECOVERY_ON_TIMER_EXPIRE:
                return 504;
        case SWITCH_CAUSE_ORIGINATOR_CANCEL:
                return 487;
        case SWITCH_CAUSE_EXCHANGE_ROUTING_ERROR:
                return 483;
        default:
                return 503;        /*! Default 503 */
        }
}

/**
 * @Function @name switch_ivr_originate Bridge Origination Equipment To Termination Equipment
 * @param session
 * @param SIP Termination Equipment @var handler
 * @param Destination_number @var Destination number for Termination Equipment
 * @param source_number @var Source Number Of Termination Equipment
 * 
 */

static int switch_amaze_ivr_originate(switch_core_session_t *caller_session, sip_termination_equipment_handler_st *handler, char *destination_number, char *source_number, char *pcharge_info, int noa_flag)
{
	switch_status_t status = SWITCH_STATUS_FALSE;                                 /*! Switch Originate Call Status */   
	switch_core_session_t *callee_session;                                        /*! Switch Callee session */
	switch_call_cause_t cause = SWITCH_CAUSE_NORMAL_CLEARING;                     /*! Switch Hangup Cause */
	switch_channel_t *callee_channel = NULL;                                      /*! Switch Callee Channel */
	switch_channel_t *channel = switch_core_session_get_channel(caller_session);  /*! Switch Caller Channel */

	int try_done = 0;                                 /*! Done try */                        
	int progress_timeout = 0;                         /*! Progress Timeout */
	int vox_call_timeout = 0;                         /*! Origination Equipment Call Timeout */
	int drop_on_orig_progress_timeout = 0;            /*! Drop On Progress Timeout Flag */
	char *sofia_bridge = NULL;                        /*! Sofia Brideg Dial String */
	char *bypass_media = NULL;                        /*! By pass media */
	char *vox_bypass_media = NULL;                    /*! By pass media */ 
// 	char *from_sip_ip = NULL;                         /*! From SIP IP Address */
	char *from_sip_port = NULL;                       /*! From SIP IP Port */
	char *absolute_codec_string = NULL;               /*! Absolute codec string */
	const char *call_timeout = NULL;                        /*! Call Timeout */
	char *vox_orig_progress_timeout = NULL;           /*! Origination Equipment Progress timeout */
	char *privacy_method = NULL;                      /*! Privacy Method */
	char *vox_origination_equipment_id = NULL;        /*! Origination Equipment ID */
	char *sip_hangup_cause_code = NULL;               /*! SIP Hangup CODE */  
	char *q850_hangup_cause_code = NULL;              /*! ITUT Hangup Code */
	char *privacy_data = "id";                        /*! Privacy Data */
	char *sip_from_call_id = NULL;                    /*! SIP Call ID Of Origination */
	
// 	char *dp_id = (char *)switch_channel_get_variable(channel, "vox_dp_id");   /*! Dialpeer ID */
	switch_event_t *ovars;                            /*! SWITCH Event */
	
	char *sip_default_ip_port = NULL;                 /*! Opensips Default IP PORT */
	char *sip_sig_node_id = NULL;                     /*! SIP Signaling Node ID */
	char *sip_media_node_id = NULL;                   /*! SIP Media Node ID */ 
	
	char *default_time_timeout = NULL;
	
	
	/*! Getting Termination Default IP Port */
	sip_default_ip_port = (char *)switch_channel_get_variable(channel, "vox_termination_default_ipport");
	/*! Signaling NODE ID */
	sip_sig_node_id = (char *)switch_channel_get_variable(channel, "vox_termination_sig_node_id");
	/*! Media NODE ID */
	sip_media_node_id = (char *)switch_channel_get_variable(channel, "vox_termination_med_node_id");
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Opensips Default IP:PORT [ %s ]\n", sip_default_ip_port);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Signaling Node ID [ %s ]\n", sip_sig_node_id);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Media Node ID [ %s ]\n", sip_media_node_id);

	/**
	 * @Section getting value from channel variables.
	 */
	
	sip_from_call_id = (char *)switch_channel_get_variable(channel, "vox_originator_sip_call_id");
	vox_bypass_media = (char *)switch_channel_get_variable(channel, "vox_bypass_media");
	
	/**
	 * @var Check Termination Equipment Meida Mode. 
	 */
	
	if( (!strcmp(handler->term_media_proxy, "FLOW_AROUND")) && (!strcmp(vox_bypass_media,"true")) ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[AMAZE-TE] : Setting Bypass_media = true\n");
		bypass_media = switch_mprintf("true");
	} else {
		bypass_media = switch_mprintf("false");
	}


	/**
	 * @Section Getting value From Channel variables and setting for CDR report
	 */
	
	from_sip_port = (char *)switch_channel_get_variable(channel, "sip_received_port");
	vox_orig_progress_timeout = (char *)switch_channel_get_variable(channel, "vox_progress_timeout");
	call_timeout = switch_channel_get_variable(channel, "call_timeout");
	vox_origination_equipment_id = (char *)switch_channel_get_variable(channel, "vox_origination_equipment_id");
	
	switch_channel_export_variable(channel, "sip_received_ip", (char *)switch_channel_get_variable(channel, "sip_received_ip"), SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
	switch_channel_export_variable(channel, "sip_received_port", from_sip_port, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
	switch_channel_set_variable(channel, "originate_continue_on_timeout", "true");
	
	/**
	 * @var On Progress Time-out call action ----> REROUTE OR -----> ABORT
	 */
	
	if(!strcmp(handler->term_progress_timeout_action, "REROUTE") && atoi( handler->term_progress_timeout) > 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : SIP Termination Equipment [ %s ] On Progress Timeout [ REROUTE ] is Enable\n", handler->term_id);
		
// 		if(!zstr(vox_orig_progress_timeout)) {
			if(!zstr(handler->term_progress_timeout) &&  ( (zstr(vox_orig_progress_timeout) && atoi( handler->term_progress_timeout) > 0) || atoi(vox_orig_progress_timeout) > atoi( handler->term_progress_timeout)) ) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : SIP Termination Equipment [ %s ] Setting Progress Timeout [ %s ]\n", handler->term_id, handler->term_progress_timeout);  
				
				progress_timeout = atoi( handler->term_progress_timeout);
				switch_channel_set_variable(channel, "vox_orig_ring_tone", "no");

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : SIP Termination Equipment [ %s ] vox_orig_ring_tone [ no ]\n", handler->term_id); 

				switch_channel_set_variable(channel, "vox_reroute_on_next_termination_equipment", "yes");
			} else {
				char *vox_default_timeout = (char *)switch_channel_get_variable(channel, "vox_default_timeout");
				progress_timeout = atoi( vox_orig_progress_timeout);
				
				if( !zstr(vox_default_timeout) && !strcasecmp(vox_default_timeout, "yes")) {
					switch_channel_set_variable(channel, "vox_orig_ring_tone", NULL);
					drop_on_orig_progress_timeout = 1;
				}
				switch_channel_set_variable(channel, "vox_reroute_on_next_termination_equipment", "no");
			}
// 		}
		
		/**
		 * @var Set Call Time-out 
		 */
		
		if(call_timeout) {
			vox_call_timeout = atoi(call_timeout);
		}
	} else {
		char *vox_default_timeout = (char *)switch_channel_get_variable(channel, "vox_default_timeout");
		
		if(!zstr(handler->term_progress_timeout)) {
			// set call timeout  
			if(zstr(call_timeout) || !call_timeout || (atoi(call_timeout) > atoi(handler->term_progress_timeout))) {
				vox_call_timeout = atoi(handler->term_progress_timeout);
			} else {
				vox_call_timeout = 0;
			}
		} else {
			if(zstr(call_timeout) || !call_timeout) {
			  vox_call_timeout = 0;
			} else {
				vox_call_timeout = atoi(call_timeout);
			}
		}
		
		if(!zstr(vox_orig_progress_timeout)) {

			if(atoi(vox_orig_progress_timeout) > atoi( handler->term_progress_timeout) && atoi( handler->term_progress_timeout) != 0){
				progress_timeout = atoi( handler->term_progress_timeout);
				switch_channel_set_variable(channel, "vox_orig_ring_tone", "no");
			} else  {
				progress_timeout = atoi( vox_orig_progress_timeout);
			}
		} 
		
		if(!zstr(vox_default_timeout) && !strcasecmp(vox_default_timeout, "yes")) {
			switch_channel_set_variable(channel, "vox_orig_ring_tone", NULL);
			drop_on_orig_progress_timeout = 1;
		}
	}
	
	/*! Log on console */
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : SIP Termination Equipment [ %s ] MAX Call Duration [ %s ] and Origination Equipment Max Call Duration [ %s ]\n", handler->term_id, handler->term_max_call_dur, switch_channel_get_variable(channel, "vox_sched_seconds"));
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : SIP Termination Equipment [ %s ] IP:PORT [ %s ]\n", handler->term_id, sip_default_ip_port);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : SIP Termination Equipment [ %s ] privacy_method [ %s ]\n", handler->term_id, handler->term_privacy_method);
	absolute_codec_string = (char *)switch_channel_get_variable(channel, "switch_vox_sip_term_outbound_codec_prefs");

	/**
	 * @var Checking Termination Equipment Privacy Method.
	 */
	
	if(!strcmp(handler->term_privacy_method, "ASSERTED_ID")) {
		privacy_method = switch_mprintf("pid");
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : setting privacy_method [ %s ]\n", privacy_method);
	  
	} else if (!strcmp(handler->term_privacy_method, "CISCO_ID")) {
		privacy_method = switch_mprintf("rpid");
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : setting privacy_method [ %s ]\n", privacy_method);
	  
	} else if (!strcmp(handler->term_privacy_method, "PRES_ALLOW")) {
		privacy_method = switch_mprintf("rpid");
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : setting privacy_method [ %s ]\n", privacy_method);
		privacy_data = "hide_name:hide_number:screen";

	} else if (!strcmp(handler->term_privacy_method, "PRES_REST")) {
		privacy_method = switch_mprintf("rpid");
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : setting privacy_method [ %s ]\n", privacy_method);
		privacy_data = "NULL";
	}
	
	/**
	 * @var Setting Channel varibales for CDR Reporting. 
	 */
	
	switch_channel_set_variable(channel, "vox_terminator_ipaddress", handler->term_signal_ip);
	switch_channel_set_variable(channel, "vox_terminator_port", handler->term_signal_port);
	switch_channel_export_variable(channel, "vox_terminator_ipaddress", handler->term_signal_ip, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
	switch_channel_export_variable(channel, "vox_terminator_port", handler->term_signal_port, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
	switch_channel_export_variable(channel, "vox_term_equipment_ip_zone_id", sip_sig_node_id, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : SIP Termination Equipment [ %s ] Domain IP [FreeSwitch IP] [ %s ]\n",handler->term_id, switch_channel_get_variable(channel, "sip_local_network_addr"));
	
	/**
	 * @var Using Source Numbering Plan and Type Set P-Charge-Info header
	 */
	
	if(noa_flag) {
		pcharge_info = switch_mprintf("<sip:%s@%s>%s", source_number, switch_channel_get_variable(channel, "sip_local_network_addr"), pcharge_info);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : SIP Termination Equipment [ %s ] P-Charge-Info Header [ %s ]\n",handler->term_id, pcharge_info);
		switch_channel_set_variable(channel, "sip_h_P-Charge-Info", pcharge_info);
	}

	/**
	 * @var : Built Sofia String For Bridge Origination and Termination Equipment
	 */
	
        switch_event_create(&ovars, SWITCH_EVENT_REQUEST_PARAMS);

        switch_event_add_header(ovars, SWITCH_STACK_BOTTOM, "absolute_codec_string", "%s", absolute_codec_string);
        switch_event_add_header(ovars, SWITCH_STACK_BOTTOM, "progress_timeout", "%d", progress_timeout);
        switch_event_add_header(ovars, SWITCH_STACK_BOTTOM, "origination_privacy", "%s", privacy_data);
        switch_event_add_header(ovars, SWITCH_STACK_BOTTOM, "sip_cid_type", "%s", privacy_method);
        switch_event_add_header(ovars, SWITCH_STACK_BOTTOM, "vox_termination_equipment_media_proxy_mode", "%s", handler->term_media_proxy);
        switch_event_add_header(ovars, SWITCH_STACK_BOTTOM, "vox_termination_equipment_media_zone_id","%s", sip_media_node_id);
        switch_event_add_header(ovars, SWITCH_STACK_BOTTOM, "sdp_m_per_ptime", "false");
        switch_event_add_header(ovars, SWITCH_STACK_BOTTOM, "ignore_early_media", "true");

        switch_channel_export_variable(channel, "sdp_m_per_ptime",  "false", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
        switch_channel_export_variable(channel, "ignore_early_media",  "true", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
        switch_channel_export_variable(channel, "swicth_vox_transcoding", switch_str_nil(switch_channel_get_variable(channel, "swicth_vox_transcoding")), SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
        switch_channel_export_variable(channel, "vox_term_calling_number_plan", switch_str_nil(switch_channel_get_variable(channel, "vox_term_calling_number_plan")), SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
        switch_channel_export_variable(channel, "vox_term_calling_number_type", switch_str_nil(switch_channel_get_variable(channel, "vox_term_calling_number_type")), SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
        switch_channel_export_variable(channel, "vox_termination_equipment_id", switch_str_nil(switch_channel_get_variable(channel, "vox_termination_equipment_id")), SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);

        if(zstr(handler->term_max_call_dur) || atoi(handler->term_max_call_dur) <= 0) {
                switch_channel_export_variable(channel, "vox_term_sched_seconds",  globals.default_max_call_dur, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
                default_time_timeout = strdup(globals.default_max_call_dur);
        } else {
                switch_channel_export_variable(channel, "vox_term_sched_seconds", handler->term_max_call_dur, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
                default_time_timeout = strdup(handler->term_max_call_dur);
        }

        switch_channel_export_variable(channel, "vox_term_out_source_number", switch_str_nil(switch_channel_get_variable(channel, "vox_term_out_source_number")), SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
        switch_channel_export_variable(channel, "vox_term_out_destination_number", switch_str_nil(switch_channel_get_variable(channel, "vox_term_out_destination_number")), SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
        switch_channel_export_variable(channel, "vox_terminator_ipaddress", switch_str_nil(switch_channel_get_variable(channel, "vox_terminator_ipaddress")), SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
        switch_channel_export_variable(channel, "vox_terminator_port", switch_str_nil(switch_channel_get_variable(channel, "vox_terminator_port")), SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
            

        sofia_bridge = switch_mprintf("{vox_term_equipment_ip_zone_id='%s',vox_term_calling_number_type='%s',vox_term_calling_number_plan='%s',vox_terminator_port='%s',vox_termination_equipment_id='%s',vox_terminator_ipaddress='%s',vox_term_out_destination_number='%s',vox_term_out_source_number='%s' ,vox_termination_equipment_type='%s',vox_termination_equipment_media_zone_id='%s',vox_termination_equipment_media_proxy_mode='%s',sip_cid_type='%s',origination_privacy='%s',absolute_codec_string='%s',vox_term_sched_seconds='%s',execute_on_answer='execute_extension TERMINATION_SCHEDULE_HANGUP XML TERMINATION_SCHEDULE_HANGUP'}[bypass_media='%s' ,sip_h_VX-RL-IP='%s',sip_h_VX-RL-PT='%s',sip_h_VX-MD-ZN='%s',sip_h_VX-SIG-ZN='%s',sip_h_VX-MM='%s',leg_timeout='%d',rtp_timeout_sec='%s',progress_timeout='%d', sip_h_FROM-SERVER='%s']sofia/external/%s@%s;transport=UDP",sip_sig_node_id,switch_str_nil(switch_channel_get_variable(channel, "vox_term_calling_number_type")),switch_str_nil(switch_channel_get_variable(channel, "vox_term_calling_number_plan")),switch_str_nil(switch_channel_get_variable(channel, "vox_terminator_port")),switch_str_nil(switch_channel_get_variable(channel, "vox_termination_equipment_id")),switch_str_nil(switch_channel_get_variable(channel, "vox_terminator_ipaddress")),switch_str_nil(switch_channel_get_variable(channel, "vox_term_out_destination_number")),switch_str_nil(switch_channel_get_variable(channel, "vox_term_out_source_number")),switch_str_nil(switch_channel_get_variable(channel, "vox_termination_equipment_type")),sip_media_node_id,handler->term_media_proxy,privacy_method,privacy_data,absolute_codec_string, default_time_timeout, bypass_media, handler->term_signal_ip,handler->term_signal_port, sip_media_node_id,sip_sig_node_id, handler->term_media_proxy, vox_call_timeout, handler->term_rtp_time, progress_timeout,sip_from_call_id, destination_number, sip_default_ip_port);

        switch_channel_process_export(channel, NULL, ovars, "vox_export_vars");
        sofia_bridge = switch_channel_expand_variables(channel, sofia_bridge);
                    

	/*! Free Memory */
	switch_safe_free(bypass_media);
	switch_safe_free(privacy_method);

	/**
	 * @Section Log on console 
	 */
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5,"[AMAZE-TE] : SIP Termination Equipment [ %s ] Sofia String :\n%s\n", handler->term_id,sofia_bridge);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5,"[AMAZE-TE] : SIP Termination Equipment [ %s ] effective_caller_id_name [ %s ]\n", handler->term_id,source_number);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5,"[AMAZE-TE] : SIP Termination Equipment [ %s ] effective_caller_id_number [ %s ]\n", handler->term_id,source_number);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5,"[AMAZE-TE] : SIP Termination Equipment [ %s ] Origination Equipment Id [ %s ]\n", handler->term_id,vox_origination_equipment_id);
	
        //Active Call Entry
        
        
        
//         if(!strcmp(switch_str_nil(switch_channel_get_variable(channel, "active_call_status")) ,"empty")) {
           if(!strcmp(switch_str_nil(switch_channel_get_variable(channel, "call_is_from_origination")) ,"yes")) {
                char *sql = NULL;
                switch_caller_profile_t *originator_cp;
                originator_cp = switch_channel_get_caller_profile(channel);

                sql = switch_mprintf("INSERT INTO vca_active_call (ac_uuid, ac_orig_setup_time, ac_in_calling_number, ac_in_called_number, oe_id, te_id, dp_id ,ac_internal_in_call_id , ac_remote_orig_signalling_ip, ac_remote_term_signalling_ip ,ac_host_name, ac_local_orig_signal_ip, ac_local_term_signal_ip,ac_internal_call_id ,oe_desc, te_desc, ac_equipment_type) VALUE ('%q','%" SWITCH_TIME_T_FMT "','%q','%q','%q','%q','%q','%q', '%q', '%q:%q', '%q', '%q:%q','%q','%q','%q','%q','SIP_TERMINATION')", switch_str_nil(switch_channel_get_variable(channel, "uuid")),  originator_cp->times->profile_created, switch_str_nil(switch_channel_get_variable(channel, "sip_from_user")), switch_str_nil(switch_channel_get_variable(channel, "sip_to_user")),switch_str_nil(switch_channel_get_variable(channel, "vox_origination_equipment_id")),switch_str_nil(switch_channel_get_variable(channel, "vox_termination_equipment_id")),switch_str_nil(switch_channel_get_variable(channel, "vox_dp_id")), switch_str_nil(switch_channel_get_variable(channel, "sip_call_id")),switch_str_nil(switch_channel_get_variable(channel, "vox_originator_ipaddress")) , handler->term_signal_ip,handler->term_signal_port, switch_str_nil(switch_channel_get_variable(channel, "sip_local_network_addr")),switch_str_nil(switch_channel_get_variable(channel, "sip_received_ip")), switch_str_nil(switch_channel_get_variable(channel, "sip_received_port")),sip_default_ip_port , switch_str_nil(switch_channel_get_variable(channel, "sip_call_id")),switch_str_nil(switch_channel_get_variable(channel, "vox_origination_equipment_id")), switch_str_nil(switch_channel_get_variable(channel, "vox_termination_equipment_id")) );

                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Termination Equipment [ %s ] Create Active Call record SQL :: \n%s\n",handler->term_id, sql);
		switch_execute_sql(sql, globals.mutex);
		switch_safe_free(sql);
                
                // To Identify Call is from 
                switch_channel_set_variable(channel, "call_is_from_origination", "no");
                
                switch_channel_set_variable(channel, "active_call_status", "active");
                
//                 switch_channel_set_variable(channel, "first_term_equipment_id",switch_str_nil(switch_channel_get_variable(channel, "vox_termination_equipment_id")));
        } else {
                char *sql = NULL;
                
                
                //TODO UPDATE ALL PARAM HERE excluding origination
                sql = switch_mprintf("UPDATE  vca_active_call SET te_id = '%q',te_desc = '%q', ac_local_term_signal_ip = '%q',dp_id='%q',ac_in_calling_number ='%q', ac_in_called_number='%q', ac_remote_orig_signalling_ip='%q',ac_remote_term_signalling_ip='%q:%q' ,ac_host_name='%q', ac_local_orig_signal_ip='%q:%q' ",switch_str_nil(switch_channel_get_variable(channel, "vox_termination_equipment_id")),switch_str_nil(switch_channel_get_variable(channel, "vox_termination_equipment_id")),sip_default_ip_port ,switch_str_nil(switch_channel_get_variable(channel, "vox_dp_id")),switch_str_nil(switch_channel_get_variable(channel, "sip_from_user")), switch_str_nil(switch_channel_get_variable(channel, "sip_to_user")), switch_str_nil(switch_channel_get_variable(channel, "vox_originator_ipaddress")),handler->term_signal_ip,handler->term_signal_port, switch_str_nil(switch_channel_get_variable(channel, "sip_local_network_addr")),switch_str_nil(switch_channel_get_variable(channel, "sip_received_ip")), switch_str_nil(switch_channel_get_variable(channel, "sip_received_port")) );

                switch_execute_sql(sql, globals.mutex);
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : Updating Active Call SQL : \n%s\n", sql);
                switch_safe_free(sql);
        }
        
	/**
	 * @Function Call switch_ivr_originate To Bridge Origination And Termination Equipment.
	 */
	while(handler->term_max_retry >= try_done && (status != SWITCH_STATUS_SUCCESS) && switch_channel_up(channel) ) {
            
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : sofia_bridge :\n\n%s\n\n", sofia_bridge);
            
		/**
		 * @Function Sending call to Opensips 
		 */
	  
                    
		status = switch_ivr_originate(caller_session, &callee_session, &cause, sofia_bridge, vox_call_timeout, NULL, source_number, source_number, NULL, ovars, SOF_NONE, NULL, NULL);
		
		++try_done;              /*! Done Try increment */
		
		/**
		 * @Section Checking Call Status
		 */
		
		if(status != SWITCH_STATUS_SUCCESS) {
			char *ptr = NULL;                                     /*! Pointer variable */
			char *sql = NULL;                                     /*! SQL */
			char result[50] = "";                                 /*! SQL result */
			char *reroute_try_done = NULL;                        /*! Reroute Try Done */
			char *last_bridge_proto_specific_hangup_cause = NULL; /*! Last Bridge Hangup Code from channel variable */
			
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-TE] : SIP Termination Equipment [ %s ] Call Origination Failed [ %s ]\n", handler->term_id, switch_channel_cause2str(cause));
                        
			/**
			 * @Section Checking Hangup Cause is PROGRESS_TIMEOUT and SIP Termination has REROUTE Action on it than proceed to reroute
			 */
                        
                            if(!strcmp(switch_channel_cause2str(cause), "PROGRESS_TIMEOUT") && !strcmp(handler->term_progress_timeout_action, "REROUTE") && (drop_on_orig_progress_timeout == 0 ) && (( zstr(vox_orig_progress_timeout) && atoi( handler->term_progress_timeout) > 0 ) || ( atoi(vox_orig_progress_timeout) > atoi( handler->term_progress_timeout) ) ) ) {
                            
				char *idname = switch_mprintf("%d_cg_max_call", handler->term_cpg_id);
                                char *idname_te = switch_mprintf("%s_term_max_call", handler->term_id);   /*! Termination Equipment Max Call */
//                                 char *idname_te = switch_mprintf("%_cg_max_call", handler->term_id);
				int limit_status = SWITCH_STATUS_SUCCESS;      /*! Limit Status */
				if(handler->term_cpg_id > 0) {
					/*! Lon On Console */
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[AMAZE-TE] : SIP Termination Equipment [ %s ] Progress Timeout Occurse, so proceed for next SIP Termination Equipment\n", handler->term_id);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : SIP Termination Equipment [ %s ] Capacity Group [ %d ] MAX Limit  Decrease\n", handler->term_id, handler->term_cpg_id);
					
					/*! release hiredis resource */
					limit_status = switch_limit_release("hiredis", caller_session, globals.profile, idname);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : Releases Capacity Group [ %d ] MAX CALL LIMIT [ %s ] :: %d\n", handler->term_cpg_id, idname, limit_status);

					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : Decreament Capacity Group [ %d ] MAX CALL LIMIT [ %s ]\n", handler->term_cpg_id, idname);
				}
				
				if(handler->term_max_calls > 0 ) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : SIP Termination Equipment [ %s ]  MAX Limit  Decrease\n", handler->term_id);
					
					/*! release hiredis resource */
					limit_status = switch_limit_release("hiredis", caller_session, globals.profile, idname_te);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : Releases SIP Termination Equipment [ %s ]  MAX Limit [ %d ] hashkey [ %s ]\n", handler->term_id, limit_status,idname_te);
                                }
				
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : Proceed For SIP REROUTE ON PROGRESS TIMEOUT.\n");

				switch_channel_set_variable(channel, "vox_call_is_rerouted", "yes");
				switch_safe_free(idname);
                                
				return SWITCH_SIP_REROUTE;    /*! SIP Reroute */	
			}
			
			/*! SIP Retry Done */
			reroute_try_done = switch_mprintf("%d", try_done);
			switch_channel_export_variable(channel, "vox_reroute_retry", reroute_try_done, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
			switch_safe_free(reroute_try_done);
			
			/**
			 * @var SIP AND Q850 HANGUP CODES ON CALL HANGUP
			 */
			
			last_bridge_proto_specific_hangup_cause = (char *)switch_channel_get_variable(channel, "last_bridge_proto_specific_hangup_cause");
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : SIP Termination Equipment [ %s ] <last_bridge_proto_specific_hangup_cause> [ %s ]\n",handler->term_id, last_bridge_proto_specific_hangup_cause);
		
			 /*! If Last Bridge Hangup Time is Not Set Than Use from Hangup cause */ 
			 if(!zstr(last_bridge_proto_specific_hangup_cause))  {
				last_bridge_proto_specific_hangup_cause = (last_bridge_proto_specific_hangup_cause + 4);

				if(zstr(last_bridge_proto_specific_hangup_cause))  {
					sip_hangup_cause_code = switch_mprintf("%d", switch_hangup_cause_to_sip(cause));
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : SIP Termination Equipment [ %s ] <last_bridge_proto_specific_hangup_cause> Hangup Code [ %s ]\n",handler->term_id, last_bridge_proto_specific_hangup_cause);
					sip_hangup_cause_code = switch_mprintf("%s", last_bridge_proto_specific_hangup_cause);
				}
			} else {
				sip_hangup_cause_code = switch_mprintf("%d", switch_hangup_cause_to_sip(cause));
			}
			
			/*! Get ITUT Hangup Code From Hangup Cause */
			q850_hangup_cause_code = switch_mprintf("%d", (int)switch_channel_cause_q850(cause));
			
			/*! Log on Console */
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : SIP Termination Equipment [ %s ] sip_hangup_cause_code [ %s ]\n", handler->term_id, sip_hangup_cause_code);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : SIP Termination Equipment [ %s ] q850_hangup_cause_code [ %s ]\n", handler->term_id, q850_hangup_cause_code);

			/**
			 * @var Setting CHANNEL Variables For CDR Reporting
			 */
			
			switch_channel_export_variable(channel, "vox_termination_equipment_media_zone_id", sip_media_node_id, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
			switch_channel_export_variable(channel, "vox_sip_hangup_code", sip_hangup_cause_code, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
			
			// Hangup code mapping and respond with Hangup Code and Reason
			if(zstr(vox_origination_equipment_id)) {
				switch_channel_set_variable(channel, "switch_hangup_code", sip_hangup_cause_code);
				switch_channel_set_variable(channel, "switch_hangup_reason", switch_channel_cause2str(cause));
				switch_channel_set_variable(channel, "dialout_string", "SWITCH_HANGUP XML SWITCH_HANGUP");
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : SIP Termination Equipment [ %s ] SIP hangup code [ %s ]\n", handler->term_id, sip_hangup_cause_code);

				/**
				 * @Query : Get Origination Or Global SIP Hangup Code is mapped with Hangup code which get from TE.
				 * Respond With new SIP Code and Hangup Reason.
				 */
				
				sql = switch_mprintf("SELECT dc_code as map_code FROM  vca_disconnect_code_master WHERE dc_id=(SELECT a.dcm_map_code FROM vca_disconnect_code_mapping a, vca_disconnect_code_master b WHERE  a .dcm_orig_code= b.dc_id AND ( ( a.dcm_type = 'ORIGINATION' AND a.oe_id = '%s' AND b.dc_code = '%s') OR (( a.dcm_type = 'GLOBAL' AND a.oe_id = 0 AND b.dc_code = '%s' )) ) ORDER BY a.oe_id DESC LIMIT 1)", vox_origination_equipment_id, sip_hangup_cause_code, sip_hangup_cause_code);
				switch_execute_sql2str(NULL, sql, result, sizeof(result));
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5,"[AMAZE-TE] : SIP HANGUP CODE RESPOND SQL :: \n\n%s\n\n", sql);
				switch_safe_free(sql);
					
					if(zstr(result)) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5,"[AMAZE-TE] : SIP Hangup code [ %s ] does not mapped with any disconnect codec mapping.", sip_hangup_cause_code);
						
						sql = switch_mprintf("SELECT dc_code FROM vca_disconnect_code_master WHERE dc_type='SIP' AND dc_desc=(SELECT dc_desc as map_code FROM  vca_disconnect_code_master WHERE dc_id=(SELECT a.dcm_map_code FROM vca_disconnect_code_mapping a, vca_disconnect_code_master b WHERE  a .dcm_orig_code= b.dc_id AND ( ( a.dcm_type = 'ORIGINATION' AND a.oe_id = '%s' AND b.dc_code = '%s') OR (( a.dcm_type = 'GLOBAL' AND a.oe_id = 0 AND b.dc_code = '%s' )) ) ORDER BY a.oe_id DESC LIMIT 1))",vox_origination_equipment_id, q850_hangup_cause_code, q850_hangup_cause_code);
						switch_execute_sql2str(NULL, sql, result, sizeof(result));
						
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5,"[AMAZE-TE] : SIP Termination Equipment [ %s ] ITUT HANGUP CODE RESPOND SQL :: \n%s\n", handler->term_id, sql);
						switch_safe_free(sql);
						
						/**
						 * @var [ Auxilary Code ] Q850 Hangup Code respond To Origination Equipment
						 */
						if(zstr(result)) {
							switch_channel_set_variable(channel, "switch_hangup_code", sip_hangup_cause_code);
							switch_channel_set_variable(channel, "dialout_string", "SWITCH_HANGUP XML SWITCH_HANGUP");
						} else {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5,"[AMAZE-TE] : ITUT HANGUP CODE %s REPLACE BY HANGUP CODE : %s\n", q850_hangup_cause_code, result);
							switch_channel_export_variable(channel, "vox_aux_disconnect_code", result, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
							switch_channel_set_variable(channel, "switch_hangup_code", result);
							
							switch_channel_set_variable(channel, "dialout_string", "SWITCH_HANGUP XML SWITCH_HANGUP");
						}
					} else {
					  
					  /**
					   * @var [ Auxilary Code ] SIP Hangup Code respond To Origination Equipment
					   */
					  
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"[AMAZE-TE] :SIP Termination Equipment [ %s ]  SIP HANGUP CODE %s REPLACE BY HANGUP CODE [ %s ]\n", handler->term_id, sip_hangup_cause_code, result);

						switch_channel_export_variable(channel, "vox_aux_disconnect_code", result, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
						switch_channel_set_variable(channel, "switch_hangup_code", result);
						switch_channel_set_variable(channel, "dialout_string", "SWITCH_HANGUP XML SWITCH_HANGUP");
					}
			}
			
			/*! Kill Session From UUID */  
			switch_ivr_kill_uuid(switch_core_session_get_uuid(callee_session), cause);

			/**
			 * @Section Checking SIP Hangup Code is Found in Prevent Reroute List 
			 */
			
			if(!zstr(handler->term_sip_prevent_call_reroute)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"[AMAZE-TE] : SIP Termination Equipment [ %s ] SIP Prevent Reroute SIP Code Are [ %s ]\n",handler->term_id, handler->term_sip_prevent_call_reroute);
			
				/*! Check hangup Code is in SIP Prevent List */
				ptr = strstr(handler->term_sip_prevent_call_reroute, sip_hangup_cause_code);
				if(!ptr) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"[AMAZE-TE] : SIP Termination Equipment [ %s ] SIP Hangup Cause Code [ %s ] Does Not Found In SIP Prevent Code List.",handler->term_id, sip_hangup_cause_code);
				} else {
					  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"[AMAZE-TE] : SIP Termination Equipment [ %s ] SIP Hangup Cause Code %s Is Found In SIP Prevent Code List, so system will hangup this call", handler->term_id,sip_hangup_cause_code);
                                          
					  return SWITCH_SIP_PREVENT;
				}
				ptr = NULL;
			}
			
			/**
			 * @Section Checking ITUT Hangup Code is Found in Prevent Reroute List 
			 */
			
			if(!zstr(handler->term_itu_prevent_call_reroute)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"[AMAZE-TE] : SIP Termination Equipment [ %s ] ITU-T Prevent Re-Route Codes List Are :  %s\n",handler->term_id,handler->term_itu_prevent_call_reroute);
			
				/*! Checking Substring */
				ptr = strstr(handler->term_itu_prevent_call_reroute, q850_hangup_cause_code);
				if(!ptr) {
					  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"[AMAZE-TE] : SIP Termination Equipment [ %s ] ITU-T Q_850 Hangup Cause Code [ %s ] does not found ITU-T Q_850 prevent code list of SIP Termination Equipment.",handler->term_id, q850_hangup_cause_code);
				} else {
					  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"[AMAZE-TE] : SIP Termination Equipment [ %s ] ITU-T Q_850 Hangup Cause Code [ %s ] is found in ITU-T Q_850 prevent code list of SIP Termination Equipment, so system will hangup this call", handler->term_id,q850_hangup_cause_code);
                                          
					  return SWITCH_ITUT_PREVENT;
				}
			}
			
			/**
			 * @Section Checking SIP Hangup Code is Found in Reroute List 
			 */

			if(!zstr(handler->term_sip_call_reroute)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"[AMAZE-TE] : SIP Termination Equipment [ %s ] SIP Re-Route Codes Are : %s\n",handler->term_id,handler->term_sip_call_reroute);
			
				/**
				* @var Checking SIP Hangup code is mapped with SIP Reroute code of SIP Termination Equipment
				* @action --> Try to another SIP Equipment if found otherwise Terminate it.
				*/
				
				ptr = strstr(handler->term_sip_call_reroute, sip_hangup_cause_code);
				if(!ptr) {
					  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"[AMAZE-TE] : SIP Termination Equipment [ %s ] SIP Hangup Cause Code [ %s ] does not found in sip reroute code list.",handler->term_id, sip_hangup_cause_code);
				} else {
// 					char result[50] = "";                                                   /*! Query Result */
					char *idname = switch_mprintf("%d_cg_max_call", handler->term_cpg_id);  /*! Capacity Group Max Call Key */
					 char *idname_te = switch_mprintf("%s_term_max_call", handler->term_id);   /*! Termination Equipment Max Call */
                                        int limit_status = SWITCH_STATUS_SUCCESS;

						/*! Log On Console */
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"[AMAZE-TE] : SIP Termination Equipment [ %s ] SIP Hangup Cause  Code [ %s ] is found in SIP RE-ROUTE Code List of SIP Termination Equipment, So system will try on another SIP Termination Equipment\n", handler->term_id,sip_hangup_cause_code);
					
					switch_channel_export_variable(channel, "vox_sip_reroute_reason", "HANGUP_CODE_MAPPED_IN_SIP_REROUTE", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
					
					switch_safe_free(sip_hangup_cause_code);
					switch_safe_free(q850_hangup_cause_code);
					
					if(handler->term_cpg_id > 0) {
					    
						/*! Log On Console */
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : SIP Termination Equipment [ %s ] Capacity Group [ %d ] MAX Limit  Decrease\n", handler->term_id, handler->term_cpg_id);

						/*! Release Redis Key value from Session */
						limit_status = switch_limit_release("hiredis", caller_session, globals.profile, idname);
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : Releases Capacity Group [ %d ] MAX CALL LIMIT [ %s ] :: %d\n", handler->term_cpg_id, idname, limit_status);

						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : Decreament Capacity Group [ %d ] MAX CALL LIMIT [ %s ]\n", handler->term_cpg_id, idname);
					    
					}
					switch_safe_free(idname);
                                        
                                        if(handler->term_max_calls > 0 ) {
                                                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : SIP Termination Equipment [ %s ]  MAX Limit  Decrease\n", handler->term_id);
                                                
                                                /*! release hiredis resource */
                                                limit_status = switch_limit_release("hiredis", caller_session, globals.profile, idname_te);
                                                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : Releases SIP Termination Equipment [ %s ]  MAX Limit [ %d ] hashkey [ %s ]\n", handler->term_id, limit_status,idname_te);
                                        }
                                        switch_safe_free(idname_te);

					
					/*! SIP Reroute is Set*/
					switch_channel_set_variable(channel, "vox_call_is_rerouted", "yes");
					
					return SWITCH_SIP_REROUTE;  /*! Proceed For SIP Reroute */
				}
			}
			
			/**
			 * @Section ITUT SIP Termination Reroute Checking
			 */
			
			if(!zstr(handler->term_itu_call_reroute)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"[AMAZE-TE] : SIP Termination Equipment [ %s ] ITU-T Call Re-Route Codes List Are [ %s ]\n",handler->term_id,handler->term_itu_call_reroute);
				ptr = NULL;
				
				/*! Checking Substring of Hangup code in ITUT reroute List*/
				ptr = strstr(handler->term_itu_call_reroute, q850_hangup_cause_code);
				if(!ptr) {
					  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"[AMAZE-TE] : SIP Termination Equipment [ %s ] ITU-T Q_850 Hangup Code %s does not found in ITU-T Q_850 RE-ROUTE code list.", handler->term_id, q850_hangup_cause_code);
				} else {
// 					char result[50] = "";                /*! SQL Result */
                                        int limit_status = SWITCH_STATUS_SUCCESS;
					char *idname = switch_mprintf("%d_cg_max_call", handler->term_cpg_id);     /*! Capacity Group Id*/
                                        char *idname_te = switch_mprintf("%s_term_max_call", handler->term_id);   /*! Termination Equipment Max Call */
					/*! Log on Console */
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"[AMAZE-TE] : SIP Termination Equipment [ %s ] ITU-T Q_850 Hangup Cause Code %s is found in ITU-T Q_850 RE-ROUTE Code List, So system will try on another SIP Termination Equipment if there is available another sip termination equipment\n", handler->term_id, q850_hangup_cause_code);
					switch_channel_export_variable(channel, "vox_sip_reroute_reason", "HANGUP_CODE_MAPPED_IN_ITUT_REROUTE", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
					switch_safe_free(sip_hangup_cause_code);
					switch_safe_free(q850_hangup_cause_code);

					if(handler->term_cpg_id > 0) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : SIP Termination Equipment [ %s ] Capacity Group [ %d ] MAX Limit  Decrease\n", handler->term_id, handler->term_cpg_id);

						/*! Release Hiredis resource Limit */
						limit_status = switch_limit_release("hiredis", caller_session, globals.profile, idname);
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : Releases Capacity Group [ %d ] MAX CALL LIMIT [ %s ] [ %d ]\n", handler->term_cpg_id, idname, limit_status);
						
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : Decreament Capacity Group [ %d ] MAX CALL LIMIT [ %s ]\n", handler->term_cpg_id, idname);
					}
					switch_safe_free(idname);
					
                                        if(handler->term_max_calls > 0 ) {
                                                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : SIP Termination Equipment [ %s ]  MAX Limit  Decrease\n", handler->term_id);
                                                
                                                /*! release hiredis resource */
                                                limit_status = switch_limit_release("hiredis", caller_session, globals.profile, idname_te);
                                                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : Releases SIP Termination Equipment [ %s ]  MAX Limit [ %d ] hashkey [ %s ]\n", handler->term_id, limit_status,idname_te);
                                        }
                                        switch_safe_free(idname_te);

                                        
					
					//for check call is re-routed
					switch_channel_set_variable(channel, "vox_call_is_rerouted", "yes");
					
					return SWITCH_ITUT_REROUTE;   /*! Reroute This Call */
				}
			}
			
			/**
			 * @Section SIP Call Retry On Hangup Code is mapped in Retry List
			 */
			
			if(!zstr(handler->term_sip_call_retry)) {
				//Retry Logic
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"[AMAZE-TE] : SIP Termination Equipment [ %s ] SIP Call Retry Codes List Are [ %s ]\n",handler->term_id, handler->term_sip_call_retry);
				
				/*! Find Sub string */
				ptr = strstr(handler->term_sip_call_retry, sip_hangup_cause_code);
				if(!ptr) {
					  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"[AMAZE-TE] : SIP Termination Equipment [ %s ] SIP Hangup Cause Code [ %s ] does not found in SIP Retry Code List.", handler->term_id,sip_hangup_cause_code);
				} else {
					  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"[AMAZE-TE] : SIP Termination Equipment [ %s ] SIP Hangup Cause Code [ %s ] is found in SIP Retry Codes List, So System Will Retry On Same SIP Termination Equipment\n",handler->term_id, sip_hangup_cause_code);
					  switch_safe_free(sip_hangup_cause_code);
					  switch_safe_free(q850_hangup_cause_code);
					  
					  /*! Log On COnsole For Retry */
					  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"[AMAZE-TE] : SIP Termination Equipment [ %s ] Proceed To Originate attempt try %d/%d retry interval %d seconds\n",  handler->term_id,try_done, handler->term_max_retry, handler->term_retry_time);
					  switch_channel_export_variable(channel, "vox_sip_reroute_reason", "HANGUP_CODE_MAPPED_IN_SIP_RETRY_ROUTE", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
                                          
                                          if(handler->term_retry_time > 0) {
                                                /*! Wait for retry route */
                                                switch_yield(handler->term_retry_time*1000000);
                                          }
					  continue;
				}
			}
			
			/**
			 * @Section Checking Hangup Code is mapped with any ITUT Code List.
			 */
			
			if(!zstr(handler->term_itu_call_retry)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"[AMAZE-TE] : SIP Termination Equipment [ %s ] ITUT Call Retry Code [ %s ]\n", handler->term_id,handler->term_itu_call_retry);
				
				ptr = NULL;
				/*! Find Sub string in ITUT Hangup Code List */
				ptr = strstr(handler->term_itu_call_retry, q850_hangup_cause_code);
				if(!ptr) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"[AMAZE-TE] : SIP Termination Equipment [ %s ] ITU(Q850) Hangup Code [ %s ] does not found in ITU(Q850) Retry code list.",handler->term_id, q850_hangup_cause_code);
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"[AMAZE-TE] : SIP Termination Equipment [ %s ] ITU(Q850) Hangup Code [ %s ] is found in ITU(Q850) Retry Code List, system will retry on same sip termination equipmet\n",handler->term_id, q850_hangup_cause_code);
					switch_safe_free(sip_hangup_cause_code);
					switch_safe_free(q850_hangup_cause_code);
					  
					switch_channel_export_variable(channel, "vox_sip_reroute_reason", "HANGUP_CODE_MAPPED_IN_ITUT_RETRY_ROUTE", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
					  
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"[AMAZE-TE] : SIP Termination Equipment [ %s ] Proceed To Originate attempt try %d/%d retry interval %d seconds\n", handler->term_id,try_done, handler->term_max_retry, handler->term_retry_time);
                                        
                                        if(handler->term_retry_time > 0) {
                                                /*! Wait For Retry Duration */
                                                switch_yield(handler->term_retry_time*1000000);
                                        }
					continue;
				}
			}
			
			
// 			if(!strcmp(switch_str_nil(switch_channel_get_variable(channel, "redirection_to_termination")),"yes")) {
//                             switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : This Call is From SIP Redirect Application\n");
//                             
//                         }
//                         
// 			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"[AMAZE-TE] : Remove Active Call Entry\n");
// 			sql = switch_mprintf("DELETE FROM vca_active_call WHERE ac_uuid='%s'", switch_str_nil(switch_channel_get_variable(channel, "uuid")));
// 			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : SIP Termination Equipment [ %s ] Delete Active Call Record SQL :: %s\n", handler->term_id,sql);
// 			switch_execute_sql(sql, globals.mutex);
// 			switch_safe_free(sql);
			
                        
                        
			
			return SWITCH_TERMINATOR_SUCCESS;
		} else {
			char *sip_hangup_disposition = NULL;                    /*! Hangup Disposition */
			switch_caller_profile_t *originator_cp, *originatee_cp;  /*! Call Profile */ 
			char ori_profile_created[80] = "", term_profile_created[80] = "", ori_answered[80]="", term_answered[80]=""; /*! Call Times*/
			char *sql = NULL;     /*! SQL */
			const char *orig_uuid = NULL, *term_uuid = NULL, *orig_sip_from_user = NULL, *term_sip_from_user = NULL;
			const char *orig_sip_to_user = NULL, *term_sip_to_user = NULL, *ori_sip_call_id = NULL, *term_sip_call_id = NULL;
			const char *orig_sip_from_host = NULL, *term_sip_from_host = NULL, *vox_host_name = NULL;
			const char *vox_dp_id = NULL, *vox_orig_id = NULL, *vox_term_id = NULL;
			const char *vox_sip_received_ip = NULL, *vox_sip_received_port = NULL;
                        
                        
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"[AMAZE-TE] : SIP Termination Equipment [ %s ] Bridge Call Successfully\n", handler->term_id);
			
			/*! initialize variables*/
			callee_channel = switch_core_session_get_channel(callee_session);
			originator_cp = switch_channel_get_caller_profile(channel);
			originatee_cp = switch_channel_get_caller_profile(callee_channel);
			
                        switch_channel_export_variable(callee_channel, "cdr_call_flag", "1", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
                        
                        switch_channel_export_variable(callee_channel, "reroute_on_dialpeer", "no", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
			switch_channel_export_variable(channel, "reroute_on_dialpeer", "no", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
			switch_channel_export_variable(channel, "cdr_call_flag", "1", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
                        
                        switch_channel_export_variable(callee_channel, "rerouting_is", "disable", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
                         
			
			/*! Origination Equipmet Setup Time */
			switch_snprintf(ori_profile_created, sizeof(ori_profile_created), "%" SWITCH_TIME_T_FMT, originator_cp->times->profile_created);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Origination Equipment Setup Time [ %s ]\n", ori_profile_created);
			
			/*! Termination Equipmet Setup Time */
			switch_snprintf(term_profile_created, sizeof(term_profile_created), "%" SWITCH_TIME_T_FMT, originatee_cp->times->profile_created);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Termination Equipment Setup Time [ %s ]\n", term_profile_created);

			/*! Origination Equipmet Answer Time */
			switch_snprintf(ori_answered, sizeof(ori_answered), "%" SWITCH_TIME_T_FMT, originator_cp->times->answered);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Origination Equipment Answer Time [ %s ]\n", ori_answered);
			
			/*! Termination Equipmet Answer Time */
			switch_snprintf(term_answered, sizeof(term_answered), "%" SWITCH_TIME_T_FMT, originatee_cp->times->answered);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Termination Equipment Answer Time [ %s ]\n", term_answered);

			/*! Termination Equipmet SIP from User */
			term_sip_from_user = (char *)switch_channel_get_variable(callee_channel, "sip_from_user");
			//ac_out_calling_number
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Termination Equipment  sip_from_user [ %s ]\n", term_sip_from_user);
			
			/*! Origination Equipmet SIP from User */
			orig_sip_from_user = (char *)switch_channel_get_variable(channel, "sip_from_user");
			//ac_in_calling_number
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Origination Equipment sip_from_user [ %s ]\n", orig_sip_from_user);
			
			/*! Termination Equipmet SIP TO User */
			term_sip_to_user = (char *)switch_channel_get_variable(callee_channel, "sip_to_user");
			//ac_out_called_number
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Termination Equipment  sip_to_user [ %s ]\n", term_sip_to_user);
			
			/*! Origination Equipmet SIP To User */
			orig_sip_to_user = (char *)switch_channel_get_variable(channel, "sip_to_user");
			//ac_in_called_number
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Origination Equipment sip_to_user [ %s ]\n", orig_sip_to_user);
			term_sip_call_id = (char *)switch_channel_get_variable(callee_channel, "sip_call_id");

			//ac_internal_in_call_id
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Termination Equipment  sip_call_id [ %s ]\n", term_sip_call_id);
			ori_sip_call_id =  (char *)switch_channel_get_variable(channel, "sip_call_id");

			//ac_internal_out_call_id
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Origination Equipment sip_call_id [ %s ]\n",ori_sip_call_id);
			term_sip_from_host = (char *)switch_channel_get_variable(callee_channel, "sip_from_host");

			//ac_remote_term_signalling_ip
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Termination Equipment  sip_from_host [ %s ]\n", term_sip_from_host);
			orig_sip_from_host = (char *)switch_channel_get_variable(channel, "vox_originator_ipaddress");

			//ac_remote_orig_signalling_ip
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Origination Equipment sip_from_host [ %s ]\n", orig_sip_from_host);
			
			/*! Origination Equipmet Call UUID */
			orig_uuid = (char 	*)switch_channel_get_variable(channel, "uuid");
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Origination Equipment uuid [ %s ]\n", orig_uuid);
			
			/*! Termination Equipmet Call UUID */
			term_uuid = (char *)switch_channel_get_variable(callee_channel, "uuid");
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Termination Equipment uuid [ %s ]\n", term_uuid);
			
			/*! Dialpeer ID */
			vox_dp_id = (char *)switch_channel_get_variable(channel, "vox_dp_id");
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Termination Equipment Dial peer ID [ %s ]\n", vox_dp_id);
			
			/*! Origination Equipmennt ID */
			vox_orig_id = (char *)switch_channel_get_variable(channel, "vox_origination_equipment_id");
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Origination Equipment ID [ %s ]\n", vox_orig_id);
			
			/*! Termination Equipmennt ID */
			vox_term_id = (char *)switch_channel_get_variable(channel, "vox_termination_equipment_id");
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Termination Equipment ID [ %s ]\n", vox_term_id);
			
			/*! Hostname */
			vox_host_name = (char *)switch_channel_get_variable(channel, "sip_local_network_addr");
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : FreeSwitch Host Name [IP] [ %s ]\n", vox_host_name);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Termination Equipment sip_received_ip [ %s ]\n", switch_channel_get_variable(callee_channel, "sip_received_ip"));
			
			/*! SIP User Agent */
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Termination Equipment sip_user_agent [ %s ]\n", switch_channel_get_variable(channel, "sip_user_agent"));
			
			/*! SIP Received IP */
			vox_sip_received_ip = (char *)switch_channel_get_variable(channel, "sip_received_ip");
			/*! SIP Received PORT */
			vox_sip_received_port = (char *)switch_channel_get_variable(channel, "sip_received_port");
			
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Origination Equipment vox_sip_received_ip [ %s ]\n", vox_sip_received_ip);
			
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Termination Equipment vox_sip_received_port [ %s ]\n", vox_sip_received_port);
                        
                        
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Origination Equipment vox_termination_equipment_type [ %s ]\n", switch_channel_get_variable(channel, "vox_termination_equipment_type"));
			
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Termination Equipment vox_termination_equipment_type [ %s ]\n", switch_channel_get_variable(callee_channel, "vox_termination_equipment_type"));
                        
                        sql = switch_mprintf("UPDATE vca_active_call SET ac_call_status = 'ANSWER', ac_orig_connect_time= '%" SWITCH_TIME_T_FMT "',ac_term_setup_time = '%" SWITCH_TIME_T_FMT "',ac_term_connect_time = '%" SWITCH_TIME_T_FMT "',ac_out_calling_number='%q',ac_out_called_number='%q',ac_internal_out_call_id='%q' WHERE ac_uuid = '%q' " , originator_cp->times->answered, originatee_cp->times->profile_created, originatee_cp->times->answered, switch_str_nil(switch_channel_get_variable(callee_channel, "sip_from_user")), switch_str_nil(switch_channel_get_variable(callee_channel, "sip_to_user")),switch_str_nil(switch_channel_get_variable(callee_channel, "sip_call_id")),switch_str_nil(switch_channel_get_variable(channel, "uuid")));
                        switch_execute_sql(sql, globals.mutex);
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Updating Active Call SQL : \n%s\n", sql);
                        switch_safe_free(sql);
                                        

			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : SIP Termination Equipment [ %s ] UUID [ %s ]\n",handler->term_id, switch_core_session_get_uuid(callee_session));
			
			/**
			 * @Section Setting Channel Variables For CDR and Reporting
			 */
			
			switch_channel_export_variable(channel, "vox_sip_hangup_code", "200", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
			switch_channel_export_variable(channel, "vox_termination_equipment_media_zone_id", sip_media_node_id, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
			switch_channel_export_variable(channel, "vox_terminator_call_uuid", switch_core_session_get_uuid(callee_session), SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
			switch_channel_export_variable(channel, "vox_term_sip_call_id",switch_channel_get_variable(callee_channel, "sip_call_id"), SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
			switch_channel_set_variable(callee_channel, "vox_terminator_call_uuid", switch_core_session_get_uuid(callee_session));
			switch_channel_set_variable(callee_channel, "vox_term_sip_call_id", switch_channel_get_variable(callee_channel, "sip_call_id"));
			
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : SIP Termination Equipment [ %s ] call ID <sip_call_id> [ %s ]\n", handler->term_id,(char *)switch_channel_get_variable(callee_channel, "sip_call_id"));

			/**
			 * @Section Bridge Origination And termibation Equipment  
			 */
			
			switch_ivr_multi_threaded_bridge(caller_session, callee_session, NULL, NULL, NULL);
			switch_core_session_rwunlock(callee_session);

			/**
			 * @Wait For Call Hangup
			 */
			while(switch_channel_up(callee_channel)) {
				switch_yield(100000);
			}

			/**
			 * @Delete Active Call Record From DB 
			 */
			
// 			sql = switch_mprintf("DELETE FROM vca_active_call WHERE ac_uuid='%s'", switch_str_nil(switch_channel_get_variable(channel, "uuid")));
// 			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : SIP Termination Equipment [ %s ] Delete Active Call Record SQL :: %s\n", handler->term_id,sql);
// 			switch_execute_sql(sql, globals.mutex);
// 			switch_safe_free(sql);

			/*! Delay SIP Origination Equipment By time */
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : vox_delay_bye_time [ %s ]\n", (char *)switch_channel_get_variable(callee_channel, "vox_delay_bye_time"));
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : SIP Termination Equipment [ %s ] Origination LEG sip_hangup_disposition [ %s ]\n", handler->term_id, (char *)switch_channel_get_variable(channel, "sip_hangup_disposition"));
			sip_hangup_disposition = (char *)switch_channel_get_variable(callee_channel, "sip_hangup_disposition");
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : SIP Termination Equipment [ %s ] Termination LEG sip_hangup_disposition [ %s ]\n", handler->term_id,sip_hangup_disposition);
			
			/*! On Success Send 200 Ok*/
			switch_channel_set_variable(channel, "switch_hangup_code", "200");
			switch_channel_set_variable(channel, "dialout_string", "hangup XML HANGUP");
				
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"[AMAZE-TE] : SIP Termination Equipment [ %s ] call is completed.\n", handler->term_id);
			
			/*! Return Success */
			return SWITCH_TERMINATOR_SUCCESS;
		}  
	}
	switch_event_destroy(&ovars);
	switch_safe_free(sofia_bridge);
	return SWITCH_TERMINATOR_FAILED;
}

/**
 * @Function Wait For Socket Connection For Percentage Base Equipment Find
 */

int amaze_wait_sock(int sock, uint32_t ms, int flags)
{
 	int s = 0, r = 0;     
	fd_set rfds; 
	fd_set wfds;
	fd_set efds;
	struct timeval tv;

    if (sock < 0) {
	  return -1;
    }

	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	FD_ZERO(&efds); 
 	
	if ((flags & 1)) {//For Read
		FD_SET(sock, &rfds);
	}
	
	if ((flags & 2)) {  //For write
		FD_SET(sock, &wfds);
	}
	
	if ((flags & 3)) { //For Error
		FD_SET(sock, &efds);
	}

	tv.tv_sec = ms / 1000;
	tv.tv_usec = (ms % 1000) * ms;
	
	s = select(sock + 1, (flags & 1) ? &rfds : NULL, (flags & 2) ? &wfds : NULL, (flags & 3) ? &efds : NULL, &tv);
	if (s < 0) {
		r = s;
	} else if (s > 0) {
		if ((flags & 1) && FD_ISSET(sock, &rfds)) {
			r |= 1;
		}

		if ((flags & 2) && FD_ISSET(sock, &wfds)) {
			r |= 2;
		}

		if ((flags & 3) && FD_ISSET(sock, &efds)) {
			r |= 3;
		}
	}

	return r;
}

/**
 * @function Get SIP Termination Equipment ID
 */

static char* get_termination_equipment_id(int RequestId, int flag)
{
	struct sockaddr_in addr;          /*! Socket Address */
	int sfd = 0;                      /*! Socket Descriptore */
	int fd_flags = 0;                 /*! Socket Flag */
	uint32_t timeout = 30;            /*! Socket Connection Timeout */
	int rval = 0;                     /*! return Value */
	unsigned short portno1 = 0;       /*! Port Number */
	int x = 1;                        /*! variable */
	char RequestData[25] = "";        /*! Data buffer */
	char term_id[50] = "";            /*! SIP Termination Equipmet ID */
	char *receiveData = NULL;         /*! Request Data */

	
	/*! Flag = 1 Means Get Termination ID + Signaling Serevr IDS */
	if(flag == 1) {
		sprintf(RequestData,"GETTEID#%d", RequestId); /*! Get Termination Equipment ID */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Request DialPeer ID [ %s ]\n", RequestData);
	} else {
		/*! Flag = 0 means Get Only Signaling Server IDS */
		sprintf(RequestData,"GETSGID#%d", RequestId); /*! Get Termination Equipment Signaling Server ID */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Request SIP Termination Equipment ID [ %s ]\n", RequestData);
	}
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Proceed To Create A Socket For Get SIP Termination Equipment ID From [ %s:%d ].\n", globals.bind_ipaddress, globals.bind_ipport);

	/**
	 * @Create a Socket  
	 */
	
	sfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(sfd < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-TE] : SIP Termination Application Unable To Create a Socket [ %s:%d ].\n", globals.bind_ipaddress, globals.bind_ipport);
		goto fail;
    }
	
	/**
	 * @Section Checking Timeout of connection
	 */
	
	if (timeout) {
		fd_flags = fcntl(sfd, F_GETFL, 0);
		if (fcntl(sfd, F_SETFL, fd_flags | O_NONBLOCK)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-TE] : SIP Termination Application Unable To Connect On [ %s:%d ].\n", globals.bind_ipaddress, globals.bind_ipport);
			goto fail;
		}
	}
	
	/**
	 * @Section initialize Socket Structure  
	 */
	
  	addr.sin_family = AF_INET;                                             /*! Socket family */
	portno1 = globals.bind_ipport;                                         /*! Socket Bind Port */
	addr.sin_port = htons(portno1);                                        /*! Socket Bind port */
	addr.sin_addr.s_addr = inet_addr(globals.bind_ipaddress);              /*! Socket Bind IP Address */ 

	/**
	 * @Section Connecting Server 
	 */
	
	rval = connect(sfd,(const struct sockaddr *)&addr,sizeof(addr));
	if (timeout) {
		int r;
		r = amaze_wait_sock(sfd, timeout, 2);
		if (r <= 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-TE] : SIP Termination Connection Timeout On [ %s:%d ].\n", globals.bind_ipaddress, globals.bind_ipport);
			goto fail;
		}
		if (!(r & 2)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-TE] : SIP Termination Connection Timeout On [ %s:%d ].\n", globals.bind_ipaddress, globals.bind_ipport);
			goto fail;
		}
		fcntl(sfd, F_SETFL, fd_flags);
		rval = 0;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : SIP Termination Equipment Proceed To Connect On [ %s:%d ] and rval [%d].\n", globals.bind_ipaddress, globals.bind_ipport, rval);
	}

	/*! Setting Socket option*/
	setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, &x, sizeof(x));

	/**
	 * @Section validating RequestData 
	 */
	
	switch_mutex_lock(globals.mutex);
	if(send(sfd, RequestData, strlen(RequestData), 0) != strlen(RequestData)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : SIP Termination Equipment is Unable To Send Request On [ %s:%d ].\n", globals.bind_ipaddress, globals.bind_ipport);
		switch_mutex_unlock(globals.mutex);
		goto fail;
	}
	switch_mutex_unlock(globals.mutex);

	switch_mutex_lock(globals.mutex);
	memset(&term_id, 0, sizeof(term_id));

	/*! Read SIP Termination ID*/
	recv(sfd,term_id,sizeof(term_id),0);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Termination Get ID [ %s ] From [ %s:%d ].\n",term_id, globals.bind_ipaddress, globals.bind_ipport);
	
	switch_mutex_unlock(globals.mutex);
	close(sfd);    /*! Close Socket */
	
	if(zstr(term_id)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Termination Get Invalid Data From [ %s:%d ].\n", globals.bind_ipaddress, globals.bind_ipport);
	}
	
	receiveData = switch_mprintf("%s", term_id);
	
fail:
	close(sfd);            /*! Close Socket Descriptore */
	return receiveData;
}

/**
 * @function Managing SIP Termination Equipments
 */

static int switch_manage_termination_equipment(int term_id, switch_channel_t *channel, switch_core_session_t *session, char *source_number, char *destination_number)
{
	char *sql  = NULL;                                 /*! SQL */
	sip_termination_equipment_handler_st sip_handler;  /*! SIP Termination Equipment Handler */
	regex_master_st regex_master;                      /*! SIP Termination Equipment Regex Handler */
	char *ep_codec_string = NULL;                      /*! SIP read EPIC Codec String */
	char *noa = NULL;                                  /*! Nature of address */
	char *npi = NULL;                                  /*! numbering Plan indicator */
	char *pcharge_info = NULL;                         /*! p-charge Header */ 
	char *tmp_num = NULL;                              /*! Temp variable */
	int noa_flag = 1;                                  /*! noa flag */
	char resbuf[400] = "";                             /*! Query result */    
	
	/**
	 * @Section Proceed To Get SIP Termination Information
	 */
	
	memset(&sip_handler, 0, sizeof(sip_handler));
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : Proceed To Get SIP Termination Equipment [ %d ] Info.\n", term_id);
	if(term_id == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-TE] : Termination Application Is Not Able To Get SIP Termination Equipment Info.\n");
		goto end;
	}
	
	/**
	 * @Query To Get SIP Termination Equipment info 
	 */

	sql = switch_mprintf("SELECT a.term_id, IF(isnull(a.term_max_call_dur),0,a.term_max_call_dur) as term_max_call_dur , IF(isnull(a.term_max_calls),0,a.term_max_calls) as term_max_calls ,IF(isnull(a.term_max_calls_sec),0,a.term_max_calls_sec) as term_max_calls_sec, (IF(isnull(a.term_progress_timeout),0,a.term_progress_timeout)/1000) as term_progress_timeout, IF(isnull(term_progress_timeout_action),NULL, term_progress_timeout_action) as term_progress_timeout_action, IF(isnull(a.term_session_time),0,a.term_session_time) as term_session_time , IF(isnull(a.term_rtp_time),0,a.term_rtp_time) as term_rtp_time, IF(isnull(a.term_max_retry),0,a.term_max_retry) as term_max_retry, IF(isnull(a.term_retry_time),0,a.term_retry_time) as term_retry_time, a.group_id, IF(isnull(a.cpg_id),0,a.cpg_id) as cpg_id, a.term_media_proxy, a.term_privacy_method, a.term_codec_policy, a.term_source_number_type, a.term_source_number_plan, a.term_signal_ip, a.term_signal_port, a.term_sip_call_reroute, a.term_itu_call_reroute, a.term_sip_prevent_call_reroute, a.term_itu_prevent_call_reroute, a.term_sip_call_retry, a.term_itu_call_retry FROM vca_term_equipment a  WHERE a.term_id='%d' AND UNIX_TIMESTAMP(NOW()) BETWEEN a.term_start_date AND a.term_end_date AND a.term_status='Y'", term_id);

	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : SIP Termination Equipment SQL : \n%s\n", sql);
	switch_execute_sql_callback(globals.mutex, sql, switch_termination_callback, &sip_handler);
	switch_safe_free(sql);
	
	 if(sip_handler.term_cpg_id > 0) {
		sql = switch_mprintf("SELECT cpg_max_calls, cpg_max_calls_sec FROM vca_capacity_group WHERE cpg_id='%d'", sip_handler.term_cpg_id);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : SIP Termination Equipment Capacity Group SQL : \n%s\n", sql);
		switch_execute_sql_callback(globals.mutex, sql, switch_capacitygp_callback, &sip_handler);
		switch_safe_free(sql);
	 }
	
	/*! SIP Termination Equipment Validation */
	if(zstr(sip_handler.term_id)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-TE] : Termination Application is unable To Get Termination Equipment [ %d ] Information.\n", term_id);
				
		switch_channel_set_variable(channel, "switch_hangup_code", "401");
		switch_channel_set_variable(channel, "switch_hangup_reason", "CALL_REJECTED");
		switch_channel_set_variable(channel, "dialout_string", "SWITCH_HANGUP XML SWITCH_HANGUP");
		goto end;
	}
	
	/*! Log */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : Proceed To SIP Termination Equipment [ %d ] Resource Limit Varification.\n", term_id);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : SIP Termination Equipment [ %d ] Capacity Group < MAX CPS > varifying.\n", term_id);
	
	/**
	 * @Section SIP Termination Equipment Capacity Group Max CPS
	 */
	
	if(sip_handler.cpg_max_calls_sec > 0) {
		char *idname = switch_mprintf("%d_cg_max_cps", sip_handler.term_cpg_id);        /*! Capacity Group Mac cps Key*/
		int retval = 0;                                                                 /*! Return Value */
		char *hiredis_raw_response = NULL;                                              /*! Hiredis Response */

		/**
		 * @Section Set Capacity Group Max cps
		 */
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[AMAZE-TE] : ********* PROCEED TO SET LIMIT *********\n");
		retval = switch_check_resource_limit("hiredis", session, globals.profile, idname,  sip_handler.cpg_max_calls_sec, 1, "SIP_TERM_CPS_EXCEEDED"); 
		
		/*! Hiredis Resource Key */
		hiredis_raw_response = switch_mprintf("%s",switch_channel_get_variable(channel, "vox_hiredis_response"));
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : hiredis_raw_response : %s\n",hiredis_raw_response); 
		
		/*! SIP Termination Equipment resource validation */
		if((strlen(hiredis_raw_response)>0 && !strcasecmp(hiredis_raw_response, "success") )) {
			if(retval != 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-TE] : SIP Termination Equipment [ %d ] Capacity Group CPS Limit Exceeded\n", term_id);
				switch_channel_set_variable(channel, "dialout_string", "sip_term_cps_exceeded XML SIP_TERM_CPS_EXCEEDED");
				
                                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] :Proceed For Next SIP Termination Equipment.\n");
                                
                                switch_safe_free(idname);
                                //Date : 08-09-2016
                                return 1;//return 1 for reroute
                                //goto end;
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[AMAZE-TE] : Hiredis is unable to connect redis server [ %s ], Capacity Group [ %d ]  MAX CPS will not check.\n",hiredis_raw_response, sip_handler.term_cpg_id);
		}
		switch_safe_free(idname);
		switch_safe_free(hiredis_raw_response);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : SIP Termination Equipment [ %d ] Capacity Group CPS Is Unlimited.\n", term_id);
	}
	
	/*! Log on Console */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : SIP Termination Equipment [ %d ] Capacity Group CPS validate Successfully\n", term_id);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : SIP Termination Equipment [ %d ] Capacity Group < MAX CALL > varifying.\n", term_id);

	/*! Section Verisy SIP Equipment Max CPS */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : SIP Termination Equipment [ %d ] < MAX CPS > varifying.\n", term_id);
	if(sip_handler.term_max_calls_sec > 0) {
		char *hiredis_raw_response = NULL;
		char *idname = switch_mprintf("%s_term_max_cps", sip_handler.term_id);
		int retval = 0;

		/*! Checking Resource limit */
		retval = switch_check_resource_limit("hiredis", session, globals.profile, idname,  sip_handler.term_max_calls_sec, 1, "SIP_TERM_EQUIP_CPS_EXCEEDED");
		hiredis_raw_response = switch_mprintf("%s",switch_channel_get_variable(channel, "vox_hiredis_response"));
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : hiredis_raw_response : %s\n",hiredis_raw_response); 
		
		if((strlen(hiredis_raw_response)>0 && !strcasecmp(hiredis_raw_response, "success") )) {
			if(retval != 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-TE] : SIP Terminator Equipment [ %d ] CPS Limit Exceeded\n", term_id);
				switch_channel_set_variable(channel, "dialout_string", "sip_term_equip_cps_exceeded XML SIP_TERM_EQUIP_CPS_EXCEEDED");
                                
                                 switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : Proceed For Next SIP Termination Equipment\n");
                                 
                                 switch_safe_free(idname);
                                //Date : 08-09-2016
                                return 1;//return 1 for reroute
                                
// 				goto end;
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[AMAZE-TE] : Hiredis is unable to connect redis server [ %s ], SIP Termination Equipment [ %d ]  MAX CPS will not check.\n",hiredis_raw_response, term_id);
		}
		switch_safe_free(idname);
		switch_safe_free(hiredis_raw_response);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : SIP Termination Equipment [ %d ] CPS Is Unlimited.\n", term_id);
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : SIP Termination Equipment CPS validate Successfully\n");

	/**
	 * @Section Checking SIP Termination Equipment Varifying.
	 */
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : SIP Termination Equipment [ %d ] < MAX CALL > varifying.\n", term_id);
	if(sip_handler.term_max_calls > 0 ) {
		char *hiredis_raw_response = NULL;                                        /*! hiredis response */
		char *idname = switch_mprintf("%s_term_max_call", sip_handler.term_id);   /*! Termination Equipment Max Call */
		
		/*! Checking Resource Limit */
		int retval = switch_check_resource_limit("hiredis", session, globals.profile, idname,  sip_handler.term_max_calls, 0, "SIP_TERM_EQUIP_MC_EXCEEDED");

		/*! Checking hiredis response */
		hiredis_raw_response = switch_mprintf("%s",switch_channel_get_variable(channel, "vox_hiredis_response"));
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : hiredis_raw_response : %s\n",hiredis_raw_response); 
		if((strlen(hiredis_raw_response)>0 && !strcasecmp(hiredis_raw_response, "success") )) {
			if(retval != 0) {
                                int limit_status = SWITCH_STATUS_SUCCESS;
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-TE] : SIP Termination Equipment [ %d ] Max Call Limit Exceeded\n", term_id);
				switch_channel_set_variable(channel, "dialout_string", "sip_term_equip_mc_exceeded XML SIP_TERM_EQUIP_MC_EXCEEDED");
                                
                                
                                limit_status = switch_limit_release("hiredis", session, globals.profile, idname);
                                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : Releases SIP Termination Equipment [ %d ]  MAX Limit [ %d ] hashkey [ %s ]\n", term_id, limit_status, idname);
                                
                                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : Proceed For Next SIP Termination Equipment\n");

                                switch_safe_free(idname);
                                
                                //Date : 08-09-2016
                                return 1;//return 1 for reroute                                
// 				goto end;
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[AMAZE-TE] : Hiredis is unable to connect redis server [ %s ], SIP Termination Equipment [ %d ]  MAX CALL will not check.\n",hiredis_raw_response, term_id);
		}
		switch_safe_free(idname);
		switch_safe_free(hiredis_raw_response);
		
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : SIP Termination Equipment [ %d ] Max Call Limit is Unlimited.\n", term_id);
	}	
	
	/**
	 * @Section SIP Termination Equipment Max CPS validation 
	 */
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : SIP Termination Equipment [ %d ] CPS validate Successfully\n", term_id);

	if(sip_handler.cpg_max_calls > 0) {
		char *hiredis_raw_response = NULL;                                         /*! Hiredis response */
		char *idname = switch_mprintf("%d_cg_max_call", sip_handler.term_cpg_id);  /*! Capacity Group Max Call */
		int retval = 0;                                                            /*! Return value */
		
		/**
		 * @Section Check resource Limit
		 */
		
		retval = switch_check_resource_limit("hiredis", session, globals.profile, idname,  sip_handler.cpg_max_calls, 0, "CG_CPS_EXCEEDED");
		hiredis_raw_response = switch_mprintf("%s",switch_channel_get_variable(channel, "vox_hiredis_response"));
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : hiredis_raw_response : %s\n",hiredis_raw_response); 
		
		/*! Hiredis Response */
		if((strlen(hiredis_raw_response)>0 && !strcasecmp(hiredis_raw_response, "success") )) {
			if(retval != 0) {
                                int limit_status = SWITCH_STATUS_SUCCESS;
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-TE] : SIP Terminator Equipment [ %d ] Capacity Group [ %d ] CALL Limit Exceeded\n", term_id, sip_handler.term_cpg_id);
				switch_channel_set_variable(channel, "dialout_string", "CG_MC_EXCEEDED XML CG_MC_EXCEEDED");
                                
                                limit_status = switch_limit_release("hiredis", session, globals.profile, idname);
                                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : Releases SIP Termination Equipment [ %d ]  CAPACITY GROUP MAX Limit [ %d ] hashkey [ %s ]\n", term_id, limit_status, idname);
                                
                                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : Proceed For Next SIP Termination Equipment\n");

                                switch_safe_free(idname);
                                
                                if(sip_handler.term_max_calls > 0 ) {
                                        char *idname = switch_mprintf("%s_term_max_call", sip_handler.term_id);   /*! Termination Equipment Max Call */
                                        limit_status = switch_limit_release("hiredis", session, globals.profile, idname);
                                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : Releases SIP Termination Equipment [ %d ]  MAX Limit [ %d ] hashkey [ %s ]\n", term_id, limit_status, idname);
                                        
                                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : Proceed For Next SIP Termination Equipment\n");
                                        switch_safe_free(idname);
                                }
                                
                                //Date : 08-09-2016
                                return 1;//return 1 for reroute                                  
                                
// 				goto end;
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[AMAZE-TE] : Hiredis is unable to connect redis server [ %s ], SIP Termination Equipment [ %d ]  MAX CALL will not check.\n",hiredis_raw_response, term_id);
		}
		switch_safe_free(idname);
		switch_safe_free(hiredis_raw_response);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : SIP Termination Equipment [ %d ] Capacity Group [ %d ] CALL Is Unlimited.\n", term_id,sip_handler.term_cpg_id);
	}
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : SIP Termination Equipment [ %d ] Capacity Group Max Calls validate Successfully\n",term_id);	
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : Proceed For SIP Termination Equipment [ %d ] Regex Information.\n",term_id);
		
	/**
	 * @Section Getting Regex information
	 */
	
	memset(&regex_master, 0, sizeof(regex_master));
	sql = switch_mprintf("SELECT regex_type,IF((regex_type='CHANGE_DESTINATION' OR regex_type='CHANGE_SOURCE'), group_concat(regex_string SEPARATOR '|'), group_concat(regex_string SEPARATOR '\\\\|')) as regex_string FROM `vca_regex_master` WHERE regex_equip_id='%d' AND `regex_equip_type`='TERMINATION' GROUP BY regex_type",term_id);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : SIP Termination Equipment [ %d ] Regex SQL : \n%s\n",term_id, sql);

	/*! Log Regex Info */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : %s\n", line);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : SIP Termination Equipment [ %d ] Regex information.\n", term_id);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : %s\n", line);

	switch_execute_sql_callback(globals.mutex, sql, switch_regex_callback, &regex_master);
	switch_safe_free(sql);

	/**
	 * @Section Source Number Manipulation Section
	 */
	
	if(!zstr(regex_master.change_source)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : Proceed To SIP Termination Equipment [ %d ] Source Number [ %s ] Regex  [ %s ] Manipulation.\n", term_id, source_number, regex_master.change_source);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : SIP Termination Equipment [ %d ] Before Regex [ %s ] Manipulation Source Number [ %s ]\n", term_id, regex_master.change_source, source_number);
		
		tmp_num = strdup(source_number);
		
		/*! Source Number Manipulation Section */
		source_number = switch_regex_manipulation(session, channel, regex_master.change_source, source_number);
		if(!strcmp(source_number, "true") || !strcmp(source_number, "false")) {
			source_number = strdup(tmp_num);
		} else {
			source_number = strdup(source_number);
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : SIP Termination Equipment [ %d ] After Regex [ %s ] Manipulation Source Number [ %s ]\n", term_id, regex_master.change_source, source_number);
	} else {
		  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[AMAZE-TE] : SIP Termination Equipment [ %d ] Source Number [ %s ] Manipulation Regex is not set, So System will not check it.\n", term_id, source_number);
	}
	
	/*! Setting Channel Variables */
	switch_channel_export_variable(channel, "vox_term_out_source_number", source_number, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
	switch_channel_set_variable(channel, "vox_term_out_source_number", source_number);
	
	/**
	 * @Section Destination Number manipulation 
	 */
	
	if(!zstr(regex_master.change_destination)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : Proceed To SIP Termination Equipment [ %d ] Destination Number [ %s ] Regex  [ %s ] Manipulation.\n", term_id, destination_number, regex_master.change_destination);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : SIP Termination Equipment [ %d ] Before Regex [ %s ] Manipulation Destination Number [ %s ]\n", term_id, regex_master.change_destination, destination_number);
		
		tmp_num = strdup(destination_number);
		
		/*! manipulate Destination Number */
		destination_number = switch_regex_manipulation(session, channel, regex_master.change_destination, destination_number);
		if(!strcmp(destination_number, "true") || !strcmp(destination_number, "false")) {
			destination_number = strdup(tmp_num);
		} else {
			destination_number = strdup(destination_number);
		}
		
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : SIP Termination Equipment [ %d ] After Regex [ %s ] Manipulation Destination Number [ %s ]\n", term_id, regex_master.change_destination, destination_number);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[AMAZE-TE] : SIP Termination Equipment [ %d ] Destination Number [ %s ] Manipulation Regex is not set, So System will not check it.\n", term_id, destination_number);
	}
	
	/*! Setting Channel Variable for report */
	switch_channel_export_variable(channel, "vox_term_out_destination_number", destination_number, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
	switch_channel_set_variable(channel, "vox_term_out_destination_number", destination_number);
	
	
	/**
	 *  @Section Read Offer Code String 
	 */
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : SIP Termination Equipment [ %d ] Get Codec String [ %s ] From Origination Equipment.\n", term_id, (char *)switch_channel_get_variable(channel, "vox_outbound_codec_prefs"));

	ep_codec_string = (char *)switch_channel_get_variable(channel, "vox_outbound_codec_prefs");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : SIP Termination Equipment [ %d ] Codec Group Policy is [ %s ]\n", term_id, sip_handler.term_codec_policy);
	
	/**
	 * @Section SIP Termination Equipmet Codec Group Info
	 */
	
	sql = switch_mprintf("SELECT group_concat(concat(b.codec_name,'@',b.codec_sample_rate,'h@',b.codec_frame_packet,'i') SEPARATOR ',') as codec_string FROM vca_codec_group_details a, vca_codec b WHERE a.codec_id=b.codec_id AND a.group_id=%d", sip_handler.term_group_id);
	switch_execute_sql2str(NULL, sql, resbuf, sizeof(resbuf));
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : SIP Termination Equipment [ %d ] Codec List SQL : \n%s\n",term_id, sql);
	switch_safe_free(sql);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : SIP Termination Equipment [ %d ] Assigned Codec List [ %s ]\n", term_id, resbuf);
	
	/**
	 * @Section Checing Codec Policy And Offer Codec using policy
	 */
	
	if(!zstr(sip_handler.term_codec_policy) && !strcmp(sip_handler.term_codec_policy, "SWITCH")) {       /*! SWITCH Policy */
		switch_channel_export_variable(channel, "switch_vox_sip_term_outbound_codec_prefs", resbuf, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : SIP Termination Equipment [ %d ] Offered Codec [ %s ]\n", term_id,resbuf);
		switch_channel_export_variable(channel, "swicth_vox_transcoding", "Y", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
	} else if(!zstr(sip_handler.term_codec_policy) && !strcmp(sip_handler.term_codec_policy, "INHERIT")) {   /*! INHERIT Policy */
// 		char *token = NULL;
// 		char *codec_str = NULL;
// 		const char *seprator = ",";
// 		int flag = 0;
		
		
		/**
		 * @Section validating Offer Codec 
		 */
/*		
		codec_str = switch_mprintf("%s", resbuf);
		token = strtok(codec_str, seprator);
		while( token != NULL) {
			char *p = NULL;
			
			p = strstr(ep_codec_string, token);
			if(p!=NULL) {
				flag = 1;
				break;
			}
			token = strtok(NULL, seprator);
		}
		switch_safe_free(codec_str);
		
		if(flag == 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-TE] : SIP Termination Equipment [ %d ] Assigned Codec does not match with received codec .\n", term_id);
			switch_channel_set_variable(channel, "dialout_string", "VOX_HANGUP_488 XML VOX_HANGUP_488");
			goto end;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : SIP Termination Equipment [ %d ] done Codec Verification Successfully.\n", term_id);
		}*/
		
		/**
		 * @Section Setting Channel Variables For CDR and report 
		 */
		
		switch_channel_export_variable(channel, "swicth_vox_transcoding", "N", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
		switch_channel_export_variable(channel, "switch_vox_sip_term_outbound_codec_prefs", ep_codec_string, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : SIP Termination Equipment [ %d ] Offered Codec [ %s ]\n", term_id,ep_codec_string);
		
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-TE] : SIP Termination Equipment [ %d ] is not able to get codec policy, so system will not allow to dial this number.\n", term_id);
		switch_channel_set_variable(channel, "dialout_string", "VOX_HANGUP_488 XML VOX_HANGUP_488");
		goto end;
	}
	
	switch_channel_export_variable(channel, "vox_term_calling_number_type", sip_handler.term_source_number_type, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
        
	//================================NOA=================================
	
	/**
	 * @Section Source Number Nature Of Address Validation
	 */
	
	if(!strcmp(sip_handler.term_source_number_type, "UNKNOWN")) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : SIP Termination Equipment [ %d ] Source Number NOA [Nature of Address]  is [ UNKNOWN ]\n", term_id);

		noa = switch_mprintf("noa=2;");
	
	} else if(!strcmp(sip_handler.term_source_number_type, "NATIONAL")) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : SIP Termination Equipment [ %d ] Source Number NOA [Nature of Address]  is [ NATIONAL ]\n", term_id);

		noa = switch_mprintf("noa=3;");
		
	} else if (!strcmp(sip_handler.term_source_number_type, "INTERNATIONAL")) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : SIP Termination Equipment [ %d ] Source Number NOA [Nature of Address]  is [ INTERNATIONAL ]\n", term_id);

		noa = switch_mprintf("noa=4;");
	} else if(!strcmp(sip_handler.term_source_number_type, "NETWORK")) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : SIP Termination Equipment [ %d ] Source Number NOA [Nature of Address]  is [ NETWORK ]\n", term_id);

		noa = switch_mprintf("noa=5;");
		
	} else if(!strcmp(sip_handler.term_source_number_type, "SUBSCRIBER")) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : SIP Termination Equipment [ %d ] Source Number NOA [Nature of Address]  is [ SUBSCRIBER ]\n", term_id);
	
		noa = switch_mprintf("noa=1;");
	} else if(!strcmp(sip_handler.term_source_number_type, "ABBREVIATED")) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : SIP Termination Equipment [ %d ] Source Number NOA [Nature of Address]  is [ ABBREVIATED ]\n", term_id);
		
	} else if(!strcmp(sip_handler.term_source_number_type, "INHERIT")) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Source NOA [Nature of Address] is : INHERIT\n");
		noa_flag = 0;
		//noa = switch_mprintf("noa=0;"); //TODO verify for it value not get
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-TE] : Source NOA [Nature of Address] is : NOT FOUND\n");
		noa_flag = 0;
	}
	
	switch_channel_export_variable(channel, "vox_term_calling_number_plan", sip_handler.term_source_number_plan, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
	
	/**
	 * @Section Source Number Numner Plan Indicator
	 */

	if(!strcmp(sip_handler.term_source_number_plan, "UNKNOWN")) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Source NPI [Numbering Plan Indicator] is [ UNKNOWN ]\n");
	  
		npi = switch_mprintf("npi=0;");
	} else if(!strcmp(sip_handler.term_source_number_plan, "ISDN")) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Source NPI [Numbering Plan Indicator] is [ ISDN ]\n");
	  
		npi = switch_mprintf("npi=1;");
	} else if (!strcmp(sip_handler.term_source_number_plan, "DATA_NUMBER")) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Source NPI [Numbering Plan Indicator] is [ DATA_NUMBER ]\n");
	  
		npi = switch_mprintf("npi=3;");
	} else if(!strcmp(sip_handler.term_source_number_plan, "TELEX_NUMBER")) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Source NPI [Numbering Plan Indicator] is [ TELEX_NUMBER ]\n");
	  
		npi = switch_mprintf("npi=4;");
	} else if(!strcmp(sip_handler.term_source_number_plan, "PRIVATE")) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Source NPI [Numbering Plan Indicator] is [ PRIVATE ]\n");
	  
		npi = switch_mprintf("npi=5;");
	} else if(!strcmp(sip_handler.term_source_number_plan, "NATIONAL")) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Source NPI [Numbering Plan Indicator] is [ NATIONAL ]\n");
	  
		npi = switch_mprintf("npi=6;"); //TODO Confirm with Sir value not get
	} else if(!strcmp(sip_handler.term_source_number_plan, "INHERIT")) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Source NPI [Numbering Plan Indicator] is [ INHERIT ]\n");
	  
// 		npi = switch_mprintf("npi=0;");//TODO Configurm with Sir.
		noa_flag = 0;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-TE] : Source NPI [Numbering Plan Indicator] is [ NOT FOUND ]\n");
		noa_flag = 0;
	}


	if(noa_flag) {
		pcharge_info = switch_mprintf("%s%s", npi,noa); 
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : P-Charge-Info Required Data : %s\n",pcharge_info);
	}

	if(sip_handler.term_group_id > 0) {
		int switch_vox_status = 0;

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"[AMAZE-TE] : SIP Termination Equipment SIP Signaling IP:PORT [ %s:%s]\n", sip_handler.term_signal_ip, sip_handler.term_signal_port);

		/**
		 * @Section Proceed for validate SIP Equipment and Originate Calls and Handle hangup cause..
		 */
		
		switch_vox_status = switch_amaze_ivr_originate(session, &sip_handler, destination_number, source_number, pcharge_info, noa_flag);
		if(noa_flag) {
			switch_safe_free(pcharge_info);
		}
		switch_safe_free(npi);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5,"[AMAZE-TE] : SIP Termination Bridge switch_vox_status [ %d ].", switch_vox_status);
                
//                 if(switch_vox_status == 1 || switch_vox_status == 2 || switch_vox_status == -1) {
//                     char *sql = NULL;
//                     switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"[AMAZE-TE] : Remove Active Call Entry\n");
//                     sql = switch_mprintf("DELETE FROM vca_active_call WHERE ac_uuid='%s'", switch_str_nil(switch_channel_get_variable(channel, "uuid")));
//                     switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : SIP Termination Equipment Delete Active Call Record SQL :: \n%s\n", sql);
//                     switch_execute_sql(sql, globals.mutex);
//                     switch_safe_free(sql);
//                 }
                
		/**
		 * @Section Origination Call Status and According To Do action
		 */
		
		switch(switch_vox_status) {
			case 0 :
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"[AMAZE-TE] : SIP Termination Bridge Status Done Successfully\n");
				return 0;
			case SWITCH_SIP_PREVENT :
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"[AMAZE-TE] : SIP Termination Equipment Is SIP prevent.\n");
                                
				goto end;
			case SWITCH_ITUT_PREVENT :
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"[AMAZE-TE] : SIP Termination Equipment Is ITU-I Q_850 prevent.\n");
				goto end;
			case SWITCH_SIP_REROUTE :
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"[AMAZE-TE] : SIP Termination Equipment SIP Reroute Allowed.\n");
                                
				return 1; //return 1 for reroute
			case SWITCH_ITUT_REROUTE :
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"[AMAZE-TE] : SIP Termination Equipment ITU-I Q_850 Reroute Allowed.\n");
                                
				return 1;//return 1 for reroute
			default :
                           
				goto end;
		}
	}
	return 0;
end :
  return -1;
}

/*! Function Get SIP Termination Signaling Serevr Data In case of unable to get data from Loadbalace script */
static int switch_term_server_data(void *ptr, int argc, char **argv, char **col) 
{
	struct __sip_server *SipServer = (struct __sip_server *)ptr;      /*! SIP redirect Equipment Structure */
	int index = 0;                                            /*! Coloum COunt */  
	
	/**
	 * @Section Log SIP Redirect Equipment on console 
	 */
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : %s\n", line);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : ========= SIP SIGNALING SERVER INFO =========\n");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : %s\n", line);

	for(index = 0; index < argc ; index++) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : %s : %s\n", col[index], argv[index]);
	}
	
	SipServer->opensips_default_ip = strdup(argv[0]);        /*! Opensips Default IP */
	SipServer->sip_sig_node_id = strdup(argv[1]);            /*! Signaling Node ID */  
	SipServer->sip_media_node_id = strdup(argv[2]);          /*! Media Node ID */ 
	
	return SWITCH_TERMINATOR_SUCCESS;
}


SWITCH_STANDARD_APP(switch_termination_app)
{
	char *mydata = NULL;                     /*! Session Data */
// 	char *argv[25] = {0};                    /*! Session Application Argument */
	char *Pargv[6] = {0};                    /*! Session String Seperatoring */ 
// 	char *sql = NULL;                        /*! SQL */
// 	char *sip_id_data = NULL;                /*! SIP ID data */
	char *source_number = NULL;              /*! Source Number */
	char *destination_number = NULL;         /*! Destination Number */
	char *term_source_number = NULL;         /*! SIP Termination Source Number */    
	char *term_destination_number = NULL;    /*! SIP Termination Destination Number */
	char *dp_balancing_method = NULL;        /*! SIP Termination Balancing Method */
	char *dp_termination_type = "";          /*! SIP Termination Equipment Type */
	int vox_term_id = 0;                     /*! SIP Termination ID */
	int dp_id = 0;                           /*! SIP Termination Dialpeer ID */
	
	char *redirection_to_termination = NULL;
        char *redirect_term_id = NULL;

	/*! Session Channel */
	switch_channel_t *channel = switch_core_session_get_channel(session);

	/**
	 * @Section Channel validation
	 */
	
	if(!channel) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-TE] : Hey I am unable to get channel..!!\n");
		switch_channel_set_variable(channel, "switch_hangup_code", "401");
		switch_channel_set_variable(channel, "switch_hangup_reason", "CALL_REJECTED");
		switch_channel_set_variable(channel, "dialout_string", "SWITCH_HANGUP XML SWITCH_HANGUP");
		goto end;
	}
	
	mydata = switch_core_session_strdup(session, data); /*! Copy Data From Session */
	if(!mydata) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-TE] : termination application is not able to get data from session.\n");
		switch_channel_set_variable(channel, "switch_hangup_code", "401");
		switch_channel_set_variable(channel, "switch_hangup_reason", "CALL_REJECTED");
		switch_channel_set_variable(channel, "dialout_string", "SWITCH_HANGUP XML SWITCH_HANGUP");
		goto end;
	}
	
	redirection_to_termination = (char *)switch_str_nil(switch_channel_get_variable(channel, "redirection_to_termination"));
        

	/*! Application Log */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : BILLING_SOURCE_NUMBER,BILLING_DESTINATION_NUMBER, DIALPEER_ID,BALANCING_METHOD,TERMINATION_TYPE\n");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : Termination application Get Data From DIALPEER [ %s ].\n", mydata);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[AMAZE-TE] : +++++++ Termination application Validation Started +++++++.\n");
	
	/**
	 * @Section Application Data Validation
	 */
	
	switch_separate_string(mydata, ',', Pargv, (sizeof(Pargv) / sizeof(Pargv[0])));
	if(zstr(Pargv[0]) || zstr(Pargv[1]) || zstr(Pargv[2]) || zstr(Pargv[3]) || zstr(Pargv[4])) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-TE] : SIP Termination Equipment is unable to get data.\n");
		switch_channel_set_variable(channel, "switch_hangup_code", "401");
		switch_channel_set_variable(channel, "switch_hangup_reason", "CALL_REJECTED");
		switch_channel_set_variable(channel, "dialout_string", "SWITCH_HANGUP XML SWITCH_HANGUP");
		goto end;  
	}

	/**
	 * @Section Application Log on console
	 */
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Termination Application Get Source Number [ %s ] From DIALPEER.\n", Pargv[0]);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Termination Application Get Destination Number [ %s ] From DIALPEER.\n", Pargv[1]);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Termination Application Get DIALPEER [ %s ] From DIALPEER.\n", Pargv[2]);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Termination Application Get DIALPEER Balancing Method [ %s ] From DIALPEER.\n", Pargv[3]);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Termination Application Get DIALPEER Termination Type [ %s ] From DIALPEER.\n", Pargv[4]);

	term_source_number = strdup(Pargv[0]);         /*! Termination Equipment Source Number */
	term_destination_number = strdup(Pargv[1]);    /*! Termination Equipment Destination Number */
	dp_termination_type = strdup(Pargv[4]);        /*! Termination Equipment Type */
	
	//separte sip termination ids 
// 	switch_separate_string(sip_id_data, ',', argv, (sizeof(argv) / sizeof(argv[0])));
        
        if(!strcasecmp(redirection_to_termination,"yes")) {
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : Proceed For SIP Termination Equipment From SIP Redirection Application.\n");
        
            redirect_term_id = strdup(Pargv[2]);
            
        } else {
                dp_id = atoi(Pargv[2]);                              /*! Termination Equipment DialPeer ID */
        }
	
	dp_balancing_method = strdup(Pargv[3]);              /*! Termination Equipment Balancing Method */

	//for cdr
	switch_channel_export_variable(channel, "vox_termination_equipment_type", dp_termination_type, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
        
        //Check it is first call from dialpeer 
        if(strcmp(switch_str_nil(switch_channel_get_variable(channel, "reroute_on_dialpeer")),"yes") ) {
                switch_channel_set_variable(channel, "active_call_status", "empty");
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : This call is recieved from dial peer\n");
        } else {
             switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : reroute on dialpeer application start.\n");
        }
                        
//     switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[ DIALPEER ] : Termination Application Execution done.\n");
       //Set Flag For Active Call Entry
                
        
	/*! Equipment Type is SIP Termination Equipment */
	if(!strcmp(dp_termination_type, "SIP_TERMINATION")) {
                char *sql = NULL;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : Proceed For SIP Termination Equipment Validation\n");
                
                
		/*! SIP termiantion Equipment Balancing Method */
		if(!strcmp(dp_balancing_method, "PERCENTAGE_BASED") || !strcmp(dp_balancing_method, "ROUND_ROBIN")) {
			int ret = -1;
			char *termination_ep_id = NULL;
			char *receiveData = NULL;
			char *StrIng[4] = {0};
                        int rerouted_call = 0;
                        char *first_equip_id = NULL;
			
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : SIP Termination Equipment Balancing Method [ %s ].\n",dp_balancing_method);

		reroute :
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : Proceed To Get SIP Termination Equipment ID From Load Balance Server.\n");
                        
			/**
			 * @Function Get SIP Termination Equipment ID
			 */

			/*! Get Termination Server info 0 means only Signaling Server info and 1 means both TE and Signaling Info */
			/*! Get SIP Termination Equipment ID and Get Termination Server info*/
                        
			receiveData = get_termination_equipment_id(dp_id, 1);
			switch_separate_string(receiveData, '#', StrIng, (sizeof(StrIng) / sizeof(StrIng[0])));
			
			if(zstr(StrIng[0]) || zstr(StrIng[1]) || zstr(StrIng[2]) || zstr(StrIng[3])) {
				char resbuf[25] = "";
				struct __sip_server SipServer;
				
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-TE] : SIP Termination Equipment is unable to get Termination Equipment ID and Opensips Default ID.\n");
				memset(&SipServer, 0, sizeof(SipServer));

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[AMAZE-TE] : SIP Termination Equipment Does Not Get Proper ID From [ %s:%d ], So system will get sip termination without balancing method.\n", globals.bind_ipaddress, globals.bind_ipport);
				
				sql = switch_mprintf("SELECT `term_id` FROM `vca_dial_term_equi_mapping` WHERE `dp_id` = '%d' LIMIT 1", dp_id);
				switch_execute_sql2str(NULL, sql, resbuf, sizeof(resbuf));
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : SIP Termination ID LIST SQL : %s\n", sql);
				switch_safe_free(sql);
				if(zstr(resbuf)) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-TE] : Can't Get Termination Equipment Id\n");
                                        
/*                                        sql = switch_mprintf("DELETE FROM vca_active_call WHERE ac_uuid='%s'", switch_str_nil(switch_channel_get_variable(channel, "uuid")));
                                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : SIP Termination Equipment  Delete Active Call Record SQL :: %s\n", sql);
                                        switch_execute_sql(sql, globals.mutex);
                                        switch_safe_free(sql);   */                                     
                                        
					goto end;
				}
				vox_term_id = atoi(resbuf);
                                first_equip_id = strdup(resbuf);
				
				sql = switch_mprintf("SELECT concat(B.sm_ip,':',sm_port), A.`zone_id`,A.`media_id` FROM `vca_term_equipment_mapping` A, vca_server_mapping B WHERE A.`term_id`='%d' AND B.sm_id=A.server_id AND sm_default = 'Y' limit 1",vox_term_id);
				
				switch_execute_sql_callback(globals.mutex, sql, switch_term_server_data, &SipServer);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : SIP Termination Equipment [ %d ] Default SIP Server Data SQL  :\n %s\n", vox_term_id, sql);
				switch_safe_free(sql);
	
				switch_channel_set_variable(channel, "vox_termination_default_ipport", SipServer.opensips_default_ip);
				switch_channel_set_variable(channel, "vox_termination_sig_node_id", SipServer.sip_sig_node_id);
				switch_channel_set_variable(channel, "vox_termination_med_node_id", SipServer.sip_media_node_id);
				
			} else {
				switch_channel_set_variable(channel, "vox_termination_default_ipport", StrIng[1]);
				switch_channel_set_variable(channel, "vox_termination_sig_node_id", StrIng[2]);
				switch_channel_set_variable(channel, "vox_termination_med_node_id", StrIng[3]);
			
				//TID=1#DIP=192.168.1.90:5060#SNID=3#MNID=4
				vox_term_id = atoi(StrIng[0]);
                                first_equip_id = strdup(StrIng[0]);
			}
			
			switch_safe_free(receiveData);

                        if(rerouted_call != 0) {
                            if((atoi(switch_str_nil(switch_channel_get_variable(channel, "first_term_equipment_id")) ) ==  vox_term_id ))  {
                                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : REPEATING EQUIPMENT PLEASE DELETE IT [ %d ].\n", vox_term_id);
                                
                                switch_channel_set_variable(channel, "reroute_on_dialpeer", "yes");
                                goto end;
                            }
                        } else {
                                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : CALL IS FROM DIALPEER APPLICATION AND SET TERM ID.\n");
                                //unset this when next dial peer use.
                                switch_channel_set_variable(channel, "first_term_equipment_id", first_equip_id);
                        }
                        
                        rerouted_call++;
			
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : Termination Application Get SIP Termination Equipment Id [ %d ].\n", vox_term_id);
		
			termination_ep_id = switch_mprintf("%d", vox_term_id);
			//For cdr record
			switch_channel_set_variable(channel, "vox_termination_equipment_id", termination_ep_id);
			switch_channel_export_variable(channel, "vox_termination_equipment_id", termination_ep_id, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
			switch_safe_free(termination_ep_id);

			source_number = strdup(term_source_number);
			destination_number = strdup(term_destination_number);
		
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : SIP Termination Equipment [ %d ] Receive Source Number [ %s ].\n", vox_term_id, source_number);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : SIP Termination Equipment [ %d ] Receive Destination Number [ %s ].\n",vox_term_id, destination_number);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : Proceed To Manage SIP Termination Equipment [ %d ] .\n",vox_term_id);
			
			/**
			 * @Function Validate SIP Termination Equipment
			 */  
			ret = switch_manage_termination_equipment(vox_term_id, channel, session, source_number, destination_number);
			if(ret < 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-TE] : SIP Termination Equipment  [ %d ] validation failed.\n",vox_term_id);
                                
//                                 sql = switch_mprintf("DELETE FROM vca_active_call WHERE ac_uuid='%s'", switch_str_nil(switch_channel_get_variable(channel, "uuid")));
//                                 switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : SIP Termination Equipment  Delete Active Call Record SQL :: %s\n", sql);
//                                 switch_execute_sql(sql, globals.mutex);
//                                 switch_safe_free(sql);
				
				goto end;
			} else if(ret == 1) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[AMAZE-TE] : Proceed For Get Another SIP Termination Equipment.\n");
				goto reroute;
			}
		} else if(!strcmp(dp_balancing_method, "PRIORITY_BASED")) {
			char resbuf[25] = "";
			int xy = 0, i = 0;
			char *term_ids[25] = {0};
			int ret = -1;
			
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : SIP Termination Application Balancing method [ PRIORITY_BASED ]\n");
			sql = switch_mprintf("SELECT GROUP_CONCAT(DISTINCT(`term_id`)) FROM `vca_dial_term_equi_mapping` WHERE `dp_id` = '%d' ORDER BY `dtem_priority`", dp_id);
			
			switch_execute_sql2str(NULL, sql, resbuf, sizeof(resbuf));
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : Get SIP Termination Equipments ID List SQL : \n%s\n", sql);
			switch_safe_free(sql);
			
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : SIP Termination Equipments ID List : %s\n", resbuf);
			xy = switch_separate_string(resbuf, ',', term_ids, (sizeof(term_ids) / sizeof(term_ids[0])));

			for(i = 0; i< xy ;i++) {
				char *StrIng[3] = {0};
				char *receiveData = NULL;
                                
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : Proceed For %s Termination Equipment\n", term_ids[i]);

				source_number = strdup(term_source_number);
				destination_number = strdup(term_destination_number);
			
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : SIP Termination Equipment Receive Source Number : %s.\n", source_number);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : SIP Termination Equipment Receive Destination Number : %s.\n", destination_number);

				//DIP=192.168.1.90:5060#SNID=3#MNID=4
				/*! Get Termination Server info 0 means only Signaling Server info and 1 means both TE and Signaling Info */
				receiveData = get_termination_equipment_id(atoi(term_ids[i]), 0);

				switch_separate_string(receiveData, '#', StrIng, (sizeof(StrIng) / sizeof(StrIng[0])));
				
				if(zstr(StrIng[0]) || zstr(StrIng[1]) || zstr(StrIng[2])) {

					struct __sip_server SipServer;
					memset(&SipServer, 0, sizeof(SipServer));

					sql = switch_mprintf("SELECT concat(B.sm_ip,':',sm_port), A.`zone_id`,A.`media_id` FROM `vca_term_equipment_mapping` A, vca_server_mapping B WHERE A.`term_id`='%d' AND B.sm_id=A.server_id AND sm_default = 'Y' limit 1",atoi(term_ids[i]));
					
					switch_execute_sql_callback(globals.mutex, sql, switch_term_server_data, &SipServer);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : SIP Termination Equipment [ %d ] Default SIP Server Data SQL  :\n %s\n", atoi(term_ids[i]), sql);
					switch_safe_free(sql);
		
					switch_channel_set_variable(channel, "vox_termination_default_ipport", SipServer.opensips_default_ip);
					switch_channel_set_variable(channel, "vox_termination_sig_node_id", SipServer.sip_sig_node_id);
					switch_channel_set_variable(channel, "vox_termination_med_node_id", SipServer.sip_media_node_id);

					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : vox_termination_default_ipport [ %s ]\n", SipServer.opensips_default_ip);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : vox_termination_sig_node_id [ %s ]\n", SipServer.sip_sig_node_id);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : vox_termination_med_node_id [ %s ]\n", SipServer.sip_media_node_id);
					
				
				} else {
					switch_channel_set_variable(channel, "vox_termination_default_ipport", StrIng[0]);
					switch_channel_set_variable(channel, "vox_termination_sig_node_id", StrIng[1]);
					switch_channel_set_variable(channel, "vox_termination_med_node_id", StrIng[2]);
					
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : vox_termination_default_ipport [ %s ]\n", receiveData);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : vox_termination_sig_node_id [ %s ]\n", StrIng[1]);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : vox_termination_med_node_id [ %s ]\n", StrIng[2]);
				}
				switch_safe_free(receiveData);
				
				//For cdr record
				switch_channel_set_variable(channel, "vox_termination_equipment_id", term_ids[i]);
				switch_channel_export_variable(channel, "vox_termination_equipment_id", term_ids[i], SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
				
				/**
				 * @Function SIP Termination Equipment validation
				 */
				ret = switch_manage_termination_equipment(atoi(term_ids[i]), channel, session, source_number, destination_number);
				if(ret < 0) {
//                                         char *sql = NULL;
                                        
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-TE] : Failed To Handle Termination Equipment\n");
                                        
//                                         sql = switch_mprintf("DELETE FROM vca_active_call WHERE ac_uuid='%s'", switch_str_nil(switch_channel_get_variable(channel, "uuid")));
//                                         switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : SIP Termination Equipment  Delete Active Call Record SQL :: %s\n", sql);
//                                         switch_execute_sql(sql, globals.mutex);
//                                         switch_safe_free(sql);
                                        
					goto end;
				} else if(ret == 1) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : Continue for next SIP Termination Equipment.\n");
					//goto reroute;
					continue;
				} else if(ret == 0) {
					break;
				}
			}
			
			if(i >= xy ) {
                            switch_channel_set_variable(channel, "reroute_on_dialpeer", "yes");
                            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : Proceed For Next Dial Peer Equipment.\n");
                        }
		} else if(!strcmp(dp_balancing_method, "NOBALANCING_METHOD")) {

                        if(!strcasecmp(redirection_to_termination,"yes")) {
                                char *receiveData = NULL;
                                char *StrIng[3] = {0};
                                int ret = -1;
                                
                                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : Proceed For SIP Termination Equipment ID [ %s ].\n", redirect_term_id);
                            
                            
                            
				//DIP=192.168.1.90:5060#SNID=3#MNID=4
				/*! Get Termination Server info 0 means only Signaling Server info and 1 means both TE and Signaling Info */
				receiveData = get_termination_equipment_id(atoi(redirect_term_id), 0);

				switch_separate_string(receiveData, '#', StrIng, (sizeof(StrIng) / sizeof(StrIng[0])));
				
				if(zstr(StrIng[0]) || zstr(StrIng[1]) || zstr(StrIng[2])) {

					struct __sip_server SipServer;
					memset(&SipServer, 0, sizeof(SipServer));

					sql = switch_mprintf("SELECT concat(B.sm_ip,':',sm_port), A.`zone_id`,A.`media_id` FROM `vca_term_equipment_mapping` A, vca_server_mapping B WHERE A.`term_id`='%d' AND B.sm_id=A.server_id AND sm_default = 'Y' limit 1",atoi(redirect_term_id));
					
					switch_execute_sql_callback(globals.mutex, sql, switch_term_server_data, &SipServer);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-TE] : SIP Termination Equipment [ %d ] Default SIP Server Data SQL  :\n %s\n", atoi(redirect_term_id), sql);
					switch_safe_free(sql);
		
					switch_channel_set_variable(channel, "vox_termination_default_ipport", SipServer.opensips_default_ip);
					switch_channel_set_variable(channel, "vox_termination_sig_node_id", SipServer.sip_sig_node_id);
					switch_channel_set_variable(channel, "vox_termination_med_node_id", SipServer.sip_media_node_id);

					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : vox_termination_default_ipport [ %s ]\n", SipServer.opensips_default_ip);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : vox_termination_sig_node_id [ %s ]\n", SipServer.sip_sig_node_id);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : vox_termination_med_node_id [ %s ]\n", SipServer.sip_media_node_id);
					
				
				} else {
					switch_channel_set_variable(channel, "vox_termination_default_ipport", StrIng[0]);
					switch_channel_set_variable(channel, "vox_termination_sig_node_id", StrIng[1]);
					switch_channel_set_variable(channel, "vox_termination_med_node_id", StrIng[2]);
					
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : vox_termination_default_ipport [ %s ]\n", receiveData);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : vox_termination_sig_node_id [ %s ]\n", StrIng[1]);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : vox_termination_med_node_id [ %s ]\n", StrIng[2]);
				}
				switch_safe_free(receiveData);
				
				//For cdr record
				switch_channel_set_variable(channel, "vox_termination_equipment_id", redirect_term_id);
				switch_channel_export_variable(channel, "vox_termination_equipment_id", redirect_term_id, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
                                
				/**
				 * @Function SIP Termination Equipment validation
				 */
				ret = switch_manage_termination_equipment(atoi(redirect_term_id), channel, session, source_number, destination_number);
                                
                                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-TE] : ret [ %d ]\n", ret);
                                
// 				if(ret < 0) {
//                                         
// 					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-TE] : Failed To Handle Termination Equipment\n");
//                                         
// 					goto end;
// 				} else if(ret == 1) {
// 					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-TE] : Continue for next SIP Termination Equipment.\n");
// 					//goto reroute;
// 					continue;
// 				} else if(ret == 0) {
// 					break;
// 				}
				
                                

                        }
                }
		
		
		
		
		
		
		
		
		
	}

end:

	switch_channel_export_variable(channel, "cdr_call_flag", "1", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);

	return;
}

/*! vox sbc switch api */
SWITCH_STANDARD_API(switch_custom_api)
{
    char *mydata = NULL;
	char *argv[2] = {0};
	char *function = NULL;
	int argc;
	
	mydata = strdup(cmd);
	switch_assert(mydata);

	stream->write_function(stream, "mydata:%s\n", mydata);
	argc = switch_separate_string(mydata, ' ', argv, sizeof(argv)/sizeof(argv[0]));
	if(argc < 1) {
		stream->write_function(stream, "[ ORIGINATION ] : Invalide arguments\n");
		return SWITCH_STATUS_TERM;
	}

	function = switch_mprintf("%s", argv[0]);
	stream->write_function(stream, "[ ORIGINATION ] : Proceed For [ %s ]\n", function);
	
	if(!zstr(function) && !strcmp(function, "UPDATE_DEFAULT_MAX_CALL_DURATION")) {
		globals.default_max_call_dur = strdup(argv[1]);
		stream->write_function(stream, "Default Max call Duration [ %s ] updated\n", globals.default_max_call_dur);
		stream->write_function(stream, "UPDATED");
	} else {
		goto done;
	}
	
done:

	if(mydata)
		switch_safe_free(mydata);

	switch_safe_free(function);
	
	return SWITCH_STATUS_SUCCESS;
}

// Load module when FS start
SWITCH_MODULE_LOAD_FUNCTION(mod_termination_load)
{
	switch_application_interface_t *app_interface;
	switch_status_t status;
	switch_api_interface_t *api_interface;

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

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-TE] : Loading termination Module.\n");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[AMAZE-TE] : ======================================\n");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s", banner);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[AMAZE-TE] : ======================================\n");
	
	//Define termination application
	SWITCH_ADD_APP(app_interface, "termination", "termination", "termination", switch_termination_app, NULL, SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);
	
	//Define routing api
	SWITCH_ADD_API(api_interface, "termination_config", "SWITCH configuration", switch_custom_api, NULL);
	
	
	return SWITCH_STATUS_SUCCESS;
}

//shutdown module or execute when module unloaded from FS or Stop FS.
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_termination_shutdown)
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



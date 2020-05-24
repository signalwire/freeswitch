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
 * mod_origination.c  Origination Equipment module
 * 
 */

//-------------------------------------------------------------------------------------//
//  To include Header Guard
//-------------------------------------------------------------------------------------//

#ifndef ORIGINATION_HEADER
#define ORIGINATION_HEADER

//-------------------------------------------------------------------------------------//
//  Include Header Files.
//-------------------------------------------------------------------------------------//

#include <switch.h> 

//-------------------------------------------------------------------------------------//
//  Function Prototype
//-------------------------------------------------------------------------------------//

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_origination_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_origination_runtime);
SWITCH_MODULE_LOAD_FUNCTION(mod_origination_load);
SWITCH_MODULE_DEFINITION(mod_origination, mod_origination_load, mod_origination_shutdown, NULL);

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
" * mod_origination.c Origination Equipment module.\n"
" */\n";

static char *line = "++++++++++++++++++++++++++++++++++++++++++++++++++++++++";


#ifndef  SWITCH_ORIGINATION_SUCCESS
#define  SWITCH_ORIGINATION_SUCCESS            0              /*! ON SUCCESS */
#define  SWITCH_ORIGINATION_FAILED            -1              /*! ON FAILED */
#define  SWITCH_ORIGINATION_SQLITE_DBNAME     "amazeswitch"        /*! DEFAULT DB NAME */
#endif

static const char *global_cf = "amazeswitch.conf";                  /*! Config File Name */

static struct {
	char *odbc_dsn;                          /*! ODBC DSN */
	char *dbname;                            /*! DBNAME */
	char *profile;                           /*! REDIS PROFILE NAME */
	char *sound_path;                    /*! SOUND PREFIX PATH */
	char *default_max_call_dur;              /*! MAX DEFAULT CALL DURATION */
	switch_mutex_t *mutex;                   /*! MUTEX */  
	switch_memory_pool_t *pool;              /*! MEMORY POOL */
} globals;                                   /*! STRUCTURE VARIABLE */  

struct origination_equipment_handler {
	unsigned int orig_id;                               /*! ORIGINATION EQUIPMENT ID */
	char *orig_wait_time_answer;                        /*! ORIGINATION EQUIPMENT CALL TIMEOUT */
	char *orig_wait_time_rbt;                           /*! ORIGINATION EQUIPMENT PROGRESS TIMEOUT */
	char *orig_session_timeout;                         /*! ORIGINATION EQUIPMENT SESSION TIMEOUT */ 
	char *orig_rtp_timeout;                             /*! ORIGINATION EQUIPMENT RTP TIMEOUT */
	unsigned int group_id;                              /*! ORIGINATION EQUIPMENT CODEC GROUP ID */
	char *orig_sip_header;                              /*! ORIGINATION EQUIPMENT SOURCE NUMBER SIP HEADER */
	char *orig_ring_tone;                               /*! ORIGINATION EQUIPMENT RINGBACK */  
	unsigned int af_id;                                 /*! ORIGINATION EQUIPMENT RINGBACK FILE ID */      
	char *orig_max_sec;                             /*! ORIGINATION EQUIPMENT MAX CALL DURATION ALLOWED */
	char *orig_codec_policy;                            /*! ORIGINATION EQUIPMENT CODEC POLICY */
	char *orig_delay_bye_time;                          /*! ORIGINATION EQUIPMENT SEND SEND HANGUP DELAY TIME */
};
typedef struct origination_equipment_handler origination_equipment_handler_st;

struct regex_master {
	char *change_source;                              /*! ORIGINATION EQUIPMENT CHANGE SOURCE NUMBER REGEX STRING */
	char *change_destination;                         /*! ORIGINATION EQUIPMENT CHANGE DESTINATION NUMBER REGEX STRING */
};
typedef struct regex_master regex_master_st;

SWITCH_STANDARD_APP(switch_origination_app);
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
 		    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-OE] : Your resource limit is exceeded...!!!\n");
// 		    switch_channel_set_variable(channel, "transfer_string", "cg_cps_exceeded XML CG_CPS_EXCEEDED");
		    ret = -1;
		    goto end;
	    }
    }
    
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[AMAZE-OE] : ********* LIMIT SET SUCCESSFULLY *********\n");
    
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
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : api_cmd : \n%s\n", api_cmd);
		switch_safe_free(api_cmd);

		api_result = (char *)switch_channel_get_variable(channel, "api_result");
		if(!zstr(api_result)) {
			char *switch_result = NULL;
			switch_result = switch_mprintf("%s", api_result);
			if(!strcmp(switch_result, "true")) {
			    
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-OE] : Routing is Disallow on this number.\n");
				status = switch_mprintf("true");
				switch_safe_free(switch_result);
				switch_channel_set_variable(channel, "transfer_string", "REGEX_DISALLOW_NUMBER XML REGEX_DISALLOW_NUMBER");
				goto end;
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-OE] : [ %s ] is passed to regex string [ %s ].\n", source, regex_str);
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
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : api_cmd : %s\n",api_cmd);
		switch_safe_free(api_cmd);

		api_result = (char *)switch_channel_get_variable(channel, "api_result");

		if(!zstr(api_result)) {
			char *switch_result = NULL;
			
			switch_result = switch_mprintf("%s", api_result);
			if(!strcmp(switch_result, "false")) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-OE] : validation false.\n");
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
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : regex_str : %s\n",regex_str);
		
		token = strtok(string, seprator);
		while( token != NULL) {
			char *ptr = NULL;
			char *api_result = NULL;
			
			ptr = switch_mprintf("%s", token);
			switch_replace_character(ptr, '/', '|');
			switch_replace_character(ptr, '\\', '%'); //replace $1 by %1
			ptr[strlen(ptr)-1]  = '\0'; //to remove $ from string end
			
			api_cmd = switch_mprintf("api_result=${regex(%s|%s}", source, ptr);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : api_cmd : %s\n", api_cmd);
			switch_core_session_execute_application(session, "set", api_cmd);
			api_result = (char *)switch_channel_get_variable(channel, "api_result");
			
			if(!zstr(api_result)) {
				char *switch_result = NULL;
				switch_result = switch_mprintf("%s", api_result);
				source = strdup(switch_result);
				
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : api_result : %s\n", api_result);
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
	switch_status_t status = SWITCH_STATUS_SUCCESS;     /*! Loading Config Status */
	switch_xml_t cfg, xml, settings, param;             /*! XML Config Params */
	char result[50] = ""; 
	char *sql = NULL;
	  
	if (!(xml = switch_xml_open_cfg(global_cf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-OE] : Open of %s failed\n", global_cf);
		status = SWITCH_STATUS_TERM;
		goto end;
	}
	
	switch_mutex_lock(globals.mutex);

	if ((settings = switch_xml_child(cfg, "settings"))) {
		for (param = switch_xml_child(settings, "param"); param; param = param->next) {
			char *var = (char *) switch_xml_attr_soft(param, "name");
			char *val = (char *) switch_xml_attr_soft(param, "value");

			// Load dbname in variable
			if (!strcasecmp(var, "dbname")) {
				globals.dbname = strdup(val);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : dbname [ %s ]\n", globals.dbname);
			} else if (!strcasecmp(var, "odbc-dsn")) {
				globals.odbc_dsn = strdup(val);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : odbc-dsn [ %s ]\n", globals.odbc_dsn);
			} else if (!strcasecmp(var, "redis-profile")) {
				globals.profile = strdup(val);
				switch_log_printf(SWITCH_CHANNEL_LOG,SWITCH_LOG_DEBUG5, "[AMAZE-OE] : redis-profile [ %s ]\n", globals.profile);
			} else if (!strcasecmp(var, "sound-path")) {
				globals.sound_path = strdup(val);
				switch_log_printf(SWITCH_CHANNEL_LOG,SWITCH_LOG_DEBUG5, "[AMAZE-OE] : sound-path [ %s ]\n", globals.sound_path);
			}
		}
	}
		
	// SQL To Get Default MAX Call Duartion 
	sql = switch_mprintf("SELECT cfg_value FROM vca_global_config WHERE cfg_key='max_call_duration' limit 1");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : Get Default MAX Call Duration SQL :: \n%s\n",sql);
	switch_execute_sql2str(NULL, sql, result, sizeof(result));
	switch_safe_free(sql);
	
	if(!zstr(result)) {
		globals.default_max_call_dur = strdup(result);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : Default Max Call Duration :: %s\n", globals.default_max_call_dur);
	}

	if (!globals.dbname) {
		//use default db name if not specify in configure
		globals.dbname = strdup(SWITCH_ORIGINATION_SQLITE_DBNAME);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : set default DB :%s\n", globals.dbname);
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-OE] : Loaded Configuration successfully\n");
	
	end:
	switch_mutex_unlock(globals.mutex);
	
	//free memory
	if (xml) {
		switch_xml_free(xml);
	}

	return status;
}

static switch_cache_db_handle_t *switch_get_db_handler(void)
{
	switch_cache_db_handle_t *dbh = NULL;    /*! ODBC DSN HANDLER */
	char *dsn;                               /*! ODBC DSN STRING */

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

static switch_bool_t switch_execute_sql_callback(switch_mutex_t *mutex, char *sql, switch_core_db_callback_func_t callback, void *pdata)
{
	switch_bool_t ret = SWITCH_FALSE;          /*! SQL Query Execution Status */
	char *errmsg = NULL;                       /*! SQL Query Execution ERROR String */
	switch_cache_db_handle_t *dbh = NULL;      /*! MySQL odbc dsn Handler */
	
	if (mutex) {
		switch_mutex_lock(mutex);
	} else {
		switch_mutex_lock(globals.mutex);
	}

	if (!(dbh = switch_get_db_handler())) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-OE] : Error Opening DB\n");
		goto end;
	}

	switch_cache_db_execute_sql_callback(dbh, sql, callback, pdata, &errmsg);
	if (errmsg) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-OE] : SQL ERR: [%s] %s\n", sql, errmsg);
		free(errmsg);
	}

	end:
	switch_cache_db_release_db_handle(&dbh);

	if (mutex) {
		switch_mutex_unlock(mutex);
	} else {
		switch_mutex_unlock(globals.mutex);
	}

	return ret;
}

static char *switch_execute_sql2str(switch_mutex_t *mutex, char *sql, char *resbuf, size_t len)
{
	char *ret = NULL;                          /*! SQL Query Result */
	switch_cache_db_handle_t *dbh = NULL;      /*! MySQL ODBC DSN Handler */   

	if (mutex) {
		switch_mutex_lock(mutex);
	} else {
		switch_mutex_lock(globals.mutex);
	}

	if (!(dbh = switch_get_db_handler())) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[ SWITCH ] : Error Opening DB\n");
		goto end;
	}
	ret = switch_cache_db_execute_sql2str(dbh, sql, resbuf, len, NULL);

	end:
	switch_cache_db_release_db_handle(&dbh);

	if (mutex) {
		switch_mutex_unlock(mutex);
	} else {
		switch_mutex_unlock(globals.mutex);
	}

	//return SQL result.
	return ret;
}

static int switch_originator_callback(void *ptr, int argc, char **argv, char **col) 
{
	origination_equipment_handler_st *oe = (origination_equipment_handler_st *)ptr;         /*! ORIGINATION EQUIPMENT STRUCTURE POINTER */
	int index = 0;                                                                          /*! ORIGINATION EQUIPMENT Query Coloum */

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : %s\n", line);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : ======== ORIGINATION EQUIPMENT INFORMATION ========\n");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : %s\n", line);

	for(index = 0; index < argc ; index++) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : %s : %s\n", col[index], argv[index]);
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : %s\n", line);
	
	oe->orig_id = atoi(argv[0]);                           /*! ORIGINATION EQUIPMENT ID */
	oe->orig_max_sec = strdup(argv[1]);                /*! ORIGINATION EQUIPMENT MAX CALL DURATION IN SECONDS */
	oe->orig_wait_time_answer = strdup(argv[2]);           /*! ORIGINATION EQUIPMENT CALL TIMEOUT */
	oe->orig_wait_time_rbt = strdup(argv[3]);              /*! ORIGINATION EQUIPMENT PROGRESS TIMEOUT */
	oe->orig_session_timeout = strdup(argv[4]);            /*! ORIGINATION EQUIPMENT SESSION TIMEOUT */
	oe->orig_rtp_timeout = strdup(argv[5]);                /*! ORIGINATION EQUIPMENT RTP TIMEOUT */
	oe->group_id = atoi(argv[6]);                          /*! ORIGINATION EQUIPMENT CODEC GROUP ID */
	oe->orig_sip_header = strdup(argv[7]);                 /*! ORIGINATION EQUIPMENT SOURCE NUMBER SIP HEADER */
	oe->orig_ring_tone = strdup(argv[8]);                  /*! ORIGINATION EQUIPMENT RINGBACK */ 
	oe->af_id = atoi(argv[9]);                             /*! ORIGINATION EQUIPMENT RINGBACK FILE ID */
	oe->orig_codec_policy = strdup(argv[10]);              /*! ORIGINATION EQUIPMENT CODEC POLICY */
	oe->orig_delay_bye_time = strdup(argv[11]);            /*! ORIGINATION EQUIPMENT SEND BYE BY DELAY TIME */
	
	return SWITCH_ORIGINATION_SUCCESS;
}

static int switch_regex_callback(void *ptr, int argc, char **argv, char **col) 
{
	regex_master_st *oe = (regex_master_st *)ptr;          /*! Regex Master Pointer */
	int index = 0;                                         /*! regex coloum count */

	for(index = 0; index < argc ; index++) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : %s : %s\n", col[index], argv[index]);
	}
	
	if(!strcmp(argv[0], "CHANGE_SOURCE")) {                               /*! Copy Source Number Manipulation String */
		oe->change_source = strdup(argv[1]);
	} else if(!strcmp(argv[0], "CHANGE_DESTINATION")) {                   /*! Copy Destination Number Manipulation String */ 
		oe->change_destination = strdup(argv[1]);
	}
	
	return SWITCH_ORIGINATION_SUCCESS;
}

static switch_status_t switch_execute_sql(char *sql, switch_mutex_t *mutex)
{
	switch_cache_db_handle_t *dbh = NULL;            /*! odbc dsn Handler */
	switch_status_t status = SWITCH_STATUS_FALSE;    /*! SQL Execution Status */

	if (mutex) {
		switch_mutex_lock(mutex);
	} else {
		switch_mutex_lock(globals.mutex);
	}

	if (!(dbh = switch_get_db_handler())) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[ DIALPEER ] : Error Opening DB\n");
		goto end;
	}
	status = switch_cache_db_execute_sql(dbh, sql, NULL);

	end:
	switch_cache_db_release_db_handle(&dbh);
	if (mutex) {
		switch_mutex_unlock(mutex);
	} else {
		switch_mutex_unlock(globals.mutex);
	}
	return status;
}
SWITCH_STANDARD_APP(switch_origination_app)
{
	char *souce_context_info = NULL;                              /* context header value from opensips */
	char *argv[9] = {0};                                          /* string tokenizing */
	char *transfer = NULL;                                        /* Transfer String */
	char *ip_zone_id = NULL;                                      /* origination equipment IP ZONE id */
	char *media_zone_id = NULL;                                   /* origination equipment MEDIA ZONE id */
	char *oequip_id = NULL;                                       /* origination equipment id */
	char *oequip_cps = NULL;                                      /* origination equipment max cps.*/
	char *oequip_mc = NULL;                                       /* origination equipment max calls. */
	char *capacity_gp_cps = NULL;                                 /* origination equipment capacity group max cps. */
	char *capacity_gp_mc = NULL;                                  /* origination equipment capacity group max calls. */
	char * media_proxy_mode = NULL;                               /* origination equipment media proxy mode */
	char *capacity_gp_id = NULL;                                  /* originateon equipment capacity group id. */
	char *source_number = NULL;                                   /* originateon equipment Source Number from opensips */
	char *destination_number = NULL;                              /* originateon equipment destination number */
	const char *ep_codec_prefer_sdp = NULL;                       /* originateon equipment read codec list */
	char *sql = NULL;                                             /* SQL Query */
	char *source_billing_number = NULL;                           /* originateon equipment Source Billing Number */
	char *destination_billing_number = NULL;                      /* originateon equipment Destination Billing Number */ 
	char *offer_codec_str = NULL;                                 /* originateon equipment Offer codec string */
	char *tmp_num = NULL;                                         /* temp store number */
	origination_equipment_handler_st originator;                  /* originateon equipment structure variable */
	regex_master_st regex;                                        /* originateon equipment regex struture variable */
	char *app_data = NULL;

	switch_channel_t *channel = switch_core_session_get_channel(session);
	
	//Log on console
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-OE] : Origination Equpment Routing Started.\n");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-OE] : Origination application called.\n");
	if(!channel) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-OE] : Hey I am unable to get channel..!!\n");
		switch_channel_set_variable(channel, "switch_hangup_code", "501");
		switch_channel_set_variable(channel, "switch_hangup_reason", "FACILITY_NOT_IMPLEMENTED	");
		switch_channel_set_variable(channel, "transfer_string", "SWITCH_HANGUP XML SWITCH_HANGUP");
		goto end;
	}
	switch_channel_set_variable(channel, "call_is_from_origination", "yes");
	
	souce_context_info = (char *)switch_channel_get_variable(channel, "sip_h_X-CONTEXT-INFO");
	if(zstr(souce_context_info) || strlen(souce_context_info)<=0 || !strcmp(souce_context_info, "<null>")) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-OE] : Routing is unable to get context info.\n");
		switch_channel_set_variable(channel, "switch_hangup_code", "501");
		switch_channel_set_variable(channel, "switch_hangup_reason", "FACILITY_NOT_IMPLEMENTED	");
		switch_channel_set_variable(channel, "transfer_string", "SWITCH_HANGUP XML SWITCH_HANGUP");
		switch_channel_export_variable(channel, "cdr_call_flag", "1", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
		goto end;  
	}
	
	destination_number = switch_mprintf("%s", (char *)switch_channel_get_variable(channel, "destination_number"));

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : origination Equipment channel name [ %s ]\n", switch_channel_get_name(channel));
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : origination Equipment call uuid [ %s ]\n", switch_core_session_get_uuid(session));
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : origination Equipment IP [ %s ]\n", (char *)switch_channel_get_variable(channel, "sip_h_X-REMOTE-INFO"));
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : origination Equipment call ID <sip_call_id> [ %s ]\n", (char *)switch_channel_get_variable(channel, "sip_call_id"));
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : origination Equipment inbound call protocol [ %s ]\n", (char *)switch_channel_get_variable(channel, "sip_via_protocol"));
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : Origination Equipment Destination Number [ %s ]\n", destination_number);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : vox switch local network ip address [ %s ]\n", (char *)switch_channel_get_variable(channel, "sip_local_network_addr"));

	switch_channel_export_variable(channel, "vox_originator_sip_call_id",switch_channel_get_variable(channel, "sip_call_id"), SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
	switch_channel_export_variable(channel, "vox_originator_ipaddress", switch_channel_get_variable(channel, "sip_h_X-REMOTE-INFO"), SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
	switch_channel_export_variable(channel, "vox_originator_org_destination_number", destination_number, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
	switch_channel_export_variable(channel, "vox_originator_call_uuid", switch_core_session_get_uuid(session), SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
	switch_channel_export_variable(channel, "vox_sip_local_network_addr", switch_channel_get_variable(channel, "sip_local_network_addr"), SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
	switch_channel_set_variable(channel, "sip_h_X-CONTEXT-INFO", NULL);

	// Log SIP context Header
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-OE] : souce_context_info : %s\n", souce_context_info);
	//contact info :: ip_zone, med_zone,equip_id,OE_CPS,OE_MC,CG_CPS,CG_MC,med_proxy

	switch_separate_string(souce_context_info, ',', argv, (sizeof(argv) / sizeof(argv[0])));
	if(zstr(souce_context_info)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-OE] : Routing is unable to get context info.\n");
		switch_channel_set_variable(channel, "switch_hangup_code", "501");
		switch_channel_set_variable(channel, "switch_hangup_reason", "FACILITY_NOT_IMPLEMENTED	");
		switch_channel_set_variable(channel, "transfer_string", "SWITCH_HANGUP XML SWITCH_HANGUP");
		switch_channel_export_variable(channel, "cdr_call_flag", "1", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
		goto end;
	}

	ip_zone_id = switch_mprintf("%s", argv[0]);               /*! ORIGINATION EQUIPMENT IP ZONE ID */
	media_zone_id = switch_mprintf("%s", argv[1]);            /*! ORIGINATION EQUIPMENT MEDIA ZONE ID */ 
	oequip_id = switch_mprintf("%s", argv[2]);                /*! ORIGINATION EQUIPMENT ID */
	oequip_cps = switch_mprintf("%s", argv[3]);               /*! ORIGINATION EQUIPMENT MAX CPS */ 
	oequip_mc = switch_mprintf("%s", argv[4]);                /*! ORIGINATION EQUIPMENT MAX CALL */ 
	media_proxy_mode = switch_mprintf("%s", argv[5]);         /*! ORIGINATION EQUIPMENT MEDIA PROXY MODE */
	capacity_gp_id = switch_mprintf("%s", argv[6]);           /*! ORIGINATION EQUIPMENT CAPACITY GROUP ID */
	
	if(!zstr(capacity_gp_id) && atoi(capacity_gp_id) > 0) {
		char msgbuf[10] = "";
		char *switchargv[3] = {0};                                          /* string tokenizing */
	  
		sql = switch_mprintf("SELECT concat(cpg_max_calls,',',cpg_max_calls_sec) as capacity_gp_data FROM  vca_capacity_group WHERE cpg_id = '%s' AND cpg_status='Y'",capacity_gp_id);
		
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : Proceed To Get Origination Equipment Capacity Group Data SQL :: \n%s\n",sql);
		switch_execute_sql2str(NULL, sql, msgbuf, sizeof(msgbuf));
		switch_safe_free(sql);
		
		if(zstr(msgbuf)) {
			capacity_gp_cps = strdup("0");
			capacity_gp_mc = strdup("0");
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-OE] : Proceed To Get Origination Equipment Capacity Group Data  [ %s ]\n",msgbuf);
			switch_separate_string(msgbuf, ',', switchargv, (sizeof(switchargv) / sizeof(switchargv[0])));
			capacity_gp_mc = switch_mprintf("%s", switchargv[0]);           /*! ORIGINATION EQUIPMENT CAPACITY GROUP MAX CALL */
			capacity_gp_cps = switch_mprintf("%s", switchargv[1]);          /*! ORIGINATION EQUIPMENT CAPACITY GROUP MAX CPS */
		}
	  
	} else {
		capacity_gp_cps = strdup("0");
		capacity_gp_mc = strdup("0");
	}
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : --------- SOURCE CONTEXT INFORMATION OF ORIGINATION EQUIPMENT ---------\n");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : Origination Equipment ip zone id [ %s ]\n", ip_zone_id);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : Origination Equipment media zone id [ %s ]\n", media_zone_id);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : Origination Equipment id [ %s ]\n", oequip_id);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : Origination Equipment max cps limt [ %s ]\n", oequip_cps);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : Origination Equipment max call limit [ %s ]\n", oequip_mc);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : Origination Equipment capacity group cps limit [ %s ]\n", capacity_gp_cps);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : Origination Equipment capacity group max call limit [ %s ]\n", capacity_gp_mc);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : Origination Equipment media proxy mode [ %s ]\n", media_proxy_mode);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : Origination Equipment capacity group id [ %s ]\n", capacity_gp_id);
	
	switch_channel_export_variable(channel, "vox_origination_equipment_id", oequip_id, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
	switch_channel_export_variable(channel, "vox_origination_equipment_ip_zone_id", ip_zone_id, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
	switch_channel_export_variable(channel, "vox_origination_equipment_media_zone_id", media_zone_id, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
	switch_channel_export_variable(channel, "vox_origination_equipment_media_proxy_mode", media_proxy_mode, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
	switch_channel_export_variable(channel, "vox_origination_equipment_capacity_gp_id", capacity_gp_id, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);

	if(!strcmp(media_proxy_mode, "FLOW_AROUND")) {
		switch_channel_set_variable(channel, "vox_bypass_media", "true");              /*! Enable bypass media in FreeSWITCH */
	} else {
		switch_channel_set_variable(channel, "vox_bypass_media", "false");             /*! Disable bypass media in FreeSWITCH */
	}
	
	if(zstr(capacity_gp_cps) || zstr(capacity_gp_id)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-OE] : Origination Equipment [ %s ] capacity group cps is not set so system is unable to proceed further.\n",oequip_id);
		switch_channel_set_variable(channel, "switch_hangup_code", "501");
		switch_channel_set_variable(channel, "switch_hangup_reason", "FACILITY_NOT_IMPLEMENTED	");
		switch_channel_set_variable(channel, "transfer_string", "SWITCH_HANGUP XML SWITCH_HANGUP");
		switch_channel_export_variable(channel, "cdr_call_flag", "1", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
		goto end;
	}
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-OE] : Origination Equipment [ %s ] Capacity Group [ %s ] < MAX CPS > varifying\n",oequip_id, capacity_gp_id);
	
	if(atoi(capacity_gp_cps) > 0 ) {
	  
		char *hiredis_raw_response = NULL;                                 /*! Hiredis Response */
		char *idname = switch_mprintf("%s_cg_max_cps", capacity_gp_id);    /*! Capacity Group Max CPS Key */
		int retval = 0;                                                    /*! return value from function */
		
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[AMAZE-OE] : ********* PROCEED TO SET LIMIT *********\n");
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : idname : %s\n", idname);

		retval = switch_check_resource_limit("hiredis", session, globals.profile, idname, atoi(capacity_gp_cps), 1, "CAPACITY_GP_CPS_EXCEEDED"); 
	
		//get hiredis response from hiredis module.
		hiredis_raw_response = switch_mprintf("%s",switch_channel_get_variable(channel, "vox_hiredis_response"));
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : hiredis_raw_response : %s\n",hiredis_raw_response); 

		if((strlen(hiredis_raw_response)>0 && !strcasecmp(hiredis_raw_response, "success") )) {
			switch_safe_free(idname);
			if(retval != 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-OE] : Origination Equipment [ %s ] Capacity Group [ %s ] MAX CPS is Exceeded\n", oequip_id, capacity_gp_id);
				switch_channel_set_variable(channel, "transfer_string", "CAPACITY_GP_CPS_EXCEEDED XML CAPACITY_GP_CPS_EXCEEDED");
				switch_channel_export_variable(channel, "cdr_call_flag", "1", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
				goto end;
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[AMAZE-OE] : Hiredis is unable to connect redis server [ %s ], Origination Equipment [ %s ] Capacity Group [ %s ] MAX CPS is set for unlimited.\n",hiredis_raw_response, oequip_id, capacity_gp_id);
		}
		//free memory
		switch_safe_free(hiredis_raw_response);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-OE] : Origination Equipment [ %s ] Capacity Group MAX CPS is set for unlimited.\n", oequip_id);
	}
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-OE] : Origination Equipment [ %s ] Capacity Group [ %s ] MAX CPS varified Successfully\n", oequip_id, capacity_gp_id);

	if(zstr(capacity_gp_mc)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-OE] : Origination Equipment [ %s ] is unable to get Capacity Group [ %s ] MAX calls.\n",oequip_id,capacity_gp_id);
		switch_channel_set_variable(channel, "switch_hangup_code", "400");
		switch_channel_set_variable(channel, "switch_hangup_reason", "NORMAL_TEMPORARY_FAILURE");
		switch_channel_set_variable(channel, "transfer_string", "SWITCH_HANGUP XML SWITCH_HANGUP");
		switch_channel_export_variable(channel, "cdr_call_flag", "1", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
		goto end;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-OE] : Origination Equipment [ %s ] Capacity Group [ %s ] < MAX CALL > varifying\n", oequip_id, capacity_gp_id);
	
	if(atoi(capacity_gp_mc) > 0 ) {
	  
		char *idname = switch_mprintf("%s_cg_max_call", capacity_gp_id);         /*! Capacity Group Max Calls Key */
		int retval = 0;                                                          /*! Function return value */
		char *hiredis_raw_response = NULL;                                       /*! Hiredis Response */  
		
		//Log on console
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[AMAZE-OE] : ********* PROCEED TO SET LIMIT *********\n");
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : idname : %s\n", idname);

		retval = switch_check_resource_limit("hiredis", session, globals.profile, idname,  atoi(capacity_gp_mc), 0, "CG_MC_EXCEEDED");

		//get hiredis response from hiredis module.
		hiredis_raw_response = switch_mprintf("%s",switch_channel_get_variable(channel, "vox_hiredis_response"));
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : hiredis_raw_response : %s\n",hiredis_raw_response); 

		if((strlen(hiredis_raw_response)>0 && !strcasecmp(hiredis_raw_response, "success") )) {
			if(retval != 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-OE] : Originate Equipment [ %s ] Capacity Group [ %s ] Max Call Limit Exceeded\n", oequip_id,capacity_gp_id);
				switch_channel_set_variable(channel, "transfer_string", "CG_MC_EXCEEDED XML CG_MC_EXCEEDED");
				switch_channel_export_variable(channel, "cdr_call_flag", "1", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
				goto end;
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[AMAZE-OE] : Hiredis is unable to connect redis server [ %s ], Origination Equipment [ %s ] Capacity Group [ %s ] MAX CALL is set for unlimited.\n",hiredis_raw_response, oequip_id, capacity_gp_id);
		}
		
		//free memory
		switch_safe_free(idname);
		switch_safe_free(hiredis_raw_response);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-OE] : Origination Equipment [ %s ] Capacity Group MAX CALL is set for unlimited.\n", oequip_id);
	}
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-OE] : Origination Equipment [ %s ] Capacity Group [ %s ] MAX CALL varified Successfully\n", oequip_id, capacity_gp_id);

	if(zstr(oequip_cps)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-OE] : Originate equipment [ %s ] CPS is not set so system is unable to proceed further.\n", oequip_id);
		switch_channel_set_variable(channel, "switch_hangup_code", "400");
		switch_channel_set_variable(channel, "switch_hangup_reason", "NORMAL_TEMPORARY_FAILURE");
		switch_channel_set_variable(channel, "transfer_string", "SWITCH_HANGUP XML SWITCH_HANGUP");
		switch_channel_export_variable(channel, "cdr_call_flag", "1", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
		goto end;
	}
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-OE] : proceed to verify Origination Equipment [ %s ] MAX CPS limit\n", oequip_id);
	if(atoi(oequip_cps) > 0 ) {
	  
		char *idname = switch_mprintf("%s_oe_max_cps", oequip_id);      /*! Origination Equipment Max CPS Key */
		int retval = 0;                                                 /*! Function return value */
		char *hiredis_raw_response = NULL;                              /*! Hiredis Response */
		
		//Log on console
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[AMAZE-OE] : ********* PROCEED TO SET LIMIT *********\n");
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : idname : %s\n", idname);
		
		/**
		 * @function check key value in redis server <KEY = [ORIGINATION-EQUIPMENT-ID]_oe_max_cps >. 
		 */
		
		retval = switch_check_resource_limit("hiredis", session, globals.profile, idname,  atoi(oequip_cps), 1, "CHECK_ORIG_EQUIPMENT_CPS"); 
		
		//get hiredis response from hiredis module.
		hiredis_raw_response = switch_mprintf("%s",switch_channel_get_variable(channel, "vox_hiredis_response"));
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : hiredis_raw_response : %s\n",hiredis_raw_response); 

		/**
		 * @section maxcps validation
		 */
		
		if((strlen(hiredis_raw_response)>0 && !strcasecmp(hiredis_raw_response, "success") )) {
			  
			  if(retval != 0) {
				  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-OE] : Originate Equipment [ %s ]  CPS Limit Exceeded\n", oequip_id);
				  switch_channel_set_variable(channel, "transfer_string", "oe_cps_exceeded XML OE_CPS_EXCEEDED");
				  switch_channel_export_variable(channel, "cdr_call_flag", "1", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
				  
				  goto end;
			  }
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[AMAZE-OE] : Hiredis is unable to connect redis server [ %s ], Origination Equipment [ %s ]  MAX CPS is set for unlimited.\n",hiredis_raw_response, oequip_id);
		}
		
		//free memory
		switch_safe_free(idname);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-OE] : Originate Equipment [ %s ] CPS is not set, so system will consider it as unlimited.\n", oequip_id);
	}
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-OE] : proceed to verify Origination Equipment [ %s ] MAX CALL limit\n", oequip_id);

	if(zstr(oequip_mc)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-OE] : Originate Equipment max call is not set so system is unable to proceed further.\n");
		switch_channel_set_variable(channel, "switch_hangup_code", "400");
		switch_channel_set_variable(channel, "switch_hangup_reason", "NORMAL_TEMPORARY_FAILURE");
		switch_channel_set_variable(channel, "transfer_string", "SWITCH_HANGUP XML SWITCH_HANGUP");
		switch_channel_export_variable(channel, "cdr_call_flag", "1", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
		goto end;
	}
	
	if(atoi(oequip_mc) > 0 ) {
		char *idname = switch_mprintf("%s_oe_max_call", oequip_id);             /*! Origination Equipment Max Call Key */
		int retval = 0;                                                         /*! Function Return value */
		char *hiredis_raw_response = NULL;                                      /*! Hiredis Response */
		
		//Log on console
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[AMAZE-OE] : ********* PROCEED TO SET LIMIT *********\n");
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : idname : %s\n", idname);

		/**
		 * @function check key value in redis server <KEY = [ORIGINATION-EQUIPMENT-ID]_oe_max_call >. 
		 */
		
		retval = switch_check_resource_limit("hiredis", session, globals.profile, idname,  atoi(oequip_mc), 0, "CHECK_ORIG_EQUIPMENT_MC"); 
		
		//get hiredis response from hiredis module.
		hiredis_raw_response = switch_mprintf("%s",switch_channel_get_variable(channel, "vox_hiredis_response"));
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : hiredis_raw_response : %s\n",hiredis_raw_response); 
		
		/**
		 * @section maxcall validation
		 */
		
		if((strlen(hiredis_raw_response)>0 && !strcasecmp(hiredis_raw_response, "success") )) {
			if(retval != 0) {
				int limit_status = SWITCH_STATUS_SUCCESS;      /*! Limit Status */
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-OE] : Originate Equipment [ %s ] Max Call Limit Exceeded\n", oequip_id);
				switch_channel_set_variable(channel, "transfer_string", "ORIGINATION_EQUIPMENT_MAX_CALL_EXCEEDED XML ORIGINATION_EQUIPMENT_MAX_CALL_EXCEEDED");
				switch_channel_export_variable(channel, "cdr_call_flag", "1", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
				
				/*! release hiredis resource */
				limit_status = switch_limit_release("hiredis", session, globals.profile, idname);
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : release [ %s ] [ %d ]\n", idname, limit_status);
				switch_safe_free(idname);
				
				goto end;
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[AMAZE-OE] : Hiredis is unable to connect redis server [ %s ], Origination Equipment [ %s ]  MAX CALL will not check.\n",hiredis_raw_response, oequip_id);
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-OE] : Originate Equipment [ %s ] Max Calls is not set, so system will consider it as unlimited.\n",oequip_id);
	}
	
	if(!zstr(oequip_id)) {
		char result[50] = "";           /*! Query Result */
		
		// SQL To Select User-Agent-Name 
		sql = switch_mprintf("SELECT param_value FROM vca_sofia_settings WHERE param_name='user-agent-string' AND sofia_id='2' limit 1");
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : Switch user-agent-string SQL :: \n%s\n",sql);
		switch_execute_sql2str(NULL, sql, result, sizeof(result));
		switch_safe_free(sql);
		if(!zstr(result)) {
			switch_channel_export_variable(channel, "vox_user_agent_name", result, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
		}
	}
	
	/**
	 * @var origination equipment structure variable.
	 */
	
	memset(&originator, 0, sizeof(originator));        /*! Initializing structure variable */
	originator.orig_id = 0;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-OE] : proceed to get Origination Equipment [ %s ] information.\n", oequip_id);

	/**
	 * @Section Get origination equipment information from MySQL database.
	 * @Query SELECT FROM origination equipment table.
	 */

	sql = switch_mprintf("SELECT orig_id, IF(isnull(orig_max_call_dur),0,orig_max_call_dur) as orig_max_call_dur, (IF(isnull(orig_wait_time_answer),0,orig_wait_time_answer)/1000) as orig_wait_time_answer, (IF(isnull(orig_wait_time_rbt),0,orig_wait_time_rbt)/1000) as orig_wait_time_rbt, IF(isnull(orig_session_time),0,orig_session_time) as orig_session_time, IF(isnull(orig_rtp_time),0,orig_rtp_time) as orig_rtp_time, group_id, orig_sip_header, orig_ring_tone, af_id, orig_codec_policy, IF(isnull(orig_delay_bye_time),0,orig_delay_bye_time*1000) as delay_bye_usec FROM vca_orig_equipment WHERE orig_id = '%s' AND orig_status = 'Y'", oequip_id);
	
	switch_execute_sql_callback(globals.mutex, sql, switch_originator_callback, &originator);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : Originate Equipment Information SQL : \n%s\n", sql);

	if(originator.orig_id == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-OE] : Routing is unable to get originate equipment information please verify it.\n");
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[AMAZE-OE] : System Is Unable To Get Data From SQL : \n\n%s\n\n", sql);
		switch_safe_free(sql);
		switch_channel_set_variable(channel, "switch_hangup_code", "400");
		switch_channel_set_variable(channel, "switch_hangup_reason", "NORMAL_TEMPORARY_FAILURE");
		switch_channel_set_variable(channel, "transfer_string", "SWITCH_HANGUP XML SWITCH_HANGUP");
		switch_channel_export_variable(channel, "cdr_call_flag", "1", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
		goto end;
	}
	//free memory
	switch_safe_free(sql);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : Origination Equipment [ %s ] has max call duration is [ %s ] seconds\n", oequip_id,  originator.orig_max_sec);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : Origination Equipment [ %s ] Delay Bye Time is [ %s ] useconds\n",oequip_id, originator.orig_delay_bye_time); 
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : Origination Equipment [ %s ] if call will terminate by originator than bleg will be Hangup after [ %s ] useconds.\n", oequip_id, originator.orig_delay_bye_time); 

	switch_channel_export_variable(channel, "vox_delay_bye_time", originator.orig_delay_bye_time, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
	switch_channel_set_variable(channel, "sip_bye_h_FROMBLEG", originator.orig_delay_bye_time);
	
	/*! If max call duration is zero than do not set it and call can be run unlimited */
	if(atoi(originator.orig_max_sec) > 0 ) {
		switch_channel_export_variable(channel, "vox_sched_seconds", originator.orig_max_sec, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
		switch_core_session_execute_application(session, "set", "execute_on_answer=execute_extension SET_SCHED_HANGUP XML SET_SCHED_HANGUP");
	} else {
		switch_channel_export_variable(channel, "vox_sched_seconds", globals.default_max_call_dur, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-OE] : Switch is used Default Max Call Duration [ %s ] for Origination Equipment [ %s ]\n",globals.default_max_call_dur, oequip_id); 
// 		switch_core_session_execute_application(session,"set", "rg_toll_allow");
		switch_core_session_execute_application(session, "set", "execute_on_answer=execute_extension SET_SCHED_HANGUP XML SET_SCHED_HANGUP");
	}
	
	/**
	 * @Section Getting Source Number From <FROM SIP Header> 
	 */
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-OE] : Proceed To Get Source Number For Origination Equipment [ %s ]\n",oequip_id); 
	if(!strcmp(originator.orig_sip_header, "FROM")) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-OE] : Proceed to Get Source Number From <FROM> header.\n");
		
		//get from channel variable ${sip_from_user}
		source_number = (char *)switch_channel_get_variable(channel, "sip_from_user");
		if(!zstr(source_number)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-OE] : Origination Equipment [ %s ] Get Source Number [ %s ] From <FROH> SIP Header.\n",oequip_id, source_number);
		}
	} 
	
	/**
	 * @Section Getting Source Number From <REMOTE-PARTY-ID SIP Header> 
	 */
	else if (!strcmp(originator.orig_sip_header, "REMOTE-PARTY-ID")) {
		char *calling_number = NULL;         /*! REMOTE-PARTY-ID SIP HEADER */
		
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-OE] : Proceed to Get Source Number From <REMOTE-PARTY-ID> header.\n");
		
		calling_number = (char *)switch_channel_get_variable(channel, "sip_Remote-Party-ID");   /*! REMOTE-PARTY-ID SIP HEADER */

		/**
		* @Section username from <SIP URI> 
		*/
		
		if(!zstr(calling_number)) {
			char *tmp = NULL;
			tmp = strstr(calling_number, "<sip:");
			if(tmp) {
				(*tmp) = '\0';
				source_number = switch_strip_quotes(calling_number);
			} else {
				source_number = strdup(calling_number);
			}
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-OE] : Origination Equipment [ %s ] Get Source Number [ %s ] From <REMOTE-PARTY-ID> SIP Header.\n",oequip_id, source_number);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-OE] : Origination Equipment [ %s ] Do Not Get <REMOTE-PARTY-ID> Header, So syetm is unable To get Source Number From <REMOTE-PARTY-ID>\n",oequip_id);
			source_number = "";
			
			// setting channel variable to hangle hangup cause.
			switch_channel_set_variable(channel, "switch_hangup_code", "401");
			switch_channel_set_variable(channel, "switch_hangup_reason", "CALL_REJECTED");
			switch_channel_set_variable(channel, "transfer_string", "SWITCH_HANGUP XML SWITCH_HANGUP");
			switch_channel_export_variable(channel, "cdr_call_flag", "1", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
			goto end;
		}
	}
	
	/**
	 * @Section Getting Source Number From <P-ASSERTED-ID SIP Header> 
	 */
	
	else if (!strcmp(originator.orig_sip_header, "P-ASSERTED-ID")) {
		char *calling_number = NULL;                 /*! P-ASSERTED-ID SIP HEADER */

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-OE] : Proceed to Get Source Number From <P-ASSERTED-ID> header.\n");
		
		//get from channel variable ${sip_P-Asserted-Identity} it will give URI
		calling_number = (char *)switch_channel_get_variable(channel, "sip_P-Asserted-Identity");
		if(!zstr(calling_number)) {
			char *tmp = NULL;
			tmp = strstr(calling_number, "<sip:");
			if(tmp) {
				(*tmp) = '\0';
				source_number = switch_strip_quotes(calling_number);
			} else {
				source_number = strdup(calling_number);
			}
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-OE] : Origination Equipment [ %s ] Get Source Number [ %s ] From <P-ASSERTED-ID> SIP Header.\n",oequip_id, source_number);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-OE] : source_number is not set\n");
			source_number = "";
			
			// setting channel variables for hangup response.
			switch_channel_set_variable(channel, "switch_hangup_code", "401");
			switch_channel_set_variable(channel, "switch_hangup_reason", "CALL_REJECTED");
			switch_channel_set_variable(channel, "transfer_string", "SWITCH_HANGUP XML SWITCH_HANGUP");
			switch_channel_export_variable(channel, "cdr_call_flag", "1", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
			goto end;			
		}
	} 
	
	/**
	 * @Section Error unable to get source number and respond with 401 CALL_REJECTED.
	 */
	
	else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-OE] : invalid originate equipment sip header.\n");
		
		//setting channel variable for hangup response
		switch_channel_set_variable(channel, "switch_hangup_code", "401");
		switch_channel_set_variable(channel, "switch_hangup_reason", "CALL_REJECTED");
		switch_channel_set_variable(channel, "transfer_string", "SWITCH_HANGUP XML SWITCH_HANGUP");
		goto end;
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-OE] : Origination Equipment [ %s ] Original Source Number [ %s ]\n",oequip_id, source_number);
	
	/**
	 * @var regex struture variable.
	 */
	
	memset(&regex, 0, sizeof(regex));           /* Initializing regex structure variable */
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-OE] : Proceed To Get Origination Equipment [ %s ] Regex information\n", oequip_id);
	
	/**
	 * @Section Getting Origination Equipment regex string from db. 
	 */
	sql = switch_mprintf("SELECT regex_type, group_concat(regex_string SEPARATOR '|') as regex_string  FROM `vca_regex_master` WHERE (regex_type='CHANGE_DESTINATION' OR regex_type='CHANGE_SOURCE') AND regex_equip_id='%s' AND `regex_equip_type`='ORIGINATION' GROUP BY regex_type",oequip_id);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : %s\n", line);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : ===== ORIGINATION EQUIPMENT REGEX INFORMATION =====\n");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : %s\n", line);
	
	//get origination equipment regex info
	switch_execute_sql_callback(globals.mutex, sql, switch_regex_callback, &regex);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : Origination Equipment Regex SQL : \n%s\n", sql);
	switch_safe_free(sql);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : %s\n", line);
	
	/**
	 * @Section Source Number Manipulation <ORIGINATION-EQUIPMENT>
	 */
	
	if(!zstr(regex.change_source)) {
		
		// Log on console
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-OE] : Proceed To Check Origination Equipment [ %s ] Source Number [ %s ] Regex [ %s ] Manipulation \n", oequip_id, source_number, regex.change_source);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-OE] : Origination Equipment [ %s ] Before Source Number Manipulation Source Number [ %s ].\n", oequip_id, source_number);
		
		tmp_num = strdup(source_number);
		
		/**
		 * @Section Source Number manipulate on Freeswitch Server and gives new number after regex manipulation
		 */
		
		source_number = switch_regex_manipulation(session, channel, regex.change_source, source_number);
		
		//if it will return true/false than do not change source number and regex failed.
		if(!strcmp(source_number, "true") || !strcmp(source_number, "false")) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[AMAZE-OE] : Origination Equipment [ %s ] Souce Number [ %s ] manipulation Regex [ %s ] is not valid.\n", oequip_id, tmp_num, regex.change_source);
			source_number = strdup(tmp_num);
		} else {
			source_number = strdup(source_number);
		}
		
		// save as a Source Billing Number 
		source_billing_number = strdup(source_number);

		/**
		 * @var Setting channel variable for CDR Report.
		 */
		
		switch_channel_set_variable(channel, "vox_originator_source_billing_number", source_billing_number);
		
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-OE] : Origination Equipment [ %s ] After Source Number Manipulation Source Number [ %s ].\n", oequip_id, source_number);
	} else {
		// source billing number.
		source_billing_number = strdup(source_number);

		/**
		 * @var Setting channel variable for CDR Report.
		 */
		
		switch_channel_set_variable(channel, "vox_originator_source_billing_number", source_number);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[AMAZE-OE] : Proceed To Check Origination Equipment [ %s ] Source Number [ %s ] Regex is not found, so system will not manipulate Source Number \n", oequip_id, source_number);
	}
	
	/**
	 * @Section Destination Number Manipulation <ORIGINATION-EQUIPMENT>
	 */
	
	if(!zstr(regex.change_destination)) {
	  
		// Log on console
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-OE] : Origination Equipment [ %s ] Billing Source Number [ %s ].\n", oequip_id, source_billing_number);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-OE] : Proceed To Check Origination Equipment [ %s ] Destination Number [ %s ] Regex [ %s ] Manipulation.\n", oequip_id, destination_number, regex.change_destination);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-OE] : Origination Equipment [ %s ] Before Destination Number Manipulation Destination Number [ %s ].\n", oequip_id, destination_number);

		/**
		 * @var Store Destination Number Before manipulation.
		 */
		
		tmp_num = strdup(destination_number);
		
		/**
		 * @Section Destination Number manipulate on Freeswitch Server and gives new number after regex manipulation
		 */
		
		destination_number = switch_regex_manipulation(session, channel, regex.change_destination, destination_number);
		
		//if it will return true/false than do not change destination number and regex failed.
		if(!strcmp(destination_number, "true") || !strcmp(destination_number, "false")) {
			  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "[AMAZE-OE] : Origination Equipment [ %s ] Destination Number [ %s ] manipulation Regex [ %s ] is not valid.\n", oequip_id, tmp_num, regex.change_destination);
			destination_number = strdup(tmp_num);
		} else {
			destination_number = strdup(destination_number);
		}

		destination_billing_number = strdup(destination_number);   /*! Destination Number */

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-OE] : Origination Equipment [ %s ] After Destination Number Manipulation Destination Number [ %s ].\n", oequip_id, destination_number);
		
		/**
		 * @var Setting Channel variable for CDR report
		 */
		
		switch_channel_set_variable(channel, "vox_originator_destination_billing_number", destination_billing_number);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-OE] : Origination Equipment [ %s ] Billing Destination Number [ %s ].\n", oequip_id, destination_billing_number);
	} else {
		
		/**
		 * @Section destination number manipulation regex is not set.
		 */
		
		destination_billing_number = strdup(destination_number);
		switch_channel_set_variable(channel, "vox_originator_destination_billing_number", destination_number);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[AMAZE-OE] : Proceed To Check Origination Equipment [ %s ] Destination Number [ %s ] Regex manipulation is not found, so system will not manipulate it.\n", oequip_id, destination_number);
	}

	/**
	 * @Section Checking Ring Back Tone on Wait Time Alert/RBT
	 */
	
	if (!strcmp(originator.orig_ring_tone, "Y")) {
		char result[100] = "";                  /*! MySQL Query Result */

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-OE] : Originate Equipment [ %s ] Ring Back Tone is Enable\n",oequip_id);
		
		/**
		 * @var Setting channel variables.
		 */
		
		switch_channel_set_variable(channel, "vox_pre_answer", "yes");
		switch_channel_set_variable(channel, "ignore_early_media", "true");

		/**
		 * @Query <Getting RINGBACK TONE File Name For Origination Equipment >
		 */
		
		sql = switch_mprintf("SELECT concat(af_name,'.wav') FROM vca_audio_file WHERE af_id='%d' AND af_status='Y'", originator.af_id);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : Originate Equipment [ %s ] playback file SQL :: \n%s\n",oequip_id, sql);
		switch_execute_sql2str(NULL, sql, result, sizeof(result));
		switch_safe_free(sql);

		/**
		 * @Section validate ringback file and set in channel variable.
		 */
		
		if(!zstr(result)) {
                        char *playfile = NULL;
                        playfile = switch_mprintf("%s/%s",globals.sound_path,result);
                        switch_channel_export_variable(channel, "ringback", playfile, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
                        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-OE] : Originate Equipment [ %s ] ringback File Name [ %s ]\n",oequip_id, playfile);
                        switch_safe_free(playfile);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[AMAZE-OE] : Originate Equipment [ %s ] ringback File Name is not found, so system will not set it as ringback\n",oequip_id);
		}
		switch_channel_set_variable(channel, "vox_orig_ring_tone", "yes");
	} 
	
	/**
	 * @Section Drop-call On progress timeout system should drop call.
	 */
	
	else if (!strcmp(originator.orig_ring_tone, "D")) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-OE] : Originate Equipment [ %s ] Set Drop Call On progress Timeout.\n",oequip_id);
	  
		//Setting Channel variables.
		switch_channel_set_variable(channel, "vox_default_timeout", "yes");
		switch_channel_set_variable(channel, "vox_orig_ring_tone", NULL);
	}
	
	/**
	 * @Section ringback play ring back tone on progress time-out
	 */
	
	else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-OE] : Originate Equipment [ %s ] ringback Tone is set.\n",oequip_id);
		
		/**
		 * @Section Setting channel variables for ringback tone.
		 */
		
		switch_channel_set_variable(channel, "ringback", "%(2000,4000,440.0,480.0)");
		switch_channel_set_variable(channel, "vox_pre_answer", "no");
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-OE] : Originate Equipment [ %s ] ringback Tone [ %%(2000,4000,440.0,480.0) ]\n",oequip_id);
		
		switch_channel_set_variable(channel, "vox_orig_ring_tone", "yes");
	}

	/**
	 * @Section Setting channel variable for progress timeout <ON ORIGINATION EQUIPMENT>
	 */
	
	if(atoi(originator.orig_wait_time_rbt) > 0 ) {
		switch_channel_set_variable(channel, "progress_timeout", originator.orig_wait_time_rbt);
		switch_channel_set_variable(channel, "vox_progress_timeout", originator.orig_wait_time_rbt);
	}

	/**
	 * @Section Setting channel variable for session timeout <ON ORIGINATION EQUIPMENT>
	 */
	
	if(atoi(originator.orig_session_timeout) > 0 ) {
		switch_channel_export_variable(channel, "vox_session_timeout", originator.orig_session_timeout, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
	}

	/**
	 * @Section Setting channel variable for rtp-timeout-sec timeout <ON ORIGINATION EQUIPMENT>
	 */
	
	if(atoi(originator.orig_rtp_timeout) > 0) {
		switch_channel_export_variable(channel, "vox_rtp_timeout", originator.orig_rtp_timeout, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
	} 
	
	/**
	 * @Section Setting channel variable for call bridge timeout <ON ORIGINATION EQUIPMENT>
	 */
	
	if(atoi(originator.orig_wait_time_answer) > 0) {
		switch_channel_export_variable(channel, "call_timeout", originator.orig_wait_time_answer, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
	}
	
	/**
	 * @Section Log on console <ON ORIGINATION EQUIPMENT>
	 */
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : Originate Equipment [ %s ] Progress Timeout [ %s ]\n", oequip_id, originator.orig_wait_time_rbt);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : Originate Equipment [ %s ] RTP Timeout [ %s ]\n", oequip_id, originator.orig_rtp_timeout);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : Originate Equipment [ %s ] Call Timeout [ %s ]\n", oequip_id, originator.orig_wait_time_answer);

	/**
	 * @Section Getting Offer codec string from call originator <ON ORIGINATION EQUIPMENT>
	 */
	
	ep_codec_prefer_sdp = (char *)switch_channel_get_variable(channel, "ep_codec_string");
	if(zstr(ep_codec_prefer_sdp)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-OE] : routing is unable to get read codec string.\n");

		/**
		* @Section Setting channel variable for hangup responds <ON ORIGINATION EQUIPMENT>
		*/

		switch_channel_set_variable(channel, "switch_hangup_code", "488");
		switch_channel_set_variable(channel, "switch_hangup_reason", "INCOMPATIBLE_DESTINATION");
		switch_channel_set_variable(channel, "transfer_string", "SWITCH_HANGUP XML SWITCH_HANGUP");
		switch_channel_export_variable(channel, "cdr_call_flag", "1", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
		goto end;
	}
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-OE] : Originate Equipment [ %s ] Read Codec From Origination [ %s ]\n", oequip_id, ep_codec_prefer_sdp);
	
	/**
	 * @Section Origination-Equipment assigned codec validation <ON ORIGINATION EQUIPMENT>
	 */
	
	if(originator.group_id > 0) {
	  
		char resbuf[400] = "";                    /* MySQL Query Result */
		char *token = NULL;                       /* Tokenizing String */
		char *codec_str = NULL;                   /* codec string */
		const char *seprator = ",";               /* string delimeter */ 
		int flag = 0;                             /* flag */
		
		/**
		 * @Query Get <SELECT Origination Equipment Assigned Codec String from DB >
		 */
		
		sql = switch_mprintf("SELECT group_concat(concat(b.codec_name,'@',b.codec_sample_rate,'h@',b.codec_frame_packet,'i') SEPARATOR ',') as codec_string FROM vca_codec_group_details a, vca_codec b WHERE a.codec_id=b.codec_id AND a.group_id=%d AND b.codec_status='Y'", originator.group_id);
		switch_execute_sql2str(NULL, sql, resbuf, sizeof(resbuf));
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : Origination Equipment [ %s ] Allowed Codecs [ %s ]\n", oequip_id, sql);
		
		/**
		 * @Section validating Origination Equipment Codec 
		 */
		
		if(zstr(resbuf)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-OE] : Origination Equipment [ %s ] Failed to get codec from group.\n",oequip_id);
			
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-OE] : System is unable to get Origination Equipment [ %s ] Codec From SQL : \n\n%s\n\n",oequip_id, sql);
			
			/**
			 * @Section Setting channel variables for call hangup respond
			 */
			
			switch_channel_set_variable(channel, "switch_hangup_code", "488");
			switch_channel_set_variable(channel, "switch_hangup_reason", "INCOMPATIBLE_DESTINATION");
			switch_channel_set_variable(channel, "transfer_string", "SWITCH_HANGUP XML SWITCH_HANGUP");
			switch_safe_free(sql);
			switch_channel_export_variable(channel, "cdr_call_flag", "1", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
			goto end;
		}
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-OE] : Originate Equipment [ %s ] Allowed Codecs List [ %s ]\n", oequip_id, resbuf);

		switch_safe_free(sql);
		
		/**
		 * @Section Origination-Equipment assigned codec validation with offer codec string.
		 */
		
		// assigned codec string
		offer_codec_str = switch_mprintf("%s", resbuf);
		codec_str = switch_mprintf("%s", resbuf);

		token = strtok(codec_str, seprator);
		while( token != NULL) {
			char *p = NULL;
			
			p = strstr(ep_codec_prefer_sdp, token);
			if(p!=NULL) {
				flag = 1;
				break;
			}
			token = strtok(NULL, seprator);
		}
		switch_safe_free(codec_str);
		
		/**
		 * @Section Failed to validate origination equipment codec with assigned codec.
		 */
		
		if(flag == 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-OE] : Originate Equipment [ %s ] read codec does not match with originate equipment, so system will not allow to dial this call.\n", oequip_id);
			switch_channel_set_variable(channel, "transfer_string", "SWITCH_HANGUP_488 XML SWITCH_HANGUP_488");
			switch_channel_export_variable(channel, "cdr_call_flag", "1", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
			goto end;
		}
		
		/**
		 * @Section validate codec of <ORIGINATION EQUIPMENT CODECS>
		 */

		else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-OE] : Originate Equipment [ %s ] Codec Verification Done Successfully\n", oequip_id);
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-OE] : Originate Equipment [ %s ] has not any Codec Group assigned, so system will hangup it.\n", oequip_id);
		
		/**
		 * @Section respond To Origination Equipment with 488 on codec varification failed.
		 */
		
		switch_channel_set_variable(channel, "switch_hangup_code", "488");
		switch_channel_set_variable(channel, "switch_hangup_reason", "INCOMPATIBLE_DESTINATION");
		switch_channel_set_variable(channel, "transfer_string", "SWITCH_HANGUP XML SWITCH_HANGUP");
		switch_channel_export_variable(channel, "cdr_call_flag", "1", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
		goto end;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-OE] : Originate Equipment [ %s ] codec policy is [ %s ]\n", oequip_id, originator.orig_codec_policy);
	
	/**
	 * @Section Origination-Equipment offer codec to Termination equipment using origination codec policy
	 * @SWITCH ==> Offer Origination assigned codec list.
	 * @INHERIT ==> Offer as it as Origination Equipment read from call originator 
	 */

	/**
	 * @Section Origination-Equipment <CODEC Policy -- SWITCH >
	 */
	
	if(!zstr(originator.orig_codec_policy) && !strcmp(originator.orig_codec_policy, "SWITCH")) {
		
		/**
		 * @Section setting absolute codec string before bridge OE and TE 
		 */
		
		switch_channel_export_variable(channel, "vox_outbound_codec_prefs", offer_codec_str, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
		switch_channel_set_variable(channel, "vox_origination_equipment_outbound_codec", offer_codec_str);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-OE] : Originate Equipment [ %s ] outbound_codec_prefs [ %s ]\n", oequip_id, offer_codec_str);
		switch_safe_free(offer_codec_str);
		
	}
	
	/**
	 * @Section Origination-Equipment <CODEC Policy -- INHERIT >
	 */
	
	else if(!zstr(originator.orig_codec_policy) && !strcmp(originator.orig_codec_policy, "INHERIT")) {
	  
		/**
		 * @Section Setting channel variables for absolute codec string
		 */
		
		switch_channel_export_variable(channel, "inherit_codec", "true", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
		switch_channel_export_variable(channel, "vox_outbound_codec_prefs", ep_codec_prefer_sdp, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
		switch_channel_set_variable(channel, "vox_origination_equipment_outbound_codec", ep_codec_prefer_sdp);

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-OE] : Originate Equipment [ %s ] outbound_codec_prefs [ %s ]\n", oequip_id, ep_codec_prefer_sdp);

	} 
	
	/**
	 * @Section Origination-Equipment codec policy is not valid
	 */
	
	else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "[AMAZE-OE] : Originate Equipment [ %s ] has invalid codec policy, so system will not allow to dial this number.\n", oequip_id);

		/**
		 * @Section Setting channel variables for call hangup responds
		 */
		
		switch_channel_set_variable(channel, "switch_hangup_code", "488");
		switch_channel_set_variable(channel, "switch_hangup_reason", "INCOMPATIBLE_DESTINATION");
		switch_channel_set_variable(channel, "transfer_string", "SWITCH_HANGUP XML SWITCH_HANGUP");
		switch_channel_export_variable(channel, "cdr_call_flag", "1", SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
		goto end;
	}

	/**
	 * @Section origination application validate successfully and proceed for DIAL peer validation.
	 */
	
// 	transfer = switch_mprintf("%s_%s_%s_%s_%s XML PROCEED_DIAL_PEER_ROUTING", oequip_id, source_number, destination_number, source_billing_number, destination_billing_number);
// 	switch_channel_set_variable(channel, "transfer_string", transfer);

//         if(!zstr(oequip_id)) {
//                 char *app_data = NULL;
//                 char *sql = NULL;
                
                app_data = switch_mprintf("%s_%s_%s_%s_%s", oequip_id, source_number, destination_number, source_billing_number, destination_billing_number);
                
                switch_core_session_execute_application(session,"dialpeer", app_data);
                switch_safe_free(app_data);
                
                sql = switch_mprintf("DELETE FROM vca_active_call WHERE ac_uuid='%s'", switch_str_nil(switch_channel_get_variable(channel, "uuid")));
                switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-OE] : SIP Termination Equipment  Delete Active Call Record SQL :: \n%s\n", sql);
                switch_execute_sql(sql, globals.mutex);
                switch_safe_free(sql);
//         }
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "[AMAZE-OE] : Application Execution Done, Remove Active Call record.\n");
	
	/**
	 * @Section Log on console.
	 */
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : Originate Equipment [ %s ] is validated successfully and proceed for dialpeer validation\n", oequip_id);
        
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG5, "[AMAZE-OE] : Origination Transfer String : %s\n", transfer);
	switch_safe_free(transfer);

end :
	/**
	 * @Section release all memory allocated.
	 */

	switch_safe_free(ip_zone_id);                             /* Free Memory Of IP ZONE ID */
	switch_safe_free(media_zone_id);                          /* Free Memory Of MEDIA ZONE ID */
	switch_safe_free(oequip_id);                              /* Free Memory Of OE ID */
	switch_safe_free(oequip_cps);                             /* Free Memory Of OE CPS */
	switch_safe_free(oequip_mc);                              /* Free Memory Of OE MAX CALL */
	switch_safe_free(capacity_gp_cps);                        /* Free Memory Of OE CG CPS */
	switch_safe_free(capacity_gp_mc);                         /* Free Memory Of OE CG MAX CALL */
	switch_safe_free(media_proxy_mode);                       /* Free Memory Of OE PROXY MODE */
	switch_safe_free(capacity_gp_id);                         /* Free Memory Of IP CG ID */
	
	return ;         
}

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
		stream->write_function(stream, "[AMAZE-OE] : Invalide arguments\n");
		return SWITCH_STATUS_TERM;
	}

	function = switch_mprintf("%s", argv[0]);
	stream->write_function(stream, "[AMAZE-OE] : Proceed For [ %s ]\n", function);
	
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

SWITCH_MODULE_LOAD_FUNCTION(mod_origination_load)
{

	switch_application_interface_t *app_interface;        /* application interface object */
	switch_status_t status;                               /* Loading module status */
	switch_api_interface_t *api_interface;

	memset(&globals, 0, sizeof(globals));
	globals.pool = pool;

	switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, globals.pool);

	if ((status = load_config()) != SWITCH_STATUS_SUCCESS) {
		return status;
	}
	
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "[AMAZE-OE] : Loading Origination Equipment Module.\n");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[AMAZE-OE] : ======================================\n");
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s", banner);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "[AMAZE-OE] : ======================================\n");
	
	SWITCH_ADD_APP(app_interface, "origination", "origination", "origination app", switch_origination_app, NULL, SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);
	SWITCH_ADD_API(api_interface, "vox_origination_config", "configuration", switch_custom_api, NULL);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_origination_shutdown)
{
	return SWITCH_STATUS_SUCCESS;
}

// End Header Guard.
#endif



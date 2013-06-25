/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
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
 * William King <william.king@quentustech.com>
 *
 * mod_xml_radius.c -- Radius authentication and authorization
 *
 */
#include <switch.h>
#include <freeradius-client.h>

static struct {
	switch_memory_pool_t *pool;
	switch_xml_t auth_invite_configs;
	switch_xml_t auth_reg_configs;
	switch_xml_t auth_app_configs;
	switch_xml_t acct_start_configs;
	switch_xml_t acct_end_configs;
	/* xml read write lock */
} globals = {0};

SWITCH_MODULE_LOAD_FUNCTION(mod_xml_radius_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_xml_radius_shutdown);
SWITCH_MODULE_DEFINITION(mod_xml_radius, mod_xml_radius_load, mod_xml_radius_shutdown, NULL);

int GLOBAL_DEBUG = 0;

switch_status_t mod_xml_radius_new_handle(rc_handle **new_handle, switch_xml_t xml) {
	switch_xml_t server, param;

	if ( (*new_handle = rc_new()) == NULL ) {
		goto err;
	}

	if ( rc_config_init(*new_handle) == NULL ) {
		*new_handle = NULL;
		goto err;
	}
	
	if (rc_add_config(*new_handle, "auth_order", "radius", "mod_radius_cdr.c", 0) != 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error adding auth_order\n");		
		goto err;
	}
	
	if ((server = switch_xml_child(xml, "connection")) == NULL ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not find 'connection' section in config file.\n");		
		goto err;		
	}
	
	for (param = switch_xml_child(server, "param"); param; param = param->next) {
		char *var = (char *) switch_xml_attr_soft(param, "name");
		char *val = (char *) switch_xml_attr_soft(param, "value");
		
		if ( GLOBAL_DEBUG ) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Attempting to add param '%s' with value '%s' \n", var, val);
		}
		
		if (strncmp(var, "dictionary", 10) == 0) {
			if ( rc_read_dictionary(*new_handle, val) != 0) {
				goto err;
			}
		} else if (rc_add_config(*new_handle, var, val, "mod_xml_radius", 0) != 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error adding param '%s' with value '%s' \n", var, val);			
			goto err;
		}
	}

	return SWITCH_STATUS_SUCCESS;

 err:
	if ( *new_handle ) {
		rc_destroy( *new_handle );
		*new_handle = NULL;
	}
	return SWITCH_STATUS_GENERR;
}

switch_status_t do_config() 
{
	char *conf = "xml_radius.conf";
	switch_xml_t xml, cfg, tmp, server, param;
	int serv, timeout, deadtime, retries, dict, seq;

	/* TODO:
	   1. Fix read/write lock on configs
	      a. read new configs
	      b. Create replacement xml and vas objects
		  c. Get the write lock.
		  d. Replace xml and vas objects
		  e. unlock and return.
	   2. Don't manually check for proper configs. Use the function in the client library
	   3. Add api that would reload configs
	 */

	if (!(xml = switch_xml_open_cfg(conf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", conf);
		goto err;
	}

	serv = timeout = deadtime = retries = dict = seq = 0;
	if ((tmp = switch_xml_dup(switch_xml_child(cfg, "auth_invite"))) != NULL ) {
		if ( (server = switch_xml_child(tmp, "connection")) != NULL) {
				for (param = switch_xml_child(server, "param"); param; param = param->next) {
					char *var = (char *) switch_xml_attr_soft(param, "name");
					if ( strncmp(var, "authserver", 10) == 0 ) {
						serv = 1;
					} else if ( strncmp(var, "radius_timeout", 14) == 0 ) {
						timeout = 1;
					} else if ( strncmp(var, "radius_deadtime", 15) == 0 ) {
						deadtime = 1;
					} else if ( strncmp(var, "radius_retries", 14) == 0 ) {
						retries = 1;
					} else if ( strncmp(var, "dictionary", 10) == 0 ) {
						dict = 1;
					} else if ( strncmp(var, "seqfile", 7) == 0 ) {
						seq = 1;
					}
				}
				
				if ( serv && timeout && deadtime && retries && dict && seq ) {
					globals.auth_invite_configs = tmp;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing a require section for radius connections\n");
					goto err;
				}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not find 'connection' section for auth_invite\n");
			goto err;
		}		
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Could not find 'auth_invite' section in config file.\n");		
	}
	
	serv = timeout = deadtime = retries = dict = seq = 0;
	if ((tmp = switch_xml_dup(switch_xml_child(cfg, "auth_reg"))) != NULL ) {
		if ( (server = switch_xml_child(tmp, "connection")) != NULL) {
				for (param = switch_xml_child(server, "param"); param; param = param->next) {
					char *var = (char *) switch_xml_attr_soft(param, "name");
					if ( strncmp(var, "authserver", 10) == 0 ) {
						serv = 1;
					} else if ( strncmp(var, "radius_timeout", 14) == 0 ) {
						timeout = 1;
					} else if ( strncmp(var, "radius_deadtime", 15) == 0 ) {
						deadtime = 1;
					} else if ( strncmp(var, "radius_retries", 14) == 0 ) {
						retries = 1;
					} else if ( strncmp(var, "dictionary", 10) == 0 ) {
						dict = 1;
					} else if ( strncmp(var, "seqfile", 7) == 0 ) {
						seq = 1;
					}
				}
				
				if ( serv && timeout && deadtime && retries && dict && seq ) {
					globals.auth_reg_configs = tmp;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing a require section for radius connections\n");
					goto err;
				}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not find 'connection' section for auth_invite\n");
			goto err;
		}		
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Could not find 'auth_invite' section in config file.\n");		
	}
	
	serv = timeout = deadtime = retries = dict = seq = 0;
	if ((tmp = switch_xml_dup(switch_xml_child(cfg, "auth_app"))) != NULL ) {
		if ( (server = switch_xml_child(tmp, "connection")) != NULL) {
				for (param = switch_xml_child(server, "param"); param; param = param->next) {
					char *var = (char *) switch_xml_attr_soft(param, "name");
					if ( strncmp(var, "authserver", 10) == 0 ) {
						serv = 1;
					} else if ( strncmp(var, "radius_timeout", 14) == 0 ) {
						timeout = 1;
					} else if ( strncmp(var, "radius_deadtime", 15) == 0 ) {
						deadtime = 1;
					} else if ( strncmp(var, "radius_retries", 14) == 0 ) {
						retries = 1;
					} else if ( strncmp(var, "dictionary", 10) == 0 ) {
						dict = 1;
					} else if ( strncmp(var, "seqfile", 7) == 0 ) {
						seq = 1;
					}
				}
				
				if ( serv && timeout && deadtime && retries && dict && seq ) {
					globals.auth_app_configs = tmp;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing a require section for radius connections\n");
					goto err;
				}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not find 'connection' section for auth_app\n");
			goto err;
		}		
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Could not find 'auth_app' section in config file.\n");		
	}
	
	serv = timeout = deadtime = retries = dict = seq = 0;
	if (( tmp = switch_xml_dup(switch_xml_child(cfg, "acct_start"))) != NULL ) {
		if ( (server = switch_xml_child(tmp, "connection")) != NULL) {
				for (param = switch_xml_child(server, "param"); param; param = param->next) {
					char *var = (char *) switch_xml_attr_soft(param, "name");
					if ( strncmp(var, "acctserver", 10) == 0 ) {
						serv = 1;
					} else if ( strncmp(var, "radius_timeout", 14) == 0 ) {
						timeout = 1;
					} else if ( strncmp(var, "radius_deadtime", 15) == 0 ) {
						deadtime = 1;
					} else if ( strncmp(var, "radius_retries", 14) == 0 ) {
						retries = 1;
					} else if ( strncmp(var, "dictionary", 10) == 0 ) {
						dict = 1;
					} else if ( strncmp(var, "seqfile", 7) == 0 ) {
						seq = 1;
					}
				}
				
				if ( serv && timeout && deadtime && retries && dict && seq ) {
					globals.acct_start_configs = tmp;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing a require section for radius connections\n");
					goto err;
				}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not find 'connection' section for acct_start\n");
			goto err;
		}		
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Could not find 'acct_start' section in config file.\n");		
	}
	
	serv = timeout = deadtime = retries = dict = seq = 0;
	if (( tmp = switch_xml_dup(switch_xml_child(cfg, "acct_end"))) != NULL ) {
		if ( (server = switch_xml_child(tmp, "connection")) != NULL) {
				for (param = switch_xml_child(server, "param"); param; param = param->next) {
					char *var = (char *) switch_xml_attr_soft(param, "name");
					if ( strncmp(var, "acctserver", 10) == 0 ) {
						serv = 1;
					} else if ( strncmp(var, "radius_timeout", 14) == 0 ) {
						timeout = 1;
					} else if ( strncmp(var, "radius_deadtime", 15) == 0 ) {
						deadtime = 1;
					} else if ( strncmp(var, "radius_retries", 14) == 0 ) {
						retries = 1;
					} else if ( strncmp(var, "dictionary", 10) == 0 ) {
						dict = 1;
					} else if ( strncmp(var, "seqfile", 7) == 0 ) {
						seq = 1;
					}
				}
				
				if ( serv && timeout && deadtime && retries && dict && seq ) {
					globals.acct_end_configs = tmp;
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing a require section for radius connections\n");
					goto err;
				}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not find 'connection' section for acct_end\n");
			goto err;
		}		
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Could not find 'acct_end' section in config file.\n");		
	}
	
	if ( xml ) {
		switch_xml_free(xml);
		xml = NULL;
	}

	return SWITCH_STATUS_SUCCESS;
	
 err:
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mod_xml_radius: Configuration error\n");
	if ( xml ) {
		switch_xml_free(xml);
		xml = NULL;
	}
	
	return SWITCH_STATUS_GENERR;
}

switch_status_t mod_xml_radius_add_params(switch_core_session_t *session, switch_event_t *params, rc_handle *handle, VALUE_PAIR **send, switch_xml_t fields) 
{
	switch_xml_t param;
	void *av_value = NULL;
	
	if ( (param = switch_xml_child(fields, "param")) == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to locate a param under the fields section\n");
		goto err;		
	}
	
	for (; param; param = param->next) {
		DICT_ATTR *attribute = NULL;
		DICT_VENDOR *vendor = NULL;
		int attr_num = 0, vend_num = 0;
		
		char *var = (char *) switch_xml_attr(param, "name");
		char *vend = (char *) switch_xml_attr(param, "vendor");
		char *variable = (char *) switch_xml_attr(param, "variable");
		char *variable_secondary = (char *) switch_xml_attr(param, "variable_secondary");
		char *val_default = (char *) switch_xml_attr(param, "default");
		char *format = (char *) switch_xml_attr(param, "format");
		char *other_leg = (char *) switch_xml_attr(param, "other_leg");

		attribute = rc_dict_findattr(handle, var);
		
		if ( attribute == NULL ) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mod_xml_radius: Could not locate attribute '%s' in the configured dictionary\n", var);
			goto err;
		}
		
		if ( GLOBAL_DEBUG ) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mod_xml_radius: dict attr '%s' value '%d' type '%d'\n", 
							  attribute->name, attribute->value, attribute->type);
		}
		
		attr_num = attribute->value;
		
		if ( vend ) {
			vendor = rc_dict_findvend(handle, vend);
			
			if ( vendor == NULL ) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mod_xml_radius: Could not locate vendor '%s' in the configured dictionary %p\n", 
								  vend, vend);
				goto err;
			}			

			if ( GLOBAL_DEBUG ) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mod_xml_radius: dict vend name '%s' vendorpec '%d'\n", 
								  vendor->vendorname, vendor->vendorpec);
			}
			
			vend_num = vendor->vendorpec;
		} 
		
		if ( var ) {
			if ( session ) {
				switch_channel_t *channel = switch_core_session_get_channel(session);
				
				/*  Accounting only */
				if ( strncmp( var, "h323-setup-time", 15) == 0 ) {
					switch_caller_profile_t *profile = switch_channel_get_caller_profile(channel);
					switch_time_t time = profile->times->created;
					switch_time_exp_t tm;
					
					if ( !time ) {
						goto end_loop;
					}
					
					switch_time_exp_lt(&tm, time);
					av_value = switch_mprintf("%04u-%02u-%02uT%02u:%02u:%02u.%06u%+03d%02d",
											  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
											  tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_usec, 
											  tm.tm_gmtoff / 3600, tm.tm_gmtoff % 3600);
					
					if (rc_avpair_add(handle, send, attr_num, av_value, -1, vend_num) == NULL) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mod_xml_radius: failed to add option to handle\n");
						goto err;
					} 
					if ( GLOBAL_DEBUG ) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mod_xml_radius: value: %s\n", (char *) av_value);
					}
				} else if ( strncmp( var, "h323-connect-time", 17) == 0 ) {
					switch_caller_profile_t *profile = switch_channel_get_caller_profile(channel);
					switch_time_t time = profile->times->answered;
					switch_time_exp_t tm;

					if ( !time ) {
						goto end_loop;
					}
					
					switch_time_exp_lt(&tm, time);
					
					av_value = switch_mprintf("%04u-%02u-%02uT%02u:%02u:%02u.%06u%+03d%02d",
											  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
											  tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_usec, 
											  tm.tm_gmtoff / 3600, tm.tm_gmtoff % 3600);
						
					if (rc_avpair_add(handle, send, attr_num, av_value, -1, vend_num) == NULL) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mod_xml_radius: failed to add option to handle\n");
						goto err;
					} 
					if ( GLOBAL_DEBUG ) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mod_xml_radius: value: %s\n", (char *) av_value);
					}
				} else if ( strncmp( var, "h323-disconnect-time", 20) == 0 ) {
					switch_caller_profile_t *profile = switch_channel_get_caller_profile(channel);
					switch_time_t time = profile->times->hungup;
					switch_time_exp_t tm;

					if ( !time ) {
						if ( variable_secondary != NULL && strncmp(variable_secondary, "now", 3) == 0 ) {
							time = switch_time_now();
						} else {
							goto end_loop;
						}
					}
					
					switch_time_exp_lt(&tm, time);
					
					av_value = switch_mprintf("%04u-%02u-%02uT%02u:%02u:%02u.%06u%+03d%02d",
											  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
											  tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_usec, 
											  tm.tm_gmtoff / 3600, tm.tm_gmtoff % 3600);
					
					if (rc_avpair_add(handle, send, attr_num, av_value, -1, vend_num) == NULL) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mod_xml_radius: failed to add option to handle\n");
						goto err;
					} 
					if ( GLOBAL_DEBUG ) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mod_xml_radius: value: %s\n", (char *) av_value);
					}
				} else if ( strncmp( var, "h323-disconnect-cause", 21) == 0 ) {
					switch_call_cause_t cause = switch_channel_get_cause(channel);
					av_value = switch_mprintf("h323-disconnect-cause=%x", cause);
					if (rc_avpair_add(handle, send, 30, av_value, -1, 9) == NULL) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mod_xml_radius: failed to add disconnect cause \n");
						goto err;
					}			
					
				} else {
					if ( format == NULL ) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Missing format attribute for %s variable\n", variable);
						goto err;
					}

					if ( attribute->type == 0 ) {
						const char *val = NULL;
						
						if ( other_leg ) {
							val = switch_channel_get_variable_partner(channel, variable);
							if ( val == NULL && variable_secondary != NULL) {
								val = switch_channel_get_variable_partner(channel, variable_secondary);
							}
						} else {
							val = switch_channel_get_variable(channel, variable);
							if ( val == NULL && variable_secondary != NULL) {
								val = switch_channel_get_variable(channel, variable_secondary);
							}
						}
						
						if ( val == NULL && val_default != NULL) {
							av_value = switch_mprintf(format, val_default);							
						} else {
							av_value = switch_mprintf(format, val);
						}
						
						if ( GLOBAL_DEBUG ) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mod_xml_radius: value: %s\n", (char *) av_value);
						}
				
						if (rc_avpair_add(handle, send, attr_num, av_value, -1, vend_num) == NULL) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, 
											  "mod_xml_radius: failed to add option with val '%s' to handle\n", (char *) av_value);
							goto err;
						}			
					} else if ( attribute->type == 1 ) {
						int number = atoi(switch_channel_get_variable(channel, variable));
						
						if (rc_avpair_add(handle, send, attr_num, &number, -1, vend_num) == NULL) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, 
											  "mod_xml_radius: failed to add option with value '%d' to handle\n", number);
							goto err;
						}						
					}
				}			
			} else if ( params ) {
				/* Auth only */
				char *tmp = switch_event_get_header(params, variable);

				if ( GLOBAL_DEBUG ) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mod_xml_radius: param var '%s' val: %s\n", variable, tmp);
				}
				
				if ( tmp == NULL ) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mod_xml_radius: Unable to locate '%s' on the event\n", variable);
					goto err;					
				}
				
				av_value = switch_mprintf(format, tmp);
				if (rc_avpair_add(handle, send, attr_num, av_value, -1, vend_num) == NULL) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mod_xml_radius: failed to add option to handle\n");
					goto err;
				}				
			} else {
				goto err;
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mod_xml_radius: all params must have a name attribute\n");
			goto err;
		}

	end_loop:
		if ( av_value != NULL ) {
			free(av_value);
			av_value  = NULL;
		}
	}
	
	return SWITCH_STATUS_SUCCESS;
 err:
	if ( av_value != NULL ) {
		free(av_value);
		av_value  = NULL;
	}
	return SWITCH_STATUS_GENERR;
	
}

/* static switch_status_t name (_In_opt_z_ const char *cmd, _In_opt_ switch_core_session_t *session, _In_ switch_stream_handle_t *stream) */
SWITCH_STANDARD_API(mod_xml_radius_debug_api)
{
	if ( !strncmp(cmd, "on", 2) ) {
		GLOBAL_DEBUG = 1;
	} else if ( !strncmp(cmd, "off", 3)){
		GLOBAL_DEBUG = 0;
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Valid options are [yes|no]\n" );
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "debug is %s\n", (GLOBAL_DEBUG ? "on" : "off") );
	return SWITCH_STATUS_SUCCESS;
}

switch_xml_t mod_xml_radius_auth_invite(switch_event_t *params) {
	int result = 0, param_idx = 0;
	VALUE_PAIR *send = NULL, *recv = NULL, *service_vp = NULL;
	char msg[512 * 10 + 1] = {0};
	uint32_t service = PW_AUTHENTICATE_ONLY;
	rc_handle *new_handle = NULL;
	switch_xml_t fields, xml, dir, dom, usr, vars, var;
	char name[512], value[512], *strtmp;

	if (GLOBAL_DEBUG ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mod_xml_radius: starting invite authentication\n");
	}
	
	if ( mod_xml_radius_new_handle(&new_handle, globals.auth_invite_configs) != SWITCH_STATUS_SUCCESS ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to load radius handle for digest invite authentication\n");
		goto err;		
	}

	if ( new_handle == NULL ) {
		goto err;
	}
	
	if ((fields = switch_xml_child(globals.auth_invite_configs, "fields")) == NULL ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not find 'fields' section in config file.\n");		
		goto err;
	}
	
	if ( mod_xml_radius_add_params(NULL, params, new_handle, &send, fields) != SWITCH_STATUS_SUCCESS ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to add params to rc_handle\n");		
		goto err;
	}
	
	if (rc_avpair_add(new_handle, &send, PW_SERVICE_TYPE, &service, -1, 0) == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mod_xml_radius: failed to add option to handle\n");
		goto err;
	}
	
	result = rc_auth(new_handle, 0, send, &recv, msg);
	
	if ( GLOBAL_DEBUG ){
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mod_xml_radius: result(RC=%d) %s \n", result, msg);
	}
	
	if ( result != 0 ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mod_xml_radius: Failed to authenticate\n");
		goto err;
	}

	xml = switch_xml_new("document");
	switch_xml_set_attr_d(xml, "type", "freeswitch/xml");
	dir = switch_xml_add_child_d(xml, "section", 0);
	switch_xml_set_attr_d(dir, "name", "directory");
	dom = switch_xml_add_child_d(dir, "domain", 0);
	switch_xml_set_attr_d(dom, "name", switch_event_get_header(params, "domain"));
	usr = switch_xml_add_child_d(dom, "user", 0);
	vars = switch_xml_add_child_d(usr, "variables", 0);
	
	switch_xml_set_attr_d(usr, "id", switch_event_get_header(params, "user"));
		
	service_vp = recv;
	while (service_vp != NULL) {
		rc_avpair_tostr(new_handle, service_vp, name, 512, value, 512);
		if ( GLOBAL_DEBUG )
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "\tattribute (%s)[%s] found in radius packet\n", name, value);
		var = switch_xml_add_child_d(vars, "variable", param_idx++);
		strtmp = strdup(name);
		switch_xml_set_attr_d(var, "name", strtmp);
		free(strtmp);
		strtmp = strdup(value);
		switch_xml_set_attr_d(var, "value", strtmp);
		free(strtmp);
		service_vp = service_vp->next;
	}

	if ( GLOBAL_DEBUG ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "XML: %s \n", switch_xml_toxml(xml, 1));
	}
	
	if ( recv ) {
		rc_avpair_free(recv);
		recv = NULL;
	}
	if ( send ) {
		rc_avpair_free(send);
		send = NULL;
	}
	if ( new_handle ) {
		rc_destroy(new_handle);
		new_handle = NULL;
	}
	return xml;
 err:
	if ( recv ) {
		rc_avpair_free(recv);
		recv = NULL;
	}
	if ( send ) {
		rc_avpair_free(send);
		send = NULL;
	}
	if ( new_handle ) {
		rc_destroy(new_handle);
		new_handle = NULL;
	}
	
	return NULL;
}

switch_xml_t mod_xml_radius_auth_reg(switch_event_t *params) {
	int result = 0, param_idx = 0;
	VALUE_PAIR *send = NULL, *recv = NULL, *service_vp = NULL;
	char msg[512 * 10 + 1] = {0};
	uint32_t service = PW_AUTHENTICATE_ONLY;
	rc_handle *new_handle = NULL;
	switch_xml_t fields, xml, dir, dom, usr, vars, var;
	char name[512], value[512], *strtmp;

	if (GLOBAL_DEBUG ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mod_xml_radius: starting registration authentication\n");
	}
	
	if ( mod_xml_radius_new_handle(&new_handle, globals.auth_invite_configs) != SWITCH_STATUS_SUCCESS ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to load radius handle for registration authentication\n");
		goto err;		
	}

	if ( new_handle == NULL ) {
		goto err;
	}
	
	if ((fields = switch_xml_child(globals.auth_reg_configs, "fields")) == NULL ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not find 'fields' section in config file.\n");		
		goto err;
	}
	
	if ( mod_xml_radius_add_params(NULL, params, new_handle, &send, fields) != SWITCH_STATUS_SUCCESS ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to add params to rc_handle\n");		
		goto err;
	}
	
	if (rc_avpair_add(new_handle, &send, PW_SERVICE_TYPE, &service, -1, 0) == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mod_xml_radius: failed to add option to handle\n");
		goto err;
	}
	
	result = rc_auth(new_handle, 0, send, &recv, msg);
	
	if ( GLOBAL_DEBUG ){
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mod_xml_radius: result(RC=%d) %s \n", result, msg);
	}
	
	if ( result != 0 ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mod_xml_radius: Failed to authenticate\n");
		goto err;
	}

	xml = switch_xml_new("document");
	switch_xml_set_attr_d(xml, "type", "freeswitch/xml");
	dir = switch_xml_add_child_d(xml, "section", 0);
	switch_xml_set_attr_d(dir, "name", "directory");
	dom = switch_xml_add_child_d(dir, "domain", 0);
	switch_xml_set_attr_d(dom, "name", switch_event_get_header(params, "domain"));
	usr = switch_xml_add_child_d(dom, "user", 0);
	vars = switch_xml_add_child_d(usr, "variables", 0);
	
	switch_xml_set_attr_d(usr, "id", switch_event_get_header(params, "user"));
		
	var = switch_xml_add_child_d(vars, "variable", param_idx++);
	switch_xml_set_attr_d(var, "name", "radius_auth_result");
	switch_xml_set_attr_d(var, "value", "0");

	service_vp = recv;
	while (service_vp != NULL) {
		rc_avpair_tostr(new_handle, service_vp, name, 512, value, 512);
		if ( GLOBAL_DEBUG )
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "\tattribute (%s)[%s] found in radius packet\n", name, value);
		var = switch_xml_add_child_d(vars, "variable", param_idx++);
		strtmp = strdup(name);
		switch_xml_set_attr_d(var, "name", strtmp);
		free(strtmp);
		strtmp = strdup(value);
		switch_xml_set_attr_d(var, "value", strtmp);
		free(strtmp);
		service_vp = service_vp->next;
	}

	if ( GLOBAL_DEBUG ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "XML: %s \n", switch_xml_toxml(xml, 1));
	}
	
	if ( recv ) {
		rc_avpair_free(recv);
		recv = NULL;
	}
	if ( send ) {
		rc_avpair_free(send);
		send = NULL;
	}
	if ( new_handle ) {
		rc_destroy(new_handle);
		new_handle = NULL;
	}

	return xml;
 err:
	if ( recv ) {
		rc_avpair_free(recv);
		recv = NULL;
	}
	if ( send ) {
		rc_avpair_free(send);
		send = NULL;
	}
	if ( new_handle ) {
		rc_destroy(new_handle);
		new_handle = NULL;
	}
	
	return NULL;
}

static switch_xml_t mod_xml_radius_directory_search(const char *section, const char *tag_name, const char *key_name, const char *key_value, 
													switch_event_t *params,	void *user_data)
{
	char *event_buf = NULL;
	switch_xml_t xml = NULL;
	char *auth_method = switch_event_get_header(params,"sip_auth_method");
	

	if ( GLOBAL_DEBUG ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mod_xml_radius: starting authentication\n");
		switch_event_serialize(params, &event_buf, SWITCH_TRUE);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Event: %s \n", event_buf);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Section: %s \nTag: %s\nKey_name: %s\nKey_value: %s\n", 
						  section, tag_name, key_name, key_value);
	}
	
	if ( auth_method == NULL) {
		return NULL;
	}
	
	if ( strncmp( "INVITE", auth_method, 6) == 0) {
		xml = mod_xml_radius_auth_invite(params);
	} else if ( strncmp( "REGISTER", auth_method, 8) == 0) {
		xml = mod_xml_radius_auth_reg(params);
	} else {
		xml = NULL;
	}

	return xml;
}

switch_status_t mod_xml_radius_check_conditions(switch_channel_t *channel, switch_xml_t conditions) {
	switch_xml_t condition, param;
	char *channel_var = NULL;
	const char *channel_val = NULL;
	char *regex = NULL;
	char *anti = NULL;
	int all_matched = 1;
	int result = 0;
	
	if ( (condition = switch_xml_child(conditions, "condition")) == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to locate a condition under the conditions section\n");
		return SWITCH_STATUS_FALSE;
	}
	
	for (; condition; condition = condition->next) {
		
		if ( (param = switch_xml_child(condition, "param")) == NULL) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to locate a param under this condition\n");
			return SWITCH_STATUS_FALSE;
		}
		
		all_matched = 1;
		for (; param && all_matched; param = param->next) {
			channel_var = (char *) switch_xml_attr(param, "var");
			regex = (char *) switch_xml_attr(param, "regex");
			anti = (char *) switch_xml_attr(param, "anti");
			
			if ( channel_var == NULL || regex == NULL ) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Improperly constructed mod_radius condition: %s %s\n", channel_var, regex);
				continue;
			}
			
			if ( ( channel_val = switch_channel_get_variable(channel, channel_var) ) == NULL ) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, 
								  "Improperly constructed mod_radius condition, no such channel variable: %s %s\n", channel_var, regex);
				continue;
			}

			result = ( switch_regex_match( channel_val, regex) != SWITCH_STATUS_SUCCESS);
			if (( anti == NULL && result ) || ( anti != NULL && !result ) ){
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Didn't match: %s == %s \n", switch_channel_get_variable(channel, channel_var), regex);
				all_matched = 0;
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Result of %s match: %s == %s \n", 
								  anti, switch_channel_get_variable(channel, channel_var), regex);
			}
		}

		if ( all_matched ) {
			return SWITCH_STATUS_SUCCESS;
		}
	}
	
	return SWITCH_STATUS_FALSE;
}

switch_status_t mod_xml_radius_accounting_start(switch_core_session_t *session){
	VALUE_PAIR *send = NULL;
	uint32_t service = PW_STATUS_START;
	rc_handle *new_handle = NULL;
	switch_xml_t fields, conditions;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	if (GLOBAL_DEBUG ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mod_xml_radius: starting accounting start\n");
		switch_core_session_execute_application(session, "info", NULL);
	}

	/* If there are conditions defined, and none of them pass, then skip this accounting */
	if ((conditions = switch_xml_child(globals.acct_start_configs, "conditions")) != NULL &&
		mod_xml_radius_check_conditions(channel, conditions) != SWITCH_STATUS_SUCCESS ) {
		goto end;
	}
	
	if ( mod_xml_radius_new_handle(&new_handle, globals.acct_start_configs) != SWITCH_STATUS_SUCCESS || new_handle == NULL ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to create new accounting_start handle for call: %s\n",
						  switch_channel_get_variable(channel, "uuid"));
		goto end;		
	}

	if ((fields = switch_xml_child(globals.acct_start_configs, "fields")) == NULL ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not find 'fields' section in config file.\n");		
		goto end;
	}
	
	if ( mod_xml_radius_add_params(session, NULL, new_handle, &send, fields) != SWITCH_STATUS_SUCCESS ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to add params to rc_handle\n");		
		goto end;
	}
	
	if (rc_avpair_add(new_handle, &send, PW_ACCT_STATUS_TYPE, &service, -1, 0) == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mod_xml_radius: failed to add option to handle\n");
		goto end;
	}	

	if (rc_acct(new_handle, 0, send) == OK_RC) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "mod_xml_radius:  Accounting Start success\n");
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mod_xml_radius:  Accounting Start failed\n");
	}

 end:
	if ( send ) {
		rc_avpair_free(send);
		send = NULL;
	}
	if ( new_handle ) {
		rc_destroy(new_handle);
		new_handle = NULL;
	}

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t mod_xml_radius_accounting_end(switch_core_session_t *session){
	VALUE_PAIR *send = NULL;
	uint32_t service = PW_STATUS_STOP;
	rc_handle *new_handle = NULL;
	switch_xml_t fields = NULL, conditions = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	
	if (GLOBAL_DEBUG ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mod_xml_radius: starting accounting stop\n");
		switch_core_session_execute_application(session, "info", NULL);
	}
	
	/* If there are conditions defined, and none of them pass, then skip this accounting */
	if ((conditions = switch_xml_child(globals.acct_start_configs, "conditions")) != NULL &&
		mod_xml_radius_check_conditions(channel, conditions) != SWITCH_STATUS_SUCCESS ) {
		goto end;
	}
	
	if ( mod_xml_radius_new_handle(&new_handle, globals.acct_end_configs) != SWITCH_STATUS_SUCCESS || new_handle == NULL ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to create new accounting_end handle for call: %s\n",
						  switch_channel_get_variable(channel, "uuid"));
		goto end;		
	}

	if ((fields = switch_xml_child(globals.acct_end_configs, "fields")) == NULL ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not find 'fields' section in config file.\n");		
		goto end;
	}
	
	if ( mod_xml_radius_add_params(session, NULL, new_handle, &send, fields) != SWITCH_STATUS_SUCCESS ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to add params to rc_handle\n");		
		goto end;
	}
	
	if (rc_avpair_add(new_handle, &send, PW_ACCT_STATUS_TYPE, &service, -1, 0) == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mod_xml_radius: failed to add option to handle\n");
		goto end;
	}	

	if (rc_acct(new_handle, 0, send) == OK_RC) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "mod_xml_radius:  Accounting Stop success\n");
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mod_xml_radius:  Accounting Stop failed\n");
	}

 end:
	if ( send ) {
		rc_avpair_free(send);
		send = NULL;
	}
	if ( new_handle) {
		rc_destroy(new_handle);
		new_handle = NULL;
	}
	
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_APP(radius_auth_handle)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	int result = 0;
	VALUE_PAIR *send = NULL, *recv = NULL, *service_vp = NULL;
	char msg[512 * 10 + 1] = {0};
	uint32_t service = PW_AUTHENTICATE_ONLY;
	rc_handle *new_handle = NULL;
	switch_xml_t fields;
	char name[512], value[512], *temp = NULL;

	if (GLOBAL_DEBUG ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mod_xml_radius: starting app authentication\n");
	}

	if ( mod_xml_radius_new_handle(&new_handle, globals.auth_app_configs) != SWITCH_STATUS_SUCCESS || new_handle == NULL ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to create new authentication handle for call: %s\n",
						  switch_channel_get_variable(channel, "uuid"));
		goto err;
	}
	
	if ((fields = switch_xml_child(globals.auth_app_configs, "fields")) == NULL ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not find 'fields' section in config file.\n");		
		goto err;
	}
	
	if ( mod_xml_radius_add_params(session, NULL, new_handle, &send, fields) != SWITCH_STATUS_SUCCESS ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to add params to rc_handle\n");		
		goto err;
	}
	
	if (rc_avpair_add(new_handle, &send, PW_SERVICE_TYPE, &service, -1, 0) == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mod_xml_radius: failed to add option to handle\n");
		goto err;
	}
	
	result = rc_auth(new_handle, 0, send, &recv, msg);
	
	if ( GLOBAL_DEBUG ){
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mod_xml_radius: result(RC=%d) %s \n", result, msg);
	}
	temp = switch_mprintf("%d",result);	
	switch_channel_set_variable(channel, "radius_auth_result", temp);
	free(temp);
	temp = NULL;

	if ( result != 0 ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mod_xml_radius: Failed to authenticate\n");
		goto err;
	}


	service_vp = recv;
	while (service_vp != NULL) {
		rc_avpair_tostr(new_handle, service_vp, name, 512, value, 512);
		if ( GLOBAL_DEBUG )
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "\tattribute (%s)[%s] found in radius packet\n", name, value);

		switch_channel_set_variable(channel, name, value);
		service_vp = service_vp->next;
	}

	if ( recv ) {
		rc_avpair_free(recv);
		recv = NULL;
	}
	if ( send ) {
		rc_avpair_free(send);
		send = NULL;
	}
	if ( new_handle ) {
		rc_destroy(new_handle);
		new_handle = NULL;
	}
	
	return;
 err:
	if ( recv ) {
		rc_avpair_free(recv);
		recv = NULL;
	}
	if ( send ) {
		rc_avpair_free(send);
		send = NULL;
	}
	if ( new_handle ) {
		rc_destroy(new_handle);
		new_handle = NULL;
	}
	return;
}

static const switch_state_handler_table_t state_handlers = {
	/*.on_init */ NULL,
	/*.on_routing */ mod_xml_radius_accounting_start,
	/*.on_execute */ NULL,
	/*.on_hangup */ NULL,
	/*.on_exchange_media */ NULL,
	/*.on_soft_execute */ NULL,
	/*.on_consume_media */ NULL,
	/*.on_hibernate */ NULL,
	/*.on_reset */ NULL,
	/*.on_park */ NULL,
	/*.on_reporting */ mod_xml_radius_accounting_end
};


/* switch_status_t name (switch_loadable_module_interface_t **module_interface, switch_memory_pool_t *pool) */
SWITCH_MODULE_LOAD_FUNCTION(mod_xml_radius_load)
{
	switch_api_interface_t *mod_xml_radius_api_interface;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_application_interface_t *app_interface;

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	
	memset(&globals, 0, sizeof(globals));
	globals.pool = pool;

	if ( GLOBAL_DEBUG != 0 ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mod_xml_radius: loading\n");
	}

	if ( (status = do_config()) != SWITCH_STATUS_SUCCESS ) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "mod_xml_radius: Failed to load configs\n");
		return SWITCH_STATUS_TERM;
	}
	
	if ( globals.auth_invite_configs ) {
		status = switch_xml_bind_search_function(mod_xml_radius_directory_search, switch_xml_parse_section_string("directory"), NULL);
	}
		
	SWITCH_ADD_API(mod_xml_radius_api_interface, "xml_radius_debug", "mod_xml_radius toggle debug", mod_xml_radius_debug_api, NULL);

	switch_core_add_state_handler(&state_handlers);

	SWITCH_ADD_APP(app_interface, "radius_auth", NULL, NULL, radius_auth_handle, "radius_auth", SAF_SUPPORT_NOMEDIA | SAF_ROUTING_EXEC);
	
	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_xml_radius_shutdown)
{
	switch_core_remove_state_handler(&state_handlers);
	switch_xml_unbind_search_function_ptr(mod_xml_radius_directory_search);

	if ( globals.auth_invite_configs ) {
		switch_xml_free(globals.auth_invite_configs);
	}
	if ( globals.auth_reg_configs ) {
		switch_xml_free(globals.auth_reg_configs);
	}
	if ( globals.auth_app_configs ) {
		switch_xml_free(globals.auth_app_configs);
	}
	if ( globals.acct_start_configs ) {
		switch_xml_free(globals.acct_start_configs);
	}
	if ( globals.acct_end_configs ) {
		switch_xml_free(globals.acct_end_configs);
	}
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

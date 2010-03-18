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
* Anthony Minessale II <anthmct@yahoo.com>
* Neal Horman <neal at wanlink dot com>
* Tihomir Culjaga <tculjaga@gmail.com>
*
* mod_rad_auth.c -- module for radius authorization/authentication
*
*/
#include <switch.h>
#include <freeradius-client.h>

#define RC_CONFIG_FILE "/usr/local/etc/radiusclient/radiusclient.conf"
#define EMBENDED_CONFIG 1

#define STR_LENGTH 512
char* rc_config_file = NULL;

struct config_vsas
{
	char* name;
	int id;
	char* value;
	int pec;
	int expr;
	int direction;
	
	struct config_vsas *pNext;
};

struct config_client
{
	char* name;
	char* value;
	
	struct config_client *pNext;
};

typedef struct config_vsas CONFIG_VSAS;
typedef struct config_client CONFIG_CLIENT;

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_rad_authshutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_rad_authruntime);
SWITCH_MODULE_LOAD_FUNCTION(mod_rad_authload);
SWITCH_MODULE_DEFINITION(mod_rad_auth, mod_rad_authload, mod_rad_authshutdown, NULL /*mod_rad_authruntime*/);

CONFIG_VSAS* CONFIGVSAS;
CONFIG_CLIENT* CONFIGCLIENT;

void free_radius_auth_value_pair(VALUE_PAIR *send, VALUE_PAIR *received, rc_handle *rh)
{
	if (send)
		rc_avpair_free(send);

	if (received)
		rc_avpair_free(received);
		
	if (rh)
		rc_destroy(rh);
	rh = NULL;
}

char* extract_in_variable(char* invar)
{
	char *var = NULL;
	if (strlen(invar) < 4)
		return NULL;

	while (invar[0] == ' ')
		invar++;

	if (invar[0] != 'i' && invar[0] != 'I' && invar[1] != 'n' && invar[1]
			!= 'N' && invar[2] != ' ' && invar[2] != ' ')
	{
		return NULL;
	}

	var = strchr(invar, ' ');

	while (var[0] == ' ')
		var++;

	return var;
}

char* extract_out_variable(char* outvar)
{
	char *var = NULL;
	if (strlen(outvar) < 5)
		return NULL;

	while (outvar[0] == ' ')
		outvar++;

	if (outvar[0] != 'o' && outvar[0] != 'O' && outvar[1] != 'u' && outvar[1]
			!= 'U' && outvar[2] != 't' && outvar[2] != 'T' && outvar[3] != ' '
			&& outvar[3] != ' ')
	{
		return NULL;
	}

	var = strchr(outvar, ' ');

	while (var[0] == ' ')
	{
		var++;
	}

	return var;
}


CONFIG_VSAS* GetVSAS(char* key)
{
	CONFIG_VSAS* PCONFIGVSAS = CONFIGVSAS;
	
	while(PCONFIGVSAS)
	{
		if (strcmp(key, PCONFIGVSAS->name) == 0)
		{
			return PCONFIGVSAS;
		}
		
		PCONFIGVSAS = PCONFIGVSAS->pNext;
	}
	
	return NULL;
}

char* GetValue(switch_channel_t *channel, CONFIG_VSAS* VSAS, char* value)
{
	if (VSAS == NULL)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Internal Error : VSAS is null object.\n");
		return "";
	}
	
	if (VSAS->value == NULL)
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Internal Error : VSAS->value is null object.\n");
		return "";
	}
	
	if (VSAS->expr == 1)
	{
		const char* v = switch_channel_get_variable(channel, VSAS->value);

		if (v != NULL)
		{
			strcpy(value, v);
			return value;
		}
		else
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Undefined channel variable: %s.\n", VSAS->value);
			strcpy(value, "");
			return value;
		}
	}
	else
	{
		strcpy(value, VSAS->value);
		return value;
	}
}

int radius_auth_test(switch_channel_t *channel, char* username1, char* passwd1, char* auth_result, char* biling_model, char* credit_amount, char* currency, char* preffered_lang)
{
	int             result;
	char		username[128];
	char            passwd[AUTH_PASS_LEN + 1];
	VALUE_PAIR 	*send, *received;
	uint32_t		service;
	char 		msg[4096], username_realm[256];
	char		*default_realm;
	rc_handle	*rh;

	strcpy(username, "123");
	strcpy(passwd, "123");

	if ((rh = rc_read_config(RC_CONFIG_FILE)) == NULL)
		return ERROR_RC;

	if (rc_read_dictionary(rh, rc_conf_str(rh, "dictionary")) != 0)
		return ERROR_RC;

	default_realm = rc_conf_str(rh, "default_realm");


	send = NULL;

	/*
	 * Fill in User-Name
	 */

	strncpy(username_realm, username, sizeof(username_realm));

	/* Append default realm */
	if ((strchr(username_realm, '@') == NULL) && default_realm &&
	    (*default_realm != '\0'))
	{
		strncat(username_realm, "@", sizeof(username_realm)-strlen(username_realm)-1);
		strncat(username_realm, default_realm, sizeof(username_realm)-strlen(username_realm)-1);
	}

	if (rc_avpair_add(rh, &send, PW_USER_NAME, username_realm, -1, 0) == NULL)
		return ERROR_RC;

	/*
	 * Fill in User-Password
	 */

	if (rc_avpair_add(rh, &send, PW_USER_PASSWORD, passwd, -1, 0) == NULL)
		return ERROR_RC;

	/*
	 * Fill in Service-Type
	 */

	service = PW_AUTHENTICATE_ONLY;
	if (rc_avpair_add(rh, &send, PW_SERVICE_TYPE, &service, -1, 0) == NULL)
		return ERROR_RC;

	result = rc_auth(rh, 0, send, &received, msg);

	if (result == OK_RC)
	{
		fprintf(stderr, "\"%s\" RADIUS Authentication OK\n", username);
	}
	else
	{
		fprintf(stderr, "\"%s\" RADIUS Authentication failure (RC=%i)\n", username, result);
	}

	return result;

}

int radius_auth(switch_channel_t *channel, char* called_number, char* username, char* password , char* auth_result/*, char* biling_model, char* credit_amount, char* currency, char* preffered_lang*/)
{
	int result = OK_RC;
	VALUE_PAIR *send = NULL;
	VALUE_PAIR *received = NULL;
	VALUE_PAIR 	*service_vp;
	DICT_ATTR   *pda;
	CONFIG_VSAS* PCONFIGVSAS = NULL;
	char *default_realm = NULL;
	rc_handle *rh = NULL;
	int attrid  =0;

	char msg[STR_LENGTH * 10 + 1];
	char username_realm[STR_LENGTH + 1];
	char value[STR_LENGTH + 1];
	int integer;

	memset(msg, 0, STR_LENGTH * 10);
	memset(username_realm, 0, STR_LENGTH);
	
	send = NULL;
	


	do
	{
		
#if EMBENDED_CONFIG

		CONFIG_CLIENT* PCONFIGCLIENT = CONFIGCLIENT;
		
		rh = rc_new();
		if (rh == NULL) 
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ERROR: Failed to allocate initial structure.\n");
			result = ERROR_RC;
			break;
		}
		
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "allocate initial structure.\n");
	
		/* Initialize the config structure */
	
		rh = rc_config_init(rh);
		if (rh == NULL)
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,"ERROR: Failed to initialze configuration.\n");
			result = ERROR_RC;
			break;
		}
		
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"initialzed configuration.\n");
	
		while(PCONFIGCLIENT)
		{
			//if (rc_add_config(rh, "auth_order", "radius", "config", 0) != 0) 
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "set %s := %s.\n", PCONFIGCLIENT->name, PCONFIGCLIENT->value);
			if (rc_add_config(rh, PCONFIGCLIENT->name, PCONFIGCLIENT->value, "config", 0) != 0) 
			{
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "ERROR: Unable to set %s := %s.\n", PCONFIGCLIENT->name, PCONFIGCLIENT->value);
				
				result = ERROR_RC;
				break;
			}
			
			PCONFIGCLIENT = PCONFIGCLIENT->pNext;
		}
		
		if (result == ERROR_RC)
			break;

		
#else
		if ((rh = rc_read_config(!rc_config_file ? RC_CONFIG_FILE : rc_config_file)) == NULL)
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error loading radius config file\n");
			
			result = ERROR_RC;
			break;
		}
		
#endif

		if (rc_read_dictionary(rh, rc_conf_str(rh, "dictionary")) != 0)
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error loading radius dictionary\n");
			
			result = ERROR_RC;
			break;
		}
		
		default_realm = rc_conf_str(rh, "default_realm");
		if (default_realm == NULL)
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "default_realm is null object.\n");
			result = ERROR_RC;
			break;
		}
		
		strncpy(username_realm, username, sizeof(username_realm));

		if ((strchr(username_realm, '@') == NULL) && default_realm &&
		    (*default_realm != '\0'))
		{
			strncat(username_realm, "@", sizeof(username_realm)-strlen(username_realm)-1);
			strncat(username_realm, default_realm, sizeof(username_realm)-strlen(username_realm)-1);
		}
		
	
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
			"... radius: User-Name: %s\n", username);
		if (rc_avpair_add(rh, &send, PW_USER_NAME, username_realm, -1, 0)== NULL)
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "An Error occured during rc_avpair_add : username\n");
			result = ERROR_RC;
			break;
		}
		
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
			"... radius: User-Password: %s\n", password);
		if (rc_avpair_add(rh, &send, PW_USER_PASSWORD, password, -1, 0) == NULL)
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "An Error occured during rc_avpair_add : password\n");
			result = ERROR_RC;
			break;
		}
		
		if (!called_number || strcmp(called_number, "") == 0)
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
				"... radius: Called-station-Id is empty, ignoring...\n");
		}
		else
		{
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
				"... radius: Called-station-Id: %s\n", called_number);
			if (rc_avpair_add(rh, &send, 30, called_number, -1, 0) == NULL)
			{
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "An Error occured during rc_avpair_add : Called-station-Id\n");
				result = ERROR_RC;
				break;
			}
		}

		
		PCONFIGVSAS = CONFIGVSAS;
	
		while(PCONFIGVSAS)
		{
			if (PCONFIGVSAS->direction == 1)
			{
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Handle attribute: %s\n", PCONFIGVSAS->name	);
				
				memset(value, 0, STR_LENGTH);
				GetValue(channel, PCONFIGVSAS, value);
				
				if (PCONFIGVSAS->pec != 0)
					attrid = PCONFIGVSAS->id | (PCONFIGVSAS->pec << 16);
				else
					attrid = PCONFIGVSAS->id ;
					
				pda = rc_dict_getattr(rh, attrid);
				
				if (pda == NULL)
				{
					result = ERROR_RC;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unknown attribute: key:%s, not found in dictionary\n", PCONFIGVSAS->name);
					break;	
				}
				
				if (PCONFIGVSAS->pec != 0 && rc_dict_getvend(rh, PCONFIGVSAS->pec) == NULL)
				{
					result = ERROR_RC;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unknown vendor specific id: key:%s, id:%dnot found in dictionary\n", PCONFIGVSAS->name, PCONFIGVSAS->pec);
					break;	
				}
				
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "... dictionary data: id:%d, vendor id:%d, attr type:%d, attr name:%s (%d)\n", PCONFIGVSAS->id, PCONFIGVSAS->pec, pda->type, pda->name, attrid);
				
				switch(pda->type)
				{
					case PW_TYPE_STRING:
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "... radius: key:%s, value:%s (%s) as string\n", PCONFIGVSAS->name, PCONFIGVSAS->value, value);
						if (rc_avpair_add(rh, &send, PCONFIGVSAS->id, value, -1, PCONFIGVSAS->pec) == NULL)
						{
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "An Error occured during rc_avpair_add : %s\n", PCONFIGVSAS->name);
							result = ERROR_RC;
							break;
						}
						break;
						
					//case PW_TYPE_DATE:
					case PW_TYPE_INTEGER:
						integer = atoi(value);
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "... radius: key:%s, value:%s (%d) as integer\n", PCONFIGVSAS->name, PCONFIGVSAS->value, integer);
						
						
						if (rc_avpair_add(rh, &send, PCONFIGVSAS->id, &integer, -1, PCONFIGVSAS->pec) == NULL)
						{
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "An Error occured during rc_avpair_add : %s\n", PCONFIGVSAS->name);
							result = ERROR_RC;
							break;
						}
						break;
					case PW_TYPE_IPADDR:
						integer = rc_get_ipaddr(value);
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "... radius: key:%s, value:%s (%d) as ipaddr\n", PCONFIGVSAS->name, PCONFIGVSAS->value, integer);
						
						
						if (rc_avpair_add(rh, &send, PCONFIGVSAS->id, &integer, -1, PCONFIGVSAS->pec) == NULL)
						{
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "An Error occured during rc_avpair_add : %s\n", PCONFIGVSAS->name);
							result = ERROR_RC;
							break;
						}
						break;						
						
					default:
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Unknown attribute type: key:%s, type %d\n", PCONFIGVSAS->name, pda->type);
						break;
				}
			}
			
			PCONFIGVSAS = PCONFIGVSAS->pNext;
		}

		
		if (result != ERROR_RC)
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "sending radius packet ...\n"	);
			result = rc_auth(rh, 0, send, &received, msg);
		
	
			if (result == OK_RC)
			{
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
					"RADIUS Authentication OK\n");
					
					strcpy(auth_result, "OK");
			}
			else
			{
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
					" RADIUS Authentication failure (RC=%d)\n", 
					result);
					
					strcpy(auth_result, "NOK");
			}
			
			
			
			PCONFIGVSAS = CONFIGVSAS;
		
			while(PCONFIGVSAS)
			{
				if (PCONFIGVSAS->direction == 0)
				{
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Handle attribute: %s\n", PCONFIGVSAS->name	);
					if ((service_vp = rc_avpair_get(received, PCONFIGVSAS->id, PCONFIGVSAS->pec)) != NULL)
					{
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\tattribute (%s) found in radius packet\n", PCONFIGVSAS->name);
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\tset variable %s := %s\n", PCONFIGVSAS->value, service_vp->strvalue);
						
						switch_channel_set_variable(channel, PCONFIGVSAS->value, service_vp->strvalue);
					}
					else
					{
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\tNo found out attribute id: %d, pec:%d, (%s)\n", PCONFIGVSAS->id, PCONFIGVSAS->pec, PCONFIGVSAS->name	);
					}
				}
				
				PCONFIGVSAS = PCONFIGVSAS->pNext;
			}
		}
		else
		{
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "abort sending radius packet.\n"	);
			break;
		}
	
	} while(1 == 0);

	if (result == ERROR_RC)
	{
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
				"An error occured during RADIUS Authentication(RC=%d)\n", 
				result);
	}
	
	free_radius_auth_value_pair(send, received, rh);
	
	return result;
}

SWITCH_STANDARD_APP(auth_function)
{
	char* in_called_number = NULL;
	char *in_username = NULL;
	char *in_password = NULL;
	
	char *out_auth_result = NULL;
	/*char *out_biling_model = NULL;
	char *out_credit_amount = NULL;
	char *out_currency = NULL;
	char *out_preffered_lang = NULL;*/
	
	char auth_result[STR_LENGTH + 1];
	/*char biling_model[STR_LENGTH + 1]; 
	char credit_amount[STR_LENGTH + 1];
	char currency[STR_LENGTH + 1];
	char preffered_lang[STR_LENGTH + 1];*/

	switch_channel_t *channel = switch_core_session_get_channel(session);
		
	memset(auth_result, 0, STR_LENGTH);
	/*memset(biling_model, 0, STR_LENGTH); 
	memset(credit_amount, 0, STR_LENGTH);
	memset(currency, 0, STR_LENGTH);
	memset(preffered_lang, 0, STR_LENGTH);*/

	if (switch_strlen_zero(data))
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
			"No variable name specified.\n");
	}
	else
	{
		
		char *in_called_number_expanded = NULL;
		char *in_username_expanded = NULL;
		char *in_password_expanded = NULL;


		in_called_number = switch_core_session_strdup(session, data);
		
		in_username = strchr(in_called_number, ',');

		if (in_username)
		{
			*in_username++ = '\0';
			if (switch_strlen_zero(in_username))
			{
				in_username = NULL;
			}
		}
		
		in_password = strchr(in_username, ',');

		if (in_password)
		{
			*in_password++ = '\0';
			if (switch_strlen_zero(in_password))
			{
				in_password = NULL;
			}
		}

		out_auth_result = strchr(in_password, ',');

		if (out_auth_result)
		{
			*out_auth_result++ = '\0';
			if (switch_strlen_zero(out_auth_result))
			{
				out_auth_result = NULL;
			}
		}
		
		/*out_biling_model = strchr(out_auth_result, ',');

		if (out_biling_model)
		{
			*out_biling_model++ = '\0';
			if (switch_strlen_zero(out_biling_model))
			{
				out_biling_model = NULL;
			}
		}
		
		out_credit_amount = strchr(out_biling_model, ',');

		if (out_credit_amount)
		{
			*out_credit_amount++ = '\0';
			if (switch_strlen_zero(out_credit_amount))
			{
				out_credit_amount = NULL;
			}
		}
		
		out_currency = strchr(out_credit_amount, ',');

		if (out_currency)
		{
			*out_currency++ = '\0';
			if (switch_strlen_zero(out_currency))
			{
				out_currency = NULL;
			}
		}
		
		out_preffered_lang = strchr(out_currency, ',');

		if (out_preffered_lang)
		{
			*out_preffered_lang++ = '\0';
			if (switch_strlen_zero(out_preffered_lang))
			{
				out_preffered_lang = NULL;
			}
		}*/

		if (in_called_number)
			in_called_number = extract_in_variable(in_called_number);
			
		in_username = extract_in_variable(in_username);
		in_password = extract_in_variable(in_password);
		out_auth_result = extract_out_variable(out_auth_result);
		/*out_biling_model = extract_out_variable(out_biling_model);
		out_credit_amount = extract_out_variable(out_credit_amount);
		out_currency = extract_out_variable(out_currency);
		out_preffered_lang = extract_out_variable(out_preffered_lang);*/

		if (!in_username || !in_password)
		{
			//todo: throw Exception
			switch_log_printf(SWITCH_CHANNEL_LOG,SWITCH_LOG_ERROR, "Syntax error.\n" );
			return;
		}

		if (in_called_number)
			in_called_number_expanded = switch_channel_expand_variables(channel, in_called_number);
			
		in_username_expanded = switch_channel_expand_variables(channel, in_username);
		in_password_expanded = switch_channel_expand_variables(channel, in_password);


		if (radius_auth(channel, in_called_number_expanded, in_username_expanded, in_password_expanded ,
				auth_result/*, biling_model, credit_amount, currency, preffered_lang*/) != OK_RC)
		{
			switch_log_printf(SWITCH_CHANNEL_LOG,SWITCH_LOG_ERROR, "An error occured during radius authorization.\n");
		}
		

		switch_channel_set_variable(channel, out_auth_result, auth_result);
		
		/*switch_channel_set_variable(channel, out_biling_model, biling_model);
		switch_channel_set_variable(channel, out_credit_amount, credit_amount);
		switch_channel_set_variable(channel, out_currency, currency);
		switch_channel_set_variable(channel, out_preffered_lang, preffered_lang);*/
		
		if (in_called_number && in_called_number_expanded && in_called_number_expanded != in_called_number)
		{
			switch_safe_free(in_called_number_expanded);
		}
		
		if (in_username_expanded && in_username_expanded != in_username)
		{
			switch_safe_free(in_username_expanded);
		}

		if (in_password_expanded && in_password_expanded != in_password)
		{
			switch_safe_free(in_password_expanded);
		}
	}
}




switch_status_t load_config()
{
	CONFIG_VSAS* PCONFIGVSAS = NULL;
	CONFIG_CLIENT* PCONFIGCLIENT = NULL;

	char *cf = "rad_auth.conf";
	switch_xml_t cfg, xml, settings, param;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_event_t *params = NULL;
	
	char *name;
	char *id;
	char *value;
	char *pec;
	char *expr;
	char* direction;
			
	CONFIGVSAS = NULL;
	CONFIGCLIENT = NULL;

	switch_event_create(&params, SWITCH_EVENT_MESSAGE);
	switch_assert(params);
	switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "profile",
		"profile_rad_auth");

	//vsas
	
	if (!(xml = switch_xml_open_cfg(cf, &cfg, params)))
	{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR,
			"open of %s failed\n", cf);
		status = SWITCH_STATUS_FALSE;
		return status;
	}
	
	if ((settings = switch_xml_child(cfg, "settings")))
	{
		for (param = switch_xml_child(settings, "param"); param; param
			= param->next)
		{
			name = (char *) switch_xml_attr_soft(param, "name");
			value = (char *) switch_xml_attr_soft(param, "value");
			
			if (strcmp(name, "radius_config") == 0)
			{
				if (rc_config_file == NULL)
					rc_config_file = malloc(STR_LENGTH + 1);
				strcpy(rc_config_file, value);
				
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "radius config: %s\n", value);
			}
		}
	}
	
	if ((settings = switch_xml_child(cfg, "vsas")))
	{
		for (param = switch_xml_child(settings, "param"); param; param
			= param->next)
		{
			if (CONFIGVSAS == NULL)
			{
				CONFIGVSAS = malloc(sizeof(CONFIG_VSAS));
				PCONFIGVSAS = CONFIGVSAS;
			}
			else
			{
				PCONFIGVSAS->pNext = malloc(sizeof(CONFIG_VSAS));
				PCONFIGVSAS = PCONFIGVSAS->pNext;
			}
			
			name = (char *) switch_xml_attr_soft(param, "name");
			id = (char *) switch_xml_attr_soft(param, "id");
			value = (char *) switch_xml_attr_soft(param, "value");
			pec = (char *) switch_xml_attr_soft(param, "pec");
			expr = (char *) switch_xml_attr_soft(param, "expr");
			direction = (char *) switch_xml_attr_soft(param, "direction");
			
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "config attr: %s, %s, %s, %s, %s, %s\n", name, id, value, pec, expr, direction);
			
			PCONFIGVSAS->name = (char*) malloc(STR_LENGTH + 1);
			PCONFIGVSAS->value = (char*) malloc(STR_LENGTH + 1);
			
			strncpy(PCONFIGVSAS->name, name, STR_LENGTH);
			strncpy(PCONFIGVSAS->value, value, STR_LENGTH);
			PCONFIGVSAS->id = atoi(id);
			PCONFIGVSAS->pec = atoi(pec);
			PCONFIGVSAS->expr = atoi(expr);
			if(strcmp(direction, "in") == 0)
				PCONFIGVSAS->direction = 1;
			else
				PCONFIGVSAS->direction = 0;
			PCONFIGVSAS->pNext = NULL;
		}
	}
	
	
	if ((settings = switch_xml_child(cfg, "client")))
	{
		for (param = switch_xml_child(settings, "param"); param; param
			= param->next)
		{
			if (CONFIGCLIENT == NULL)
			{
				CONFIGCLIENT = malloc(sizeof(CONFIG_CLIENT));
				PCONFIGCLIENT = CONFIGCLIENT;
			}
			else
			{
				PCONFIGCLIENT->pNext = malloc(sizeof(CONFIG_CLIENT));
				PCONFIGCLIENT = PCONFIGCLIENT->pNext;
			}
			
			name = (char *) switch_xml_attr_soft(param, "name");
			value = (char *) switch_xml_attr_soft(param, "value");
			
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "config client: %s, %s\n", name, value);
			
			PCONFIGCLIENT->name = (char*) malloc(STR_LENGTH + 1);
			PCONFIGCLIENT->value = (char*) malloc(STR_LENGTH + 1);
			
			strncpy(PCONFIGCLIENT->name, name, STR_LENGTH);
			strncpy(PCONFIGCLIENT->value, value, STR_LENGTH);

			PCONFIGCLIENT->pNext = NULL;
		}
	}

	switch_xml_free(xml);
	return status;
}




SWITCH_MODULE_LOAD_FUNCTION(mod_rad_authload)
{
	switch_application_interface_t *app_interface;

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	
	SWITCH_ADD_APP(app_interface, "auth_function", NULL, NULL, auth_function, "in <USERNAME>, in <PASSWORD>, out <AUTH_RESULT>, out <BILING_MODEL>, out <CREDIT_AMOUNT>, out <CURRENCY>, out <PREFFERED_LANG>", SAF_SUPPORT_NOMEDIA);

	load_config();
	
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
		"mod rad_auth services is loaded.\n");

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_RUNTIME_FUNCTION(mod_rad_authruntime)
{
	return SWITCH_STATUS_TERM;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_rad_authshutdown) 	
{
	CONFIG_VSAS* PCONFIGVSAS = CONFIGVSAS;
	CONFIG_CLIENT* PCONFIGCLIENT = CONFIGCLIENT;
	
	CONFIG_VSAS* tmpVSAS = NULL;
	CONFIG_CLIENT* tmpCLIENT = NULL;
	
	while(PCONFIGVSAS)
	{
		if (PCONFIGVSAS->name)
			free(PCONFIGVSAS->name);
		PCONFIGVSAS->name = NULL;
		
		if (PCONFIGVSAS->value)
			free(PCONFIGVSAS->value);
		PCONFIGVSAS->value = NULL;		
		
		tmpVSAS = PCONFIGVSAS;
		PCONFIGVSAS = PCONFIGVSAS->pNext;
		
		free(tmpVSAS);
	}
	
	CONFIGVSAS = NULL;
	
	
	while(PCONFIGCLIENT)
	{
		if (PCONFIGCLIENT->name)
			free(PCONFIGCLIENT->name);
		PCONFIGCLIENT->name = NULL;
		
		if (PCONFIGCLIENT->value)
			free(PCONFIGCLIENT->value);
		PCONFIGCLIENT->value = NULL;		
		
		tmpCLIENT = PCONFIGCLIENT;
		PCONFIGCLIENT = PCONFIGCLIENT->pNext;
		
		free(tmpCLIENT);
	}
	
	CONFIGCLIENT = NULL;
	
	return SWITCH_STATUS_SUCCESS;
}



/*
* FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
* Copyright (C) 2005-2015, Anthony Minessale II <anthm@freeswitch.org>
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
* Christopher Rienzo <chris.rienzo@citrix.com>
*
* mod_hiredis.c -- redis client built using the C client library hiredis
*
*/

#include <mod_hiredis.h>

switch_status_t mod_hiredis_do_config() 
{
	char *conf = "hiredis.conf";
	switch_xml_t xml, cfg, profiles, profile, connections, connection, params, param;

	if (!(xml = switch_xml_open_cfg(conf, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "open of %s failed\n", conf);
		goto err;
	}

	if ( (profiles = switch_xml_child(cfg, "profiles")) != NULL) {
		for (profile = switch_xml_child(profiles, "profile"); profile; profile = profile->next) {		
			hiredis_profile_t *new_profile = NULL;
			uint8_t ignore_connect_fail = 0;
			char *name = (char *) switch_xml_attr_soft(profile, "name");

			// Load params
			if ( (params = switch_xml_child(profile, "params")) != NULL) {
				for (param = switch_xml_child(params, "param"); param; param = param->next) {
					char *var = (char *) switch_xml_attr_soft(param, "name");
					if ( !strncmp(var, "ignore-connect-fail", 19) ) {
						ignore_connect_fail = switch_true(switch_xml_attr_soft(param, "value"));
					}
				}
			}

			if ( hiredis_profile_create(&new_profile, name, ignore_connect_fail) == SWITCH_STATUS_SUCCESS ) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Created profile[%s]\n", name);
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to create profile[%s]\n", name);
			}

			/* Add connection to profile */
			if ( (connections = switch_xml_child(profile, "connections")) != NULL) {
				for (connection = switch_xml_child(connections, "connection"); connection; connection = connection->next) {		
					char *host = NULL, *password = NULL;
					uint32_t port = 0, timeout_ms = 0, max_connections = 0;
					
					for (param = switch_xml_child(connection, "param"); param; param = param->next) {
						char *var = (char *) switch_xml_attr_soft(param, "name");
						if ( !strncmp(var, "hostname", 8) ) {
							host = (char *) switch_xml_attr_soft(param, "value");
						} else if ( !strncmp(var, "port", 4) ) {
							port = atoi(switch_xml_attr_soft(param, "value"));
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "hiredis: adding conn[%u == %s]\n", port, switch_xml_attr_soft(param, "value"));
						} else if ( !strncmp(var, "timeout-ms", 10) || !strncmp(var, "timeout_ms", 10) ) {
							timeout_ms = atoi(switch_xml_attr_soft(param, "value"));
						} else if ( !strncmp(var, "password", 8) ) {
							password = (char *) switch_xml_attr_soft(param, "value");
						} else if ( !strncmp(var, "max-connections", 15) ) {
							max_connections = atoi(switch_xml_attr_soft(param, "value"));
						}
					}

					if ( hiredis_profile_connection_add(new_profile, host, password, port, timeout_ms, max_connections) == SWITCH_STATUS_SUCCESS) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Created profile[%s]\n", name);
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to create profile[%s]\n", name);
					}
				}
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Profile->connections config is missing\n");
				goto err;
			}
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Profiles config is missing\n");
		goto err;
	}
	
	return SWITCH_STATUS_SUCCESS;
	
 err:
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Configuration failed\n");
	return SWITCH_STATUS_GENERR;
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

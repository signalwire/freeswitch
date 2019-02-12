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
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Anthony Minessale II <anthm@freeswitch.org>
 * Ken Rice <krice at freeswitch.org>
 * Paul D. Tinsley <pdt at jackhammer.org>
 * Bret McDanel <trixter AT 0xdecafbad.com>
 * Raymond Chandler <intralanman@freeswitch.org>
 * Emmanuel Schmidbauer <eschmidbauer@gmail.com>
 * Kathleen King <kathleen.king@quentustech.com>
 *
 *
 * mod_sofia.c -- SOFIA SIP Endpoint
 *
 */

/* Best viewed in a 160 x 60 VT100 Terminal or so the line below at least fits across your screen*/
/*************************************************************************************************************************************************************/
#include "mod_sofia.h"


typedef switch_status_t (*sofia_command_t) (char **argv, int argc, switch_stream_handle_t *stream);

extern const char *sofia_state_names[];

switch_status_t build_sofia_status_json(cJSON * container)
{
	sofia_profile_t *profile = NULL;
	sofia_gateway_t *gp;
	switch_hash_index_t *hi;
	void *val;
	const void *vvar;
	switch_mutex_lock(mod_sofia_globals.hash_mutex);
	for (hi = switch_core_hash_first(mod_sofia_globals.profile_hash); hi; hi = switch_core_hash_next(&hi)) {
		cJSON * jprofile = cJSON_CreateObject();
		cJSON * jstatus = cJSON_CreateObject();
		switch_core_hash_this(hi, &vvar, NULL, &val);
		cJSON_AddItemToObject(container, (const char *)vvar, jprofile);
		cJSON_AddItemToObject(jprofile, "status", jstatus);
		profile = (sofia_profile_t *) val;
		if (strcmp(vvar, profile->name)) {
			cJSON_AddItemToObject(jstatus, "type", cJSON_CreateString("alias"));
			cJSON_AddItemToObject(jstatus, "data", cJSON_CreateString(profile->name));
			cJSON_AddItemToObject(jstatus, "state", cJSON_CreateString("ALIASED"));
		} else {
			cJSON_AddItemToObject(jstatus, "type", cJSON_CreateString("profile"));
			cJSON_AddItemToObject(jstatus, "state", cJSON_CreateString(sofia_test_pflag(profile, PFLAG_RUNNING) ? "RUNNING" : "DOWN"));
			cJSON_AddItemToObject(jstatus, "in-use", cJSON_CreateNumber(profile->inuse));
			if (! sofia_test_pflag(profile, PFLAG_TLS) || ! profile->tls_only) {
				cJSON_AddItemToObject(jstatus, "data", cJSON_CreateString(profile->url));
			} else if (sofia_test_pflag(profile, PFLAG_TLS)) {
				cJSON_AddItemToObject(jstatus, "data", cJSON_CreateString(profile->tls_url));
				cJSON_AddItemToObject(jstatus, "transport", cJSON_CreateString("tls"));
			} else if (profile->ws_bindurl) {
				cJSON_AddItemToObject(jprofile, "data", cJSON_CreateString(profile->ws_bindurl));
				cJSON_AddItemToObject(jprofile, "transport", cJSON_CreateString("ws"));
			} else if (profile->wss_bindurl) {
				cJSON_AddItemToObject(jprofile, "data", cJSON_CreateString(profile->wss_bindurl));
				cJSON_AddItemToObject(jprofile, "transport", cJSON_CreateString("wss"));
			}

			if (profile->gateways) {
				cJSON *gateways = cJSON_CreateObject();
				cJSON_AddItemToObject(jprofile, "gateways", gateways);
				for (gp = profile->gateways; gp; gp = gp->next) {
					cJSON *gateway = cJSON_CreateObject();
					cJSON_AddItemToObject(gateways, gp->name, gateway);
					switch_assert(gp->state < REG_STATE_LAST);
					cJSON_AddItemToObject(gateway, "type", cJSON_CreateString("gateway"));
					cJSON_AddItemToObject(gateway, "data", cJSON_CreateString(gp->register_to));
					cJSON_AddItemToObject(gateway, "state", cJSON_CreateString(sofia_state_names[gp->state]));
					if (gp->state == REG_STATE_FAILED || gp->state == REG_STATE_TRYING) {
						time_t now = switch_epoch_time_now(NULL);
						if (gp->retry > now) {
							cJSON_AddItemToObject(gateway, "retry", cJSON_CreateNumber(gp->retry - now));
						} else {
							cJSON_AddItemToObject(gateway, "retry", cJSON_CreateString("never"));
						}
					}
				}
			}
		}
	}
	switch_mutex_unlock(mod_sofia_globals.hash_mutex);
	return SWITCH_STATUS_SUCCESS;
}

switch_status_t build_sofia_profile_info_json(cJSON * container)
{
	sofia_profile_t *profile = NULL;
	cJSON *item, *iter = container->child;
	while(iter) {
		if ( (profile = sofia_glue_find_profile(iter->string))) {
			cJSON *info = cJSON_CreateObject();
			cJSON_AddItemToObject(iter, "info", info);

			cJSON_AddItemToObject(info, "domain-name", cJSON_CreateString(profile->domain_name ? profile->domain_name : "N/A"));
			if (strcasecmp(iter->string, profile->name)) {
				cJSON_AddItemToObject(info, "alias-of", cJSON_CreateString(switch_str_nil(profile->name)));
			}

			cJSON_AddItemToObject(info, "auto-nat", cJSON_CreateString(sofia_test_pflag(profile, PFLAG_AUTO_NAT) ? "true" : "false"));
			cJSON_AddItemToObject(info, "db-name", cJSON_CreateString(profile->dbname ? profile->dbname : switch_str_nil(profile->odbc_dsn)));
			cJSON_AddItemToObject(info, "pres-hosts", cJSON_CreateString(switch_str_nil(profile->presence_hosts)));
			cJSON_AddItemToObject(info, "dialplan", cJSON_CreateString(switch_str_nil(profile->dialplan)));
			cJSON_AddItemToObject(info, "context", cJSON_CreateString(switch_str_nil(profile->context)));
			cJSON_AddItemToObject(info, "challenge-realm", cJSON_CreateString(zstr(profile->challenge_realm) ? "auto_to" : profile->challenge_realm));

			item = cJSON_CreateStringArray((const char **)profile->rtpip, profile->rtpip_index);
			cJSON_AddItemToObject(info, "rtp-ip", item);

			cJSON_AddItemToObject(info, "ext-rtp-ip", cJSON_CreateString(profile->extrtpip));
			cJSON_AddItemToObject(info, "sip-ip", cJSON_CreateString(switch_str_nil(profile->sipip)));
			cJSON_AddItemToObject(info, "ext-sip-ip", cJSON_CreateString(switch_str_nil(profile->extsipip)));

			if (! sofia_test_pflag(profile, PFLAG_TLS) || ! profile->tls_only) {
				cJSON_AddItemToObject(info, "url", cJSON_CreateString(switch_str_nil(profile->url)));
				cJSON_AddItemToObject(info, "bind-url", cJSON_CreateString(switch_str_nil(profile->bindurl)));
			}
			if (sofia_test_pflag(profile, PFLAG_TLS)) {
				cJSON_AddItemToObject(info, "tls-url", cJSON_CreateString(switch_str_nil(profile->tls_url)));
				cJSON_AddItemToObject(info, "tls-bind-url", cJSON_CreateString(switch_str_nil(profile->tls_bindurl)));
			}
			if (profile->ws_bindurl) {
				cJSON_AddItemToObject(info, "ws-bind-url", cJSON_CreateString(switch_str_nil(profile->ws_bindurl)));
			}
			if (profile->wss_bindurl) {
				cJSON_AddItemToObject(info, "wss-bind-url", cJSON_CreateString(switch_str_nil(profile->wss_bindurl)));
			}

			cJSON_AddItemToObject(info, "hold-music", cJSON_CreateString(switch_str_nil(profile->hold_music)));
			cJSON_AddItemToObject(info, "outbound-proxy", cJSON_CreateString(zstr(profile->outbound_proxy) ? "N/A" : profile->outbound_proxy));

			sofia_glue_release_profile(profile);

		}

		iter = iter->next;

	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_JSON_API(sofia_status_json_function)
{
	cJSON *ret = cJSON_CreateObject();
	cJSON *profiles = cJSON_CreateObject();
	cJSON_AddItemToObject(ret, "profiles", profiles);
	build_sofia_status_json(profiles);

	*json_reply = ret;

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_STANDARD_JSON_API(sofia_status_info_json_function)
{
	cJSON *ret = cJSON_CreateObject();
	cJSON *profiles = cJSON_CreateObject();
	cJSON_AddItemToObject(ret, "profiles", profiles);
	build_sofia_status_json(profiles);
	build_sofia_profile_info_json(profiles);

	*json_reply = ret;

	return SWITCH_STATUS_SUCCESS;
}

switch_status_t cmd_json_status(char **argv, int argc, switch_stream_handle_t *stream)
{
	char * json;
	cJSON *ret = cJSON_CreateObject();
	cJSON *profiles = cJSON_CreateObject();
	cJSON_AddItemToObject(ret, "profiles", profiles);
	build_sofia_status_json(profiles);
	build_sofia_profile_info_json(profiles);

	json = cJSON_Print(ret);
	stream->write_function(stream, "%s\n", json);

	switch_safe_free(json);
	cJSON_Delete(ret);

	return SWITCH_STATUS_SUCCESS;
}

void add_sofia_json_apis(switch_loadable_module_interface_t **module_interface)
{
	switch_json_api_interface_t *json_api_interface;
	SWITCH_ADD_JSON_API(json_api_interface, "sofia.status", "sofia status JSON API", sofia_status_json_function, "");
	SWITCH_ADD_JSON_API(json_api_interface, "sofia.status.info", "sofia status JSON API", sofia_status_info_json_function, "");
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

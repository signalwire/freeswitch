/*
* Copyright (c) 2012, Sangoma Technologies
* Kapil Gupta <kgupta@sangoma.com>
* All rights reserved.
* 
* <Insert license here>
*/
#include "mod_media_gateway.h"
#include "media_gateway_stack.h"

/****************************************************************************************************************************/
static switch_xml_config_item_t *get_instructions(megaco_profile_t *profile) ;
static switch_xml_config_item_t *get_peer_instructions(mg_peer_profile_t *profile) ;
static int mg_sap_id;
static switch_status_t modify_mid(char** pmid);

/****************************************************************************************************************************/
switch_status_t config_profile(megaco_profile_t *profile, switch_bool_t reload)
{
	switch_xml_t cfg, xml, param, mg_interfaces, mg_interface, mg_peers, mg_peer, peer_interfaces ;
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_event_t *event = NULL;
	const char *file = "media_gateway.conf";
	switch_xml_config_item_t *instructions = (profile ? get_instructions(profile) : NULL);
	int count;
	int idx;
	char *var, *val;
	mg_peer_profile_t* peer_profile = NULL;
	switch_xml_config_item_t *instructions1 = NULL;
	switch_memory_pool_t *pool;

	if (!(xml = switch_xml_open_cfg(file, &cfg, NULL))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Could not open %s\n", file);
		goto done;
	}

	if (!(mg_interfaces = switch_xml_child(cfg, "mg_profiles"))) {
		goto done;
	}

	for (mg_interface = switch_xml_child(mg_interfaces, "mg_profile"); mg_interface; mg_interface = mg_interface->next) {
		const char *name = switch_xml_attr_soft(mg_interface, "name");
		if (strcmp(name, profile->name)) {
			continue;
		}

		count = switch_event_import_xml(switch_xml_child(mg_interface, "param"), "name", "value", &event);
		status = switch_xml_config_parse_event(event, count, reload, instructions);

		/* now build peer list */
		if (!(peer_interfaces = switch_xml_child(mg_interface, "peers"))) {
			goto done;
		}

		for (param = switch_xml_child(peer_interfaces, "param"); param; param = param->next) {
			var = (char *) switch_xml_attr_soft(param, "name");
			val = (char *) switch_xml_attr_soft(param, "value");
			profile->peer_list[profile->total_peers] = switch_core_strdup(profile->pool, val);
			profile->total_peers++;
		}

		if(SWITCH_STATUS_FALSE == (status = modify_mid(&profile->mid))){
			goto done;
		}


		profile->idx = ++mg_sap_id;

		/* we should break from here , profile name should be unique */
		break;
	}

	if (!mg_interface) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error profile %s not found\n", profile->name);
			return SWITCH_STATUS_FALSE;
	}

	/* go through the peer configuration and get the mg profile associated peers only */
	if (!(mg_peers = switch_xml_child(cfg, "mg_peers"))) {
		goto done;
	}

	for (mg_peer = switch_xml_child(mg_peers, "mg_peer"); mg_peer; mg_peer = mg_peer->next) {
		const char *name = switch_xml_attr_soft(mg_peer, "name");
		for(idx=0; idx<profile->total_peers; idx++){
			count = 0x00;
			event = NULL;
			peer_profile = NULL;
			if (!strcmp(name, profile->peer_list[idx])) {
				/* peer profile */
				switch_core_new_memory_pool(&pool);
				peer_profile = switch_core_alloc(pool, sizeof(*peer_profile));
				peer_profile->pool = pool;
				peer_profile->name = switch_core_strdup(peer_profile->pool, name);
				switch_thread_rwlock_create(&peer_profile->rwlock, peer_profile->pool);
				instructions1 = (peer_profile ? get_peer_instructions(peer_profile) : NULL);

				count = switch_event_import_xml(switch_xml_child(mg_peer, "param"), "name", "value", &event);
				if(SWITCH_STATUS_FALSE == (status = switch_xml_config_parse_event(event, count, reload, instructions1))){
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, " Peer XML Parsing failed \n");
					goto done;
				}

				if(SWITCH_STATUS_FALSE == (status = modify_mid(&peer_profile->mid))){
					goto done;
				}

				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"peer_profile name[%s], ipaddr[%s] port[%s], mid[%s] transport_type[%s], encoding_type[%s] \n",
						peer_profile->name, peer_profile->ipaddr, peer_profile->port,peer_profile->mid, peer_profile->transport_type, peer_profile->encoding_type);

				switch_core_hash_insert_wrlock(megaco_globals.peer_profile_hash, peer_profile->name, peer_profile, megaco_globals.peer_profile_rwlock);
			}
		}
	}

	/* configure the MEGACO stack */
	status = sng_mgco_cfg(profile);

done:
	if (xml) {
		switch_xml_free(xml);	
	}

	if (event) {
		switch_event_destroy(&event);
	}
	return status;
}

/****************************************************************************************************************************/
switch_status_t mg_config_cleanup(megaco_profile_t* profile)
{
	switch_xml_config_item_t *instructions = (profile ? get_instructions(profile) : NULL);
	switch_xml_config_cleanup(instructions);

	return SWITCH_STATUS_SUCCESS;
}

/****************************************************************************************************************************/
switch_status_t mg_peer_config_cleanup(mg_peer_profile_t* profile)
{
	switch_xml_config_item_t *instructions = (profile ? get_peer_instructions(profile) : NULL);
	switch_xml_config_cleanup(instructions);

	return SWITCH_STATUS_SUCCESS;
}

/****************************************************************************************************************************/
static switch_xml_config_item_t *get_peer_instructions(mg_peer_profile_t *profile) {
	switch_xml_config_item_t *dup;


	switch_xml_config_item_t instructions[] = {
		/* parameter name        type                 reloadable   pointer                         default value     options structure */
		SWITCH_CONFIG_ITEM("ip", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE, &profile->ipaddr, "", &switch_config_string_strdup, "", "Peer IP"),
		SWITCH_CONFIG_ITEM("port", SWITCH_CONFIG_STRING, 0, &profile->port, "", &switch_config_string_strdup, "", "peer port"),
		SWITCH_CONFIG_ITEM("encoding-scheme", SWITCH_CONFIG_STRING, 0, &profile->encoding_type, "", &switch_config_string_strdup, "", "peer encoding type"),
		SWITCH_CONFIG_ITEM("transport-type", SWITCH_CONFIG_STRING, 0, &profile->transport_type, "", &switch_config_string_strdup, "", "peer transport type "),
		SWITCH_CONFIG_ITEM("message-identifier", SWITCH_CONFIG_STRING, 0, &profile->mid, "", &switch_config_string_strdup, "", "peer message identifier "),
		SWITCH_CONFIG_ITEM_END()
	};

	dup = malloc(sizeof(instructions));
	memcpy(dup, instructions, sizeof(instructions));
	return dup;
}

/****************************************************************************************************************************/

static switch_xml_config_item_t *get_instructions(megaco_profile_t *profile) {
	switch_xml_config_item_t *dup;
	static switch_xml_config_int_options_t opt_version = { 
		SWITCH_TRUE,  /* enforce min */
		1,
		SWITCH_TRUE, /* enforce Max */
		3
	};

	static switch_xml_config_int_options_t opt_termination_id_len = {
		SWITCH_TRUE,  /* enforce min */
		1,
		SWITCH_TRUE, /* enforce Max */
		9
	};

	static switch_xml_config_enum_item_t opt_default_codec_enum[] = {
		{  "PCMA",  MEGACO_CODEC_PCMA},
		{  "PCMU",  MEGACO_CODEC_PCMU},
		{  "G.729",  MEGACO_CODEC_G729},
		{  "G.723.1",  MEGACO_CODEC_G723_1},
		{  "ILBC", MEGACO_CODEC_ILBC },
	};

	switch_xml_config_item_t instructions[] = {
		/* parameter name        type                 reloadable   pointer                         default value     options structure */
		SWITCH_CONFIG_ITEM("protocol", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE, &profile->protocol_type, "MEGACO", &switch_config_string_strdup, "", "MG Protocol type"),
		SWITCH_CONFIG_ITEM("version", SWITCH_CONFIG_INT, CONFIG_RELOADABLE, &profile->protocol_version, 2, &opt_version, "", "MG Protocol version"),
		SWITCH_CONFIG_ITEM("local-ip", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE, &profile->my_ipaddr, "127.0.0.1", &switch_config_string_strdup, "", "local ip"),
		SWITCH_CONFIG_ITEM("port", SWITCH_CONFIG_STRING, 0, &profile->port, "2944", &switch_config_string_strdup, "", "port"),
		SWITCH_CONFIG_ITEM("domain-name", SWITCH_CONFIG_STRING, 0, &profile->my_domain, "", &switch_config_string_strdup, "", "domain name"),
		SWITCH_CONFIG_ITEM("message-identifier", SWITCH_CONFIG_STRING, 0, &profile->mid, "", &switch_config_string_strdup, "", "message identifier "),

		SWITCH_CONFIG_ITEM("default-codec", SWITCH_CONFIG_ENUM, CONFIG_RELOADABLE, &profile->default_codec, "PCMU", &opt_default_codec_enum, "", "default codec"),
		SWITCH_CONFIG_ITEM("rtp-port-range", SWITCH_CONFIG_STRING, CONFIG_REQUIRED, &profile->rtp_port_range, "1-65535", &switch_config_string_strdup, "", "rtp port range"),
		SWITCH_CONFIG_ITEM("rtp-termination-id-prefix", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE, &profile->rtp_termination_id_prefix, "", &switch_config_string_strdup, "", "rtp termination prefix"),
		SWITCH_CONFIG_ITEM("rtp-termination-id-len", SWITCH_CONFIG_INT, CONFIG_RELOADABLE, &profile->rtp_termination_id_len, "", &opt_termination_id_len, "", "rtp termination id"),
		SWITCH_CONFIG_ITEM_END()
	};
	
	dup = malloc(sizeof(instructions));
	memcpy(dup, instructions, sizeof(instructions));
	return dup;
}

/****************************************************************************************************************************/

static switch_status_t modify_mid(char** pmid)
{
	char*			mid = *pmid;
	char*			dup;
	char* 			val[10];
	int 			count;
	switch_status_t		status = SWITCH_STATUS_FALSE;
	switch_assert(mid);

	dup = strdup(mid);

	/* If MID type is IP then add mid into [] brackets ,
	 * If MID type is domain then add mid into <> brackets *
	 */

	count = switch_split(dup, '.', val);

	if(!count) {
		/* Input string is not separated by '.', check if its separated by '-' as format could be xxx-xx-xxx/xxx-xx-xx-xxx  */
		if(0 == (count = switch_split(dup, '-', val))){
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid input MID string[%s]\n",mid);
			goto done;
		}
	}

	if(('<' == val[0][0]) || ('[' == val[0][0])){
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "MID = %s is already prefixed with proper brackets \n",mid);
		status = SWITCH_STATUS_SUCCESS;
		goto done;
	}
	
	/*first check could be if count is 3 means domain name as generally we have xxx-xx-xxx/xxx.xx.xxx domain */
	if(3 == count){
		/* domain-type, add value into <> */
		*pmid = switch_mprintf("<%s>", mid);
		free(mid);
		mid = *pmid;
	}else if(4 == count){
		/* IP address in xxx.xxx.xxx.xxx format */
		*pmid = switch_mprintf("[%s]", mid);
		free(mid);
		mid = *pmid;
	}else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid input MID string[%s]\n",mid);
		goto done;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Added proper brackets to MID = %s \n",mid);

	status = SWITCH_STATUS_SUCCESS;

done:
	return status;
}

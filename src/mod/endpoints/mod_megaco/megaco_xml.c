/*
* Copyright (c) 2012, Sangoma Technologies
* Kapil Gupta <kgupta@sangoma.com>
* All rights reserved.
* 
* <Insert license here>
*/
#include "mod_megaco.h"
#include "megaco_stack.h"

/****************************************************************************************************************************/
static switch_xml_config_item_t *get_instructions(megaco_profile_t *profile) ;
static switch_xml_config_item_t *get_peer_instructions(mg_peer_profile_t *profile) ;
static int mg_sap_id;

/****************************************************************************************************************************/
switch_status_t config_profile(megaco_profile_t *profile, switch_bool_t reload)
{
	switch_xml_t cfg, xml, param, mg_interfaces, mg_interface, mg_peers, mg_peer, peer_interfaces ;
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_event_t *event = NULL;
	const char *file = "megaco.conf";
	switch_xml_config_item_t *instructions = (profile ? get_instructions(profile) : NULL);
	int count;
	int idx;
	char *var, *val;
	mg_peer_profile_t* peer_profile = NULL;
	switch_xml_config_item_t *instructions1 = NULL;

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

		profile->idx = ++mg_sap_id;

		/* we should break from here , profile name should be unique */
		break;
	}

	/* go through the peer configuration and get the mg profile associated peers only */
	if (!(mg_peers = switch_xml_child(cfg, "mg_peers"))) {
		goto done;
	}

	count = 0x00;
	event = NULL;
	for (mg_peer = switch_xml_child(mg_peers, "mg_peer"); mg_peer; mg_peer = mg_peer->next) {
		const char *name = switch_xml_attr_soft(mg_peer, "name");
		for(idx=0; idx<profile->total_peers; idx++){
			if (!strcmp(name, profile->peer_list[idx])) {
				/* peer profile */
				peer_profile = switch_core_alloc(profile->pool, sizeof(*peer_profile));
				peer_profile->pool = profile->pool;
				peer_profile->name = switch_core_strdup(peer_profile->pool, name);
				switch_thread_rwlock_create(&peer_profile->rwlock, peer_profile->pool);
				instructions1 = (peer_profile ? get_peer_instructions(peer_profile) : NULL);

				count = switch_event_import_xml(switch_xml_child(mg_peer, "param"), "name", "value", &event);
				if(SWITCH_STATUS_FALSE == (status = switch_xml_config_parse_event(event, count, reload, instructions1))){
				     switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, " Peer XML Parsing failed \n");
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
static switch_xml_config_item_t *get_peer_instructions(mg_peer_profile_t *profile) {
	switch_xml_config_item_t *dup;

	switch_xml_config_item_t instructions[] = {
		/* parameter name        type                 reloadable   pointer                         default value     options structure */
		SWITCH_CONFIG_ITEM("ip", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE, &profile->ipaddr, "", &switch_config_string_strdup, "", "Peer IP"),
		SWITCH_CONFIG_ITEM("port", SWITCH_CONFIG_STRING, 0, &profile->port, "", &switch_config_string_strdup, "", "peer port"),
		SWITCH_CONFIG_ITEM("encoding-scheme", SWITCH_CONFIG_STRING, 0, &profile->encoding_type, "TEXT", &switch_config_string_strdup, "", "peer encoding type"),
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
		SWITCH_TRUE, /* Enforce Max */
		3
	};

	switch_xml_config_item_t instructions[] = {
		/* parameter name        type                 reloadable   pointer                         default value     options structure */
		SWITCH_CONFIG_ITEM("protocol", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE, &profile->protocol_type, "MEGACO", &switch_config_string_strdup, "", "MG Protocol type"),
		SWITCH_CONFIG_ITEM("version", SWITCH_CONFIG_INT, CONFIG_RELOADABLE, &profile->protocol_version, 2, &opt_version, "", "MG Protocol version"),
		SWITCH_CONFIG_ITEM("local-ip", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE, &profile->my_ipaddr, "127.0.0.1", &switch_config_string_strdup, "", "local ip"),
		SWITCH_CONFIG_ITEM("port", SWITCH_CONFIG_STRING, 0, &profile->port, "2944", &switch_config_string_strdup, "", "port"),
		SWITCH_CONFIG_ITEM("domain-name", SWITCH_CONFIG_STRING, 0, &profile->my_domain, "", &switch_config_string_strdup, "", "domain name"),
		SWITCH_CONFIG_ITEM("message-identifier", SWITCH_CONFIG_STRING, 0, &profile->mid, "", &switch_config_string_strdup, "", "message identifier "),
		SWITCH_CONFIG_ITEM_END()
	};
	
	dup = malloc(sizeof(instructions));
	memcpy(dup, instructions, sizeof(instructions));
	return dup;
}

/****************************************************************************************************************************/

#if 0
switch_status_t sng_parse_mg_profile(switch_xml_t mg_interface)
{
	int i = 0x00;
	const char *prof_name   = NULL;
	switch_xml_t param;

	/*************************************************************************/
	prof_name = switch_xml_attr_soft(mg_interface, "name");

	/*************************************************************************/
	for (param = switch_xml_child(mg_interface, "param"); param; param = param->next) {
		char *var = (char *) switch_xml_attr_soft(param, "name");
		char *val = (char *) switch_xml_attr_soft(param, "value");
		if (!var || !val) {
			continue;
		}

		/******************************************************************************************/
		if(!strcasecmp(var, "id")){
			i   = atoi(val);
			megaco_globals.g_mg_cfg.mgCfg[i].id = i;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, " mg_interface Id[%d] \n", i);
			/*******************************************************************************************/
		}else if(!strcasecmp(var, "protocol")){
			/********************************************************************************************/
			if(!strcasecmp(val,"MEGACO")) {
				megaco_globals.g_mg_cfg.mgCfg[i].protocol_type = SNG_MG_MEGACO;
			}else if(!strcasecmp(val,"MGCP")){
				megaco_globals.g_mg_cfg.mgCfg[i].protocol_type = SNG_MG_MGCP;
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "MGCP Protocol Not Yet Supported \n");
				return SWITCH_STATUS_FALSE;
			}else{
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Protocol Value[%s] \n",val);
				return SWITCH_STATUS_FALSE;
			}
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, " mg_interface protocol[%d] \n", 
					megaco_globals.g_mg_cfg.mgCfg[i].protocol_type);
			/********************************************************************************************/
		}else if(!strcasecmp(var, "version")){
			/********************************************************************************************/
			megaco_globals.g_mg_cfg.mgCfg[i].protocol_version = atoi(val);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, " mg_interface protocol version[%s] \n",val); 
			if((megaco_globals.g_mg_cfg.mgCfg[i].protocol_version < 1) 
					|| (megaco_globals.g_mg_cfg.mgCfg[i].protocol_version > 3))
			{
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Protocol version[%s] , Supported values are [1/2/3] \n",val);
				return SWITCH_STATUS_FALSE;
			}
			/********************************************************************************************/
		}else if(!strcasecmp(var, "transportProfileId")){
			/********************************************************************************************/
			megaco_globals.g_mg_cfg.mgCfg[i].transport_prof_id   = atoi(val);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, " mg_interface transport_prof_id[%d] \n", 
					megaco_globals.g_mg_cfg.mgCfg[i].transport_prof_id);
			/********************************************************************************************/
		}else if(!strcasecmp(var, "localIp")){
			/***********************************************************************i*********************/
			strcpy((char*)&megaco_globals.g_mg_cfg.mgCfg[i].my_ipaddr[0],val);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, " mg_interface my_ipaddr[%s] \n", 
					megaco_globals.g_mg_cfg.mgCfg[i].my_ipaddr);
			/********************************************************************************************/
		}else if(!strcasecmp(var, "port")){
			/********************************************************************************************/
			megaco_globals.g_mg_cfg.mgCfg[i].port   = atoi(val);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, 
					" mg_interface my_port[%d] \n", megaco_globals.g_mg_cfg.mgCfg[i].port);
			/********************************************************************************************/
		}else if(!strcasecmp(var, "myDomainName")){
			/********************************************************************************************/
			strcpy((char*)&megaco_globals.g_mg_cfg.mgCfg[i].my_domain[0],val);	
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, 
					" mg_interface myDomainName[%s] \n", megaco_globals.g_mg_cfg.mgCfg[i].my_domain);
			/********************************************************************************************/
		}else if(!strcasecmp(var, "mid")){
			/********************************************************************************************/
			strcpy((char*)&megaco_globals.g_mg_cfg.mgCfg[i].mid[0],val);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, 
					" mg_interface mid[%s] \n", megaco_globals.g_mg_cfg.mgCfg[i].mid);
			/********************************************************************************************/
		}else if(!strcasecmp(var, "peerId")){
			/********************************************************************************************/
			megaco_globals.g_mg_cfg.mgCfg[i].peer_id   = atoi(val);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, 
					" mg_interface peerId[%d] \n", megaco_globals.g_mg_cfg.mgCfg[i].peer_id);
			/********************************************************************************************/
		}else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, " Invalid var[%s] in mg_interface \n", var);
			return SWITCH_STATUS_FALSE;
		}
	}

	strcpy((char*)&megaco_globals.g_mg_cfg.mgCfg[i].name[0], prof_name);

	return SWITCH_STATUS_SUCCESS;
}

/***********************************************************************************************************/

switch_status_t sng_parse_mg_tpt_profile(switch_xml_t mg_tpt_profile)
{
	int i = 0x00;
	switch_xml_t param;
	const char *prof_name   = NULL;

	/*************************************************************************/
	prof_name = switch_xml_attr_soft(mg_tpt_profile, "name");

	/*************************************************************************/
	for (param = switch_xml_child(mg_tpt_profile, "param"); param; param = param->next) {
		char *var = (char *) switch_xml_attr_soft(param, "name");
		char *val = (char *) switch_xml_attr_soft(param, "value");
		if (!var || !val) {
			continue;
		}

		/******************************************************************************************/
		if(!strcasecmp(var, "id")){
			/*******************************************************************************************/
			i   = atoi(val);
			megaco_globals.g_mg_cfg.mgTptProf[i].id = i;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, " mg_tpt_profile Id[%d] \n", i);
			/*******************************************************************************************/
		}else if(!strcasecmp(var, "transportType")){
			/*******************************************************************************************/
			if(!strcasecmp(val,"UDP")) {
				megaco_globals.g_mg_cfg.mgTptProf[i].transport_type = SNG_MG_TPT_UDP;
			}else if(!strcasecmp(val,"TCP")){
				megaco_globals.g_mg_cfg.mgTptProf[i].transport_type = SNG_MG_TPT_TCP;
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "TCP Transport for H.248 Protocol Not Yet Supported \n");
				return SWITCH_STATUS_FALSE;
			}else if(!strcasecmp(val,"STCP")){
				megaco_globals.g_mg_cfg.mgTptProf[i].transport_type = SNG_MG_TPT_SCTP;
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "STCP Transport for H.248 Protocol Not Yet Supported \n");
				return SWITCH_STATUS_FALSE;
			}else{
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Protocol Value[%s] \n",val);
				return SWITCH_STATUS_FALSE;
			}
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, " mg_tpt_profile transport_type[%d] \n", 
					megaco_globals.g_mg_cfg.mgTptProf[i].transport_type);
			/********************************************************************************************/
		}else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, " Invalid var[%s] in mg_transport \n", var);
			return SWITCH_STATUS_FALSE;
		}
	}

	strcpy((char*)&megaco_globals.g_mg_cfg.mgTptProf[i].name[0], prof_name);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, 
			" mg_tpt_profile Name[%s] \n", &megaco_globals.g_mg_cfg.mgTptProf[i].name[0]); 

	return SWITCH_STATUS_SUCCESS;
}
/***********************************************************************************************************/

switch_status_t sng_parse_mg_peer_profile(switch_xml_t mg_peer_profile)
{
	int i = 0x00;
	switch_xml_t param;
	const char *prof_name   = NULL;

	/*************************************************************************/
	prof_name = switch_xml_attr_soft(mg_peer_profile, "name");

	for (param = switch_xml_child(mg_peer_profile, "param"); param; param = param->next) {
		/***********************************************************************************************************/
		char *var = (char *) switch_xml_attr_soft(param, "name");
		char *val = (char *) switch_xml_attr_soft(param, "value");
		if (!var || !val) {
			continue;
		}

		/***********************************************************************************************************/
		if(!strcasecmp(var, "id")){
			/***********************************************************************************************************/
			i   = atoi(val);
			megaco_globals.g_mg_cfg.mgPeer.peers[i].id = i;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, " mg_peer_profile Id[%d] \n", i);
			/***********************************************************************************************************/
		}else if(!strcasecmp(var, "port")){
			/***********************************************************************************************************/
			megaco_globals.g_mg_cfg.mgPeer.peers[i].port = atoi(val);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, 
					" mg_peer_profile port[%d] \n", megaco_globals.g_mg_cfg.mgPeer.peers[i].port);
			/***********************************************************************************************************/
		}else if(!strcasecmp(var, "encodingScheme")){
			/***********************************************************************************************************/
			if(!strcasecmp(val, "TEXT")){
				megaco_globals.g_mg_cfg.mgPeer.peers[i].encoding_type = SNG_MG_ENCODING_TEXT; 
			} else if(!strcasecmp(val, "BINARY")){
				megaco_globals.g_mg_cfg.mgPeer.peers[i].encoding_type = SNG_MG_ENCODING_BINARY; 
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Encoding Type[%s] \n",val);
				return SWITCH_STATUS_FALSE;
			}
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, 
					" mg_peer_profile encodingScheme[%d] \n", megaco_globals.g_mg_cfg.mgPeer.peers[i].encoding_type);
			/***********************************************************************************************************/
		}else if(!strcasecmp(var, "mid")){
			/***********************************************************************************************************/
			strcpy((char*)&megaco_globals.g_mg_cfg.mgPeer.peers[i].mid[0],val);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, 
					" mg_peer_profile mid[%s] \n", megaco_globals.g_mg_cfg.mgPeer.peers[i].mid);
			/***********************************************************************************************************/
		}else if(!strcasecmp(var, "ip")){
			/***********************************************************************************************************/
			strcpy((char*)&megaco_globals.g_mg_cfg.mgPeer.peers[i].ipaddr[0],val);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, 
					" mg_peer_profile ip[%s] \n", megaco_globals.g_mg_cfg.mgPeer.peers[i].ipaddr);
			/***********************************************************************************************************/
		}else{
			/***********************************************************************************************************/
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, " Invalid var[%s] in mg_peer \n", var);
			return SWITCH_STATUS_FALSE;
		}
	}

	strcpy((char*)&megaco_globals.g_mg_cfg.mgPeer.peers[i].name[0], prof_name);

	megaco_globals.g_mg_cfg.mgPeer.total_peer++;
	return SWITCH_STATUS_SUCCESS;
}
#endif
/***********************************************************************************************************/

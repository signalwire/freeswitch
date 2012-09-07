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
static switch_status_t modify_mg_profile_mid(megaco_profile_t *profile, char** pmid) ;
static switch_status_t modify_mg_peer_mid(mg_peer_profile_t *peer_profile, char** pmid) ;

/****************************************************************************************************************************/
switch_status_t config_profile(megaco_profile_t *profile, switch_bool_t reload)
{
	switch_xml_t cfg, xml, param, mg_interfaces, mg_interface, mg_peers, mg_peer, mg_phys_terms, mg_term, peer_interfaces ;
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_event_t *event = NULL;
	const char *file = "media_gateway.conf";
	switch_xml_config_item_t *instructions = (profile ? get_instructions(profile) : NULL);
	int count;
	int idx;
	int ret=0;
	char *var, *val;
	mg_peer_profile_t* peer_profile = NULL;
	switch_xml_config_item_t *instructions1 = NULL;
	switch_memory_pool_t *pool;
	char lic_sig_file[4096];

	memset(&lic_sig_file[0],0,sizeof(lic_sig_file));

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

		/* If RTP-IP is not defined then default to local-ip */
		if((!profile->rtp_ipaddr) || 
				(profile->rtp_ipaddr && ('\0' == profile->rtp_ipaddr[0]))){
			profile->rtp_ipaddr = switch_mprintf("%s", profile->my_ipaddr);
		}

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"rtp_ipaddr[%s], local ip[%s]\n", profile->rtp_ipaddr, profile->my_ipaddr);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,"t38_fax_notify[%s]\n",
		(MG_T38_FAX_NOTIFY_YES == profile->t38_fax_notify)?"ENABLE":"DISABLE");

		if(SWITCH_STATUS_FALSE == (status = modify_mg_profile_mid(profile, &profile->mid))){
			goto done;
		}

		profile->idx = ++mg_sap_id;

		if ((mg_phys_terms = switch_xml_child(mg_interface, "physical_terminations"))) {
			for (mg_term = switch_xml_child(mg_phys_terms, "map"); mg_term; mg_term = mg_term->next) {
				// <map  termination-id-prefix="Term1/" termination-id-base="1" tech="freetdm" channel-prefix="wp2" channel-map"1-15,17-31"/>
				const char *prefix = switch_xml_attr(mg_term, "termination-id-prefix");
				const char *sztermination_id_base = switch_xml_attr(mg_term, "termination-id-base");
				const char *tech =  switch_xml_attr(mg_term, "tech");
				const char *channel_prefix = switch_xml_attr(mg_term, "channel-prefix");
				const char *channel_map = switch_xml_attr(mg_term, "channel-map");
				char *p = NULL;

				if (!zstr(channel_map)) {
					/* Split channel-map */
					char *channel_map_dup = strdup(channel_map);
					char *chanmap[24] = {0};
					int chanmap_count = 0;
					int i = 0;
					int startchan, endchan, j;
					int mg_term_idx = (sztermination_id_base)?atoi(sztermination_id_base):1; 

					/* we can have following combinations *
					 * i)   only one channel i.e. channel-map="1"
					 * ii)  only one chanel range i.e channel-map="1-15"
					 * iii) full channel range i.e. channel-map="1-15,17-31"
					 */


					chanmap_count = switch_split(channel_map_dup, ',', chanmap);

					if(1 == chanmap_count) {
						p = strchr(channel_map_dup, '-');
						if(NULL != p){
							/* case (ii) */ 
							i = switch_split(channel_map_dup, '-', chanmap);
							if(i && chanmap[0] && chanmap[1]) {
								startchan = atoi(chanmap[0]);
								endchan   = atoi(chanmap[1]);
								for (j = startchan; j <= endchan; j++) {
									mg_create_tdm_term(profile, tech, channel_prefix, prefix, mg_term_idx, j);
									mg_term_idx++;
								}
							}
						}else{
							/* case (i) */ 
							p = channel_map_dup;	
							startchan = endchan = atoi(p);
							mg_create_tdm_term(profile, tech, channel_prefix, prefix, mg_term_idx, startchan);
						}
					}else
					{
						/* case (iii) */
						for (i = 0; i < chanmap_count; i++) {
							p = strchr(chanmap[i], '-');
							if (p) {
								*p++ = '\0';
								startchan = atoi(chanmap[i]);
								endchan = atoi(p);

								for (j = startchan; j <= endchan; j++) {
if (0 == i)
									mg_create_tdm_term(profile, tech, channel_prefix, prefix, mg_term_idx, j);
else
									mg_create_tdm_term(profile, tech, channel_prefix, prefix, mg_term_idx, j-1);
									mg_term_idx++;
								}
							}
						}
					}

					free(channel_map_dup);
				}
			}
		}


		/* we should break from here , profile name should be unique */
		break;
	}

	if (!mg_interface) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error profile %s not found\n", profile->name);
		status = SWITCH_STATUS_FALSE;
	}


	if((profile->license) && ('\0' != profile->license[0])){
		sprintf(lic_sig_file, "%s.sig", profile->license);
	}else{
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to get License file \n");
		status = SWITCH_STATUS_FALSE;
		goto done;
	}

	MG_CHECK_LICENSE(profile->total_cfg_term,profile->license, &lic_sig_file[0], ret);
	if(ret){
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "License validation failed \n");
		status = SWITCH_STATUS_FALSE;
		goto done;
	}


	/* go through the peer configuration and get the mg profile associated peers only */
	if (!(mg_peers = switch_xml_child(cfg, "mg_peers"))) {
		status = SWITCH_STATUS_FALSE;
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

				if (SWITCH_STATUS_FALSE == (status = modify_mg_peer_mid(peer_profile, &peer_profile->mid))) {
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

	switch_safe_free(instructions);

	return status;
}

/****************************************************************************************************************************/
void mg_create_tdm_term(megaco_profile_t *profile, const char *tech, const char *channel_prefix, const char *prefix, int chan_num, int tdm_chan_num)
{
	mg_termination_t *term;
	switch_memory_pool_t *pool;

	switch_core_new_memory_pool(&pool);
	term = switch_core_alloc(profile->pool, sizeof *term);
	term->pool = pool;
	term->type = MG_TERM_TDM;
	term->profile = profile;
	term->tech = switch_core_strdup(pool, tech);
	term->active_events = NULL;
	term->name = switch_core_sprintf(pool, "%s%d", prefix, chan_num);
	term->u.tdm.channel = tdm_chan_num;
	term->u.tdm.span_name = switch_core_strdup(pool, channel_prefix);
	switch_set_flag(term, MG_OUT_OF_SERVICE);
    switch_clear_flag(term, MG_FAX_NOTIFIED);

	switch_core_hash_insert_wrlock(profile->terminations, term->name, term, profile->terminations_rwlock);
	term->next = profile->physical_terminations;
	profile->physical_terminations = term;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, 
			"Mapped termination [%s] to freetdm span: %s chan: %d\n", 
			term->name, term->u.tdm.span_name, term->u.tdm.channel);
	megaco_prepare_tdm_termination(term);

	/* by-default : DTMF removal disable 
	 * by default do not modify in-band audio stream*/
	megaco_tdm_term_dtmf_removal(term,0x00);

	profile->total_cfg_term++;
}
/****************************************************************************************************************************/
switch_status_t mg_config_cleanup(megaco_profile_t* profile)
{
	switch_xml_config_item_t *instructions = (profile ? get_instructions(profile) : NULL);
	switch_xml_config_cleanup(instructions);
    free(instructions);

	return SWITCH_STATUS_SUCCESS;
}

/****************************************************************************************************************************/
switch_status_t mg_peer_config_cleanup(mg_peer_profile_t* profile)
{
	switch_xml_config_item_t *instructions = (profile ? get_peer_instructions(profile) : NULL);
	switch_xml_config_cleanup(instructions);
    free(instructions);

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

#if 0
	static switch_xml_config_int_options_t opt_termination_id_len = {
		SWITCH_TRUE,  /* enforce min */
		1,
		SWITCH_TRUE, /* enforce Max */
		9
	};

    static switch_xml_config_int_options_t pre_buffer_len = {
		SWITCH_TRUE,  /* enforce min */
		0,
		SWITCH_TRUE, /* enforce Max */
		10000
	};
    
	static switch_xml_config_enum_item_t opt_default_codec_enum[] = {
		{  "PCMA",  MEGACO_CODEC_PCMA},
		{  "PCMU",  MEGACO_CODEC_PCMU},
		{  "G.729",  MEGACO_CODEC_G729},
		{  "G.723.1",  MEGACO_CODEC_G723_1},
		{  "ILBC", MEGACO_CODEC_ILBC },
	};

	static switch_xml_config_enum_item_t opt_fax_detect_type_enum[] = {
		{  "CED",  MG_FAX_DETECT_EVENT_TYPE_CED},
		{  "CNG",  MG_FAX_DETECT_EVENT_TYPE_CNG},
		{  "CED_CNG",  MG_FAX_DETECT_EVENT_TYPE_CNG_CED},
		{  "DISABLE",  MG_FAX_DETECT_EVENT_TYPE_DISABLE},
	};
#endif
	static switch_xml_config_enum_item_t opt_t38_fax_notify[] = {
		{  "ENABLE",  MG_T38_FAX_NOTIFY_YES},
		{  "DISABLE",  MG_T38_FAX_NOTIFY_NO},
	};



	switch_xml_config_item_t instructions[] = {
		/* parameter name        type                 reloadable   pointer                         default value     options structure */
		SWITCH_CONFIG_ITEM("protocol", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE, &profile->protocol_type, "MEGACO", &switch_config_string_strdup, "", "MG Protocol type"),
		SWITCH_CONFIG_ITEM("version", SWITCH_CONFIG_INT, CONFIG_RELOADABLE, &profile->protocol_version, 2, &opt_version, "", "MG Protocol version"),
		SWITCH_CONFIG_ITEM("local-ip", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE, &profile->my_ipaddr, "127.0.0.1", &switch_config_string_strdup, "", "local ip"),
		SWITCH_CONFIG_ITEM("port", SWITCH_CONFIG_STRING, 0, &profile->port, "2944", &switch_config_string_strdup, "", "port"),
		SWITCH_CONFIG_ITEM("domain-name", SWITCH_CONFIG_STRING, 0, &profile->my_domain, "", &switch_config_string_strdup, "", "domain name"),
		SWITCH_CONFIG_ITEM("message-identifier", SWITCH_CONFIG_STRING, 0, &profile->mid, "", &switch_config_string_strdup, "", "message identifier "),

		//SWITCH_CONFIG_ITEM("default-codec", SWITCH_CONFIG_ENUM, CONFIG_RELOADABLE, &profile->default_codec, "PCMU", &opt_default_codec_enum, "", "default codec"),
		//SWITCH_CONFIG_ITEM("rtp-port-range", SWITCH_CONFIG_STRING, CONFIG_REQUIRED, &profile->rtp_port_range, "1-65535", &switch_config_string_strdup, "", "rtp port range"),
		SWITCH_CONFIG_ITEM("rtp-termination-id-prefix", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE, &profile->rtp_termination_id_prefix, "", &switch_config_string_strdup, "", "rtp termination prefix"),
		//SWITCH_CONFIG_ITEM("rtp-termination-id-length", SWITCH_CONFIG_INT, CONFIG_RELOADABLE, &profile->rtp_termination_id_len, "", &opt_termination_id_len, "", "rtp termination id"),
		//SWITCH_CONFIG_ITEM("tdm-pre-buffer-size", SWITCH_CONFIG_INT, CONFIG_RELOADABLE, &profile->tdm_pre_buffer_size, 0, &pre_buffer_len, "", "freetdm pre buffer size"),
		SWITCH_CONFIG_ITEM("codec-prefs", SWITCH_CONFIG_STRING, 0, &profile->codec_prefs, "", &switch_config_string_strdup, "", "codec preferences, coma-separated"),
		SWITCH_CONFIG_ITEM("license", SWITCH_CONFIG_STRING, 0, &profile->license, "/usr/local/nsg/conf/license.txt", &switch_config_string_strdup, "", "License file"),
		SWITCH_CONFIG_ITEM("rtp-ip", SWITCH_CONFIG_STRING, CONFIG_RELOADABLE, &profile->rtp_ipaddr, "" , &switch_config_string_strdup, "", "rtp ip"),
		//SWITCH_CONFIG_ITEM("fax-detect-event-type", SWITCH_CONFIG_ENUM, CONFIG_RELOADABLE, &profile->fax_detect_evt_type, MG_FAX_DETECT_EVENT_TYPE_CNG_CED , &opt_fax_detect_type_enum, "", "fax-detect-event-type"),
		SWITCH_CONFIG_ITEM("t38-fax-notify", SWITCH_CONFIG_ENUM, CONFIG_RELOADABLE, &profile->t38_fax_notify, MG_T38_FAX_NOTIFY_YES , &opt_t38_fax_notify, "", "t38_fax_notify"),
		SWITCH_CONFIG_ITEM_END()
	};
	
	dup = malloc(sizeof(instructions));
	memcpy(dup, instructions, sizeof(instructions));
	return dup;
}

/****************************************************************************************************************************/
static switch_status_t modify_mg_peer_mid(mg_peer_profile_t *peer_profile, char** pmid)
{
	char*			mid = *pmid;
	switch_assert(mid);
	switch_assert(peer_profile);

	if(!strcasecmp(mid,"IP-PORT")){
		*pmid = switch_mprintf("[%s]:%s", peer_profile->ipaddr,peer_profile->port);
	} else if(!strcasecmp(mid,"IP")){
		*pmid = switch_mprintf("[%s]", peer_profile->ipaddr);
	}else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Invalid mid-type[%s] \n",mid);
		return SWITCH_STATUS_FALSE;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Updated PEER MID [%s] \n",*pmid);
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t modify_mg_profile_mid(megaco_profile_t *profile, char** pmid)
{
	char*			mid = *pmid;
	//char*			dup;
	//char* 			val[10];
//	int 			count;
	//switch_status_t		status = SWITCH_STATUS_SUCCESS;
	switch_assert(mid);
	switch_assert(profile);

	if(!strcasecmp(mid,"IP-PORT")){
		*pmid = switch_mprintf("[%s]:%s", profile->my_ipaddr,profile->port);
	} else if(!strcasecmp(mid,"IP")){
		*pmid = switch_mprintf("[%s]", profile->my_ipaddr);
	} else if(!strcasecmp(mid,"DOMAIN")){
		*pmid = switch_mprintf("<%s>", profile->my_domain);
	}else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Invalid mid-type[%s] \n",mid);
		return SWITCH_STATUS_FALSE;
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Updated MG MID [%s] \n",*pmid);
	return SWITCH_STATUS_SUCCESS;
#if 0
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
#endif
}

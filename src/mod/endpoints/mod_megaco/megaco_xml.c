/*
* Copyright (c) 2012, Sangoma Technologies
* Kapil Gupta <kgupta@sangoma.com>
* All rights reserved.
* 
* <Insert license here>
*/
#include "mod_megaco.h"


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

	megaco_globals.g_mg_cfg.mgPeer.total_peer++;
	return SWITCH_STATUS_SUCCESS;
}
/***********************************************************************************************************/

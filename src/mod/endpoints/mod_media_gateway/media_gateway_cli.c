/*
* Copyright (c) 2012, Sangoma Technologies
* Kapil Gupta <kgupta@sangoma.com>
* All rights reserved.
* 
* <Insert license here>
*/

/* INCLUDES *******************************************************************/
#include "mod_media_gateway.h"
#include "media_gateway_stack.h"
/******************************************************************************/

/* FUNCTION PROTOTYPES ********************************************************/
switch_status_t megaco_profile_status(switch_stream_handle_t *stream, megaco_profile_t* mg_cfg);
switch_status_t megaco_profile_xmlstatus(switch_stream_handle_t *stream, megaco_profile_t* mg_cfg);
switch_status_t megaco_profile_peer_xmlstatus(switch_stream_handle_t *stream, megaco_profile_t* mg_cfg);
switch_status_t handle_term_status_cli_cmd(switch_stream_handle_t *stream, megaco_profile_t* mg_profile, char* term_id);
switch_status_t handle_all_term_status_cli_cmd(switch_stream_handle_t *stream, megaco_profile_t* mg_profile);
void get_peer_xml_buffer(char* prntBuf, MgPeerSta* cfm);
void  megaco_cli_print_usage(switch_stream_handle_t *stream);
switch_status_t handle_show_activecalls_cli_cmd(switch_stream_handle_t *stream, megaco_profile_t* mg_profile);
switch_status_t handle_show_stats(switch_stream_handle_t *stream, megaco_profile_t* mg_profile);
switch_status_t handle_show_stack_mem(switch_stream_handle_t *stream);
switch_status_t handle_span_term_status_cli_cmd(switch_stream_handle_t *stream, megaco_profile_t* mg_profile, char* span_name);

/******************************************************************************/

/* FUNCTIONS ******************************************************************/

switch_status_t mg_process_cli_cmd(const char *cmd, switch_stream_handle_t *stream)
{
	int 			argc;
	char* 			argv[10];
	char* 			dup = NULL;
	int                     wild = 0x00;
	megaco_profile_t* 	profile = NULL;

	if (zstr(cmd)) {
		goto usage;
	}

	dup = strdup(cmd);
	argc = switch_split(dup, ' ', argv);

	if (argc < 1 || zstr(argv[0])) {
		goto usage;
	}

/**********************************************************************************/
	if (!strcmp(argv[0], "profile")) {
		if (zstr(argv[1]) || zstr(argv[2])) {
			goto usage;
		}
/**********************************************************************************/
	 	profile = megaco_profile_locate(argv[1]);

/**********************************************************************************/
		if (!strcmp(argv[2], "start")) {
/**********************************************************************************/
			if (profile) {
				megaco_profile_release(profile);
				stream->write_function(stream, "-ERR Profile %s is already started\n", argv[2]);
			} else {
				megaco_profile_start(argv[1]);
				stream->write_function(stream, "+OK\n");
			}
/**********************************************************************************/
		} else if (!strcmp(argv[2], "stop")) {
/**********************************************************************************/
			if (profile) {
				megaco_profile_release(profile);
				megaco_profile_destroy(&profile);
				stream->write_function(stream, "+OK\n");
			} else {
				stream->write_function(stream, "-ERR No such profile\n");
			}

/**********************************************************************************/
		}else if(!strcmp(argv[2], "status")) {
/**********************************************************************************/
			if (profile) {
				megaco_profile_release(profile);
				megaco_profile_status(stream, profile);
			} else {
				stream->write_function(stream, "-ERR No such profile\n");
			}
/**********************************************************************************/
		}else if(!strcmp(argv[2], "xmlstatus")) {
/**********************************************************************************/
			if (profile) {
				megaco_profile_release(profile);
				megaco_profile_xmlstatus(stream, profile);
			} else {
				stream->write_function(stream, "-ERR No such profile\n");
			}
/**********************************************************************************/
		}else if(!strcmp(argv[2], "peerxmlstatus")) {
/**********************************************************************************/
			if (profile) {
				megaco_profile_release(profile);
				megaco_profile_peer_xmlstatus(stream, profile);
			} else {
				stream->write_function(stream, "-ERR No such profile\n");
			}
/**********************************************************************************/
		}else if(!strcmp(argv[2], "send")) {
/**********************************************************************************/
			if (profile) {
				switch(argc)
				{
					case 7:
						{
							/* mg profile <profile-name> send sc <term-id> <method> <reason>*/
							if(zstr(argv[3]) || zstr(argv[4]) || zstr(argv[5]) || zstr(argv[6])){
								goto usage;
							}

							if(!zstr(argv[7]) && !strcasecmp(argv[7],"wild")){
								wild = 0x01;
							}

							printf("Input to Send Service Change command : "
									"Profile Name[%s], term-id[%s] method[%s] reason[%s] \n",
									profile->name, argv[4], argv[5], argv[6]);

							megaco_profile_release(profile);
							mg_send_service_change(profile->idx, argv[4], atoi(argv[5]), atoi(argv[6]),wild);

							break;
						}
					case 6:
						{
							/* mg profile <profile-name> send notify <term-id> <digits>*/
							if(zstr(argv[3]) || zstr(argv[4]) || zstr(argv[5])){
								goto usage;
							}

							if(strcasecmp(argv[3],"notify")){
								stream->write_function(stream, "-ERR wrong input \n");
								goto usage;
							}

							printf("Sending DTMF digits[%s] NOTIFY for termination[%s]\n", argv[5], argv[4]);

							megaco_profile_release(profile);
							mg_send_dtmf_notify(profile, argv[4], (char*)argv[5], (int)strlen(argv[5]));

							break;
						}
					case 5:
						{
							if(zstr(argv[3])){
								goto usage;
							}

							/*************************************************************************/
							if(!strcasecmp(argv[3],"ito")){
								/* mg profile <profile-name> send ito notify */

								printf("Sending In-Activity  NOTIFY \n");

								megaco_profile_release(profile);
								mg_send_ito_notify(profile);
								/*************************************************************************/
							}else if(!strcasecmp(argv[3],"cng")){
								/*************************************************************************/
								/* mg profile <profile-name> send cng <term-id> */

								if(zstr(argv[4])){
									goto usage;
								}
								megaco_profile_release(profile);
								mg_send_t38_cng_notify(profile, argv[4]);

								/*************************************************************************/
							}else {
								stream->write_function(stream, "-ERR wrong input \n");
								goto usage;
							}
							/*************************************************************************/

							break;
						}

					default:
						{
							goto usage;
						}
				}
			}else{
				stream->write_function(stream, "-ERR No such profile\n");
			}

/**********************************************************************************/
		}else if (!strcmp(argv[2], "show")) {
/**********************************************************************************/
			/* mg <mg-profile> show activecalls*/
			
			if(zstr(argv[3])) {
				goto usage;
			}

			if(profile){
				if(!strcasecmp(argv[3], "activecalls")){
					/* mg <mg-profile> show activecalls */
					megaco_profile_release(profile);
					handle_show_activecalls_cli_cmd(stream, profile);
			     /*******************************************************************/
				} else if(!strcasecmp(argv[3], "stats")){
			     /*******************************************************************/
					/* mg <mg-profile> show stats */
					megaco_profile_release(profile);
					handle_show_stats(stream, profile);
			     /*******************************************************************/
				}else if(!strcasecmp(argv[3], "alltermstatus")){
			     /*******************************************************************/
					/* mg <mg-profile> show alltermstatus */
					megaco_profile_release(profile);
					handle_all_term_status_cli_cmd(stream, profile);
			     /*******************************************************************/
				}else if(!strcasecmp(argv[3], "termstatus")){
			     /*******************************************************************/
					/* mg <mg-profile> show termstatus <term-id> */
					if (zstr(argv[4])) {
						goto usage;
					}
					megaco_profile_release(profile);
					handle_term_status_cli_cmd(stream, profile, argv[4]);
			     /*******************************************************************/
				}else if(!strcasecmp(argv[3], "spantermstatus")){
			     /*******************************************************************/
					/* mg <mg-profile> show spantermstatus <span-name> */
					if (zstr(argv[4])) {
						goto usage;
					}
					megaco_profile_release(profile);
					handle_span_term_status_cli_cmd(stream, profile, argv[4]);

			     /*******************************************************************/
				}else if(!strcasecmp(argv[3], "stackmem")){
			     /*******************************************************************/
					megaco_profile_release(profile);
					handle_show_stack_mem(stream);
			     /*******************************************************************/
#ifdef LEAK_TEST
				}else if(!strcasecmp(argv[3], "leak-report")){
			     /*******************************************************************/
					megaco_profile_release(profile);
					mgPrntLeakReport();
			     /*******************************************************************/
#endif
				} else {
			     /*******************************************************************/
					stream->write_function(stream, "-ERR No such profile\n");
					goto usage;
				}
			}
/**********************************************************************************/
		}else {
/**********************************************************************************/
			goto usage;
		}
/**********************************************************************************/
	}else if (!strcmp(argv[0], "logging")) {
/**********************************************************************************/
		if (zstr(argv[1])) {
			goto usage;
		}
		/******************************************************************/
		if(!strcasecmp(argv[1], "enable")){
			mg_enable_logging();
		/******************************************************************/
		}else if(!strcasecmp(argv[1], "disable")){
		/******************************************************************/
			mg_disable_logging();
		/******************************************************************/
		} else {
		/******************************************************************/
			goto usage;
		}
/**********************************************************************************/
	
	}else {
/**********************************************************************************/
			goto usage;
	}
/**********************************************************************************/

	goto done;

usage:
    if(profile)
        megaco_profile_release(profile);

	megaco_cli_print_usage(stream);

done:
	switch_safe_free(dup);
	return SWITCH_STATUS_SUCCESS;
}

/******************************************************************************/
void  megaco_cli_print_usage(switch_stream_handle_t *stream)
{

	stream->write_function(stream, "Usage: Profile Specific\n");
	stream->write_function(stream, "mg profile <profile-name> start \n");
	stream->write_function(stream, "mg profile <profile-name> stop \n");
	//stream->write_function(stream, "mg profile <profile-name> status \n");
	//stream->write_function(stream, "mg profile <profile-name> xmlstatus \n");
	stream->write_function(stream, "mg profile <profile-name> peerxmlstatus \n");
	//stream->write_function(stream, "mg profile <profile-name> send sc <term-id> <method> <reason> \n");
	//stream->write_function(stream, "mg profile <profile-name> send notify <term-id> <digits> \n");
	//stream->write_function(stream, "mg profile <profile-name> send ito notify \n");
	//stream->write_function(stream, "mg profile <profile-name> send cng <term-id> \n");
	stream->write_function(stream, "mg profile <profile-name> show activecalls  \n");
	stream->write_function(stream, "mg profile <profile-name> show spantermstatus <span_name> \n");
	stream->write_function(stream, "mg profile <profile-name> show termstatus <term-id> \n");
	stream->write_function(stream, "mg profile <profile-name> show alltermstatus \n");
	stream->write_function(stream, "mg profile <profile-name> show stackmem  \n");
	stream->write_function(stream, "mg profile <profile-name> show stats  \n");

	stream->write_function(stream, "Usage: Logging \n");
	stream->write_function(stream, "mg logging enable \n");
	stream->write_function(stream, "mg logging disable \n");

}

/******************************************************************************/
switch_status_t megaco_profile_peer_xmlstatus(switch_stream_handle_t *stream, megaco_profile_t* mg_cfg)
{
	int idx   = 0x00;
	int peerIdx   = 0x00;
	int len   = 0x00;
	MgMngmt   cfm;
	char*     xmlhdr = "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>";
	char 	  prntBuf[3048];
	int i = 0x00;
	char *asciiAddr;
	CmInetIpAddr ip;
	mg_peer_profile_t*  mg_peer = NULL;

	switch_assert(mg_cfg);

	memset((U8 *)&cfm, 0, sizeof(cfm));
	memset((char *)&prntBuf, 0, sizeof(prntBuf));

	idx = mg_cfg->idx;

	len = len + sprintf(&prntBuf[0] + len,"%s\n",xmlhdr);

	len = len + sprintf(&prntBuf[0] + len,"<mg_peers>\n");

	for(peerIdx =0; peerIdx < mg_cfg->total_peers; peerIdx++){

		mg_peer = megaco_peer_profile_locate(mg_cfg->peer_list[peerIdx]);

		if(!mg_peer){
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR," No MG peer configuration found for peername[%s] against profilename[%s]\n",mg_cfg->peer_list[peerIdx],mg_cfg->name);
			return SWITCH_STATUS_FALSE;
		}

		len = len + sprintf(&prntBuf[0] + len,"<mg_peer>\n");
		len = len + sprintf(&prntBuf[0] + len,"<name>%s</name>\n",mg_peer->name);

		/* send request to MEGACO Trillium stack to get peer information*/
		sng_mgco_mg_get_status(STGCPENT, &cfm, mg_cfg, mg_peer);

		ip = ntohl(cfm.t.ssta.s.mgPeerSta.peerAddrTbl.netAddr[i].u.ipv4NetAddr);
		cmInetNtoa(ip, &asciiAddr);
		len = len + sprintf(prntBuf+len, "<ipv4_address>%s</ipv4_address>\n",asciiAddr); 

		len = len + sprintf(prntBuf+len, "<peer_state>%s</peer_state>\n",PRNT_MG_PEER_STATE(cfm.t.ssta.s.mgPeerSta.peerState)); 

		len = len + sprintf(&prntBuf[0] + len,"</mg_peer>\n");
	}

	len = len + sprintf(&prntBuf[0] + len,"</mg_peers>\n");

	stream->write_function(stream, "\n%s\n",&prntBuf[0]);

	return SWITCH_STATUS_SUCCESS;
}

/******************************************************************************/

switch_status_t megaco_profile_xmlstatus(switch_stream_handle_t *stream, megaco_profile_t* mg_cfg)
{
	int idx   = 0x00;
	int peerIdx   = 0x00;
	int len   = 0x00;
	MgMngmt   cfm;
	char*     xmlhdr = "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>";
	char 	  prntBuf[3048];
	int i = 0x00;
	char *asciiAddr;
	CmInetIpAddr ip;
	mg_peer_profile_t*  mg_peer = NULL;

	switch_assert(mg_cfg);

	memset((U8 *)&cfm, 0, sizeof(cfm));
	memset((char *)&prntBuf, 0, sizeof(prntBuf));

	 

	idx = mg_cfg->idx;

	len = len + sprintf(&prntBuf[0] + len,"%s\n",xmlhdr);

	len = len + sprintf(&prntBuf[0] + len,"<mg_profile>\n");
	len = len + sprintf(&prntBuf[0] + len,"<name>%s</name>\n",mg_cfg->name);
/****************************************************************************************************************/
/* Print Peer Information ***************************************************************************************/

	len = len + sprintf(&prntBuf[0] + len,"<mg_peers>\n");

	for(peerIdx =0; peerIdx < mg_cfg->total_peers; peerIdx++){

		mg_peer = megaco_peer_profile_locate(mg_cfg->peer_list[peerIdx]);

		if(!mg_peer){
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR," No MG peer configuration found for peername[%s] against profilename[%s]\n",mg_cfg->peer_list[peerIdx],mg_cfg->name);
			return SWITCH_STATUS_FALSE;
		}

		len = len + sprintf(&prntBuf[0] + len,"<mg_peer name=%s>\n",mg_peer->name);

		/* send request to MEGACO Trillium stack to get peer information*/
		sng_mgco_mg_get_status(STGCPENT, &cfm, mg_cfg, mg_peer);

		get_peer_xml_buffer(&prntBuf[0] + len, &cfm.t.ssta.s.mgPeerSta);

		len = len + sprintf(&prntBuf[0] + len,"</mg_peer>\n");
	}
	len = len + sprintf(&prntBuf[0] + len,"</mg_peers>\n");

	
/****************************************************************************************************************/
/* Print MG SAP Information ***************************************************************************************/

	len = len + sprintf(&prntBuf[0] + len,"<mg_sap>\n");

	/* MG SAP Information */
	sng_mgco_mg_get_status(STSSAP, &cfm, mg_cfg, mg_peer);

	len = len + sprintf(prntBuf+len, "<state> %s </state>\n", PRNT_SAP_STATE((int)(cfm.t.ssta.s.mgSSAPSta.state)));
	len = len + sprintf(prntBuf+len, "<num_of_peer> %u </num_of_peer>\n", (unsigned int)(cfm.t.ssta.s.mgSSAPSta.numAssocPeer));
	len = len + sprintf(prntBuf+len, "<num_of_listeners> %u </num_of_listeners>\n", (unsigned int)(cfm.t.ssta.s.mgSSAPSta.numServers));
	len = len + sprintf(&prntBuf[0] + len,"<mg_sap_peers>\n");
	for (i = 0; i < cfm.t.ssta.s.mgSSAPSta.numAssocPeer; i++)
	{
		len = len + sprintf(&prntBuf[0] + len,"<mg_sap_peer>\n");
		if(cfm.t.ssta.s.mgSSAPSta.peerInfo[i].dname.namePres.pres == PRSNT_NODEF)
		{
			len = len + sprintf(prntBuf+len, "<domain_name> %s </domain_name>\n", (char *)(cfm.t.ssta.s.mgSSAPSta.peerInfo[i].dname.name));
		}
		switch(cfm.t.ssta.s.mgSSAPSta.peerInfo[i].dname.netAddr.type)
		{
			case CM_NETADDR_IPV4:
				{
					ip = ntohl(cfm.t.ssta.s.mgSSAPSta.peerInfo[i].dname.netAddr.u.ipv4NetAddr);
					cmInetNtoa(ip, &asciiAddr);
					len = len + sprintf(prntBuf+len, "<ipv4_address>%s</ipv4_address>\n",asciiAddr); 
					break;
				}
			default:
				len = len + sprintf(prntBuf+len, "<ip_address>invalid type </ip_address>\n");
				break;
		}

#ifdef GCP_MGCO
		if (PRSNT_NODEF == cfm.t.ssta.s.mgSSAPSta.peerInfo[i].mid.pres)
		{
			len = len + sprintf(prntBuf+len, "<peer_mid> %s </peer_mid>\n", (char *)(cfm.t.ssta.s.mgSSAPSta.peerInfo[i].mid.val));
		}
#endif /* GCP_MGCO */
		len = len + sprintf(&prntBuf[0] + len,"</mg_sap_peer>\n");
	}
	len = len + sprintf(&prntBuf[0] + len,"</mg_sap_peers>\n");

	len = len + sprintf(&prntBuf[0] + len,"</mg_sap>\n");

/****************************************************************************************************************/
/* Print MG Transport SAP Information ***************************************************************************************/

	len = len + sprintf(&prntBuf[0] + len,"<mg_transport_sap>\n");
	/* MG Transport SAP Information */
	sng_mgco_mg_get_status(STTSAP, &cfm, mg_cfg, mg_peer);
	len = len + sprintf(&prntBuf[0] + len,"<state> %s </state>\n", PRNT_SAP_STATE(cfm.t.ssta.s.mgTSAPSta.state));
	len = len + sprintf(&prntBuf[0] + len,"<num_of_listeners> %u </num_of_listeners>\n", (unsigned int)(cfm.t.ssta.s.mgTSAPSta.numServers)); 
	len = len + sprintf(&prntBuf[0] + len,"</mg_transport_sap>\n");

/****************************************************************************************************************/
/* Print MG Transport Server Information ***************************************************************************************/

	if(sng_mgco_mg_get_status(STSERVER, &cfm, mg_cfg, mg_peer)){
		len = len + sprintf(&prntBuf[0] + len,"<mg_transport_server> no established server found </mg_transport_server>\n");
	}
	else {
		len = len + sprintf(&prntBuf[0] + len,"<mg_transport_server>\n");
		len = len + sprintf(&prntBuf[0] + len,"<state> %s </state>\n", PRNT_SAP_STATE(cfm.t.ssta.s.mgTptSrvSta.state));
		len = len + sprintf(prntBuf+len, "<transport_address>");

		switch (cfm.t.ssta.s.mgTptSrvSta.tptAddr.type)
		{
			case CM_TPTADDR_NOTPRSNT:
				{
					len = len + sprintf(prntBuf+len, "none");
					break;
				}
			case CM_TPTADDR_IPV4:
				{
					ip = ntohl(cfm.t.ssta.s.mgTptSrvSta.tptAddr.u.ipv4TptAddr.address);
					cmInetNtoa(ip, &asciiAddr);
					len = len + sprintf(prntBuf+len, "IPv4 IP address #%s, port %u",asciiAddr,
							(unsigned int)(cfm.t.ssta.s.mgTptSrvSta.tptAddr.u.ipv4TptAddr.port));

					break;
				}
			default:
				len = len + sprintf(prntBuf+len, "unknown");
				break;
		}
		len = len + sprintf(prntBuf+len, "</transport_address>\n");
		len = len + sprintf(&prntBuf[0] + len,"</mg_transport_server>\n");
	}

/****************************************************************************************************************/
	len = len + sprintf(&prntBuf[0] + len,"</mg_profile>\n");

	stream->write_function(stream, "\n%s\n",&prntBuf[0]);

	return SWITCH_STATUS_SUCCESS;
}

/****************************************************************************************************************/
switch_status_t megaco_profile_status(switch_stream_handle_t *stream, megaco_profile_t* mg_cfg)
{
	int idx   = 0x00;
	int len   = 0x00;
	MgMngmt   cfm;
	char 	  prntBuf[1024];
	mg_peer_profile_t*  mg_peer = NULL;

	switch_assert(mg_cfg);

	memset((U8 *)&cfm, 0, sizeof(cfm));
	memset((char *)&prntBuf, 0, sizeof(prntBuf));

	mg_peer = megaco_peer_profile_locate(mg_cfg->peer_list[0]);

	if(!mg_peer){
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR," No MG peer configuration found for peername[%s] against profilename[%s]\n",mg_cfg->peer_list[0],mg_cfg->name);
		return SWITCH_STATUS_FALSE;
	}

	idx = mg_cfg->idx;

	/*stream->write_function(stream, "Collecting MG Profile[%s] status... \n",profilename);*/

	/* Fetch data from Trillium MEGACO Stack 	*
	 * SystemId - Software version information 	*
	 * SSAP     - MG SAP Information 		*
	 * TSAP     - MG Transport SAP Information 	*
	 * Peer     - MG Peer Information 		*
	 * TPT-Server - MG Transport Server information *
	 */ 

#if 0
	/* get System ID */
	sng_mgco_mg_get_status(STSID, &cfm, idx);
	stream->write_function(stream, "***********************************************\n");
	stream->write_function(stream, "**** TRILLIUM MEGACO Software Information *****\n");
	stream->write_function(stream, "Version 	  = %d \n", cfm.t.ssta.s.systemId.mVer);
	stream->write_function(stream, "Version Revision  = %d \n", cfm.t.ssta.s.systemId.mRev);
	stream->write_function(stream, "Branch  Version   = %d \n", cfm.t.ssta.s.systemId.bVer);
	stream->write_function(stream, "Branch  Revision  = %d \n", cfm.t.ssta.s.systemId.bRev);
	stream->write_function(stream, "Part    Number    = %d \n", cfm.t.ssta.s.systemId.ptNmb);
	stream->write_function(stream, "***********************************************\n");
#endif

	/* MG Peer Information */
	sng_mgco_mg_get_status(STGCPENT, &cfm, mg_cfg, mg_peer);
	smmgPrntPeerSta(&cfm.t.ssta.s.mgPeerSta);

	/* MG Peer Information */
	sng_mgco_mg_get_status(STSSAP, &cfm, mg_cfg, mg_peer);
	smmgPrntSsapSta(&cfm.t.ssta.s.mgSSAPSta);

	/* MG Transport SAP Information */
	sng_mgco_mg_get_status(STTSAP, &cfm, mg_cfg, mg_peer);
	len = len + sprintf(prntBuf+len,"***********************************************\n"); 
	len = len + sprintf(prntBuf+len,"**********MG TRANSPORT SAP Information**********\n");
	len = len + sprintf(prntBuf+len,"TSAP status:\n");
	len = len + sprintf(prntBuf+len,"state = %d, number of listeners %u\n",
			(int)(cfm.t.ssta.s.mgTSAPSta.state),
			(unsigned int)(cfm.t.ssta.s.mgTSAPSta.numServers));
	len = len + sprintf(prntBuf+len,"***********************************************\n"); 
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO,"%s\n",prntBuf); 

	/* MG Transport Server Information */
	sng_mgco_mg_get_status(STSERVER, &cfm, mg_cfg, mg_peer);
	smmgPrntSrvSta(&cfm.t.ssta.s.mgTptSrvSta);

	return SWITCH_STATUS_SUCCESS;
}

/******************************************************************************/
void get_peer_xml_buffer(char* prntBuf, MgPeerSta* cfm)
{
	int len = 0x00;
	int i = 0x00;
	char *asciiAddr;
	CmInetIpAddr ip;

	if(PRSNT_NODEF == cfm->namePres.pres)
	{
		len = len + sprintf(prntBuf+len, "<domain_name> %s </domain_name>\n", (char *)(cfm->name));
	}
	else
	{
		len = len + sprintf(prntBuf+len, "<domain_name> Not Present </domain_name>\n");
	}

	/* 
	 * Print all IP addresses in the IP addr table
	 */
	for(i=0; i<cfm->peerAddrTbl.count; i++)
	{
		switch (cfm->peerAddrTbl.netAddr[i].type)
		{
			case CM_NETADDR_IPV4:
				{
					ip = ntohl(cfm->peerAddrTbl.netAddr[i].u.ipv4NetAddr);
					cmInetNtoa(ip, &asciiAddr);
					len = len + sprintf(prntBuf+len, "<ipv4_address>%s</ipv4_address>\n",asciiAddr); 
					break;
				}
			case CM_NETADDR_IPV6:
				{
					char ipv6_buf[128];
					int len1= 0;
					int j = 0;
					memset(&ipv6_buf[0], 0, sizeof(ipv6_buf));
					len1 = len1 + sprintf(ipv6_buf+len1, "IP V6 address : %2x", (unsigned int)
							(cfm->peerAddrTbl.netAddr[i].u.ipv6NetAddr[0]));

					for (j = 1; j < CM_IPV6ADDR_SIZE; j++)
					{
						len1 = len1 + sprintf(ipv6_buf+len1, ":%2x", (unsigned int)
								(cfm->peerAddrTbl.netAddr[i].u.ipv6NetAddr[j]));
					}
					len1 = len1 + sprintf(ipv6_buf+len1,  "\n");
					len = len + sprintf(prntBuf+len, "<ipv6_address>%s</ipv6_address>\n", ipv6_buf); 
					break;
				}
			default:
				{
					len = len + sprintf(prntBuf+len, "<ip_address> Invalid address type[%d]</ip_address>\n", cfm->peerAddrTbl.netAddr[i].type);
					break;
				}
		}
	} /* End of for */

	len = len + sprintf(prntBuf+len,"<num_of_pending_out_txn> %lu </num_of_pending_out_txn>\n",(unsigned long)(cfm->numPendOgTxn));
	len = len + sprintf(prntBuf+len,"<num_of_pending_in_txn> %lu </num_of_pending_in_txn>\n",(unsigned long)(cfm->numPendIcTxn));
	len = len + sprintf(prntBuf+len,"<round_trip_estimate_time> %lu </round_trip_estimate_time>\n",(unsigned long)(cfm->rttEstimate));

	switch(cfm->protocol)
	{
		case LMG_PROTOCOL_MGCP:
			len = len + sprintf(prntBuf+len,"<protocol_type> MGCP </protocol_type>\n");
			break;

		case LMG_PROTOCOL_MGCO:
			len = len + sprintf(prntBuf+len,"<protocol_type> MEGACO </protocol_type>\n");
			break;

		case LMG_PROTOCOL_NONE:
			len = len + sprintf(prntBuf+len,"<protocol_type> MGCP/MEGACO </protocol_type>\n");
			break;

		default:
			len = len + sprintf(prntBuf+len,"<protocol_type> invalid </protocol_type>\n");
			break;
	}

	switch(cfm->transportType)
	{
		case LMG_TPT_UDP:
			len = len + sprintf(prntBuf+len, "<transport_type>UDP</transport_type>\n");
			break;

		case LMG_TPT_TCP:
			len = len + sprintf(prntBuf+len, "<transport_type>TCP</transport_type>\n");
			break;

		case LMG_TPT_NONE:
			len = len + sprintf(prntBuf+len, "<transport_type>UDP/TCP</transport_type>\n");
			break;

		default:
			len = len + sprintf(prntBuf+len, "<transport_type>invalid</transport_type>\n");
			break;
	}
#ifdef GCP_MGCO
	switch(cfm->encodingScheme)
	{
		case LMG_ENCODE_BIN:
			len = len + sprintf(prntBuf+len, "<encoding_type>BINARY</encoding_type>\n");
			break;

		case LMG_ENCODE_TXT:
			len = len + sprintf(prntBuf+len, "<encoding_type>TEXT</encoding_type>\n");
			break;

		case LMG_ENCODE_NONE:
			len = len + sprintf(prntBuf+len, "<encoding_type>TEXT/BINARY</encoding_type>\n");
			break;

		default:
			len = len + sprintf(prntBuf+len, "<encoding_type>invalid</encoding_type>\n");
			break;
	}

	if(LMG_VER_PROF_MGCO_H248_1_0 == cfm->version){
		len = len + sprintf(prntBuf+len,  "<version>1.0</version> \n");
	} else if(LMG_VER_PROF_MGCO_H248_2_0 == cfm->version){
		len = len + sprintf(prntBuf+len,  "<version>2.0</version> \n");
	}else if(LMG_VER_PROF_MGCO_H248_3_0 == cfm->version){
		len = len + sprintf(prntBuf+len,  "<version>3.0</version> \n");
	} else{
		len = len + sprintf(prntBuf+len,  "<version>invalid</version> \n");
	}
#endif

}
/******************************************************************************/
switch_status_t handle_all_term_status_cli_cmd(switch_stream_handle_t *stream, megaco_profile_t* mg_profile)
{
	void                *val = NULL;
	switch_hash_index_t *hi = NULL;
	mg_termination_t *term = NULL;
	const void *var;

	if(!mg_profile){
		stream->write_function(stream, "-ERR NULL profile\n");
		return SWITCH_STATUS_FALSE;
	}

	stream->write_function(stream, " Termination Name"); 
	stream->write_function(stream, "\t Termination State"); 
	stream->write_function(stream, "\t Call State"); 
	stream->write_function(stream, "\t Termination Type"); 
	stream->write_function(stream, "\t Span-Id "); 
	stream->write_function(stream, "\t Channel-Id "); 

	switch_thread_rwlock_rdlock(mg_profile->terminations_rwlock);

	for (hi = switch_hash_first(NULL, mg_profile->terminations); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, &var, NULL, &val);
		term = (mg_termination_t *) val;
		if(!term) continue;

		stream->write_function(stream, "\n");

		stream->write_function(stream, " %s",(NULL != term->name)?term->name:"NULL");
		if(MG_TERM_RTP == term->type){
			stream->write_function(stream, "\t\t\t IN-SERVICE");
		}else{
			stream->write_function(stream, "\t\t\t %s",
					(switch_test_flag(term, MG_IN_SERVICE))?"IN-SERVICE":"OUT-OF-SERVICE");
		}

		stream->write_function(stream, "\t\t%s",(NULL != term->uuid)?"IN-CALL ":"IDLE  ");
		stream->write_function(stream, "\t\t %s",(MG_TERM_RTP == term->type)?"MG_TERM_RTP":"MG_TERM_TDM");

		if(MG_TERM_TDM == term->type){
			stream->write_function(stream, "\t\t %s",
					(NULL != term->u.tdm.span_name)?term->u.tdm.span_name:"NULL");
			stream->write_function(stream, "\t\t %d",term->u.tdm.channel);
		}else{
			stream->write_function(stream, "\t\t -");
			stream->write_function(stream, "\t\t -");
		}
		stream->write_function(stream, "\n");
	}

	switch_thread_rwlock_unlock(mg_profile->terminations_rwlock);

	return SWITCH_STATUS_SUCCESS;
}
/******************************************************************************/

switch_status_t handle_span_term_status_cli_cmd(switch_stream_handle_t *stream, megaco_profile_t* mg_profile, char* span_name)
{
	void                *val = NULL;
	switch_hash_index_t *hi = NULL;
	mg_termination_t *term = NULL;
	const void *var;
	int found = 0x00;
	int  first = 0x01;

	if(!mg_profile || !span_name){
		stream->write_function(stream, "-ERR NULL profile or NULL span_name\n");
		return SWITCH_STATUS_FALSE;
	}

		switch_thread_rwlock_rdlock(mg_profile->terminations_rwlock);

	for (hi = switch_hash_first(NULL, mg_profile->terminations); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, &var, NULL, &val);
		term = (mg_termination_t *) val;
		if(!term) continue;

		if(MG_TERM_RTP == term->type) continue;

		if(!term->u.tdm.span_name) continue;

		if(strcasecmp(span_name,term->u.tdm.span_name)) continue;

		found = 0x01;
		
		if(first){	
			stream->write_function(stream, " Termination Name"); 
			stream->write_function(stream, "\t Termination State"); 
			stream->write_function(stream, "\t Call State"); 
			stream->write_function(stream, "\t Termination Type"); 
			stream->write_function(stream, "\t Span-Id "); 
			stream->write_function(stream, "\t Channel-Id "); 
			first = 0x00;
		}

		stream->write_function(stream, "\n");

		stream->write_function(stream, " %s",(NULL != term->name)?term->name:"NULL");
		if(MG_TERM_RTP == term->type){
			stream->write_function(stream, "\t\t\t IN-SERVICE");
		}else{
			stream->write_function(stream, "\t\t\t %s",
					(switch_test_flag(term, MG_IN_SERVICE))?"IN-SERVICE":"OUT-OF-SERVICE");
		}

		stream->write_function(stream, "\t\t%s",(NULL != term->uuid)?"IN-CALL ":"IDLE  ");
		stream->write_function(stream, "\t\t %s",(MG_TERM_RTP == term->type)?"MG_TERM_RTP":"MG_TERM_TDM");

		if(MG_TERM_TDM == term->type){
			stream->write_function(stream, "\t\t %s",
					(NULL != term->u.tdm.span_name)?term->u.tdm.span_name:"NULL");
			stream->write_function(stream, "\t\t %d",term->u.tdm.channel);
		}else{
			stream->write_function(stream, "\t\t -");
			stream->write_function(stream, "\t\t -");
		}
		stream->write_function(stream, "\n");
	}

	if(!found){
		stream->write_function(stream, "No span[%s] configured\n",span_name);
	}

	switch_thread_rwlock_unlock(mg_profile->terminations_rwlock);

	return SWITCH_STATUS_SUCCESS;
}
/******************************************************************************/

switch_status_t handle_term_status_cli_cmd(switch_stream_handle_t *stream, megaco_profile_t* mg_profile, char* term_id)
{
	mg_termination_t* term = NULL;

	if(!mg_profile || !term_id){
		stream->write_function(stream, "-ERR NULL profile/term pointer \n");
		return SWITCH_STATUS_FALSE;
	}

	term = 	megaco_find_termination(mg_profile, term_id);

	if(!term || !term->profile){
		stream->write_function(stream, "-ERR No such termination\n");
		return SWITCH_STATUS_FALSE;
	}

	stream->write_function(stream, "Associated MG Profile Name [%s] \n",term->profile->name);
	stream->write_function(stream, "MEGACO Termination Name[%s] \n",(NULL != term->name)?term->name:"NULL");
	stream->write_function(stream, "MEGACO Termination Type[%s] \n",(MG_TERM_RTP == term->type)?"MG_TERM_RTP":"MG_TERM_TDM");
	stream->write_function(stream, "Termination UUID[%s] \n",(NULL != term->uuid)?term->uuid:"Term Not Activated");
	if(term->context){
		stream->write_function(stream, "Associated Context-Id[%d] \n",term->context->context_id);
		if(term->context->terminations[0] && term->context->terminations[1]){
			if(term == term->context->terminations[0]){
				stream->write_function(stream, "Associated Termination Name[%s] \n",
						(NULL != term->context->terminations[1]->name)?term->context->terminations[1]->name:"NULL");
			}else {
				stream->write_function(stream, "Associated Termination Name[%s] \n",
						(NULL != term->context->terminations[0]->name)?term->context->terminations[0]->name:"NULL");
			}
		}
	}
	

	if(MG_TERM_RTP == term->type){
		stream->write_function(stream, "RTP Termination ID [%d] \n",term->u.rtp.term_id);
		stream->write_function(stream, "RTP MEDIA Type [%s] \n",
				( MGM_IMAGE == term->u.rtp.media_type)?"MGM_IMAGE":"MGM_AUDIO");
		stream->write_function(stream, "RTP Termination Local Address[%s] \n",
				(NULL != term->u.rtp.local_addr)?term->u.rtp.local_addr:"NULL");
		stream->write_function(stream, "RTP Termination Local Port[%d] \n",term->u.rtp.local_port);
		stream->write_function(stream, "RTP Termination Remote Address[%s] \n",
				(NULL != term->u.rtp.remote_addr)?term->u.rtp.remote_addr:"NULL");
		stream->write_function(stream, "RTP Termination Remote Port[%d] \n",term->u.rtp.remote_port);
		stream->write_function(stream, "RTP Termination PTIME [%d] \n",term->u.rtp.ptime);
		stream->write_function(stream, "RTP Termination PT [%d] \n",term->u.rtp.pt);
		stream->write_function(stream, "RTP Termination rfc2833_pt [%d] \n",term->u.rtp.rfc2833_pt);
		stream->write_function(stream, "RTP Termination Sampling Rate [%d] \n",term->u.rtp.rate);
		stream->write_function(stream, "RTP Termination Codec [%s] \n",
				(NULL != term->u.rtp.codec)?term->u.rtp.codec:"NULL");
	}else{
		stream->write_function(stream, "TDM Termination Service-State [%s] \n",
					(switch_test_flag(term, MG_IN_SERVICE))?"IN-SERVICE":"OUT-OF-SERVICE");
		stream->write_function(stream, "TDM Termination channel [%d] \n",term->u.tdm.channel);
		stream->write_function(stream, "TDM Termination span name [%s] \n",
				(NULL != term->u.tdm.span_name)?term->u.tdm.span_name:"NULL");
	}

	return SWITCH_STATUS_SUCCESS;
}
/******************************************************************************/
switch_status_t handle_show_activecalls_cli_cmd(switch_stream_handle_t *stream, megaco_profile_t* mg_profile)
{
	void                	*val = NULL;
	switch_hash_index_t 	*hi = NULL;
	mg_termination_t 	*term = NULL;
	const void 		*var;
	int 			 found = 0x00;

	if(!mg_profile || !mg_profile->terminations){
		stream->write_function(stream, "-ERR NULL profile/term pointer \n");
		return SWITCH_STATUS_FALSE;
	}

	stream->write_function(stream, "\n ------- Active Calls Terminations ------- \n"); 

	switch_thread_rwlock_rdlock(mg_profile->terminations_rwlock);

	for (hi = switch_hash_first(NULL, mg_profile->terminations); hi; hi = switch_hash_next(hi)) {
		switch_hash_this(hi, &var, NULL, &val);
		term = (mg_termination_t *) val;
		if(!term) continue;
		if(NULL == term->uuid) continue;

		found = 0x01;
		stream->write_function(stream, "\n ********************************* \n"); 
		stream->write_function(stream, "MEGACO Termination Name[%s] \n",(NULL != term->name)?term->name:"NULL");
		stream->write_function(stream, "MEGACO Termination Type[%s] \n",(MG_TERM_RTP == term->type)?"MG_TERM_RTP":"MG_TERM_TDM");
		stream->write_function(stream, "Termination UUID[%s] \n",(NULL != term->uuid)?term->uuid:"Term Not Activated");
		if(MG_TERM_RTP == term->type){
			stream->write_function(stream, "RTP Termination ID [%d] \n",term->u.rtp.term_id);
		}else{
			stream->write_function(stream, "TDM Termination channel [%d] \n",term->u.tdm.channel);
			stream->write_function(stream, "TDM Termination span name [%s] \n",
					(NULL != term->u.tdm.span_name)?term->u.tdm.span_name:"NULL");
		}
		stream->write_function(stream, "\n ********************************* \n"); 
	}

	switch_thread_rwlock_unlock(mg_profile->terminations_rwlock);


	if(!found)
		stream->write_function(stream, "\n ------- NO Active Calls FOUND ------- \n"); 


	return SWITCH_STATUS_SUCCESS;
}

/******************************************************************************/
switch_status_t handle_show_stats(switch_stream_handle_t *stream, megaco_profile_t* mg_profile)
{
	if(!mg_profile || !mg_profile->mg_stats){
		stream->write_function(stream, "-ERR NULL profile/term pointer \n");
		return SWITCH_STATUS_FALSE;
	}

	stream->write_function(stream, "Total Number of Physical ADD received  = %d \n", mg_profile->mg_stats->total_num_of_phy_add_recvd); 
	stream->write_function(stream, "Total Number of RTP      ADD received  = %d \n", mg_profile->mg_stats->total_num_of_rtp_add_recvd); 
	stream->write_function(stream, "Total Number of SUB received  = %d \n", mg_profile->mg_stats->total_num_of_sub_recvd); 
	stream->write_function(stream, "Total Number of CALL received  = %d \n", mg_profile->mg_stats->total_num_of_call_recvd); 
	stream->write_function(stream, "Total Number of T38-FAX CALL received  = %d \n", mg_profile->mg_stats->total_num_of_fax_call_recvd++); 
	stream->write_function(stream, "Total Number of IN-Service Service change sent  = %d \n", 
			mg_profile->mg_stats->total_num_of_term_in_service_change_sent); 
	stream->write_function(stream, "Total Number of Out-Of-Service Service change sent  = %d \n", 
			mg_profile->mg_stats->total_num_of_term_oos_service_change_sent); 
	stream->write_function(stream, "Total Number of ADD failed  = %d \n", mg_profile->mg_stats->total_num_of_add_failed); 
	stream->write_function(stream, "Total Number of Term Already in context Error  = %d \n", 
			mg_profile->mg_stats->total_num_of_term_already_in_ctxt_error); 
	stream->write_function(stream, "Total Number of choose context failed Error  = %d \n", 
			mg_profile->mg_stats->total_num_of_choose_ctxt_failed_error); 
	stream->write_function(stream, "Total Number of choose term failed Error  = %d \n", 
			mg_profile->mg_stats->total_num_of_choose_term_failed_error); 
	stream->write_function(stream, "Total Number of find term failed Error  = %d \n", 
			mg_profile->mg_stats->total_num_of_find_term_failed_error); 
	stream->write_function(stream, "Total Number of get context failed Error  = %d \n", 
			mg_profile->mg_stats->total_num_of_get_ctxt_failed_error); 
	stream->write_function(stream, "Total Number of un-supported codec error  = %d \n", 
			mg_profile->mg_stats->total_num_of_un_supported_codec_error); 
	stream->write_function(stream, "Total Number of Term addition to context failed error  = %d \n", 
			mg_profile->mg_stats->total_num_of_add_term_failed_error); 
	stream->write_function(stream, "Total Number of Term activation failed error  = %d \n", 
			mg_profile->mg_stats->total_num_of_term_activation_failed_error); 
	stream->write_function(stream, "Total Number of Term not found in context  error  = %d \n", 
			mg_profile->mg_stats->total_num_of_no_term_ctxt_error); 
	stream->write_function(stream, "Total Number of Term not in service error  = %d \n", 
			mg_profile->mg_stats->total_num_of_term_not_in_service_error); 
	stream->write_function(stream, "Total Number of unknown context error  = %d \n", 
			mg_profile->mg_stats->total_num_of_unknown_ctxt_error); 



	return SWITCH_STATUS_SUCCESS;
}
/******************************************************************************/
switch_status_t handle_show_stack_mem(switch_stream_handle_t *stream)
{
	U32 availMem = 0;
	char buffer[4098];

	memset(buffer,0,sizeof(buffer));

	SGetMemInfoBuffer(S_REG, &availMem, buffer);

	stream->write_function(stream, "%s",buffer);
			
	return SWITCH_STATUS_SUCCESS;
}
/******************************************************************************/


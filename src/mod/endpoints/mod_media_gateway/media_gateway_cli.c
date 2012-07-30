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
void get_peer_xml_buffer(char* prntBuf, MgPeerSta* cfm);

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
			printf("count = %d \n",argc);

            if (profile) {

                switch(argc)
                {
                    case 7:
                        {
                            /* mg profile <profile-name> send sc <term-id> <method> <reason>*/
                            printf("ARGC = 7 \n");
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
	stream->write_function(stream, "-ERR Usage: \n""\t"MEGACO_CLI_SYNTAX" \n \t"MEGACO_FUNCTION_SYNTAX"\n \t" MEGACO_LOGGING_CLI_SYNTAX "\n");

done:
	switch_safe_free(dup);
	return SWITCH_STATUS_SUCCESS;
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


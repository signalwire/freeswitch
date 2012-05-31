/*
* Copyright (c) 2012, Sangoma Technologies
* Kapil Gupta <kgupta@sangoma.com>
* All rights reserved.
* 
* <Insert license here>
*/

#include "sng_megaco/sng_ss7.h"

#ifndef _MEGACO_CFG_H_
#define _MEGACO_CFG_H_

#define MAX_MID_LEN    30
#define MAX_DOMAIN_LEN 30
#define MAX_NAME_LEN    25
#define MAX_MG_PROFILES 5

typedef struct sng_mg_peer{
  char          name[MAX_NAME_LEN];	     /* Peer Name as defined in config file */
  uint16_t 	id;			     /* Peer ID as defined in config file */
  uint8_t  	ipaddr[MAX_DOMAIN_LEN];      /* Peer IP  */
  uint16_t 	port;                        /*Peer Port */
  uint8_t       mid[MAX_MID_LEN];  	     /* Peer H.248 MID */
  uint16_t      encoding_type;               /* Encoding TEXT/Binary */
}sng_mg_peer_t;

typedef struct sng_mg_peers{
    uint16_t  total_peer;   		     /* Total number of MGC Peer */
    sng_mg_peer_t peers[MG_MAX_PEERS+1];
}sng_mg_peers_t;

typedef struct sng_mg_transport_profile{
  	char          name[MAX_NAME_LEN];	     /* Peer Name as defined in config file */
        uint32_t      id;      		     	     /* map to tsap id */
        uint16_t      transport_type;                /* transport type */
}sng_mg_transport_profile_t;


typedef enum{
        SNG_MG_TPT_NONE,
        SNG_MG_TPT_UDP,
        SNG_MG_TPT_TCP,
        SNG_MG_TPT_SCTP,
        SNG_MG_TPT_MTP3
}sng_mg_transport_types_e;

typedef enum{
        SNG_MG_NONE,
        SNG_MG_MGCP,
        SNG_MG_MEGACO,
}sng_mg_protocol_types_e;

#define PRNT_PROTOCOL_TYPE(_val)\
	((_val == SNG_MG_MGCP)?"SNG_MG_MGCP":\
	 (_val == SNG_MG_MEGACO)?"SNG_MG_MEGACO":\
	 "SNG_MG_NONE")

typedef enum{
        SNG_MG_ENCODING_NONE,
        SNG_MG_ENCODING_BINARY,
        SNG_MG_ENCODING_TEXT,
}sng_mg_encoding_types_e;

#define PRNT_ENCODING_TYPE(_val)\
	((_val == SNG_MG_ENCODING_TEXT)?"SNG_MG_ENCODING_TEXT":\
	 (_val == SNG_MG_ENCODING_BINARY)?"SNG_MG_ENCODING_BINARY":\
	 "SNG_MG_ENCODING_NONE")


/* each profile is corresponds to each MG Instance */
typedef struct sng_mg_cfg{
  	char          		name[MAX_NAME_LEN];	     /* MG(Virtual MG) Name as defined in config file */
        uint32_t                id;                	     /* Id - map to MG SAP ID */
        uint8_t                 mid[MAX_MID_LEN];  	     /* MG H.248 MID */
        uint8_t                 my_domain[MAX_DOMAIN_LEN];   /* local domain name */
        uint8_t                 my_ipaddr[MAX_DOMAIN_LEN];   /* local domain name */
        uint32_t                port;              	     /* port */
	uint16_t 		protocol_version;            /* Protocol supported version */
        uint16_t                peer_id;                     /* MGC Peer ID */
        uint16_t                transport_prof_id;           /* Transport profile id ..this also will be the spId for MG SAP*/
        uint16_t                protocol_type;    	     /* MEGACO/MGCP */
}sng_mg_cfg_t;


typedef struct sng_mg_gbl_cfg{
	sng_mg_cfg_t                    mgCfg[MAX_MG_PROFILES + 1];
	sng_mg_transport_profile_t 	mgTptProf[MG_MAX_PEERS+1];	/* transport profile */
	sng_mg_peers_t 			mgPeer;
}sng_mg_gbl_cfg_t;


extern switch_status_t sng_parse_mg_peer_profile(switch_xml_t mg_peer_profile);
extern switch_status_t sng_parse_mg_tpt_profile(switch_xml_t mg_tpt_profile);
extern switch_status_t sng_parse_mg_profile(switch_xml_t mg_interface);


void handle_sng_log(uint8_t level, char *fmt, ...);
void handle_mgco_sta_ind(Pst *pst, SuId suId, MgMgtSta* msg);
void handle_mgco_txn_sta_ind(Pst *pst, SuId suId, MgMgcoInd* msg);
void handle_mgco_cmd_ind(Pst *pst, SuId suId, MgMgcoCommand* msg);
void handle_mgco_cntrl_cfm(Pst *pst, SuId suId, MgMgtCntrl* cntrl, Reason reason); 
void handle_mgco_txn_ind(Pst *pst, SuId suId, MgMgcoMsg* msg);
void handle_mgco_audit_cfm(Pst *pst, SuId suId, MgMgtAudit* audit, Reason reason); 
void handle_mg_alarm(Pst *pst, MgMngmt *sta);
void handle_tucl_alarm(Pst *pst, HiMngmt *sta);


switch_status_t sng_mgco_init(sng_isup_event_interface_t* event);
switch_status_t sng_mgco_cfg(const char* profilename);
switch_status_t sng_mgco_start(const char* profilename);
switch_status_t sng_mgco_stop(const char* profilename);
switch_status_t sng_mgco_stack_shutdown(void);

/*****************************************************************************************************/

#define GET_MG_CFG_IDX(_profilename, _idx){\
	for(idx=0; idx < MAX_MG_PROFILES; idx++){\
		/* id zero is not acceptable */\
		if(megaco_globals.g_mg_cfg.mgCfg[idx].id){\
			if (strcmp(megaco_globals.g_mg_cfg.mgCfg[idx].name, profilename)) {\
				continue;\
			} else{\
				break;\
			}\
		}\
	}\
}

#define GET_TPT_ID(_id) 		megaco_globals.g_mg_cfg.mgTptProf[megaco_globals.g_mg_cfg.mgCfg[_id].transport_prof_id].id
#define GET_MU_SAP_ID(_id) 		megaco_globals.g_mg_cfg.mgCfg[_id].id

#define GET_TPT_TYPE(_id)		megaco_globals.g_mg_cfg.mgTptProf[megaco_globals.g_mg_cfg.mgCfg[_id].transport_prof_id].transport_type

#define GET_ENCODING_TYPE(_id) 		megaco_globals.g_mg_cfg.mgPeer.peers[megaco_globals.g_mg_cfg.mgCfg[_id].peer_id].encoding_type

#endif /* _MEGACO_CFG_H_ */

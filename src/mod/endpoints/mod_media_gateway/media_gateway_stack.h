/*
* Copyright (c) 2012, Sangoma Technologies
* Kapil Gupta <kgupta@sangoma.com>
* All rights reserved.
* 
* <Insert license here>
*/

#include "mod_media_gateway.h"

#ifndef _MEGACO_STACK_H_
#define _MEGACO_STACK_H_

#define MAX_MID_LEN    30

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


void handle_sng_log(uint8_t level, char *fmt, ...);
void handle_mgco_sta_ind(Pst *pst, SuId suId, MgMgtSta* msg);
void handle_mgco_txn_sta_ind(Pst *pst, SuId suId, MgMgcoInd* msg);
void handle_mgco_cmd_ind(Pst *pst, SuId suId, MgMgcoCommand* msg);
void handle_mgco_cntrl_cfm(Pst *pst, SuId suId, MgMgtCntrl* cntrl, Reason reason); 
void handle_mgco_txn_ind(Pst *pst, SuId suId, MgMgcoMsg* msg);
void handle_mgco_audit_cfm(Pst *pst, SuId suId, MgMgtAudit* audit, Reason reason); 
void handle_mg_alarm(Pst *pst, MgMngmt *sta);
void handle_tucl_alarm(Pst *pst, HiMngmt *sta);


switch_status_t sng_mgco_cfg(megaco_profile_t* profile);
switch_status_t sng_mgco_init(sng_mg_event_interface_t* event);
switch_status_t sng_mgco_stack_shutdown(void);
int sng_mgco_mg_get_status(int elemId, MgMngmt* cfm, megaco_profile_t* mg_cfg, mg_peer_profile_t* mg_peer);

#endif /* _MEGACO_STACK_H_ */

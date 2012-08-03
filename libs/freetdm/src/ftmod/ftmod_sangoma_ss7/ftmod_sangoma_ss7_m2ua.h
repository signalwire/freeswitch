/*
 * Copyright (c) 2012, Kapil Gupta <kgupta@sangoma.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *
 * Contributors: 
 *
 */
/******************************************************************************/
#ifndef __FTMOD_SNG_SS7_M2UA_H__
#define __FTMOD_SNG_SS7_M2UA_H__
/******************************************************************************/
#include "private/ftdm_core.h"

#define MAX_NAME_LEN			25

typedef struct sng_nif_cfg{
	char 	    name[MAX_NAME_LEN];
	uint32_t    flags;
	uint32_t    id;
	uint32_t    m2uaLnkNmb;
	uint32_t    mtp2LnkNmb;
}sng_nif_cfg_t;

typedef enum{
	SNG_M2UA_NODE_TYPE_SGP  = 1,      /* type SG */
	SNG_M2UA_NODE_TYPE_ASP  = 2,      /* type ASP */
}sng_m2ua_node_types_e;

typedef struct sng_m2ua_cfg{
	char 	    		 name[MAX_NAME_LEN];
	uint32_t    		 flags;
	uint32_t    		 id;		/* ID */
	uint32_t    		 iid;		/* ID */
	uint8_t    		 nodeType; 	/*Node Type SG/ASP */
	uint8_t    		 end_point_opened; /* flag to check is end-point already opened */	
	uint16_t    		 clusterId;	/* idx to m2ua_cluster profile */
}sng_m2ua_cfg_t;

typedef struct sng_m2ua_peer_cfg{
	char 	    		 name[MAX_NAME_LEN];
	uint32_t    		 flags;
	uint32_t    		 id;		/* ID */
	uint8_t    		 aspIdFlag;	/* Flag used to indicate whether include the ASP ID in the ASP UP message */
	uint16_t    		 selfAspId;	/* Self ASP ID. ASP identifier for this ASP node if the aspIdFlag is TRUE. */
	uint32_t    		 numDestAddr;	/* Number of destination address defined */
	uint16_t    		 sctpId;	/* idx to sctp profile */
	uint16_t 		 port;
	uint32_t    	         destAddrList[SCT_MAX_NET_ADDRS+1]; /* Destination adddress list */
	uint16_t    		 locOutStrms;	/*Number of outgoing streams supported by this association*/ 
	int 			 init_sctp_assoc; /* flag to tell if we need to initiate SCTP association */
}sng_m2ua_peer_cfg_t;

typedef enum{
	SNG_M2UA_LOAD_SHARE_ALGO_RR    = 0x1,     /* Round Robin Mode*/
	SNG_M2UA_LOAD_SHARE_ALGO_LS    = 0x2,     /* Link Specified */
	SNG_M2UA_LOAD_SHARE_ALGO_CS    = 0x3,     /* Customer Specified */
}sng_m2ua_load_share_algo_types_e;


/* Possible values of Traffic mode */
typedef enum{
	SNG_M2UA_TRF_MODE_OVERRIDE   = 0x1,     /* Override Mode */
	SNG_M2UA_TRF_MODE_LOADSHARE  = 0x2,     /* Loadshare Mode */
	SNG_M2UA_TRF_MODE_BROADCAST  = 0x3,     /* Broadcast Mode */
	SNG_M2UA_TRF_MODE_ANY        = 0x0,     /* ANY       Mode */
}sng_m2ua_traffic_mode_types_e;

typedef struct sng_m2ua_cluster_cfg{
	char 	    		 name[MAX_NAME_LEN];
	uint32_t    		 flags;
	uint32_t    		 id;		/* ID */
	uint32_t    		 sct_sap_id;   /* Internal - sct_sap_id */
	uint8_t    		 trfMode;	/* Traffic mode. This parameter defines the mode in which this m2ua cluster is supposed to work */ 
	uint8_t    		 loadShareAlgo;	/* This parameter defines the M2UA load share algorithm which is used to distribute the traffic */ 
	uint16_t    		 numOfPeers;	/* idx to m2ua_peer profile */
	uint16_t    		 peerIdLst[MW_MAX_NUM_OF_PEER];	/* idx to m2ua_peer profile */
}sng_m2ua_cluster_cfg_t;

typedef struct sng_m2ua_gbl_cfg{
	sng_nif_cfg_t 		nif[MW_MAX_NUM_OF_INTF+1];
	sng_m2ua_cfg_t 		m2ua[MW_MAX_NUM_OF_INTF+1];
	sng_m2ua_peer_cfg_t 	m2ua_peer[MW_MAX_NUM_OF_PEER+1];
	sng_m2ua_cluster_cfg_t 	m2ua_clus[MW_MAX_NUM_OF_CLUSTER+1];
}sng_m2ua_gbl_cfg_t;

/* m2ua xml parsing APIs */
int ftmod_ss7_parse_nif_interfaces(ftdm_conf_node_t *nif_interfaces);
int ftmod_ss7_parse_m2ua_interfaces(ftdm_conf_node_t *m2ua_interfaces);
int ftmod_ss7_parse_m2ua_peer_interfaces(ftdm_conf_node_t *m2ua_peer_interfaces);
int ftmod_ss7_parse_m2ua_clust_interfaces(ftdm_conf_node_t *m2ua_clust_interfaces);
int ftmod_ss7_parse_sctp_links(ftdm_conf_node_t *node);
uint32_t iptoul(const char *ip);

int ftmod_ss7_m2ua_start(void);
void ftmod_ss7_m2ua_free(void);

ftdm_status_t ftmod_ss7_m2ua_cfg(void);
ftdm_status_t ftmod_ss7_m2ua_init(void);

int ftmod_sctp_ssta_req(int elemt, int id, SbMgmt* cfm);
int ftmod_m2ua_ssta_req(int elemt, int id, MwMgmt* cfm);
int ftmod_nif_ssta_req(int elemt, int id, NwMgmt* cfm);
void ftmod_ss7_enable_m2ua_sg_logging(void);
void ftmod_ss7_disable_m2ua_sg_logging(void);


#endif /*__FTMOD_SNG_SS7_M2UA_H__*/

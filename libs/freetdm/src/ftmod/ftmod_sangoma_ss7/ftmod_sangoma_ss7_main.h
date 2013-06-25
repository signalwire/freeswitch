/*
 * Copyright (c) 2009, Konrad Hammel <konrad@sangoma.com>
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
 * James Zhang <jzhang@sangoma.com>
 *
 */
/******************************************************************************/
#ifndef __FTMOD_SNG_SS7_H__
#define __FTMOD_SNG_SS7_H__
/******************************************************************************/

/* INCLUDE ********************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <unistd.h>
#include <ctype.h>

#include "private/ftdm_core.h"

#include "sng_ss7/sng_ss7.h"
#include "ftmod_sangoma_ss7_m2ua.h"

/******************************************************************************/

/* DEFINES ********************************************************************/
#define MAX_NAME_LEN			25

#define MAX_CIC_LENGTH			5
#define MAX_CIC_MAP_LENGTH		1000 
#define MAX_SCTP_LINK 			100

#define SNGSS7_EVENT_QUEUE_SIZE	100
#define SNGSS7_PEER_CHANS_QUEUE_SIZE 100
#define SNGSS7_CHAN_EVENT_QUEUE_SIZE 100

#define MAX_SIZEOF_SUBADDR_IE	24	/* as per Q931 4.5.9 */

#define SNGSS7_SWITCHTYPE_ANSI(switchtype)	(switchtype == LSI_SW_ANS88) || \
											(switchtype == LSI_SW_ANS92) || \
											(switchtype == LSI_SW_ANS95)

#define sngss7_flush_queue(queue) \
			do { \
					void *__queue_data = NULL; \
					while ((__queue_data = ftdm_queue_dequeue(queue))) { \
						ftdm_safe_free(__queue_data); \
					} \
			} while (0)

typedef struct ftdm2trillium {
	uint8_t ftdm_val;
	uint8_t trillium_val;
} ftdm2trillium_t;

typedef enum {
	SNGSS7_CON_IND_EVENT = 0,
	SNGSS7_CON_CFM_EVENT,
	SNGSS7_CON_STA_EVENT,
	SNGSS7_REL_IND_EVENT,
	SNGSS7_REL_CFM_EVENT,
	SNGSS7_DAT_IND_EVENT,
	SNGSS7_FAC_IND_EVENT,
	SNGSS7_FAC_CFM_EVENT,
	SNGSS7_UMSG_IND_EVENT,
	SNGSS7_STA_IND_EVENT,
	SNGSS7_SUSP_IND_EVENT,
	SNGSS7_RESM_IND_EVENT,
	SNGSS7_SSP_STA_CFM_EVENT,
	SNGSS7_INVALID_EVENT,
} sng_event_type_t;
#define SNG_EVENT_TYPE_STRINGS "CON_IND", "CON_CFM", "CON_STA", "REL_IND", "REL_CFM", "DAT_IND", "FAC_IND", \
	                       "FAC_CFM", "UMSG_IND", "STA_IND", "SUSP_IND", "RESM_IND", "SSP_STA_CFM", "INVALID"
FTDM_STR2ENUM_P(ftdm_str2sngss7_event, ftdm_sngss7_event2str, sng_event_type_t)

typedef enum {
	SNG_BIT_A	= (1 << 0),
	SNG_BIT_B	= (1 << 1),
	SNG_BIT_C	= (1 << 2),
	SNG_BIT_D	= (1 << 3),
	SNG_BIT_E	= (1 << 4),
	SNG_BIT_F	= (1 << 5),
	SNG_BIT_G	= (1 << 6),
	SNG_BIT_H	= (1 << 7)
} sng_bit_enums_t;

typedef enum {
	SNG_CKT_VOICE = 0,
	SNG_CKT_SIG,
	SNG_CKT_HOLE
} sng_ckt_type_t;

typedef enum {
	SNG_RTE_UP	= 0,
	SNG_RTE_DN
} sng_route_direction_t;

typedef enum {
	SNGSS7_LPA_FOR_COT		= (1 << 0),	/* send LPA when COT arrives */
	SNGSS7_ACM_OBCI_BITA	= (1 << 10)	/* in-band indication */
} sng_intf_options_t;

typedef enum {
	SNG_CALLED			= 1,
	SNG_CALLING			= 2
} sng_addr_type_t;

typedef enum {
	SNG_GEN_CFG_STATUS_INIT    = 0,
	SNG_GEN_CFG_STATUS_PENDING = 1,
	SNG_GEN_CFG_STATUS_DONE    = 2
} nsg_gen_cfg_type_t;

typedef struct sng_mtp2_error_type {
	int	init;
	char sng_type[MAX_NAME_LEN];
	uint32_t tril_type;
} sng_mtp2_error_type_t;

typedef struct sng_link_type {
	int init;
	char sng_type[MAX_NAME_LEN];
	uint32_t tril_mtp2_type;
	uint32_t tril_mtp3_type;
} sng_link_type_t;

typedef struct sng_switch_type {
	int init;
	char sng_type[MAX_NAME_LEN];
	uint32_t tril_mtp3_type;
	uint32_t tril_isup_type;
} sng_switch_type_t;

typedef struct sng_ssf_type {
	int init;
	char sng_type[MAX_NAME_LEN];
	uint32_t tril_type;
} sng_ssf_type_t;

typedef struct sng_cic_cntrl_type {
	int init;
	char sng_type[MAX_NAME_LEN];
	uint32_t tril_type;
} sng_cic_cntrl_type_t;

typedef struct sng_mtp1_link {
	char		name[MAX_NAME_LEN];
	uint32_t	flags;
	uint32_t	id;
	uint32_t	span;
	uint32_t	chan;
} sng_mtp1_link_t;

typedef struct sng_mtp2_link {
	char		name[MAX_NAME_LEN];
	uint32_t	flags;
	uint32_t	id;
	uint32_t	lssuLength;
	uint32_t	errorType;
	uint32_t	linkType;
	uint32_t	mtp1Id;
	uint32_t	mtp1ProcId;
	uint32_t	t1;
	uint32_t	t2;
	uint32_t	t3;
	uint32_t	t4n;
	uint32_t	t4e;
	uint32_t	t5;
	uint32_t	t6;
	uint32_t	t7;
} sng_mtp2_link_t;

/* defining glare handling methods: 
	SNGSS7_GLARE_PC: 
		higher PointCode controls even number CIC
		lower PointCode controls odd number CIC
	SNGSS7_GLARE_DOWN:
		always give control to the other side
	SNGSS7_GLARE_CONTROL:
		always trying to control
*/
typedef enum {
	SNGSS7_GLARE_PC = 0,
	SNGSS7_GLARE_DOWN,
	SNGSS7_GLARE_CONTROL
} sng_glare_resolution;

typedef struct sng_mtp3_link {
	char		name[MAX_NAME_LEN];
	uint32_t	flags;
	uint32_t	id;
	uint32_t	priority;
	uint32_t	linkType;
	uint32_t	switchType;
	uint32_t	apc;
	uint32_t	spc;
	uint32_t	ssf;
	uint32_t	slc;
	uint32_t	linkSetId;
	uint32_t	mtp2Id;
	uint32_t	mtp2ProcId;
	uint32_t	t1;
	uint32_t	t2;
	uint32_t	t3;
	uint32_t	t4;
	uint32_t	t5;
	uint32_t	t6;
	uint32_t	t7;
	uint32_t	t8;
	uint32_t	t9;
	uint32_t	t10;
	uint32_t	t11;
	uint32_t	t12;
	uint32_t	t13;
	uint32_t	t14;
	uint32_t	t15;
	uint32_t	t16;
	uint32_t	t17;
	uint32_t	t18;
	uint32_t	t19;
	uint32_t	t20;
	uint32_t	t21;
	uint32_t	t22;
	uint32_t	t23;
	uint32_t	t24;
	uint32_t	t25;
	uint32_t	t27;
	uint32_t	t28;
	uint32_t	t29;
	uint32_t	t30;
	uint32_t	t31;
	uint32_t	t32;
	uint32_t	t33;
	uint32_t	t34;
	uint32_t	t35;
	uint32_t	t36;
	uint32_t	t37;
	uint32_t	tcraft;
	uint32_t	tflc;
	uint32_t	tbnd;
} sng_mtp3_link_t;

typedef struct sng_link_set {
	char			name[MAX_NAME_LEN];
	uint32_t		flags;
	uint32_t		id;
	uint32_t		apc;
	uint32_t		linkType;
	uint32_t		switchType;
	uint32_t		ssf;
	uint32_t 		minActive;
	uint32_t		numLinks;
	uint32_t		links[16];
} sng_link_set_t;

typedef struct sng_link_set_list {
	uint32_t					lsId;
	struct sng_link_set_list	*next;
} sng_link_set_list_t;

typedef struct sng_route {
	char			name[MAX_NAME_LEN];
	uint32_t		flags;
	uint32_t		id;
	uint32_t		dpc;
	uint32_t		cmbLinkSetId;
	struct sng_link_set_list	lnkSets;
	uint32_t		linkType;
	uint32_t		switchType;
	uint32_t		ssf;
	uint32_t		nwId;
	uint32_t		isSTP;
	uint32_t		dir;
	uint32_t		t6;
	uint32_t		t8;
	uint32_t		t10;
	uint32_t		t11;
	uint32_t		t15;
	uint32_t		t16;
	uint32_t		t18;
	uint32_t		t19;
	uint32_t		t21;
	uint32_t		t25;
	uint32_t		t26;
} sng_route_t;

typedef struct sng_isup_intf {
	char			name[MAX_NAME_LEN];
	uint32_t		options;
	uint32_t		flags;
	uint32_t		id;
	uint32_t		spc;
	uint32_t		dpc;
	uint32_t		switchType;
	uint32_t		nwId;
	uint32_t		mtpRouteId;
	uint32_t		ssf;
	uint32_t		isap;
	uint16_t		t4;
	uint32_t		t11;
	uint32_t		t18;
	uint32_t		t19;
	uint32_t		t20;
	uint32_t		t21;
	uint32_t		t22;
	uint32_t		t23;
	uint32_t		t24;
	uint32_t		t25;
	uint32_t		t26;
	uint32_t		t28;
	uint32_t		t29;
	uint32_t		t30;
	uint32_t		t32;
	uint32_t		t37;
	uint32_t		t38;
	uint32_t		t39;
	uint32_t		tfgr;
	uint32_t		tpause;
	uint32_t		tstaenq;
} sng_isup_inf_t;

typedef struct sng_isup_ckt {
	uint32_t		options;
	uint32_t		flags;
	uint32_t		procId;
	uint32_t		id;
	uint32_t		ccSpanId;
	uint32_t		span;
	uint32_t		chan;
	uint32_t		type;	/* VOICE/SIG/HOLE */
	uint32_t		cic;
	uint32_t		infId;
	uint32_t		typeCntrl;
	uint32_t		ssf;
	uint32_t		switchType;
	
	uint32_t		clg_nadi;
	uint32_t		cld_nadi;
	uint8_t			rdnis_nadi;
	uint32_t		loc_nadi;

	/* Generic Number defaults */
	uint8_t			gn_nmbqual;			/* Number Qualifier */
	uint8_t			gn_nadi;			/* Nature of Address indicator */
	uint8_t 		gn_screen_ind;		/* Screening Indicator */
	uint8_t			gn_pres_ind;		/* Presentation Indicator */
	uint8_t			gn_npi;				/* Numbering Plan Indicator */
	uint8_t			gn_num_inc_ind;		/* Number Incomplete Indicator */
	/* END - Generic Number defaults */
			
	uint32_t		min_digits;
	uint32_t		transparent_iam_max_size;
	uint8_t			transparent_iam;
	uint8_t			cpg_on_progress_media;
	uint8_t			cpg_on_progress;
	uint8_t			itx_auto_reply;
	void			*obj;
	uint16_t		t3;
	uint32_t		t10;
	uint16_t		t12;
	uint16_t		t13;
	uint16_t		t14;
	uint16_t		t15;
	uint16_t		t16;
	uint16_t		t17;
	uint32_t		t35;
	uint32_t		t39;
	uint16_t		tval;
} sng_isup_ckt_t;

typedef struct sng_nsap {
	uint32_t		flags;
	uint32_t		id;
	uint32_t		suId;
	uint32_t		spId;
	uint32_t		nwId;
	uint32_t		linkType;
	uint32_t		switchType;
	uint32_t		ssf;
} sng_nsap_t;

typedef struct sng_isap {
	uint32_t		id;
	uint32_t		suId;
	uint32_t		spId;
	uint32_t		switchType;
	uint32_t		ssf;
	uint32_t		flags;
	uint32_t		t1;
	uint32_t		t2;
	uint32_t		t5;
	uint32_t		t6;
	uint32_t		t7;
	uint32_t		t8;
	uint32_t		t9;
	uint32_t		t27;
	uint32_t		t31;
	uint32_t		t33;
	uint32_t		t34;
	uint32_t		t36;
	uint32_t		tccr;
	uint32_t		tccrt;
	uint32_t		tex;
	uint32_t		tcrm;
	uint32_t		tcra;
	uint32_t		tect;
	uint32_t		trelrsp;
	uint32_t		tfnlrelrsp;
} sng_isap_t;

typedef struct sng_relay {
	uint32_t		id;
	char			name[MAX_NAME_LEN];
	uint32_t		flags;
	uint32_t		type;
	uint32_t		port;
	char			hostname[RY_REMHOSTNAME_SIZE];
	uint32_t		procId;
} sng_relay_t;

/**********************************************
sctp structures and data definitions
**********************************************/

typedef struct sng_sctp_gen_cfg {
} sng_sctp_gen_cfg_t;

typedef struct sng_sctp_link {
	char		name[MAX_NAME_LEN];
	uint32_t	flags;
	uint32_t	id;
	uint32_t	port;
	uint32_t	numSrcAddr;
	uint32_t	srcAddrList[SCT_MAX_NET_ADDRS+1];	
} sng_sctp_link_t;

typedef struct sng_sctp_cfg {
	sng_sctp_gen_cfg_t	genCfg;
	sng_sctp_link_t		linkCfg[MAX_SCTP_LINK+1];
} sng_sctp_cfg_t;



typedef struct sng_ss7_cfg {
	uint32_t			spc;
	uint32_t			procId;
	char				license[MAX_SNGSS7_PATH];
	char				signature[MAX_SNGSS7_PATH];
	uint32_t			transparent_iam_max_size;
	uint32_t			flags;
	sng_relay_t			relay[MAX_RELAY_CHANNELS+1];
	sng_mtp1_link_t		mtp1Link[MAX_MTP_LINKS+1];
	sng_mtp2_link_t		mtp2Link[MAX_MTP_LINKS+1];
	sng_mtp3_link_t		mtp3Link[MAX_MTP_LINKS+1];
	sng_link_set_t		mtpLinkSet[MAX_MTP_LINKSETS+1];
	sng_route_t			mtpRoute[MAX_MTP_ROUTES+1];
	sng_isup_inf_t		isupIntf[MAX_ISUP_INFS+1];
	sng_isup_ckt_t		isupCkt[10000]; 	/* KONRAD - only need 2000 ( and 0-1000 aren't used) since other servers are registerd else where */
	sng_nsap_t			nsap[MAX_NSAPS+1];
	sng_isap_t			isap[MAX_ISAPS+1];	
	sng_glare_resolution	glareResolution;
	uint32_t				force_inr;
	sng_m2ua_gbl_cfg_t 	g_m2ua_cfg;
	sng_sctp_cfg_t		sctpCfg;
} sng_ss7_cfg_t;

typedef struct ftdm_sngss7_data {
	sng_ss7_cfg_t		cfg;
	int					gen_config;
	int					function_trace;
	int					function_trace_level;
	int					message_trace;
	int					message_trace_level;
	fio_signal_cb_t		sig_cb;
} ftdm_sngss7_data_t;

typedef enum{
	SNG_SS7_OPR_MODE_NONE,
	SNG_SS7_OPR_MODE_M2UA_SG,
	SNG_SS7_OPR_MODE_ISUP,
}ftdm_sngss7_operating_modes_e;

typedef ftdm_sngss7_operating_modes_e ftdm_sngss7_opr_mode;

typedef struct sngss7_timer_data {
	ftdm_timer_id_t			hb_timer_id;
	int						beat;
	int						counter;
	ftdm_sched_callback_t	callback;
	ftdm_sched_t			*sched;
	void					*sngss7_info;
} sngss7_timer_data_t;

typedef struct sngss7_glare_data {
	uint32_t				spInstId; 
	uint32_t				circuit; 
	SiConEvnt				iam;
} sngss7_glare_data_t;

typedef struct sngss7_group_data {
	uint32_t				circuit;
	uint32_t				range;
	uint8_t					status[255];
	uint8_t					type;
	uint8_t					cause;
} sngss7_group_data_t;

typedef struct sngss7_chan_data {
	ftdm_channel_t			*ftdmchan;
	sng_isup_ckt_t			*circuit;
	uint32_t				base_chan;
	uint32_t				suInstId;
	uint32_t				spInstId;
	uint32_t				spId;
	uint8_t					globalFlg;
	uint32_t				ckt_flags;
	uint32_t				blk_flags;
	ftdm_hash_t*			variables;		/* send on next sigevent */
	ftdm_size_t				raw_data_len;
	void					*raw_data;		/* send on next sigevent */
	sngss7_glare_data_t		glare;
	sngss7_timer_data_t		t35;
	sngss7_timer_data_t		t10;
	sngss7_timer_data_t		t39;
	sngss7_group_data_t		rx_grs;
	sngss7_group_data_t		rx_gra;
	sngss7_group_data_t		tx_grs;
	sngss7_group_data_t		ucic;
	ftdm_queue_t 			*event_queue;
	struct sngss7_chan_data         *peer_data;
	uint8_t peer_event_transfer_cnt;
} sngss7_chan_data_t;

#define SNGSS7_RX_GRS_PENDING (1 << 0)
#define SNGSS7_UCIC_PENDING (1 << 1)
#define SNGSS7_RX_GRA_PENDING (1 << 2)
typedef struct sngss7_span_data {
	ftdm_sched_t			*sched;
	uint32_t                        flags;
	sngss7_group_data_t		rx_cgb;
	sngss7_group_data_t		tx_cgb;
	sngss7_group_data_t		rx_cgu;
	sngss7_group_data_t		tx_cgu;
	ftdm_queue_t 			*event_queue;
} sngss7_span_data_t;

typedef struct sngss7_event_data
{
	uint32_t		event_id;
	uint32_t		spId;
	uint32_t		suId;
	uint32_t		spInstId;
	uint32_t		suInstId;
	uint32_t		circuit;
	uint8_t			globalFlg;
	uint8_t			evntType;
	union
	{
		SiConEvnt	siConEvnt;
		SiCnStEvnt	siCnStEvnt;
		SiRelEvnt	siRelEvnt;
		SiInfoEvnt	siInfoEvnt;
		SiFacEvnt	siFacEvnt;
		SiStaEvnt	siStaEvnt;
		SiSuspEvnt	siSuspEvnt;
		SiResmEvnt	siResmEvnt;
	} event;
} sngss7_event_data_t;


typedef enum {
	FLAG_RESET_RX			= (1 << 0),
	FLAG_RESET_TX			= (1 << 1),
	FLAG_RESET_SENT			= (1 << 2),
	FLAG_RESET_TX_RSP		= (1 << 3),
	FLAG_GRP_RESET_RX		= (1 << 4),
	FLAG_GRP_RESET_RX_DN	= (1 << 5),
	FLAG_GRP_RESET_RX_CMPLT	= (1 << 6),
	FLAG_GRP_RESET_BASE		= (1 << 7),
	FLAG_GRP_RESET_TX		= (1 << 8),
	FLAG_GRP_RESET_SENT		= (1 << 9),
	FLAG_GRP_RESET_TX_RSP	= (1 << 10),
	FLAG_REMOTE_REL			= (1 << 11),
	FLAG_LOCAL_REL			= (1 << 12),
	FLAG_GLARE				= (1 << 13),
	FLAG_INFID_RESUME		= (1 << 14),
	FLAG_INFID_PAUSED		= (1 << 15),
	FLAG_SENT_ACM			= (1 << 16),
	FLAG_SENT_CPG			= (1 << 17),
	FLAG_SUS_RECVD		    = (1 << 18),
	FLAG_T6_CANCELED 		= (1 << 19),
	FLAG_INR_TX			= (1 << 20),
	FLAG_INR_SENT			= (1 << 21),
	FLAG_INR_RX			= (1 << 22),
	FLAG_INR_RX_DN			= (1 << 23),
	FLAG_INF_TX			= (1 << 24),
	FLAG_INF_SENT			= (1 << 25),
	FLAG_INF_RX			= (1 << 26),
	FLAG_INF_RX_DN			= (1 << 27),
	FLAG_FULL_NUMBER			= (1 << 28),
	FLAG_RELAY_DOWN			= (1 << 30),
	FLAG_CKT_RECONFIG		= (1 << 31)
} sng_ckt_flag_t;

#define CKT_FLAGS_STRING \
	"RX_RSC", \
	"TX_RSC", \
	"TX_RSC_REQ_SENT", \
	"TX_RSC_RSP_RECEIVED", \
	"RX_GRS", \
	"RX_GRS_DONE", \
	"RX_GRS_CMPLT", \
	"GRS_BASE", \
	"TX_GRS", \
	"TX_GRS_REQ_SENT", \
	"TX_GRS_RSP_RECEIVED", \
	"REMOTE_REL", \
	"LOCAL_REL", \
	"GLARE", \
	"INF_RESUME", \
	"INF_PAUSED", \
	"TX_ACM_SENT" \
	"TX_INR" \
	"INR_SENT" \
	"RX_INR" \
	"RX_INR_DN" \
	"TX_INF" \
	"INF SENT" \
	"RX_INF" \
	"RX_INF_DN" \
	"RELAY_DOWN", \
	"CKT_RECONFIG"
FTDM_STR2ENUM_P(ftmod_ss7_ckt_state2flag, ftmod_ss7_ckt_flag2str, sng_ckt_flag_t)

/* ckt blocking flags */
typedef enum {
	FLAG_CKT_UCIC_BLOCK		= (1 << 0),
	FLAG_CKT_UCIC_BLOCK_DN	= (1 << 1),
	FLAG_CKT_UCIC_UNBLK		= (1 << 2),
	FLAG_CKT_UCIC_UNBLK_DN	= (1 << 3),
	FLAG_CKT_LC_BLOCK_RX	= (1 << 4),
	FLAG_CKT_LC_BLOCK_RX_DN	= (1 << 5),
	FLAG_CKT_LC_UNBLK_RX	= (1 << 6),
	FLAG_CKT_LC_UNBLK_RX_DN	= (1 << 7),
	FLAG_CKT_MN_BLOCK_RX	= (1 << 8),
	FLAG_CKT_MN_BLOCK_RX_DN	= (1 << 9),
	FLAG_CKT_MN_UNBLK_RX	= (1 << 10),
	FLAG_CKT_MN_UNBLK_RX_DN	= (1 << 11),
	FLAG_CKT_MN_BLOCK_TX	= (1 << 12),
	FLAG_CKT_MN_BLOCK_TX_DN	= (1 << 13),
	FLAG_CKT_MN_UNBLK_TX	= (1 << 14),
	FLAG_CKT_MN_UNBLK_TX_DN	= (1 << 15),
	FLAG_GRP_HW_BLOCK_RX	= (1 << 16),
	FLAG_GRP_HW_BLOCK_RX_DN	= (1 << 17),
	FLAG_GRP_HW_BLOCK_TX	= (1 << 18),
	FLAG_GRP_HW_BLOCK_TX_DN	= (1 << 19),
	FLAG_GRP_MN_BLOCK_RX	= (1 << 20),
	FLAG_GRP_MN_BLOCK_RX_DN	= (1 << 21),
	FLAG_GRP_MN_BLOCK_TX	= (1 << 22),
	FLAG_GRP_MN_BLOCK_TX_DN	= (1 << 23),
	FLAG_GRP_HW_UNBLK_TX	= (1 << 24),
	FLAG_GRP_HW_UNBLK_TX_DN	= (1 << 25),
	FLAG_GRP_MN_UNBLK_TX	= (1 << 26),
	FLAG_GRP_MN_UNBLK_TX_DN	= (1 << 27),
} sng_ckt_block_flag_t;

#define BLK_FLAGS_STRING \
	"UCIC BLK", \
	"UCIC BLK DN", \
	"UCIC UNBLK", \
	"UCIC UNBLK DN", \
	"RX LC BLK", \
	"RX LC BLK DN", \
	"RX LC UNBLK", \
	"RX LC UNBLK DN", \
	"RX CKT BLK", \
	"RX CKT BLK DN", \
	"RX CKT UNBLK", \
	"RX CKT UNBLK DN", \
	"TX CKT BLK", \
	"TX CKT BLK DN", \
	"TX CKT UNBLK", \
	"TX CKT UNBLK DN", \
	"RX GRP HW BLK", \
	"RX GRP HW BLK DN", \
	"TX GRP HW BLK", \
	"TX GRP HW BLK DN", \
	"RX GRP MN BLK", \
	"RX GRP MN BLK DN", \
	"TX GRP MN BLK", \
	"TX GRP MN BLK DN", \
	"TX GRP HW UNBLK", \
	"TX GRP HW UNBLK DN", \
	"TX GRP MN UNBLK", \
	"TX GRP MN UNBLK DN", \
	"RX GRP HW UNBLK", \
	"RX GRP HW UNBLK DN"
FTDM_STR2ENUM_P(ftmod_ss7_blk_state2flag, ftmod_ss7_blk_flag2str, sng_ckt_block_flag_t)

/* valid for every cfg array except circuits */
typedef enum {
	SNGSS7_CONFIGURED		= (1 << 0),
	SNGSS7_ACTIVE			= (1 << 1),
	SNGSS7_RELAY_INIT		= (1 << 3),
	SNGSS7_PAUSED			= (1 << 7) /* for isup interfaces */
} sng_cfg_flag_t;

typedef enum {
	SNGSS7_SM_STARTED	= (1 << 0),

	SNGSS7_RY_PRESENT	= (1 << 2),
	SNGSS7_RY_STARTED	= (1 << 3),
	
	SNGSS7_MTP1_PRESENT	= (1 << 4),
	SNGSS7_MTP1_STARTED	= (1 << 5),

	SNGSS7_MTP2_PRESENT	= (1 << 6),
	SNGSS7_MTP2_STARTED     = (1 << 7),

	SNGSS7_MTP3_PRESENT	= (1 << 8),
	SNGSS7_MTP3_STARTED     = (1 << 9),

	SNGSS7_ISUP_PRESENT	= (1 << 10),
	SNGSS7_ISUP_STARTED     = (1 << 11),

	SNGSS7_CC_PRESENT	= (1 << 12),
	SNGSS7_CC_STARTED       = (1 << 13),

	SNGSS7_TUCL_PRESENT	 = (1 << 14),
	SNGSS7_TUCL_STARTED      = (1 << 15),

	SNGSS7_SCTP_PRESENT	 = (1 << 16),
	SNGSS7_SCTP_STARTED      = (1 << 17),

	SNGSS7_M2UA_PRESENT	 = (1 << 18),
	SNGSS7_M2UA_STARTED      = (1 << 19),
	SNGSS7_M2UA_EP_OPENED    = (1 << 20),
	SNGSS7_M2UA_INIT_ASSOC_DONE    = (1 << 21),

	SNGSS7_NIF_PRESENT	 = (1 << 22),
	SNGSS7_NIF_STARTED       = (1 << 23),

} sng_task_flag_t;
/******************************************************************************/

/* GLOBALS ********************************************************************/
extern ftdm_sngss7_data_t		g_ftdm_sngss7_data;
extern ftdm_sngss7_opr_mode		g_ftdm_operating_mode;
extern sng_ssf_type_t			sng_ssf_type_map[];
extern sng_switch_type_t		sng_switch_type_map[];
extern sng_link_type_t			sng_link_type_map[];
extern sng_mtp2_error_type_t	sng_mtp2_error_type_map[];
extern sng_cic_cntrl_type_t 	sng_cic_cntrl_type_map[];
extern uint32_t					sngss7_id;
extern ftdm_sched_t				*sngss7_sched;
extern int						cmbLinkSetId;
/******************************************************************************/

/* PROTOTYPES *****************************************************************/
/* in ftmod_sangoma_ss7_main.c */
ftdm_status_t ftdm_sangoma_ss7_process_state_change (ftdm_channel_t *ftdmchan);

/* in ftmod_sangoma_ss7_logger.c */
void handle_sng_log(uint8_t level, char *fmt,...);
void handle_sng_mtp1_alarm(Pst *pst, L1Mngmt *sta);
void handle_sng_mtp2_alarm(Pst *pst, SdMngmt *sta);
void handle_sng_mtp3_alarm(Pst *pst, SnMngmt *sta);
void handle_sng_isup_alarm(Pst *pst, SiMngmt *sta);
void handle_sng_cc_alarm(Pst *pst, CcMngmt *sta);
void handle_sng_relay_alarm(Pst *pst, RyMngmt *sta);
void handle_sng_m2ua_alarm(Pst *pst, MwMgmt *sta);
void handle_sng_nif_alarm(Pst *pst, NwMgmt *sta);
void handle_sng_tucl_alarm(Pst *pst, HiMngmt *sta);
void handle_sng_sctp_alarm(Pst *pst, SbMgmt *sta);

/* in ftmod_sangoma_ss7_relay.c */
ftdm_status_t handle_relay_connect(RyMngmt *sta);
ftdm_status_t handle_relay_disconnect_on_down(RyMngmt *sta);
ftdm_status_t handle_relay_disconnect_on_error(RyMngmt *sta);

/* in ftmod_sangoma_ss7_cfg.c */
int ft_to_sngss7_cfg_all(void);
int ftmod_ss7_mtp1_gen_config(void);
int ftmod_ss7_mtp2_gen_config(void);
int ftmod_ss7_mtp3_gen_config(void);
int ftmod_ss7_isup_gen_config(void);
int ftmod_ss7_cc_gen_config(void);
int ftmod_ss7_mtp1_psap_config(int id);
int ftmod_ss7_mtp2_dlsap_config(int id);
int ftmod_ss7_mtp3_dlsap_config(int id);
int ftmod_ss7_mtp3_nsap_config(int id);
int ftmod_ss7_mtp3_linkset_config(int id);
int ftmod_ss7_mtp3_route_config(int id);
int ftmod_ss7_isup_nsap_config(int id);
int ftmod_ss7_isup_intf_config(int id);
int ftmod_ss7_isup_ckt_config(int id);
int ftmod_ss7_isup_isap_config(int id);
int ftmod_ss7_cc_isap_config(int id);

/* in ftmod_sangoma_ss7_cntrl.c */
int  ft_to_sngss7_activate_all(void);

int ftmod_ss7_inhibit_mtp3link(uint32_t id);
int ftmod_ss7_uninhibit_mtp3link(uint32_t id);
int ftmod_ss7_bind_mtp3link(uint32_t id);
int ftmod_ss7_unbind_mtp3link(uint32_t id);
int ftmod_ss7_activate_mtp3link(uint32_t id);
int ftmod_ss7_deactivate_mtp3link(uint32_t id);
int ftmod_ss7_deactivate2_mtp3link(uint32_t id);
int ftmod_ss7_activate_mtplinkSet(uint32_t id);
int ftmod_ss7_deactivate_mtplinkSet(uint32_t id);
int ftmod_ss7_deactivate2_mtplinkSet(uint32_t id);
int ftmod_ss7_lpo_mtp3link(uint32_t id);
int ftmod_ss7_lpr_mtp3link(uint32_t id);

int ftmod_ss7_shutdown_isup(void);
int ftmod_ss7_shutdown_mtp3(void);
int ftmod_ss7_shutdown_mtp2(void);
int ftmod_ss7_shutdown_relay(void);
int ftmod_ss7_disable_relay_channel(uint32_t chanId);

int ftmod_ss7_disable_grp_mtp3Link(uint32_t procId);
int ftmod_ss7_enable_grp_mtp3Link(uint32_t procId);

int ftmod_ss7_disable_grp_mtp2Link(uint32_t procId);

#define ftmod_ss7_block_isup_ckt(x) 		__ftmod_ss7_block_isup_ckt(x,FTDM_TRUE)
#define ftmod_ss7_block_isup_ckt_nowait(x) 	__ftmod_ss7_block_isup_ckt(x,FTDM_FALSE)
int __ftmod_ss7_block_isup_ckt(uint32_t cktId, ftdm_bool_t wait);
int ftmod_ss7_unblock_isup_ckt(uint32_t cktId);


/* in ftmod_sangoma_ss7_sta.c */
int ftmod_ss7_mtp1link_sta(uint32_t id, L1Mngmt *cfm);
int ftmod_ss7_mtp2link_sta(uint32_t id, SdMngmt *cfm);
int ftmod_ss7_mtp3link_sta(uint32_t id, SnMngmt *cfm);
int ftmod_ss7_mtplinkSet_sta(uint32_t id, SnMngmt *cfm);
int ftmod_ss7_isup_intf_sta(uint32_t id, uint8_t *status);
int ftmod_ss7_relay_status(uint32_t id, RyMngmt *cfm);
int ftmod_ss7_isup_ckt_sta(uint32_t id, unsigned char *state);


/* in ftmod_sangoma_ss7_out.c */
void ft_to_sngss7_iam(ftdm_channel_t *ftdmchan);
void ft_to_sngss7_acm(ftdm_channel_t *ftdmchan);
void ft_to_sngss7_cpg (ftdm_channel_t *ftdmchan);
void ft_to_sngss7_anm(ftdm_channel_t *ftdmchan);
void ft_to_sngss7_rel(ftdm_channel_t *ftdmchan);
void ft_to_sngss7_rlc(ftdm_channel_t *ftdmchan);
void ft_to_sngss7_rsc(ftdm_channel_t *ftdmchan);
void ft_to_sngss7_rsca(ftdm_channel_t *ftdmchan);
void ft_to_sngss7_blo(ftdm_channel_t *ftdmchan);
void ft_to_sngss7_bla(ftdm_channel_t *ftdmchan);
void ft_to_sngss7_ubl(ftdm_channel_t *ftdmchan);
void ft_to_sngss7_uba(ftdm_channel_t *ftdmchan);
void ft_to_sngss7_lpa(ftdm_channel_t *ftdmchan);
void ft_to_sngss7_gra(ftdm_channel_t *ftdmchan);
void ft_to_sngss7_grs(ftdm_channel_t *ftdmchan);
void ft_to_sngss7_cgba(ftdm_channel_t * ftdmchan);
void ft_to_sngss7_cgua(ftdm_channel_t * ftdmchan);
void ft_to_sngss7_cgb(ftdm_channel_t * ftdmchan);
void ft_to_sngss7_cgu(ftdm_channel_t * ftdmchan);
void ft_to_sngss7_itx (ftdm_channel_t * ftdmchan);
void ft_to_sngss7_txa (ftdm_channel_t * ftdmchan);
void ft_to_sngss7_inr(ftdm_channel_t * ftdmchan);
void ft_to_sngss7_inf(ftdm_channel_t *ftdmchan, SiCnStEvnt *inr);



/* in ftmod_sangoma_ss7_in.c */
void sngss7_sta_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
void sngss7_con_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiConEvnt *siConEvnt);
void sngss7_con_cfm(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiConEvnt *siConEvnt);
void sngss7_con_sta(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiCnStEvnt *siCnStEvnt, uint8_t evntType);
void sngss7_rel_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiRelEvnt *siRelEvnt);
void sngss7_rel_cfm(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiRelEvnt *siRelEvnt);
void sngss7_dat_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiInfoEvnt *siInfoEvnt);
void sngss7_fac_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t evntType, SiFacEvnt *siFacEvnt);
void sngss7_fac_cfm(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t evntType, SiFacEvnt *siFacEvnt);
void sngss7_sta_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
void sngss7_umsg_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit);
void sngss7_resm_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiResmEvnt *siResmEvnt);
void sngss7_susp_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiSuspEvnt *siSuspEvnt);
void sngss7_ssp_sta_cfm(uint32_t infId);

ftdm_status_t sngss7_bufferzero_iam(SiConEvnt *siConEvnt);
ftdm_status_t sngss7_retrieve_iam(ftdm_channel_t *ftdmchan, SiConEvnt *siConEvnt);
ftdm_status_t sngss7_save_iam(ftdm_channel_t *ftdmchan, SiConEvnt *siConEvnt);

/* in ftmod_sangoma_ss7_handle.c */
ftdm_status_t handle_con_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiConEvnt *siConEvnt);
ftdm_status_t handle_con_sta(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiCnStEvnt *siCnStEvnt, uint8_t evntType);
ftdm_status_t handle_con_cfm(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiConEvnt *siConEvnt);
ftdm_status_t handle_rel_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiRelEvnt *siRelEvnt);
ftdm_status_t handle_rel_cfm(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiRelEvnt *siRelEvnt);
ftdm_status_t handle_dat_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiInfoEvnt *siInfoEvnt);
ftdm_status_t handle_fac_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t evntType, SiFacEvnt *siFacEvnt);
ftdm_status_t handle_fac_cfm(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t evntType, SiFacEvnt *siFacEvnt);
ftdm_status_t handle_umsg_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit);
ftdm_status_t handle_susp_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiSuspEvnt *siSuspEvnt);
ftdm_status_t handle_resm_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiResmEvnt *siResmEvnt);
ftdm_status_t handle_sta_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);

ftdm_status_t handle_reattempt(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
ftdm_status_t handle_pause(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
ftdm_status_t handle_resume(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
ftdm_status_t handle_cot_start(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
ftdm_status_t handle_cot_stop(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
ftdm_status_t handle_cot(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
ftdm_status_t handle_rsc_req(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
ftdm_status_t handle_local_rsc_req(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
ftdm_status_t handle_rsc_rsp(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
ftdm_status_t handle_grs_req(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
ftdm_status_t handle_grs_rsp(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
ftdm_status_t handle_blo_req(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
ftdm_status_t handle_blo_rsp(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
ftdm_status_t handle_ubl_req(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
ftdm_status_t handle_ubl_rsp(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
ftdm_status_t handle_local_blk(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
ftdm_status_t handle_local_ubl(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
ftdm_status_t handle_ucic(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);

/* in ftmod_sangoma_ss7_xml.c */
int ftmod_ss7_parse_xml(ftdm_conf_parameter_t *ftdm_parameters, ftdm_span_t *span);

/* in ftmod_sangoma_ss7_cli.c */
ftdm_status_t ftdm_sngss7_handle_cli_cmd(ftdm_stream_handle_t *stream, const char *data);

/* in ftmod_sangoma_ss7_support.c */
ftdm_status_t copy_cgPtyNum_from_sngss7(ftdm_channel_t *ftdmchan, SiCgPtyNum *cgPtyNum);
ftdm_status_t copy_cgPtyNum_to_sngss7(ftdm_channel_t *ftdmchan, SiCgPtyNum *cgPtyNum);
ftdm_status_t copy_cdPtyNum_from_sngss7(ftdm_channel_t *ftdmchan, SiCdPtyNum *cdPtyNum);
ftdm_status_t copy_cdPtyNum_to_sngss7(ftdm_channel_t *ftdmchan, SiCdPtyNum *cdPtyNum);
ftdm_status_t copy_redirgNum_to_sngss7(ftdm_channel_t *ftdmchan, SiRedirNum *redirgNum);
ftdm_status_t copy_redirgNum_from_sngss7(ftdm_channel_t *ftdmchan, SiRedirNum *redirgNum);
ftdm_status_t copy_redirgInfo_from_sngss7(ftdm_channel_t *ftdmchan, SiRedirInfo *redirInfo);
ftdm_status_t copy_redirgInfo_to_sngss7(ftdm_channel_t *ftdmchan, SiRedirInfo *redirInfo);
ftdm_status_t copy_ocn_to_sngss7(ftdm_channel_t *ftdmchan, SiOrigCdNum *origCdNum);
ftdm_status_t copy_ocn_from_sngss7(ftdm_channel_t *ftdmchan, SiOrigCdNum *origCdNum);

ftdm_status_t copy_access_transport_from_sngss7(ftdm_channel_t *ftdmchan, SiAccTrnspt *accTrnspt);
ftdm_status_t copy_access_transport_to_sngss7(ftdm_channel_t *ftdmchan, SiAccTrnspt *accTrnspt);
ftdm_status_t copy_locPtyNum_to_sngss7(ftdm_channel_t *ftdmchan, SiCgPtyNum *locPtyNum);
ftdm_status_t copy_locPtyNum_from_sngss7(ftdm_channel_t *ftdmchan, SiCgPtyNum *locPtyNum);
ftdm_status_t copy_genNmb_to_sngss7(ftdm_channel_t *ftdmchan, SiGenNum *genNmb);
ftdm_status_t copy_genNmb_from_sngss7(ftdm_channel_t *ftdmchan, SiGenNum *genNmb);
ftdm_status_t copy_cgPtyCat_to_sngss7(ftdm_channel_t *ftdmchan, SiCgPtyCat *cgPtyCat);
ftdm_status_t copy_cgPtyCat_from_sngss7(ftdm_channel_t *ftdmchan, SiCgPtyCat *cgPtyCat);
ftdm_status_t copy_accTrnspt_to_sngss7(ftdm_channel_t *ftdmchan, SiAccTrnspt *accTrnspt);
ftdm_status_t copy_natConInd_to_sngss7(ftdm_channel_t *ftdmchan, SiNatConInd *natConInd);
ftdm_status_t copy_fwdCallInd_hex_from_sngss7(ftdm_channel_t *ftdmchan, SiFwdCallInd *fwdCallInd);
ftdm_status_t copy_fwdCallInd_to_sngss7(ftdm_channel_t *ftdmchan, SiFwdCallInd *fwdCallInd);
ftdm_status_t copy_txMedReq_to_sngss7(ftdm_channel_t *ftdmchan, SiTxMedReq *txMedReq);
ftdm_status_t copy_usrServInfoA_to_sngss7(ftdm_channel_t *ftdmchan, SiUsrServInfo *usrServInfoA);
ftdm_status_t copy_NatureOfConnection_from_sngss7(ftdm_channel_t *ftdmchan, SiNatConInd *natConInd );
ftdm_status_t copy_NatureOfConnection_to_sngss7(ftdm_channel_t *ftdmchan, SiNatConInd *natConInd);

ftdm_status_t copy_tknStr_from_sngss7(TknStr str, char *ftdm, TknU8 oddEven);
ftdm_status_t append_tknStr_from_sngss7(TknStr str, char *ftdm, TknU8 oddEven);
ftdm_status_t copy_tknStr_to_sngss7(char* str, TknStr *tknStr, TknU8 *oddEven);

int check_for_state_change(ftdm_channel_t *ftdmchan);
int check_for_reset(sngss7_chan_data_t *sngss7_info);
ftdm_status_t extract_chan_data(uint32_t circuit, sngss7_chan_data_t **sngss7_info, ftdm_channel_t **ftdmchan);
unsigned long get_unique_id(void);

ftdm_status_t check_if_rx_grs_started(ftdm_span_t *ftdmspan);
ftdm_status_t check_if_rx_grs_processed(ftdm_span_t *ftdmspan);
ftdm_status_t check_if_rx_gra_started(ftdm_span_t *ftdmspan);
ftdm_status_t check_for_res_sus_flag(ftdm_span_t *ftdmspan);

ftdm_status_t process_span_ucic(ftdm_span_t *ftdmspan);

ftdm_status_t clear_rx_grs_flags(sngss7_chan_data_t *sngss7_info);
ftdm_status_t clear_tx_grs_flags(sngss7_chan_data_t *sngss7_info);
ftdm_status_t clear_rx_rsc_flags(sngss7_chan_data_t *sngss7_info);
ftdm_status_t clear_tx_rsc_flags(sngss7_chan_data_t *sngss7_info);
ftdm_status_t clear_rx_grs_data(sngss7_chan_data_t *sngss7_info);
ftdm_status_t clear_rx_gra_data(sngss7_chan_data_t *sngss7_info);
ftdm_status_t clear_tx_grs_data(sngss7_chan_data_t *sngss7_info);

ftdm_status_t encode_subAddrIE_nsap(const char *subAddr, char *subAddrIE, int type);
ftdm_status_t encode_subAddrIE_nat(const char *subAddr, char *subAddrIE, int type);

int find_mtp2_error_type_in_map(const char *err_type);
int find_link_type_in_map(const char *linkType);
int find_switch_type_in_map(const char *switchType);
int find_ssf_type_in_map(const char *ssfType);
int find_cic_cntrl_in_map(const char *cntrlType);

ftdm_status_t check_status_of_all_isup_intf(void);
ftdm_status_t check_for_reconfig_flag(ftdm_span_t *ftdmspan);

void sngss7_send_signal(sngss7_chan_data_t *sngss7_info, ftdm_signal_event_t event_id);
void sngss7_set_sig_status(sngss7_chan_data_t *sngss7_info, ftdm_signaling_status_t status);
ftdm_status_t sngss7_add_var(sngss7_chan_data_t *ss7_info, const char* var, const char* val);
ftdm_status_t sngss7_add_raw_data(sngss7_chan_data_t *sngss7_info, uint8_t* data, ftdm_size_t data_len);

/* in ftmod_sangoma_ss7_timers.c */
void handle_isup_t35(void *userdata);
void handle_isup_t10(void *userdata);
void handle_isup_t39(void *userdata);


/******************************************************************************/

/* MACROS *********************************************************************/
#define SS7_STATE_CHANGE(ftdmchan, new_state) \
if (ftdmchan->state == new_state) { \
	ftdm_set_state(ftdmchan, FTDM_CHANNEL_STATE_IDLE); \
} else { \
	ftdm_set_state(ftdmchan, new_state); \
}

#define SS7_DEBUG(a,...)	ftdm_log(FTDM_LOG_DEBUG,a , ##__VA_ARGS__ );
#define SS7_INFO(a,...)	 ftdm_log(FTDM_LOG_INFO,a , ##__VA_ARGS__ );
#define SS7_WARN(a,...)	 ftdm_log(FTDM_LOG_WARNING,a , ##__VA_ARGS__ );
#define SS7_ERROR(a,...)	ftdm_log(FTDM_LOG_ERROR,a , ##__VA_ARGS__ );
#define SS7_CRITICAL(a,...) ftdm_log(FTDM_LOG_CRIT,a , ##__VA_ARGS__ );

#define SS7_DEBUG_CHAN(fchan, msg, args...)	ftdm_log_chan(fchan, FTDM_LOG_DEBUG, msg , ##args)
#define SS7_INFO_CHAN(fchan, msg, args...)	ftdm_log_chan(fchan, FTDM_LOG_INFO, msg , ##args)
#define SS7_WARN_CHAN(fchan, msg, args...)	ftdm_log_chan(fchan, FTDM_LOG_WARNING, msg , ##args)
#define SS7_ERROR_CHAN(fchan, msg, args...)	ftdm_log_chan(fchan, FTDM_LOG_ERROR, msg , ##args)
#define SS7_CRIT_CHAN(fchan, msg, args...)	ftdm_log_chan(fchan, FTDM_LOG_CRIT, msg , ##args)

#ifdef SS7_CODE_DEVEL
#define SS7_DEVEL_DEBUG(a,...)   ftdm_log(FTDM_LOG_DEBUG,a,##__VA_ARGS__ );
#else
#define SS7_DEVEL_DEBUG(a,...)
#endif

#define SS7_FUNC_TRACE_ENTER(a) if (g_ftdm_sngss7_data.function_trace) { \
									switch (g_ftdm_sngss7_data.function_trace_level) { \
										case 0: \
											ftdm_log(FTDM_LOG_EMERG,"Entering %s\n", a); \
											break; \
										case 1: \
											ftdm_log(FTDM_LOG_ALERT,"Entering %s\n", a); \
											break; \
										case 2: \
											ftdm_log(FTDM_LOG_CRIT,"Entering %s\n", a); \
											break; \
										case 3: \
											ftdm_log(FTDM_LOG_ERROR,"Entering %s\n", a); \
											break; \
										case 4: \
											ftdm_log(FTDM_LOG_WARNING,"Entering %s\n", a); \
											break; \
										case 5: \
											ftdm_log(FTDM_LOG_NOTICE,"Entering %s\n", a); \
											break; \
										case 6: \
											ftdm_log(FTDM_LOG_INFO,"Entering %s\n", a); \
											break; \
										case 7: \
											ftdm_log(FTDM_LOG_DEBUG,"Entering %s\n", a); \
											break; \
										default: \
											ftdm_log(FTDM_LOG_INFO,"Entering %s\n", a); \
											break; \
										} /* switch (g_ftdm_sngss7_data.function_trace_level) */ \
								} /*  if(g_ftdm_sngss7_data.function_trace) */

#define SS7_FUNC_TRACE_EXIT(a) if (g_ftdm_sngss7_data.function_trace) { \
									switch (g_ftdm_sngss7_data.function_trace_level) { \
										case 0: \
											ftdm_log(FTDM_LOG_EMERG,"Exiting %s\n", a); \
											break; \
										case 1: \
											ftdm_log(FTDM_LOG_ALERT,"Exiting %s\n", a); \
											break; \
										case 2: \
											ftdm_log(FTDM_LOG_CRIT,"Exiting %s\n", a); \
											break; \
										case 3: \
											ftdm_log(FTDM_LOG_ERROR,"Exiting %s\n", a); \
											break; \
										case 4: \
											ftdm_log(FTDM_LOG_WARNING,"Exiting %s\n", a); \
											break; \
										case 5: \
											ftdm_log(FTDM_LOG_NOTICE,"Exiting %s\n", a); \
											break; \
										case 6: \
											ftdm_log(FTDM_LOG_INFO,"Exiting %s\n", a); \
											break; \
										case 7: \
											ftdm_log(FTDM_LOG_DEBUG,"Exiting %s\n", a); \
											break; \
										default: \
											ftdm_log(FTDM_LOG_INFO,"Exiting %s\n", a); \
											break; \
										} /* switch (g_ftdm_sngss7_data.function_trace_level) */ \
								} /*  if(g_ftdm_sngss7_data.function_trace) */

#define SS7_MSG_TRACE(fchan, sngss7info ,msg) if (g_ftdm_sngss7_data.message_trace) { \
								switch (g_ftdm_sngss7_data.message_trace_level) { \
									case 0: \
										ftdm_log_chan(fchan, FTDM_LOG_DEBUG, "[CIC:%d][SPINSTID:%d][SUINSTID:%d]%s", \
														sngss7info->circuit->cic,sngss7info->spInstId,sngss7info->suInstId, msg); \
										break; \
									case 1: \
										ftdm_log_chan(fchan, FTDM_LOG_ALERT, "[CIC:%d][SPINSTID:%d][SUINSTID:%d]%s", \
														sngss7info->circuit->cic,sngss7info->spInstId,sngss7info->suInstId, msg); \
										break; \
									case 2: \
										ftdm_log_chan(fchan, FTDM_LOG_CRIT, "[CIC:%d][SPINSTID:%d][SUINSTID:%d]%s", \
														sngss7info->circuit->cic,sngss7info->spInstId,sngss7info->suInstId, msg); \
										break; \
									case 3: \
										ftdm_log_chan(fchan, FTDM_LOG_ERROR, "[CIC:%d][SPINSTID:%d][SUINSTID:%d]%s", \
														sngss7info->circuit->cic,sngss7info->spInstId,sngss7info->suInstId, msg); \
										break; \
									case 4: \
										ftdm_log_chan(fchan, FTDM_LOG_WARNING, "[CIC:%d][SPINSTID:%d][SUINSTID:%d]%s", \
														sngss7info->circuit->cic,sngss7info->spInstId,sngss7info->suInstId, msg); \
										break; \
									case 5: \
										ftdm_log_chan(fchan, FTDM_LOG_NOTICE, "[CIC:%d][SPINSTID:%d][SUINSTID:%d]%s", \
														sngss7info->circuit->cic,sngss7info->spInstId,sngss7info->suInstId, msg); \
										break; \
									case 6: \
										ftdm_log_chan(fchan, FTDM_LOG_INFO, "[CIC:%d][SPINSTID:%d][SUINSTID:%d]%s", \
														sngss7info->circuit->cic,sngss7info->spInstId,sngss7info->suInstId, msg); \
										break; \
									case 7: \
										ftdm_log_chan(fchan, FTDM_LOG_DEBUG, "[CIC:%d][SPINSTID:%d][SUINSTID:%d]%s", \
														sngss7info->circuit->cic,sngss7info->spInstId,sngss7info->suInstId, msg); \
										break; \
									default: \
										ftdm_log_chan(fchan, FTDM_LOG_INFO, "[CIC:%d][SPINSTID:%d][SUINSTID:%d]%s", \
														sngss7info->circuit->cic,sngss7info->spInstId,sngss7info->suInstId, msg); \
										break; \
									} /* switch (g_ftdm_sngss7_data.message_trace_level) */ \
							} /* if(g_ftdm_sngss7_data.message_trace) */

#define sngss7_test_flag(obj, flag)  ((obj)->flags & flag)
#define sngss7_clear_flag(obj, flag) ((obj)->flags &= ~(flag))
#define sngss7_set_flag(obj, flag)   ((obj)->flags |= (flag))

#define sngss7_test_ckt_flag(obj, flag)  ((obj)->ckt_flags & flag)
#define sngss7_clear_ckt_flag(obj, flag) ((obj)->ckt_flags &= ~(flag))
#define sngss7_set_ckt_flag(obj, flag)   ((obj)->ckt_flags |= (flag))

#define sngss7_test_ckt_blk_flag(obj, flag)  ((obj)->blk_flags & flag)
#define sngss7_clear_ckt_blk_flag(obj, flag) ((obj)->blk_flags &= ~(flag))
#define sngss7_set_ckt_blk_flag(obj, flag)   ((obj)->blk_flags |= (flag))

#define sngss7_test_options(obj, option) ((obj)->options & option)
#define sngss7_clear_options(obj, option) ((obj)->options &= ~(option))
#define sngss7_set_options(obj, option)   ((obj)->options |= (option))

#define sngss7_tx_block_status_clear(obj) (!sngss7_test_ckt_blk_flag(obj, (FLAG_CKT_MN_BLOCK_TX | \
								           FLAG_CKT_MN_BLOCK_TX_DN | \
									   FLAG_GRP_MN_BLOCK_TX | \
									   FLAG_GRP_MN_BLOCK_TX_DN | \
									   FLAG_GRP_HW_BLOCK_TX | \
									   FLAG_GRP_HW_BLOCK_TX_DN | \
			       						   FLAG_GRP_HW_UNBLK_TX | \
									   FLAG_CKT_MN_UNBLK_TX	))) 

#define sngss7_block_status_clear(obj) (obj->blk_flags == 0)

#define sngss7_reset_status_clear(obj) (!sngss7_test_ckt_flag(obj, (FLAG_RESET_TX | \
														    FLAG_RESET_RX | \
														  	FLAG_GRP_RESET_TX | \
														  	FLAG_GRP_RESET_RX )))

#define sngss7_tx_reset_sent(obj) ((sngss7_test_ckt_flag(obj, (FLAG_RESET_TX)) && \
											  sngss7_test_ckt_flag(obj, (FLAG_RESET_SENT))) || \
											 (sngss7_test_ckt_flag(obj, (FLAG_GRP_RESET_TX)) && \
											  sngss7_test_ckt_flag(obj, (FLAG_GRP_RESET_SENT))))

#define sngss7_tx_reset_status_pending(obj) (sngss7_test_ckt_flag(obj, (FLAG_RESET_TX)) || sngss7_test_ckt_flag(obj, (FLAG_GRP_RESET_TX))) 
	
#define sngss7_channel_status_clear(obj) ((sngss7_block_status_clear(obj)) && \
					  					  (sngss7_reset_status_clear(obj)) && \
										  (!sngss7_test_ckt_flag((obj),FLAG_INFID_PAUSED)))

#define sngss7_tx_reset_restart(obj) do { clear_tx_grs_flags((obj)); \
										  clear_tx_grs_data((obj)); \
            							  clear_tx_rsc_flags((obj)); \
										  sngss7_set_ckt_flag((obj), (FLAG_RESET_TX)); \
								     } while (0);
 


#ifdef SMG_RELAY_DBG
#define SS7_RELAY_DBG(a,...)	printf(a"\n", ##__VA_ARGS__)
#define SS7_RELAY_DBG_FUN(a)	printf(#a"\n")
#else
#define SS7_RELAY_DBG(a, ...)  
#define SS7_RELAY_DBG_FUN(a)
#endif
/******************************************************************************/

/******************************************************************************/
#endif /* __FTMOD_SNG_SS7_H__ */
/******************************************************************************/

/******************************************************************************/
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
/******************************************************************************/

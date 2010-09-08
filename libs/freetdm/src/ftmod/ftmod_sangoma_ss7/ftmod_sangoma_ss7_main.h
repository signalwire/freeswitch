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

#include "sng_ss7.h"

/******************************************************************************/

/* DEFINES ********************************************************************/
#define MAX_NAME_LEN			10
#define MAX_PATH				255

#define MAX_CIC_LENGTH			5
#define MAX_CIC_MAP_LENGTH		1000 

#define SNGSS7_EVENT_QUEUE_SIZE	100

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
	SNGSS7_STA_IND_EVENT
} sng_event_type_t;

typedef enum {
	VOICE = 0,
	SIG,
	HOLE
} sng_ckt_type_t;

typedef enum {
	CONFIGURED		= (1 << 0),
	ACTIVE			= (1 << 1)
} sng_flag_t;

typedef struct sng_mtp_link {
	char			name[MAX_NAME_LEN];
	uint32_t		id;
	uint32_t		flags;
	struct {
		uint32_t	span;
		uint32_t	chan;
	} mtp1;
	struct {
		uint32_t	lssuLength;
		uint32_t	errorType;
		uint32_t	linkType;
		uint32_t	mtp1Id;
		uint32_t	t1;
		uint32_t	t2;
		uint32_t	t3;
		uint32_t	t4n;
		uint32_t	t4e;
		uint32_t	t5;
		uint32_t	t6;
		uint32_t	t7;
	} mtp2;
	struct {
		uint32_t	priority;
		uint32_t	linkType;
		uint32_t	switchType;
		uint32_t	apc;
		uint32_t	spc;
		uint32_t	ssf;
		uint32_t	slc;
		uint32_t	linkSetId;
		uint32_t	mtp2Id;
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
	} mtp3;
} sng_mtp_link_t;

typedef struct sng_link_set {
	uint32_t		id;
	char			name[MAX_NAME_LEN];
	uint32_t		flags;
	uint32_t		apc;
	uint32_t		linkType;
	uint32_t 		minActive;
	uint32_t		numLinks;
	uint32_t		links[16];
} sng_link_set_t;

typedef struct sng_route {
	uint32_t		id;
	char			name[MAX_NAME_LEN];
	uint32_t		flags;
	uint32_t		dpc;
	uint32_t		cmbLinkSetId;
	uint32_t		linkType;
	uint32_t		switchType;
	uint32_t		ssf;
	uint32_t		isSTP;
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
	uint32_t		id;
	char			name[MAX_NAME_LEN];
	uint32_t		flags;
	uint32_t		spc;
	uint32_t		dpc;
	uint32_t		switchType;
	uint32_t		nwId;
	uint32_t		mtpRouteId;
	uint32_t		ssf;
	uint32_t		isap;
	uint16_t		t4;
	uint32_t		t10;
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
	uint32_t		t35;
	uint32_t		t37;
	uint32_t		t38;
	uint32_t		t39;
	uint32_t		tfgr;
	uint32_t		tpause;
	uint32_t		tstaenq;
} sng_isup_inf_t;

typedef struct sng_isup_ckt {
	uint32_t		id;
	uint32_t		flags;
	uint32_t		span;
	uint32_t		chan;
	uint32_t		type;	/* VOICE/SIG/HOLE */
	uint32_t		cic;
	uint32_t		infId;
	uint32_t		ssf;
	uint32_t		typeCntrl;
	void			*obj;
	uint16_t		t3;
	uint16_t		t12;
	uint16_t		t13;
	uint16_t		t14;
	uint16_t		t15;
	uint16_t		t16;
	uint16_t		t17;
	uint16_t		tval;
} sng_isup_ckt_t;

typedef struct sng_nsap {
	uint32_t		id;
	uint32_t		flags;
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

typedef struct sng_ss7_cfg {
	uint32_t			spc;
	char				license[MAX_PATH];
	char				signature[MAX_PATH];
	sng_mtp_link_t		mtpLink[MAX_MTP_LINKS+1];
	sng_link_set_t		mtpLinkSet[MAX_MTP_LINKSETS+1];
	sng_route_t			mtpRoute[MAX_MTP_ROUTES+1];
	sng_isup_inf_t		isupIntf[MAX_ISUP_INFS+1];
	sng_isup_ckt_t		isupCkt[MAX_ISUP_CKTS+1];
	sng_nsap_t			nsap[MAX_NSAPS+1];
	sng_isap_t			isap[MAX_ISAPS+1];	
}sng_ss7_cfg_t;

typedef struct ftdm_sngss7_data {
	sng_ss7_cfg_t		cfg;
	int					gen_config;
	int					min_digits;
	int					function_trace;
	int					function_trace_level;
	int					message_trace;
	int					message_trace_level;
	fio_signal_cb_t		sig_cb;
}ftdm_sngss7_data_t;

typedef struct sngss7_timer_data {
	ftdm_timer_t			*heartbeat_timer;
	int						beat;
	int						counter;
	ftdm_sched_callback_t	callback;
	ftdm_sched_t			*sched;
	void					*sngss7_info;
}sngss7_timer_data_t;

typedef struct sngss7_glare_data {
	uint32_t				spInstId; 
	uint32_t				circuit; 
	SiConEvnt				iam;
}sngss7_glare_data_t;

typedef struct sngss7_group_data {
	uint32_t				circuit;
	uint32_t				range;
	uint8_t					status[255];
	uint8_t					type;
}sngss7_group_data_t;

typedef struct sngss7_chan_data {
	ftdm_channel_t			*ftdmchan;
	sng_isup_ckt_t			*circuit;
	uint32_t				base_chan;
	uint32_t				suInstId;
	uint32_t				spInstId;
	uint32_t				spId;
	uint8_t					globalFlg;
	uint32_t				flags;
	sngss7_glare_data_t		glare;
	sngss7_timer_data_t		t35;
}sngss7_chan_data_t;

typedef struct sngss7_span_data {
	ftdm_sched_t			*sched;
	sngss7_group_data_t		rx_grs;
	sngss7_group_data_t		tx_grs;
	sngss7_group_data_t		rx_cgb;
	sngss7_group_data_t		tx_cgb;
	sngss7_group_data_t		rx_cgu;
	sngss7_group_data_t		tx_cgu;
	ftdm_queue_t 			*event_queue;
}sngss7_span_data_t;

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
	FLAG_CKT_UCIC_BLOCK		= (1 << 16),
	FLAG_CKT_UCIC_UNBLK		= (1 << 17),
	FLAG_CKT_LC_BLOCK_RX	= (1 << 18),
	FLAG_CKT_LC_UNBLK_RX	= (1 << 19),
	FLAG_CKT_MN_BLOCK_RX	= (1 << 20),
	FLAG_CKT_MN_UNBLK_RX	= (1 << 21),
	FLAG_CKT_MN_BLOCK_TX	= (1 << 22),
	FLAG_CKT_MN_UNBLK_TX	= (1 << 23),
	FLAG_GRP_HW_BLOCK_RX	= (1 << 24),
	FLAG_GRP_HW_BLOCK_TX	= (1 << 25),
	FLAG_GRP_MN_BLOCK_RX	= (1 << 26),
	FLAG_GRP_MN_BLOCK_TX	= (1 << 27),
	FLAG_GRP_HW_UNBLK_TX	= (1 << 28),
	FLAG_GRP_MN_UNBLK_TX	= (1 << 29)
} flag_t;
/******************************************************************************/

/* GLOBALS ********************************************************************/
extern ftdm_sngss7_data_t   g_ftdm_sngss7_data;
extern uint32_t			 sngss7_id;
extern ftdm_sched_t		 *sngss7_sched;
/******************************************************************************/

/* PROTOTYPES *****************************************************************/
void handle_sng_log(uint8_t level, char *fmt,...);
void handle_sng_mtp1_alarm(Pst *pst, L1Mngmt *sta);
void handle_sng_mtp2_alarm(Pst *pst, SdMngmt *sta);
void handle_sng_mtp3_alarm(Pst *pst, SnMngmt *sta);
void handle_sng_isup_alarm(Pst *pst, SiMngmt *sta);
void handle_sng_cc_alarm(Pst *pst, CcMngmt *sta);

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

int ftmod_ss7_inhibit_mtplink(uint32_t id);
int ftmod_ss7_uninhibit_mtplink(uint32_t id);

int ftmod_ss7_mtplink_sta(uint32_t id, SnMngmt *cfm);
int ftmod_ss7_mtplinkSet_sta(uint32_t id, SnMngmt *cfm);

int  ft_to_sngss7_activate_all(void);

void ft_to_sngss7_iam(ftdm_channel_t *ftdmchan);
void ft_to_sngss7_acm(ftdm_channel_t *ftdmchan);
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

ftdm_status_t handle_con_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiConEvnt *siConEvnt);
ftdm_status_t handle_con_sta(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiCnStEvnt *siCnStEvnt, uint8_t evntType);
ftdm_status_t handle_con_cfm(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiConEvnt *siConEvnt);
ftdm_status_t handle_rel_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiRelEvnt *siRelEvnt);
ftdm_status_t handle_rel_cfm(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiRelEvnt *siRelEvnt);
ftdm_status_t handle_dat_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiInfoEvnt *siInfoEvnt);
ftdm_status_t handle_fac_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t evntType, SiFacEvnt *siFacEvnt);
ftdm_status_t handle_fac_cfm(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t evntType, SiFacEvnt *siFacEvnt);
ftdm_status_t handle_umsg_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit);
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

uint8_t copy_cgPtyNum_from_sngss7(ftdm_caller_data_t *ftdm, SiCgPtyNum *cgPtyNum);
uint8_t copy_cgPtyNum_to_sngss7(ftdm_caller_data_t *ftdm, SiCgPtyNum *cgPtyNum);
uint8_t copy_cdPtyNum_from_sngss7(ftdm_caller_data_t *ftdm, SiCdPtyNum *cdPtyNum);
uint8_t copy_cdPtyNum_to_sngss7(ftdm_caller_data_t *ftdm, SiCdPtyNum *cdPtyNum);
uint8_t copy_tknStr_from_sngss7(TknStr str, char *ftdm, TknU8 oddEven);
int check_for_state_change(ftdm_channel_t *ftdmchan);
int check_cics_in_range(sngss7_chan_data_t *sngss7_info);
int check_for_reset(sngss7_chan_data_t *sngss7_info);
ftdm_status_t extract_chan_data(uint32_t circuit, sngss7_chan_data_t **sngss7_info, ftdm_channel_t **ftdmchan);
unsigned long get_unique_id(void);

int ftmod_ss7_parse_xml(ftdm_conf_parameter_t *ftdm_parameters, ftdm_span_t *span);

void handle_isup_t35(void *userdata);

ftdm_status_t ftdm_sngss7_handle_cli_cmd(ftdm_stream_handle_t *stream, const char *data);

ftdm_status_t check_if_rx_grs_processed(ftdm_span_t *ftdmspan);
/******************************************************************************/

/* MACROS *********************************************************************/
#define SS7_DEBUG(a,...)	ftdm_log(FTDM_LOG_DEBUG,a , ##__VA_ARGS__ );
#define SS7_INFO(a,...)	 ftdm_log(FTDM_LOG_INFO,a , ##__VA_ARGS__ );
#define SS7_WARN(a,...)	 ftdm_log(FTDM_LOG_WARNING,a , ##__VA_ARGS__ );
#define SS7_ERROR(a,...)	ftdm_log(FTDM_LOG_ERROR,a , ##__VA_ARGS__ );
#define SS7_CRITICAL(a,...) ftdm_log(FTDM_LOG_CRIT,a , ##__VA_ARGS__ );

#define SS7_DEBUG_CHAN(fchan, msg, args...)	ftdm_log_chan(fchan, FTDM_LOG_DEBUG, msg , ##args)
#define SS7_INFO_CHAN(fchan, msg, args...)	ftdm_log_chan(fchan, FTDM_LOG_INFO, msg , ##args)
#define SS7_WARN_CHAN(fchan, msg, args...)	ftdm_log_chan(fchan, FTDM_LOG_WARNING, msg , ##args)
#define SS7_ERROR_CHAN(fchan, msg, args...)	ftdm_log_chan(fchan, FTDM_LOG_ERROR, msg , ##args)
#define SS7_CTRIT_CHAN(fchan, msg, args...)	ftdm_log_chan(fchan, FTDM_LOG_CRIT, msg , ##args)

#ifdef KONRAD_DEVEL
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
											ftdm_log(FTDM_LOG_EMERG,"Exitting %s\n", a); \
											break; \
										case 1: \
											ftdm_log(FTDM_LOG_ALERT,"Exitting %s\n", a); \
											break; \
										case 2: \
											ftdm_log(FTDM_LOG_CRIT,"Exitting %s\n", a); \
											break; \
										case 3: \
											ftdm_log(FTDM_LOG_ERROR,"Exitting %s\n", a); \
											break; \
										case 4: \
											ftdm_log(FTDM_LOG_WARNING,"Exitting %s\n", a); \
											break; \
										case 5: \
											ftdm_log(FTDM_LOG_NOTICE,"Exitting %s\n", a); \
											break; \
										case 6: \
											ftdm_log(FTDM_LOG_INFO,"Exitting %s\n", a); \
											break; \
										case 7: \
											ftdm_log(FTDM_LOG_DEBUG,"Exitting %s\n", a); \
											break; \
										default: \
											ftdm_log(FTDM_LOG_INFO,"Exitting %s\n", a); \
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

# define SS7_ASSERT	*(int*)0=0;
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
/******************************************************************************/

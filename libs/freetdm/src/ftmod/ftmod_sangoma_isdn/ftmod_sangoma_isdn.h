/*
 * Copyright (c) 2010, Sangoma Technologies 
 * David Yat Sin <davidy@sangoma.com>
 * Moises Silva <moy@sangoma.com>
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
#ifndef __FTMOD_SNG_ISDN_H__
#define __FTMOD_SNG_ISDN_H__

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <ctype.h>

#include "private/ftdm_core.h"
#include "ftmod_sangoma_isdn_user.h"

#ifdef WIN32
#include <sng_isdn.h>
#else
#include <sng_isdn/sng_isdn.h>
#endif

/* Theoretical limit for MAX_SPANS_PER_NFAS_LINK is 31,
   but set to 8 for now to save some memory */

#define MAX_SPANS_PER_NFAS_LINK	  	16
#define MAX_NFAS_GROUPS				16
#define NUM_E1_CHANNELS_PER_SPAN 	32
#define NUM_T1_CHANNELS_PER_SPAN 	24
#define NUM_BRI_CHANNELS_PER_SPAN	2
#define SNGISDN_EVENT_QUEUE_SIZE	100
#define SNGISDN_EVENT_POLL_RATE		100
#define SNGISDN_NUM_LOCAL_NUMBERS	8
#define SNGISDN_DCHAN_QUEUE_LEN		200
#define MAX_NFAS_GROUP_NAME			50

#define NSG

#ifndef MI_NOTIFY
#define MI_NOTIFY 0x14
#endif

typedef enum {
	FLAG_RESET_RX           = (1 << 0),
	FLAG_RESET_TX           = (1 << 1),
	FLAG_REMOTE_REL         = (1 << 2),
	FLAG_LOCAL_REL          = (1 << 3),
	FLAG_REMOTE_ABORT       = (1 << 4),
	FLAG_LOCAL_ABORT        = (1 << 5),
	FLAG_GLARE              = (1 << 6),
	FLAG_DELAYED_REL        = (1 << 7),
	FLAG_SENT_PROCEED       = (1 << 8),
	FLAG_SEND_DISC  		= (1 << 9),	
	/* Used for BRI only, flag is set after we request line CONNECTED */
	FLAG_ACTIVATING			= (1 << 10),
	/* Used when we receive an ALERT msg + inband tones ready */
	FLAG_MEDIA_READY		= (1 << 11),
	/* Set when we already sent a Channel ID IE */
	FLAG_SENT_CHAN_ID		= (1 << 12),
	/* Set when we already sent a Connect */
	FLAG_SENT_CONNECT		= (1 << 13),
} sngisdn_flag_t;


typedef enum {
	SNGISDN_SWITCH_INVALID = 0,	/* invalid */
	SNGISDN_SWITCH_NI2 ,	/* national isdn-2 */
	SNGISDN_SWITCH_5ESS,	/* att 5ess */
	SNGISDN_SWITCH_4ESS,	/* att 4ess */
	SNGISDN_SWITCH_DMS100,	/* nt dms100 */
	SNGISDN_SWITCH_EUROISDN,/* etsi */
	SNGISDN_SWITCH_QSIG,	/* etsi qsig */
	SNGISDN_SWITCH_INSNET,	/* int - net */
} sngisdn_switchtype_t;

typedef enum {
	SNGISDN_SIGNALING_INVALID = 0,		/* invalid */
	SNGISDN_SIGNALING_CPE ,				/* customer side emulation */
	SNGISDN_SIGNALING_NET,				/* network side emulation */
} sngisdn_signalingtype_t;

typedef enum {
	SNGISDN_TRACE_DISABLE = 0,
	SNGISDN_TRACE_Q921 = 1,
	SNGISDN_TRACE_Q931 = 2,
} sngisdn_tracetype_t;

typedef enum {
	SNGISDN_OPT_DEFAULT = 0,
	SNGISDN_OPT_TRUE = 1,
	SNGISDN_OPT_FALSE = 2,
} sngisdn_opt_t;

typedef enum {
	SNGISDN_EARLY_MEDIA_ON_PROCEED = (1 << 0),
	SNGISDN_EARLY_MEDIA_ON_PROGRESS = (1 << 1),
	SNGISDN_EARLY_MEDIA_ON_ALERT= (1 << 2),
} sngisdn_early_media_opt_t;

typedef enum {
	SNGISDN_AVAIL_DOWN = 1,
	SNGISDN_AVAIL_PWR_SAVING = 5,
	SNGISDN_AVAIL_UP = 10,
} sngisdn_avail_t;

typedef enum {
	SNGISDN_CID_NAME_AUTO,
	SNGISDN_CID_NAME_DISPLAY_IE,
	SNGISDN_CID_NAME_USR_USR_IE,
	SNGISDN_CID_NAME_FACILITY_IE,
} sngisdn_cid_name_t;

typedef enum {
	SNGISDN_EVENT_CON_IND = 1,
	SNGISDN_EVENT_CON_CFM,
	SNGISDN_EVENT_CNST_IND,
	SNGISDN_EVENT_DISC_IND,
	SNGISDN_EVENT_REL_IND,
	SNGISDN_EVENT_DAT_IND,
	SNGISDN_EVENT_SSHL_IND,
	SNGISDN_EVENT_SSHL_CFM,
	SNGISDN_EVENT_RMRT_IND,
	SNGISDN_EVENT_RMRT_CFM,
	SNGISDN_EVENT_FLC_IND,
	SNGISDN_EVENT_FAC_IND,
	SNGISDN_EVENT_STA_CFM,
	SNGISDN_EVENT_SRV_IND,
	SNGISDN_EVENT_SRV_CFM,
	SNGISDN_EVENT_RST_CFM,
	SNGISDN_EVENT_RST_IND,
} ftdm_sngisdn_event_id_t;

typedef struct ftdm_sngisdn_prog_ind {
	ftdm_sngisdn_progind_loc_t loc;		/* location */
	ftdm_sngisdn_progind_descr_t descr;	/* description */
} ftdm_sngisdn_progind_t;

/* Only timers that can be cancelled are listed here */
#define SNGISDN_NUM_CHAN_TIMERS 2
/* Increase SNGISDN_NUM_CHAN_TIMERS as number of ftdm_sngisdn_chan_timer_t increases */
typedef enum {	
	SNGISDN_CHAN_TIMER_FACILITY,
	SNGISDN_CHAN_TIMER_ATT_TRANSFER,
} ftdm_sngisdn_chan_timer_t;

#define SNGISDN_NUM_SPAN_TIMERS 1
/* Increase SNGISDN_NUM_SPAN_TIMERS as number of ftdm_sngisdn_spanan_timer_t increases */
typedef enum {	
	SNGISDN_SPAN_TIMER_RESTART,
} ftdm_sngisdn_span_timer_t;

typedef struct sngisdn_glare_data {
	int16_t		suId;
    uint32_t	suInstId;
    uint32_t	spInstId;
	int16_t		dChan;
    ConEvnt		setup;
	uint8_t		ces;
} sngisdn_glare_data_t;

typedef enum {
	SNGISDN_TRANSFER_NONE = 0, /* Default value, no transfer being done */
	SNGISDN_TRANSFER_ATT_COURTESY_VRU,
	SNGISDN_TRANSFER_ATT_COURTESY_VRU_DATA,
	SNGISDN_TRANSFER_INVALID,
} sngisdn_transfer_type_t;
#define SNGISDN_TRANSFER_TYPE_STRINGS "NONE", "ATT_COURTESY_VRU", "ATT_COURTERY_VRU_DATA", "INVALID"
SNGISDN_STR2ENUM_P(ftdm_str2sngisdn_transfer_type, sngisdn_transfer_type2str, sngisdn_transfer_type_t)

/* From section 4.2 of TR50075, max length of data is 100 when single UUI is sent */
#define COURTESY_TRANSFER_MAX_DATA_SIZE 100

typedef struct _att_courtesy_vru
{
	char dtmf_digits [20];
	char data[COURTESY_TRANSFER_MAX_DATA_SIZE];
} att_courtesy_vru_t;

typedef struct _sngisdn_transfer_data
{
	sngisdn_transfer_type_t type; /* Specifies which type of transfer is being used */
	ftdm_transfer_response_t response;
	union
	{
		att_courtesy_vru_t att_courtesy_vru;
	} tdata;
} sngisdn_transfer_data_t;

/* Channel specific data */
typedef struct sngisdn_chan_data {
    ftdm_channel_t			*ftdmchan;
    uint32_t				flags;
	uint8_t 				ces;		/* used only for BRI, otherwise always 0 */
	uint8_t					dchan_id;
	uint16_t				call_ref;	/* Q.931 call reference, only valid for ETSI/INSNET/QSIG */
	uint32_t				suInstId;	/* instance ID generated locally */
	uint32_t				spInstId;	/* instance ID generated by stack */

	uint8_t                 globalFlg;
	sngisdn_glare_data_t	glare;
	ftdm_timer_id_t			timers[SNGISDN_NUM_CHAN_TIMERS];
	sngisdn_transfer_data_t transfer_data;

	/* variables saved here will be sent to the user application
	on next SIGEVENT_XXX */
	ftdm_hash_t*			variables;

	/* raw_data saved here will be sent to the user application
	on next SIGEVENT_XXX */
	void					*raw_data;
	ftdm_size_t				raw_data_len;
} sngisdn_chan_data_t;

struct sngisdn_nfas_data;
typedef struct sngisdn_nfas_data sngisdn_nfas_data_t;

typedef enum {
	SNGISDN_NFAS_DCHAN_NONE,
	SNGISDN_NFAS_DCHAN_PRIMARY,
	SNGISDN_NFAS_DCHAN_BACKUP,
} sngisdn_nfas_sigchan_t;

/* Span specific data */
typedef struct sngisdn_span_data {
	ftdm_span_t		*ftdm_span;
	ftdm_channel_t	*dchan;
	uint8_t			link_id;
	uint8_t 		switchtype;
	uint8_t			signalling;		/* SNGISDN_SIGNALING_CPE or SNGISDN_SIGNALING_NET */
	uint8_t 		cc_id;
	ftdm_signaling_status_t		sigstatus;

	uint8_t			tei;
	uint8_t			min_digits;
	uint8_t			trace_flags;		/* TODO change to bit map of sngisdn_tracetype_t */
	uint8_t			early_media_flags;	/* bit map of ftdm_sngisdn_early_media_opt_t */
	uint8_t			overlap_dial;
	uint8_t			setup_arb;
	uint8_t			facility_ie_decode;
	uint8_t			facility;
	int32_t			facility_timeout;
	uint8_t			att_remove_dtmf;
	int32_t			transfer_timeout;
	uint8_t			num_local_numbers;
	uint8_t			ignore_cause_value;
	uint8_t			trace_q931; /* TODO: combine with trace_flags */
	uint8_t			trace_q921; /* TODO: combine with trace_flags */
	uint8_t			raw_trace_q931; /* TODO: combine with trace_flags */
	uint8_t			raw_trace_q921; /* TODO: combine with trace_flags */
	uint8_t			timer_t3;
	uint8_t			restart_opt;
	uint8_t			restart_timeout;
	uint8_t			force_sending_complete;
	uint8_t			cid_name_method;
	uint8_t			send_cid_name;	
	uint8_t 		send_connect_ack;
	uint8_t                 dl_request_pending; /* Whether we have a DL request pending */


	int32_t			timer_t301;
	int32_t			timer_t302;
	int32_t			timer_t303;
	int32_t			timer_t304;
	int32_t			timer_t305;
	int32_t			timer_t306;
	int32_t			timer_t307;
	int32_t			timer_t308;
	int32_t			timer_t310;
	int32_t			timer_t312;
	int32_t			timer_t313;
	int32_t			timer_t314;
	int32_t			timer_t316;
	int32_t			timer_t318;
	int32_t			timer_t319;
	int32_t			timer_t322;
	char*			local_numbers[SNGISDN_NUM_LOCAL_NUMBERS];
	ftdm_timer_id_t timers[SNGISDN_NUM_SPAN_TIMERS];
	ftdm_sched_t 	*sched;
	ftdm_queue_t 	*event_queue;

	struct nfas_info {
		sngisdn_nfas_data_t *trunk;
		sngisdn_nfas_sigchan_t sigchan;
		uint8_t interface_id;
	} nfas;

	uint32_t num_chans;
	sngisdn_chan_data_t *channels[NUM_E1_CHANNELS_PER_SPAN];
} sngisdn_span_data_t;

typedef struct sngisdn_event_data {
	
	int16_t		suId;
	int16_t		dChan;
	uint32_t	suInstId;
	uint32_t	spInstId;
	uint8_t		ces;
	uint8_t		action;
	uint8_t		evntType;

	sngisdn_chan_data_t *sngisdn_info;
	sngisdn_span_data_t *signal_data;	
	
	ftdm_sngisdn_event_id_t event_id;
	
	union
	{
		ConEvnt		conEvnt;
		CnStEvnt	cnStEvnt;
		DiscEvnt	discEvnt;
		RelEvnt		relEvnt;
		InfoEvnt	infoEvnt;
		SsHlEvnt	ssHlEvnt;
		RmRtEvnt	rmRtEvnt;
		StaEvnt 	staEvnt;
		FacEvnt 	facEvnt;
		Srv			srvEvnt;
		Rst			rstEvnt;
	}event;
	
} sngisdn_event_data_t;

struct sngisdn_nfas_data {
	char name[MAX_NFAS_GROUP_NAME];

	char dchan_span_name[20];
	sngisdn_span_data_t *dchan;	/* Span that contains primary d-channel */

	char backup_span_name[20];
	sngisdn_span_data_t *backup; /* Span that contains backup d-channel */
	uint8_t num_spans;			/* Number of spans within this NFAS */
	uint8_t num_spans_configured;
	sngisdn_span_data_t	*spans[MAX_SPANS_PER_NFAS_LINK+1]; //indexed by logical span id
};

typedef struct sngisdn_cc {
	/* TODO: use flags instead of config_done and activation_done */
	uint8_t				config_done;
	uint8_t				activation_done;
	uint8_t				switchtype;
	ftdm_trunk_type_t	trunktype;
	uint32_t			last_suInstId;	
	ftdm_mutex_t		*mutex;
	sngisdn_chan_data_t	*active_spInstIds[MAX_INSTID+1];
	sngisdn_chan_data_t	*active_suInstIds[MAX_INSTID+1];
}sngisdn_cc_t;

/* Global sngisdn data */
typedef struct ftdm_sngisdn_data {
	uint8_t	gen_config_done;
	uint8_t num_cc;						/* 1 ent per switchtype */	
	struct sngisdn_cc ccs[MAX_VARIANTS+1];	
	uint8_t num_nfas;
	sngisdn_nfas_data_t nfass[MAX_NFAS_GROUPS+1];
	sngisdn_span_data_t *spans[MAX_L1_LINKS+1]; /* spans are indexed by link_id */

#ifdef SANGOMA_ISDN_CHAN_ID_INVERT_BIT
	/* Since this is a global configuration, place it here instead of sngisdn_span_data_t */
	uint8_t chan_id_invert_extend_bit;
#endif
}ftdm_sngisdn_data_t;

typedef struct ftdm2trillium
{
	uint8_t ftdm_val;
	uint8_t trillium_val;
}ftdm2trillium_t;


/* TODO implement these 2 functions */
#define ISDN_FUNC_TRACE_ENTER(a)
#define ISDN_FUNC_TRACE_EXIT(a)

/* Global Structs */
extern ftdm_sngisdn_data_t	g_sngisdn_data;

/* Configuration functions */
ftdm_status_t ftmod_isdn_parse_cfg(ftdm_conf_parameter_t *ftdm_parameters, ftdm_span_t *span);

/* Support functions */
uint32_t get_unique_suInstId(int16_t cc_id);
void clear_call_data(sngisdn_chan_data_t *sngisdn_info);
void clear_call_glare_data(sngisdn_chan_data_t *sngisdn_info);
ftdm_status_t get_ftdmchan_by_suInstId(int16_t cc_id, uint32_t suInstId, sngisdn_chan_data_t **sngisdn_data);
ftdm_status_t get_ftdmchan_by_spInstId(int16_t cc_id, uint32_t spInstId, sngisdn_chan_data_t **sngisdn_data);

ftdm_status_t sngisdn_set_span_avail_rate(ftdm_span_t *span, sngisdn_avail_t avail);
ftdm_status_t sngisdn_set_chan_avail_rate(ftdm_channel_t *chan, sngisdn_avail_t avail);
void sngisdn_set_span_sig_status(ftdm_span_t *ftdmspan, ftdm_signaling_status_t status);
void sngisdn_set_chan_sig_status(ftdm_channel_t *ftdmchan, ftdm_signaling_status_t status);

ftdm_status_t sngisdn_activate_trace(ftdm_span_t *span, sngisdn_tracetype_t trace_opt);


void stack_hdr_init(Header *hdr);
void stack_pst_init(Pst *pst);

/* Outbound Call Control functions */
void sngisdn_snd_setup(ftdm_channel_t *ftdmchan);
void sngisdn_snd_setup_ack(ftdm_channel_t *ftdmchan);
void sngisdn_snd_proceed(ftdm_channel_t *ftdmchan, ftdm_sngisdn_progind_t prog_ind);
void sngisdn_snd_progress(ftdm_channel_t *ftdmchan, ftdm_sngisdn_progind_t prog_ind);
void sngisdn_snd_alert(ftdm_channel_t *ftdmchan, ftdm_sngisdn_progind_t prog_ind);
void sngisdn_snd_notify_req(ftdm_channel_t *ftdmchan);
void sngisdn_snd_connect(ftdm_channel_t *ftdmchan);
void sngisdn_snd_disconnect(ftdm_channel_t *ftdmchan);
void sngisdn_snd_release(ftdm_channel_t *ftdmchan, uint8_t glare);
void sngisdn_snd_reset(ftdm_channel_t *ftdmchan);
void sngisdn_snd_con_complete(ftdm_channel_t *ftdmchan);
void sngisdn_snd_fac_req(ftdm_channel_t *ftdmchan);
void sngisdn_snd_dl_req(ftdm_channel_t *ftdmchan);
void sngisdn_snd_status_enq(ftdm_channel_t *ftdmchan);
void sngisdn_snd_restart(ftdm_channel_t *ftdmchan);
void sngisdn_snd_data(ftdm_channel_t *dchan, uint8_t *data, ftdm_size_t len);
void sngisdn_snd_event(sngisdn_span_data_t *signal_data, ftdm_oob_event_t event);

/* Inbound Call Control functions */
void sngisdn_rcv_con_ind(int16_t suId, uint32_t suInstId, uint32_t spInstId, ConEvnt *conEvnt, int16_t dChan, uint8_t ces);
void sngisdn_rcv_con_cfm(int16_t suId, uint32_t suInstId, uint32_t spInstId, CnStEvnt *cnStEvnt, int16_t dChan, uint8_t ces);
void sngisdn_rcv_cnst_ind(int16_t suId, uint32_t suInstId, uint32_t spInstId, CnStEvnt *cnStEvnt, uint8_t evntType, int16_t dChan, uint8_t ces);
void sngisdn_rcv_disc_ind(int16_t suId, uint32_t suInstId, uint32_t spInstId, DiscEvnt *discEvnt);
void sngisdn_rcv_rel_ind(int16_t suId, uint32_t suInstId, uint32_t spInstId, RelEvnt *relEvnt);
void sngisdn_rcv_dat_ind(int16_t suId, uint32_t suInstId, uint32_t spInstId, InfoEvnt *infoEvnt);
void sngisdn_rcv_sshl_ind(int16_t suId, uint32_t suInstId, uint32_t spInstId, SsHlEvnt *ssHlEvnt, uint8_t action);
void sngisdn_rcv_sshl_cfm(int16_t suId, uint32_t suInstId, uint32_t spInstId, SsHlEvnt *ssHlEvnt, uint8_t action);
void sngisdn_rcv_rmrt_ind(int16_t suId, uint32_t suInstId, uint32_t spInstId, RmRtEvnt *rmRtEvnt, uint8_t action);
void sngisdn_rcv_rmrt_cfm(int16_t suId, uint32_t suInstId, uint32_t spInstId, RmRtEvnt *rmRtEvnt, uint8_t action);
void sngisdn_rcv_flc_ind(int16_t suId, uint32_t suInstId, uint32_t spInstId, StaEvnt *staEvnt);
void sngisdn_rcv_fac_ind(int16_t suId, uint32_t suInstId, uint32_t spInstId, FacEvnt *facEvnt, uint8_t evntType, int16_t dChan, uint8_t ces);
void sngisdn_rcv_sta_cfm(int16_t suId, uint32_t suInstId, uint32_t spInstId, StaEvnt *staEvnt);
void sngisdn_rcv_srv_ind(int16_t suId, Srv *srvEvnt, int16_t dChan, uint8_t ces);
void sngisdn_rcv_srv_cfm(int16_t suId, Srv *srvEvnt, int16_t dChan, uint8_t ces);
void sngisdn_rcv_rst_cfm(int16_t suId, Rst *rstEvnt, int16_t dChan, uint8_t ces, uint8_t evntType);
void sngisdn_rcv_rst_ind(int16_t suId, Rst *rstEvnt, int16_t dChan, uint8_t ces, uint8_t evntType);
int16_t sngisdn_rcv_l1_data_req(uint16_t spId, sng_l1_frame_t *l1_frame);
int16_t sngisdn_rcv_l1_cmd_req(uint16_t spId, sng_l1_cmd_t *l1_cmd);


void sngisdn_process_con_ind (sngisdn_event_data_t *sngisdn_event);
void sngisdn_process_con_cfm (sngisdn_event_data_t *sngisdn_event);
void sngisdn_process_cnst_ind (sngisdn_event_data_t *sngisdn_event);
void sngisdn_process_disc_ind (sngisdn_event_data_t *sngisdn_event);
void sngisdn_process_rel_ind (sngisdn_event_data_t *sngisdn_event);
void sngisdn_process_dat_ind (sngisdn_event_data_t *sngisdn_event);
void sngisdn_process_sshl_ind (sngisdn_event_data_t *sngisdn_event);
void sngisdn_process_sshl_cfm (sngisdn_event_data_t *sngisdn_event);
void sngisdn_process_rmrt_ind (sngisdn_event_data_t *sngisdn_event);
void sngisdn_process_rmrt_cfm (sngisdn_event_data_t *sngisdn_event);
void sngisdn_process_flc_ind (sngisdn_event_data_t *sngisdn_event);
void sngisdn_process_fac_ind (sngisdn_event_data_t *sngisdn_event);
void sngisdn_process_sta_cfm (sngisdn_event_data_t *sngisdn_event);

void sngisdn_process_srv_ind (sngisdn_event_data_t *sngisdn_event);
void sngisdn_process_srv_cfm (sngisdn_event_data_t *sngisdn_event);
void sngisdn_process_rst_cfm (sngisdn_event_data_t *sngisdn_event);
void sngisdn_process_rst_ind (sngisdn_event_data_t *sngisdn_event);

void sngisdn_rcv_phy_ind(SuId suId, Reason reason);
void sngisdn_rcv_q921_ind(BdMngmt *status);

void sngisdn_trace_interpreted_q921(sngisdn_span_data_t *signal_data, ftdm_trace_dir_t dir, uint8_t *data, uint32_t data_len);
void sngisdn_trace_interpreted_q931(sngisdn_span_data_t *signal_data, ftdm_trace_dir_t dir, uint8_t *data, uint32_t data_len);
void sngisdn_trace_raw_q921(sngisdn_span_data_t *signal_data, ftdm_trace_dir_t dir, uint8_t *data, uint32_t data_len);
void sngisdn_trace_raw_q931(sngisdn_span_data_t *signal_data, ftdm_trace_dir_t dir, uint8_t *data, uint32_t data_len);

void sngisdn_get_memory_info(void);

ftdm_status_t sng_isdn_activate_trace(ftdm_span_t *span, sngisdn_tracetype_t trace_opt);
ftdm_status_t sngisdn_check_free_ids(void);

void sngisdn_rcv_q921_trace(BdMngmt *trc, Buffer *mBuf);
void sngisdn_rcv_q931_ind(InMngmt *status);
void sngisdn_rcv_q931_trace(InMngmt *trc, Buffer *mBuf);
void sngisdn_rcv_cc_ind(CcMngmt *status);
void sngisdn_rcv_sng_log(uint8_t level, char *fmt,...);
void sngisdn_rcv_sng_assert(char *message);

#ifdef NETBORDER_CALL_REF
ftdm_status_t get_callref(ftdm_channel_t *ftdmchan, BCCallRef* callRef);
#endif
ftdm_status_t get_calling_num(ftdm_channel_t *ftdmchan, CgPtyNmb *cgPtyNmb);
ftdm_status_t get_calling_num2(ftdm_channel_t *ftdmchan, CgPtyNmb *cgPtyNmb);
ftdm_status_t get_called_num(ftdm_channel_t *ftdmchan, CdPtyNmb *cdPtyNmb);
ftdm_status_t get_redir_num(ftdm_channel_t *ftdmchan, RedirNmb *redirNmb);
ftdm_status_t get_calling_name(ftdm_channel_t *ftdmchan, ConEvnt *conEvnt);
ftdm_status_t get_calling_subaddr(ftdm_channel_t *ftdmchan, CgPtySad *cgPtySad);
ftdm_status_t get_prog_ind_ie(ftdm_channel_t *ftdmchan, ProgInd *progInd);
ftdm_status_t get_facility_ie(ftdm_channel_t *ftdmchan, FacilityStr *facilityStr);
ftdm_status_t get_facility_ie_str(ftdm_channel_t *ftdmchan, uint8_t *data, uint8_t data_len);
ftdm_status_t get_network_specific_fac(ftdm_channel_t *ftdmchan, NetFac *netFac);

ftdm_status_t set_calling_num(ftdm_channel_t *ftdmchan, CgPtyNmb *cgPtyNmb);
ftdm_status_t set_calling_num2(ftdm_channel_t *ftdmchan, CgPtyNmb *cgPtyNmb);
ftdm_status_t set_called_num(ftdm_channel_t *ftdmchan, CdPtyNmb *cdPtyNmb);
ftdm_status_t set_redir_num(ftdm_channel_t *ftdmchan, RedirNmb *redirNmb);
ftdm_status_t set_calling_name(ftdm_channel_t *ftdmchan, ConEvnt *conEvnt);
ftdm_status_t set_calling_subaddr(ftdm_channel_t *ftdmchan, CgPtySad *cgPtySad);
ftdm_status_t set_called_subaddr(ftdm_channel_t *ftdmchan, CdPtySad *cdPtySad);
ftdm_status_t set_prog_ind_ie(ftdm_channel_t *ftdmchan, ProgInd *progInd, ftdm_sngisdn_progind_t prog_ind);
ftdm_status_t set_bear_cap_ie(ftdm_channel_t *ftdmchan, BearCap *bearCap);
ftdm_status_t set_network_specific_fac(ftdm_channel_t *ftdmchan, NetFac *netFac);
ftdm_status_t set_chan_id_ie(ftdm_channel_t *ftdmchan, ChanId *chanId);
ftdm_status_t set_restart_ind_ie(ftdm_channel_t *ftdmchan, RstInd *rstInd);
ftdm_status_t set_facility_ie(ftdm_channel_t *ftdmchan, FacilityStr *facilityStr);
ftdm_status_t set_facility_ie_str(ftdm_channel_t *ftdmchan, uint8_t *data, uint8_t *data_len);
ftdm_status_t set_user_to_user_ie(ftdm_channel_t *ftdmchan, UsrUsr *usrUsr);
ftdm_status_t set_cause_ie(ftdm_channel_t *ftdmchan, CauseDgn *causeDgn);
ftdm_status_t set_not_ind_ie(ftdm_channel_t *ftdmchan, NotInd *notInd);

ftdm_status_t sngisdn_add_var(sngisdn_chan_data_t *sngisdn_info, const char* var, const char* val);
ftdm_status_t sngisdn_add_raw_data(sngisdn_chan_data_t *sngisdn_info, uint8_t* data, ftdm_size_t data_len);
ftdm_status_t sngisdn_clear_data(sngisdn_chan_data_t *sngisdn_info);
void sngisdn_send_signal(sngisdn_chan_data_t *sngisdn_info, ftdm_signal_event_t event_id);

uint8_t sngisdn_get_infoTranCap_from_user(ftdm_bearer_cap_t bearer_capability);
uint8_t sngisdn_get_usrInfoLyr1Prot_from_user(ftdm_user_layer1_prot_t layer1_prot);
ftdm_bearer_cap_t sngisdn_get_infoTranCap_from_stack(uint8_t bearer_capability);
ftdm_user_layer1_prot_t sngisdn_get_usrInfoLyr1Prot_from_stack(uint8_t layer1_prot);

ftdm_status_t sngisdn_transfer(ftdm_channel_t *ftdmchan);
ftdm_status_t sngisdn_att_transfer_process_dtmf(ftdm_channel_t *ftdmchan, const char* dtmf);

static __inline__ uint32_t sngisdn_test_flag(sngisdn_chan_data_t *sngisdn_info, sngisdn_flag_t flag)
{
	return (uint32_t) sngisdn_info->flags & flag;
}
static __inline__ void sngisdn_clear_flag(sngisdn_chan_data_t *sngisdn_info, sngisdn_flag_t flag)
{
	sngisdn_info->flags &= ~flag;
}

static __inline__ void sngisdn_set_flag(sngisdn_chan_data_t *sngisdn_info, sngisdn_flag_t flag)
{
	sngisdn_info->flags |= flag;
}

#define sngisdn_set_trace_flag(obj, flag)   ((obj)->trace_flags |= (flag))
#define sngisdn_clear_trace_flag(obj, flag) ((obj)->trace_flags &= ~(flag))
#define sngisdn_test_trace_flag(obj, flag)  ((obj)->trace_flags & flag)


void handle_sng_log(uint8_t level, char *fmt,...);
void sngisdn_delayed_setup(void* p_sngisdn_info);
void sngisdn_delayed_release(void* p_sngisdn_info);
void sngisdn_delayed_release_nfas(void *p_sngisdn_info);
void sngisdn_delayed_connect(void* p_sngisdn_info);
void sngisdn_delayed_disconnect(void* p_sngisdn_info);
void sngisdn_facility_timeout(void* p_sngisdn_info);
void sngisdn_t3_timeout(void* p_sngisdn_info);
void sngisdn_restart_timeout(void* p_signal_data);
void sngisdn_delayed_dl_req(void* p_signal_data);

/* Stack management functions */
ftdm_status_t sngisdn_start_gen_cfg(void);
ftdm_status_t sngisdn_stack_cfg(ftdm_span_t *span);
ftdm_status_t sngisdn_stack_start(ftdm_span_t *span);
ftdm_status_t sngisdn_stack_stop(ftdm_span_t *span);
ftdm_status_t sngisdn_wake_up_phy(ftdm_span_t *span);
sngisdn_span_data_t *sngisdn_dchan(sngisdn_span_data_t *signal_data);

ftdm_status_t sngisdn_show_l1_stats(ftdm_stream_handle_t *stream, ftdm_span_t *span);
ftdm_status_t sngisdn_show_spans(ftdm_stream_handle_t *stream);
ftdm_status_t sngisdn_show_span(ftdm_stream_handle_t *stream, ftdm_span_t *span);

#endif /* __FTMOD_SNG_ISDN_H__ */

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

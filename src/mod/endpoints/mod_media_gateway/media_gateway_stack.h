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

#define MG_INACTIVITY_TMR_RESOLUTION   100          /* mit in ito package is experessed in 10ms steps */

/* rtp/avp profiles */
#define MG_RTP_AVP_PROFILE_A_LAW 8
#define MG_RTP_AVP_PROFILE_U_LAW 0


typedef enum{
	MG_SDP_NONE,
	MG_SDP_LOCAL,
	MG_SDP_REMOTE,
}mgco_sdp_types_e;


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

typedef enum{
	MG_TERM_SERVICE_STATE_UNKNOWN,
	MG_TERM_SERVICE_STATE_IN_SERVICE,
	MG_TERM_SERVICE_STATE_OUT_OF_SERVICE,
	MG_TERM_SERVICE_STATE_INVALID,
}mg_term_states_e;

#define PRNT_ENCODING_TYPE(_val)\
	((_val == SNG_MG_ENCODING_TEXT)?"SNG_MG_ENCODING_TEXT":\
	 (_val == SNG_MG_ENCODING_BINARY)?"SNG_MG_ENCODING_BINARY":\
	 "SNG_MG_ENCODING_NONE")

typedef struct _mgStr
{
   U8  len;
   U8  val[128];
} MgStr;

#define MAX_PACKAGE_NAME 128

typedef struct _mgPackage
{     
   uint8_t        package_id;  
   uint16_t       version;
   uint8_t        name[MAX_PACKAGE_NAME+1];
}MgPackage_t;

extern MgPackage_t mg_pkg_list[];

/* Service change Reason  */
typedef enum { 
   MG_SVC_REASON_NOTUSED = 0,
   MG_SVC_REASON_900_RESTORED = 1,
   MG_SVC_REASON_905_TERM_OOS = 2,
   MG_SVC_REASON_LAST = 4
} MgServiceChangeReason_e;

#define MG_TXN_INVALID 0

#define MG_ZERO(_buf, _size) {cmMemset((U8 *)(_buf), 0, _size);}

/* Set pres field */
#define MG_SET_PRES(_pres)           \
         (_pres) = PRSNT_NODEF; 
 
/* Set token value  */
#define MG_SET_VAL_PRES(tkn,_val)    \
   MG_SET_PRES((tkn).pres);          \
   (tkn).val = _val;                        

#define MG_SET_TKN_VAL_PRES(_tkn, _val, _pres)                       \
{                                                                   \
   (_tkn)->val  = _val;                                             \
   (_tkn)->pres = _pres;                                            \
}


#define MG_MEM_COPY(_dst, _src, _len) \
	cmMemcpy((U8*) (_dst), (const U8*) (_src), _len)


#define MG_STACK_MEM_ALLOC(_buf, _size)\
{\
	if (SGetSBuf(S_REG, S_POOL, (Data**) _buf, (Size) _size) == ROK){   \
	 cmMemset((U8 *) *(_buf), 0, _size); 				    \
	} else {							    \
		*(_buf) = NULLP; 					    \
	}                                 				    \
}

#define MG_STACK_MEM_FREE(_buf, _size)\
{\
	if(_buf != NULL){						    \
		(Void) SPutSBuf(S_REG, S_POOL, (Data *) _buf, (Size) _size);\
		(_buf) = NULL;						    \
	}								    \
}



#define MG_INIT_TOKEN_VALUE(_tkn, _val)                               \
{                                                                         \
   (_tkn)->pres = PRSNT_NODEF;                                            \
   (_tkn)->val  = (_val);                                                 \
}

#define MG_GETMEM(_ptr,_len,_memCp,_ret)                              \
{                                                                         \
        ret = cmGetMem((_memCp), (_len), (Ptr *)&(_ptr));                 \
        if( ret == ROK)                                                   \
          cmMemset((U8 *)(_ptr), (U8)0,  (PTR)(_len));                    \
}

#define MG_INIT_TKNSTR(_tkn, _val, _len)                              \
{                                                                         \
   (_tkn)->pres = PRSNT_NODEF;                                            \
   (_tkn)->len  = (U8)(_len);                                             \
   cmMemcpy((U8 *)(_tkn)->val, (CONSTANT U8 *)(_val), (_len));            \
}

#define MG_SET_TKNSTROSXL(_tkn, _len, _val, _mem)\
{\
   (_tkn).pres = PRSNT_NODEF;\
   (_tkn).len = (_len);\
   cmGetMem((Ptr)_mem, (Size)((_len)*sizeof(U8)), (Ptr*)&((_tkn).val));\
   cmMemcpy((U8*)((_tkn).val), (U8*)(_val), _len);\
}

#define MG_SET_DEF_REQID(_reqId)                \
   (_reqId)->type.pres = PRSNT_NODEF;               \
   (_reqId)->type.val = MGT_REQID_OTHER;            \
   (_reqId)->id.pres = PRSNT_NODEF;                 \
   (_reqId)->id.val = 0xFFFFFFFF;



void mg_set_term_ec_status(mg_termination_t* term, mg_ec_types_t status);
switch_status_t mg_prc_descriptors(megaco_profile_t* mg_profile, MgMgcoCommand *inc_cmd, mg_termination_t* term, CmMemListCp  *memCp);
void handle_sng_log(uint8_t level, char *fmt, ...);
void handle_mgco_sta_ind(Pst *pst, SuId suId, MgMgtSta* msg);
void handle_mgco_txn_sta_ind(Pst *pst, SuId suId, MgMgcoInd* msg);
void handle_mgco_cmd_ind(Pst *pst, SuId suId, MgMgcoCommand* msg);
void handle_mgco_cntrl_cfm(Pst *pst, SuId suId, MgMgtCntrl* cntrl, Reason reason); 
void handle_mgco_txn_ind(Pst *pst, SuId suId, MgMgcoMsg* msg);
void handle_mgco_audit_cfm(Pst *pst, SuId suId, MgMgtAudit* audit, Reason reason); 
void handle_mg_alarm(Pst *pst, MgMngmt *sta);
void handle_tucl_alarm(Pst *pst, HiMngmt *sta);
int mg_enable_logging(void);
int mg_disable_logging(void);
void mg_util_set_err_string ( MgStr  *errTxt, char* str);

switch_status_t mg_build_sdp(MgMgcoMediaDesc* out, MgMgcoMediaDesc* inc, megaco_profile_t* mg_profile, mg_termination_t* term, CmMemListCp     *memCp);
switch_status_t mg_add_local_descriptor(MgMgcoMediaDesc* media, megaco_profile_t* mg_profile, mg_termination_t* term, CmMemListCp  *memCp);
switch_status_t mg_send_term_service_change(char *span_name, char *chan_number, mg_term_states_e term_state); 


switch_status_t  mg_send_t38_cng_notify(megaco_profile_t* mg_profile, const char* term_name);
switch_status_t  mg_send_t38_ans_notify(megaco_profile_t* mg_profile, const char* term_name);
switch_status_t  mg_send_t38_v21flag_notify(megaco_profile_t* mg_profile, const char* term_name);


switch_status_t sng_mgco_cfg(megaco_profile_t* profile);
switch_status_t sng_mgco_init(sng_mg_event_interface_t* event);
switch_status_t sng_mgco_stack_shutdown(void);
int sng_mgco_mg_get_status(int elemId, MgMngmt* cfm, megaco_profile_t* mg_cfg, mg_peer_profile_t* mg_peer);

switch_status_t mg_is_ito_pkg_req(megaco_profile_t* mg_profile, MgMgcoCommand *cmd);
switch_status_t mg_send_end_of_axn(SuId suId, MgMgcoTransId* transId, MgMgcoContextId* ctxtId, TknU32* peerId);
void mgco_handle_incoming_sdp(CmSdpInfoSet *sdp,mg_termination_t* term, mgco_sdp_types_e sdp_type, megaco_profile_t* mg_profile, CmMemListCp     *memCp);
void mg_util_set_ctxt_string ( MgStr  *errTxt, MgMgcoContextId     *ctxtId);
switch_status_t handle_mg_add_cmd(megaco_profile_t* mg_profile, MgMgcoCommand *inc_cmd, MgMgcoContextId* new_ctxtId);
switch_status_t handle_mg_subtract_cmd(megaco_profile_t* mg_profile, MgMgcoCommand *inc_cmd);
switch_status_t handle_mg_modify_cmd(megaco_profile_t* mg_profile, MgMgcoCommand *cmd);
switch_status_t mg_stack_free_mem(void* msg);
switch_status_t mg_stack_alloc_mem( Ptr* _memPtr, Size _memSize );
MgMgcoMediaDesc* get_default_media_desc(megaco_profile_t* mg_profile, MgMgcoTermId* termId, CmMemListCp   *memCp);
switch_status_t handle_media_audit( SuId suId, MgMgcoCommand *auditReq);
switch_status_t mg_send_add_rsp(SuId suId, MgMgcoCommand *req);
S16 mg_fill_mgco_termid ( MgMgcoTermId  *termId, char* term_str, int term_len, CmMemListCp   *memCp);
void mg_util_set_txn_string(MgStr  *errTxt, U32 *txnId);
switch_status_t mg_build_mgco_err_request(MgMgcoInd  **errcmd,U32  trans_id, MgMgcoContextId   *ctxt_id, U32  err, MgStr  *errTxt);
switch_status_t mg_send_audit_rsp(SuId suId, MgMgcoCommand *req);
switch_status_t handle_mg_audit_cmd(megaco_profile_t* mg_profile, MgMgcoCommand *auditReq);
switch_status_t mg_stack_termination_is_in_service(megaco_profile_t* mg_profile, char* term_str, int len);
void mg_create_tdm_term(megaco_profile_t *profile, const char *tech, const char *channel_prefix, const char *prefix, int j, int k);
void mg_util_set_cmd_name_string (MgStr *errTxt, MgMgcoCommand       *cmd);
switch_status_t mgco_init_ins_service_change(SuId suId);

switch_status_t mg_send_modify_rsp(SuId suId, MgMgcoCommand *req);
switch_status_t mg_send_subtract_rsp(SuId suId, MgMgcoCommand *req);
void mg_util_set_term_string ( MgStr  *errTxt, MgMgcoTermId   *termId); 
MgMgcoTermIdLst *mg_get_term_id_list(MgMgcoCommand *cmd);
switch_status_t handle_pkg_audit( SuId suId, MgMgcoCommand *auditReq);
switch_status_t mg_build_pkg_desc(MgMgcoPkgsDesc* pkg, CmMemListCp  *memCp);
switch_status_t mg_send_heartbeat_audit_rsp( SuId suId, MgMgcoCommand *auditReq);
void mg_get_time_stamp(MgMgcoTimeStamp *timeStamp);
switch_status_t  mg_fill_svc_change(MgMgcoSvcChgPar  *srvPar, uint8_t  method, const char  *reason,CmMemListCp   *memCp);
void mg_fill_null_context(MgMgcoContextId* ctxt);
switch_status_t  mg_send_service_change(SuId suId, const char* term_name, uint8_t method, MgServiceChangeReason_e reason,uint8_t wild); 
switch_status_t  mg_create_mgco_command(MgMgcoCommand  *cmd, uint8_t apiType, uint8_t cmdType);
switch_status_t mg_add_lcl_media(CmSdpMediaDescSet* med, megaco_profile_t* mg_profile, mg_termination_t* term, CmMemListCp     *memCp);
switch_status_t mg_add_supported_media_codec(CmSdpMediaDesc* media, megaco_profile_t* mg_profile, mg_termination_t* term, CmMemListCp     *memCp);
switch_status_t mg_rem_unsupported_codecs (megaco_profile_t* mg_profile, mg_termination_t* term, CmSdpMedFmtRtpList  *fmtList, CmSdpAttrSet  *attrSet, CmMemListCp     *memCp);

switch_status_t mg_send_oos_service_change(megaco_profile_t* mg_profile, const char* term_name, int wild);
switch_status_t mg_send_ins_service_change(megaco_profile_t* mg_profile, const char* term_name, int wild);
switch_status_t  mg_send_notify(megaco_profile_t* mg_profile, const char* term_name, MgMgcoObsEvt* oevt);
switch_status_t  mg_send_t38_fax_con_change_notify(megaco_profile_t* mg_profile, const char* term_name, char* state);
switch_status_t  mg_send_dtmf_notify(megaco_profile_t* mg_profile, const char* term_name, char* digits, int num_of_collected_digits);
switch_status_t  mg_send_ito_notify(megaco_profile_t* mg_profile);
void mg_print_t38_attributes(mg_termination_t* term);
switch_status_t  mg_util_build_obs_evt_desc (MgMgcoObsEvt *obs_event, MgMgcoRequestId *request_id, MgMgcoObsEvtDesc **ptr_obs_desc);
void mg_print_time();
switch_status_t mg_activate_ito_timer(megaco_profile_t* profile);

void mg_restart_inactivity_timer(megaco_profile_t* profile);

switch_status_t mgco_process_mgc_failure(SuId suId);
void mg_apply_tdm_dtmf_removal(mg_termination_t* term, mg_context_t *mg_ctxt);
void mg_apply_tdm_ec(mg_termination_t* term, mg_context_t *mg_ctxt);


/****************************************************************************************************************/
/* MG Stack defines */
   
/* Free Commands inside MG CH command */
#define mg_free_cmd(_cmd)   mgFreeEventMem(_cmd)

/****************************************************************************************************************/

#endif /* _MEGACO_STACK_H_ */

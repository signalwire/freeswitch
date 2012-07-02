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


#define MG_TXN_INVALID 0

#define MG_ZERO(_buf, _size) {cmMemset((U8 *)(_buf), 0, _size);}

/* Set pres field */
#define MG_SET_PRES(_pres)           \
         (_pres) = PRSNT_NODEF; 
 
/* Set token value  */
#define MG_SET_VAL_PRES(tkn,_val)    \
   MG_SET_PRES((tkn).pres);          \
   (tkn).val = _val;                        


#define MG_MEM_COPY(_dst, _src, _len) \
	cmMemcpy((U8*) (_dst), (const U8*) (_src), _len)


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


switch_status_t sng_mgco_cfg(megaco_profile_t* profile);
switch_status_t sng_mgco_init(sng_mg_event_interface_t* event);
switch_status_t sng_mgco_stack_shutdown(void);
int sng_mgco_mg_get_status(int elemId, MgMngmt* cfm, megaco_profile_t* mg_cfg, mg_peer_profile_t* mg_peer);

switch_status_t mg_send_end_of_axn(SuId suId, MgMgcoTransId* transId, MgMgcoContextId* ctxtId, TknU32* peerId);
void mgco_print_sdp(CmSdpInfoSet *sdp);
void mg_util_set_ctxt_string ( MgStr  *errTxt, MgMgcoContextId     *ctxtId);
switch_status_t handle_mg_add_cmd(MgMgcoAmmReq *addReq);
switch_status_t mg_stack_free_mem(MgMgcoMsg* msg);
switch_status_t mg_stack_free_mem(MgMgcoMsg* msg);
switch_status_t mg_stack_alloc_mem( Ptr* _memPtr, Size _memSize );
switch_status_t mg_send_add_rsp(SuId suId, MgMgcoCommand *req);
S16 mg_fill_mgco_termid ( MgMgcoTermId  *termId, CONSTANT U8   *str, CmMemListCp   *memCp);
void mg_util_set_txn_string(MgStr  *errTxt, U32 *txnId);
switch_status_t mg_build_mgco_err_request(MgMgcoInd  **errcmd,U32  trans_id, MgMgcoContextId   *ctxt_id, U32  err, MgStr  *errTxt);
switch_status_t mg_send_audit_rsp(SuId suId, MgMgcoCommand *req);
switch_status_t handle_mg_audit_cmd(SuId suId, MgMgcoCommand *auditReq);

switch_status_t mg_send_modify_rsp(SuId suId, MgMgcoCommand *req);
switch_status_t mg_send_subtract_rsp(SuId suId, MgMgcoCommand *req);
void mg_util_set_term_string ( MgStr  *errTxt, MgMgcoTermId   *termId); 
MgMgcoTermIdLst *mg_get_term_id_list(MgMgcoCommand *cmd);
switch_status_t handle_pkg_audit( SuId suId, MgMgcoCommand *auditReq);
switch_status_t mg_build_pkg_desc(MgMgcoPkgsDesc* pkg);



/****************************************************************************************************************/
/* MG Stack defines */
   
/* Free Commands inside MG CH command */
#define mg_free_cmd(_cmd)   mgFreeEventMem(_cmd)

/****************************************************************************************************************/

#endif /* _MEGACO_STACK_H_ */

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

#include "sng_sit.h"
#include "sng_ss7.h"
#include "sng_ss7_error.h"
/******************************************************************************/

/* DEFINES ********************************************************************/
#define MAX_CIC_LENGTH      5
#define MAX_CIC_MAP_LENGTH  256 
#define MAX_MTP_LINKS       MAX_SN_LINKSETS

#if 0
#define SS7_HARDCODED
#endif
#define SNG_BASE    1

typedef enum {
    SNG_IAM = 1,
    SNG_ACM,
    SNG_CPG,
    SNG_ANM,
    SNG_REL,
    SNG_RLC
}sng_msg_type_t;

typedef struct ftdm_sngss7_data {
    sng_config_t        cfg;
    int                 gen_config_done;
    int                 min_digits;
    int                 function_trace;
    int                 function_trace_level;
    int                 message_trace;
    int                 message_trace_level;
    fio_signal_cb_t     sig_cb;
}ftdm_sngss7_data_t;

typedef struct sngss7_timer_data {
    ftdm_timer_t            *heartbeat_timer;
    int                     beat;
    int                     counter;
    ftdm_sched_callback_t   callback;
    ftdm_sched_t            *sched;
    void                    *sngss7_info;
}sngss7_timer_data_t;

typedef struct sngss7_glare_data {
    uint32_t                suInstId; 
    uint32_t                spInstId;
    uint32_t                circuit; 
    SiConEvnt               iam;
}sngss7_glare_data_t;

typedef struct sngss7_chan_data {
    ftdm_channel_t          *ftdmchan;
    sng_isupCircuit_t       *circuit;
    uint32_t                suInstId;
    uint32_t                spInstId;
    uint32_t                spId;
    uint8_t                 globalFlg;
    uint32_t                flags;
    sngss7_glare_data_t     glare;
    sngss7_timer_data_t     t35;
}sngss7_chan_data_t;



typedef enum {
    FLAG_RESET_RX           = (1 << 0),
    FLAG_RESET_TX           = (1 << 1),
    FLAG_REMOTE_REL         = (1 << 2),
    FLAG_LOCAL_REL          = (1 << 3),
    FLAG_GLARE              = (1 << 4),
    FLAG_INFID_RESUME       = (1 << 17),
    FLAG_INFID_PAUSED       = (1 << 18),
    FLAG_CKT_MN_BLOCK_RX    = (1 << 19),
    FLAG_CKT_MN_BLOCK_TX    = (1 << 20),
    FLAG_CKT_MN_UNBLK_RX    = (1 << 21),
    FLAG_CKT_MN_UNBLK_TX    = (1 << 22),
    FLAG_GRP_HW_BLOCK_RX    = (1 << 23),
    FLAG_GRP_HW_BLOCK_TX    = (1 << 24),
    FLAG_GRP_MN_BLOCK_RX    = (1 << 25),
    FLAG_GRP_MN_BLOCK_TX    = (1 << 28),
    FLAG_GRP_HW_UNBLK_RX    = (1 << 27),
    FLAG_GRP_HW_UNBLK_TX    = (1 << 28),
    FLAG_GRP_MN_UNBLK_RX    = (1 << 29),
    FLAG_GRP_MN_UNBLK_TX    = (1 << 30)
} flag_t;
/******************************************************************************/

/* GLOBALS ********************************************************************/
extern ftdm_sngss7_data_t   g_ftdm_sngss7_data;
extern uint32_t             sngss7_id;
extern ftdm_sched_t         *sngss7_sched;
/******************************************************************************/

/* PROTOTYPES *****************************************************************/
extern void handle_sng_log(uint8_t level, char *fmt,...);
extern void handle_sng_alarm(sng_alrm_t t_alarm);

extern int  ft_to_sngss7_cfg(void);
extern int  ft_to_sngss7_activate_all(void);

extern void ft_to_sngss7_iam(ftdm_channel_t *ftdmchan);
extern void ft_to_sngss7_acm(ftdm_channel_t *ftdmchan);
extern void ft_to_sngss7_anm(ftdm_channel_t *ftdmchan);
extern void ft_to_sngss7_rel(ftdm_channel_t *ftdmchan);
extern void ft_to_sngss7_rlc(ftdm_channel_t *ftdmchan);
extern void ft_to_sngss7_rsc(ftdm_channel_t *ftdmchan);
extern void ft_to_sngss7_rsca(ftdm_channel_t *ftdmchan);
extern void ft_to_sngss7_blo(ftdm_channel_t *ftdmchan);
extern void ft_to_sngss7_bla(ftdm_channel_t *ftdmchan);
extern void ft_to_sngss7_ubl(ftdm_channel_t *ftdmchan);
extern void ft_to_sngss7_uba(ftdm_channel_t *ftdmchan);
extern void ft_to_sngss7_lpa(ftdm_channel_t *ftdmchan);

extern void sngss7_sta_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
extern void sngss7_con_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiConEvnt *siConEvnt);
extern void sngss7_con_cfm(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiConEvnt *siConEvnt);
extern void sngss7_con_sta(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiCnStEvnt *siCnStEvnt, uint8_t evntType);
extern void sngss7_rel_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiRelEvnt *siRelEvnt);
extern void sngss7_rel_cfm(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiRelEvnt *siRelEvnt);
extern void sngss7_dat_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, SiInfoEvnt *siInfoEvnt);
extern void sngss7_fac_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t evntType, SiFacEvnt *siFacEvnt);
extern void sngss7_fac_cfm(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t evntType, SiFacEvnt *siFacEvnt);
extern void sngss7_sta_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit, uint8_t globalFlg, uint8_t evntType, SiStaEvnt *siStaEvnt);
extern void sngss7_umsg_ind(uint32_t suInstId, uint32_t spInstId, uint32_t circuit);

extern uint8_t copy_cgPtyNum_from_sngss7(ftdm_caller_data_t *ftdm, SiCgPtyNum *cgPtyNum);
extern uint8_t copy_cgPtyNum_to_sngss7(ftdm_caller_data_t *ftdm, SiCgPtyNum *cgPtyNum);
extern uint8_t copy_cdPtyNum_from_sngss7(ftdm_caller_data_t *ftdm, SiCdPtyNum *cdPtyNum);
extern uint8_t copy_cdPtyNum_to_sngss7(ftdm_caller_data_t *ftdm, SiCdPtyNum *cdPtyNum);
extern uint8_t copy_tknStr_from_sngss7(TknStr str, char *ftdm, TknU8 oddEven);
extern int check_for_state_change(ftdm_channel_t *ftdmchan);
extern ftdm_status_t extract_chan_data(uint32_t circuit, sngss7_chan_data_t **sngss7_info, ftdm_channel_t **ftdmchan);
extern unsigned long get_unique_id(void);

extern int ftmod_ss7_parse_xml(ftdm_conf_parameter_t *ftdm_parameters, ftdm_span_t *span);

extern void handle_isup_t35(void *userdata);

extern ftdm_status_t ftdm_sngss7_handle_cli_cmd(ftdm_stream_handle_t *stream, const char *data);
/******************************************************************************/

/* MACROS *********************************************************************/
#define SS7_DEBUG(a,...)    ftdm_log(FTDM_LOG_DEBUG,a,##__VA_ARGS__ );
#define SS7_INFO(a,...)     ftdm_log(FTDM_LOG_INFO,a,##__VA_ARGS__ );
#define SS7_WARN(a,...)     ftdm_log(FTDM_LOG_WARNING,a,##__VA_ARGS__ );
#define SS7_ERROR(a,...)    ftdm_log(FTDM_LOG_ERROR,a,##__VA_ARGS__ );
#define SS7_CRITICAL(a,...) ftdm_log(FTDM_LOG_CRIT,a,##__VA_ARGS__ );

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

#define SS7_MSG_TRACE(a,...) if (g_ftdm_sngss7_data.message_trace) { \
                                    switch (g_ftdm_sngss7_data.message_trace_level) { \
                                        case 0: \
                                            ftdm_log(FTDM_LOG_EMERG,a,##__VA_ARGS__ ); \
                                            break; \
                                        case 1: \
                                            ftdm_log(FTDM_LOG_ALERT,a,##__VA_ARGS__ ); \
                                            break; \
                                        case 2: \
                                            ftdm_log(FTDM_LOG_CRIT,a,##__VA_ARGS__ ); \
                                            break; \
                                        case 3: \
                                            ftdm_log(FTDM_LOG_ERROR,a,##__VA_ARGS__ ); \
                                            break; \
                                        case 4: \
                                            ftdm_log(FTDM_LOG_WARNING,a,##__VA_ARGS__ ); \
                                            break; \
                                        case 5: \
                                            ftdm_log(FTDM_LOG_NOTICE,a,##__VA_ARGS__ ); \
                                            break; \
                                        case 6: \
                                            ftdm_log(FTDM_LOG_INFO,a,##__VA_ARGS__ ); \
                                            break; \
                                        case 7: \
                                            ftdm_log(FTDM_LOG_DEBUG,a,##__VA_ARGS__ ); \
                                            break; \
                                        default: \
                                            ftdm_log(FTDM_LOG_INFO,a,##__VA_ARGS__ ); \
                                            break; \
                                        } /* switch (g_ftdm_sngss7_data.message_trace_level) */ \
                                } /* if(g_ftdm_sngss7_data.message_trace) */

#define sngss7_test_flag(obj, flag)  ((obj)->flags & flag)
#define sngss7_clear_flag(obj, flag) ((obj)->flags &= ~(flag))
#define sngss7_set_flag(obj, flag)   ((obj)->flags |= (flag))

/******************************************************************************/

/******************************************************************************/
#endif /* __FTMOD_SNG_SS7_H__ */
/******************************************************************************/

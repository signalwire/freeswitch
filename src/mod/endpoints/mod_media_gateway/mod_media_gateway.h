/*
* Copyright (c) 2012, Sangoma Technologies
* Mathieu Rene <mrene@avgs.ca>
* All rights reserved.
* 
* <Insert license here>
*/


#ifndef MOD_MEGACO_H
#define MOD_MEGACO_H

#include "sng_mg/sng_mg.h"
#include <switch.h>

#define MG_MAX_PEERS    5


#define MG_CONTEXT_MAX_TERMS 3

#define MEGACO_CLI_SYNTAX "profile|logging"
#define MEGACO_LOGGING_CLI_SYNTAX "logging [enable|disable]"
#define MEGACO_FUNCTION_SYNTAX "profile [name] [start | stop] [status] [xmlstatus] [peerxmlstatus]"

#define PVT_MG_TERM "_mg_term_"

struct megaco_globals {
	switch_memory_pool_t 		*pool;
	switch_hash_t 			*profile_hash;
	switch_hash_t 			*peer_profile_hash;
	switch_thread_rwlock_t 		*profile_rwlock;
	switch_thread_rwlock_t 		*peer_profile_rwlock;
};
extern struct megaco_globals megaco_globals; /* < defined in mod_megaco.c */

typedef enum {
	PF_RUNNING = (1 << 0)
} megaco_profile_flags_t;

typedef enum {
	MEGACO_CODEC_PCMA,
	MEGACO_CODEC_PCMU,
	MEGACO_CODEC_G729,
	MEGACO_CODEC_G723_1,
	MEGACO_CODEC_ILBC,
    
    /* Nothing below this line */
    MEGACO_CODEC_INVALID = 0xFFFFFF
} megaco_codec_t;

typedef struct mg_peer_profile_s{
	char 				*name;
	switch_memory_pool_t 		*pool;
	switch_thread_rwlock_t 		*rwlock; /* < Reference counting rwlock */
	megaco_profile_flags_t 		flags;
	char*  				ipaddr;      /* Peer IP  */
  	char* 				port;        /*Peer Port */
	char*       			mid;  	     /* Peer H.248 MID */
	char*       			transport_type; /* UDP/TCP */ 
	char*      			encoding_type; /* Encoding TEXT/Binary */
} mg_peer_profile_t;


typedef struct mg_stats_s{
	uint32_t  total_num_of_phy_add_recvd;
	uint32_t  total_num_of_rtp_add_recvd;
	uint32_t  total_num_of_sub_recvd;
	uint32_t  total_num_of_call_recvd;
	uint32_t  total_num_of_add_failed;
	uint32_t  total_num_of_term_already_in_ctxt_error;
	uint32_t  total_num_of_choose_ctxt_failed_error;
	uint32_t  total_num_of_choose_term_failed_error;
	uint32_t  total_num_of_find_term_failed_error;
	uint32_t  total_num_of_get_ctxt_failed_error;
	uint32_t  total_num_of_un_supported_codec_error;
	uint32_t  total_num_of_add_term_failed_error;
	uint32_t  total_num_of_term_activation_failed_error;
	uint32_t  total_num_of_no_term_ctxt_error;
	uint32_t  total_num_of_term_not_in_service_error;
	uint32_t  total_num_of_unknown_ctxt_error;
}mg_stats_t;


typedef enum {
    MG_TERM_FREE = 0,
    MG_TERM_TDM,
    MG_TERM_RTP
} mg_termination_type_t;

typedef struct megaco_profile_s megaco_profile_t;
typedef struct mg_context_s mg_context_t;

/* RTP parameters understood by the controllable channel */
#define kLOCALADDR "local_addr"
#define kLOCALPORT "local_port"
#define kREMOTEADDR "remote_addr"
#define kREMOTEPORT "remote_port"
#define kCODEC "codec"
#define kPTIME "ptime"
#define kPT "pt"
#define kRFC2833PT "rfc2833_pt"
#define kMODE "mode"
#define kRATE "rate"
#define kMEDIATYPE "media_type"

/* TDM parameters understood by the controllable channel */
#define kSPAN_ID "span"
#define kCHAN_ID "chan"
#define kSPAN_NAME "span_name"
#define kPREBUFFER_LEN "prebuffer_len"
#define kECHOCANCEL "echo_cancel"

typedef struct mg_termination_s mg_termination_t;

enum {
    MGT_ALLOCATED 	= (1 << 0),
    MGT_ACTIVE    	= (1 << 1),
    MG_IN_SERVICE 	= (1 << 2),
    MG_OUT_OF_SERVICE 	= (1 << 3),
    
} mg_termination_flags;


typedef enum {
    MGM_AUDIO = 0,
    MGM_IMAGE,
    MGM_INVALID
} mg_media_type_t;

static inline const char *mg_media_type2str(mg_media_type_t type) {
    switch (type) {
        case MGM_AUDIO:
            return "audio";
        case MGM_IMAGE:
            return "image";
        case MGM_INVALID:
            return NULL;
    }
    return NULL;
}

static inline mg_media_type_t mg_media_type_parse(const char *str) {
    if (!strcasecmp(str, "audio")) {
        return MGM_AUDIO;
    } else if (!strcasecmp(str, "image")) {
        return MGM_IMAGE;
    }
    return MGM_INVALID;
}

struct mg_context_s {
    uint32_t context_id;
    mg_termination_t *terminations[MG_CONTEXT_MAX_TERMS];
    megaco_profile_t *profile;
    mg_context_t *next;
    switch_memory_pool_t *pool;
};

struct mg_termination_s {
    switch_memory_pool_t *pool;
    mg_termination_type_t type;
    const char *name; /*!< Megaco Name */    
    const char *uuid; /*!< UUID of the associated FS channel, or NULL if it's not activated */
    mg_context_t *context; /*!< Context in which this termination is connected, or NULL */
    megaco_profile_t *profile; /*!< Parent MG profile */
    MgMgcoReqEvtDesc  *active_events;     /* !< active megaco events */
    mg_termination_t *next; /*!< List for physical terminations */
    int  *mg_error_code; /* MEGACO error code */
    uint32_t flags;
    const char *tech; /* Endpoint controlling the TDM interface - only FreeTDM tested so far */
    
    union {
        struct {
            /* The RTP termination will automatically operate as "sendonly" or "recvonly" as soon as
             * one of the network addresses are NULL */
            const char *local_addr; /*!< RTP Session's Local IP address  */
            switch_port_t local_port; /*!< RTP Session's Local IP address  */
            
            const char *remote_addr; /*!< RTP Session's Remote IP address  */
            switch_port_t remote_port; /*!< RTP Session's Remote IP address  */

            int ptime;  /*!< Packetization Interval, in miliseconds. The default is 20, but it has to be set */
            int pt;     /*!< Payload type */
            int rfc2833_pt; /*!< If the stream is using rfc2833 for dtmf events, this has to be set to its negotiated payload type */
            int rate;       /*!< Sampling rate */
            const char *codec; /*!< Codec to use, using the freeswitch nomenclature. This could be "PCMU" for G711.U, "PCMA" for G711.A, or "G729" for g729 */
            int term_id;
            switch_t38_options_t *t38_options;
            mg_media_type_t media_type;
        } rtp;
        
        struct {
            int channel;
            const char *span_name;
        } tdm;
    } u;
};




#define MG_CONTEXT_MODULO 16
#define MG_MAX_CONTEXTS 32768
#define MG_MAX_RTPID 32768


struct megaco_profile_s {
	char 				*name;
	switch_memory_pool_t 	*pool;
	switch_thread_rwlock_t 	*rwlock; /* < Reference counting rwlock */
	megaco_profile_flags_t 	flags;
	int 					idx;         /* Trillium MEGACO SAP identification*/
	char*					mid;  	     /* MG H.248 MID */
	char*					my_domain;   /* local domain name */
	char*					my_ipaddr;   /* local domain name */
	char*					port;              	     /* port */
	char*					protocol_type;    	     /* MEGACO/MGCP */
	int 					protocol_version;            /* Protocol supported version */
	int 					total_peers;
	megaco_codec_t			default_codec;
	char*					rtp_port_range;
	char*					rtp_termination_id_prefix;
	int						rtp_termination_id_len;
    int                     tdm_pre_buffer_size;
	char*                	peer_list[MG_MAX_PEERS];     /* MGC Peer ID LIST */
    char*                   codec_prefs;
	int						inact_tmr;                   /* inactivity timer value */
	int						peer_active;                   /* inactivity timer value */
    uint32_t                inact_tmr_task_id;                 /* FS timer scheduler task-id */
    
    switch_thread_rwlock_t  *contexts_rwlock;
    uint32_t next_context_id;
    uint8_t contexts_bitmap[MG_MAX_CONTEXTS/8]; /* Availability matrix, enough bits for a 32768 bitmap */    
    mg_context_t *contexts[MG_CONTEXT_MODULO];
    
    uint8_t rtpid_bitmap[MG_MAX_CONTEXTS/8];
    uint32_t rtpid_next;
    
    mg_termination_t *physical_terminations;
    mg_stats_t* mg_stats;
    
    switch_hash_t *terminations;
    switch_thread_rwlock_t *terminations_rwlock;
};



static inline const char *megaco_codec_str(megaco_codec_t codec)
{
    switch (codec) {
        case MEGACO_CODEC_PCMU:
            return "PCMU";
        case MEGACO_CODEC_PCMA:
            return "PCMA";
        case MEGACO_CODEC_G729:
            return "G729";
        case MEGACO_CODEC_G723_1:
            return "G723"; /* XXX double check this */
        case MEGACO_CODEC_ILBC:
            return "ILBC";
        case MEGACO_CODEC_INVALID:
        default:
            return NULL;
    }
}

static inline megaco_codec_t megaco_codec_parse(const char *codec) {
    if (!strcasecmp(codec, "PCMU")) {
        return MEGACO_CODEC_PCMU;
    } else if (!strcasecmp(codec, "PCMA")) {
        return MEGACO_CODEC_PCMA;
    } else if (!strcasecmp(codec, "G729")) {
        return MEGACO_CODEC_G729;
    } else if (!strcasecmp(codec, "G723")) {
        return MEGACO_CODEC_G723_1;
    } else if (!strcasecmp(codec, "ILBC")) {
        return MEGACO_CODEC_ILBC;
    } else {
        return MEGACO_CODEC_INVALID;
    }
}


megaco_profile_t *megaco_profile_locate(const char *name);
mg_termination_t *megaco_term_locate_by_span_chan_id(const char *span_name, const char *chan_number);
mg_peer_profile_t *megaco_peer_profile_locate(const char *name);
void megaco_profile_release(megaco_profile_t *profile);
mg_termination_t* megaco_find_termination_by_span_chan(megaco_profile_t *profile, const char *span_name, const char *chan_number);

switch_status_t megaco_start_all_profiles(void);
switch_status_t megaco_profile_start(const char *profilename);
switch_status_t megaco_profile_destroy(megaco_profile_t **profile);

uint32_t mg_rtp_request_id(megaco_profile_t *profile);
void mg_rtp_release_id(megaco_profile_t *profile, uint32_t id);
void mg_term_set_pre_buffer_size(mg_termination_t *term, int newval);
void mg_term_set_ec(mg_termination_t *term, int enable);

mg_context_t *megaco_get_context(megaco_profile_t *profile, uint32_t context_id);
mg_context_t *megaco_choose_context(megaco_profile_t *profile);
void megaco_release_context(mg_context_t *ctx);
switch_status_t megaco_context_sub_termination(mg_context_t *ctx, mg_termination_t *term);
switch_status_t megaco_context_sub_all_termination(mg_context_t *ctx);
switch_status_t megaco_activate_termination(mg_termination_t *term);
switch_status_t megaco_prepare_termination(mg_termination_t *term);

mg_termination_t *megaco_choose_termination(megaco_profile_t *profile, const char *prefix);
mg_termination_t *megaco_find_termination(megaco_profile_t *profile, const char *name);
void megaco_termination_destroy(mg_termination_t *term);

megaco_profile_t*  megaco_get_profile_by_suId(SuId suId);
mg_context_t *megaco_find_context_by_suid(SuId suId, uint32_t context_id);

switch_status_t config_profile(megaco_profile_t *profile, switch_bool_t reload);
switch_status_t sng_mgco_start(megaco_profile_t* profile);
switch_status_t sng_mgco_stop(megaco_profile_t* profile);
switch_status_t mg_config_cleanup(megaco_profile_t* profile);
switch_status_t mg_peer_config_cleanup(mg_peer_profile_t* profile);
switch_status_t megaco_peer_profile_destroy(mg_peer_profile_t **profile); 
switch_status_t mg_process_cli_cmd(const char *cmd, switch_stream_handle_t *stream);
switch_status_t megaco_context_add_termination(mg_context_t *ctx, mg_termination_t *term);
switch_status_t megaco_context_is_term_present(mg_context_t *ctx, mg_termination_t *term);

switch_status_t megaco_prepare_tdm_termination(mg_termination_t *term);
switch_status_t megaco_check_tdm_termination(mg_termination_t *term);



#endif /* MOD_MEGACO_H */


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

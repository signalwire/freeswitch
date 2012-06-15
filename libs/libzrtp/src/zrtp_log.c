/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */

#include "zrtp.h"

#if (ZRTP_PLATFORM == ZP_WIN32_KERNEL)
#include <ndis.h>
#include <ntstrsafe.h>
#endif

#if ZRTP_LOG_MAX_LEVEL >= 1

/*----------------------------------------------------------------------------*/
#if defined ZRTP_HAVE_STDIO_H
#	include <stdio.h>
#endif
#if defined ZRTP_HAVE_STRING_H
#	include <string.h>
#endif
#if defined ZRTP_HAVE_STDARG_H
#	include <stdarg.h>
#endif

static const char* k_unknown = "UNKNOWN";

#if ZRTP_PLATFORM != ZP_WIN32_KERNEL
void zrtp_def_log_write(int level, char *buffer, int len, int offset) {
    printf("%s", buffer);
}

static zrtp_log_engine *log_writer = &zrtp_def_log_write;
#else
static zrtp_log_engine *log_writer = NULL;
#endif

static uint32_t log_max_level = ZRTP_LOG_MAX_LEVEL;


/*----------------------------------------------------------------------------*/
void zrtp_log_set_level(uint32_t level) {
	log_max_level = level;
}

void zrtp_log_set_log_engine(zrtp_log_engine *engine) {
    log_writer = engine;
}

/*----------------------------------------------------------------------------*/
static void zrtp_log(uint8_t is_clean, const char *sender, uint32_t level,  const char *format, va_list marker)
{ 	
#if (defined(ZRTP_USE_STACK_MINIM) && (ZRTP_USE_STACK_MINIM == 1))
	char *log_buffer = zrtp_sys_alloc(ZRTP_LOG_BUFFER_SIZE);
#else
	char log_buffer[ZRTP_LOG_BUFFER_SIZE];
#endif
	char* sline = log_buffer;
	uint32_t offset = 0;
	int len = 0;
	
	if (!sline) {
		return;
	}
	
	if (!is_clean) {
		/* Print sender with left aligment */	
		uint32_t sender_len = strlen(sender);
		*sline++ = ' ';
		*sline++ = '[';
		if (sender_len <= ZRTP_LOG_SENDER_MAX_LEN) {
			while (sender_len < ZRTP_LOG_SENDER_MAX_LEN) {
				*sline++ = ' ', ++sender_len;
			}
			while (*sender) {
				*sline++ = *sender++;
			}
		} else {
			int i = 0;
			for (i=0; i<ZRTP_LOG_SENDER_MAX_LEN; ++i) {
				*sline++ = *sender++;
			}
		}
		
		*sline++ = ']';
		*sline++ = ':';
		offset += 3 + ZRTP_LOG_SENDER_MAX_LEN;
			
		*sline++ = ' ';
		offset += 1; 
	}
	
	/* Print Message itself */
#if (ZRTP_PLATFORM == ZP_WIN32) || (ZRTP_PLATFORM == ZP_WIN64) || (ZRTP_PLATFORM == ZP_WINCE)
#	if (_MSC_VER >= 1400) && (ZRTP_PLATFORM != ZP_WINCE)
	len = _vsnprintf_s(sline, ZRTP_LOG_BUFFER_SIZE-offset-1, ZRTP_LOG_BUFFER_SIZE-offset-1, format, marker);
#	else
	len = _vsnprintf(sline, ZRTP_LOG_BUFFER_SIZE-offset, format, marker);
#	endif
#elif (ZRTP_PLATFORM == ZP_WIN32_KERNEL)
	RtlStringCchVPrintfA(sline, ZRTP_LOG_BUFFER_SIZE-offset, format, marker);
#elif (ZRTP_PLATFORM == ZP_LINUX) || (ZRTP_PLATFORM == ZP_DARWIN) || (ZRTP_PLATFORM == ZP_BSD) || (ZRTP_PLATFORM == ZP_ANDROID)
	len = vsnprintf(sline, ZRTP_LOG_BUFFER_SIZE-offset, format, marker);
#elif (ZRTP_PLATFORM == ZP_SYMBIAN)
	len = vsprintf(sline, format, marker);
#endif

	if ((len > 0) && log_writer) {
		(*log_writer)(level, log_buffer, len+offset, offset);
	}

#if (defined(ZRTP_USE_STACK_MINIM) && (ZRTP_USE_STACK_MINIM == 1))
	zrtp_sys_free(log_buffer);
#endif
}


#if ZRTP_LOG_MAX_LEVEL >= 1
void zrtp_log_1(const char *obj, const char *format, ...)
{	
    va_list arg;
    va_start(arg, format);
    zrtp_log(0, obj, 1, format, arg);
    va_end(arg);
}
void zrtp_logc_1(const char *format, ...)
{	
    va_list arg;
    va_start(arg, format);
    zrtp_log(1, NULL, 1, format, arg);
    va_end(arg);
}

#endif

#if ZRTP_LOG_MAX_LEVEL >= 2
void zrtp_log_2(const char *obj, const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    zrtp_log(0, obj, 2, format, arg);
    va_end(arg);
}
void zrtp_logc_2(const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    zrtp_log(1, NULL, 2, format, arg);
    va_end(arg);
}

#endif

#if ZRTP_LOG_MAX_LEVEL >= 3
void zrtp_log_3(const char *obj, const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    zrtp_log(0, obj, 3, format, arg);
    va_end(arg);
}
void zrtp_logc_3(const char *format, ...)
{
    va_list arg;
    va_start(arg, format);
    zrtp_log(1, NULL, 3, format, arg);
    va_end(arg);
}

#endif

#endif

/*---------------------------------------------------------------------------*/
struct _error_strings_t
{
	zrtp_protocol_error_t code;
	char*				  descr;
};

static const struct _error_strings_t _error_strings[] = {
	{zrtp_error_unknown,		"Unknown"},
	{zrtp_error_timeout,		"Protocol Packets Retries Timeout"},
	{zrtp_error_invalid_packet,	"Malformed packet (CRC OK, but wrong structure)"},
	{zrtp_error_software,		"Critical software error: no memory, can't call some system function, etc"},
	{zrtp_error_version,		"Unsupported ZRTP version"},
	{zrtp_error_hello_mistmatch,"Hello components mismatch "},

	{zrtp_error_hash_unsp,		"Hash type not supported"},
	{zrtp_error_cipher_unsp,	"Cipher type not supported"},
	{zrtp_error_pktype_unsp,	"Public key exchange not supported"},
	{zrtp_error_auth_unsp,		"SRTP auth. tag not supported"},
	{zrtp_error_sas_unsp,		"SAS scheme not supported"},
	{zrtp_error_no_secret,		"No shared secret available, DH mode required"},

	{zrtp_error_possible_mitm1,	"Attack DH Error: bad pvi or pvr ( == 1, 0, or p-1)"},
	{zrtp_error_possible_mitm2,	"Attack DH Error: hvi != hashed data"},
	{zrtp_error_possible_mitm3,	"Attack Received relayed SAS from untrusted MiTM"},

	{zrtp_error_auth_decrypt,	"Auth. Error: Bad Confirm pkt HMAC"},
	{zrtp_error_nonse_reuse,	"Nonce reuse"},
	{zrtp_error_equal_zid,		"Equal ZIDs in Hello"},
	{zrtp_error_service_unavail,"Service unavailable"},
	{zrtp_error_goclear_unsp,	"GoClear packet received, but not allowed"},

	{zrtp_error_wrong_zid,		"ZID received in new Hello doesn't equal to ZID from the previous stream"},
	{zrtp_error_wrong_meshmac,	"Message HMAC doesn't match with pre-received one"}
};

const char* zrtp_log_error2str(zrtp_protocol_error_t error)
{
	int i=0;
	for(i=0; i<22; i++) {
		if (error == _error_strings[i].code) {
			return _error_strings[i].descr;
		}
	}
	
	return k_unknown;
}

/*---------------------------------------------------------------------------*/
static char* _status_strings[zrtp_status_count] =
{
	"OK status",
	"General, unspecified failure",
	"Wrong, unsupported parameter",
	"Fail allocate memory",
	"SRTP authentication failure",
	"Cipher failure on RTP encrypt/decrypt",
	"General Crypto Algorithm failure",
	"SRTP can't use key any longer",
	"Input buffer too small",
	"Packet process DROP status",
	"Failed to open file/device",
	"Unable to read data from the file/stream",
	"Unable to write to the file/stream",
	"SRTP packet is out of sliding window",
	"RTP replay protection failed",
	"ZRTP replay protection failed",
	"ZRTP packet CRC is wrong",
	"Can't generate random value",
	"Illegal operation in current state",
	"Attack detected",
	"Function is not available in current configuration"
};

const char* zrtp_log_status2str(zrtp_status_t error)
{
	if (zrtp_status_count > error) {
		return _status_strings[error];
	} else {
		return k_unknown;
	}
}

/*---------------------------------------------------------------------------*/
static char* _state_names[ZRTP_STATE_COUNT] =
{
	"NONE",
	"ACTIVE",
	"START",
	"W4HACK",
	"W4HELLO",
	"CLEAR",
	"SINITSEC",
	"INITSEC",
	"WCONFIRM",
	"W4CONFACK",
	"PENDSEC",
	"W4CONF2",
	"SECURE",
	"SASRELAY",
	"INITCLEAR",
	"PENDCLEAR",
	"INITERROR",
	"PENDERROR",
	"ERROR",
	#if (defined(ZRTP_BUILD_FOR_CSD) && (ZRTP_BUILD_FOR_CSD == 1))
	"DRIVINIT",
	"DRIVRESP",
	"DRIVPEND",
	#endif
	"NOZRTP"
};

const char* zrtp_log_state2str(zrtp_state_t state)
{
	if (state < ZRTP_STATE_COUNT) {
		return _state_names[state];
	} else {
		return k_unknown;
	}	
};

/*---------------------------------------------------------------------------*/
static char* _stream_mode_name[ZRTP_STREAM_MODE_COUNT] =
{
	"UNKNOWN",
	"CLEAR",
	"DH",
	"PRESHARED",
	"MULTI"
};

const char* zrtp_log_mode2str(zrtp_stream_mode_t mode)
{
	if (mode <  ZRTP_STREAM_MODE_COUNT) {
		return _stream_mode_name[mode];
	} else {
		return k_unknown;
	}
};

/*---------------------------------------------------------------------------*/
static char* _msg_type_names[ZRTP_MSG_TYPE_COUNT] =
{	
	"NONE",
	"HELLO",
	"HELLOACK",
	"COMMIT",
	"DH1",
	"DH2",
	"CONFIRM1",
	"CONFIRM2",
	"CONFIRMACK",
	"GOCLEAR",
	"CLEARACKE",
	"ERROR",
	"ERRORACK",
	"PROCESS",
	"SASRELAY",
	"RELAYACK",
	"PING",
	"PINGACK",
};

const char* zrtp_log_pkt2str(zrtp_msg_type_t type)
{
	if (type < ZRTP_MSG_TYPE_COUNT) {
		return _msg_type_names[type];
	} else {
		return k_unknown;
	}
}

/*---------------------------------------------------------------------------*/
static char* _event_code_name[] = 
{
	"ZRTP_EVENT_UNSUPPORTED",
	"ZRTP_EVENT_IS_CLEAR",
	"ZRTP_EVENT_IS_INITIATINGSECURE",
	"ZRTP_EVENT_IS_PENDINGSECURE",
	"ZRTP_EVENT_IS_PENDINGCLEAR",
	"ZRTP_EVENT_NO_ZRTP",
	"ZRTP_EVENT_NO_ZRTP_QUICK",
	"ZRTP_EVENT_IS_CLIENT_ENROLLMENT",
	"ZRTP_EVENT_NEW_USER_ENROLLED",
	"ZRTP_EVENT_USER_ALREADY_ENROLLED",
	"ZRTP_EVENT_USER_UNENROLLED",
	"ZRTP_EVENT_LOCAL_SAS_UPDATED",
	"ZRTP_EVENT_REMOTE_SAS_UPDATED",
	"ZRTP_EVENT_IS_SECURE",
	"ZRTP_EVENT_IS_SECURE_DONE",
	"ZRTP_EVENT_IS_PASSIVE_RESTRICTION",
	"ZRTP_EVENT_PROTOCOL_ERROR",
	"ZRTP_EVENT_WRONG_SIGNALING_HASH",
	"ZRTP_EVENT_WRONG_MESSAGE_HMAC",
	"ZRTP_EVENT_MITM_WARNING"
};

const char* zrtp_log_event2str(uint8_t event)
{
	if (event <= ZRTP_EVENT_WRONG_MESSAGE_HMAC) {
		return _event_code_name[event];
	} else {
		return k_unknown;
	}
}

static char* _sign_role_name[] =
{
	"Unknown",
	"Initiator",
	"Responder"
};

const char* zrtp_log_sign_role2str(unsigned role) {
	if (role < ZRTP_SIGNALING_ROLE_COUNT) {
		return _sign_role_name[role];
	} else {
		return k_unknown;
	}
}

/*---------------------------------------------------------------------------*/
typedef struct _zrtp_aling_test
{
	uint_8t	c1;
	uint_8t	c2;
	uint_8t	c3;
} _zrtp_aling_test;

void zrtp_print_env_settings(zrtp_config_t* config)
{
#if (ZRTP_PLATFORM == ZP_WIN32)
	char* platform = "Windows 32bit";
#elif (ZRTP_PLATFORM == ZP_WIN32_KERNEL) 
	char* platform = "Windows Kernel 32bit";
#elif (ZRTP_PLATFORM == ZP_WINCE) 
	char* platform = "Windows CE";
#elif (ZRTP_PLATFORM == ZP_DARWIN)
	char* platform = "Darwin OS X";
#elif (ZRTP_PLATFORM == ZP_BSD)
	char* platform = "BSD";
#elif (ZRTP_PLATFORM == ZP_LINUX)
	char* platform = "Linux OS";
#elif (ZRTP_PLATFORM == ZP_SYMBIAN) 
	char* platform = "Symbian OS";
#elif (ZRTP_PLATFORM == ZP_ANDROID) 
	char* platform = "Android OS";
#endif
	
	ZRTP_LOG(3,("zrtp","============================================================\n"));
	ZRTP_LOG(3,("zrtp","ZRTP Configuration Settings\n"));
	ZRTP_LOG(3,("zrtp","============================================================\n"));
	ZRTP_LOG(3,("zrtp","                      PLATFORM: %s\n", platform));
#if (ZRTP_BYTE_ORDER == ZBO_BIG_ENDIAN)
	ZRTP_LOG(3,("zrtp","                    BYTE ORDER: BIG ENDIAN\n"));
#else
	ZRTP_LOG(3,("zrtp","                    BYTE ORDER: LITTLE ENDIAN\n"));
#endif
	ZRTP_LOG(3,("zrtp","        ZRTP_SAS_DIGEST_LENGTH: %d\n", ZRTP_SAS_DIGEST_LENGTH));
	ZRTP_LOG(3,("zrtp","  ZRTP_MAX_STREAMS_PER_SESSION: %d\n", ZRTP_MAX_STREAMS_PER_SESSION));
	ZRTP_LOG(3,("zrtp","          ZRTP_USE_EXTERN_SRTP: %d\n", ZRTP_USE_EXTERN_SRTP));
	ZRTP_LOG(3,("zrtp","          ZRTP_USE_STACK_MINIM: %d\n", ZRTP_USE_STACK_MINIM));
    ZRTP_LOG(3,("zrtp","            ZRTP_BUILD_FOR_CSD: %d\n", ZRTP_BUILD_FOR_CSD));
    ZRTP_LOG(3,("zrtp","              ZRTP_USE_BUILTIN: %d\n", ZRTP_USE_BUILTIN));
	ZRTP_LOG(3,("zrtp","    ZRTP_USE_BUILTIN_SCEHDULER: %d\n", ZRTP_USE_BUILTIN_SCEHDULER));
	ZRTP_LOG(3,("zrtp","        ZRTP_USE_BUILTIN_CACHE: %d\n", ZRTP_USE_BUILTIN_CACHE));
    ZRTP_LOG(3,("zrtp","            ZRTP_LOG_MAX_LEVEL: %d\n", ZRTP_LOG_MAX_LEVEL));
	
	ZRTP_LOG(3,("zrtp","         sizeo of unsigned int: %d\n", sizeof(unsigned int)));
    ZRTP_LOG(3,("zrtp","    size of unsigned long long: %d\n", sizeof(unsigned long long)));
	ZRTP_LOG(3,("zrtp","          sizeo of three chars: %d\n", sizeof(_zrtp_aling_test)));
	ZRTP_LOG(3,("zrtp","\n"));
	ZRTP_LOG(3,("zrtp","ZRTP Initialization Settings\n"));
	ZRTP_LOG(3,("zrtp","                    client ID: %s\n", config->client_id));
	ZRTP_LOG(3,("zrtp","                      license: %d\n", config->lic_mode));
	ZRTP_LOG(3,("zrtp","                         MiTM: %s\n", config->is_mitm?"ENABLED":"DIABLED"));
	ZRTP_LOG(3,("zrtp","                   cache path: %s\n", config->def_cache_path.length?config->def_cache_path.buffer:""));
}

/*---------------------------------------------------------------------------*/
void zrtp_log_print_streaminfo(zrtp_stream_info_t* info)
{
	ZRTP_LOG(3,("zrtp"," ZRTP Stream ID=%u\n", info->id));
	ZRTP_LOG(3,("zrtp","           mode: %s\n", zrtp_log_mode2str(info->mode)));
	ZRTP_LOG(3,("zrtp","          state: %s\n", zrtp_log_state2str(info->state)));
	ZRTP_LOG(3,("zrtp","          error: %s\n", zrtp_log_error2str(info->last_error)));
	
	ZRTP_LOG(3,("zrtp","   peer passive: %s\n", info->peer_passive?"ON":"OFF"));
	ZRTP_LOG(3,("zrtp","  peer disclose: %s\n", info->peer_disclose?"ON":"OFF"));
	ZRTP_LOG(3,("zrtp","      peer mitm: %s\n", info->peer_mitm?"ON":"OFF"));
	ZRTP_LOG(3,("zrtp"," res allowclear: %s\n", info->res_allowclear?"ON":"OFF"));
}

void zrtp_log_print_sessioninfo(zrtp_session_info_t* info)
{
	char buffer[256];
	
	ZRTP_LOG(3,("zrtp"," ZRTP Session sID=%u is ready=%s\n", info->id, info->sas_is_ready?"YES":"NO"));
	ZRTP_LOG(3,("zrtp","    peer client: <%s> V=<%s>\n", info->peer_clientid.buffer, info->peer_version.buffer));
	hex2str(info->zid.buffer, info->zid.length, buffer, sizeof(buffer));
	ZRTP_LOG(3,("zrtp","            zid: %s\n", buffer));
	hex2str(info->peer_zid.buffer, info->peer_zid.length, buffer, sizeof(buffer));
	ZRTP_LOG(3,("zrtp","       peer zid: %s\n", buffer));
	hex2str(info->zid.buffer, info->zid.length, buffer, sizeof(buffer));
	
	ZRTP_LOG(3,("zrtp","     is base256: %s\n", info->sas_is_base256?"YES":"NO"));
	ZRTP_LOG(3,("zrtp","           SAS1: %s\n", info->sas1.buffer));
	ZRTP_LOG(3,("zrtp","           SAS2: %s\n", info->sas2.buffer));
	hex2str(info->sasbin.buffer, info->sasbin.length, buffer, sizeof(buffer));
	ZRTP_LOG(3,("zrtp","        bin SAS: %s\n", buffer));
	ZRTP_LOG(3,("zrtp","            TTL: %u\n", info->secrets_ttl));
	
	ZRTP_LOG(3,("zrtp","           hash: %s\n", info->hash_name.buffer));
	ZRTP_LOG(3,("zrtp","         cipher: %s\n", info->cipher_name.buffer));
	ZRTP_LOG(3,("zrtp","           auth: %s\n", info->auth_name.buffer));
	ZRTP_LOG(3,("zrtp","            sas: %s\n", info->sas_name.buffer));
	ZRTP_LOG(3,("zrtp","            pks: %s\n", info->pk_name.buffer));
}

/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */

#ifndef __ZRTP_LOG_H__
#define __ZRTP_LOG_H__

#include "zrtp_config.h"
#include "zrtp_types.h"
#include "zrtp_base.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ZRTP_LOG_SENDER_MAX_LEN	12
#define ZRTP_LOG_BUFFER_SIZE	512


/*!
 * \defgroup iface_log Functions for debug and information logging
 * \ingroup interfaces
 * \{
 */	
	
/**
 * @brief Write log message.
 * This is the main macro used to write text to the logging backend.
 * @param level	The logging verbosity level. Lower number indicates higher
 *		    importance, with level zero indicates fatal error. Only
 *		    numeral argument is permitted (e.g. not variable).
 * @param arg Enclosed 'printf' like arguments, with the first 
 *		    argument is the sender, the second argument is format 
 *		    string and the following arguments are variable number of 
 *		    arguments suitable for the format string.
 *
 * Sample:
 * @code
 * ZRTP_LOG(2, (__UNITE__, "Some log message with id %d", id));
 * @endcode
 */

#define ZRTP_LOG(level,arg)	do { \
zrtp_log_wrapper_##level(arg); \
} while (0)

#define ZRTP_LOGC(level,arg)	do { \
zrtp_log_wrapperc_##level(arg); \
} while (0)

	
/**
 * @brief Signature for function to be registered to the logging subsystem to
 * write the actual log message to some output device.
 *
 * @param level	    Log level.
 * @param data	    Log message, which will be NULL terminated. 
 * @param len	    Message length. (prefix + text)
 * @param offset	Log message prefix length
 */
typedef void zrtp_log_engine(int level, char *data, int len, int offset);
	

#if ZRTP_LOG_MAX_LEVEL >= 1
	
/**
 * @brief Changes default log writer function.
 * This function may be used to implement log writer in a way native for target
 * OS or product. By default libzrtp uses console output.
 * @param engine - log writer.
 */
void zrtp_log_set_log_engine(zrtp_log_engine *engine);
	
/**
 * @brief Changes Log-Level in run-time mode
 * Libzrtp uses 3 log levels: 
 * - 1 - system related errors;
 * - 2 - security, ZRTP protocol related errors and warnings;
 * - 3 - debug logging.
 * By default, libzrtp uses debug logging - level 3.
 * @param level - log level.
 */	
void zrtp_log_set_level(uint32_t level);

/* \} */
		
#else	/* If logger is enabled */
	
#  define zrtp_log_set_log_engine(engine)
#  define zrtp_log_set_level(level)
	
#endif	/* If logger is enabled */
	
	
#if ZRTP_LOG_MAX_LEVEL >= 1
#	define zrtp_log_wrapper_1(arg)	zrtp_log_1 arg
	void zrtp_log_1(const char *src, const char *format, ...);
#	define zrtp_log_wrapperc_1(arg)	zrtp_logc_1 arg
	void zrtp_logc_1(const char *format, ...);
#else
#	define zrtp_log_wrapper_1(arg)
#	define zrtp_log_wrapperc_1(arg)
#endif
	
#if ZRTP_LOG_MAX_LEVEL >= 2
#	define zrtp_log_wrapper_2(arg)	zrtp_log_2 arg
	void zrtp_log_2(const char *src, const char *format, ...);
#	define zrtp_log_wrapperc_2(arg)	zrtp_logc_2 arg
	void zrtp_logc_2(const char *format, ...);
#else
#define zrtp_log_wrapper_2(arg)
#define zrtp_log_wrapperc_2(arg)
#endif
	
#if ZRTP_LOG_MAX_LEVEL >= 3
#	define zrtp_log_wrapper_3(arg)	zrtp_log_3 arg
	void zrtp_log_3(const char *src, const char *format, ...);
#	define zrtp_log_wrapperc_3(arg)	zrtp_logc_3 arg
	void zrtp_logc_3(const char *format, ...);

#else
#	define zrtp_log_wrapper_3(arg)
#	define zrtp_log_wrapperc_3(arg)
#endif
	
const char* zrtp_log_error2str(zrtp_protocol_error_t error);
const char* zrtp_log_status2str(zrtp_status_t error);

/** Returns symbolical name of ZRTP protocol state for the current stream. */
const char* zrtp_log_state2str(zrtp_state_t state);

/**  Returns symbolical name of ZXRTP protocol packet by it's code. */
const char*	zrtp_log_pkt2str(zrtp_msg_type_t type);

/** Returns symbolical name of the PK Exchange mode for the current stream. */
const char* zrtp_log_mode2str(zrtp_stream_mode_t mode);

/** Returns symbolical name of the protocol and security events. */
const char* zrtp_log_event2str(uint8_t event);

/**
 * Returns character name of the Signaling role.
 *
 * @param role One of zrtp_signaling_role_t values.
 * @return character name of the \c role.
 */
const char* zrtp_log_sign_role2str(unsigned role);


/** Print out ZRTP environment configuration setting to log level 3. */
void  zrtp_print_env_settings();

/** Print out ZRTP stream info strxucture. (use ZRTP log-level 3). */
void zrtp_log_print_streaminfo(zrtp_stream_info_t* info);

/** Print out ZRTP session info structure. (use ZRTP log-level 3). */
void zrtp_log_print_sessioninfo(zrtp_session_info_t* info);

#ifdef __cplusplus
}
#endif


#endif /* __ZRTP_LOG_H__ */

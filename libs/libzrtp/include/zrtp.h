/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */

/**
 * \file zrtp.h
 * \brief Defines basic libzrtp functions and data types 
 */

#ifndef __ZRTP_H__
#define __ZRTP_H__

#include "zrtp_config.h"
#include "zrtp_base.h"
#include "zrtp_error.h"
#include "zrtp_types.h"
#include "zrtp_protocol.h"
#include "zrtp_engine.h"
#include "zrtp_crypto.h"
#include "zrtp_iface.h"
#include "zrtp_iface_system.h"
#include "zrtp_iface_scheduler.h"
#include "zrtp_list.h"
#include "zrtp_legal.h"
#include "zrtp_log.h"
#include "zrtp_srtp.h"
#include "zrtp_srtp_builtin.h"
#include "zrtp_string.h"
#include "zrtp_pbx.h"
#include "zrtp_legal.h"
#include "zrtp_version.h"
#include "zrtp_iface_cache.h"
#include "zrtp_ec.h"



/**
 * \defgroup zrtp_api API
 *
 * In this section the basic functions for using the library are defined. They include 
 * initialization and deinitialization functions, functions for session and stream management and 
 * functions for RTP traffic management.
 *
 * In most cases this section is all you need to start working with libzrtp. The typical simplified
 * order of operations in using libzrtp is the following:
 *  -# library configuration
 *  -# library initialization;
 *  -# ZRTP session creation and configuration;
 *  -# ZRTP stream attaching and Protocol initiation;
 *  -# RTP stream processing;
 *  -# ZRTP protocol stopping and releasing resources.
 * For each of these actions there is a set of corresponding functions. 
 * \sa
 *  - \ref howto
 *  - \ref XXX_GUIDE
 */



/*======================================================================*/
/*    Public ZRTP libzrtp datatypes                                     */
/*======================================================================*/


/**
 * \defgroup zrtp_types Types and Definitions
 * \ingroup zrtp_api
 * The data types used in libzrtp are defined in this section
 * \{
 *
 */
/**
 * \typedef typedef uint32_t zrtp_id_t;
 * \brief libzrtp general identifier used to debug connections management.
 * \ingroup zrtp_main_init 
 */

/** Length of "zrtp-hash-value", RFC 6189 sec 8. @sa zrtp_signaling_hash_get(); */
#define ZRTP_SIGN_ZRTP_HASH_LENGTH	(ZRTP_MESSAGE_HASH_SIZE*2)

/**
 * \brief Enumeration for ZRTP Licensing modes
 * \ingroup zrtp_main_init 
 *
 * A ZRTP endpoint that is Passive will never send a Commit message, which means that it cannot be 
 * the initiator in the ZRTP exchange. Since at least one of the two parties must be the initiator, 
 * two Passive endpoints cannot make a secure connection. However, a non-Passive ZRTP endpoint can 
 * send a Commit message, enabling it to act as the initiator in a ZRTP exchange. This allows it to 
 * make a secure connection to a Passive endpoint, or to another non-Passive endpoint.
 *
 * In addition, a Passive ZRTP endpoint declares that it is Passive by setting the passive flag in 
 * the Hello message, which means the other party will recognize it as Passive. This allows for a 
 * Passive mode and two forms of Active mode-- Active, or Unlimited.  These three possible behaviors 
 * for a ZRTP endpoint are defined as:
 *	- \b Passive:  Never send a Commit message, and thus can never be the initiator.
 *	- \b Active:  Will send a Commit message, but only to non-Passive ZRTP partners.
 *	- \b Unlimited:  Will send a Commit message to any ZRTP partner, Passive or non-Passive.
 *
 * This can be used to provide three classes of service, which can be licensed t different price 
 * points.  Passive can be used in freeware for widest possible deployment, Active can be used in 
 * discount products that can only talk to non-freeware, and Unlimited can be used in full-price 
 * products that will benefit from the network effect of widely deployed Passive freeware.
 */
typedef enum zrtp_license_mode_t
{
	/** @brief  Never send a Commit message, and thus can never be the initiator. */
	ZRTP_LICENSE_MODE_PASSIVE = 0,
	/** @brief Will initiate ZRTP exchange, but only to non-Passive ZRTP partners. */
	ZRTP_LICENSE_MODE_ACTIVE,
	/** @brief Will send a Commit message to any ZRTP partner, Passive or non-Passive. */
	ZRTP_LICENSE_MODE_UNLIMITED
} zrtp_license_mode_t;

/**
 * @brief Enumeration to define Signaling initiator/responder roles.
 * 
 * Used by libzrtp to optimize some internal processes and protocol handshake.
 *
 * @sas zrtp_stream_start().
 */
typedef enum zrtp_signaling_role_t
{
	/** @brief Unknown Signaling role, should be used when the app can't determine the role. */
	ZRTP_SIGNALING_ROLE_UNKNOWN	= 0,
	/** @brief Signaling Initiator. */	
	ZRTP_SIGNALING_ROLE_INITIATOR,
	/** @brief Signaling Responder. */
	ZRTP_SIGNALING_ROLE_RESPONDER,	
	ZRTP_SIGNALING_ROLE_COUNT
} zrtp_signaling_role_t;


/** @brief 12-byte ZID for unique ZRTP endpoint identification. */
typedef unsigned char zrtp_zid_t[12];

/** \brief 16-byte ID for ZRTP endpoint's software identification. */
typedef char zrtp_client_id_t[16];
	
/**
 * @brief ZRTP global configuration options
 * @ingroup zrtp_main_init
 * @warning Use \ref zrtp_config_defaults() before start configuring this structure.
 */
typedef struct zrtp_config_t
{	
	/** @brief Symbolic client identifier */
	zrtp_client_id_t		client_id;
	
	/** @brief libzrtp license mode defined protocol behavior */
	zrtp_license_mode_t		lic_mode;
	
	/** @brief Set this flag to 1 if you product is MiTM box */
	uint8_t					is_mitm;
	
	/** @brief Set of interfaces required to operate with libzrtp */
	zrtp_callback_t			cb;

	/** @brief Path to zrtp cache file (set if you use built-in realization) */
	zrtp_string256_t		def_cache_path;

	/**
	 * @brief Flush the cache automatically
	 * Set to 1 if you want libzrtp to flush the cache to the persistent storage
	 * right after it is modified. If cache_auto_store is 0, libzrtp will flush
	 * the cache on going down only and the app is responsible for storing the
	 * cache in unexpected situations. Enabled by default.
	 *
	 * @sa zrtp_def_cache_store()
	 */
	unsigned				cache_auto_store;
} zrtp_config_t;

/**
 * \brief zrtp stream information structure
 * \ingroup zrtp_main_management
 * 
 * libzrtp, since v0.80 takes data encapsulating approach and hides all private data inside
 * zrtp_stream_t structure. Developers shouldn't access them directly. \ref zrtp_stream_get() should 
 * be used instead to fill zrtp_stream_info_t structure. zrtp_stream_info_t contains all needed 
 * information in safe and easy to use form.
 */
struct zrtp_stream_info_t
{
	/** \brief Stream unique identifier for debug purposes */
	zrtp_id_t				id;
		
	/** \brief Pointer to the parent zrtp session */
	zrtp_session_t*			session;
	
	/** \brief Stream mode. Defines libzrtp behavior related to specified contexts. */
	zrtp_stream_mode_t		mode;
	
	/** \brief Defines ZRTP Trusted mitm mode for the current session. */
	zrtp_mitm_mode_t		mitm_mode;
	
	/** \brief Reflects current state of ZRTP protocol */
	zrtp_state_t			state;
	
	/**
	 * \brief Last protocol error code
	 *
	 * Available for reading in ERROR state on zrtp_security_event_t#ZRTP_EVENT_PROTOCOL_ERROR.
	 */
	zrtp_protocol_error_t	last_error;
	
	/**
	 * \brief Remote passive flag
	 * 
	 * This flag shows when remote side is "passive" (has license mode PASSIVE) available in CLEAR 
	 * state and later.
	 */
	uint8_t					peer_passive;
	
	/**
	 * \brief Allowclear flag.
	 *
	 * Current value of "allowclear" option exchanged during ZRTP negotiation. Available in SECURE 
	 * state.     
	 */
	uint8_t					res_allowclear;
		
	/**
	 * \brief Peer disclose bit flag
	 *
	 * Indicates the ability of the remote side to disclose its session key.  Specifies that the 
	 * remote side allows call monitoring. If this flag is set, the end user must be informed. It 
	 * can be read in the SECURE state.
	 */
	uint8_t					peer_disclose; 
	
	/**
	 * \brief Defines that remote party is ZRTP MiTM endpoint
	 *
	 * Enabled by (Asterisk PBX, UMLab SIP Firewall or etc.) Available for reading in CLEAR state 
	 * ande later.
	 */
	uint8_t					peer_mitm;
};

/**
 * \brief zrtp session information structure
 * \ingroup zrtp_main_management
 * libzrtp, since v0.80 takes data incapsulating approach and hides all private date inside 
 * zrtp_session_t structure. Developers shouldn't access them directly. \ref zrtp_session_get() 
 * should  be used instead to fill zrtp_session_info_t structure. zrtp_session_info_t contains all 
 * needed information in safe and easy to use form.
 */
struct zrtp_session_info_t
{
	/** \brief Session unique identifier for debug purposes */
	zrtp_id_t			id;
		
	/**
	 * \brief Local ZID
	 *
	 The unique 12-characters string that identifies the local ZRTP endpoint.This ID allows remote 
	 * peers to recognize this ZRTP endpoint.
	 */	
	zrtp_string16_t		zid;
	
	/**
	 * \brief Remote  ZID
	 *
	 * Extracted from the Hello packet of the very first ZRTP stream. Uniquely identifies the remote 
	 * ZRTP peer.
	 */
	zrtp_string16_t		peer_zid;
	
	/** \brief Character name identified remote ZRTP endpoint.*/
	zrtp_string16_t		peer_clientid;
	
	/** \brief ZRTP Protocol version supported by the remote endpoint. */
	zrtp_string16_t		peer_version;
	
	/**
	 * \brief Indicates that SAS related data is available for reading.
	 * \note 
	 * As SAS is computed in SECURE state only, it may contain unknown values in other states. Check 
	 * sas_is_ready before displaying SAS to the user.
	 */
	uint8_t				sas_is_ready;
	
	/** \brief First Short Authentication String */
	zrtp_string16_t		sas1;
	
	/**
	 * \brief Second Short Authentication string.
	 * \note
	 * Second SAS is available for \c base256 authentication only (\c sas_is_base256 is set). In 
	 * other case, \c sas1 contains \c base32 value and \c sas2 is empty.
	 */
	zrtp_string16_t		sas2;
	
	/** \brief Binary SAS digest (ZRTP_SAS_DIGEST_LENGTH bytes) */
	zrtp_string32_t		sasbin;
	
	/**
	 * \brief Bit-map to summarize shared secrets "Cached" flags.
	 *
	 * 1 at appropriate bit means that the secrets was found in the cache and restored successfully.
	 * Value equal to 0 indicates that secret for the remote endpoint was not found  in the cache
	 * and  was generated randomly.
	 * Use ZRTP_BIT_RS1, ZRTP_BIT_RS2, ZRTP_BIT_AUX and ZRTP_BIT_PBX bit-masks to get "cached" value
	 * for the appropriate secret.
	 */
	uint32_t			cached_flags;
	
	/**
	 * \brief Bit-map to summarize shared secrets "Matched" flags.
	 *
	 * 1 at appropriate bit means that the secret, locally computed by your ZRTP endpoint is equal
	 * to the secret, received from the remote endpoint. Secrets may not match if one of the
	 * endpoints doesn't use cache of the shared secrets, if the cache was deleted or in case of
	 * an attack.
	 * Use ZRTP_BIT_RS1, ZRTP_BIT_RS2, ZRTP_BIT_AUX and ZRTP_BIT_PBX bit-masks to get "cached" value
	 * for the appropriate secret.
	 */
	uint32_t			matches_flags;
	
	/**
	 * \brief Bit-map to summarize shared secrets "Wrong" flags.
	 *
	 * 1 at appropriate bit means that the secret was restored from the cache, but doesn't match
	 * to the remote endpoint's secret. Such situation may happen if the remote endpoint lost cache
	 * or in case of attach.
	 * Use ZRTP_BIT_RS1, ZRTP_BIT_RS2, ZRTP_BIT_AUX and ZRTP_BIT_PBX bit-masks to get "cached" value
	 * for the appropriate secret.
	 */
	uint32_t			wrongs_flags;
   
	/** 
	 * \brief SAS Verification flag.
	 *
	 * The SAS Verified flag (V) is set based on the user indicating that SAS comparison has been 
	 * successfully performed. Each party sends the SAS Verified flag from the previous session in 
	 * the Confirm message of the current session. 
	 * \sa
	 *	- ZRTP RFC section. "7.1.  SAS Verified Flag" for more information about Verification Flag.
	 *	- zrtp_verified_set()
	 */
	uint32_t			sas_is_verified;

	/** \brief Indicates base256 SAS encoding */
	uint8_t				sas_is_base256;
		
	/**
	 * \brief actual lifetime of the secrets
	 * 
	 * This variable contains the interval for retaining secrets within an established session. In
	 * accordance with ZRTP RFC this value is calculated as the minimal of local and remote TTLs 
	 * after confirmation. Value is given in seconds and can be read in the SECURE state.
	 */
	uint32_t			secrets_ttl;
		
	/** \brief Hash crypto component name used in ZRTP calculations. */
	zrtp_string32_t		hash_name;
	
	/** \brief Cipher crypto component name used in ZRTP encryption. */
	zrtp_string32_t		cipher_name;
	
	/** \brief SRTP Authentication crypto component name used in ZRTP exchange. */
	zrtp_string32_t		auth_name;
	
	/** \brief SAS scheme crypto component name used in ZRTP exchange. */
	zrtp_string32_t		sas_name;
	
	/** \brief Publik Key Exchange name used in ZRTP exchange. */
	zrtp_string32_t		pk_name;
};

/* \} */


/*======================================================================*/
/*    libzrtp Public API: Streams management                            */
/*======================================================================*/


#if defined(__cplusplus)
extern "C"
{
#endif	

/**
 * \defgroup zrtp_main_init Initalization and Configuration
 * \ingroup zrtp_api
 * \{
 */

/**
 * \brief Initializes libzrtp global config
 *
 * zrtp_config_defaults() prepares all fields of zrtp_config_t for further usage in zrtp_init(). 
 * This function allocates all necessary resources and initialize zrtp_config_t#cb with default 
 * implementations.
 *
 * \param config - libzrtp config for initialization.
 * \warning this function must be used before start operating with the config.
 */
void zrtp_config_defaults(zrtp_config_t* config);
	
/**
 * \brief Initializing libzrtp
 *
 * This function initializes the library and all its components. zrtp_init() initialize global data 
 * for all sessions and streams. Fields of the global zrtp context are initialized automatically and 
 * shouldn't be modified. For correct memory management, global context should be released by 
 * calling zrtp_down().
 *
 * \param config - libzrtp inital parameters
 * \param zrtp - out parameter, pointer to allocated zrtp global context structure;
 * \warning this function \b must be called before any operation with libzrtp.
 * \return
 *  - zrtp_status_ok in successfully initialized or one of zrtp status errors in other case.
 * \sa zrtp_down()
*/
zrtp_status_t zrtp_init(zrtp_config_t* config, zrtp_global_t** zrtp);

/*!
 * \brief Shutting down the library
 *
 * Frees all allocated structures and resources. This function \b must be called at the end of use 
 * to stop libzrtp correctly. zrtp_down() doesn't stop in-progress ZRTP streams. To avoid mistakes, 
 * close all sessions before library deinitialization.
 *
 * \param zrtp - global ZRTP context previously allocated by zrtp_init();
 * \return
 *  - zrtp_status_ok if successfully shut down;
 *  - zrtp_status_fail if an error occurred.
 * \sa zrtp_init()
 */
zrtp_status_t zrtp_down(zrtp_global_t* zrtp);

/* \} */

/**
 * \defgroup zrtp_main_management ZRTP Connections
 * \ingroup zrtp_api
 * \{
 */

/**
 * \brief ZRTP Session Initialization.
 *
 * This function allocates and initializes the internal session context data. The given context is 
 * associated with the specified ZRTP identifier. Only after initialization does the session contain
 * ZRTP_MAX_STREAMS_PER_SESSION streams ready to be used.
 *
 * After successfully initialization, configuration will be done according to the relevant profile 
 * \c profile. Profile will be applyed to every stream allocated within this session. Before using 
 * the profile, call zrtp_profile_check() function to make sure that the profile you  are applying 
 * is correct.
 *
 * \warning Don't call zrtp_session_init() in parallel with other operations on this session.
 * \param zrtp - global libzrtp context;
 * \param profile - the session configuration profile. If value of this parameter is NULL, default 
 *     profile will be used. NULL profile usage is equivalent to calling zrtp_profile_defaults().
 * \param zid - ZRTP peer identificator.  
 * \param role - identifies if the endpoint was the signaling initiator of the call. Used to 
 *    provide Passive Mode options to the developer. If your application doesn't control signaling 
 *    or you don't want to support Passive Mode features - set it to ZRTP_SIGNALING_ROLE_UNKNOWN.
 * \param session - allocated session structure.
 * \return 
 *  - zrtp_status_ok if initialization is successful;
 *  - zrtp_status_fail if an error occurs.
 * \sa zrtp_session_down()
 */
zrtp_status_t zrtp_session_init( zrtp_global_t* zrtp,
								 zrtp_profile_t* profile,
								 zrtp_zid_t zid,
								 zrtp_signaling_role_t role,
								 zrtp_session_t **session);
/**
 * \brief ZRTP Session context deinitialization
 *
 * This function releases all resources allocated for internal context operations by zrtp_init().
 *
 * \warning Don't call zrtp_session_init() in parallel with other operations on this session.
 * \param session - session for deinitialization.
 * \sa zrtp_session_init()
 */
void zrtp_session_down(zrtp_session_t *session);
	

/**
 * \brief Obtain information about ZRTP session
 *
 * Function initialize and fills all fields of zrtp_session_info_t structure according to
 * the current state of ZRTP session.
 *
 * \param session - zrtp session which parameters should be extracted;
 * \param info - out structure to be initialized.
 * \return
 *  - zrtp_status_ok in case of success.
 *  - zrtp_status_fail if an error occurs.
 */
zrtp_status_t zrtp_session_get(zrtp_session_t *session, zrtp_session_info_t *info);

/**
 * \brief Allow user to associate some data with current zrtp session.
 * \param session - zrtp session to attach data to.
 * \param udata - pointer to the user-data context.
 * \sa zrtp_session_get_userdata()
 */
void zrtp_session_set_userdata(zrtp_session_t *session, void* udata);
	
/**
 * \brief Return user data associated with the zrtp session
 * \param session - zrtp session to extract user data.
 * \return
 *  - pointer to the user-data context previously set by zrtp_session_set_userdata().
 *  - NULL if the user data unavailable.
 * \sa zrtp_session_set_userdata()
 */
void* zrtp_session_get_userdata(zrtp_session_t *session);

/**
 * \brief Attaching a new stream to the session
 *
 * This function call initializes a ZRTP stream and prepares it for use within the specified 
 * session. The maximum number of streams for one session is defined by the
 * ZRTP_MAX_STREAMS_PER_SESSION variable. All newly created streams are equivalent and have 
 * ZRTP_STREAM_MODE_CLEAR mode and ZRTP_ACTIVE state. Only after attaching a stream, ZRTP protocol 
 * can be initiated.
 *
 * \param session - the ZRTP session within which a new stream is to be
 * \param stream - out parameter, attached stream will be stored there
 * \return
 *  - zrtp_status_ok if stream was attached successfully
 *  - one of zrtp_status_t errors in case of failure
 * \sa zrtp_stream_start() zrtp_stream_stop()
 */
zrtp_status_t zrtp_stream_attach(zrtp_session_t *session, zrtp_stream_t** stream);

/**
 * \brief Starting a ZRTP stream
 *
 * ZRTP stream setup is initiated by calling this function. Exchange of command packets begins 
 * immediately according to protocol. If the option "autosecure" is on, calling this function is the 
 * only requirement for setting up the ZRTP connection within a stream. If "autosecure" mode is not 
 * available, calling this function activates only connection within a ZRTP stream. A connection can 
 * be established manually later by calling  zrtp_stream_secure().
 * 
 * Setup of the stream/connection takes a certain interval of time. This function just initiates 
 * this process. The system of callbacks informs the user about the progress of libzrtp protocol. 
 * 
 * \param stream - ZRTP stream to be started.
 * \param ssrc - ssrc which will be used in ZRTP protocol messages. It should match with ssrc of 
 *    appropriate RTP stream which will be encrypted by this ZRTP stream.
 * \return
 *  - zrtp_status_ok in case of success;
 *  - one of zrtp_status_t errors in case of failure
 * \sa
 *  - \ref XXX_GUIDE_CB \ref XXX_GUIDE_MANAGEMENT
 *  - zrtp_stream_stop() zrtp_stream_secure() zrtp_stream_clear()
 */
zrtp_status_t zrtp_stream_start(zrtp_stream_t* stream,
								uint32_t ssrc);

/**
 * \brief ZRTP protocol stopping
 *
 * This function stops all protocol operations for the specified stream, releases resources 
 * allocated on the zrtp_stream_start() and prepares the stream structure for the next use.
 * 
 * This function will stop the protocol at any stage: all delayed tasks are canceled, and the 
 * protocol packet exchange and encryption is stopped. After this function call it is necessary to 
 * stop processing traffic using the zrtp_process_xxx() function.
 *
 * \param stream - the stream being shutdown.
  * \return
 *  - zrtp_status_ok in case of success;
 *  - one of zrtp_status_t errors in case of failure
 * \sa
 *  - \ref XXX_GUIDE_CB \ref XXX_GUIDE_MANAGEMENT
 *  - zrtp_stream_start() zrtp_stream_secure() zrtp_stream_clear()
 */
zrtp_status_t zrtp_stream_stop(zrtp_stream_t* stream);

/*!
 * \brief Initiating an interruption of the secure connection
 *
 * This function initiates the shutting down of the ZRTP connection within a stream. In other words, 
 * after successfully switching to secure mode (\ref XXX SECURE state, fig. 1.5), calling this 
 * function begins the exchange of packets switching back to insecure (CLEAR) mode.
 *
 * This function can only be implemented from the SECURE state. Attempt to call this function from 
 * any other state will end in failure. The client application is informed about protocol
 * progress through a system of callbacks.
 *
 * \param stream - ZRTP stream .
 * \return
 *  - zrtp_status_ok - if shutting down the connection is started successfully.
 *  - zrtp_status_fail - if shutting down the connection is initiated from an incorrect state.
 * \sa
 *  - \ref XXX_GUIDE_CB \ref XXX_GUIDE_MANAGEMENT
 *  - zrtp_stream_start() zrtp_stream_secure() zrtp_stream_clear()
 */
zrtp_status_t zrtp_stream_clear(zrtp_stream_t *stream);

/**
 * \brief Initiating a secure connection setup
 *
 * The function initiates a ZRTP connection setup within a stream. In other words, after the 
 * protocol has started and Discovery phase have been successfully accomplished, calling this 
 * function will begin the exchange of packets for switching to SECURE mode.
 *
 * This function can be successfully performed only from the CLEAR state (\ref XXX Figure 1.6). 
 * Attempting to call this function from any other state will result in failure. The client 
 * application is informed about protocol progress through a system of callbacks.
 * 
 * \param stream - ZRTP stream to be secured. 
 * \return
 *  - zrtp_status_ok - if switching to secure mode started successfully.
 *  - zrtp_status_fail - if switching to secure mode is initiated from a state other than CLEAR.
 * \sa
 *  - \ref XXX_GUIDE_CB \ref XXX_GUIDE_MANAGEMENT.
 *  - zrtp_stream_start() zrtp_stream_clear().
 */
zrtp_status_t zrtp_stream_secure(zrtp_stream_t *stream);

/**
 * \brief Obtain information about zrtp stream
 * 
 * Function initialize and fills all fields of zrtp_stream_info_t structure accordint to
 * current state of zrtp stream.
 *
 * \param stream - zrtp stream which parameters should be extracted
 * \param info - out structure to be initialized
 * \return
 *  - zrtp_status_ok in case of success.
 *  - zrtp_status_fail if an error occurs.
 */
zrtp_status_t zrtp_stream_get(zrtp_stream_t *stream, zrtp_stream_info_t *info);

/**
 * @brief Allow user to associate some data with zrtp stream. 
 * @param stream - zrtp stream to attach data to.
 * @param udata - pointer to the user-data context.
 * @sa zrtp_stream_get_userdata()
 */	
void zrtp_stream_set_userdata(zrtp_stream_t *stream, void* udata);
	
/**
 * \brief Return user data associated with the zrtp stream
 * \return
 *  - pointer to the user-data context previously set by zrtp_stream_set_userdata()
 *  - NULL if user data unavailable;
 * \sa zrtp_stream_set_userdata()
 */	
void* zrtp_stream_get_userdata(const zrtp_stream_t *stream);

/* \} */

/*======================================================================*/
/*    libzrtp Public API: Encryption                                    */
/*======================================================================*/

/**
 * \defgroup zrtp_main_proto Traffic Processing
 * \ingroup zrtp_api
 * \{
 */

/**
 * \brief Processing outgoing RTP packets
 *
 * This is the main function for processing outgoing RTP packets. As soon as the protocol is  
 * started, each outgoing RTP packet (not encrypted) has to go through this function.
 *
 * It performs different actions depending on the connection state and packet type:
 *  - In setup ZRTP connection mode, it encrypts outgoing RTP packets. The packet is encrypted right 
 *    in the transferred buffer;
 *  - Protects codec and data privacy by deleting certain packets from the stream. In this case the 
 *    body and the length of the packet remain unchanged.
 *
 * \param stream - ZRTP stream to process RTP packet;
 * \param packet - buffer storing the RTP packet for encryption. After processing, the encrypted 
 *    packet is stored in the same buffer.
 * \param length - the length of the buffered packet. After processing, the length of encrypted 
 *    packet is stored here.
 * \warning During encryption, the data length increases in comparison to the source data. Because 
 *   the function uses the same buffer both for incoming and resulting values, the length of the 
 *   buffer must be larger than size of source packet.
 * \return
 *  - zrtp_status_ok if encryption is successful. The packet should be sent to the recipient.
 *  - zrtp_status_fail if there was an error during encryption. The packet should be rejected.
 *  - zrtp_status_drop if there was interference in the VoIP client codec protection mechanism. The 
 *    packet should be rejected.
 * \sa zrtp_process_srtp() zrtp_process_rtcp() zrtp_process_srtcp()
 */
zrtp_status_t  zrtp_process_rtp( zrtp_stream_t *stream,
								 char* packet,
								 unsigned int* length);

/**
 * \brief Processing incoming RTP packets 
 * 
 * This is the main function for incoming RTP packets processing. It is an analogue of 
 * zrtp_process_rtp() but for an incoming stream. After the protocol is started, each (encrypted) 
 * incoming RTP packet has to go through this function. 
 *
 * It performs different actions depending on the connection state and packet type: 
 *  - during setup/interruption of ZRTP connection, processes incoming protocol packets. The body 
 *    and length of the packet remain unchanged;
 *  - in setup ZRTP connection mode, decrypts incoming RTP packet. The packet is decrypted right in 
 *    the transferred buffer;
 *  - protects codec and data privacy by deleting certain packets from the stream. In this case the 
 *    body and the length of the packet remain unchanged.
 *
 * \param stream - ZRTP stream for processing
 * \param packet - buffer storing the packet for decrypting. After processing, the decrypted packet 
 *    is stored in the same buffer;
 * \param length - the length of the buffered packet. After processing, the length of decrypted 
 *    packet is stored here;
 * \return
 *  - zrtp_status_ok if decrypting is successful. Such a packet should be sent to the recipient;
 *  - zrtp_status_fail if an error occurred during decrypting or command packet processing. The 
 *    packet should be rejected;
 *  - zrtp_status_drop if the command packet processing is successful or if there was interference 
 *    in the VoIP client codec protection mechanism. The packet should be rejected in either case;
 * \sa zrtp_process_rtp() zrtp_process_rtcp() zrtp_process_srtcp() 
 */
zrtp_status_t  zrtp_process_srtp( zrtp_stream_t *stream,
								  char* packet,
								  unsigned int* length);

/*!
 * \brief Processing outgoing RTCP packets 
 * 
 * This is the main function for processing outgoing RTCP packets. The function behavior is similar 
 * to that of zrtp_process_rtp():
 *  - In SECURE mode, encrypts outgoing RTCP packets. The packet is encrypted right in the 
 *    transferred buffer. The length of encrypted packet is returned in the \c length variable;
 *  - protects codec and data privacy by deleting certain packets from the stream. In this case the 
 *    body and the length of the packet remain unchanged.
 *
 * \param stream - ZRTP session for processing;
 * \param packet - buffer storing RTCP packet;
 * \param length - length of the buffered packet.
 * \return
 *  - zrtp_status_ok if encryption is successful. The packet should be sent to the recipient.
 *  - zrtp_status_fail if there was an error during encryption. The packet should be rejected.
 *  - zrtp_status_drop if there was interference in the VoIP client codec protection mechanism. The 
 *    packet should be rejected.
 * \sa zrtp_process_srtp() zrtp_process_rtp() zrtp_process_srtcp()
 */
zrtp_status_t  zrtp_process_rtcp( zrtp_stream_t *stream,
 								  char* packet,
								  unsigned int* length);

/**
 * \brief Processing incoming RTCP packets 
 * 
 * This is the main function for processing incoming RTCP packets. The function behavior is similar 
 * to that of zrtp_process_srtp():
 *  - In SECURE mode, decrypts incoming RTCP packets. The packet is decrypted right in the 
 *    transferred buffer. The length of the encrypted packet is returned in the \c length variable;
 *  - In transition states, drops all incoming RTCP traffic. In this case the body and the length of 
 *    the packet remain unchanged.
 *
 * \param stream - ZRTP stream for processing;
 * \param packet - buffer storing the RTCP packet;
 * \param length - length of the buffered packet.
 * \return
 *  - zrtp_status_ok if decrypting is successful. Such a packet should be sent to the recipient;
 *  - zrtp_status_drop if the command packet processing is successful or if there was interference 
 *    in the VoIP client codec protection mechanism.  The packet should be rejected in either case; 
 *  - zrtp_status_fail if there was an error during encryption. The packet should be rejected.
 * \sa zrtp_process_srtp() zrtp_process_rtp() zrtp_process_rtcp() 
 */
zrtp_status_t  zrtp_process_srtcp( zrtp_stream_t *stream,
								   char* packet,
								   unsigned int* length);

/* \} */

/**
 * \defgroup zrtp_main_utils Utilities
 * \ingroup zrtp_api
 * \{
 */

/**
 * \brief Specifies the hash of the peer Hello message for verification.
 *
 * In accordance with the ZRTP RFC sec. 9, this protocol can prevent DOS attacks by verification of 
 * the Hello message hash sent through the signaling protocol.
 *
 * This function allows the user to specify the Hello hash for verification. If after the 
 * discovering phase the Hello hashes don't match, libzrtp raises the 
 * zrtp_event_t#ZRTP_EVENT_WRONG_SIGNALING_HASH event. This function should only be called before 
 * starting the protocol from the ZRTP_STATE_ACTIVE state.
 * 
 * \param stream - stream for operating with;
 * \param hash_buff - signaling hash buffer. Function accepts string, not a binary value!;
 * \param hash_buff_length - signaling hash length in bytes, must be ZRTP_SIGN_ZRTP_HASH_LENGTH bytes;
 * \return:
 *  - zrtp_status_ok if the operation finished successfully
 *  - one of the errors otherwise
 * \sa
 *  - ZRTP RFC. sec 8;
 *  - zrtp_signaling_hash_get()
 */
zrtp_status_t zrtp_signaling_hash_set( zrtp_stream_t* stream,
									  const char *hash_buff,
									  uint32_t hash_buff_length);

/**
 * \brief Returns the hash of the Hello message to be transferred in signaling.
 *
 * To prevent DOS attacks, the hash of the Hello message may be sent through signaling. 
 * zrtp_signaling_hash_get() may be called after attaching the stream to receive the value of this 
 * hash.
 *
 * \param stream - stream for operating with
 * \param hash_buff - buffer for storing signaling hash. Function returns already parsed hex string.
 *      String is null-terminated. Buffer must be at least ZRTP_SIGN_ZRTP_HASH_LENGTH bytes length.
 * \param hash_buff_length - buffer length in bytes, non less  than ZRTP_SIGN_ZRTP_HASH_LENGTH bytes.
 * \return:
 *  - zrtp_status_ok if the operation finished successfully
 *  - one of the errors otherwise
 * \sa
 *  - ZRTP RFC. sec 8;
 *  - zrtp_signaling_hash_set()
 */
zrtp_status_t zrtp_signaling_hash_get(zrtp_stream_t* stream,
									  char* hash_buff,
									  uint32_t hash_buff_length);

/**
 * \brief Changing the value of the secret's verification flag
 * 
 * This function is used to change (set, unset) the secret's verification flag. zrtp_verified_set() 
 * changes the relevant internal data and stores a flag in the cache.
 * \note
 * Special synchronization  mechanisms are provided to protect the cache from race conditions. Don't 
 * change the verified flag  directly in the cache - use this function.
 *
 * \param zrtp - zrtp global data;
 * \param zid1 - ZID of the first party;
 * \param zid2 - ZID of the second party;
 * \param verified - Boolean value of the verified flag.
 * \return
 *  - zrtp_status_ok - if successful;
 *	- one of zrtp_status_t errors if fails.
 */
zrtp_status_t zrtp_verified_set( zrtp_global_t *zrtp,
								 zrtp_string16_t *zid1,
								 zrtp_string16_t *zid2,
								 uint8_t verified);	

/**
 * \brief Verifying the ZRTP profile
 * 
 * zrtp_profile_check() checks the correctness of the values in the profile. The following checks 
 * are performed:
 *  - the number of components in each group does not exceed ZRTP_MAX_COMP_COUNT;
 *  - the components declared are supported by the library kernel.
 *  - presence of the set of obligatory components defined by ZRTP RFC.
 *
 * \param profile - ZRTP profile for validation;
 * \param zrtp - global ZRTP context.
 * \return
 *  - zrtp_status_ok - if profile passed all available tests;
 *  - one of ZRTP errors - if there are mistakes in the profile. See debug logging for additional 
 *    information.
 */
zrtp_status_t zrtp_profile_check(const zrtp_profile_t* profile, zrtp_global_t* zrtp);

/**
 * \brief Configure the default ZRTP profile
 * 
 * These options are used:
 * \code
 * "active" is enabled;
 * "allowclear" is disabled by default and enabled for Zfone only;
 * "autosecure" is enabled;
 * "disclose_bit" is disabled;
 * cache_ttl = ZRTP_CACHE_DEFAULT_TTL defined by ZRTP RFC;
 *
 * [sas_schemes] = ZRTP_SAS_BASE256, ZRTP_SAS_BASE32;
 * [cipher_types] = ZRTP_CIPHER_AES128;
 * [pk_schemes] = ZRTP_PKTYPE_DH3072;
 * [auth_tag_lens] = ZRTP_ATL_HS32;
 * [hash_schemes] = ZRTP_HASH_SHA256;
 * \endcode
 *
 * \param profile - ZRTP stream profile for filling;
 * \param zrtp - libzrtp global context.
 */
void zrtp_profile_defaults(zrtp_profile_t* profile, zrtp_global_t* zrtp);

/**
 * \brief Search for a component in the profile by ID
 *
 * The utility function returning the position of an element of the specified  type in the profile. 
 * Used by libZRTP kernel and for external use.
 *
 * \param profile - ZRTP profile;
 * \param type - sought component type;
 * \param id - sought component ID.
 * \return
 *  - component position - if component was found;
 *  -1 - if the component with the specified ID can't be found in profile.
 */
int zrtp_profile_find(const zrtp_profile_t* profile, zrtp_crypto_comp_t type, uint8_t id);
	
/* \} */

/**
 * \defgroup zrtp_main_rng Random Number Generation
 * \ingroup zrtp_api
 * \{
 * The generation of cryptographic key material is a highly sensitive process. To do this, you need
 * high entropy random numbers that an attacker cannot predict. This section \ref rng gives basic
 * knowliges andbot the RNG and it's implementation in libzrtp.
 * \warning
 * \ref rng \c MUST be read by every developer using libzrtp.
 */
	
/**
 * \brief Entropy accumulation routine
 * 
 * The random number generation scheme is described in detail in chapter \ref XXX.  This function 
 * gets \c length bytes of entropy from \c buffer and hashes it into the special storage. This 
 * function should be called periodically from the user's space to increase entropy quality.
 * \warning
 *    RNG is a very important and sensitive component of the crypto-system. Please, pay attention to 
 *    \ref rng.
 * \param zrtp - libzrtp global context;
 * \param buffer - pointer to the buffer with entropy for accumulating;
 * \param length - entropy size in bytes.
 * \return: number of hashed bytes.
 */
int zrtp_entropy_add(zrtp_global_t* zrtp, const unsigned char *buffer, uint32_t length);

/**
 * \brief Random string generation 
 *
 * zrtp_randstr() generates \c length bytes of "random" data. We say "random" because the 
 * "randomness" of the generated sequence depends on the quality of the entropy passed to  
 * zrtp_entropy_add(). If the user provides "good" entropy, zrtp_randstr() generates sufficiently 
 * "random" data.
 *
 * \param zrtp - libzrtp global context;
 * \param buffer - buffer into which random data will be generated;
 * \param length - length of required sequence in bytes.
 * \return
 *  - length of generated sequence in bytes or -1 in case of error
 * \sa \ref rng
 */
int zrtp_randstr(zrtp_global_t* zrtp, unsigned char *buffer, uint32_t length);

int zrtp_randstr2(unsigned char *buffer, uint32_t length);

/* \} */

#if defined(__cplusplus)
}
#endif

#endif /* __ZRTP_H__ */

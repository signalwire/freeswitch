/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */



/**
 * \file zrtp_iface.h
 * \brief libzrtp product-dependent functions
 */

#ifndef __ZRTP_IFACE_H__ 
#define __ZRTP_IFACE_H__

#include "zrtp_config.h"
#include "zrtp_base.h"
#include "zrtp_string.h"
#include "zrtp_error.h"
#include "zrtp_iface_system.h"


#if defined(__cplusplus)
extern "C"
{
#endif
	
/*======================================================================*/
/*    libzrtp interface: Cache                                          */
/*======================================================================*/

/*!
 * \defgroup zrtp_iface_cache ZRTP Cache
 * \ingroup zrtp_iface
 *
 * The secret cache implementation should have a two-layer structure: each pair of ZIDs should have   
 * a relevant pair of secrets (current and previous). In addition to the value of the secret, the 
 * cache should contain: verification flag, last usage time-stamp and cache TTL value.
 *
 * The simplest secret cache scheme implementation is:
 * \code
 * [local_ZID][remote_ZID][curr_cache][prev_cache][verified][used at][cache ttl]
 * \endcode
 * \warning
 * Libzrtp doen't provide synchronization for cache read/write operation. Cache is not thread safe 
 * by default. Implementor must take care of synchronization inside his implementation.
 * 
 * For more information see corresponding section \ref XXX. Samples can be found at \ref XXX 
 * (\c zrtp_iface_builtin.h, \c zrtp_iface_cache.c)
 * \{
 */

/**
 * @brief Data types and functions related to shared secrets.
 */
typedef struct zrtp_callback_cache_t
{
	/**
	 * \brief Cache initialization.
	 * 
	 * libzrtp calls this function before start using cache routine at zrtp_init().
	 *
	 * \param zrtp - libzrtp global context;
	 * \sa zrtp_callback_cache_t#on_down()
	 */
	zrtp_status_t (*on_init)(zrtp_global_t* zrtp);

	/**
	 * \brief Cache deinitialization.
	 * 
	 * libzrtp calls this function  when zrtp cache is no longer needed at zrtp_down().
	 * \sa zrtp_callback_cache_t#on_init()
	 */
	void (*on_down)();
	
	/**
	 * \brief Add/Update cache value
	 * 
	 * Interface function for entering the retained secret to the cache. This function should
	 * guarantee permanent storage in the cache. The implementation algorithm is the following:
	 *  - if the entry associated with a given pair of ZIDs does not exist, the value should be
	 *    stored in cache. 
	 *  - if the entry already exists, the current secret value becomes stored as the previous one.
	 *    The new value becomes stored as the current one. Besides rss->value a timestamp 
	 *    (rss->lastused_at) and cache TTL(rss->ttl)  should be updated.
	 *
	 * \param one_zid - ZID of one side;
	 * \param another_zid - ZID of the other side;
	 * \param rss - a structure storing the value of the secret that needs to be saved.
	 * \return
	 * - zrtp_status_ok if operation is successful;
	 * - some error code from \ref zrtp_status_t in case of error.
	 * \sa zrtp_callback_cache_t#on_get
	 */
	zrtp_status_t (*on_put)( const zrtp_stringn_t* one_zid,
						     const zrtp_stringn_t* another_zid, 
						 	 zrtp_shared_secret_t *rss);
	
	/**
	 * \brief Return secret cache associated with specified pair of ZIDs.
	 *
	 * This function should return the secret associated with the specified pair of ZIDs. In
	 * addition to the secret value, TTL (rss->ttl) and cache timestamp (rss->lastused_at) value 
	 * should be also returned.
	 *
	 * \param one_zid - one side's ZID;
	 * \param another_zid - the other side's ZID;
	 * \param prev_requested - if this parameter value is 1, the function should return the previous
	 *    secret's value. If this parameter value is 0, the function should return the current
	 *    secret's value;
	 * \param rss - structure that needs to be filled in.
	 * \return
	 *  - zrtp_status_ok - if operation is successful;
	 *  - zrtp_status_fail - if the secret cannot be found;
	 *  - some error code from zrtp_status_t if an error occurred.
	 * \sa zrtp_callback_cache_t#on_put
	 */
	zrtp_status_t (*on_get)( const zrtp_stringn_t* one_zid,
							 const zrtp_stringn_t* another_zid,
							 zrtp_shared_secret_t *rss,
							 int prev_requested);
	
	/**
	 * \brief Set/clear cache verification flag
	 *
	 * This function should set the secret verification flag associated with a pair of ZIDs.
	 * \warning
	 *   For internal use only. To change the verification flag from the user space use the
	 *   zrtp_verified_set() function.
	 *
	 * \param one_zid - first ZID for cache identification;
	 * \param another_zid - second ZID for cache identification;
	 * \param verified - verification flag (value can be 0 or 1).
	 * \return
	 *  - zrtp_status_ok if flag is successfully modified;
	 *  - zrtp_status_fail if the secret cannot be found;
	 *  - some other error code from \ref zrtp_status_t if another error occurred.
	 */
	zrtp_status_t (*on_set_verified)( const zrtp_stringn_t* one_zid,
									  const zrtp_stringn_t* another_zid, 
									  uint32_t verified);
	
	/**
	 * \brief Return cache verification flag
	 *
	 * This function return the secret verification flag associated with a pair of ZIDs.
	 *
	 * \param one_zid - first ZID for cache identification;
	 * \param another_zid - second ZID for cache identification;
	 * \param verified - verification flag to be filled in
	 * \return
	 *  - zrtp_status_ok if flag is successfully returned;
	 *  - zrtp_status_fail if the secret cannot be found;
	 *  - some other error code from \ref zrtp_status_t if another error occurred.
	 */
	zrtp_status_t (*on_get_verified)( const zrtp_stringn_t* one_zid,
									  const zrtp_stringn_t* another_zid, 
									  uint32_t* verified);
	
	/**
	 * \brief Should set Secure Since cache aparemeter to current date and time
	 *
	 * This function is optional and may be ommited.
	 *
	 * \param one_zid - first ZID for cache identification;
	 * \param another_zid - second ZID for cache identification;
	 * \return
	 *  - zrtp_status_ok if the oprtation finished sucessfully.
	 *  - some other error code from \ref zrtp_status_t if another error occurred.
	 */
	zrtp_status_t (*on_reset_since)( const zrtp_stringn_t* one_zid,
									 const zrtp_stringn_t* another_zid);
	
	/**
	 *  \brief Add/Update cache value for MiTM endpoint	 
	 *
	 * This function is analogy to zrtp_callback_cache_t#on_put but for MiTM endpoint.
	 * \todo Add more detail description
	 * \sa zrtp_callback_cache_t#on_put zrtp_callback_cache_t#on_get_mitm 
	 */
	zrtp_status_t (*on_put_mitm)( const zrtp_stringn_t* one_zid,
								  const zrtp_stringn_t* another_zid, 
								  zrtp_shared_secret_t *rss);
	
	/**
	 * \brief Return secret cache for MiTM endpoint
	 *
	 * This function is analogy to zrtp_callback_cache_t#on_get but for MiTM endpoint.
	 * \todo Add more detail description
	 * \sa zrtp_callback_cache_t#on_get zrtp_callback_cache_t#on_put_mitm
	 */
	zrtp_status_t (*on_get_mitm)( const zrtp_stringn_t* one_zid,
								  const zrtp_stringn_t* another_zid,
								  zrtp_shared_secret_t *rss);
	
	/**
	 * \brief Return Preshared calls counter
	 *
	 * This function should return the preshared calls counter associated with a pair of ZIDs.
	 *
	 * \param one_zid - first ZID for cache identification;
	 * \param another_zid - second ZID for cache identification;
	 * \param counter - preshared calls counter to be filled in
	 * \return
	 *  - zrtp_status_ok if counter is successfully returned;
	 *  - zrtp_status_fail if the secret cannot be found;
	 *  - some other error code from \ref zrtp_status_t if another error occurred.
	 */
	zrtp_status_t (*on_presh_counter_get)( const zrtp_stringn_t* one_zid,
										   const zrtp_stringn_t* another_zid,
										   uint32_t* counter);
	
	/**
	 * \brief Increase/reset Preshared streams counter made between two endpoints (ZIDs)
	 *
	 * This function should set the preshared calls counter associated with a pair of ZIDs.
	 * Function is optional and should be implemented if your prodict uses Preshared keys exchange.
	 *
	 * \param one_zid - first ZID for;
	 * \param another_zid - second ZID;
	 * \param counter - Preshared calls counter.
	 * \return
	 *  - zrtp_status_ok if the counter is successfully modified;
	 *  - zrtp_status_fail if the secret cannot be found;
	 *  - some other error code from \ref zrtp_status_t if another error occurred.
	 */
	zrtp_status_t (*on_presh_counter_set)( const zrtp_stringn_t* one_zid,
										   const zrtp_stringn_t* another_zid,
										   uint32_t counter);
} zrtp_callback_cache_t;

	
/** \} */

/*======================================================================*/
/*    libzrtp interface: Scheduler                                      */
/*======================================================================*/

/**
 * \defgroup zrtp_iface_scheduler ZRTP Delay Calls 
 * \ingroup zrtp_iface
 *
 * Algorithm used in the scheduled call module is described in detail in section \ref XXX of the
 * developer's guide documentation. Technical details of this function's implementation follows.
 *
 * For more information see corresponding section \ref XXX. Samples can be found at \ref XXX 
 * (\c zrtp_iface_builtin.h, \c zrtp_iface_scheduler.c)
 * \{
 */

/** \brief ZRTP Delays Calls signature. */
typedef void (*zrtp_call_callback_t)(zrtp_stream_t*, zrtp_retry_task_t*);

/**
 * @brief Delay Call wrapper
 */
struct zrtp_retry_task_t
{
	/** \brief Task action callback */
	zrtp_call_callback_t	callback;
	
	/** \brief Timeout before call in milliseconds */
	zrtp_time_t				timeout;
	
	/**
	 * \brief User data pointer.
	 *
	 * Pointer to the user data. This pointer can be used for fast access to some additional data
	 * attached to this task by the user application.
	 */
	void*					usr_data;
	
	
	// TODO: hide these elements
	/**
	 * \brief Task activity flag.
	 *
	 * Libzrtp unsets this flag on task canceling. It prevents the scheduler engine from re-adding
	 * an already canceled task. Callback handlers skip passive tasks.
	 * \note
	 * For internal use only. Don't' modify this field in implementation.
	 */
	uint8_t					_is_enabled;
	
	/**
	 * \brief Number of task retries.
	 *
	 * Every handler that attempts the task increases it by one. When the limit is reached the
	 * scheduler should stop retries and performs a specified action - generally raises an error.
	 * \note
	 * For internal use only. Don't' modify this field in implementation.
	 */
	uint32_t				_retrys;
	
	/**
	 * \brief Task Busy flag.
	 * 
	 * Built-in cache implementation uses this flag to protect task from being removed during the 
	 * callback.
	 *
	 * Default cache implementation "locks" this flag before call zrtp_retry_task#callback 
	 * and "unlocks" when the call is performed. zrtp_callback_scheduler_t#on_wait_call_later exits 
	 * when there are no callbacks in progress - no tasks with \c _is_busy enabled.
	 */
	uint8_t					_is_busy;
};

/**
 * @brief Delay Calls callbacks
 */
typedef struct zrtp_callback_scheduler_t
{
	/**
	 * \brief Delay Calls initialization.
	 * 
	 * libzrtp calls this function before start using scheduler routine at zrtp_init().
	 *
	 * \param zrtp - libzrtp global context;
	 * \sa zrtp_callback_scheduler_t#on_down()
	 */
	zrtp_status_t (*on_init)(zrtp_global_t* zrtp);
	
	/**
	 * \brief Delay Calls deinitialization.
	 * 
	 * libzrtp calls this function  when zrtp scheduler is no longer needed at zrtp_down().
	 * \sa zrtp_callback_scheduler_t#on_init()
	 */
	void (*on_down)();

	/**
	 * \brief Interface for performing delay call
	 *
	 * This function should add delay call request (\c task) to the processing queue. When the 
	 * zrtp_retry_task_t#timeout is expired, scheduler should call zrtp_retry_task_t#callback and 
	 * remove tasks from the processing queue.
	 *
	 * \param stream - stream context for processing the callback function;
	 * \param task - task structure that should be processed. 
	 * \sa zrtp_callback_scheduler_t#on_cancel_call_later
	 */
	void (*on_call_later)(zrtp_stream_t *stream, zrtp_retry_task_t* task);
	
	/**
	 * \brief Interface for canceling a delay calls
	 *
	 * This function cancels delay call if it still in the processing queue. The algorithm is the 
	 * following:
	 *  - If there is a specified task for a specified stream, this task should be deleted.
	 *  - If the \c task parameter is equal to NULL - ALL tasks for the specified stream must be 
	 *    terminated and removed from the queue.
	 *
	 * \param ctx - stream context for the operation;
	 * \param task - delayed call wrapper structure.
	 * \sa zrtp_callback_scheduler_t#on_call_later
	 */
	void (*on_cancel_call_later)(zrtp_stream_t* ctx, zrtp_retry_task_t* task);
	
	/**
	 * \brief Interface for waiting for scheduling tasks is finished
	 *
	 * This function is called by libzrtp when the state-mamchine is in a position to destroy ZRTP
	 * session and all incapsulated streams. Allocated for the stream memory may be cleared and
	 * released. If after this operation, scheduler perform time-out call it will bring system to
	 * crash.
	 *
	 * The scheduler implementation must guarantee that any delay call for the \c stream will not be 
	 * performed after on_wait_call_later().
	 *	 
	 * \param stream - stream context for the operation;
	 * \sa zrtp_callback_scheduler_t#on_call_later.
	 */
	void (*on_wait_call_later)(zrtp_stream_t* stream);
} zrtp_callback_scheduler_t;

/** \} */

/*======================================================================*/
/*    libzrtp interface: Protocol                                       */
/*======================================================================*/

/**
 * \defgroup zrtp_iface_proto ZRTP Protocol Feedback
 * \ingroup zrtp_iface
 *
 * This section defines ZRTP protcol events. Detail description of ZRTP state-machine is defined in 
 * \ref XXX.
 * \{
 */

/**
 * \brief ZRTP Protocol events
 *
 * For additional information see \ref XXX
 */
typedef enum zrtp_protocol_event_t
{
	/** \brief Just a stub for error detection. */
	ZRTP_EVENT_UNSUPPORTED = 0,
	
	/** \brief Switching to CLEAR state */
	ZRTP_EVENT_IS_CLEAR,

	/** \brief Switching to INITIATING_SECURE state */
	ZRTP_EVENT_IS_INITIATINGSECURE,
	
	/** \brief Switching to PENDING_SECURE state */
	ZRTP_EVENT_IS_PENDINGSECURE,
	
	/** \brief Switching to PENDING_CLEAR state */
	ZRTP_EVENT_IS_PENDINGCLEAR,
	
	/**
	 * \brief Switching to NO_ZRTP state.
	 * 
	 * Hello packet undelivered - no ZRTP endpoint and other end
	 */	 
	ZRTP_EVENT_NO_ZRTP,
	
	/**
	 * \brief First N Hello packet undelivered - probably, no ZRTP endpoint and other end
	 *
	 * Libzrtp raises this event after few Hello have been send without receiving response from the
	 * remote endpoint. User application may use this event to stop Securing ritual if connection
	 * lag is important.
	 *
	 * Developer should take into account that delays in Hello receiving may be conditioned by 
	 * interruptions in media channel
	 *
	 * \warning Don't handle this event unless necessary
	 */
	ZRTP_EVENT_NO_ZRTP_QUICK,
	
	/**
	 * \brief MiTM Enrollment with MiTM endpoint
	 *
	 * Informs the Client-side endpoint of receiving a registration invitation from the MiTM.
	 * Libzrtp raises this event after switching to the Secure state (ZRTP_EVENT_IS_SECURE). The
	 * user may accept the invitation using a zrtp_register_with_trusted_mitm() call.
	 */
	ZRTP_EVENT_IS_CLIENT_ENROLLMENT,
	
	/**
	 * \brief New user has registered to the MitM
	 *
	 * Informs MitM of the registration of a new user. Libzrtp raises this event when a user calls
	 * the special registration number and has switched to the secure state.
	 */
	ZRTP_EVENT_NEW_USER_ENROLLED,
	
	/**
	 * \brief New user has already registered with the MiTM
	 *
	 * Notifies the MiTM of an attempt to register from a user that is already registered. In this
	 * case a new MiTM secret will not be generated and the user may be informed by voice prompt.
	 * Libzrtp raises this event from the SECURE state.
	 */
	ZRTP_EVENT_USER_ALREADY_ENROLLED,
	
	/**
	 * \brief User has cancelled registration
	 *
	 * Libzrtp may raise this event during regular calls when it discovers that the user has removed
	 * its MiTM secret. This event informs the MiTM that the SAS can no longer be transferred to
	 * this user.
	 */
	ZRTP_EVENT_USER_UNENROLLED,
	
	/**
	 * \brief SAS value and/or rendering scheme was updated
	 *
	 * LibZRTP raises this event when the SAS value is transferred from the trusted MiTM. The value
	 * is rendered automatically according to the rendering scheme specified by the trusted MiTM.
	 * (it may be different than that of the previous one).
	 *
	 * On receiving this event, the Client application should replace the old SAS with the new one 
	 * and ask the user to verify it. This event is called from the Secure state only.
	 */
	ZRTP_EVENT_LOCAL_SAS_UPDATED,
	
	/**
	 * \brief SAS transfer was accepted by the remote side
	 *
	 * Libzrtp raises this event to inform the Server-side about accepting the change of SAS value
	 * and/or rendering scheme by the remote client. This event is called from the Secure state 
	 * only.
	 */
	ZRTP_EVENT_REMOTE_SAS_UPDATED,
	
	/**
	 * \brief Swishing to SECURE state
	 * 
	 * Duplicates zrtp_callback_event_t#on_zrtp_secure for more thin adjustments.
	 */
	ZRTP_EVENT_IS_SECURE,
	
	/**
	 * \brief Swishing to SECURE state is finished.
	 *
	 * Equal to ZRTP_EVENT_IS_SECURE but called when the Securing process is completely finished: 
	 * new RS secret is generate, cache flags updated and etc. Can be used in extended application 
	 * for more thin adjustments.
	 */
	ZRTP_EVENT_IS_SECURE_DONE,
	
	/**
	  * \brief Indicates DRM restriction. Stream can't go Secure.
	  * 
	  * Libzrtp generate this event if DRM rules don't allow to switch to Secure mode:
	  * - A passive endpoint never sends a Commit message. Semi-active endpoint does not send a
	  *   Commit to a passive endpoint
	  * - A passive phone, if acting as a SIP initiator r ejects all commit packets from everyone.
	  * - A passive phone rejects all commit messages from a PBX.
	  */
	ZRTP_EVENT_IS_PASSIVE_RESTRICTION,
	
	ZRTP_EVENT_COUNT

} zrtp_protocol_event_t;

/**
 * \brief ZRTP Protocol Errors and Warnings
 *
 * For additional information see \ref XXX
 */
typedef enum zrtp_security_event_t
{
	/**
	 * \brief Switching to ERROR state 
	 *
	 * The exact error code can be found at zrtp_stream_info_t#last_error. Use zrtp_log_error2str() 
	 * to get error description in text mode.
	 */
	ZRTP_EVENT_PROTOCOL_ERROR = ZRTP_EVENT_COUNT,
	
	/**
	 * \brief Hello Hash is different from that received in signaling.
	 *
	 * In accordance with sec. 8.1 of the ZRTP RFC, libzrtp provides the ability to prevent DOS
	 * attacks. libzrtp can detect an attack in which the hash of the remote Hello was received
	 * through signaling and added to the ZRTP context (zrtp_signaling_hash_set()).
	 *
	 * When the hash of the incoming Hello doesn't match the hash from signaling, the 
	 * ZRTP_EVENT_WRONG_SIGNALING_HASH event is raised and the connection MAY be terminated 
	 * manually.
	 */
	ZRTP_EVENT_WRONG_SIGNALING_HASH,
	
	/**
	 * \brief Hmac of the received packet is different from the hmac value earlier received.
	 *
	 * If the Hello hash is sent through protected signaling, libzrtp provides the ability to
	 * prevent protocol packets from modification and even eliminates comparing the SAS. To do this,
	 * libzrtp compares the message Hmac with the Hmac received in the previous message.
	 * 
	 * If the Hmacs don't match, the ZRTP_EVENT_WRONG_MESSAGE_HMAC event is raised and the 
	 * connection MAY be terminated manually.
	 */
	ZRTP_EVENT_WRONG_MESSAGE_HMAC,
	
	/**
	 * \brief Retain secret was found in the cache but it doesn't match with the remote one
	 *
	 * The library rises this event when non-expired secret have been found in the cache but
	 * value of the secret doesn't match with the remote side secret. Such situation may happen
	 * in case of MiTM attack or when remote side lost it's cache.
	 *
	 * Recommended behavior: the application should notify user about the situation and ask him to
	 * verify the SAS. If SAS is different - it indicates the attack.
	 */
	ZRTP_EVENT_MITM_WARNING
} zrtp_security_event_t;

/**
 * \brief Callbacks definitions
 *
 * This section lists callback functions informing the user about the protocol status. These 
 * callbacks must be defined in the user application.
 */
typedef struct zrtp_callback_event_t
{
	/**
	 * \brief ZRTP Protocol events notification.
	 *
	 * Informs about switching between the protocol states and other events. Provides more flexible 
	 * control over the protocol then on_zrtp_secure and on_zrtp_not_secure.
	 *
	 * \param event - type of event;
	 * \param stream - ZRTP stream context.
	 */
	void (*on_zrtp_protocol_event)(zrtp_stream_t *stream, zrtp_protocol_event_t event);
	
	/**
	 * \brief ZRTP Security events notification
	 *
	 * Informs about ZRTP security events: MiTM attacks, cache desynchronization and
	 * others.
	 * \warning MUST be handled in the target application to provide high security level.
	 *
	 * \param event - type of event;
	 * \param stream - ZRTP stream context.
	 */
	void (*on_zrtp_security_event)(zrtp_stream_t *stream, zrtp_security_event_t event);
	
	/**
	 * \brief Indicates switching to SECURE state.
	 * 
	 * Pair of events: \c on_zrtp_secure and \c on_zrtp_not_secure represent simplified event 
	 * handling mechanism comparing to \c on_zrtp_protocol_event. libzrtp calls this event when the 
	 * call is SECURE and media is encrypted.
	 *
	 * SAS Verification is required on this event.
	 *
	 * \param stream - ZRTP stream context.
	 */
	void (*on_zrtp_secure)(zrtp_stream_t *stream);
	
	/**
	 * \brief Indicates switching to NOT SECURE state.
	 *
	 * This event duplicates some protocol and security events to simplify libzrtp usage. It may be
	 * used in applications which don't require detail information about ZRTP protocol.
	 *
	 * If Error appeared - the exact error code can be found at zrtp_stream_info_t#last_error. Use 
	 * zrtp_log_error2str() to get error description in text mode.
	 *
	 * \param stream - ZRTP stream context.
	 */
	void (*on_zrtp_not_secure)(zrtp_stream_t *stream);
} zrtp_callback_event_t;

/** \} */

/*======================================================================*/
/*    libzrtp interface: Misc                                           */
/*======================================================================*/

/**
 * \defgroup zrtp_iface_misc Miscellaneous functions
 * \ingroup zrtp_iface
 * \{
 */

/**
 * \brief Miscellaneous Functions
 */
typedef struct zrtp_callback_misc_t
{
	/**
	 * \brief RTP packet sending function 
	 *
	 * This function pushes an outgoing ZRTP packet to the network. Correct building of IP and UPD
	 * headers is the developer's responsibility.
	 *
	 * \param stream - ZRTP stream context;
	 * \param packet - buffer storing the ZRTP packet to send;
	 * \param length - size of the ZRTP packet.
	 * \return
	 *  - number of bytes sent if successful;
	 *  - -1 if error occurred.
	 */
	int (*on_send_packet)(const zrtp_stream_t* stream, char* packet, unsigned int length);
} zrtp_callback_misc_t;

/** \} */

/**
 * \brief ZRTP feedback interface and application dependent routine
 * \ingroup zrtp_iface
 */
typedef struct zrtp_callback_t
{
	/** \brief ZRTP Protocol Feedback */
	zrtp_callback_event_t		event_cb;
	/** \brief ZRTP Delay Calls routine */
	zrtp_callback_scheduler_t	sched_cb;
	/** \brief ZRTP Cache */
	zrtp_callback_cache_t		cache_cb;
	/** \brief Miscellaneous functions */
	zrtp_callback_misc_t		misc_cb;
} zrtp_callback_t;

	
#if defined(__cplusplus)
}
#endif

#endif /*__ZRTP_IFACE_H__*/

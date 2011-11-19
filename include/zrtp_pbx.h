/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */

 
/**
 * \file zrtp_pbx.h
 * \brief Defines basic Functions to work with MiTM endpoints
 */

#ifndef __ZRTP_PBX_H__
#define __ZRTP_PBX_H__

#include "zrtp_config.h"
#include "zrtp_types.h"

#if defined(__cplusplus)
extern "C"
{
#endif

/**
 * \defgroup zrtp_api_mitm PBX related functions and data types
 * \ingroup zrtp_api
 *
 * In this section the basic functions for using the library in MiTM mode
 * environment. Asterisk PBX, for example.  
 * \{
 */

/**
 * \brief Start ZRTP enrollment ritual on Server side
 *
 * This is the equivalent of zrtp_stream_start() but for MiTM endpoints. By calling 
 * zrtp_stream_registration_start() libzrtp prepares to engage in the enrollment ritual: send 
 * special flag in Confirm packet and prepare for generating the MiTM secret.
 * \return 
 *  - zrtp_status_ok - if operation started successfully;
 *  - one of zrtp_status_t errorrs in other case.
 * \sa zrtp_callback_event_t#on_zrtp_protocol_event
 * \sa zrtp_event_t (PBX related definitions)
 */
zrtp_status_t zrtp_stream_registration_start(zrtp_stream_t* stream, uint32_t ssrc);

/**
 * \brief Continue ZRTP enrollment ritual (from CLEAR state) on Server side.
 *
 * This is equivalent to zrtp_stream_secure() but with enrollment ritual. Use this function instead 
 * of zrtp_stream_registration_start() in case when "autosecure" option is disabled for some reason. 
 * \return 
 *  - zrtp_status_ok - if operation started successfully;
 *  - one of zrtp_status_t errorrs in other case.
 */
zrtp_status_t zrtp_stream_registration_secure(zrtp_stream_t* stream);

/**
 * \brief Confirms enrollment ritual on Client side
 *
 * Invocation of this function by event zrtp_protocol_event_t#ZRTP_EVENT_IS_CLIENT_ENROLLMENT
 * confirms enrollment process; libzrtp generates special secret which will be used to "Sign" all 
 * further calls with the trusted MiTM.
 * \return 
 *  - zrtp_status_ok - in case when enrollment was completed successfully;
 *  - zrtp_status_fail - in case of error: wrong protocol state or system error.
 */
zrtp_status_t zrtp_register_with_trusted_mitm(zrtp_stream_t* stream);

/**
 * \brief Automatically handle ZRTP call in PBX environment
 *
 * This function may be called to handle ZRTP call between two ZRTP endpoints  through PBX. As 
 * described in ID sec 8.3., there are several problems with ZRTP in PBX environment. 
 * zrtp_resolve_mitm_call() implements several steps to resolve such problems:
 *  - detect enrolled and non enrolled endpoint. If both sides are enrolled - one side for the SAS 
 *    transfer will be chousen automatically;
 *  - start SAS transfer with the enrolled endpoint;
 *  - update flags and SAS rendering scheme if necessary.
 * In other words: After switching to SECURE state, this is the one function which ZRTP MiTM 
 * endpoint should call to handle ZRTP call correctly. If you want to have more flexability in MiTM 
 * mode - resolve ambiguity manually using functions listed below. 
 * \param stream1 - one party of ZRTP call (must be in secure state already);
 * \param stream2 - other party of ZRTP call (must be in secure state already).
 * \return 
 *  - zrtp_status_ok - if operation started successfully;
 *  - one of zrtp_status_t errors in other case.
 * \ref XXX_DRAFT, XXX_GUIDE
 */
zrtp_status_t zrtp_resolve_mitm_call(zrtp_stream_t* stream1, zrtp_stream_t* stream2);
	
/**
 * @brief Links two lags of Trusted ZRTP MiTM call together.
 * 
 * This function allows libzrtp2 to optimize protocol behavior of one leg depending on the state and
 * parameters of the other lag. MitM boxes should use this API whenever possible.
 *
 * @param stream1 - one leg of the trusted MiTM call;
 * @param stream2 - another leg of the trusted MiTM call.
 *
 * @return zrtp_status_ok in case of success.
 */
zrtp_status_t zrtp_link_mitm_calls(zrtp_stream_t* stream1, zrtp_stream_t* stream2);

/**
 * \brief Updates remote-side SAS value and rendering scheme
 * 
 * zrtp_update_remote_sas() initiates process of "SAS transferring" between trusted MiTM and user. 
 * It allows to change as SAS rendering scheme as a SAS value and related flags as well. It the MiTM 
 * needs to update just one of the parameters - the other one should be set to NULL. libzrtp informs 
 * about status of the SAS updating through zrtp_protocol_event_t::ZRTP_EVENT_REMOTE_SAS_UPDATED.
 * Call this function in SECURE state only.
 * \param stream - zrtp endpoint stream to update;
 * \param transf_sas_scheme - chosen SAS rendering scheme;
 * \param transf_sas_value - relaying SAS value (full sas hash);
 * \param transf_ac_flag - relaying "allowclear" flag;
 * \param transf_d_flag - relaying "disclose" flag.
  * \return 
 *  - zrtp_status_ok - if operation started successfully;
 *  - one of zrtp_status_t errors in other case.
 */
zrtp_status_t zrtp_update_remote_options( zrtp_stream_t* stream,
										  zrtp_sas_id_t transf_sas_scheme,
										  zrtp_string32_t* transf_sas_value,
										  uint8_t transf_ac_flag,
										  uint8_t transf_d_flag );

/**
 * \brief Check if user at the end of the stream \c stream is enrolled
 * \param stream - stream for examining.
 * \return: 1 if user is enrolled and 0 in other case
 */
uint8_t zrtp_is_user_enrolled(zrtp_stream_t* stream);

/**
 * \brief Choose single enrolled stream from two enrolled
 *
 * This function may be used to resolve ambiguity with call transferring between two enrolled users.
 * \return stream which shuld be used for SAS transferring
 */
zrtp_stream_t* zrtp_choose_one_enrolled(zrtp_stream_t* stream1, zrtp_stream_t* stream2);

/* \} */

#if defined(__cplusplus)
}
#endif

#endif

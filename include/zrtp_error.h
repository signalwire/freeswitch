/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */


/**
 * \file zrtp_error.h
 * \brief libzrtp errors definitions
 */

#ifndef __ZRTP_ERROR_H__
#define __ZRTP_ERROR_H__

#include "zrtp_config.h"

/**
 * \defgroup zrtp_errors Libzrtp Error Definitions
 *
 * In this section the ZRTP protocol error codes and the library internal errors are defined.
 *
 * When ZRTP Protocl error detected, zrtp_callback_event_t#on_zrtp_security_event is called and 
 * zrtp_session_info_t#last_error contains error code.
 * \{
 */
 
/**
 * \brief Define protocol error codes according to ZRTP RFC sec. 5.9
 */
typedef enum zrtp_protocol_error_t
{
	zrtp_error_unknown			= 0,
	zrtp_error_timeout			= 1,
	
	zrtp_error_invalid_packet	= 0x10, /** Malformed packet (CRC OK, but wrong structure) */
	zrtp_error_software			= 0x20, /** Critical software error */
	zrtp_error_version			= 0x30, /** Unsupported ZRTP version */
	zrtp_error_hello_mistmatch	= 0x40, /** Hello components mismatch */
		
	zrtp_error_hash_unsp		= 0x51,	/** Hash type not supported */	
	zrtp_error_cipher_unsp		= 0x52,	/** Cipher type not supported */
	zrtp_error_pktype_unsp		= 0x53, /** Public key exchange not supported */
	zrtp_error_auth_unsp		= 0x54, /** SRTP auth. tag not supported */
	zrtp_error_sas_unsp			= 0x55, /** SAS scheme not supported */
	zrtp_error_no_secret		= 0x56, /** No shared secret available, Preshared mode required */

	zrtp_error_possible_mitm1	= 0x61, /** DH Error: bad pvi or pvr ( == 1, 0, or p-1) */
	zrtp_error_possible_mitm2	= 0x62,	/** DH Error: hvi != hashed data */
	zrtp_error_possible_mitm3	= 0x63, /** Received relayed SAS from untrusted MiTM */

	zrtp_error_auth_decrypt		= 0x70, /** Auth. Error: Bad Confirm pkt HMAC */
	zrtp_error_nonse_reuse		= 0x80, /** Nonce reuse */
	zrtp_error_equal_zid		= 0x90, /** Equal ZIDs in Hello */
	zrtp_error_service_unavail	= 0xA0,	/** Service unavailable */
	zrtp_error_goclear_unsp		= 0x100,/** GoClear packet received, but not allowed */	
	
	zrtp_error_wrong_zid		= 0x202, /** ZID received in new Hello doesn't equal to ZID from the previous stream */
	zrtp_error_wrong_meshmac	= 0x203, /** Message HMAC doesn't match with pre-received one */
	zrtp_error_count
} zrtp_protocol_error_t;

/**
 * \brief libzrtp functions statuses.
 *
 * Note that the value of zrtp_status_ok is equal to zero. This can simplify error checking 
 * somewhat.
 */
typedef enum zrtp_status_t
{
    zrtp_status_ok           = 0,	/** OK status */
    zrtp_status_fail         = 1,	/** General, unspecified failure */
    zrtp_status_bad_param    = 2,	/** Wrong, unsupported parameter */
    zrtp_status_alloc_fail   = 3,	/** Fail allocate memory */	
    zrtp_status_auth_fail    = 4,	/** SRTP authentication failure */
    zrtp_status_cipher_fail  = 5,	/** Cipher failure on RTP encrypt/decrypt */	
    zrtp_status_algo_fail    = 6,	/** General Crypto Algorithm failure */
    zrtp_status_key_expired  = 7,	/** SRTP can't use key any longer */
    zrtp_status_buffer_size  = 8,	/** Input buffer too small */
    zrtp_status_drop         = 9,	/** Packet process DROP status */
    zrtp_status_open_fail    = 10,	/** Failed to open file/device */
    zrtp_status_read_fail    = 11,	/** Unable to read data from the file/stream */
    zrtp_status_write_fail   = 12,	/** Unable to write to the file/stream */
	zrtp_status_old_pkt	     = 13,	/** SRTP packet is out of sliding window */
	zrtp_status_rp_fail		 = 14,	/** RTP replay protection failed */
	zrtp_status_zrp_fail	 = 15,	/** ZRTP replay protection failed */
	zrtp_status_crc_fail	 = 16,	/** ZRTP packet CRC is wrong */	
	zrtp_status_rng_fail	 = 17,	/** Can't generate random value */	
	zrtp_status_wrong_state	 = 18,	/** Illegal operation in current state */
	zrtp_status_attack		 = 19,	/** Attack detected */
	zrtp_status_notavailable = 20,	/** Function is not available in current configuration  */
	zrtp_status_count		 = 21
} zrtp_status_t;

/** \} */

/** \manonly */

#define ZRTP_MIM2_WARNING_STR \
    "Possible Man-In-The-Middle-Attack! Switching to state Error\n"\
    "because a packet arrived that was ZRTP_DHPART2, but contained\n"\
    "a g^y that didn't match the previous ZRTP_COMMIT.\n"

#define ZRTP_MITM1_WARNING_STR "DH validating failed. (pvi is 1 or p-1), aborted\n"

#define ZRTP_VERIFIED_INIT_WARNING_STR \
    "Falling back to cleartext because a packet arrived that was\n"\
	"ZRTP_CONFIRM1, but which couldn't be verified - the sender must have a different\n"\
	"shared secret than we have.\n"

#define ZRTP_VERIFIED_RESP_WARNING_STR \
    "Falling back to cleartext because a packet arrived that was ZRTP_CONFIRM2,\n"\
    " but which couldn't be verified - the sender must have a different shared secret than we have.\n"

#define ZRTP_EQUAL_ZID_WARNING_STR \
    "Received a ZRTP_HELLO packet with the same ZRTP ID that we have.\n"\
    " This is likely due to a bug in the software. Ignoring the ZRTP_HELLO\n"\
    " packet, therefore this call cannot be encrypted.\n"

#define ZRTP_UNSUPPORTED_COMP_WARNING_STR \
    " Received ZRTP_HELLO packet with an algorithms field which had a\n"\
    " list of hashes that didn't include any of our supported hashes. Ignoring\n"\
    " the ZRTP_HELLO packet, therefore this call cannot be encrypted.\n"
    
#define ZRTP_NOT_UNIQUE_NONCE_WARNING_STR \
    " Received COMMIT with hash value already used in another stream within this ZRTP session\n"

#define ZRTP_RELAYED_SAS_FROM_NONMITM_STR \
" Received SAS Relaying message from endpoint which haven't introduced as MiTM.\n"

/** \endmanonly */

#endif /* __ZRTP_ERROR_H__ */

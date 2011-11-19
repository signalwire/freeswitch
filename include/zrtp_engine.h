/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */


#ifndef __ZRTP_ENGINE_H__
#define __ZRTP_ENGINE_H__

#include "zrtp_config.h"
#include "zrtp_types.h"
#include "zrtp_crypto.h"


#if defined(__cplusplus)
extern "C"
{
#endif
	
/**
 * @defgroup engine_dev ZRTP Engine related types and functions
 * @ingroup zrtp_dev
 * \{
 */

#define ZRTP_IS_STREAM_DH(stream) \
(stream->mode == ZRTP_STREAM_MODE_DH)
	
#define ZRTP_IS_STREAM_FAST(stream) \
(stream->mode != ZRTP_STREAM_MODE_DH)
	
#define ZRTP_IS_STREAM_MULT(stream) \
(stream->mode == ZRTP_STREAM_MODE_MULT)
	
#define ZRTP_IS_STREAM_PRESH(stream) \
(stream->mode == ZRTP_STREAM_MODE_PRESHARED)

	
/**
 * @brief Test Passive Rule N1
 * A passive endpoint never sends a Commit message. Semi-active endpoint does
 * not send a commit to a passive endpoint.
 * Return 1 if the tast have been passed successfully and 0 in other case.
 */	
#define ZRTP_PASSIVE1_TEST(stream) \
( (ZRTP_LICENSE_MODE_UNLIMITED == stream->zrtp->lic_mode) || \
  ((ZRTP_LICENSE_MODE_ACTIVE == stream->zrtp->lic_mode) && (!stream->messages.peer_hello.pasive)) )

/**
 * @brief Test Passive Rule N2
 * A passive phone, if acting as a SIP initiator (meaning it initiated the call),
 * rejects all commit packets from everyone.
 * Return 1 if the tast have been passed successfully and 0 in other case
 */
#define ZRTP_PASSIVE2_TEST(stream) \
( !((ZRTP_LICENSE_MODE_PASSIVE == stream->zrtp->lic_mode) && \
	(stream->session->signaling_role == ZRTP_SIGNALING_ROLE_INITIATOR)) )
	
/**
 * @brief Test Passive Rule N3
 * A passive phone rejects all commit messages from a PBX.
 * Return 1 if the tast have been passed successfully and 0 in other case
 */	
#define ZRTP_PASSIVE3_TEST(stream) \
( !(!stream->zrtp->is_mitm && stream->peer_mitm_flag && \
    (ZRTP_LICENSE_MODE_PASSIVE == stream->zrtp->lic_mode)) )


/*===========================================================================*/
/*	PROTOCOL Logic														     */
/*===========================================================================*/
	
/**
 * @brief Allocate ZRTP protocol structure 
 * Allocates and initializes all necessary data according to the protocol mode.
 * Initializes required DH crypto context info and generates secret IDs.
 * @param  stream -		 stream context in which protocol should be allocated;
 * @param is_initiator - defines protocol type (1 - initiator, 0 - responder).
 * @exception SOFTWARE exceptions.
 */
zrtp_status_t _zrtp_protocol_init( zrtp_stream_t *stream,
								   uint8_t is_initiator,
								   zrtp_protocol_t **proto);

/**
 * @brief Release protocol structure
 * Stops all replay tasks, clears all crypto sources and SRTP engine, and
 * releases memory. The protocol should be destroyed on: stream closing, or
 * switching to CLEAR or ERROR states.
 */
void _zrtp_protocol_destroy(zrtp_protocol_t *proto);
	
/**
 * @brief Encrypts RTP/RTCP media
 * After switching to Secure, the protocol structure is able to encrypt
 * media using the SRTP crypto-engine.
 * @param self -	self-pointer to protocol instance;
 * @param packet -	media packet for encryption;
 * @param is_rtp -	defines type of media for encryption; value equal to 1
 *    means RTP packet, 0 - RTCP.
 * @return
 *	- zrtp_status_ok - if successfully encrypted;
 *	- one of zrtp_status_t errors otherwise.
 */
zrtp_status_t _zrtp_protocol_encrypt( zrtp_protocol_t *proto,
									  zrtp_rtp_info_t *packet,
									  uint8_t is_rtp);

/**
 * @brief Decrypts RTP/RTCP media
 * After switching to Secure, the protocol structure is able to decrypt
 * media using the SRTP crypto-engine.
 * @param self -	self-pointer to protocol instance;
 * @param packet -	media packet for decryption;
 * @param is_rtp -	defines type of media for decryption; value equal to 1
 *					means RTP packet, 0 - RTCP.
 * @return
 *	- zrtp_status_ok - if successfully decrypted;
 *	- one of zrtp_status_t errors otherwise.
 */
zrtp_status_t _zrtp_protocol_decrypt( zrtp_protocol_t *self,
									  zrtp_rtp_info_t *packet,
									  uint8_t is_rtp);
	
	
/*===========================================================================*/
/*	CRTPTO Utilities														     */
/*===========================================================================*/

/**
 * ZRTP KDF function.
 * KDF(KI, Label, Context, L) = HMAC(KI, i | Label | 0x00 | Context | L). See
 * Section "4.5.1. The ZRTP Key Derivation Function" in ZRTP RFC for more info.
 * @param stream -	used to obtain negotiated HMAC function and other parameters;
 * @param ki-		secret key derivation key that is unknown to the wiretapper
 *					(for example, s0);
 * @param label -	string of nonzero octets that identifies the purpose for the
 *					derived keying material;
 * @param context -	includes ZIDi, ZIDr, and some optional nonce material;
 * @param length -	needed digest length. (The output of the KDF is truncated to
 *					the leftmost length bits);
 * @param digest -	destination buffer.
 */
zrtp_status_t _zrtp_kdf( zrtp_stream_t* stream,
						 zrtp_stringn_t* ki,
						 zrtp_stringn_t* label, 
						 zrtp_stringn_t* context,
						 uint32_t length,
						 zrtp_stringn_t* digest);
	
/*!
 * \brief Allocate shared secret structure
 * This function allocates memory for a zrtp_shared_secret_t and initializes
 * the secret value using a zrtp_fill_shared_secret() function call. Used in
 * protocol allocating.
 * \param session - ZRTP session for access to global data.
 * \return
 *	- allocated secrets - on success;
 *	- NULL - if allocation fails.
 */
zrtp_shared_secret_t *_zrtp_alloc_shared_secret(zrtp_session_t* session);
	
/*!
 * \brief Restores secrets from the cache
 * Uploads retained secrets from the cache and initializes secret flags. If
 * the secret has expired (is_expired flag is set), its value will be randomly
 * regenerated.  _zrtp_prepare_secrets() is called after the discovery phase on
 * the setting up the very first stream. After secrets are uploaded the
 * zrtp_secrets_t#_is_ready flag is enabled to prevent secrets from reinitialization
 * on setting up the next stream.
 * \param session - ZRTP session in which secrets should be restored.
 *	- zrtp_status_ok - if secrets were restored successfully;
 *	- one of zrtp_status_t errors in case of failure.
 */
zrtp_status_t _zrtp_prepare_secrets(zrtp_session_t* session);
	
/**
 * @brief Validate confirm chmac message.
 * In case of chmac failure it switches to Initiating Error state and generate
 * ZRTP_EVENT_WRONG_MESSAGE_HMAC security event.
 * @return
 *	-1 - in case of error and 0 - on success.
 */
int _zrtp_validate_message_hmac(zrtp_stream_t *stream, zrtp_msg_hdr_t* msg2check, char* hmackey);

/**
 * @brief Computes preshared key using available secrets.
 * hash(len(rs1) | rs1 | len(auxsecret) | auxsecret | len(pbxsecret) | pbxsecret)
 * Result key stored in key variable, if key_id not NULL - hmac
 * of the preshared_key will be stored.
 * return
 *	- zrtp_status_ok on success and one of libzrtp errors in case of failure
 */
zrtp_status_t _zrtp_compute_preshared_key( zrtp_session_t *session,										  
										   zrtp_stringn_t* rs1,
										   zrtp_stringn_t* auxs,
										   zrtp_stringn_t* pbxs,
										   zrtp_stringn_t* key,
										   zrtp_stringn_t* key_id);

/** @brief Perform Key generation according to ZRTp RFC sec. 5.6 */
zrtp_status_t _zrtp_set_public_value(zrtp_stream_t *stream, int is_initiator);


/*===========================================================================*/
/*	PROTOCOL Utilites													     */
/*===========================================================================*/
	
/*!
 * \brief Check availability to start stream (DH or Preshared)
 * The ZRTP specification says that only one DH stream can be run at a time between
 * two ZRTP endpoints. So _zrtp_can_start_stream(DH) looks over all sessions
 * between two ZIDs and if any other stream is running it denies the start of
 * another DH stream in parallel. Although the ZRTP standard says that Preshared
 * or Multistream stream can't be run in parallel with DH streams between two
 * ZRTP endpoints. So _zrtp_can_start_stream(PRESH) looks over all sessions between
 * two ZIDs and if any other DH stream is running it denies the start of 
 * Preshared/Multistream stream in parallel. All operations with sessions and
 * streams are protected by mutexes. Call this function every time before starting
 * "initiating secure" process. For internal use only.
 * \sa "break the tie schemes" internal document.
 * \param stream - ZRTP stream which going to be started;
 * \param conc - in this variable _zrtp_can_start_stream() returns pointer to the
 *    concurrent DH stream if it's in progress. It's used in "breaking the tie"
 *    scheme.
 * \param mode - stream mode.
 * \return
 *	- 1 if stream can be started;
 *	- 0 - if stream can't be started and should wait for concurrent stream
 *    establishment.
 */
int _zrtp_can_start_stream( zrtp_stream_t* stream,
						    zrtp_stream_t** conc,
						    zrtp_stream_mode_t mode);

/** Return ZRTP Stream mode which should be used for current stream. */
zrtp_stream_mode_t _zrtp_define_stream_mode(zrtp_stream_t* stream);

/*!
 * \brief Chooses the best crypto component of the given type
 * Selects the crypto component according to the local initiator's profile and
 * the remote responder's Hello.
 * \param profile - local profile;
 * \param peer_hello - Hello packet, received from the remote peer;
 * \param type - type of the crypto component to be chosen.
 * \return:
 * 	- identifier of the chosen component (according to type);
 * 	- ZRTP_COMP_UNKN in case of error.
 */
uint8_t _zrtp_choose_best_comp( zrtp_profile_t* profile,
							    zrtp_packet_Hello_t* peer_hello,
							    zrtp_crypto_comp_t type);

/*!
 * \brief Computes replay timeouts
 * This function computes messages replays schedule. There are some recommended
 * values by ZRTP specification, but in some network environments values may be
 * sligh different
 */
uint32_t _zrtp_get_timeout(uint32_t curr_timeout, zrtp_msg_type_t msg);


/*!
 * \brief Terminates retransmission task
 * This function is a wrapper around zrtp_cancele_send_packet_later() which
 * unsets the zrtp_retry_task_t#_is_enabled flag to prevent the scheduler from
 * re-adding tasks after their termination.
 */
void _zrtp_cancel_send_packet_later( zrtp_stream_t* stream,
									 zrtp_msg_type_t type);

/*!
 * \brief state switcher
 * This function changes stream state to \c state, makes a backup of the previous
 * state at zrtp_stream_t#_prev_state and prints debug information.
 * \warning Don't change the stream state directly. Use this function.
 * \param stream - ZRTP stream to be changed;
 * \param state - new state.
 */
void _zrtp_change_state( zrtp_stream_t* stream, zrtp_state_t state);


/*===========================================================================*/
/*	Shared STATE-MACHINE Routine											*/
/*===========================================================================*/	

// TODO: clean this up
zrtp_status_t _zrtp_machine_enter_pendingsecure(zrtp_stream_t* stream, zrtp_rtp_info_t* commit);
zrtp_status_t _zrtp_machine_enter_initiatingsecure(zrtp_stream_t* stream);
zrtp_status_t _zrtp_machine_enter_secure(zrtp_stream_t* stream);
zrtp_status_t _zrtp_machine_enter_pendingclear(zrtp_stream_t* stream);
zrtp_status_t _zrtp_machine_enter_initiatingerror( zrtp_stream_t *stream,
												   zrtp_protocol_error_t code,
												   uint8_t notif);

zrtp_status_t _zrtp_machine_create_confirm(zrtp_stream_t *stream, zrtp_packet_Confirm_t* confirm);
zrtp_status_t _zrtp_machine_process_confirm(zrtp_stream_t *stream, zrtp_packet_Confirm_t *confirm);
zrtp_status_t _zrtp_machine_process_goclear(zrtp_stream_t* stream, zrtp_rtp_info_t* packet);	
	
zrtp_status_t _zrtp_machine_start_initiating_secure(zrtp_stream_t *stream);
zrtp_statemachine_type_t _zrtp_machine_preparse_commit(zrtp_stream_t *stream, zrtp_rtp_info_t* packet);

	
/*===========================================================================*/
/*	PARSERS																     */
/*===========================================================================*/	
	
/*!
 * \brief Prepare RTP/ZRTP media packet for the further processing.
 * This function defines the packet type, parses SSRC and makes the sequence
 * number implicit.  If it is a ZRTP message, packet length correctness and CRC
 * are checked as well.
 * \param stream - ZRTP stream associated with this packet;
 * \param packet - packet for preparing;
 * \param length - packet length;
 * \param info - resulting packet structure;
 * \param is_input - 1 - assumes incoming and 0 - outgoing packet direction.
 */
zrtp_status_t _zrtp_packet_preparse( zrtp_stream_t* stream,
									 char* packet,
									 uint32_t *length,
									 zrtp_rtp_info_t* info,
									 uint8_t is_input);

/*!
 * \brief Fills ZRTP message header and computes messages HMAC
 * _zrtp_packet_fill_msg_hdr() prepares a ZRTP message header for sending. It calculates
 * the total message length in 4-byte words and fills the message type block. 
 * \param stream - stream within in the operation will be performed
 * \param type - ZRTP message type;
 * \param body_length - message body length (without header); 
 * \param hdr - message ZRTP header
 * \return
 *	- zrtp_status_ok - if success;
 *	- zrtp_status_bad_param - if message \c type is unknown.
 */
zrtp_status_t _zrtp_packet_fill_msg_hdr( zrtp_stream_t *stream,								  
										 zrtp_msg_type_t type,
										 uint16_t body_length,
										 zrtp_msg_hdr_t *hdr);

/**
 * @brief Sends ZRTP message onto the network
 * _zrtp_packet_send_message constructs a ZRTP header and prepares packet for sending,
 * computes CRC and injects the packet into the network using the interface
 * function zrtp_send_rtp().
 * @param ctx - ZRTP stream context;
 * @param type - packet type to construct primitive ZRTP messages;
 * @param message - ZRTP message for sending.
 * @return
 *	- 0 - if sent successfully;
 *	- -1 - if error.
 */
int _zrtp_packet_send_message( zrtp_stream_t *stream,
							   zrtp_msg_type_t type,
							   const void *message);

/** @brief Returns ZRTP message type by symbolic name in header. */
zrtp_msg_type_t _zrtp_packet_get_type(ZRTP_UNALIGNED(zrtp_rtp_hdr_t)*hdr, uint32_t length);

/**
 * @brief Insert CRC32 to ZRTP packets
 * This function computes the 32 bit ZRTP packet checksum according to RFC 3309.
 * As specified at ZRTP RFC, CRC32 is appended to the end of the extension for every ZRTP packet.
 * @param packet - zrtp packet wrapper structure.
 */
void _zrtp_packet_insert_crc(char* packet, uint32_t length);

/**
 * @brief Validate ZRTP packet CRC
 * @return
 *	- 0 if correct CRC;
 *	- -1 if CRC validation failed.
 */
int8_t _zrtp_packet_validate_crc(const char* packet, uint32_t length);
		
/*  \} */

#if defined(__cplusplus)
}
#endif

#endif /* __ZRTP_ENGINE_H__ */

/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Vitaly Rozhkov <v.rozhkov at soft-industry.com>
 */

#ifndef __ZRTP_SRTP_H__
#define __ZRTP_SRTP_H__

#include "zrtp_config.h"
#include "zrtp_error.h"
#include "zrtp_types.h"
#include "zrtp_crypto.h"


/* in host order, so outside the #if */
#define ZRTP_RTCP_E_BIT      0x80000000
/* for byte-access */
#define ZRTP_RTCP_E_BYTE_BIT 0x80
#define ZRTP_RTCP_INDEX_MASK 0x7fffffff


/*!
 * \defgroup srtp SRTP encryption interface
 * \ingroup zrtp_dev
 * \{
 */

/* Special types and definitions for the embedded implementation */
#if (!defined(ZRTP_USE_EXTERN_SRTP) || (ZRTP_USE_EXTERN_SRTP == 0))
#include "zrtp_srtp_builtin.h"

/*!
 * \brief Structure describing an SRTP session.
 * An instance of this structure is created by calling zrtp_srtp_create() 
 * and destroyed by calling zrtp_srtp_destroy(). It is used for
 * protecting and unprotecting included streams.
 */
struct zrtp_srtp_ctx_t
{
   zrtp_srtp_stream_ctx_t *outgoing_srtp; /*!< pointer to outgoing SRTP stream context */
   zrtp_srtp_stream_ctx_t *incoming_srtp; /*!< pointer to incoming SRTP stream context */
};

/*!
 * \brief Global context of an internal SRTP implementation.
 * It is created by calling zrtp_srtp_init() and destroyed by calling zrtp_srtp_down().
 * This context is used for holding replay protection mechanism data.
 */
typedef struct
{   
   zrtp_rp_ctx_t *rp_ctx; /*!< pointer to replay protection context. */
} zrtp_srtp_global_t;

#else
typedef void zrtp_srtp_global_t;
#endif /* BUILDIN SRTP */

/*! Defines types of SRTP hmac functions */
typedef enum zrtp_srtp_hash_id_t
{
	/*!
	 * @warning SHA1 hash algorithm is for internal use only! It used for srtp authentication and does
	 * not used in ZRTP protocol itself. Don't use it in \ref zrtp_profile_t#hash_schemes configuration.
	 */
	ZRTP_SRTP_HASH_HMAC_SHA1	= 10
} zrtp_srtp_hash_id_t;


/*!
 * \brief Structure describing SRTP/SRTCP stream parameters.
 */
typedef struct
{   
   /*!< Cipher used to encrypt packets */
   zrtp_cipher_t        *cipher;
   /*!
    * \brief Cipher key length in bytes (not including salt length).
    * Used for cipher key derivation on stream initialization
    * by calling \ref zrtp_srtp_create().
    */
   uint32_t             cipher_key_len;
   
   /*!< Hash used for packets authentication */
   zrtp_hash_t          *hash;
   
   /*!
    * \brief Key length in bytes for HMAC generation.
    * Used for auth key derivation on stream initialization by calling \ref
    * zrtp_srtp_create() and for filling the key buffer with zeros on
    * stream deinitialization by calling \ref zrtp_srtp_destroy().
   */
   uint32_t            auth_key_len;
   
   /*!< Structure describing SRTP authentication scheme */
   zrtp_auth_tag_length_t   *auth_tag_len;
} zrtp_srtp_policy_t;


/*!
 * \brief Structure describing SRTP stream parameters.
 * Variables of this type should be mapped into the SRTP stream context when
 * a new stream is created. 
 */
typedef struct
{
   zrtp_srtp_policy_t   rtp_policy;    /*!< crypto policy for RTP stream */
   zrtp_srtp_policy_t   rtcp_policy;   /*!< crypto policy for RTCP stream */
      
   zrtp_cipher_t       *dk_cipher;     /*!< cipher for the key derivation mechanism */
   
   /*!< Master key for key derivation. (holds the key value only, without the salt) */   
   zrtp_string64_t      key;
   /*!< Master salt for key derivation. (salt should be 14 bytes length) */
   zrtp_string64_t      salt;
   
   uint16_t				ssrc;
} zrtp_srtp_profile_t;


/*!
 * \brief Initialize SRTP engine and allocate global SRTP context.
 * Contains global data for all sessions and streams. For correct memory
 * management, the global SRTP context should be released by calling \ref
 * zrtp_srtp_destroy().  A pointer to the allocated SRTP global should be saved
 * at zrtp->srtp_global.
 * \warning this function \b must be called before any operation with the SRTP
 * engine.
 * \param zrtp - pointer to libzrtp global context
 * \return
 *	- zrtp_status_ok if success
 *  - zrtp_status_fail if error.
 */
zrtp_status_t zrtp_srtp_init(zrtp_global_t *zrtp);

/*!
 * \brief Free all allocated resources that were allocated by initialization
 * This function \b must be called  at the end of SRTP engine use.
 * A pointer to deallocated SRTP global context (zrtp->srtp_global)
 * should be cleared ( set to NULL).
 * \param zrtp - pointer to libzrtp global context;
 * \return
 *   - zrtp_status_ok - if SRTP engine has been deinitialized successfully;
 *   - one of \ref zrtp_status_t errors - if deinitialization failed.
 */
zrtp_status_t zrtp_srtp_down( zrtp_global_t *zrtp);

/*!
 * \brief Creates SRTP context based on given incoming and outgoing profiles.
 * \param srtp_global - pointer to SRTP engine global context;
 * \param inc_profile - profile for incoming stream configuration;
 * \param out_profile - profile for outgoing stream configuration.
 * \return
 *   - pointer to allocated and initialized SRTP session;
 *    - NULL if error.
 */
zrtp_srtp_ctx_t * zrtp_srtp_create( zrtp_srtp_global_t *srtp_global,
									zrtp_srtp_profile_t *inc_profile,
									zrtp_srtp_profile_t *out_profile );

/*!
 * \brief Destroys SRTP context that was allocated by \ref zrtp_srtp_create()
 * \param srtp_global - pointer to SRTP engine global context;
 * \param srtp_ctx - pointer to SRTP context.
 * \return
 *   - zrtp_status_ok - if SRTP context has been destroyed successfully;
 *   - one of \ref zrtp_status_t errors if error.
 */
zrtp_status_t zrtp_srtp_destroy( zrtp_srtp_global_t *srtp_global,
								 zrtp_srtp_ctx_t * srtp_ctx );


/*!
 * \brief Function applies SRTP protection to the RTP packet.
 * If zrtp_status_ok is returned, then packet points to the resulting SRTP
 * packet; otherwise, no assumptions should be made about the value of either
 * data elements.
 * \note This function assumes that it can write the authentication tag 
 * directly into the packet buffer, right after the the RTP payload. 32-bit
 * boundary alignment of the packet is assumed as well.
 * \param srtp_global - global SRTP context;
 * \param srtp_ctx - SRTP context to use in processing the packet;
 * \param packet - pointer to the packet to be protected.
 * \return
 *   - zrtp_status_ok - if packet has been protected successfully;
 *   - one of \ref zrtp_status_t errors - if protection failed.
 */
zrtp_status_t zrtp_srtp_protect( zrtp_srtp_global_t *srtp_global,
								 zrtp_srtp_ctx_t   *srtp_ctx,
								 zrtp_rtp_info_t *packet );

/*!
 * \brief Decrypts SRTP packet.
 * If zrtp_status_ok is returned, then packet points to the resulting plain RTP
 * packet; otherwise, no assumptions should be made about the value of either
 * data elements.
 * \warning This function assumes that the SRTP packet is aligned on
 * a 32-bit boundary.
 * \param srtp_global - global SRTP context;
 * \param srtp_ctx - SRTP context to use in processing the packet;
 * \param packet - pointer to the packet to be unprotected.
 * \return
 *   - zrtp_status_ok - if packet has been unprotected successfully
 *   - one of \ref zrtp_status_t errors - if decryption failed
 */
zrtp_status_t zrtp_srtp_unprotect( zrtp_srtp_global_t *srtp_global,
								   zrtp_srtp_ctx_t   *srtp_ctx,
								   zrtp_rtp_info_t *packet );

/*!
 * \brief Function applies SRTCP protection to the RTCP packet.
 * If zrtp_status_ok is returned, then packet points to the result in SRTCP
 * packet; otherwise, no assumptions should be made about the value of either
 * data elements.
 * \note This function assumes that it can write the authentication tag 
 * directly into the packet buffer, right after the the RTP payload. 32-bit
 * boundary alignment of the packet is also assumed.
 * \param srtp_global - global SRTP context;
 * \param srtp_ctx - SRTP context to use in processing the packet;
 * \param packet - pointer to the packet to be protected.
 * \return
 *   - zrtp_status_ok - if packet has been protected successfully;
 *   - one of \ref zrtp_status_t errors - if protection failed.
 */                           
zrtp_status_t zrtp_srtp_protect_rtcp( zrtp_srtp_global_t *srtp_global,
									  zrtp_srtp_ctx_t *srtp_ctx,
									  zrtp_rtp_info_t *packet );

/*!
 * \brief Decrypts SRTCP packet.
 * If zrtp_status_ok is returned, then packet points to the resulting RTCP
 * packet; otherwise, no assumptions should be made about the value of either
 * data elements.
 * \warning This function assumes that the SRTP packet is aligned on
 * a 32-bit boundary.
 * \param srtp_global - global SRTP context;
 * \param srtp_ctx - SRTP context to use in processing the packet;
 * \param packet - pointer to the packet to be unprotected.
 * \return
 *   - zrtp_status_ok - if packet has been unprotected successfully;
 *   - one of \ref zrtp_status_t errors - if decryption failed.
*/                                                            
zrtp_status_t zrtp_srtp_unprotect_rtcp( zrtp_srtp_global_t *srtp_global,
										zrtp_srtp_ctx_t   *srtp_ctx,
										zrtp_rtp_info_t *packet );

/* \} */

#endif /*__ZRTP_SRTP_H__ */

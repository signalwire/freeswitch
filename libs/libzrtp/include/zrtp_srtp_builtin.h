/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 * Vitaly Rozhkov <v.rozhkov at soft-industry.com>
 */

#ifndef __ZRTP_SRTP_BUILTIN_H__        
#define __ZRTP_SRTP_BUILTIN_H__

#include "zrtp_config.h"
#include "zrtp_error.h"
#include "zrtp_types.h"
#include "zrtp_crypto.h"

/*!
 * \defgroup dev_srtp Built in SRTP realization
 * \ingroup zrtp_dev
 * \{
 */

/*! 
 * \brief Sliding window width in bits.
 * This window is used by the replay protection mechanism. As stated in the
 * RFC3711, '3.3.2., the replay protection sliding window width MUST be at least
 * 64, but MAY be set to a higher value.
 */
#if (ZRTP_PLATFORM == ZP_SYMBIAN)
#	define ZRTP_SRTP_WINDOW_WIDTH 16
#else
#	define ZRTP_SRTP_WINDOW_WIDTH 128
#endif

#if ZRTP_SRTP_WINDOW_WIDTH % 8
/*!
 * \brief Sliding window width in bytes if padding is needed.
 * This is used for allocating a window as a uint8_t array.
 */
#define ZRTP_SRTP_WINDOW_WIDTH_BYTES ZRTP_SRTP_WINDOW_WIDTH/8+1
#else
/*!
 * \brief Sliding window width in bytes if padding isn't needed.
 * This is used for allocating a window as a uint8_t array.
 */
#define ZRTP_SRTP_WINDOW_WIDTH_BYTES ZRTP_SRTP_WINDOW_WIDTH/8
#endif

#define RP_INCOMING_DIRECTION 1
#define RP_OUTGOING_DIRECTION 2


/*! \brief Structure describing replay protection engine data */
typedef struct
{    
    uint32_t    seq; /*!< sequence number of packet on the top of sliding window */    
    uint8_t     window[ZRTP_SRTP_WINDOW_WIDTH_BYTES]; /*!< sliding window buffer */
} zrtp_srtp_rp_t;


/*! \brief Structure describing cipher wrapper */
typedef struct
{
    /*!< cipher that will be used for packet encryption */
    zrtp_cipher_t     *cipher;
    
	/*!< pointer to cipher's context */
    void            *ctx;
} zrtp_srtp_cipher_t;


/*! \brief Structure describing authentication wrapper */
typedef struct
{    
    zrtp_hash_t    *hash;   /*!< hash component for authentication tag generation */    
    uint8_t        *key;    /*!< key buffer for HMAC generation */    
    uint32_t    key_len;    /*!< key length in bytes. Used for zeroes filling of buffer with key */    
    zrtp_auth_tag_length_t     *tag_len;    /*!< SRTP authentication scheme component */
} zrtp_srtp_auth_t;


/*! \brief Structure for SRTP stream context description. */
typedef struct
{
    /*!< wrapper for cipher component and holding its auxiliary data. Used for RTP encryption */
    zrtp_srtp_cipher_t      rtp_cipher;
    /*!< wrapper for hash component and holding its auxiliary data. Used for RTP authentication */     
    zrtp_srtp_auth_t        rtp_auth;
    
    /*!< wrapper for cipher component and holding its auxiliary data. Used for RTCP encryption */
    zrtp_srtp_cipher_t      rtcp_cipher;
    /*!< wrapper for hash component and holding its auxiliary data. Used for RTCP authentication */     
    zrtp_srtp_auth_t        rtcp_auth;
} zrtp_srtp_stream_ctx_t;


/*!
 * \brief Enumeration of labels used in key derivation for various purposes.
 * See RFC3711, "4.3.  Key Derivation" for more details
 */
typedef enum
{    
    label_rtp_encryption  = 0x00,    /*!< for RTP cipher's key derivation */
    label_rtp_msg_auth    = 0x01,    /*!< for RTP packets authentication mechanism's key derivation */    
    label_rtp_salt        = 0x02,    /*!< for RTP cipher's salt derivation */
        
    label_rtcp_encryption = 0x03,    /*!< used for RTCP cipher's key derivation */    
    label_rtcp_msg_auth   = 0x04,    /*!< for RTCP packets authentication mechanism key derivation */    
    label_rtcp_salt       = 0x05    /*!< for RTCP cipher's salt derivation */
} zrtp_srtp_prf_label;

typedef zrtp_srtp_cipher_t zrtp_dk_ctx;


/*!
 * \brief Structure describing a protection node.
 * Each node keeps data for protecting RTP and RTCP packets against replays
 * within streams with a given SSRC. There are two replay protection nodes for
 * each SSRC value in the two lists. One is used for incoming packets and
 * the other for outgoing packets. 
*/
typedef struct
{    
    zrtp_srtp_rp_t rtp_rp;    /*!< RTP replay protection data */
    zrtp_srtp_rp_t rtcp_rp;    /*!< RTCP replay protection data */    
    uint32_t ssrc;            /*!< RTP media SSRC for nodes searching in the linked list */    
	zrtp_srtp_ctx_t *srtp_ctx; /*!< SRTP context related with current node*/
    mlist_t mlist;
} zrtp_rp_node_t;


/*!
* \brief Structure describing replay protection context.
* This structure holds two linked list's heads and two mutexes for
* synchronization access to appropriate lists.
*/
typedef struct
{    
    zrtp_rp_node_t  inc_head;    /*!< head of replay protection nodes list for incoming packets */
    zrtp_mutex_t*   inc_sync;    /*!< mutex for incoming list access synchronization */
    zrtp_rp_node_t  out_head;    /*!< head of replay protection nodes list for outgoing packets */    
    zrtp_mutex_t*   out_sync;    /*!< mutex for outgoing list access synchronization */
} zrtp_rp_ctx_t;

/* \} */

#endif /* __ZRTP_SRTP_BUILTIN_H__ */

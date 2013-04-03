/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */

#ifndef __ZRTP_PROTOCOL_H__
#define __ZRTP_PROTOCOL_H__

#include "zrtp_config.h"
#include "zrtp_types.h"
#include "zrtp_error.h"

#if defined(_MSC_VER)
#pragma warning(disable:4214)
#endif

/*!
 * \defgroup dev_protocol Protocol related data types and definitions
 * \ingroup zrtp_dev
 * \{
 */

/*! ZRTP Protocol version, retransmitted in HELLO packets */
#define	ZRTP_PROTOCOL_VERSION				"1.10"
#define	ZRTP_PROTOCOL_VERSION_VALUE			110

#define	ZRTP_ZFONE_PROTOCOL_VERSION			"0.10"
#define	ZRTP_ZFONE_PROTOCOL_VERSION_VALUE	10

/*
 * Protocol constants and definitions. All these values are defined by the ZRTP
 * specification <A HREF="http://zfoneproject.com/zrtp_ietf.html">"ZRTP Internet Draft"</A>.
 * Don't change them!
 */
#define ZRTP_S384					"S384"
#define ZRTP_S256					"S256"
#define ZRTP_S160					"S160"
#define ZRTP_AES1					"AES1"
#define ZRTP_AES3					"AES3"
#define ZRTP_HS32					"HS32"
#define ZRTP_HS80					"HS80"
#define ZRTP_DH2K   				"DH2k"
#define ZRTP_DH3K   				"DH3k"
#define ZRTP_EC256P					"EC25"
#define ZRTP_EC384P					"EC38"
#define ZRTP_EC521P					"EC52"
#define ZRTP_MULT   				"Mult"
#define ZRTP_PRESHARED				"Prsh"
#define ZRTP_B32					"B32 "
#define ZRTP_B256					"B256"

#define ZRTP_ROLE_INITIATOR			"Initiator"
#define ZRTP_ROLE_RESPONDER			"Responder"
#define ZRTP_INITIATOR_HMAKKEY_STR	"Initiator HMAC key"
#define ZRTP_RESPONDER_HMAKKEY_STR	"Responder HMAC key"
#define ZRTP_GOCLEAR_STR			"GoClear"
#define ZRTP_INITIATOR_KEY_STR		"Initiator SRTP master key"
#define ZRTP_INITIATOR_SALT_STR		"Initiator SRTP master salt"
#define ZRTP_RESPONDER_KEY_STR		"Responder SRTP master key"
#define ZRTP_RESPONDER_SALT_STR		"Responder SRTP master salt"
#define ZRTP_SKEY_STR				"ZRTP Session Key"
#define ZRTP_SAS_STR				"SAS"
#define ZRTP_RS_STR					"retained secret"
#define ZRTP_INITIATOR_ZRTPKEY_STR	"Initiator ZRTP key"
#define ZRTP_RESPONDER_ZRTPKEY_STR	"Responder ZRTP key"
#define ZRTP_CLEAR_HMAC_STR			"GoClear"
#define ZRTP_KDF_STR				"ZRTP-HMAC-KDF"
#define ZRTP_SESS_STR				"ZRTP Session Key"
#define ZRTP_MULTI_STR				"ZRTP MSK"
#define ZRTP_PRESH_STR				"ZRTP PSK"
#define	ZRTP_TRUSTMITMKEY_STR		"Trusted MiTM key"
#define ZRTP_COMMIT_HV_KEY_STR		"Prsh"

#define ZRTP_CACHE_DEFAULT_TTL		(30*24*60*60)

/** ZRTP Message magic Cookie */
#define ZRTP_PACKETS_MAGIC			0x5a525450L
/** Defines ZRTP extension type for RTP protocol */
#define ZRTP_MESSAGE_MAGIC			0x505a


/**
 * @brief Retransmission timer T1 in milliseconds
 * T1 is used for the retransmission of Hello messages. The HELLO timeout is
 * doubled each time a resend occurs. The gain (max timeout value) is limited
 * by @ref ZRTP_T1_CAPPING. After reaching \c ZRTP_T1_CAPPING, the state machine
 * keeps resending HELLO packets until the resend count is less than \ref
 * ZRTP_T1_MAX_COUNT
 * @sa ZRTP_T1_MAX_COUNT ZRTP_T1_CAPPING
 */

#define ZRTP_T1						50

/*!
 * \brief Max resends count value for T1 timer 
 * This is the threshold value for HELLO replays. See \ref ZRTP_T1 ZRTP_T1 for
 * details. If the resend count exceeds the value of ZRTP_T1_MAX_COUNT then
 * the state machine calls _zrtp_machine_enter_initiatingerror() with error code \ref
 * zrtp_protocol_error_t#zrtp_error_timeout and ZRTP session establishment is
 * failed.
 */
#define	ZRTP_T1_MAX_COUNT			20

/*!
 * \brief Max resends count value for T1 timer for cases when local side have
 * received remote Hello. Libzrtp uses this extended number of retries when there
 * is an evidence, that remote side supports ZRTP protocol (remote Hello received).
 * This approach allows to eliminate problem when ZRTP state-machine switches to
 * NO_ZRTP state while remote side is computing his initial DH value. (especially
 * important for slow devices)
 */
#define	ZRTP_T1_MAX_COUNT_EXT		60

/*! Hello retries counter for ZRTP_EVENT_NO_ZRTP_QUICK event */
#define ZRTP_NO_ZRTP_FAST_COUNT		5

/*!
 * \brief Max T1 timeout
 * ZRTP_T1_MAX_COUNT is the threshold for the growth of the timeout value of
 * HELLO resends. See \ref ZRTP_T1 for details. 
 */
#define	ZRTP_T1_CAPPING				200

/*!
 * \brief ZRTP stream initiation period in milliseconds 
 * If for some reason the initiation of a secure ZRTP stream can't be performed
 * at a given time (there are no retained secrets for the session, or the
 * concurrent stream is being processed in "DH" mode) the next attempt will be
 * done in ZRTP_PROCESS_T1 milliseconds. If at the end of ZRTP_PROCESS_T1_MAX_COUNT
 * attempts the necessary conditions haven't been reached, the task is canceled.
 * The mechanism of delayed execution is the same as the mechanism of delayed
 * packet sending. \sa ZRTP_PROCESS_T1_MAX_COUNT
 */
#define ZRTP_PROCESS_T1				50

/*!
 * \brief Max recall count value 
 * This is the threshold value for ZRTP stream initiation tries. See \ref
 * ZRTP_PROCESS_T1 for details.
*/
#define ZRTP_PROCESS_T1_MAX_COUNT	20000

/*!
 * \brief Retransmission timer T2 in milliseconds
 * T2 is used for the retransmission of all ZRTP messages except HELLO. The
 * timeout value is doubled after every retransmission. The gain (max timeout's
 * value) is limited by \ref ZRTP_T2_CAPPING. \ref ZRTP_T2_MAX_COUNT is the limit
 * for packets resent as for \ref ZRTP_T1.
 */
#define	ZRTP_T2						150

/*!
 * \brief Max retransmissions for non-HELLO packets
 * ZRTP_T2_MAX_COUNT limits number of resends for the non-HELLO/GOCLEAR packets.
 * When exceeded, call_is_on_error() is called and the error code is set to
 * \ref zrtp_protocol_error_t#zrtp_error_timeout
 */
#define	ZRTP_T2_MAX_COUNT			10


/*!
 * \brief Max timeout value for protocol packets (except HELLO and GOCLEAR)
 * The resend timeout value grows until it reaches ZRTP_T2_CAPPING. After that
 * the state machine keeps resending until the resend count hits the limit of
 * \ref ZRTP_T2_MAX_COUNT
 */
#define	ZRTP_T2_CAPPING				1200

/*!
 * \brief Retransmission timer for GoClear resending in milliseconds.
 * To prevent pinholes from closing or NAT bindings from expiring, the GoClear
 * message should be resent every N seconds while waiting for confirmation from
 * the user. GoClear replays are endless.
 */
#define	ZRTP_T3						300

/*!
 * \brief Set of timeouts for Error packet replays. 
 * The meaning of these fields are the same as in the T1 group but for
 * Error/ErrorAck packets.  The values of these options are not strongly
 * defined by the draft. We use empirical values.
 */
#define	ZRTP_ET						150
#define ZRTP_ETI_MAX_COUNT			10
#define ZRTP_ETR_MAX_COUNT			3

/* ZRTP Retries schedule for slow CSD channel */
#define ZRTP_CSD_T4PROC				2000

#define ZRTP_CSD_T1					400 + ZRTP_CSD_T4PROC
#define ZRTP_CSD_T2					900 + ZRTP_CSD_T4PROC
#define ZRTP_CSD_T3					900 + ZRTP_CSD_T4PROC
#define ZRTP_CSD_T4					200 + ZRTP_CSD_T4PROC
#define ZRTP_CSD_ET					200 + ZRTP_CSD_T4PROC


/*! Defines the max component number which can be used in a HELLO agreement */
#define ZRTP_MAX_COMP_COUNT			7


/*
 * Some definitions of protocol structure sizes. To simplify sizeof() constructions
 */
#define ZRTP_VERSION_SIZE			4
#define ZRTP_ZID_SIZE				12
#define ZRTP_CLIENTID_SIZE			16
#define ZRTP_COMP_TYPE_SIZE			4
#define ZRTP_RS_SIZE				32
#define ZRTP_RSID_SIZE				8
#define ZRTP_PACKET_TYPE_SIZE		8
#define RTP_V2_HDR_SIZE				12
#define RTP_HDR_SIZE				RTP_V2_HDR_SIZE
#define RTCP_HDR_SIZE				8
#define ZRTP_HV_SIZE				32
#define ZRTP_HV_NONCE_SIZE			16
#define ZRTP_HV_KEY_SIZE			8
#define ZRTP_HMAC_SIZE				8
#define ZRTP_CFBIV_SIZE				16
#define ZRTP_MITM_SAS_SIZE			4
#define ZRTP_MESSAGE_HASH_SIZE		32
#define ZRTP_HASH_SIZE				32

/* Without header and HMAC: <verison> + <client ID> + <hash> + <ZID> + <components length> */
#define ZRTP_HELLO_STATIC_SIZE		(ZRTP_VERSION_SIZE + ZRTP_CLIENTID_SIZE + 32 + ZRTP_ZID_SIZE + 4)

/* Without header and HMAC: <hash> + <secrets IDs> */
#define ZRTP_DH_STATIC_SIZE			(32 + 4*8)

/* Without header and HMAC: <hash> + <ZID> + <components definitions> */
#define ZRTP_COMMIT_STATIC_SIZE		(32 + ZRTP_ZID_SIZE + 4*5)

/* <RTP> + <ext. header> + <ZRTP message type> + CRC32 */
#define ZRTP_MIN_PACKET_LENGTH		(RTP_HDR_SIZE + 4 + 8 + 4) 


#if ( ZRTP_PLATFORM != ZP_SYMBIAN )
	#pragma pack(push,1)
#endif



/** Base ZRTP messages header */
typedef struct zrtp_msg_hdr
{
	/** ZRTP magic cookie */
	uint16_t		magic;
	
	/** ZRTP message length in 4-byte words */
	uint16_t		length;
	
	/** ZRTP message type */
	zrtp_uchar8_t	type;
} zrtp_msg_hdr_t;

/*!
 * \brief ZRTP HELLO packet data
 * Contains fields needed to construct/store a ZRTP HELLO packet
 */
typedef struct zrtp_packet_Hello
{
	zrtp_msg_hdr_t	hdr;
	/** ZRTP protocol version */
	zrtp_uchar4_t	version;
	
	/** ZRTP client ID */
	zrtp_uchar16_t	cliend_id;
	
	/*!< Hash to prevent DOS attacks */
	zrtp_uchar32_t	hash;
	
	/** Endpoint unique ID */
	zrtp_uchar12_t	zid;
#if ZRTP_BYTE_ORDER == ZBO_LITTLE_ENDIAN
	uint8_t			padding2:4;
	
	/** Passive flag */
	uint8_t			pasive:1;
	
	/** M flag */
	uint8_t			mitmflag:1;
	
	/** Signature support flag */
	uint8_t			sigflag:1;
		
	uint8_t			uflag:1;
	
	/** Hash scheme count */	
	uint8_t			hc:4;	
	uint8_t			padding3:4;
	
	/** Cipher count */
	uint8_t			ac:4;
	
	/** Hash scheme count */	
	uint8_t			cc:4;
	
	/** SAS scheme count */
	uint8_t			sc:4;
	
	/** PK Type count */
	uint8_t			kc:4;
#elif ZRTP_BYTE_ORDER == ZBO_BIG_ENDIAN
	uint8_t			uflag:1;
	uint8_t			sigflag:1;
	uint8_t			mitmflag:1;
	uint8_t			pasive:1;
	uint8_t			padding2:4;	
	uint8_t			padding3:4;
	uint8_t			hc:4;
	uint8_t			cc:4;
	uint8_t			ac:4;
	uint8_t			kc:4;
	uint8_t			sc:4;
#endif

    zrtp_uchar4_t	comp[ZRTP_MAX_COMP_COUNT*5];
	zrtp_uchar8_t	hmac;
} zrtp_packet_Hello_t;


/**
 * @brief ZRTP COMMIT packet data 
 * Contains information to build/store a ZRTP commit packet.
 */
typedef struct zrtp_packet_Commit
{
	zrtp_msg_hdr_t	hdr;
	
	/** Hash to prevent DOS attacks */
	zrtp_uchar32_t	hash;
	
	/** ZRTP endpoint unique ID */
    zrtp_uchar12_t	zid;
	
	/** hash calculations schemes selected by ZRTP endpoint */
    zrtp_uchar4_t	hash_type;
	
	/** cipher types selected by ZRTP endpoint */
    zrtp_uchar4_t	cipher_type;
	
	/** SRTP auth tag lengths selected by ZRTP endpoint */
    zrtp_uchar4_t	auth_tag_length;
	
	/** session key exchange schemes selected by endpoints */
    zrtp_uchar4_t	public_key_type;
	
	/** SAS calculation schemes selected by endpoint*/
	zrtp_uchar4_t	sas_type;
	/** hvi. See <A HREF="http://zfoneproject.com/zrtp_ietf.html">"ZRTP Internet Draft"</A> */
    zrtp_uchar32_t	hv;
	zrtp_uchar8_t	hmac;
} zrtp_packet_Commit_t;


/**
 * @brief ZRTP DH1/2 packets data
 * Contains fields needed to constructing/storing ZRTP DH1/2 packet.
 */
typedef struct zrtp_packet_DHPart
{
	zrtp_msg_hdr_t		hdr;
	
	/** Hash to prevent DOS attacks */
	zrtp_uchar32_t		hash;
	
	/** hash of retained shared secret 1 */
    zrtp_uchar8_t		rs1ID;
	
	/** hash of retained shared secret 2 */    
    zrtp_uchar8_t		rs2ID;
	
	/** hash of user-defined secret */
    zrtp_uchar8_t		auxsID;
	
	/** hash of PBX secret */	
    zrtp_uchar8_t		pbxsID;
	
	/** pvi/pvr or nonce field depends on stream mode */
	zrtp_uchar1024_t	pv;
	zrtp_uchar8_t		hmac;
} zrtp_packet_DHPart_t;


/**
 * @brief ZRTP Confirm1/Confirm2 packets data 
 */
typedef struct zrtp_packet_Confirm
{
	zrtp_msg_hdr_t		hdr;
	
	/** HMAC of preceding parameters */
	zrtp_uchar8_t		hmac;
	
	/** The CFB Initialization Vector is a 128 bit random nonce */	
	zrtp_uchar16_t		iv;
	
	/** Hash to prevent DOS attacks */
	zrtp_uchar32_t		hash;
	
	/** Unused (Set to zero and ignored) */
	uint8_t				pad[2];
	
	/** Length of optional signature field  */
	uint8_t				sig_length;
	
	/** boolean flags for allowclear, SAS verified and disclose */	
    uint8_t				flags;
	
	/** how long (seconds) to cache shared secret */
    uint32_t			expired_interval;
} zrtp_packet_Confirm_t;


/**
 * @brief ZRTP Confirm1/Confirm2 packets data 
 */
typedef struct zrtp_packet_SASRelay
{
	zrtp_msg_hdr_t		hdr;
	
	/** HMAC of preceding parameters */
	zrtp_uchar8_t		hmac;
	
	/** The CFB Initialization Vector is a 128 bit random nonce */
	zrtp_uchar16_t		iv;
	
	/** Unused (Set to zero and ignored) */
	uint8_t				pad[2];
	
	/** Length of optionas signature field  */
	uint8_t				sig_length;
	
	/** boolean flags for allowclear, SAS verified and disclose */
    uint8_t				flags;
	
	/** Rendering scheme of relayed sasvalue (for trusted MitMs) */
	zrtp_uchar4_t		sas_scheme;
	
	/** Trusted MITM relayed sashash */
	uint8_t				sashash[32];
} zrtp_packet_SASRelay_t;


/**
 * @brief GoClear packet structure according to ZRTP specification
 */
typedef struct zrtp_packet_GoClear
{
	zrtp_msg_hdr_t		hdr;
	
	/** Clear HMAC to protect SRTP session from accidental termination */
    zrtp_uchar8_t		clear_hmac;
} zrtp_packet_GoClear_t;


/**
 * @brief Error packet structure in accordance with ZRTP specification
 */
typedef struct  zrtp_packet_Error
{
	zrtp_msg_hdr_t		hdr;	
	
	/** ZRTP error code defined by draft and \ref zrtp_protocol_error_t */
	uint32_t			code;
} zrtp_packet_Error_t;

/** ZFone Ping Message. Similar to ZRTP protocol packet format */
typedef struct
{
	zrtp_msg_hdr_t	hdr;	
	zrtp_uchar4_t	version;			/** Zfone discovery protocol version */	
	zrtp_uchar8_t	endpointhash;		/** Zfone endpoint unique identifier */
} zrtp_packet_zfoneping_t;

/** ZFone Ping MessageAck. Similar to ZRTP protocol packet format */
typedef struct
{
	zrtp_msg_hdr_t	hdr;
	zrtp_uchar4_t	version;			/** Zfone discovery protocol version */
	zrtp_uchar8_t	endpointhash;		/** Zfone endpoint unique identifier */
	zrtp_uchar8_t	peerendpointhash;	/** EndpointHash copied from Ping message */
	uint32_t		peerssrc;
} zrtp_packet_zfonepingack_t;

/*! \} */

#if ( ZRTP_PLATFORM != ZP_SYMBIAN )
	#pragma pack(pop)
#endif

#endif /*__ZRTP_PROTOCOL_H__*/

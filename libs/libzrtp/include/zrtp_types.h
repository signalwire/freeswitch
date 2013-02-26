/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */


#ifndef __ZRTP_TYPES_H__
#define __ZRTP_TYPES_H__

#include "zrtp_config.h"
#include "bn.h"
#include "zrtp_base.h"
#include "zrtp_iface.h"
#include "zrtp_list.h"
#include "zrtp_legal.h"
#include "zrtp_string.h"
#include "zrtp_protocol.h"


/**
 * \brief Defines ZRTP state-machine states
 * \ingroup zrtp_types
 *
 * The conditions for switching from one state to another, and libzrtp behavior in every state is 
 * described in detail in \ref XXX and depicted in diagram XXX and XXX.
 * 
 * The current stream state is stored in the zrtp_stream_info_t#state variable and available for 
 * reading at any time.
 */
typedef enum zrtp_state_t
{    
	ZRTP_STATE_NONE				= 0,
	ZRTP_STATE_ACTIVE,					/** Just right stream attaching, before protocol start */
	ZRTP_STATE_START,					/** Protocol initiated, Discovery haven't started yet */
	ZRTP_STATE_WAIT_HELLOACK,			/** Hello sending, waiting for HelloAck */
	ZRTP_STATE_WAIT_HELLO,				/** HelloAck received, Waiting for peer Hello */
	ZRTP_STATE_CLEAR,					/** CLEAR state */
	ZRTP_STATE_START_INITIATINGSECURE,	/** Starting Initiator state-machine */
	ZRTP_STATE_INITIATINGSECURE,		/** Commit retries, waiting for DH1 */
	ZRTP_STATE_WAIT_CONFIRM1,			/** DH2 retries, waiting for Confirm1 */
	ZRTP_STATE_WAIT_CONFIRMACK,			/** Confirm2 retries, waiting for ConfirmAck */
	ZRTP_STATE_PENDINGSECURE,			/** Responder state-machine, waiting for DH2 */
	ZRTP_STATE_WAIT_CONFIRM2,			/** Waiting for Confirm2 to finalize ZRTP exchange */
	ZRTP_STATE_SECURE,					/** SECURE state, call is encrypted */
	ZRTP_STATE_SASRELAYING,				/** SAS transferring to the remote peer (for MiTM only) */
	ZRTP_STATE_INITIATINGCLEAR,			/** Switching to CLEAR initated by the local endpoint */
	ZRTP_STATE_PENDINGCLEAR,			/** CLEAR request have been received */
	ZRTP_STATE_INITIATINGERROR,			/** Protocol ERROR detected on local side */
	ZRTP_STATE_PENDINGERROR,			/** Protocol ERROR received from the remote peer */
	ZRTP_STATE_ERROR,					/** Protocol ERROR state. Check zrtp_stream_info#last_error*/
#if (defined(ZRTP_BUILD_FOR_CSD) && (ZRTP_BUILD_FOR_CSD == 1))
	ZRTP_STATE_DRIVEN_INITIATOR,
	ZRTP_STATE_DRIVEN_RESPONDER,
	ZRTP_STATE_DRIVEN_PENDING,
#endif
	ZRTP_STATE_NO_ZRTP,					/** Discovery phase failed. Remote peer doesn't support ZRTP */
	ZRTP_STATE_COUNT
} zrtp_state_t;

/**
 * \brief Enumeration for ZRTP stream mode definition
 * \ingroup zrtp_types
 */
typedef enum zrtp_stream_mode_t
{
	ZRTP_STREAM_MODE_UNKN      = 0, /** Unused stream - unknown mode */
	ZRTP_STREAM_MODE_CLEAR     = 1, /** Just after stream attaching - mode is undefined */
	ZRTP_STREAM_MODE_DH        = 2, /** FULL DH ZRTP stream mode */
	ZRTP_STREAM_MODE_PRESHARED = 3, /** Preshared ZRTP stream mode */
	ZRTP_STREAM_MODE_MULT	   = 4, /** Multistream ZRTP stream mode */
	ZRTP_STREAM_MODE_COUNT 	   = 5
} zrtp_stream_mode_t;

/**
 * \brief ZRTP session profile
 * \ingroup zrtp_types 
 * \ingroup zrtp_main_init
 *
 * ZRTP Sessions are configured with a profile scheme. Each profile is defined by a structure of the 
 * given type.  zrtp_profile_t contains a set of preferences for crypto components and other 
 * protocol parameters.
 * 
 * The Crypto component choosing mechanism is as follows: both sides communicated their supported 
 * components during the "discovery phase". After that the initiator chooses the optimal 
 * intersection of components.
 *
 * For components identification the numerical values of the following types are used: 
 * zrtp_hash_id_t, zrtp_cipher_id_t, zrtp_atl_id_t, and zrtp_sas_id_t. The profile field responsible 
 * for components of a particular type setting is an integer-valued array where component
 * identifiers should be placed in order of priority. 0-element is of the first priority. The list 
 * should end with ZRTP_COMP_UNKN=0.
 *
 * The values in the profile may be filled either by libzrtp zrtp_profile_defaults() or by the user 
 * manually.
 *
 * The profile is applied to the stream context on allocation by zrtp_session_init().
 *
 * \sa XXX
 */
struct zrtp_profile_t
{
	/**
	 * \brief Allowclear mode flag
	 *
	 * This option means that the ZRTP peer allows SRTP termination. If allowclear is disabled, the 
	 * ZRTP peer must stay in protected mode until the moment the ZRTP stream is shut down. When not 
	 * in "allowclear" mode, libzrtp will reject all incoming GoClear packages and will not generate 
	 * its own.
	 *
	 * Setting the value equal to 1 turns "allowclear" on, and 0 turns "allowclear" off. If 
	 * "allowclear" is disabled zrtp_stream_clear() returns zrtp_status_fail.
	 */
	uint8_t				allowclear;
	
	/**
	 * \brief ZRTP "autosecure" mode flag
	 *
	 * In "autosecure" mode, a protected connection will be initiated automatically just after 
	 * stream start-up. If the option "autosecure" is switched off, then a secure connection can be 
	 *initialized only by calling zrtp_stream_secure().
	 */
	uint8_t				autosecure;   
	
	/**
	 * \brief Disclose bit.
	 * 
	 * This field MUST be set by user application if it's going to disclose stream keys.
	 */
	uint8_t				disclose_bit;
	
	/**
	 * \brief Enabled Discovery Optimization
	 *
	 * ZRTP protocol specification allows to speed-up the discovery process by sending Commit
	 * instead of HelloAck. This is the default behavior for most of ZRTP endpoints. It allows to 
	 * eliminate one unnecessary exchange.
	 *
	 * At other hand, this optimization may cose some problems on slow devices: using this option,
	 * the endpoint starts to compute DH value right after receiving remote Hello. It may take
	 * seginificent amount of time on slow device (of is the device is busy on other calculations). 
	 * As all libzrtp messages are processed in single thread, while local endpoint computing DH
	 * it be unable to response on remote Hello-s and remote side may switch to NO_ZRTP state.
	 *
	 * Not use this option is you running libzrtp on slow device or your software supports HQ video
	 * conferences. Enabled by default.
	 */
	uint8_t				discovery_optimization;
	
	/**
	 * \brief Cache time-to-live
	 *
	 * The time interval libzrtp should retain secrets. This parameter sets the secret's time to 
	 * live in seconds. This option is global for all connections processed by the library. It is 
	 * used together with zrtp_session_info_t#cache_ttl.
	 *
	 * ZRTP_CACHE_DEFAULT_TTL value is used by default.
	 */
	uint32_t			cache_ttl;
	
	/** \brief SAS calculation scheme preferences */
	uint8_t				sas_schemes[ZRTP_MAX_COMP_COUNT+1];
	
	/** \brief Cipher type preferences */
	uint8_t				cipher_types[ZRTP_MAX_COMP_COUNT+1];
	
	/** \brief Public key exchange scheme preferences */
	uint8_t				pk_schemes[ZRTP_MAX_COMP_COUNT+1];
	
	/** \brief Auth tag length preferences */
	uint8_t				auth_tag_lens[ZRTP_MAX_COMP_COUNT+1];
	
	/**
	 * \brief Hash calculation scheme preferences
	 * \note ZRTP_HASH_SHA256 is only one hash algorithm supported by current version of libzrtp.
	 */
	uint8_t				hash_schemes[ZRTP_MAX_COMP_COUNT+1];
};

/**
 * \brief Shared secret structure
 * \ingroup zrtp_iface_cache
 *
 * This structure stores ZRTP shared secret values used in the protocol.
 */
struct zrtp_shared_secret_t
{
    /** \brief ZRTP secret value */
    zrtp_string64_t			value;        

	/**
	 * \brief last usage time-stamp in seconds.
	 *
	 * Library updates this value on generation of the new value based on previous one.
	 */
	uint32_t				lastused_at;

	/**
	 * \brief TTL value in seconds.
	 *
	 * Available for reading after the Hello exchange. Updated on switching to Secure state.
	 */
	uint32_t				ttl;
	
	/**
     * \brief Loaded secret flag.
     *
     * When the flag is set (= 1), the secret has been loaded from the cache. Otherwise the secret 
     * has been generated.
     * \warning For internal use only. Don't modify this flag in the application.
     */
    uint8_t					_cachedflag;	
};

/**
 * \brief Lists MitM roles on PBX call transferring
 * 
 * Enumeration type for the ZRTP modes based on the role of the MitM.
 */
typedef enum zrtp_mitm_mode_t
{
	/** MitM is not supported or not activated. */
	ZRTP_MITM_MODE_UNKN = 0,
	
	/**
	 * \brief Client-side mode called to the PBX in ZRTP trusted MiTM mode.
	 * 
	 * Libzrtp activates this state on receiving an Hello, indicating that remote side is trusted
	 * MiTM.
	 */
	ZRTP_MITM_MODE_CLIENT,
	
	/**
	 * \brief Server-side mode to transfer SAS to the registrant.
	 *
	 * Libzrtp switches to this state on starting zrtp_update_remote_options().
	 */
	ZRTP_MITM_MODE_RECONFIRM_SERVER,
	/**
	 * \brief Client-side mode accepted SAS transfer from the trusted MiTM.
	 *
	 * Libzrtp activates this state on receiving an SASRELAY from a trusted MiTM endpoint.
	 */
	ZRTP_MITM_MODE_RECONFIRM_CLIENT,
	/**
	 * \brief Server-side mode to accept the user's registration requests.
	 *
	 * Libzrtp switches to this state on starting a registration stream by
	 * zrtp_stream_registration_start() or zrtp_stream_registration_secure().
	 */
	ZRTP_MITM_MODE_REG_SERVER,
	/**
	 * \brief User-side mode to confirm the registration ritual.
	 * 
	 * The library enables this state when a remote party invites it to the registration ritual
	 * by a special flag in the Confirm packet.
	 */
	ZRTP_MITM_MODE_REG_CLIENT
} zrtp_mitm_mode_t;


/** \manonly */


/*======================================================================*/
/*    Internal ZRTP libzrtp datatypes                                   */
/*======================================================================*/

/**
 * @defgroup types_dev libzrtp types for developers
 * The data types used in inside libzrte. This section is for libzrtp developers
 * @ingroup zrtp_dev
 * \{
 */


/**
 * @brief Enumeration for ZRTP protocol packets type definition
 * @warning! Don't change order of these definition without synchronizing with
 * print* functions (see zrtp_log.h)
 */
typedef enum
{
	ZRTP_UNPARSED		= -1,   /** Unparsed packet */
	ZRTP_NONE			= 0,	/** Not ZRTP packet */
	ZRTP_HELLO			= 1,    /** ZRTP protocol HELLO packet */
	ZRTP_HELLOACK		= 2,    /** ZRTP protocol HELLOACK packet */
	ZRTP_COMMIT			= 3,    /** ZRTP protocol COMMIT packet */
	ZRTP_DHPART1		= 4,    /** ZRTP protocol DHPART1 packet */
	ZRTP_DHPART2		= 5,    /** ZRTP protocol DHPART2 packet */
	ZRTP_CONFIRM1		= 6,    /** ZRTP protocol CONFIRM1 packet */
	ZRTP_CONFIRM2		= 7,    /** ZRTP protocol CONFIRM2 packet */
	ZRTP_CONFIRM2ACK	= 8,    /** ZRTP protocol CONFIRM2ACK packet */
	ZRTP_GOCLEAR		= 9,    /** ZRTP protocol GOCLEAR packet */
	ZRTP_GOCLEARACK		= 10,   /** ZRTP protocol GOCLEARACK packet */
	ZRTP_ERROR			= 11,   /** ZRTP protocol ERROR packet */
	ZRTP_ERRORACK		= 12,   /** ZRTP protocol ERRORACK packet */
	ZRTP_PROCESS		= 13,   /** This is not a packet type but type of task for scheduler */
	ZRTP_SASRELAY		= 14,	/** ZRTP protocol SASRELAY packet */
	ZRTP_RELAYACK		= 15,	/** ZRTP protocol RELAYACK packet */
	ZRTP_ZFONEPING		= 16,	/** Zfone3 Ping packet */
	ZRTP_ZFONEPINGACK	= 17,	/** Zfone3 PingAck packet */
	ZRTP_MSG_TYPE_COUNT	= 18
} zrtp_msg_type_t;


/**
 * @brief enumeration for protocol state-machine roles
 * Protocol role fully defines it's behavior. ZRTP peer chooses a role according
 * to specification. For details see internal developers documentation
 */
typedef enum zrtp_statemachine_type_t
{
	ZRTP_STATEMACHINE_NONE		= 0,	/** Unknown type. Used as error value */
	ZRTP_STATEMACHINE_INITIATOR	= 1,	/** Defines initiator's protocol logic */
	ZRTP_STATEMACHINE_RESPONDER	= 2		/** Defines responder's protocol logic */
} zrtp_statemachine_type_t;

#define    ZRTP_BIT_RS1		0x02
#define    ZRTP_BIT_RS2		0x04
#define    ZRTP_BIT_AUX		0x10
#define    ZRTP_BIT_PBX		0x20

/**
 * @brief Library global context
 * Compilers and linkers on some operating systems don't support the declaration
 * of global variables in c files. Storing a context allows us to solve this
 * problem in a way that unifies component use. The context is created by calling
 * zrtp_init(), and is destroyed with zrtp_down(). It contains data necessary
 * for crypto-component algorithms, including hash schemes, cipher types, SAS
 * schemes etc. Context data can be divided into three groups:
 *  - ID of client ZRTP peer;
 *  - RNG related fields (hash context for entropy computing);
 *  - DH scheme related fields(internal data used for DH exchange);
 *  - headers of the lists of every crypto-component type used for component
 *    management.
 * All of this data, except for "RNG related fields", is for internal use only
 * and set automatically. All that is needed is to link every created session
 * to global context.
 * @sa zrtp_init() zrtp_down() zrtp_session_init() 
 */
struct zrtp_global_t
{
	uint32_t				lic_mode;			/** ZRTP license mode. */
    zrtp_string16_t			client_id;			/** Local ZRTP client ID. */
	uint8_t					is_mitm;			/** Flags defines that the local endpoint acts as ZRTP MiTM. */
    MD_CTX					rand_ctx;			/** Hash context for entropy accumulation for the RNG unit. */
    uint8_t					rand_initialized;	/** RNG unit initialization flag. */
	zrtp_string256_t		def_cache_path;		/** Full path to ZRTP cache file. */
	unsigned				cache_auto_store;	/** Set when user wants libzrtp to flush the cache once it changed */
    zrtp_mutex_t*			rng_protector;		/** This object is used to protect the shared RNG hash zrtp#rand_ctx */
    struct BigNum			one;				/** This section provides static data for DH3K and DH4K components */
    struct BigNum			G;
	struct BigNum			P_2048;
    struct BigNum			P_2048_1;
    struct BigNum			P_3072;
    struct BigNum			P_3072_1;
	uint8_t					P_2048_data[256];
    uint8_t					P_3072_data[384];
    mlist_t					hash_head;			/** Head of hash components list */
    mlist_t					cipher_head;		/** Head of ciphers list */
    mlist_t					atl_head;			/** Head of ATL components list */
    mlist_t					pktype_head;		/** Head of public key exchange schemes list */
    mlist_t					sas_head;			/** SAS schemes list */
    void*					srtp_global;		/** Storage for some SRTP global data */
    mlist_t					sessions_head;		/** Head of ZRTP sessions list */
	uint32_t				sessions_count;		/** Global sessions count used to create ZRTP session IDs. For debug purposes mostly. */
	uint32_t				streams_count;		/** Global streams count used to create ZRTP session IDs. For debug purposes mostly. */
    zrtp_mutex_t*			sessions_protector;	/** This object is used to synchronize sessions list operations */
	zrtp_callback_t			cb;					/** Set of feedback callbacks used by libzrtp to interact with the user-space.*/
};


/**
 * @brief RTP packet structure used in libzrtp
 * Used for conveniently working with RTP/ZRTP packets. A binary RTP/ZRTP
 * packet is converted into a zrtp_rtp_info_t structure before processing by
 * _zrtp_packet_preparse()
 */
typedef struct zrtp_rtp_info_t
{	
	/** Packet length in bytes */
	uint32_t				*length;
	
	/** Pointer to the RTP/ZRTP packet body */
	char					*packet;
	
	/** Pointer to ZRTP Message part (skip ZRTP transport header part) */
	void					*message;
	
	/** ZRTP packet type (ZRTP_NONE in case of non command packet) */
	zrtp_msg_type_t			type;
	
	/** Straightened RTP/ZRTP sequence number in host mode */
	uint32_t				seq;
	
	/** RTP SSRC/ZRTP in network mode */
	uint32_t				ssrc;
} zrtp_rtp_info_t;


/**
 * @brief Retained secrets container
 * Contains the session's shared secret values and related flags restored from
 * the cache. Every subsequent stream within a session uses these values
 * through @ref zrtp_proto_secret_t pointers. By definition, different ZRTP
 * streams can't change secret values. Secret flags are protected against race
 * conditions by the mutex \c _protector. For internal use only.
 */
typedef struct zrtp_secrets_t
{    
	/** First retained secret RS1. */
    zrtp_shared_secret_t    *rs1;
	
	/** Second retained secret RS1. */  
    zrtp_shared_secret_t    *rs2;
	
	/** User-defined secret. */
    zrtp_shared_secret_t    *auxs;
	
	/** PBX Secret for trusted MiTMs. */
    zrtp_shared_secret_t    *pbxs;
	
	/** Bit-map to summarize shared secrets "Cached" flags. */
    uint32_t				cached;
	uint32_t				cached_curr;
	
	/** Bit-map to summarize shared secrets "Matches" flags. */
    uint32_t				matches;
	uint32_t				matches_curr;
	
	/** Bit-map to summarize shared secrets "Wrongs" flags. */
	uint32_t				wrongs;
	uint32_t				wrongs_curr;
	
	/** This flag equals one if the secrets have been uploaded from the cache. */
    uint8_t					is_ready;	
} zrtp_secrets_t;


/**
 * @brief Protocol shared secret
 * Wrapper around the session shared secrets \ref zrtp_shared_secret. Used 
 * for ID storing and secret sorting according to ZRTP ID sec. 5.4.4.
 */
typedef struct zrtp_proto_secret_t
{
	/** Local-side secret ID */
	zrtp_string8_t			id;
	
	/** Remote-side secret ID */
	zrtp_string8_t			peer_id;
	
	/** Pointer to the binary value and set of related flags */
	zrtp_shared_secret_t	*secret;
} zrtp_proto_secret_t;


/**
 * @brief ZRTP messages cache
 * This structure contains ZRTP messages prepared for sending or received from
 * the other side. This scheme allows speed-ups the resending of packets and
 * computing message hashes, and makes resending thread-safe. Besides packets,
 * tasks retries are stored as well.
 */
typedef struct zrtp_stream_mescache_t
{
	zrtp_packet_Hello_t		peer_hello;
	zrtp_packet_Hello_t		hello;
	zrtp_packet_GoClear_t   goclear;    
	zrtp_packet_Commit_t    peer_commit;
	zrtp_packet_Commit_t    commit;
	zrtp_packet_DHPart_t    peer_dhpart;
	zrtp_packet_DHPart_t    dhpart;
	zrtp_packet_Confirm_t   confirm;
	zrtp_string32_t			h0;
	zrtp_packet_Confirm_t   peer_confirm;
	zrtp_packet_Error_t     error;
	zrtp_packet_SASRelay_t  sasrelay;
	
	zrtp_retry_task_t       hello_task;
	zrtp_retry_task_t       goclear_task;
	zrtp_retry_task_t       dh_task;
	zrtp_retry_task_t       commit_task;
	zrtp_retry_task_t       dhpart_task;
	zrtp_retry_task_t       confirm_task;
	zrtp_retry_task_t       error_task;
	zrtp_retry_task_t       errorack_task;
	zrtp_retry_task_t       sasrelay_task;
	
	/*!
	 * Hash pre-image of the remote party Hello retrieved from Signaling. When
	 * user calls zrtp_signaling_hash_set() libzrtp stores hash value in this
	 * variable and checks all incoming Hello-s to prevent DOS attacks.
	 */
	zrtp_string64_t			signaling_hash;
} zrtp_stream_mescache_t;


/**
 * @brief Crypto context for Diffie-Hellman calculations
 * Used only by DH streams to store Diffie-Hellman calculations. Allocated on
 * protocol initialization and released on switching to SECURE mode.
 */
typedef struct zrtp_dh_crypto_context_t
{
	/** DH secret value */
	struct BigNum			sv;
	
	/** DH public value */
	struct BigNum			pv;
	
	/** DH public value recalculated for remote side */
	struct BigNum			peer_pv;
	
	/** DH shared secret. DHSS = hash(DHResult) */
	zrtp_string64_t			dhss;
	
	unsigned int			initialized_with;
} zrtp_dh_crypto_context_t;


/*! 
 * \brief Crypto context for ECDSA calculations
 * Used to store ECDSA keys and calculations. Allocated on
 * protocol initialization and released on switching to SECURE mode.
 */
typedef struct zrtp_dsa_crypto_context_t
{
	struct BigNum			sv;		/*!< DSA secret value */
	struct BigNum			pv;		/*!< DSA public value */
	struct BigNum			peer_pv;/*!< DSA public value for some remote side */
} zrtp_dsa_crypto_context_t;


/**
 * @brief Protocol crypto context
 * Used as temporary storage for ZRTP crypto data during protocol running.
 * Unlike \ref zrtp_stream_crypto_t this context is needed only during key
 * negotiation and destroyed on switching to SECURE state.  
 */
typedef struct zrtp_proto_crypto_t
{	
	/** ZRTP */
	zrtp_string128_t		kdf_context;
	
	/** ZRTP stream key */
	zrtp_string64_t			s0;
	
	/** Local hvi value for the hash commitment: hvi or nonce for Multistream. */
	zrtp_string64_t			hv;
	
	/** Remove hvi value for the hash commitment: hvi or nonce for Multistream. */
	zrtp_string64_t			peer_hv;
	
	/** Total messages hash. See ZRTP ID 5.4.4/5.5.4 */
	zrtp_string64_t			mes_hash;
	
	/** RS1 */
	zrtp_proto_secret_t		rs1;
	
	/** RS2 */
	zrtp_proto_secret_t		rs2;
	
	/** User-Defined secret */
	zrtp_proto_secret_t		auxs;
	
	/** PBX secret */
	zrtp_proto_secret_t		pbxs;
} zrtp_proto_crypto_t;

/*!
 * \brief ZRTP protocol structure
 * Protocol structure is responsible for ZRTP protocol logic (CLEAR-SECURE
 * switching) and RTP media encrypting/decrypting. The protocol is created
 * right after the discovery phase and destroyed on stream closing.
 */
struct zrtp_protocol_t
{
	/** Protocol mode: responder or initiator. */
	zrtp_statemachine_type_t type;
	
	/** Context for storing protocol crypto data. */
	zrtp_proto_crypto_t*	cc;
	
	/** SRTP crypto engine */
	zrtp_srtp_ctx_t*		_srtp;
	
	/** Back-pointer to ZRTP stream context. */
	zrtp_stream_t		*context;		    	
};

/**
 * @brief Stream-persistent crypto options.
 * Unlike \ref zrtp_proto_crypto_t these data are kept after switching to Secure
 * state or stopping the protocol; used to sign/verify Confirm and GoClear packets.
 */
typedef struct zrtp_stream_crypto_t
{
	/** Local side hmackey value. */
	zrtp_string64_t			hmackey;
	
	/** Remote side hmackey value. */
	zrtp_string64_t			peer_hmackey;
	
	/** Local side ZRTP key for Confirms protection. */
	zrtp_string64_t			zrtp_key;
	
	/** Remote side ZRTP key for Confirms verification. */
	zrtp_string64_t			peer_zrtp_key;	
} zrtp_stream_crypto_t;


/**
 * @brief stream media context. Contains all RTP media-related information.
 */
typedef struct zrtp_media_context_t
{
	/** The highest ZRTP message sequence number received. */
	uint32_t				high_in_zrtp_seq;
	
	/** The last ZRTP message sequence number sent. */
	uint32_t				high_out_zrtp_seq;
	
	/** The highest	RTP media sequence number received; used by SRTP. */
	uint32_t				high_in_media_seq;
	
	/** The highest RTP media sequence number sent; used by SRTP. */
	uint32_t				high_out_media_seq;
	
	/** SSRC of the RTP media stream associated with the current ZRTP stream. */
	uint32_t				ssrc;
} zrtp_media_context_t;

/*!
 * \brief ZRTP stream context
 * \warning Fields with prefix "_" are for internal use only.
 */
struct zrtp_stream_t
{
	/*! Stream unique identifier for debug purposes */
	zrtp_id_t				id;
	
	/*!
	 * \brief Stream mode
	 * This field defines libzrtp behavior related to specified contexts. See
	 * <A HREF="http://zfoneproject.com/zrtp_ietf.html">"ZRTP Internet Draft"</A>
	 * and \ref usage for additional information about stream types and their
	 * processing logic.
	 */
	zrtp_stream_mode_t		mode;
	
	/*!
	 * \brief Defines ZRTP role in trusted MitM scheme.
	 * The value of this mode determines the behavior of the ZRTP machine
	 * according to it's role in the MitM scheme.  Initially the mode is
	 * ZRTP_MITM_MODE_UNKN and then changes on protocol running.
	 */	 
	zrtp_mitm_mode_t		mitm_mode;
	
	/*! 
	 * \brief Previous ZRTP protocol states
	 * Used in analysis to determine the reason for a switch from one state to
	 * another. Enabled by _zrtp_change_state(.
	 */
	zrtp_state_t			prev_state;
	
	/** 1 means that peer Hello have been raceived within current ZRTP session */
	uint8_t					is_hello_received;
	
	/*!< Reflects current state of ZRTP protocol */
	zrtp_state_t			state;
	
	/**
	 * @brief Persistent stream crypto options.
	 * Stores persistent crypto data needed after Confirmation. This data can be
	 * cleared only when the stream is destroyed.
	 */
	zrtp_stream_crypto_t	cc;
	
	/** DH crypto context used in PK calculations */
	zrtp_dh_crypto_context_t dh_cc;
	
	/*!
	 * \brief Pointer to the ZRTP protocol implementation
	 * The protocol structure stores all crypto data during the securing
	 * procedure.  After switching to SECURE state the protocol clears all
	 * crypto sources and performs traffic encryption/decryption.
	 */
	zrtp_protocol_t			*protocol;

	/*!< Holder for RTP/ZRTP media stream options. */
	zrtp_media_context_t	media_ctx;
	
	/*!< ZRTP messages and task retries cache */
	zrtp_stream_mescache_t	messages;
	
	/*!
	 * Current value of "allowclear" option exchanged during ZRTP negotiation.
	 * Available for reading in SECURE state.     
	 */
	uint8_t					allowclear;
	
	/*!
	 * This flag shows when remote side is "passive" (has license mode PASSIVE)
	 * Available for reading in CLEAR state.      
	 */
	uint8_t					peer_passive;
	
	/*!
	 * \brief actual lifetime of stream secrets
	 * This variable contains the interval for retaining secrets within an
	 * established stream. In accordance with <A
	 * HREF="http://zfoneproject.com/zrtp_ietf.html">"ZRTP Internet Draft"</A>
	 * this value is calculated as the minimal of local and remote TTLs after
	 * confirmation. Value is given in seconds and can be read in the SECURE
	 * state. It may be used in displaying session parameters.
	 */
	uint32_t				cache_ttl;
	
	/*!
	 * \brief Peer disclose bit Indicates the ability of the remote side to
	 * disclose its session key.  Specifies that the remote side allows call
	 * monitoring. If this flag is set, the end user must be informed. It can
	 * be read in the SECURE state.
	 */
	uint8_t					peer_disclose_bit;    
	
	/*!
	 * \brief Last protocol error code
	 * If there is a mistake in running the protocol, zrtp_event_callback() 
	 * will  be called and the required error code will be set to this field.
	 * An error code is the numeric representation of ZRTP errors defined in
	 * the draft. All error codes are defined by \ref zrtp_protocol_error_t.     
	 */
	zrtp_protocol_error_t	last_error;
		
	/**
	 * Duplicates MiTM flag from peer Hello message
	 */
	uint8_t					peer_mitm_flag;
	
	/**
	 * Duplicates U flag from peer Hello message
	 */
	uint8_t					peer_super_flag;
	
	/*!
	 * \brief Pointer to the concurrent DH stream
	 * If Commit messages are sent by both ZRTP endpoints at the same time, but
	 * are received in different media streams, "tie-breaking" rules apply - the
	 * Commit message with the lowest hvi value is discarded and the other side
	 * becomes the initiator. The media stream in which the Commit was sent will
	 * proceed through the ZRTP exchange while the media stream with the discarded
	 * Commit must wait for the completion of the other ZRTP exchange. A pointer
	 * to that "waiting" stream is stored in \c _concurrent. When the running
	 * stream is switched to "Initiating Secure" the concurrent stream is resumed.
	 */
	zrtp_stream_t			*concurrent;
	
	/** Back-pointer to the ZRTP global data */
	zrtp_global_t			*zrtp;
	
	/** Pointer to parent session context. Used for back capability */
	zrtp_session_t			*session;
	
	/*!< Public key exchange component used within current stream */
	zrtp_pk_scheme_t		*pubkeyscheme;
	
	/*!
	 * Pointer to the user data. This pointer can be used for fast access to
	 * some additional data attached to this ZRTP stream by the user application
	 */
	void					*usr_data;
	
	/*!
	 * Pointer to the peer stream during a trusted MiTM call.
	 * @sa zrtp_link_mitm_calls()
	 */
	zrtp_stream_t			*linked_mitm;
	
	/*!
	 * \brief Stream data protector
	 * A mutex is used to avoid race conditions during asynchronous calls
	 * (zrtp_stream_secure(), zrtp_stream_clear() etc.) in parallel to the main
	 * processing loop zrtp_process_rtp/srtp().
	 */
	zrtp_mutex_t*			stream_protector;
};


/*!
 * \brief ZRTP session context
 * Describes the state of the ZRTP session. Stores data necessary and sufficient
 * for processing ZRTP sessions. Encapsulates ZRTP streams and all crypto-data.
 */
struct zrtp_session_t
{
	/*! Session unique identifier for debug purposes */
	zrtp_id_t				id;
	
	/*!
	 * \brief Local-side ZID
	 * The unique 12-characters string that identifies the local ZRTP endpoint.
	 * It must be generated by the user application on installation and used
	 * permanently for every ZRTP session. This ID allows remote peers to
	 * recognize this ZRTP endpoint.
	 */	
	zrtp_string16_t			zid;
	
	/*!
	 * \brief Remote-side ZID
	 * Extracted from the Hello packet of the very first ZRTP stream. Uniquely
	 * identifies the remote ZRTP peer. Used in combination with the local zid
	 * to restore secrets and other data from the previous call. Available for
	 * reading after the discovering phase. 
	 */
	zrtp_string16_t			peer_zid;
	
	/*!< ZRTP profile, defined crypto options and behavior for every stream within current session */
	zrtp_profile_t			profile;
	
	/*
	 * Signaling Role which protocol was started with, one of zrtp_signaling_role_t values.
	 */
	unsigned				signaling_role;
	
	/*!
	 * \brief Set of retained secrets and flags for the current ZRTP session.
	 * libzrtp uploads secrets and flags from the cache on the very first
	 * stream within every ZRTP session. 
	 */
	zrtp_secrets_t			secrets;
	
	/*!< ZRTP session key used to extend ZRTP session without additional DH exchange */
	zrtp_string64_t			zrtpsess;    	
	
	/** First SAS base32/256 string */
	zrtp_string16_t			sas1;
	
	/** Second SAS 256 string */
	zrtp_string16_t			sas2;
	
	/** Binary SAS digest (ZRTP_SAS_DIGEST_LENGTH bytes) */
	zrtp_string32_t			sasbin;
	
	/*!< Back-pointer to the ZRTP global data */
	zrtp_global_t			*zrtp;
	
	/*!< Back-pointer to user data associated with this session context. */
	void					*usr_data;
	
	/** Hash component used within current session */
	zrtp_hash_t				*hash;
	
	/** Cipher component used within current session */
	zrtp_cipher_t			*blockcipher;
	
	/** SRTP authentication component used within current session */
	zrtp_auth_tag_length_t	*authtaglength;    
	
	/** SAS scheme component used within current session */
	zrtp_sas_scheme_t		*sasscheme;
	
	/** List of ZRTP streams attached to the session. */
	zrtp_stream_t			streams[ZRTP_MAX_STREAMS_PER_SESSION];
	
	/** This object is used to synchronize all stream list operations */
	zrtp_mutex_t*			streams_protector;
	
	/** Prevents race conditions if streams start simultaneously. */
	zrtp_mutex_t*			init_protector;
	
	/**
	 * This flag indicates that possible MiTM attach was detected during the protocol exchange.	 
	 */
	uint8_t					mitm_alert_detected;
	
	mlist_t					_mlist;
};

/*! \} */


/*===========================================================================*/
/* Data types and definitions for SRTP                                       */
/*===========================================================================*/

#if ZRTP_BYTE_ORDER == ZBO_LITTLE_ENDIAN

/**
 * RTP header structure
 * @ingroup dev_srtp
 */
typedef struct
{
  uint16_t		cc:4;       /** CSRC count             */
  uint16_t		x:1;        /** header extension flag  */
  uint16_t		p:1;        /** padding flag           */
  uint16_t		version:2;  /** protocol version		*/
  uint16_t		pt:7;       /** payload type           */
  uint16_t		m:1;        /** marker bit             */
  uint16_t		seq;        /** sequence number        */
  uint32_t		ts;         /** timestamp              */
  uint32_t		ssrc;       /** synchronization source */
} zrtp_rtp_hdr_t;

/**
 * RTCP header structure
 * @ingroup dev_srtp
 */
typedef struct
{
  unsigned char	rc:5;       /** reception report count */
  unsigned char p:1;        /** padding flag           */
  unsigned char version:2;  /** protocol version       */
  unsigned char pt:8;       /** payload type           */
  uint16_t		len;		/** length                 */
  uint32_t		ssrc;		/** synchronization source */
} zrtp_rtcp_hdr_t;

typedef struct
{
  unsigned int	index:31;	/** srtcp packet index in network order! */
  unsigned int	e:1;        /** encrypted? 1=yes */
                            /** optional mikey/etc go here */
                            /** and then the variable-length auth tag */
} zrtp_rtcp_trailer_t;

#else

/**
 * RTP header structure
 * @ingroup dev_srtp
 */
typedef struct
{
  uint16_t		version:2;	/** protocol version       */
  uint16_t		p:1;		/** padding flag           */
  uint16_t		x:1;		/** header extension flag  */
  uint16_t		cc:4;		/** CSRC count             */
  uint16_t		m:1;		/** marker bit             */
  uint16_t		pt:7;		/** payload type           */
  uint16_t		seq;		/** sequence number        */
  uint32_t		ts;			/** timestamp              */
  uint32_t		ssrc;		/** synchronization source */
} zrtp_rtp_hdr_t;

/**
 * RTCP header structure
 * @ingroup dev_srtp
 */
typedef struct
{
  unsigned char	version:2;	/** protocol version       */
  unsigned char p:1;        /** padding flag           */
  unsigned char rc:5;       /** reception report count */
  unsigned char pt:8;       /** payload type           */
  uint16_t		len;        /** length                 */
  uint32_t		ssrc;       /** synchronization source */
} zrtp_rtcp_hdr_t;

typedef struct
{
  unsigned int e:1;         /** encrypted? 1=yes */
  unsigned int index:31;    /** srtcp packet index */  
} zrtp_rtcp_trailer_t;

#endif

/**
 * RTP header extension structure
 * @ingroup dev_srtp
 */
typedef struct
{
  uint16_t		profile_specific; /** profile-specific info               */
  uint16_t		length;           /** number of 32-bit words in extension */
} zrtp_rtp_hdr_xtnd_t;


/** \endmanonly */

#endif  /* __ZRTP_TYPES_H__ */

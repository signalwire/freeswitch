/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */

#ifndef __ZRTP_CRYPTO_H__
#define __ZRTP_CRYPTO_H__

#include "bn.h"
#include "zrtp_types.h"
#include "zrtp_error.h"
#include "zrtp_engine.h"
#include "zrtp_config_user.h"
#include "zrtp_ec.h"



/*!
 * \defgroup crypto Library crypto-components
 * \ingroup zrtp_dev
 *
 * This section describes functions and data types for managing crypto-components.
 * All these functions and structures are used by the libZRTP kernel for the
 * built-in crypt-components management. The developer has the option of
 * implementing and integrating her own components into the library. This is not
 * a full manual on creating crypto-components. Its purpose is only to elucidate
 * the library functionality.
 *
 * The concept behind crypto components is similar to that of classes in object
 * oriented programming.  The components are defined as structures and
 * manipulated by functions. Component attributes are stored in 'contexts', and
 * are defined during initialization. Resources allocated at initialization are
 * freed with the 'free' function.
 *
 * Components are divided into 5 functional groups (component types):
 *  - ciphers;
 *  - hash/hmac components;
 *  - public key exchange schemes;
 *  - components defined SRTP authentication scheme;
 *  - SAS calculation schemes.
 * Within a group, components are distinguished by integer identifiers and by
 * their defined functionality. So to fully identify a component, you need to
 * know its type and its identifier. (For example an AES cipher with a 128 bit
 * key is defined as: ZRTP_CC_CIPHER, zrtp_cipher_id_t::ZRTP_CIPHER_AES128).
 * The high number of components means that every component must have a minimal
 * set of attributes and functions: type identifier, and function initialization
 * and deinitialization. The base type of all components is zrtp_comp_t. Every
 * new component MUST start with definitions of this structure strictly in the
 * given order.
 * \warning
 * Every crypto-component included in libZRTP was developed and tested by
 * professionals. Its presence is functionally based. Using only the built-in
 * components gives you 100% crypto-strength and the guarantee of the fully
 * tested code. Never use your own components without strong reasons. If you
 * have noticed the absence of any important component in the library, contact
 * the developers. Reasonable offers will be considered for implementation in
 * the following versions. 
 * \{
 */


/*============================================================================*/
/* 	  Types  of libZRTP crypto-components definitions						  */
/*============================================================================*/

/*!
 * \brief Enumeration for crypto-components types definition
 */
typedef enum zrtp_crypto_comp_t
{
    ZRTP_CC_HASH		= 1,	/*!< hash calculation schemes */
    ZRTP_CC_SAS			= 2,	/*!< short autentification scheme components */
    ZRTP_CC_CIPHER		= 3,	/*!< ciphers */
    ZRTP_CC_PKT			= 4,	/*!< public key exchange scheme */	
	ZRTP_CC_ATL         = 5,
}zrtp_crypto_comp_t;


/*!
 * This ID with code 0 is used as an error signal by all crypto-components
 * groups to indicate a wrongly defined component identifier. 
 */
#define ZRTP_COMP_UNKN 0

/*! Defines types of hash functions */
typedef enum zrtp_hash_id_t
{	
	ZRTP_HASH_SHA256	= 1,
	ZRTP_HASH_SHA384	= 2
} zrtp_hash_id_t;

/*! Defines types of ciphers */
typedef enum zrtp_cipher_id_t
{	
	ZRTP_CIPHER_AES128	= 1,
	ZRTP_CIPHER_AES256	= 2
} zrtp_cipher_id_t;

/*! Defines SRTP authentication schemes */
typedef enum zrtp_atl_id_t
{
	ZRTP_ATL_HS32		= 1,
	ZRTP_ATL_HS80		= 2
} zrtp_atl_id_t;

/*! Defines public key exchange schemes */
/* WARNING! don't change order of the PK components definitions! */
typedef enum zrtp_pktype_id_t
{
	ZRTP_PKTYPE_PRESH	= 1,
	ZRTP_PKTYPE_MULT	= 2,
	ZRTP_PKTYPE_DH2048	= 3,
	ZRTP_PKTYPE_EC256P  = 4,
	ZRTP_PKTYPE_DH3072	= 5,	
    ZRTP_PKTYPE_EC384P  = 6,	
    ZRTP_PKTYPE_EC521P  = 7,
	ZRTP_PKTYPE_DH4096	= 8
} zrtp_pktype_id_t;

/*! Defines modes of short authentication scheme calculation */
typedef enum zrtp_sas_id
{
	ZRTP_SAS_BASE32		= 1,
	ZRTP_SAS_BASE256	= 2
} zrtp_sas_id_t;

 
/*!
 * \brief Global structure for all crypto-component types.
 * \warning All developed components must have these 4 fields at the beginning.
 */
typedef struct zrtp_comp_t
{
    zrtp_uchar4_t		type;		/*!< 4-character symbolic name defined by ZRTP Draft */
    uint8_t				id;			/*!< Integer component identifier */
    zrtp_global_t*		zrtp;/*!< ZRTP global context */
    
	/*!
     * \brief Component initiation function.
     * This function body is for holding component initialization code. libzrtp
	 * calls the function before using a component, at its registration. If the
	 * component does not require additional actions for initialization, the
	 * value of this field can be NULL.
     * \param self - self-pointer for fast access to structure data.
     * \return 
     *	- zrtp_status_ok - if initialized successfully;
     *	- one of \ref zrtp_status_t errors - if initialization failed.
     */
	zrtp_status_t		(*init)(void* self);
	
	/*!
     * \brief Component deinitializtion function.
     * This function body is for holding component deinitialization code and
     * all code for releasing allocated resources. libzrtp calls the function
     * at the end of component use, at context deinitialization. If the component
	 * does not require additional actions for deinitialization, the value of
	 * this field can be NULL.
     * \param self - pointer to component structure for deinitialization.
     * \return
     *	- zrtp_status_ok - if deinitialized successfully;
     *	- one of \ref zrtp_status_t errors - if deinitialization failed.
     */
    zrtp_status_t (*free)(void* self);
} zrtp_comp_t;


/*!
 * \brief Structure for defining the hash-value computing scheme 
 * The ZRTP context field zrtp_stream#_hash is initialized by the given type
 * value and used for all hash calculations within the ZRTP sessions. Having
 * implemented a structure of this type, it is possible to integrate new hash
 * calculation schemes into libzrtp.
 */
struct zrtp_hash_t
{
	zrtp_comp_t		base;

    /*!
     * \brief Begin hash computation with update support.
     * The following set of functions ( zrtp_hash#hash_begin, zrtp_hash#hash_update,
	 * zrtp_hash#hash_end) implements a standard hash calculation scheme with
	 * accumulation. The functions perform the required actions to start
	 * calculations and to allocate hash-contexts for preserving intermediate
	 * results and other required information. The allocated context will be
	 * passed-to by the subsequent calls zrtp_hash#hash_update and zrtp_hash#hash_end.
     * \param self - self-pointer for fast access to structure data
     * \return
     * 	- pointer to allocated hash-context if successful;
     * 	- NULL if error.
     */
    void*			(*hash_begin)(zrtp_hash_t *self);
	
    /*!
     * \brief Process more input data for hash calculation
     * This function is called in the hash-building chain to obtain additional
	 * data that it then processes and recalculates intermediate values.
     * \param self - self-pointer for fast access to structure data;
     * \param ctx - hash-context for current hash-value calculation;
     * \param msg - additional source data for processing;
     * \param length - length of additional data in bytes.
     * \return
     *	- zrtp_status_ok - if successfully processed;
     * 	- one of \ref zrtp_status_t errors - if error.
     */
    zrtp_status_t	(*hash_update)( zrtp_hash_t *self,
									void *ctx,
									const int8_t*msg,
									uint32_t length );
	
    /*! 
     * \brief Completes the computation of the current hash-value 
     * This function completes the computation of the hash-value with accumul.
	 * After completion, the hash-context previously allocated by the call to
	 * zrtp_hash#hash_begin, must be destroyed. The size of the calculated
	 * value must be kept in the parameter digest field zrtp_string#length.
     * \param self - self-pointer for fast access to structure data;
     * \param ctx - hash-context for current hash-value calculation;
     * \param digest - buffer for storing result.
     * \return 
     *	- zrtp_status_ok - if computing finished successfully;
     * 	- one of \ref zrtp_status_t errors - if error.
     */
    zrtp_status_t	(*hash_end)( zrtp_hash_t *self,
								 void *ctx,
								 zrtp_stringn_t *digest );
	
    /*!
     * \brief Calculate hash-value for current message
     * This function implicitly calls the previous 3 functions. The only
	 * difference is that initial data for hash value construction is gathered 
     * in a single buffer and is passed to the function in the \c msg argument.
     * The calculated value size must be stored in the digest zrtp_string#length 
     * parameter
     * \param self - self-pointer for fast access to structure data;
     * \param msg - source data buffer for hash computing;
     * \param digest - buffer for storing result.
     * \return 
     *	- zrtp_status_ok - if computing finished successfully;
     * 	- one of \ref zrtp_status_t errors - if error.
     */
    zrtp_status_t	(*hash)( zrtp_hash_t *self,
							 const zrtp_stringn_t *msg,
							 zrtp_stringn_t *digest );

	/*! \brief Analogue of zrtp_hash::hash for C-string */
	zrtp_status_t	(*hash_c)( zrtp_hash_t *self,
							   const char* msg, 
							   uint32_t	 msg_len,
							   zrtp_stringn_t *digest );

	/*!
	 * \brief HASH self-test.
	 * This function implements hmac self-tests using pre-defined test vectors.
	 * \param self - self-pointer for fast access to structure data;	 
	 * \return
	 *	- zrtp_status_ok - if tests have been passed successfully;
	 *	- one of \ref zrtp_status_t errors - if one or more tests have
	 *	  failed.
	 */	
	zrtp_status_t	(*hash_self_test)(zrtp_hash_t *self);


    /*!
     * \brief Begin HMAC computation with update support.
     * The zrtp_hash#hmac_begin, zrtp_hash#hmac_update and zrtp_hash#hmac_end
     * functions implement the HMAC calculation scheme with accumulation.  The
     * function performs all actions required before beginning the calculation 
     * and allocates a hash-context to store intermediate values. The allocated
     * hash-context will be passed to successive hash_update and hash_end calls
     * \param self - self-pointer for fast access to structure data;
     * \param key - secret key for hmac-value protection.
     * \return
     * 	- pointer to allocated hmac-context if successful;
     * 	- NULL - if error.
     */
    void*			(*hmac_begin)(zrtp_hash_t *self, const zrtp_stringn_t *key);
	
	/*! \brief Analogue of zrtp_hash::hmac_begin for C-string */
	void*			(*hmac_begin_c)(zrtp_hash_t *self, const char *key, uint32_t length);
	
    /*!
     * \brief Process more input data for HMAC calculation
     * This function is called to transfer additional data to the HMAC hash-
	 * calculation. Processes new data and recalculates intermediate values.
     * \param self - self-pointer for fast access to structure data;
     * \param ctx - hmac-context for current hmac-value calculation;
     * \param msg - additional source data for processing;
     * \param length - additional data length in bytes.
     * \return
     *	- zrtp_status_ok - if successfully processed;
     * 	- one of \ref zrtp_status_t errors - if error.
     */
    zrtp_status_t	(*hmac_update)( zrtp_hash_t *self,
									void *ctx,
									const char *msg,
									uint32_t length );
	
    /*! 
     * \brief Complete current HMAC-value computation
     * This function completes the hmac calculation. After the final iteration
     * \a the hash_context allocated by zrtp_hash#hmac_begin is destroyed. The
     * argument \c len holds the HMAC size. If the buffer contains more than \c
     * length characters then only the first \c length are copied to \c digest.
     * The calculated value size is stored in the digest parameter length.
     * \param self - self-pointer for fast access to structure data;
     * \param ctx - hmac-context for current hmac-value calculation;
     * \param digest - buffer for storing result;
     * \param len - required hmac-value size.
     * \return
     *	- zrtp_status_ok - if computing finished successfully;
     * 	- one of \ref zrtp_status_t errors - if error.
     */
    zrtp_status_t	(*hmac_end)( zrtp_hash_t *self,
								 void *ctx,
								 zrtp_stringn_t *digest,
								 uint32_t len);
	
    /*!
     * \brief Calculate hmac-value for current message
     * The function implicitly calls the previous 3 functions 
     * (zrtp_hash#hmac_begin, zrtp_hash#hmac_update and zrtp_hash#hmac_end). The
     * difference is that the initial data for hash value construction is
     * gathered in a single buffer and is passed to the function in the \a msg
     * argument.  The calculated value size must be stored in the \a digest
     * zrtp_string#length  parameter
     * \param self - self-pointer for fast access to structure data;
     * \param key - key for protecting hmac;
     * \param msg - source data buffer for hash computing;
     * \param digest - buffer for storing result.
     * \return 
     *	- zrtp_status_ok - if computing finished successfully;
     * 	- one of \ref zrtp_status_t errors - if error.
     */
    zrtp_status_t	(*hmac)( zrtp_hash_t *self,
							 const zrtp_stringn_t *key,
							 const zrtp_stringn_t *msg,
							 zrtp_stringn_t *digest );
	
	/*! \brief Analogue of zrtp_hash::hmac for C-string */
	zrtp_status_t	(*hmac_c)( zrtp_hash_t *self,
							   const char *key,
							   const uint32_t key_len,
							   const char *msg,
							   const uint32_t msg_len,
							   zrtp_stringn_t *digest );

    /*!
     * \brief Truncated Hmac-calculation version
     * This function acts just like the previous \a hmac except it returns the
     * first \a length bytes of the calculated value in the digest.
     * \param self - self-pointer for fast access to structure data;
     * \param key - key for hmac protection;
     * \param msg - source data buffer for hash computing;
     * \param digest - buffer for storing result;
     * \param len - required hmac-value size.
     * \return
     *	- zrtp_status_ok - if computed successfully;
     * 	- one of \ref zrtp_status_t errors - if error.
     */
    zrtp_status_t	(*hmac_truncated)( zrtp_hash_t *self,
									   const zrtp_stringn_t *key,
									   const zrtp_stringn_t *msg,
									   uint32_t len,
									   zrtp_stringn_t *digest );
	
	/*! \brief Analogue of zrtp_hash::hmac_truncated for C-string */
	zrtp_status_t	(*hmac_truncated_c)( zrtp_hash_t *self,
									     const char *key,
										 const uint32_t key_len,
										 const char *msg,
										 const uint32_t msg_len,
										 uint32_t necessary_len,
										 zrtp_stringn_t *digest );
	
	/*!
	 * \brief HMAC self-test.
	 * This function implements the hmac self-tests using pre-defined test vectors.
	 * \param self - self-pointer for fast access to structure data;	.
	 * \return
	 *	- zrtp_status_ok - if tests have passed successfully;
     *	- one of \ref zrtp_status_t errors - if one or more tests have failed.
	 */	
	zrtp_status_t	(*hmac_self_test)( zrtp_hash_t *self);
	
	uint32_t		digest_length;
	uint32_t		block_length;
	mlist_t	mlist;
};

 
/*!
 * \brief Structure for defining the SRTP authentication scheme 
 * The ZRTP context field zrtp_stream#_authtaglength is initialized by the
 * given type value and used for SRTP encryption configuration.
 */
struct zrtp_auth_tag_length_t
{    
    zrtp_comp_t	  base;
    uint32_t	  tag_length;
    mlist_t		  mlist;
};


/**
 * @brief Structure for describing the public key scheme 
 * The ZRTP context field zrtp_stream#_pubkeyscheme is initialized by the given
 * type value and used by libzrtp in public key exchange.
 */
struct zrtp_pk_scheme_t
{    
	zrtp_comp_t		base;

    /** Generate Diffie-Hellman secret value and Calculate public value */
    zrtp_status_t	(*initialize)( zrtp_pk_scheme_t *self,
								   zrtp_dh_crypto_context_t *dh_cc );
	
    /** Calculate Diffie-Hellman result (ZRTP Internet Draft) */
    zrtp_status_t	(*compute)( zrtp_pk_scheme_t *self,
								zrtp_dh_crypto_context_t *dh_cc,
								struct BigNum *dhresult,
								struct BigNum *pv);
	
    /** Validate Diffie-Hellman public value */
    zrtp_status_t	(*validate)(zrtp_pk_scheme_t *self, struct BigNum *pv);
	
	/** Diffie-Hellman self-test routine. */
	zrtp_status_t	(*self_test)(zrtp_pk_scheme_t *self);
        
	/** Diffie-Hellman secret value size in bytes */
    uint32_t		sv_length;
	
	/** Diffie-Hellman public value size in bytes */
    uint32_t		pv_length;
        
    mlist_t			mlist;
};


/*!
 * \brief Structure for defining SAS generation scheme 
 * The type of the ZRTP context's field zrtp_stream#_sasscheme. It is used
 * to generate short authentication strings. LibZRTP functionality can be augmented
 * with a new SAS scheme by supplying your own instance of zrtp_sas_scheme.
 */
struct zrtp_sas_scheme_t
{    
	zrtp_comp_t		base;	

    /*!
     * \brief Generate short authentication strings
     * This function computes SAS values according to the specified scheme. It
     * can use base32 or base256 algorithms. It stores the generated SAS values
     * as a zrtp_sas_values_t structure (string and binary representation).
     * \param self - self-pointer for fast access to structure data;
     * \param session - ZRTP session context for additional data;
	 * \param hash - hmac component to be used for SAS calculation;
	 * \param is_transferred - if this flag is equal to 1 new SAS value should
	 *    not be computed. It is already in sas->bin buffer and rendering only
	 *    is required.
     * \return
     *	- zrtp_status_ok - if generation successful;
     *	- one of zrtp_status_t errors - if generation failed.
     */ 
    zrtp_status_t	(*compute)( zrtp_sas_scheme_t *self,
								zrtp_stream_t *stream,
								zrtp_hash_t *hash,								
								uint8_t is_transferred );
	
	mlist_t	mlist;
};


#include "aes.h"

/*! Defines block cipher modes. */
typedef enum zrtp_cipher_mode_values_t
{
	ZRTP_CIPHER_MODE_CTR = 1,
	ZRTP_CIPHER_MODE_CFB = 2
} zrtp_cipher_mode_values_t;

typedef struct zrtp_cipher_mode_t
{
	uint8_t	mode;
} zrtp_cipher_mode_t;


/* \brief Structure for cipher definition */
struct zrtp_cipher_t
{
	zrtp_comp_t		base;

	/*!
	 * \brief Start cipher. 
	 * This function performs all actions necessary to allocate the cipher context
	 * for holding intermediate results and other required information. The allocated
	 * context should be related to the given key. It will be passed to the
	 * zrtp_cipher#set_iv, zrtp_cipher#encrypt and zrtp_cipher#decrypt functions.
	 * \param self - self-pointer for fast access to structure data;
	 * \param key - cipher key;
	 * \param extra_data - additional data necessary for cipher initialization;
	 * \param mode - cipher mode (one of \ref zrtp_cipher_mode_values_t values).
     * \return
     *	- pointer to allocated cipher context;
     *	- NULL if error.
	*/	
	void*			(*start)( zrtp_cipher_t *self,
							  void *key,
							  void *extra_data, uint8_t mode );
	
	/*!
	 * \brief Set Initialization Vector.
	 * Function resets the previous state of the cipher context and sets the new IV.
	 * \param self - self-pointer for fast access to structure data;
	 * \param cipher_ctx - cipher context for current key value;
	 * \param iv - new initialization vector value.
	 * \return
	 *	- zrtp_status_ok - if vector has been set successfully;
     *	- one of \ref zrtp_status_t errors - if operation failed.
	*/
	zrtp_status_t	(*set_iv)( zrtp_cipher_t *self,
							   void *cipher_ctx,
							   zrtp_v128_t *iv );
	
	/*!
	 * \brief Encrypt data.
	 * Implements the encryption engine.
	 * \param self - self-pointer for fast access to structure data;
	 * \param cipher_ctx - cipher context for current key value;
	 * \param buf - buffer with data for encryption. If successful this
	 *              buffer contains the resulting encrypted text;
	 * \param len - length of plain/encrypted data.
	 * \return
	 *	- zrtp_status_ok - if data has been encrypted successfully;
     *	- one of \ref zrtp_status_t errors - if encryption failed.
	*/
	zrtp_status_t	(*encrypt)( zrtp_cipher_t *self,
								void *cipher_ctx,
								unsigned char *buf,
								int len );
	
	/*!
	 * \brief Decrypt data.
	 * Implements the decryption engine.
	 * \param self - self-pointer for fast access to structure data;
	 * \param cipher_ctx - cipher context for current key value;
	 * \param buf - buffer with data for decryption. If successful this buffer
	 *    contains the resulting plain text;
	 * \param len - length of encrypted/plain data.
	 * \return
	 *	- zrtp_status_ok - if data has been decrypted successfully;
     *	- one of \ref zrtp_status_t errors - if decryption failed.
	*/
	zrtp_status_t	(*decrypt)( zrtp_cipher_t *self,
								void *cipher_ctx,
								unsigned char *buf,
								int len );

	/*!
	 * \brief Cipher self-test.
	 * Implements cipher self-tests using pre-defined test vectors.
	 * \param self - self-pointer for fast access to structure data;
	 * \param mode - cipher mode (one of \ref zrtp_cipher_mode_values_t values).
	 * \return
	 *	- zrtp_status_ok - if tests have passed successfully;
     *	- one of \ref zrtp_status_t errors - if one or more tests have failed.
	 */	
	zrtp_status_t	(*self_test)(zrtp_cipher_t *self, uint8_t mode);

	/*!
	 * \brief Destroy cipher context.
	 * Deallocs the cipher context previously allocated by a call to zrtp_cipher#start.
	 * \param self - self-pointer for fast access to structure data;
	 * \param cipher_ctx - cipher context for current key value.
	 * \return
	 *	- zrtp_status_ok - if the context has been deallocated
	 *	                   successfully;
     *	- one of \ref zrtp_status_t errors - if deallocation failed.
	 */
	zrtp_status_t (*stop)(zrtp_cipher_t *self, void* cipher_ctx);	

	mlist_t mlist;
};

#if defined(__cplusplus)
extern "C"
{
#endif


/*============================================================================*/
/* 	  Crypto-components management Private part		      					  */
/*============================================================================*/

	
/*!
 * \brief Destroy components buffer
 * This function clears the list of components of the specified type, destroys
 * all components and releases all allocated resources. It is used on libzrtp
 * down. zrtp_comp_done calls zrtp_comp_t#free() if it isn't NULL.
 * \param zrtp - the ZRTP global context where components are stored;
 * \param type - specifies the component pool type for destroying.
 * \return 
 * 	- zrtp_status_ok - if clearing successful;
 * 	- zrtp_status_fail - if error.
 */
zrtp_status_t zrtp_comp_done(zrtp_crypto_comp_t type, zrtp_global_t* zrtp);

/*!
 * \brief Registering a new crypto-component
 * Correctness of values in the necessary structure is the developer's
 * responsibility. zrtp_comp_register calls zrtp_comp_t#init() if it isn't NULL.
 * \param type - type of registred component;
 * \param comp - registered crypto-component;
 * \param zrtp - the ZRTP global context where components are stored.
 * \return
 *	- zrtp_status_ok if registration successful;
 * 	-  zrtp_status_fail if error (conflicts with other components).
 */
zrtp_status_t zrtp_comp_register( zrtp_crypto_comp_t type,
								  void *comp,
								  zrtp_global_t* zrtp);
	
/*!
 * \brief Search component by ID
 * \param type - type of sought component;
 * \param zrtp - the ZRTP global context where components are stored;
 * \param id - integer identifier of the necessary element.
 * \return
 * 	- the found structure if successful;
 * 	- NULL if the element with the specified ID can't be found or
 *        other error.
 */
void* zrtp_comp_find( zrtp_crypto_comp_t type,
					  uint8_t id,
					  zrtp_global_t* zrtp);


/*! Converts a component's integer ID to a symbolic ZRTP name */
char* zrtp_comp_id2type(zrtp_crypto_comp_t type, uint8_t id);

/*! Converts a component's ZRTP symbolic name to an integer ID */
uint8_t zrtp_comp_type2id(zrtp_crypto_comp_t type, char* name);


/*! \} */

#if defined(__cplusplus)
}
#endif

#endif /*__ZRTP_CRYPTO_H__ */

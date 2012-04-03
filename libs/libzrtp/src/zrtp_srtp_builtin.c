/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Vitaly Rozhkov <v.rozhkov at soft-industry.com>
 */

#include "zrtp.h"

#define _ZTU_ "zrtp srtp"

#if (!defined(ZRTP_USE_EXTERN_SRTP)) || (ZRTP_USE_EXTERN_SRTP == 0)


/* constants that are used for packet's parsing */
#define octets_in_rtp_header   12
#define uint32s_in_rtp_header  3
#define octets_in_rtcp_header  8
#define uint32s_in_rtcp_header 2


/*
  defines to make work with cipher component little bit easy
*/
#define zrtp_cipher_init(self)						\
	( ((self)->cipher)->init(((self)->cipher)) )

#define zrtp_cipher_start(self, key, extra_data, mode)					\
	( ((self)->cipher)->start(((self)->cipher),  (key), (extra_data), (mode)) )

#define zrtp_cipher_set_iv(self, iv)									\
	( ((self)->cipher)->set_iv( ((self)->cipher), ((self)->ctx), (iv)) )

#define zrtp_cipher_encrypt(self, buf, len)								\
	( ((self)->cipher)->encrypt( ((self)->cipher), ((self)->ctx), (buf), (len)) )

#define zrtp_cipher_decrypt(self, buf, len)								\
	( ((self)->cipher)->decrypt( ((self)->cipher), ((self)->ctx), (buf), (len)) )

#define zrtp_cipher_self_test(self)						\
	( ((self)->cipher)->self_test(((self)->cipher)) )

#define zrtp_cipher_stop(self)									\
	( ((self)->cipher)->stop(((self)->cipher), ((self)->ctx)) )

#define zrtp_cipher_free(self)						\
	( ((self)->cipher)->free(((self)->cipher)) )




/*===========================================================================*/
/*  Replay protection serve functions set									 */
/*===========================================================================*/


/*! \brief Allocates and initializes replay protection context. Initialize
 * mutexes and linked lists.
 * \return
 * - allocated replay protection context
 * - NULL if error
 */
/*---------------------------------------------------------------------------*/
zrtp_rp_ctx_t* rp_init()
{
	zrtp_rp_ctx_t *ctx = zrtp_sys_alloc(sizeof(zrtp_rp_ctx_t));
	if(NULL == ctx){
		return NULL;
	}

	if(zrtp_status_ok != zrtp_mutex_init(&ctx->inc_sync)){
		zrtp_sys_free(ctx);
		return NULL;
	}

	if(zrtp_status_ok != zrtp_mutex_init(&ctx->out_sync)){
		zrtp_mutex_destroy(ctx->inc_sync);
		zrtp_sys_free(ctx);
		return NULL;
	}

	init_mlist(&ctx->inc_head.mlist);
	init_mlist(&ctx->out_head.mlist);

	return ctx;
}


/*! \brief Deinitializes and deallocates replay protection context.
 *	\param ctx - replay protection context
 *	\return
 *	- zrtp_status_ok
 */
/*---------------------------------------------------------------------------*/
zrtp_status_t rp_destroy(zrtp_rp_ctx_t *ctx)
{
	mlist_t *pos, *n;
	zrtp_rp_node_t *node = NULL;

	/*free all existing replay protection nodes in the incoming list*/
	zrtp_mutex_lock(ctx->inc_sync);
	mlist_for_each_safe(pos, n, &ctx->inc_head.mlist){
		node = mlist_get_struct(zrtp_rp_node_t, mlist, pos);
		mlist_del(&node->mlist);
		zrtp_sys_free(node);
	}
	zrtp_mutex_unlock(ctx->inc_sync);

	zrtp_mutex_destroy(ctx->inc_sync);

	/*free all existing replay protection nodes in the outgoing list*/
	zrtp_mutex_lock(ctx->out_sync);
	mlist_for_each_safe(pos, n, &ctx->out_head.mlist){
		node = mlist_get_struct(zrtp_rp_node_t, mlist, pos);
		mlist_del(&node->mlist);
		zrtp_sys_free(node);
	}
	zrtp_mutex_unlock(ctx->out_sync);

	zrtp_mutex_destroy(ctx->out_sync);

	zrtp_sys_free(ctx);
	return zrtp_status_ok;
}


/*! \brief Finds replay protection node by given ssrc. Which linked list to search is
 * determined by the direction param.
 * \warning This function doesn't lock the linked list before search and is for internal usage.
 * To find necessary replay protection node use get_rp_node() function.
 * \param ctx - pointer to replay protection context
 * \param direction - defines what list to search. It may have values:
 * - RP_INCOMING_DIRECTION
 * - RP_OUTGOING_DIRECTION
 * \return
 * - pointer to found replay protection node
 * - NULL if node hasn't been found or if error
 */
/*---------------------------------------------------------------------------*/
zrtp_rp_node_t *get_rp_node_non_lock( zrtp_rp_ctx_t *ctx,
									  uint8_t direction,
									  uint32_t ssrc)
{
	zrtp_rp_node_t *node = NULL;
	mlist_t *pos;
	mlist_t *head = NULL;

	switch(direction){
	case RP_INCOMING_DIRECTION:
		head = &ctx->inc_head.mlist;
		break;
	case RP_OUTGOING_DIRECTION:
		head = &ctx->out_head.mlist;
		break;
	default:
		head = NULL;
		break;
	};

	if(NULL != head){
		mlist_for_each(pos, head){
			node = mlist_get_struct(zrtp_rp_node_t, mlist, pos);
			if(ssrc == node->ssrc){
				break;
			}else{
				node = NULL;
			}
		}
	}

	return node;
}


///*! \brief Finds replay protection node by given ssrc. Linked list to search is
// *  determined by direction param.  This function locks the linked list to
// *  ensure exclusive access.
// *
// * \param ctx - pointer to replay protection context
// * \param direction - defines what list to search. It may have values:
// * - RP_INCOMING_DIRECTION
// * - RP_OUTGOING_DIRECTION
// * \param ssrc - value by which search will be made
// * \return
// * - pointer to found replay protection node
// * - NULL if node hasn't been found or if error
// */
///*---------------------------------------------------------------------------*/
//zrtp_rp_node_t *get_rp_node(zrtp_rp_ctx_t *ctx, uint8_t direction, uint32_t ssrc)
//{
//	zrtp_rp_node_t *node = NULL;
//	zrtp_mutex_t *sync = NULL;
//
//	switch(direction){
//	case RP_INCOMING_DIRECTION:
//		sync = ctx->inc_sync;
//		break;
//	case RP_OUTGOING_DIRECTION:
//		sync = ctx->out_sync;
//		break;
//	default:
//		sync = NULL;
//		break;
//	};
//
//	if(NULL != sync){
//		zrtp_mutex_lock(sync);
//		node = get_rp_node_non_lock(ctx, direction, ssrc);
//		zrtp_mutex_unlock(sync);
//	}
//
//	return node;
//}

/*! \brief Allocates new replay protection node for given direction and ssrc and adds it into
 * appropriate linked list.
 * \warning This function is for internal usage. Use add_rp_node() and add_rp_node_unique().
 * \param srtp_ctx - pointer to SRTP ctx related with created node. Used for removing node on SRTP session destruction.
 * \param ctx - pointer to replay protection context
 * \param direction - defines in which list newly created node will be inserted. It may have values:
 * - RP_INCOMING_DIRECTION
 * - RP_OUTGOING_DIRECTION
 * \param ssrc - newly created replay protection node key value.
 * \param is_unique - defines what should be returned when replay protection node
 * with given direction and ssrc values already exists:
 * - pointer to existing node if is_unique == 0
 * - NULL if is_unique == 1
 * \return
 * - pointer to newly created replay protection node
 * - pointer to existing replay protection node
 * - NULL if is_unique == 1 and needed replay protection node already exists or if error
 */
/*---------------------------------------------------------------------------*/
zrtp_rp_node_t *add_rp_node_ex( zrtp_srtp_ctx_t *srtp_ctx,
								zrtp_rp_ctx_t *ctx,
							    uint8_t direction,
								uint32_t ssrc,
								uint8_t is_unique)
{
	zrtp_rp_node_t *node = NULL;
	zrtp_mutex_t *sync = NULL;
	mlist_t *head = NULL;

	switch(direction){
	case RP_INCOMING_DIRECTION:
		sync = ctx->inc_sync;
		head = &ctx->inc_head.mlist;
		break;
	case RP_OUTGOING_DIRECTION:
		sync = ctx->out_sync;
		head = &ctx->out_head.mlist;
		break;
	default:
		sync = NULL;
		head = NULL;
		break;
	};

	if(NULL != sync && NULL != head){
		zrtp_mutex_lock(sync);
		do{
			node = get_rp_node_non_lock(ctx, direction, ssrc);

			/*create new node if not found*/
			if(NULL == node){
				node = zrtp_sys_alloc(sizeof(zrtp_rp_node_t));
				if(NULL == node){
					break;
				}
				/*clean sliding window and on-top sequence number value*/
				zrtp_memset(node, 0, sizeof(zrtp_rp_node_t));
				node->ssrc = ssrc;
				node->srtp_ctx = srtp_ctx;
				mlist_add_tail(head, &node->mlist);
#if ZRTP_DEBUG_SRTP_KEYS				
				ZRTP_LOG(3,(_ZTU_,"\tadd %s rp node. ssrc[%u] srtp_ctx[0x%08x]", 
							direction==RP_INCOMING_DIRECTION?"incoming":"outgoing\n",
							zrtp_ntoh32(node->ssrc), node->srtp_ctx));
#endif
			}else if(is_unique){
				// ???: why do we need unique mode at all?
				node = NULL;
			}

		}while(0);
		zrtp_mutex_unlock(sync);
	}

	return node;
}

/*! \brief Allocates new replay protection node for given direction and ssrc and adds it into
 * appropriate linked list. This function is based on add_rp_node_ex().
 * \param srtp_ctx - pointer to SRTP ctx related with created node. Used for removing node on SRTP session destruction.
 * \param ctx - pointer to replay protection context
 * \param direction - defines in which list newly created node will be inserted. It may have values:
 * - RP_INCOMING_DIRECTION
 * - RP_OUTGOING_DIRECTION
 * \param ssrc - newly created replay protection node key value.
 * \return
 * - pointer to newly created replay protection node
 * - pointer to existing replay protection node
 * - NULL if error
 */
zrtp_rp_node_t *add_rp_node(zrtp_srtp_ctx_t *srtp_ctx, zrtp_rp_ctx_t *ctx, uint8_t direction, uint32_t ssrc){
	/*not-unique mode*/	
	// ???: why do we need unique mode at all?
	return add_rp_node_ex(srtp_ctx, ctx, direction, ssrc, 0);
}

///*! \brief Allocates new replay protection node for given direction and ssrc and adds it into
// * appropriate linked list. This function is based on add_rp_node_ex().
// * \param srtp_ctx - pointer to SRTP ctx related with created node. Used for removing node on SRTP session destruction.
// * \param ctx - pointer to replay protection context
// * \param direction - defines in which list newly created node will be inserted. It may have values:
// * - RP_INCOMING_DIRECTION
// * - RP_OUTGOING_DIRECTION
// * \param ssrc - newly created replay protection node key value.
// * \return
// * - pointer to newly created replay protection node
// * - NULL if error or if needed node already exists
// */
//zrtp_rp_node_t *add_rp_node_unique(zrtp_srtp_ctx_t *srtp_ctx, zrtp_rp_ctx_t *ctx, uint8_t direction, uint32_t ssrc){
//	/*unique mode*/
//	return add_rp_node_ex(srtp_ctx, ctx, direction, ssrc, 1);
//}

/*! \brief Removes replay protection node with given ssrc from linked list defined by direction value.
 * \param ctx - pointer to replay protection context
 * \param direction - defines from which list replay protection node will be removed. It may have values:
 * - RP_INCOMING_DIRECTION
 * - RP_OUTGOING_DIRECTION
 * \param ssrc - key value of replay protection node to remove
 * \return
 * - zrtp_status_ok if replay protection node has been removed successfully
 * - zrtp_status_fail if node hasn't been found
 */
/*---------------------------------------------------------------------------*/
zrtp_status_t remove_rp_node(zrtp_rp_ctx_t *ctx, uint8_t direction, uint32_t ssrc){
	zrtp_rp_node_t *node = NULL;
	zrtp_mutex_t *sync = NULL;
	zrtp_status_t res = zrtp_status_fail;

	switch(direction){
	case RP_INCOMING_DIRECTION:
		sync = ctx->inc_sync;
		break;
	case RP_OUTGOING_DIRECTION:
		sync = ctx->out_sync;
		break;
	default:
		sync = NULL;
		break;
	};

	if(NULL != sync){
		zrtp_mutex_lock(sync);
		node = get_rp_node_non_lock(ctx, direction, ssrc);
		if(NULL != node){
			mlist_del(&node->mlist);
			zrtp_sys_free(node);
			res = zrtp_status_ok;
		}
		zrtp_mutex_unlock(sync);
	}

	return res;
}


zrtp_status_t remove_rp_nodes_by_srtp_ctx(zrtp_srtp_ctx_t *srtp_ctx, zrtp_rp_ctx_t *ctx){
	zrtp_status_t res = zrtp_status_ok;
	zrtp_rp_node_t *node = NULL;
	mlist_t *pos, *n;

	if((NULL == srtp_ctx) || (NULL == ctx)){
		return zrtp_status_bad_param;
	}

	/* Walk over incoming nodes list */
	zrtp_mutex_lock(ctx->inc_sync);
	mlist_for_each_safe(pos, n, &ctx->inc_head.mlist){
		node = mlist_get_struct(zrtp_rp_node_t, mlist, pos);
		if((NULL != node->srtp_ctx) && (node->srtp_ctx == srtp_ctx)){
#if ZRTP_DEBUG_SRTP_KEYS
			ZRTP_LOG(3,(_ZTU_,"\tremove incoming rp node. ssrc[%u] srtp_ctx[0x%08x]\n",
						zrtp_ntoh32(node->ssrc), node->srtp_ctx));
#endif
			mlist_del(&node->mlist);
			zrtp_sys_free(node);
		}
	}
	zrtp_mutex_unlock(ctx->inc_sync);

	/* Walk over outgoing nodes list */
	zrtp_mutex_lock(ctx->out_sync);
	mlist_for_each_safe(pos, n, &ctx->out_head.mlist){
		node = mlist_get_struct(zrtp_rp_node_t, mlist, pos);
		if((NULL != node->srtp_ctx) && (node->srtp_ctx == srtp_ctx)){
#if ZRTP_DEBUG_SRTP_KEYS
			ZRTP_LOG(3,(_ZTU_,"\tremove outgoing rp node. ssrc[%u] srtp_ctx[0x%08x]\n",
						zrtp_ntoh32(node->ssrc), node->srtp_ctx));
#endif
			mlist_del(&node->mlist);
			zrtp_sys_free(node);
		}
	}
	zrtp_mutex_unlock(ctx->out_sync);

	return res;
}


/*===========================================================================*/
/*  Replay protection mechanism functions set								 */
/*===========================================================================*/


/*! \brief This function is used for RTCP replay protection to generate next sequence number
 * of outgoing RTCP packet. If the sequence number is too large it returns zrtp_status_key_expired.
 * See RFC3711 for more details.
 * \param srtp_rp - pointer to replay protection engine data
 * \return
 * - zrtp_status_key_expired if next sequence number is too large
 * - zrtp_status_ok otherwise
 */
zrtp_status_t zrtp_srtp_rp_increment(zrtp_srtp_rp_t *srtp_rp){

	if(srtp_rp->seq++ > 0x7fffffff){
		return zrtp_status_key_expired;
	}else{
		return zrtp_status_ok;
	}
}

/*! \brief Returns current on-top sequence number. This function is used for RTCP
 * replay protection.
 * \param srtp_rp - pointer to replay protection engine data
 * \return current on-top sequence number
 */
uint32_t zrtp_srtp_rp_get_value(zrtp_srtp_rp_t *srtp_rp){
	return srtp_rp->seq;
}


/*! \brief This function checks packet sequence number position relative to
 * sliding window current position and makes the decision to accept or discard packet.
 * \param srtp_rp - pointer to replay protection engine data
 * \param packet - pointer to packet structure
 * \return
 * - zrtp_status_ok if packet must be accepted
 * - zrtp_status_old_pkt if packet sequence number is lower than lowest sequence number
 * which can be into the sliding window at the current time. In this case packet must be discarded.
 * - zrtp_status_fail if packet must be discarded
 */
/*---------------------------------------------------------------------------*/
zrtp_status_t zrtp_srtp_rp_check(zrtp_srtp_rp_t *srtp_rp, zrtp_rtp_info_t *packet)
{
	int32_t delta = packet->seq - srtp_rp->seq;
	if(delta > 0){
		/*if delta is positive, it's good*/
		return zrtp_status_ok;
	}else if(ZRTP_SRTP_WINDOW_WIDTH-1 + delta < 0){
		/*if delta is lower than the bitmask, it's bad*/
		return zrtp_status_old_pkt;
	}else{
		if(1 == zrtp_bitmap_get_bit(srtp_rp->window, ZRTP_SRTP_WINDOW_WIDTH-1 + delta)){
			/*delta is within the window, so check the bitmask*/
			return zrtp_status_fail;
		}
	}
	return zrtp_status_ok;
}

/*! \brief This function updates the sliding window state by setting appropriate bit and
 * shifting the sliding window if needed.
 * \param srtp_rp - pointer to replay protection engine data
 * \param packet - pointer to packet structure
 * \return
 * - zrtp_status_ok
 */
/*---------------------------------------------------------------------------*/
zrtp_status_t zrtp_srtp_rp_add(zrtp_srtp_rp_t *srtp_rp, zrtp_rtp_info_t *packet)
{
	int32_t delta = packet->seq - srtp_rp->seq;
	if(delta > 0){
		/*	packet sequence nubmer is larger than current on-top sequence number.
			shift the window, set top bit and update on-top sequence number value */
		srtp_rp->seq = packet->seq;
		zrtp_bitmap_left_shift(srtp_rp->window, ZRTP_SRTP_WINDOW_WIDTH_BYTES, delta);
		zrtp_bitmap_set_bit(srtp_rp->window, ZRTP_SRTP_WINDOW_WIDTH-1);
	}else

		/*	commented by book, 19.07.07:
			we need not consider case when delta == 0
			if(0 == delta){
			zrtp_bitmap_set_bit(srtp_rp->window, ZRTP_SRTP_WINDOW_WIDTH-1);
			}else*/

	{
		/*
		  packet sequence number is into the sliding window.
		  set appropriate bit
		*/
		zrtp_bitmap_set_bit(srtp_rp->window, ZRTP_SRTP_WINDOW_WIDTH-1 + delta);
	}

	return zrtp_status_ok;
}


/*===========================================================================*/
/*  Key derivation mechanism functions set									 */
/*===========================================================================*/


/*! \brief This function allocates key derivation context and initializes it with
 * given master key, master salt and cipher.
 * \param cipher - pointer to cipher that is used for key derivation
 * \param key - pointer to master key
 * \param salt - pointer to master salt
 * \return
 * - allocated key derivation context
 * - NULL if error
 */
/*---------------------------------------------------------------------------*/
zrtp_dk_ctx *zrtp_dk_init( zrtp_cipher_t *cipher,
						   zrtp_stringn_t *key,
						   zrtp_stringn_t *salt)
{
	zrtp_dk_ctx *ctx = NULL;
#if ZRTP_DEBUG_SRTP_KEYS
	ZRTP_LOG(3,(_ZTU_,"\tzrtp_dk_init():\n"));
	ZRTP_LOG(3,(_ZTU_,"\tcipher ID[%i]\n", cipher->base.id));
#endif
	do{
		ctx = zrtp_sys_alloc(sizeof(zrtp_dk_ctx));
		if(NULL == ctx){
			break;
		}

		ctx->ctx = cipher->start(cipher, key->buffer, salt->buffer, ZRTP_CIPHER_MODE_CTR);
		if(NULL == ctx->ctx){
			zrtp_sys_free(ctx);
			ctx = NULL;
			break;
		}

		ctx->cipher = cipher;
	}while(0);

	return ctx;
}

/*! \brief This function derives key for different purposes like SRTP encryption,
 *	SRTP message authentication, etc. See RFC3711, "4.3.  Key Derivation" for more details.
 * \warning This function may change length field value in the result_key variable when
 * length is larger than max_length field value.
 * \param ctx - pointer to key derivation context
 * \param label - defines purpose of key to derive
 * \param result_key - out parameter. It contains derived key on success.
 * \return
 * - actually derived key length
 * - -1 if error
 */
/*---------------------------------------------------------------------------*/
uint16_t zrtp_derive_key( zrtp_dk_ctx *ctx,
						  zrtp_srtp_prf_label label,
						  zrtp_stringn_t *result_key )
{
	zrtp_v128_t nonce;
	uint16_t length;
#if ZRTP_DEBUG_SRTP_KEYS
	char buffer[256];
	ZRTP_LOG(3,(_ZTU_,"\tzrtp_derive_key():\n"));
#endif

	/* set eigth octet of nonce to <label>, set the rest of it to zero */
	zrtp_memset(&nonce, 0, sizeof(zrtp_v128_t));
	nonce.v8[7] = label;
#if ZRTP_DEBUG_SRTP_KEYS
	ZRTP_LOG(3,(_ZTU_, "\t\tcipher IV[%s]\n",
				   hex2str((const char*)nonce.v8, sizeof(zrtp_v128_t), (char*)buffer, sizeof(buffer))));
#endif
	zrtp_cipher_set_iv(ctx, &nonce);

	length = (uint16_t) ZRTP_MIN(result_key->length, result_key->max_length);
#if ZRTP_DEBUG_SRTP_KEYS
	ZRTP_LOG(3,(_ZTU_, "\t\texcepced key length[%i] result key length[%i]\n", result_key->length, length));
#endif
	zrtp_memset(result_key->buffer, 0, length);

	if(zrtp_status_ok == zrtp_cipher_encrypt(ctx, (uint8_t*)result_key->buffer, length)){
		result_key->length = length;
		return length;
	}else{
		return -1;
	}
}


/*! \brief This function deallocates key derivation context allocated by \ref zrtp_dk_init() call.
 * \param ctx - pointer to key derivation context to deallocate
 */
void zrtp_dk_deinit(zrtp_dk_ctx *ctx)
{
	zrtp_cipher_stop(ctx);
	zrtp_memset(ctx, 0, sizeof(zrtp_dk_ctx));
	zrtp_sys_free(ctx);
}


/*! \brief This function allocates SRTP session and two stream contexts.
 * \return
 * - pointer to allocated SRTP session structure
 * - NULL if error
 */
/*---------------------------------------------------------------------------*/
zrtp_srtp_ctx_t * zrtp_srtp_alloc()
{
	zrtp_srtp_ctx_t *srtp_ctx = NULL;

	do{
		srtp_ctx = zrtp_sys_alloc(sizeof(zrtp_srtp_ctx_t));
		if(NULL == srtp_ctx){
			break;
		}

		srtp_ctx->incoming_srtp = zrtp_sys_alloc(sizeof(zrtp_srtp_stream_ctx_t));
		if(NULL == srtp_ctx->incoming_srtp){
			/*deallocate everything previously allocated on failure*/
			zrtp_sys_free(srtp_ctx);
			srtp_ctx = NULL;
			break;
		}

		srtp_ctx->outgoing_srtp = zrtp_sys_alloc(sizeof(zrtp_srtp_stream_ctx_t));
		if(NULL == srtp_ctx->outgoing_srtp){
			/*deallocate everything previously allocated on failure*/
			zrtp_sys_free(srtp_ctx->incoming_srtp);
			zrtp_sys_free(srtp_ctx);
			srtp_ctx = NULL;
			break;
		}

	}while(0);

	return srtp_ctx;
}

/*! \brief This function deallocates SRTP session structure allocated by zrtp_srtp_alloc() call.
 * \param srtp_ctx - pointer to SRTP session structure.
 */
void zrtp_srtp_free(zrtp_srtp_ctx_t * srtp_ctx)
{
	if (srtp_ctx)
	{
		if (srtp_ctx->incoming_srtp)
			zrtp_sys_free(srtp_ctx->incoming_srtp);
		if (srtp_ctx->outgoing_srtp)
			zrtp_sys_free(srtp_ctx->outgoing_srtp);
		zrtp_sys_free(srtp_ctx);
	}
}

/*! \brief This function initializes stream context based on given profile.
 * \param srtp_global - pointer to SRTP engine global context
 * \param srtp_stream - pointer to stream context to initialize
 * \param profile - pointer to profile for stream initialization
 * \return
 * - zrtp_status_ok if stream has been initialized successfully
 * - one of \ref zrtp_status_t errors - if error
 */
/*---------------------------------------------------------------------------*/
zrtp_status_t zrtp_srtp_stream_init( zrtp_srtp_global_t *srtp_global,
									 zrtp_srtp_stream_ctx_t *srtp_stream,
									 zrtp_srtp_profile_t *profile )
{
#if ZRTP_DEBUG_SRTP_KEYS
	char buffer[256];
#endif
	zrtp_status_t res = zrtp_status_ok;

	/*
	  TODO: use dynamic buffers for temoprary keys storing

	  NOTE!: be sure that tmp_key contains enought buffer length to store all
	  of derived keys. Authentication keys may be large.
	*/
	zrtp_string128_t tmp_key = ZSTR_INIT_EMPTY(tmp_key);
	/*salt length is 16 bytes always*/
	zrtp_string16_t	tmp_salt = ZSTR_INIT_EMPTY(tmp_salt);

	do{
		zrtp_dk_ctx *dk_ctx = NULL;
#if ZRTP_DEBUG_SRTP_KEYS
		ZRTP_LOG(3,(_ZTU_, "\tzrtp_srtp_stream_init():\n"));
#endif
		if(NULL == srtp_stream || NULL == profile){
			res = zrtp_status_bad_param;
			break;
		}

		dk_ctx = zrtp_dk_init( profile->dk_cipher,
							   (zrtp_stringn_t*)&profile->key,
							   (zrtp_stringn_t*)&profile->salt );
		if(NULL == dk_ctx)
		{
			res = zrtp_status_fail;
			break;
		}
#if ZRTP_DEBUG_SRTP_KEYS
		ZRTP_LOG(3,(_ZTU_, "\t\tmaster_key[%s]\n",
					   hex2str(profile->key.buffer, profile->key.length, buffer, sizeof(buffer))));
		ZRTP_LOG(3,(_ZTU_, "\t\tmaster_salt[%s]\n",
					   hex2str(profile->salt.buffer, profile->salt.length, buffer, sizeof(buffer))));
#endif

		/*------------ init RTP-items ----------------*/
		srtp_stream->rtp_cipher.cipher = profile->rtp_policy.cipher;

		tmp_key.length = (uint16_t) profile->rtp_policy.cipher_key_len;
		tmp_salt.length = profile->salt.length;


		zrtp_derive_key(dk_ctx, label_rtp_encryption, (zrtp_stringn_t*)&tmp_key);
#if ZRTP_DEBUG_SRTP_KEYS
		ZRTP_LOG(3,(_ZTU_, "\t\tderive RTP encryption key[%s] label:%i\n",
					hex2str(tmp_key.buffer, tmp_key.length, buffer, sizeof(buffer)), label_rtp_encryption));

#endif
		zrtp_derive_key(dk_ctx, label_rtp_salt, (zrtp_stringn_t*)&tmp_salt);
#if ZRTP_DEBUG_SRTP_KEYS
		ZRTP_LOG(3,(_ZTU_, "\t\tderive RTP encryption salt[%s] label:%i\n",
					hex2str(tmp_salt.buffer, tmp_salt.length, buffer, sizeof(buffer)), label_rtp_salt));
#endif
		srtp_stream->rtp_cipher.ctx = zrtp_cipher_start(&srtp_stream->rtp_cipher,
														tmp_key.buffer,
														tmp_salt.buffer,
														ZRTP_CIPHER_MODE_CTR );
		if(NULL == srtp_stream->rtp_cipher.ctx){
			zrtp_dk_deinit(dk_ctx);
			res = zrtp_status_fail;
			break;
		}

		srtp_stream->rtp_auth.hash = profile->rtp_policy.hash;
		srtp_stream->rtp_auth.key_len = profile->rtp_policy.auth_key_len;
		srtp_stream->rtp_auth.tag_len = profile->rtp_policy.auth_tag_len;

		srtp_stream->rtp_auth.key = zrtp_sys_alloc(srtp_stream->rtp_auth.key_len);
		if(NULL == srtp_stream->rtp_auth.key){
			zrtp_dk_deinit(dk_ctx);
			zrtp_cipher_stop(&srtp_stream->rtp_cipher);
			res = zrtp_status_fail;
			break;
		}

		tmp_key.length = (uint16_t)srtp_stream->rtp_auth.key_len;
		zrtp_derive_key(dk_ctx, label_rtp_msg_auth, (zrtp_stringn_t*)&tmp_key);
		zrtp_memcpy(srtp_stream->rtp_auth.key, tmp_key.buffer, tmp_key.length);
#if ZRTP_DEBUG_SRTP_KEYS
		ZRTP_LOG(3,(_ZTU_, "\t\tderive RTP auth key[%s]\n",
					hex2str(tmp_key.buffer, tmp_key.length, buffer, sizeof(buffer))));
#endif
		/*--------- init RTCP-items ----------------*/
		srtp_stream->rtcp_cipher.cipher = profile->rtcp_policy.cipher;
		tmp_key.length = (uint16_t) profile->rtcp_policy.cipher_key_len;

		tmp_salt.length = profile->salt.length;
		zrtp_derive_key(dk_ctx, label_rtcp_encryption, (zrtp_stringn_t*)&tmp_key);
		zrtp_derive_key(dk_ctx, label_rtcp_salt, (zrtp_stringn_t*)&tmp_salt);

#if ZRTP_DEBUG_SRTP_KEYS
		ZRTP_LOG(3,(_ZTU_, "\t\tderive RTCP encryption key[%s]\n",
					hex2str(tmp_key.buffer, tmp_key.length, buffer, sizeof(buffer))));
		ZRTP_LOG(3,(_ZTU_, "\t\tderive RTCP encryption salt[%s]\n",
					   hex2str(tmp_salt.buffer, tmp_salt.length, buffer, sizeof(buffer))));
#endif
		srtp_stream->rtcp_cipher.ctx = zrtp_cipher_start(&srtp_stream->rtcp_cipher,
														 tmp_key.buffer,
														 tmp_salt.buffer,
														 ZRTP_CIPHER_MODE_CTR );

		if(NULL == srtp_stream->rtcp_cipher.ctx){
			zrtp_dk_deinit(dk_ctx);
			zrtp_cipher_stop(&srtp_stream->rtp_cipher);
			zrtp_sys_free(srtp_stream->rtp_auth.key);
			res = zrtp_status_fail;
			break;
		}

		srtp_stream->rtcp_auth.hash = profile->rtcp_policy.hash;
		srtp_stream->rtcp_auth.key_len = profile->rtcp_policy.auth_key_len;
		srtp_stream->rtcp_auth.tag_len = profile->rtcp_policy.auth_tag_len;

		srtp_stream->rtcp_auth.key = zrtp_sys_alloc(srtp_stream->rtcp_auth.key_len);
		if(NULL == srtp_stream->rtcp_auth.key){
			zrtp_dk_deinit(dk_ctx);
			zrtp_cipher_stop(&srtp_stream->rtp_cipher);
			zrtp_sys_free(srtp_stream->rtp_auth.key);
			zrtp_cipher_stop(&srtp_stream->rtcp_cipher);
			res = zrtp_status_fail;
			break;
		}

		tmp_key.length = (uint16_t)srtp_stream->rtcp_auth.key_len;
		zrtp_derive_key(dk_ctx, label_rtcp_msg_auth, (zrtp_stringn_t*)&tmp_key);
#if ZRTP_DEBUG_SRTP_KEYS
		ZRTP_LOG(3,(_ZTU_, "\t\tderive RTCP auth key[%s]\n",
				   hex2str(tmp_key.buffer, tmp_key.length, buffer, sizeof(buffer))));
#endif

		zrtp_memcpy(srtp_stream->rtcp_auth.key, tmp_key.buffer, tmp_key.length);
		zrtp_dk_deinit(dk_ctx);

		zrtp_wipe_zstring(ZSTR_GV(tmp_key));
		zrtp_wipe_zstring(ZSTR_GV(tmp_salt));

	}while(0);
	return res;
}


/*! \brief This function deinitializes stream context.
 * \param srtp_global - pointer to SRTP engine global context
 * \param srtp_stream - pointer to steam to deinitialize
 */
/*---------------------------------------------------------------------------*/
void zrtp_srtp_stream_deinit( zrtp_srtp_global_t *srtp_global,
							  zrtp_srtp_stream_ctx_t *srtp_stream )
{
	zrtp_cipher_stop(&srtp_stream->rtp_cipher);
	zrtp_memset(srtp_stream->rtp_auth.key, 0, srtp_stream->rtp_auth.key_len);
	zrtp_sys_free(srtp_stream->rtp_auth.key);

	zrtp_cipher_stop(&srtp_stream->rtcp_cipher);
	zrtp_memset(srtp_stream->rtcp_auth.key, 0, srtp_stream->rtcp_auth.key_len);
	zrtp_sys_free(srtp_stream->rtcp_auth.key);
}


/*! \brief This function initializes SRTP session context.
 * \param srtp_global - pointer to SRTP engine global context
 * \param srtp_ctx - pointer to SRTP session context to initialize
 * \param inc_profile - profile for incoming stream configuration;
 * \param out_profile - profile for outgoing stream configuration.
 * \return
 * - zrtp_status_ok if stream has been initialized successfully
 * - one of \ref zrtp_status_t errors - if error
 */
/*---------------------------------------------------------------------------*/
zrtp_status_t zrtp_srtp_init_ctx(	zrtp_srtp_global_t *srtp_global,
									zrtp_srtp_ctx_t *srtp_ctx,
									zrtp_srtp_profile_t *inc_profile,
									zrtp_srtp_profile_t *out_profile)
{
	zrtp_status_t res = zrtp_status_ok;
	do{
		if(NULL == srtp_ctx || NULL == inc_profile || NULL == out_profile){
			res = zrtp_status_bad_param;
			break;
		}

		if(zrtp_status_ok != zrtp_srtp_stream_init(srtp_global, srtp_ctx->incoming_srtp, inc_profile)){
			res = zrtp_status_fail;
			break;
		}

		if(zrtp_status_ok != zrtp_srtp_stream_init(srtp_global, srtp_ctx->outgoing_srtp, out_profile)){
			zrtp_srtp_stream_deinit(srtp_global, srtp_ctx->incoming_srtp);
			res = zrtp_status_fail;
			break;
		}

	}while(0);
	return res;
}


/*===========================================================================*/
/*  Public interface														 */
/*===========================================================================*/


/*---------------------------------------------------------------------------*/
zrtp_status_t zrtp_srtp_init(zrtp_global_t *zrtp){

	zrtp_srtp_global_t *srtp_global;
	zrtp->srtp_global = NULL;

	if(EXIT_SUCCESS != zrtp_bg_gen_tabs())
		return zrtp_status_fail;

	srtp_global = zrtp_sys_alloc(sizeof(zrtp_srtp_global_t));
	if(NULL == srtp_global){
		return zrtp_status_fail;
	}
	srtp_global->rp_ctx = rp_init();
	if(NULL == srtp_global->rp_ctx){
		zrtp_sys_free(srtp_global);
		return zrtp_status_fail;
	}

	zrtp->srtp_global = srtp_global;

	return zrtp_status_ok;
}

zrtp_status_t zrtp_srtp_down(zrtp_global_t *zrtp){
	zrtp_srtp_global_t *srtp_global = zrtp->srtp_global;

	rp_destroy(srtp_global->rp_ctx);
	zrtp_sys_free(srtp_global);
	zrtp->srtp_global = NULL;
	return zrtp_status_ok;
}

zrtp_srtp_ctx_t * zrtp_srtp_create(	zrtp_srtp_global_t *srtp_global,
									zrtp_srtp_profile_t *inc_profile,
									zrtp_srtp_profile_t *out_profile)
{
	zrtp_srtp_ctx_t *srtp_ctx = NULL;
	if(NULL == inc_profile || NULL == out_profile){
		return NULL;
	}

	do{
		srtp_ctx = zrtp_srtp_alloc();
		if(NULL == srtp_ctx){
			break;
		}

		if(zrtp_status_ok != zrtp_srtp_init_ctx(srtp_global, srtp_ctx, inc_profile, out_profile)){
			zrtp_srtp_free(srtp_ctx);
			srtp_ctx = NULL;
			break;
		}

	}while(0);

	return srtp_ctx;
}

zrtp_status_t zrtp_srtp_destroy(zrtp_srtp_global_t *srtp_global, zrtp_srtp_ctx_t * srtp_ctx){
	zrtp_status_t res = zrtp_status_ok;

	remove_rp_nodes_by_srtp_ctx(srtp_ctx, srtp_global->rp_ctx);

	zrtp_srtp_stream_deinit(srtp_global, srtp_ctx->incoming_srtp);
	zrtp_srtp_stream_deinit(srtp_global, srtp_ctx->outgoing_srtp);
	zrtp_srtp_free(srtp_ctx);

	return res;
}


/*---------------------------------------------------------------------------*/
zrtp_status_t zrtp_srtp_protect(	zrtp_srtp_global_t *srtp_global,
									zrtp_srtp_ctx_t *srtp_ctx,
									zrtp_rtp_info_t *packet)
{
	zrtp_srtp_stream_ctx_t *srtp_stream_ctx = srtp_ctx->outgoing_srtp;
	zrtp_rp_node_t *rp_node;

	uint32_t *enc_start;        /* pointer to start of encrypted portion  */
	uint32_t *auth_start;       /* pointer to start of auth. portion      */
	unsigned enc_octet_len = 0; /* number of octets in encrypted portion  */
	uint8_t *auth_tag = NULL;   /* location of auth_tag within packet     */
	zrtp_status_t status;
	ZRTP_UNALIGNED(zrtp_rtp_hdr_t) *hdr;

	zrtp_v128_t iv;
	uint64_t packet_seq = 0;
	zrtp_string64_t	auth_tag_str = ZSTR_INIT_EMPTY(auth_tag_str);
	void *hash_ctx = NULL;

	/* add new replay protection node or get existing one */
	rp_node = add_rp_node(srtp_ctx, srtp_global->rp_ctx, RP_OUTGOING_DIRECTION, packet->ssrc);
	if(NULL == rp_node){
		return zrtp_status_rp_fail;
	}

	/* check the packet length - it must at least contain a full header */
	if (*(packet->length) < octets_in_rtp_header){
		return zrtp_status_bad_param;
	}

	hdr = (zrtp_rtp_hdr_t*)(packet->packet);
	enc_start = (uint32_t *)hdr + uint32s_in_rtp_header + hdr->cc;
	if (1 == hdr->x) {
		zrtp_rtp_hdr_xtnd_t *xtn_hdr = (zrtp_rtp_hdr_xtnd_t *)enc_start;
		enc_start += (zrtp_ntoh16(xtn_hdr->length) + 1);
	}
	//WIN64
	enc_octet_len = *(packet->length) - (uint32_t)((enc_start - (uint32_t *)hdr) << 2);

	auth_start = (uint32_t *)hdr;
	auth_tag = (uint8_t *)hdr + *(packet->length);

	status = zrtp_srtp_rp_check(&rp_node->rtp_rp, packet);
	if(zrtp_status_ok != status){
		return zrtp_status_rp_fail;
	}
	zrtp_srtp_rp_add(&rp_node->rtp_rp, packet);

	iv.v32[0] = 0;
	iv.v32[1] = hdr->ssrc;

#ifdef ZRTP_NO_64BIT_MATH
	iv.v64[1] = zrtp_hton64(make64((packet->seq) >> 16, (packet->seq) << 16));
#else
	iv.v64[1] = zrtp_hton64(((uint64_t)(packet->seq)) << 16);
#endif
	status = zrtp_cipher_set_iv(&srtp_stream_ctx->rtp_cipher, &iv);
	if(status){
		return zrtp_status_cipher_fail;
	}

	status = zrtp_cipher_encrypt(&srtp_stream_ctx->rtp_cipher, (unsigned char*)enc_start, enc_octet_len);
	if(status){
		return zrtp_status_cipher_fail;
	}


	/* shift est, put into network byte order */
	packet_seq = packet->seq;
#ifdef ZRTP_NO_64BIT_MATH
	packet_seq = zrtp_hton64(make64((high32(packet_seq) << 16) |
									(low32(packet_seq) >> 16),
									low32(packet_seq) << 16));
#else
	packet_seq = zrtp_hton64(packet_seq << 16);
#endif

	hash_ctx = srtp_stream_ctx->rtp_auth.hash->hmac_begin_c( srtp_stream_ctx->rtp_auth.hash,
															 (const char*)srtp_stream_ctx->rtp_auth.key,
															 srtp_stream_ctx->rtp_auth.key_len );
	if(NULL == hash_ctx)
	{
		return zrtp_status_auth_fail;
	}
	status = srtp_stream_ctx->rtp_auth.hash->hmac_update(	srtp_stream_ctx->rtp_auth.hash,
															hash_ctx,
															(const char*)auth_start,
															*packet->length);
	if(status)
	{
		return zrtp_status_auth_fail;
	}
	status = srtp_stream_ctx->rtp_auth.hash->hmac_update(	srtp_stream_ctx->rtp_auth.hash,
															hash_ctx,
															(const char*)&packet_seq,
															4);
	if(status)
	{
		return zrtp_status_auth_fail;
	}
	status = srtp_stream_ctx->rtp_auth.hash->hmac_end(	srtp_stream_ctx->rtp_auth.hash,
														hash_ctx,
														(zrtp_stringn_t*) &auth_tag_str,
														srtp_stream_ctx->rtp_auth.tag_len->tag_length);
	if(status)
	{
		return zrtp_status_auth_fail;
	}

	/* uncomment this for authentication debug */
#if ZRTP_DEBUG_SRTP_KEYS
	{
		char buff[256];
		ZRTP_LOG(3,(_ZTU_,
					"\tzrtp_srtp_protect authentication make: npacket_seq[%s] expected auth length[%i] result auth length[%i]\n",
					  hex2str((char*)&packet_seq, sizeof(packet_seq), buff, sizeof(buff)),
					  srtp_stream_ctx->rtp_auth.tag_len->tag_length,
					  auth_tag_str.length));
		ZRTP_LOG(3,(_ZTU_, "\tauth tag[%s]\n",
					hex2str(auth_tag_str.buffer, auth_tag_str.length, buff, sizeof(buff))));
	}
#endif
	zrtp_memcpy(auth_tag, auth_tag_str.buffer, auth_tag_str.length);
	*packet->length += auth_tag_str.length;

	return status;
}


/*---------------------------------------------------------------------------*/
zrtp_status_t zrtp_srtp_unprotect(	zrtp_srtp_global_t *srtp_global,
									zrtp_srtp_ctx_t *srtp_ctx,
									zrtp_rtp_info_t *packet)
{
	zrtp_srtp_stream_ctx_t *srtp_stream_ctx = srtp_ctx->incoming_srtp;
	zrtp_rp_node_t *rp_node;


	uint32_t *enc_start;        /* pointer to start of encrypted portion  */
	uint32_t *auth_start;       /* pointer to start of auth. portion      */
	unsigned enc_octet_len = 0; /* number of octets in encrypted portion  */
	uint8_t *auth_tag = NULL;   /* location of auth_tag within packet     */
	zrtp_status_t status;
	ZRTP_UNALIGNED(zrtp_rtp_hdr_t)  *hdr = NULL;


	void *hash_ctx = NULL;
	zrtp_v128_t iv;
	int tag_len = 0;

	/*add new replay protection node or get existing one*/
	rp_node = add_rp_node(srtp_ctx, srtp_global->rp_ctx, RP_INCOMING_DIRECTION, packet->ssrc);
	if(NULL == rp_node){
		return zrtp_status_rp_fail;
	}

	/* check the packet length - it must at least contain a full header */
	if (*(packet->length) < octets_in_rtp_header)
	{
		return zrtp_status_bad_param;
	}

	hdr = (zrtp_rtp_hdr_t*)(packet->packet);

	status = zrtp_srtp_rp_check(&rp_node->rtp_rp, packet);
	if(zrtp_status_ok != status){
		return zrtp_status_rp_fail;
	}

	iv.v32[0] = 0;
	iv.v32[1] = hdr->ssrc;

#ifdef ZRTP_NO_64BIT_MATH
	iv.v64[1] = zrtp_hton64(make64((packet->seq) >> 16, (packet->seq) << 16));
#else
	iv.v64[1] = zrtp_hton64((uint64_t)(packet->seq) << 16);
#endif

	status = zrtp_cipher_set_iv(&srtp_stream_ctx->rtp_cipher, &iv);
	if(status){
		return zrtp_status_cipher_fail;
	}

	tag_len = srtp_stream_ctx->rtp_auth.tag_len->tag_length;
	hdr = (zrtp_rtp_hdr_t*)(packet->packet);

	enc_start = (uint32_t *)hdr + uint32s_in_rtp_header + hdr->cc;
	if (1 == hdr->x) {
		zrtp_rtp_hdr_xtnd_t *xtn_hdr = (zrtp_rtp_hdr_xtnd_t *)enc_start;
		enc_start += (zrtp_ntoh16(xtn_hdr->length) + 1);
	}
	//WIN64
	enc_octet_len = *(packet->length) - tag_len - (uint32_t)((enc_start - (uint32_t *)hdr) << 2);


	auth_start = (uint32_t *)hdr;
	auth_tag = (uint8_t *)hdr + *(packet->length) - tag_len;

	if(tag_len>0){
		zrtp_string64_t	auth_tag_str = ZSTR_INIT_EMPTY(auth_tag_str);

		/* shift est, put into network byte order */
		uint64_t packet_seq = packet->seq;
#ifdef ZRTP_NO_64BIT_MATH
		packet_seq = zrtp_hton64( make64((high32(packet_seq) << 16) |
								  (low32(packet_seq) >> 16),
								  low32(packet_seq) << 16));
#else
		packet_seq = zrtp_hton64(packet_seq << 16);
#endif

		hash_ctx = srtp_stream_ctx->rtp_auth.hash->hmac_begin_c( srtp_stream_ctx->rtp_auth.hash,
																 (const char*)srtp_stream_ctx->rtp_auth.key,
																 srtp_stream_ctx->rtp_auth.key_len);
		if(NULL == hash_ctx){
			return zrtp_status_auth_fail;
		}
		status = srtp_stream_ctx->rtp_auth.hash->hmac_update(	srtp_stream_ctx->rtp_auth.hash,
																hash_ctx,
																(const char*)auth_start,
																*packet->length - tag_len);
		if(status){
			return zrtp_status_auth_fail;
		}

		status = srtp_stream_ctx->rtp_auth.hash->hmac_update(	srtp_stream_ctx->rtp_auth.hash,
																hash_ctx,
																(const char*)&packet_seq,
																4);
		if(status){
			return zrtp_status_auth_fail;
		}

		status = srtp_stream_ctx->rtp_auth.hash->hmac_end(	srtp_stream_ctx->rtp_auth.hash,
															hash_ctx,
															(zrtp_stringn_t*) &auth_tag_str,
															srtp_stream_ctx->rtp_auth.tag_len->tag_length);
#if ZRTP_DEBUG_SRTP_KEYS
		{
			char buff[256];
			ZRTP_LOG(3,(_ZTU_,
						   "\tzrtp_srtp_unprotect authentication check. packet_seq[%s] expected auth length[%i] result auth length[%i]\n",
						   hex2str((char*)&packet_seq, sizeof(packet_seq), buff, sizeof(buff)),
						   srtp_stream_ctx->rtp_auth.tag_len->tag_length,
						   auth_tag_str.length));
			ZRTP_LOG(3,(_ZTU_, "\tauth tag[%s]\n",
						   hex2str(auth_tag_str.buffer, auth_tag_str.length, buff, sizeof(buff))));
		}
#endif
		if(status || tag_len != auth_tag_str.length){
#if ZRTP_DEBUG_SRTP_KEYS
			ZRTP_LOG(3,(_ZTU_, "\tAuthentication fail1: status[%i] auth_tag_length[%i] result auth_tag_len[%i]\n",
						status, tag_len, auth_tag_str.length));
#endif
			return zrtp_status_auth_fail;
		}

		if(0 != zrtp_memcmp((uint8_t *)auth_tag_str.buffer, (uint8_t *)auth_tag, tag_len)){
#if ZRTP_DEBUG_SRTP_KEYS
			char buff[256], buff2[256];
			ZRTP_LOG(3,(_ZTU_, "\tAuthentication fail2: tag[%s] computed_tag[%s]\n",
						hex2str((uint8_t *)auth_tag, tag_len, buff, sizeof(buff)),
						hex2str(auth_tag_str.buffer, auth_tag_str.length, buff2, sizeof(buff2))));
#endif
			return zrtp_status_auth_fail;
		}
	}

	status = zrtp_cipher_decrypt(&srtp_stream_ctx->rtp_cipher, (unsigned char*)enc_start, enc_octet_len);
	if(status){
		return zrtp_status_cipher_fail;
	}

	zrtp_srtp_rp_add(&rp_node->rtp_rp, packet);
	*packet->length -= tag_len;

	return status;
}

/*---------------------------------------------------------------------------*/
zrtp_status_t zrtp_srtp_protect_rtcp(	zrtp_srtp_global_t *srtp_global,
										zrtp_srtp_ctx_t	*srtp_ctx,
										zrtp_rtp_info_t *packet)
{
	zrtp_srtp_stream_ctx_t *srtp_stream_ctx = srtp_ctx->outgoing_srtp;
	zrtp_rp_node_t *rp_node;

	uint32_t *enc_start;        /* pointer to start of encrypted portion  */
	uint32_t *auth_start;       /* pointer to start of auth. portion      */
	unsigned enc_octet_len = 0; /* number of octets in encrypted portion  */
	uint8_t *auth_tag = NULL;   /* location of auth_tag within packet     */
	zrtp_status_t status;
	ZRTP_UNALIGNED(zrtp_rtcp_hdr_t) *hdr;
	ZRTP_UNALIGNED(uint32_t) *trailer;	/* pointer to start of trailer    */

	uint32_t seq_num;

	zrtp_v128_t iv;
	zrtp_string64_t	auth_tag_str = ZSTR_INIT_EMPTY(auth_tag_str);

	/*add new replay protection node or get existing one*/
	rp_node = add_rp_node(srtp_ctx, srtp_global->rp_ctx, RP_OUTGOING_DIRECTION, packet->ssrc);
	if(NULL == rp_node){
		return zrtp_status_rp_fail;
	}

	/* check the packet length - it must at least contain a full header */
	if (*(packet->length) < octets_in_rtcp_header){
		return zrtp_status_bad_param;
	}

	hdr = (zrtp_rtcp_hdr_t*)(packet->packet);
	enc_start = (uint32_t *)hdr + uint32s_in_rtcp_header;
	enc_octet_len = *(packet->length) - octets_in_rtcp_header;

	/* all of the packet, except the header, gets encrypted */
	/* NOTE: hdr->length is not usable - it refers to only the first
	   RTCP report in the compound packet! */
	/* NOTE: trailer is 32-bit aligned because RTCP 'packets' are always
	   multiples of 32-bits (RFC 3550 6.1) */
	trailer = (uint32_t *) ((char *)enc_start + enc_octet_len);

	/*
	 * RFC gives us ability of using non-crypted RTCP packets
	 * but we encrypt them anyway. It may be option of stream
	 * context in the future.
	 * if no encryption is used trailer should contain 0x00000000
	 */
	*trailer = zrtp_hton32(ZRTP_RTCP_E_BIT);     /* set encrypt bit */

	/*
	 * set the auth_start and auth_tag pointers to the proper locations
	 * (note that srtpc *always* provides authentication, unlike srtp)
	 */
	/* Note: This would need to change for optional mikey data */
	auth_start = (uint32_t *)hdr;
	auth_tag = (uint8_t *)hdr + *(packet->length) + sizeof(zrtp_rtcp_trailer_t);

	status = zrtp_srtp_rp_increment(&rp_node->rtcp_rp);
	if(zrtp_status_ok != status){
		return zrtp_status_rp_fail;
	}
	seq_num = zrtp_srtp_rp_get_value(&rp_node->rtcp_rp);
	*trailer |= zrtp_hton32(seq_num);
	packet->seq = seq_num;

	iv.v32[0] = 0;
	iv.v32[1] = hdr->ssrc;
	iv.v32[2] = zrtp_hton32(seq_num >> 16);
	iv.v32[3] = zrtp_hton32(seq_num << 16);

	status = zrtp_cipher_set_iv(&srtp_stream_ctx->rtcp_cipher, &iv);
	if(status){
		return zrtp_status_cipher_fail;
	}

	status = zrtp_cipher_encrypt(&srtp_stream_ctx->rtcp_cipher, (unsigned char*)enc_start, enc_octet_len);
	if(status){
		return zrtp_status_cipher_fail;
	}

	status = srtp_stream_ctx->rtcp_auth.hash->hmac_truncated_c(srtp_stream_ctx->rtcp_auth.hash,
															   (const char*)srtp_stream_ctx->rtcp_auth.key,
															   srtp_stream_ctx->rtcp_auth.key_len,
															   (const char*)auth_start,
															   *packet->length + sizeof(zrtp_rtcp_trailer_t),
															   srtp_stream_ctx->rtcp_auth.tag_len->tag_length,
															   (zrtp_stringn_t*) &auth_tag_str);
	if(status){
		return zrtp_status_auth_fail;
	}

	zrtp_memcpy(auth_tag, auth_tag_str.buffer, auth_tag_str.length);

	/* increase the packet length by the length of the auth tag and seq_num*/
	*packet->length += (auth_tag_str.length + sizeof(zrtp_rtcp_trailer_t));



#if ZRTP_DEBUG_SRTP_KEYS
	{
		char buffer[1000];
		ZRTP_LOG(3,(_ZTU_, "\tpacket: %s\n",
					hex2str(packet->packet, (*packet->length) - (auth_tag_str.length + sizeof(zrtp_rtcp_trailer_t)), buffer, 1000)));
		ZRTP_LOG(3,(_ZTU_, "\ttrailer and auth tag: %s\n",
					hex2str((uint8_t*)packet->packet + ((*packet->length) - (auth_tag_str.length + sizeof(zrtp_rtcp_trailer_t))),
							auth_tag_str.length + sizeof(zrtp_rtcp_trailer_t),
							buffer, 1000)));
	}
#endif

	return status;
}

/*---------------------------------------------------------------------------*/
zrtp_status_t zrtp_srtp_unprotect_rtcp(	zrtp_srtp_global_t *srtp_global,
										zrtp_srtp_ctx_t *srtp_ctx,
										zrtp_rtp_info_t *packet)
{
	zrtp_srtp_stream_ctx_t *srtp_stream_ctx = srtp_ctx->incoming_srtp;
	zrtp_rp_node_t *rp_node;

	uint32_t *enc_start;        /* pointer to start of encrypted portion  */
	uint32_t *auth_start;       /* pointer to start of auth. portion      */
	unsigned enc_octet_len = 0; /* number of octets in encrypted portion  */
	uint8_t *auth_tag = NULL;   /* location of auth_tag within packet     */
	zrtp_status_t status;
	ZRTP_UNALIGNED(zrtp_rtcp_hdr_t) *hdr;
	ZRTP_UNALIGNED(uint32_t) *trailer;	/* pointer to start of trailer    */


	int tag_len = 0;
	zrtp_v128_t iv;

	/* add new replay protection node or get existing one */
	rp_node = add_rp_node(srtp_ctx, srtp_global->rp_ctx, RP_INCOMING_DIRECTION, packet->ssrc);
	if(NULL == rp_node){
		return zrtp_status_rp_fail;
	}

	/* check the packet length - it must at least contain a full header */
	if (*(packet->length) < octets_in_rtcp_header){
		return zrtp_status_bad_param;
	}

	tag_len = srtp_stream_ctx->rtcp_auth.tag_len->tag_length;
	hdr = (zrtp_rtcp_hdr_t*)(packet->packet);

	enc_octet_len = *packet->length -
		(octets_in_rtcp_header + tag_len + sizeof(zrtp_rtcp_trailer_t));

	/*	index & E (encryption) bit follow normal data.  hdr->len
		is the number of words (32-bit) in the normal packet minus 1 */
	/*	This should point trailer to the word past the end of the
		normal data. */
	/*	This would need to be modified for optional mikey data */
	/*
	 *	NOTE: trailer is 32-bit aligned because RTCP 'packets' are always
	 *	multiples of 32-bits (RFC 3550 6.1)
	 */

	trailer = (uint32_t *) ((char *) hdr + *packet->length - (tag_len + sizeof(zrtp_rtcp_trailer_t)));

	if (*((unsigned char *) trailer) & ZRTP_RTCP_E_BYTE_BIT) {
		enc_start = (uint32_t *)hdr + uint32s_in_rtcp_header;
	} else {
		enc_octet_len = 0;
		enc_start = NULL; /* this indicates that there's no encryption */
	}

	/*
	 * set the auth_start and auth_tag pointers to the proper locations
	 * (note that srtcp *always* uses authentication, unlike srtp)
	 */
	auth_start = (uint32_t *)hdr;
	auth_tag = (uint8_t *)hdr + *packet->length - tag_len;

	packet->seq = zrtp_ntoh32(*trailer) & 0x7fffffff;

	status = zrtp_srtp_rp_check(&rp_node->rtcp_rp, packet);
	if(zrtp_status_ok != status){
		return zrtp_status_rp_fail;
	}

	iv.v32[0] = 0;
	iv.v32[1] = hdr->ssrc; /* still in network order! */
	iv.v32[2] = zrtp_hton32(packet->seq >> 16);
	iv.v32[3] = zrtp_hton32(packet->seq << 16);

	status = zrtp_cipher_set_iv(&srtp_stream_ctx->rtcp_cipher, &iv);
	if(status){
		return zrtp_status_cipher_fail;
	}

	if(tag_len>0){
		zrtp_string64_t	auth_tag_str = ZSTR_INIT_EMPTY(auth_tag_str);

		status = srtp_stream_ctx->rtcp_auth.hash->hmac_truncated_c(srtp_stream_ctx->rtcp_auth.hash,
																   (const char*)srtp_stream_ctx->rtcp_auth.key,
																   srtp_stream_ctx->rtcp_auth.key_len,
																   (const char*)auth_start,
																   *packet->length - tag_len,
																   tag_len,
																   (zrtp_stringn_t*) &auth_tag_str);
		if(status || tag_len != auth_tag_str.length){
			return zrtp_status_auth_fail;
		}

		if(0 != zrtp_memcmp((uint8_t *)auth_tag_str.buffer, (uint8_t *)auth_tag, tag_len)){
			return zrtp_status_auth_fail;
		}
	}else{
		return zrtp_status_auth_fail;
	}

	if(enc_start){
		status = zrtp_cipher_decrypt(&srtp_stream_ctx->rtcp_cipher, (unsigned char*)enc_start, enc_octet_len);
		if(status){
			return zrtp_status_cipher_fail;
		}
	}

	zrtp_srtp_rp_add(&rp_node->rtcp_rp, packet);
	*packet->length -= (tag_len + sizeof(zrtp_rtcp_trailer_t));

	return status;
}

#endif /* !ZRTP_USE_EXTERN_SRTP */

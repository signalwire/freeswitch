/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */

#ifndef __ZRTP_IFACE_CACHE_H__ 
#define __ZRTP_IFACE_CACHE_H__

#include "zrtp_config.h"
#include "zrtp_base.h"
#include "zrtp_string.h"
#include "zrtp_error.h"
#include "zrtp_iface.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#if defined(ZRTP_USE_BUILTIN_CACHE) && (ZRTP_USE_BUILTIN_CACHE == 1)

#define ZRTP_DEF_CACHE_VERSION_STR	"libZRTP cache version="
#define ZRTP_DEF_CACHE_VERSION_VAL	"1.0"
	
/**
 * @brief Cache element identifier type
 * Elements of this type link cache data with a pair of ZIDs.
 * (constructed as: [ZID1][ZID2], where ZID1 - ZID with greater binary value)
 * This type is used to identify cache elements in the built-in implementation.
 */
typedef uint8_t zrtp_cache_id_t[24];
	
#define ZRTP_MITMCACHE_ELEM_LENGTH ( sizeof(zrtp_cache_id_t) + sizeof(zrtp_string64_t) )
#define ZRTP_CACHE_ELEM_LENGTH ( sizeof(zrtp_cache_elem_t) - sizeof(mlist_t) - (sizeof(uint32_t)*2) )
#define ZFONE_CACHE_NAME_LENGTH    256
		
/**
 * @brief Secret cache element structure
 * This structure is used to store cache data in the built-in implementation
 * of the caching system.
 */
typedef struct zrtp_cache_elem
{	
	zrtp_cache_id_t    	id;				/** Cache element identifier */
	zrtp_string64_t    	curr_cache;		/** Current cache value */
	zrtp_string64_t    	prev_cache;		/** Prev cache value */
	uint32_t           	verified;		/** Verified flag for the cache value */
	uint32_t		   	lastused_at;	/** Last usage time-stamp in seconds */
	uint32_t			ttl;			/** Cache TTL since lastused_at in seconds */
	uint32_t           	secure_since;	/** Secure since date in seconds. Utility field. Don't required by libzrtp. */
	char				name[ZFONE_CACHE_NAME_LENGTH]; /** name of the user associated with this cache entry */
	uint32_t           	name_length;	/** cache name lengths */
	uint32_t			presh_counter;	/** number of Preshared streams made since last DH exchange */
	uint32_t			_index;			/** cache element index in the cache file */
	uint32_t			_is_dirty;		/** dirty flag means the entry has unsaved changes */
	mlist_t            	_mlist;
} zrtp_cache_elem_t;
	
#endif /* ZRTP_USE_BUILTIN_CACHE */

zrtp_status_t zrtp_def_cache_init(zrtp_global_t* zrtp);

void zrtp_def_cache_down();

zrtp_status_t zrtp_def_cache_set_verified( const zrtp_stringn_t* one_zid,
										   const zrtp_stringn_t* another_zid,
										   uint32_t verified);
	
zrtp_status_t zrtp_def_cache_get_verified( const zrtp_stringn_t* one_zid,
										   const zrtp_stringn_t* another_zid,
										   uint32_t* verified);
	
	
zrtp_status_t zrtp_def_cache_put( const zrtp_stringn_t* one_zid,
								  const zrtp_stringn_t* another_zid,
								  zrtp_shared_secret_t *rss);

zrtp_status_t zrtp_def_cache_put_mitm( const zrtp_stringn_t* one_zid,
									   const zrtp_stringn_t* another_zid, 
									   zrtp_shared_secret_t *rss);

zrtp_status_t zrtp_def_cache_get( const zrtp_stringn_t* one_zid,
								  const zrtp_stringn_t* another_zid,
								  zrtp_shared_secret_t *rss,
								  int prev_requested);

zrtp_status_t zrtp_def_cache_get_mitm( const zrtp_stringn_t* one_zid,
									   const zrtp_stringn_t* another_zid,
									   zrtp_shared_secret_t *rss);
	
zrtp_status_t zrtp_def_cache_set_presh_counter( const zrtp_stringn_t* one_zid,
											    const zrtp_stringn_t* another_zid,
											    uint32_t counter);

zrtp_status_t zrtp_def_cache_get_presh_counter( const zrtp_stringn_t* one_zid,
											    const zrtp_stringn_t* another_zid,
											    uint32_t* counter);
	
#if defined(ZRTP_USE_BUILTIN_CACHE) && (ZRTP_USE_BUILTIN_CACHE == 1)
/**
 * @brief Cache iterator
 * zrtp_def_cache_foreach() calls this function for every cache entry.
 * @param elem - cache element;
 * @param is_mitm - is 1 when callback was called for MiTM for each.
 * @param del - callback may return 1 to this to remove cache entry from the list.
 * @param data - pointer to some user data from zrtp_def_cache_foreach();
 * @return
 *  - 0 - if element was requested for reading only and wasn't changed;
 *  - 1 - if element was modified and cache should be updated.
 */
typedef int (*zrtp_cache_callback_t)(zrtp_cache_elem_t* elem, int is_mitm, void* data, int* del);

/**
 * @brief Iterate over all cache entries.
 * Can be used for searching and modifying cache entries. Protected by mutex.
 * Can be called in parallel with other cache operations when protocol is
 * running.
 * @param global - libzrtp global context;
 * @param is_mitm - if value of this flag is 1 - fore_each will be applied for MiTM secrets;
 * @param callback - function to be called for every cache entry;
 * @param data - this pointer will be passed to every \c callback call.
 */
void zrtp_def_cache_foreach( zrtp_global_t *global,
							 int is_mitm,
							 zrtp_cache_callback_t callback,
							 void *data);
	
#endif /* ZRTP_USE_BUILTIN_CACHE */

/**
 * @brief Store shared secrets cache to the persistent storage
 * May be used in server solutions for periodically flushing the cache to prevent data loss.
 *
 * @return 
 *  - zrtp_status_ok - if operation completed successfully;
 *	- zrtp_status_wrong_state - if a call is performed from a routine which
 *	  doesn't use the default cache.
 */
zrtp_status_t zrtp_def_cache_store(zrtp_global_t *global);

zrtp_status_t zrtp_def_cache_reset_since( const zrtp_stringn_t* one_zid,
									      const zrtp_stringn_t* another_zid);
	
zrtp_status_t zrtp_def_cache_get_since( const zrtp_stringn_t* one_zid,
									    const zrtp_stringn_t* another_zid,
									    uint32_t* since);

zrtp_status_t zrtp_def_cache_get_name( const zrtp_stringn_t* one_zid,
									   const zrtp_stringn_t* another_zid,
									   zrtp_stringn_t* name);

zrtp_status_t zrtp_def_cache_put_name( const zrtp_stringn_t* one_zid,
									   const zrtp_stringn_t* another_zid,
									   const zrtp_stringn_t* name);
	
#if defined(ZRTP_USE_BUILTIN_CACHE) && (ZRTP_USE_BUILTIN_CACHE == 1)
zrtp_cache_elem_t* zrtp_def_cache_get2(const zrtp_cache_id_t id, int is_mitm);
#endif /* ZRTP_USE_BUILTIN_CACHE */

#if defined(__cplusplus)
}
#endif

#endif /*__ZRTP_IFACE_CACHE_H__*/


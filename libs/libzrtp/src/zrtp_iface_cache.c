/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2012 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */

#include "zrtp.h"

#if defined(ZRTP_USE_BUILTIN_CACHE) && (ZRTP_USE_BUILTIN_CACHE == 1)

#define _ZTU_ "zrtp cache"


/* Windows kernel have it's own realization in Windows registry*/
#if (ZRTP_PLATFORM != ZP_WIN32_KERNEL)

static mlist_t 	cache_head;
static uint32_t	g_cache_elems_counter = 0;
static mlist_t 	mitmcache_head;
static uint32_t	g_mitmcache_elems_counter = 0;
static uint8_t	inited = 0;
static uint8_t g_needs_rewriting = 0;

static zrtp_global_t* zrtp;
static zrtp_mutex_t* def_cache_protector = NULL;


/* Create cache ID like a pair of ZIDs. ZID with lowest value at the beginning */
void zrtp_cache_create_id( const zrtp_stringn_t* first_ZID,
							 const zrtp_stringn_t* second_ZID,
							 zrtp_cache_id_t id);

/* Searching for cache element by cache ID */
static zrtp_cache_elem_t* get_elem(const zrtp_cache_id_t id, uint8_t is_mitm);

/* Allows use cache on system with different byte-order */
static void cache_make_cross( zrtp_cache_elem_t* from,
							  zrtp_cache_elem_t* to,
							  uint8_t is_upload);

static zrtp_status_t zrtp_cache_user_init();
static zrtp_status_t zrtp_cache_user_down();


/*===========================================================================*/
/*     libZRTP interface implementation										 */ 
/*===========================================================================*/

#define ZRTP_CACHE_CHECK_ZID(a,b) \
	if ( (a->length != b->length) || \
		 (a->length != sizeof(zrtp_zid_t)) ) \
	{ \
		return zrtp_status_bad_param; \
	}

zrtp_status_t zrtp_def_cache_init(zrtp_global_t* a_zrtp)
{
	zrtp_status_t s = zrtp_status_ok;

	if (!inited) {
		zrtp = a_zrtp;
		s = zrtp_mutex_init(&def_cache_protector);
		if (zrtp_status_ok != s) {
			return s;
		}
		
		init_mlist(&cache_head);
		init_mlist(&mitmcache_head);
		s =  zrtp_cache_user_init();		
		
		inited = 1;
	}

	return s;
}

void zrtp_def_cache_down()
{
	if (inited) {
		mlist_t *node = NULL, *tmp = NULL;				
		
		/* If automatic cache flushing enabled we don't need to store it in a disk as it should be already in sync. */
		if (!zrtp->cache_auto_store)
			zrtp_cache_user_down();

		mlist_for_each_safe(node, tmp, &cache_head) {
			zrtp_sys_free(mlist_get_struct(zrtp_cache_elem_t, _mlist, node));
		}
		mlist_for_each_safe(node, tmp, &mitmcache_head) {
			zrtp_sys_free(mlist_get_struct(zrtp_cache_elem_t, _mlist, node));
		}
	
		init_mlist(&cache_head);
		init_mlist(&mitmcache_head);
		
		zrtp_mutex_destroy(def_cache_protector);
		
		inited = 0;
		zrtp = NULL;
	}
}


zrtp_status_t zrtp_def_cache_set_verified( const zrtp_stringn_t* one_ZID,
										   const zrtp_stringn_t* another_ZID,
										   uint32_t verified)
{
	zrtp_cache_id_t	id;
	zrtp_cache_elem_t* new_elem = NULL;

	ZRTP_CACHE_CHECK_ZID(one_ZID, another_ZID);
	zrtp_cache_create_id(one_ZID, another_ZID, id);
	
	zrtp_mutex_lock(def_cache_protector);	
	new_elem = get_elem(id, 0);
	if (new_elem) {
		new_elem->verified = verified;
	}
	zrtp_mutex_unlock(def_cache_protector);
	
	if (zrtp->cache_auto_store) zrtp_def_cache_store(zrtp);

	return (new_elem) ? zrtp_status_ok : zrtp_status_fail;
}

zrtp_status_t zrtp_def_cache_get_verified( const zrtp_stringn_t* one_ZID,
										   const zrtp_stringn_t* another_ZID,
										   uint32_t* verified)

{
	zrtp_cache_id_t	id;	
	zrtp_cache_elem_t* elem = NULL;
	
	ZRTP_CACHE_CHECK_ZID(one_ZID, another_ZID);
	zrtp_cache_create_id(one_ZID, another_ZID, id);
	
	zrtp_mutex_lock(def_cache_protector);
	elem = get_elem(id, 0);
	if (elem) {
		*verified = elem->verified;
	}
	zrtp_mutex_unlock(def_cache_protector);
	
	return (elem) ? zrtp_status_ok : zrtp_status_fail;
}


static zrtp_status_t cache_put( const zrtp_stringn_t* one_ZID,
								const zrtp_stringn_t* another_ZID,
								zrtp_shared_secret_t *rss,
								uint8_t is_mitm )
{
    zrtp_cache_elem_t* new_elem = 0;
	zrtp_cache_id_t	id;

	ZRTP_CACHE_CHECK_ZID(one_ZID, another_ZID);
	zrtp_cache_create_id(one_ZID, another_ZID, id);
	
	{
	char zid1str[24+1], zid2str[24+1];
	ZRTP_LOG(3,(_ZTU_,"\tcache_put() zid1=%s, zis2=%s MiTM=%s\n",
			hex2str(one_ZID->buffer, one_ZID->length, zid1str, sizeof(zid1str)),
			hex2str(another_ZID->buffer, another_ZID->length, zid2str, sizeof(zid2str)),
			is_mitm?"YES":"NO"));
	}
	
	zrtp_mutex_lock(def_cache_protector);
	do {
		new_elem = get_elem(id, is_mitm);
		if (!new_elem)
		{	
			/* If cache doesn't exist - create new one */
			if (!( new_elem = (zrtp_cache_elem_t*) zrtp_sys_alloc(sizeof(zrtp_cache_elem_t)) ))	{
				break;
			}
					
			zrtp_memset(new_elem, 0, sizeof(zrtp_cache_elem_t));		
			ZSTR_SET_EMPTY(new_elem->curr_cache);
			ZSTR_SET_EMPTY(new_elem->prev_cache);
			
			new_elem->secure_since = (uint32_t)(zrtp_time_now()/1000);
							
			mlist_add_tail(is_mitm ? &mitmcache_head : &cache_head, &new_elem->_mlist);
			zrtp_memcpy(new_elem->id, id, sizeof(zrtp_cache_id_t));
			
			if (is_mitm) {
				new_elem->_index = g_mitmcache_elems_counter++;
			} else {
				new_elem->_index = g_cache_elems_counter++;
			}
			
			ZRTP_LOG(3,(_ZTU_,"\tcache_put() can't find element in the cache - create a new entry index=%u.\n", new_elem->_index));
		}
		else {
			ZRTP_LOG(3,(_ZTU_,"\tcache_put() Just update existing value.\n"));
		}
		
		/* Save current cache value as previous one and new as a current */
		if (!is_mitm) {
			if (new_elem->curr_cache.length > 0) {
				zrtp_zstrcpy(ZSTR_GV(new_elem->prev_cache), ZSTR_GV(new_elem->curr_cache));
			}
		}

		zrtp_zstrcpy(ZSTR_GV(new_elem->curr_cache), ZSTR_GV(rss->value));
		new_elem->lastused_at	= rss->lastused_at;
		if (!is_mitm) {
			new_elem->ttl		= rss->ttl;
		}
		
		new_elem->_is_dirty = 1;
	} while (0);
	zrtp_mutex_unlock(def_cache_protector);

	if (zrtp->cache_auto_store) zrtp_def_cache_store(zrtp);

    return (new_elem) ? zrtp_status_ok : zrtp_status_fail;
}

zrtp_status_t zrtp_def_cache_put( const zrtp_stringn_t* one_ZID,
								  const zrtp_stringn_t* another_ZID,
								  zrtp_shared_secret_t *rss) {	
	return cache_put(one_ZID, another_ZID, rss, 0);
}

zrtp_status_t zrtp_def_cache_put_mitm( const zrtp_stringn_t* one_ZID,
									   const zrtp_stringn_t* another_ZID,
									   zrtp_shared_secret_t *rss) {
	return cache_put(one_ZID, another_ZID, rss, 1);
}


static zrtp_status_t cache_get( const zrtp_stringn_t* one_ZID,
								const zrtp_stringn_t* another_ZID,
								zrtp_shared_secret_t *rss,
								int prev_requested,
								uint8_t is_mitm)
{
    zrtp_cache_elem_t* curr = 0;
	zrtp_cache_id_t	id;
	zrtp_status_t s = zrtp_status_ok;
	
	{
	char zid1str[24+1], zid2str[24+1];
	ZRTP_LOG(3,(_ZTU_,"\tache_get(): zid1=%s, zis2=%s MiTM=%s\n",
			hex2str(one_ZID->buffer, one_ZID->length, zid1str, sizeof(zid1str)),
			hex2str(another_ZID->buffer, another_ZID->length, zid2str, sizeof(zid2str)),
			is_mitm?"YES":"NO"));
	}

	ZRTP_CACHE_CHECK_ZID(one_ZID, another_ZID);
	zrtp_cache_create_id(one_ZID, another_ZID, id);
	
	zrtp_mutex_lock(def_cache_protector);
    do {		
		curr = get_elem(id, is_mitm);
		if (!curr || (!curr->prev_cache.length && prev_requested)) {
			s = zrtp_status_fail;
			ZRTP_LOG(3,(_ZTU_,"\tache_get() - not found.\n"));
			break;
		}    
			
		zrtp_zstrcpy( ZSTR_GV(rss->value),
					  prev_requested ? ZSTR_GV(curr->prev_cache) : ZSTR_GV(curr->curr_cache));
		
		rss->lastused_at = curr->lastused_at;
		if (!is_mitm) {
			rss->ttl = curr->ttl;
		}
	} while (0);
	zrtp_mutex_unlock(def_cache_protector);

    return s;
}

zrtp_status_t zrtp_def_cache_get( const zrtp_stringn_t* one_ZID,
								  const zrtp_stringn_t* another_ZID,
								  zrtp_shared_secret_t *rss,
								  int prev_requested)
{
	return cache_get(one_ZID, another_ZID, rss, prev_requested, 0);
}

zrtp_status_t zrtp_def_cache_get_mitm( const zrtp_stringn_t* one_ZID,
									   const zrtp_stringn_t* another_ZID,
									   zrtp_shared_secret_t *rss)
{
	return cache_get(one_ZID, another_ZID, rss, 0, 1);
}

zrtp_status_t zrtp_def_cache_set_presh_counter( const zrtp_stringn_t* one_zid,
											    const zrtp_stringn_t* another_zid,
											    uint32_t counter) 
{
	zrtp_cache_elem_t* new_elem = 0;
	zrtp_cache_id_t	id;
	
	ZRTP_CACHE_CHECK_ZID(one_zid, another_zid);
	zrtp_cache_create_id(one_zid, another_zid, id);
	
	zrtp_mutex_lock(def_cache_protector);
	new_elem = get_elem(id, 0);
	if (new_elem) {
		new_elem->presh_counter = counter;
		
		new_elem->_is_dirty = 1;
	}
	zrtp_mutex_unlock(def_cache_protector);
	
	if (zrtp->cache_auto_store) zrtp_def_cache_store(zrtp);

	return (new_elem) ? zrtp_status_ok : zrtp_status_fail;
}

zrtp_status_t zrtp_def_cache_get_presh_counter( const zrtp_stringn_t* one_zid,
												const zrtp_stringn_t* another_zid,
											    uint32_t* counter)
{
	zrtp_cache_elem_t* new_elem = 0;
	zrtp_cache_id_t	id;	
	
	ZRTP_CACHE_CHECK_ZID(one_zid, another_zid);
	zrtp_cache_create_id(one_zid, another_zid, id);
	
	zrtp_mutex_lock(def_cache_protector);
	new_elem = get_elem(id, 0);
	if (new_elem) {
		*counter = new_elem->presh_counter;
	}
	zrtp_mutex_unlock(def_cache_protector);
	
	return (new_elem) ? zrtp_status_ok : zrtp_status_fail;
}

 void zrtp_cache_create_id( const zrtp_stringn_t* first_ZID,
							 const zrtp_stringn_t* second_ZID,
							 zrtp_cache_id_t id )
{	
	if (0 < zrtp_memcmp(first_ZID->buffer, second_ZID->buffer, sizeof(zrtp_zid_t))) {
		const zrtp_stringn_t* tmp_ZID = first_ZID;
		first_ZID = second_ZID;
		second_ZID = tmp_ZID;
	}

	zrtp_memcpy(id, first_ZID->buffer, sizeof(zrtp_zid_t));
	zrtp_memcpy((char*)id+sizeof(zrtp_zid_t), second_ZID->buffer, sizeof(zrtp_zid_t));
}

zrtp_cache_elem_t* zrtp_def_cache_get2(const zrtp_cache_id_t id, int is_mitm)
{
	return get_elem(id, is_mitm);
}


static zrtp_cache_elem_t* get_elem(const zrtp_cache_id_t id, uint8_t is_mitm)
{
	mlist_t* node = NULL;
	mlist_t* head = is_mitm ? &mitmcache_head : &cache_head;
	mlist_for_each(node, head) {
		zrtp_cache_elem_t* elem = mlist_get_struct(zrtp_cache_elem_t, _mlist, node);
		if (!zrtp_memcmp(elem->id, id, sizeof(zrtp_cache_id_t))) {
			return elem;
		}
    }
    
    return NULL;	
}

static void cache_make_cross(zrtp_cache_elem_t* from, zrtp_cache_elem_t* to, uint8_t is_upload)
{
	if (!to) {
		return;
	}

	if (from) {
		zrtp_memcpy(to, from, sizeof(zrtp_cache_elem_t));
	}

	if (is_upload) {
		to->verified 	= zrtp_ntoh32(to->verified);
		to->secure_since= zrtp_ntoh32(to->secure_since);
		to->lastused_at = zrtp_ntoh32(to->lastused_at);
		to->ttl			= zrtp_ntoh32(to->ttl);
		to->name_length	= zrtp_ntoh32(to->name_length);
		to->curr_cache.length = zrtp_ntoh16(to->curr_cache.length);
		to->prev_cache.length = zrtp_ntoh16(to->prev_cache.length);
		to->presh_counter	= zrtp_ntoh32(to->presh_counter);
	} else {
		to->verified	= zrtp_hton32(to->verified);
		to->secure_since= zrtp_hton32(to->secure_since);
		to->lastused_at = zrtp_hton32(to->lastused_at);
		to->ttl			= zrtp_hton32(to->ttl);
		to->name_length	= zrtp_hton32(to->name_length);
		to->curr_cache.length = zrtp_hton16(to->curr_cache.length);
		to->prev_cache.length = zrtp_hton16(to->prev_cache.length);
		to->presh_counter	= zrtp_hton32(to->presh_counter);
	}
}


/*===========================================================================*/
/*     ZRTP cache realization as a simple binary file						 */
/*===========================================================================*/


#if ZRTP_HAVE_STDIO_H == 1
	#include <stdio.h>
#endif

#include <string.h>

/*---------------------------------------------------------------------------*/
#define ZRTP_INT_CACHE_BREAK(s, status) \
{ \
	if (!s) s = status; \
	break; \
}\

zrtp_status_t zrtp_cache_user_init()
{
	FILE* 	cache_file = 0;
	zrtp_cache_elem_t* new_elem = 0;
	zrtp_status_t s = zrtp_status_ok;	
	uint32_t cache_elems_count = 0;
	uint32_t mitmcache_elems_count = 0;
	uint32_t i = 0;
	unsigned is_unsupported = 0;
	
	ZRTP_LOG(3,(_ZTU_,"\tLoad ZRTP cache from <%s>...\n", zrtp->def_cache_path.buffer));
	
	g_mitmcache_elems_counter = 0;
	g_cache_elems_counter = 0;
	g_needs_rewriting = 0;
    
    /* Try to open existing file. If ther is no cache file - start with empty cache */
#if (ZRTP_PLATFORM == ZP_WIN32)
    if (0 != fopen_s(&cache_file, zrtp->def_cache_path.buffer, "rb")) {
		return zrtp_status_ok;
    }
#else    
    if (0 == (cache_file = fopen(zrtp->def_cache_path.buffer, "rb"))) {
		ZRTP_LOG(3,(_ZTU_,"\tCan't open file for reading.\n"));
		return zrtp_status_ok;
	}
#endif	
	/*
	 * Check for the cache file version number. Current version of libzrtp doesn't support
	 * backward compatibility in zrtp cache file structure, so we just remove the old cache file.
	 *
	 * Version field format: $ZRTP_DEF_CACHE_VERSION_STR$ZRTP_DEF_CACHE_VERSION_VAL
	 */
	do {
		char version_buff[256];
		memset(version_buff, 0, sizeof(version_buff));
		
		if (fread(version_buff, strlen(ZRTP_DEF_CACHE_VERSION_STR)+strlen(ZRTP_DEF_CACHE_VERSION_VAL), 1, cache_file) <= 0) {
			ZRTP_LOG(3,(_ZTU_,"\tCache Error: Cache file is too small.\n"));
			is_unsupported = 1;
			break;
		}
		
		if (0 != zrtp_memcmp(version_buff, ZRTP_DEF_CACHE_VERSION_STR, strlen(ZRTP_DEF_CACHE_VERSION_STR))) {
			ZRTP_LOG(3,(_ZTU_,"\tCache Error: Can't find ZRTP Version tag in the cache file.\n"));
			is_unsupported = 1;
			break;
		}
		
		ZRTP_LOG(3,(_ZTU_,"\tZRTP cache file has version=%s\n", version_buff+strlen(ZRTP_DEF_CACHE_VERSION_STR)));
		
		if (0 != zrtp_memcmp(version_buff+strlen(ZRTP_DEF_CACHE_VERSION_STR), ZRTP_DEF_CACHE_VERSION_VAL, strlen(ZRTP_DEF_CACHE_VERSION_VAL))) {
			ZRTP_LOG(3,(_ZTU_,"\tCache Error: Unsupported ZRTP cache version.\n"));
			is_unsupported = 1;
			break;
		}
	} while (0);
	
	if (is_unsupported) {
		ZRTP_LOG(3,(_ZTU_,"\tCache Error: Unsupported version of ZRTP cache file detected - white-out the cache.\n"));
		fclose(cache_file);		
		return zrtp_status_ok;
	}

	/*
	 *  Load MitM caches: first 32 bits is a MiTM secrets counter. Read it and then
	 *  upload appropriate number of MitM secrets.
	 */
	do {
		cache_elems_count = 0;
		if (fread(&mitmcache_elems_count, 4, 1, cache_file) <= 0) {
			ZRTP_INT_CACHE_BREAK(s, zrtp_status_read_fail);
		}
		mitmcache_elems_count = zrtp_ntoh32(mitmcache_elems_count);
		
		ZRTP_LOG(3,(_ZTU_,"\tZRTP cache file contains %u MiTM secrets.\n", mitmcache_elems_count));
		
		for (i=0; i<mitmcache_elems_count; i++)
		{
			new_elem = (zrtp_cache_elem_t*) zrtp_sys_alloc(sizeof(zrtp_cache_elem_t));
			if (!new_elem) {
				ZRTP_INT_CACHE_BREAK(s, zrtp_status_alloc_fail);
			}
			
			if (fread(new_elem, ZRTP_MITMCACHE_ELEM_LENGTH, 1, cache_file) <= 0) {
				ZRTP_LOG(3,(_ZTU_,"\tERROR! MiTM cache element read fail (id=%u).\n", i));
				
				zrtp_sys_free(new_elem);
				ZRTP_INT_CACHE_BREAK(s, zrtp_status_read_fail);
			}

			cache_make_cross(NULL, new_elem, 1);
			
			new_elem->_index = g_mitmcache_elems_counter++;
			new_elem->_is_dirty = 0;
			
			mlist_add_tail(&mitmcache_head, &new_elem->_mlist);
		}

		if (i != mitmcache_elems_count)
			ZRTP_INT_CACHE_BREAK(s, zrtp_status_read_fail);
	} while(0);
	if (s != zrtp_status_ok) {
		fclose(cache_file);
		zrtp_def_cache_down();
		return s;
	}
	
	ZRTP_LOG(3,(_ZTU_,"\tAll %u MiTM Cache entries have been uploaded.\n", g_mitmcache_elems_counter));

	/*
	 * Load regular caches: first 32 bits is a secrets counter. Read it and then
	 * upload appropriate number of regular secrets.
	 */
	cache_elems_count = 0;
	if (fread(&cache_elems_count, 4, 1, cache_file) <= 0) {
		fclose(cache_file);
		zrtp_def_cache_down();
		return zrtp_status_read_fail;
	}
	cache_elems_count = zrtp_ntoh32(cache_elems_count);
	
	ZRTP_LOG(3,(_ZTU_,"\tZRTP cache file contains %u RS secrets.\n", cache_elems_count));
	
	for (i=0; i<cache_elems_count; i++)
	{
		new_elem = (zrtp_cache_elem_t*) zrtp_sys_alloc(sizeof(zrtp_cache_elem_t));
		if (!new_elem) {
			ZRTP_INT_CACHE_BREAK(s, zrtp_status_alloc_fail);
		}

		if (fread(new_elem, ZRTP_CACHE_ELEM_LENGTH, 1, cache_file) <= 0) {
			ZRTP_LOG(3,(_ZTU_,"\tERROR! RS cache element read fail (id=%u).\n", i));
			zrtp_sys_free(new_elem);
			ZRTP_INT_CACHE_BREAK(s, zrtp_status_read_fail);			
		}

		cache_make_cross(NULL, new_elem, 1);
		
		new_elem->_index = g_cache_elems_counter++;
		new_elem->_is_dirty = 0;
		
		mlist_add_tail(&cache_head, &new_elem->_mlist);
	}
	if (i != cache_elems_count) {		
		s = zrtp_status_read_fail;
	}			

    if (0 != fclose(cache_file)) {
		zrtp_def_cache_down();
		return zrtp_status_fail;
    }

	ZRTP_LOG(3,(_ZTU_,"\tAll of %u RS Cache entries have been uploaded.\n", g_cache_elems_counter));

	return s;
}


#define ZRTP_DOWN_CACHE_RETURN(s, f) \
{\
	if (zrtp_status_ok != s) { \
		ZRTP_LOG(3,(_ZTU_,"\tERROR! Unable to writing to ZRTP cache file.\n")); \
	} \
	if (f) { \
		fclose(f);\
	} \
	return s;\
};

static zrtp_status_t flush_elem_(zrtp_cache_elem_t *elem, FILE *cache_file, unsigned is_mitm) {
	zrtp_cache_elem_t tmp_elem;
	uint32_t pos = 0;
	
	/*
	 * Let's calculate cache element position in the file
	 */
	
// @note: I'm going to remove unused comments when random-access cache get more stable. (vkrykun, Nov 27, 2011)
//	printf("flush_elem_(): calculate Element offset for %s..\n", is_mitm?"MiTM":"RS");
	
	/* Skip the header */
	pos += strlen(ZRTP_DEF_CACHE_VERSION_STR)+strlen(ZRTP_DEF_CACHE_VERSION_VAL);
	
	pos += sizeof(uint32_t); /* Skip MiTM secretes count. */
	
//	printf("flush_elem_(): \t pos=%u (Header, MiTM Count).\n", pos);
	
	if (is_mitm) {
		/* position within MiTM secrets block. */
		pos += (elem->_index * ZRTP_MITMCACHE_ELEM_LENGTH);
//		printf("flush_elem_(): \t pos=%u (Header, MiTM Count + %u MiTM Secrets).\n", pos, elem->_index);
	} else {
		/* Skip MiTM Secrets block */
		pos += (g_mitmcache_elems_counter * ZRTP_MITMCACHE_ELEM_LENGTH);
		
		pos += sizeof(uint32_t); /* Skip RS elements count. */
		
		pos += (elem->_index * ZRTP_CACHE_ELEM_LENGTH); /* Skip previous RS elements */
		
//		printf("flush_elem_(): \t pos=%u (Header, MiTM Count + ALL %u Secrets, RS counter and %u prev. RS).\n", pos, g_mitmcache_elems_counter, elem->_index);
	}

	fseek(cache_file, pos, SEEK_SET);
	
	/* Prepare element for storing, convert all fields to the network byte-order. */
	cache_make_cross(elem, &tmp_elem, 0);
	
//	printf("flush_elem_(): write to offset=%lu\n", ftell(cache_file));
	
	/* Flush the element. */
	if (fwrite(&tmp_elem, (is_mitm ? ZRTP_MITMCACHE_ELEM_LENGTH : ZRTP_CACHE_ELEM_LENGTH), 1, cache_file) != 1) {		
//		printf("flush_elem_(): ERROR!!! write failed!\n");
		return zrtp_status_write_fail;
	} else {
		elem->_is_dirty = 0;
		
//		printf("flush_elem_(): OK! %lu bytes were written\n", (is_mitm ? ZRTP_MITMCACHE_ELEM_LENGTH : ZRTP_CACHE_ELEM_LENGTH));
		return zrtp_status_ok;
	}
}

zrtp_status_t zrtp_cache_user_down()
{
	FILE* cache_file = 0;	
	mlist_t *node = 0;
	uint32_t count = 0, dirty_count=0;
	uint32_t pos = 0;

	ZRTP_LOG(3,(_ZTU_,"\tStoring ZRTP cache to <%s>...\n", zrtp->def_cache_path.buffer));
	
    /* Open/create file for writing */
#if (ZRTP_PLATFORM == ZP_WIN32)
    if (g_needs_rewriting || 0 != fopen_s(&cache_file, zrtp->def_cache_path.buffer, "r+")) {
		if (0 != fopen_s(&cache_file, zrtp->def_cache_path.buffer, "w+")) {
			ZRTP_LOG(2,(_ZTU_,"\tERROR! unable to open ZRTP cache file <%s>.\n", zrtp->def_cache_path.buffer));
			return zrtp_status_open_fail;
		}
    }
#else
	if (g_needs_rewriting || !(cache_file = fopen(zrtp->def_cache_path.buffer, "r+"))) {
		cache_file = fopen(zrtp->def_cache_path.buffer, "w+");
		if (!cache_file) {
			ZRTP_LOG(2,(_ZTU_,"\tERROR! unable to open ZRTP cache file <%s>.\n", zrtp->def_cache_path.buffer));
			return zrtp_status_open_fail;
		}
	}
#endif

	fseek(cache_file, 0, SEEK_SET);
	
	/* Store version string first. Format: &ZRTP_DEF_CACHE_VERSION_STR&ZRTP_DEF_CACHE_VERSION_VAL */
	if (1 != fwrite(ZRTP_DEF_CACHE_VERSION_STR, strlen(ZRTP_DEF_CACHE_VERSION_STR), 1, cache_file)) {
		ZRTP_LOG(2,(_ZTU_,"\tERROR! unable to write header to the cache file\n"));
		ZRTP_DOWN_CACHE_RETURN(zrtp_status_write_fail, cache_file);
	}
	if (1 != fwrite(ZRTP_DEF_CACHE_VERSION_VAL, strlen(ZRTP_DEF_CACHE_VERSION_VAL), 1, cache_file)) {
		ZRTP_LOG(2,(_ZTU_,"\tERROR! unable to write header to the cache file\n"));
		ZRTP_DOWN_CACHE_RETURN(zrtp_status_write_fail, cache_file);
	}

    /*
	 * Store PBX secrets first. Format: <secrets count>, <secrets' data>
	 *
	 * NOTE!!! It's IMPORTANT to store PBX secrets before the Regular secrets!!!
	 */
	pos = ftell(cache_file);
	
	count = 0; dirty_count = 0;
	fwrite(&count, sizeof(count), 1, cache_file);
	
	mlist_for_each(node, &mitmcache_head) {
		zrtp_cache_elem_t* elem = mlist_get_struct(zrtp_cache_elem_t, _mlist, node);
		/* Store dirty values only. */
		if (g_needs_rewriting || elem->_is_dirty) {
//			printf("zrtp_cache_user_down: Store MiTM elem index=%u, not modified.\n", elem->_index);
			dirty_count++;
			if (zrtp_status_ok != flush_elem_(elem, cache_file, 1)) {
				ZRTP_DOWN_CACHE_RETURN(zrtp_status_write_fail, cache_file);
			}
		} else {
//			printf("zrtp_cache_user_down: Skip MiTM elem index=%u, not modified.\n", elem->_index);	
		}
	}

	fseek(cache_file, pos, SEEK_SET);
	
	count = zrtp_hton32(g_mitmcache_elems_counter);
	if (fwrite(&count, sizeof(count), 1, cache_file) != 1) {
		ZRTP_DOWN_CACHE_RETURN(zrtp_status_write_fail, cache_file);
	}

	if (dirty_count > 0)
		ZRTP_LOG(3,(_ZTU_,"\t%u out of %u MiTM cache entries have been flushed successfully.\n", dirty_count, zrtp_ntoh32(count)));
	
	/*
	 * Store regular secrets. Format: <secrets count>, <secrets' data>
	 */
		
	/* Seek to the beginning of the Regular secrets block */
	pos = strlen(ZRTP_DEF_CACHE_VERSION_STR)+strlen(ZRTP_DEF_CACHE_VERSION_VAL);
	pos += sizeof(uint32_t); /* Skip MiTM secrets count. */
	pos += (g_mitmcache_elems_counter * ZRTP_MITMCACHE_ELEM_LENGTH); /* Skip MiTM Secrets block */
	
	fseek(cache_file, pos, SEEK_SET);
	
	count = 0; dirty_count=0;
	fwrite(&count, sizeof(count), 1, cache_file);
	
	mlist_for_each(node, &cache_head) {
		zrtp_cache_elem_t* elem = mlist_get_struct(zrtp_cache_elem_t, _mlist, node);
		
		/* Store dirty values only. */
		if (g_needs_rewriting || elem->_is_dirty) {
//			printf("zrtp_cache_user_down: Store RS elem index=%u, not modified.\n", elem->_index);
			dirty_count++;
			if (zrtp_status_ok != flush_elem_(elem, cache_file, 0)) {
				ZRTP_DOWN_CACHE_RETURN(zrtp_status_write_fail, cache_file);
			}
		}
// 		else {
// 		printf("zrtp_cache_user_down: Skip RS elem index=%u, not modified.\n", elem->_index);
//		 }
	}

	fseek(cache_file, pos, SEEK_SET);
	
	count = zrtp_hton32(g_cache_elems_counter);
	if (fwrite(&count, sizeof(count), 1, cache_file) != 1) {
		ZRTP_DOWN_CACHE_RETURN(zrtp_status_write_fail, cache_file);
	}

	if (dirty_count > 0)
		ZRTP_LOG(3,(_ZTU_,"\t%u out of %u regular cache entries have been flushed successfully.\n", dirty_count, zrtp_ntoh32(count)));
	
	g_needs_rewriting = 0;

	ZRTP_DOWN_CACHE_RETURN(zrtp_status_ok, cache_file);	
}


/*==========================================================================*/
/*						Utility  functions.								    */
/* These functions are example how cache can be used for internal needs     */
/*==========================================================================*/


/*----------------------------------------------------------------------------*/
static zrtp_status_t put_name( const zrtp_stringn_t* one_ZID,
							   const zrtp_stringn_t* another_ZID,
							   const zrtp_stringn_t* name,
							   uint8_t is_mitm)
{
    zrtp_cache_elem_t* new_elem = 0;
	zrtp_cache_id_t	id;
	zrtp_status_t s = zrtp_status_ok;

	ZRTP_CACHE_CHECK_ZID(one_ZID, another_ZID);   
	zrtp_cache_create_id(one_ZID, another_ZID, id);
	
	zrtp_mutex_lock(def_cache_protector);
	do {
		new_elem = get_elem(id, is_mitm);
		if (!new_elem) {			
			s = zrtp_status_fail;
			break;
		}

		/* Update regular cache name*/
		new_elem->name_length = ZRTP_MIN(name->length, ZFONE_CACHE_NAME_LENGTH-1);
		zrtp_memset(new_elem->name, 0, sizeof(new_elem->name));
		zrtp_memcpy(new_elem->name, name->buffer, new_elem->name_length);
		
		new_elem->_is_dirty = 1;
	} while (0);
	zrtp_mutex_unlock(def_cache_protector);
	
	if (zrtp->cache_auto_store) zrtp_def_cache_store(zrtp);

	return s;
}


zrtp_status_t zrtp_def_cache_put_name( const zrtp_stringn_t* one_ZID,
									   const zrtp_stringn_t* another_ZID,
									   const zrtp_stringn_t* name)
{
	return put_name(one_ZID, another_ZID, name, 0);
}


/*----------------------------------------------------------------------------*/
static zrtp_status_t get_name( const zrtp_stringn_t* one_ZID,
							   const zrtp_stringn_t* another_ZID,
							   zrtp_stringn_t* name,
							   uint8_t is_mitm)
{
    zrtp_cache_elem_t* new_elem = 0;
	zrtp_cache_id_t	id;
	zrtp_status_t s = zrtp_status_fail;

	ZRTP_CACHE_CHECK_ZID(one_ZID, another_ZID);	
	zrtp_cache_create_id(one_ZID, another_ZID, id);
	
	zrtp_mutex_lock(def_cache_protector);
	do {
		new_elem = get_elem(id, is_mitm);
		if (!new_elem) {			
			s = zrtp_status_fail;
			break;
		}
		
		name->length = new_elem->name_length;
		zrtp_memcpy(name->buffer, new_elem->name, name->length);
		s = zrtp_status_ok;
	} while (0);
	zrtp_mutex_unlock(def_cache_protector);

	return s;
}

zrtp_status_t zrtp_def_cache_get_name( const zrtp_stringn_t* one_zid,
									   const zrtp_stringn_t* another_zid,
									   zrtp_stringn_t* name)
{
	return get_name(one_zid, another_zid, name, 0);
}


/*----------------------------------------------------------------------------*/
zrtp_status_t zrtp_def_cache_get_since( const zrtp_stringn_t* one_ZID,
									    const zrtp_stringn_t* another_ZID,
									    uint32_t* since)
{
    zrtp_cache_elem_t* new_elem = 0;
	zrtp_cache_id_t	id;

	ZRTP_CACHE_CHECK_ZID(one_ZID, another_ZID);	   
	zrtp_cache_create_id(one_ZID, another_ZID, id);

	zrtp_mutex_lock(def_cache_protector);
	new_elem = get_elem(id, 0);
	if (new_elem) {
		*since = new_elem->secure_since;
	}
	zrtp_mutex_unlock(def_cache_protector);
	
	return (new_elem) ? zrtp_status_ok : zrtp_status_fail;
}

zrtp_status_t zrtp_def_cache_reset_since( const zrtp_stringn_t* one_zid,
										  const zrtp_stringn_t* another_zid)
{
	zrtp_cache_elem_t* new_elem = 0;
	zrtp_cache_id_t	id;
	
	ZRTP_CACHE_CHECK_ZID(one_zid, another_zid);	   
	zrtp_cache_create_id(one_zid, another_zid, id);
	
	zrtp_mutex_lock(def_cache_protector);
	new_elem = get_elem(id, 0);
	if (new_elem) {
		new_elem->secure_since = (uint32_t)(zrtp_time_now()/1000);
		
		new_elem->_is_dirty = 1;
	}
	zrtp_mutex_unlock(def_cache_protector);
	
	if (zrtp->cache_auto_store) zrtp_def_cache_store(zrtp);

	return (new_elem) ? zrtp_status_ok : zrtp_status_fail;
}


/*----------------------------------------------------------------------------*/
void zrtp_def_cache_foreach( zrtp_global_t *global,
							 int is_mitm,
							 zrtp_cache_callback_t callback,
							 void *data)
{
	int delete, result;
	unsigned index_decrease = 0;
	mlist_t* node = NULL, *tmp_node = NULL;

	zrtp_mutex_lock(def_cache_protector);
	mlist_for_each_safe(node, tmp_node, (is_mitm ? &mitmcache_head : &cache_head))
    {
		zrtp_cache_elem_t* elem = mlist_get_struct(zrtp_cache_elem_t, _mlist, node);
		
		/*
		 * We are about to delete cache element, in order to keep our
		 * random-access file working, we should re-arrange indexes of
		 * cache elements go after the deleting one.
		 */
		if (index_decrease >0) {	
			elem->_index -= index_decrease;
		}
		
		delete = 0;
		result = callback(elem, is_mitm, data, &delete);
		if (delete) {
			{
			char idstr[24*2+1];
			ZRTP_LOG(3,(_ZTU_,"\trtp_def_cache_foreach() Delete element id=%s index=%u\n",
					hex2str((const char*)elem->id, sizeof(elem->id), idstr, sizeof(idstr)),
					elem->_index));
			}
			
			index_decrease++;
			
			mlist_del(&elem->_mlist);
			
			/* Decrement global cache counter. */
			if (is_mitm)
				g_mitmcache_elems_counter--;
			else
				g_cache_elems_counter--;
				
			g_needs_rewriting = 1;
		}
		if (!result) {
			break;
		}
	}
	zrtp_mutex_unlock(def_cache_protector);
	
	return;
}

/*----------------------------------------------------------------------------*/
zrtp_status_t zrtp_def_cache_store(zrtp_global_t *zrtp)
{
	zrtp_mutex_lock(def_cache_protector);
	zrtp_cache_user_down();
	zrtp_mutex_unlock(def_cache_protector);
	
	return zrtp_status_ok;
}

#endif /* ZRTP_PLATFORM != ZP_WIN32_KERNEL */

#endif /* ZRTP_USE_BUILTIN_CACHE */

/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */

#include "zrtp.h"

#define _ZTU_ "zrtp main"

/*----------------------------------------------------------------------------*/
extern zrtp_status_t zrtp_init_rng(zrtp_global_t* zrtp);
extern void zrtp_down_rng(zrtp_global_t* zrtp);

extern zrtp_status_t zrtp_defaults_sas(zrtp_global_t* global_ctx);
extern zrtp_status_t zrtp_defaults_pkt(zrtp_global_t* global_ctx);
extern zrtp_status_t zrtp_defaults_atl(zrtp_global_t* global_ctx);
extern zrtp_status_t zrtp_defaults_aes_cipher(zrtp_global_t* global_ctx);
extern zrtp_status_t zrtp_defaults_hash(zrtp_global_t* global_ctx);
extern zrtp_status_t zrtp_prepare_pkt();
extern zrtp_status_t zrtp_done_pkt();


void zrtp_config_defaults(zrtp_config_t* config)
{
	zrtp_memset(config, 0, sizeof(zrtp_config_t));
	
	zrtp_memcpy(config->client_id, "ZRTP def. peer", 15);
	config->lic_mode = ZRTP_LICENSE_MODE_PASSIVE;
	
	ZSTR_SET_EMPTY(config->def_cache_path);
	zrtp_zstrncpyc(ZSTR_GV(config->def_cache_path), "./zrtp_def_cache_path.dat", 25);

	config->cache_auto_store = 1; /* cache auto flushing should be enabled by default */

#if (defined(ZRTP_USE_BUILTIN_CACHE) && (ZRTP_USE_BUILTIN_CACHE == 1))
	config->cb.cache_cb.on_init					= zrtp_def_cache_init;
	config->cb.cache_cb.on_down					= zrtp_def_cache_down;
	config->cb.cache_cb.on_put					= zrtp_def_cache_put;
	config->cb.cache_cb.on_put_mitm				= zrtp_def_cache_put_mitm;
	config->cb.cache_cb.on_get					= zrtp_def_cache_get;
	config->cb.cache_cb.on_get_mitm				= zrtp_def_cache_get_mitm;
	config->cb.cache_cb.on_set_verified			= zrtp_def_cache_set_verified;
	config->cb.cache_cb.on_get_verified			= zrtp_def_cache_get_verified;
	config->cb.cache_cb.on_reset_since			= zrtp_def_cache_reset_since;
	config->cb.cache_cb.on_presh_counter_set	= zrtp_def_cache_set_presh_counter;
	config->cb.cache_cb.on_presh_counter_get	= zrtp_def_cache_get_presh_counter;
#endif

#if (defined(ZRTP_USE_BUILTIN_SCEHDULER) && (ZRTP_USE_BUILTIN_SCEHDULER == 1))
	config->cb.sched_cb.on_init					= zrtp_def_scheduler_init;
	config->cb.sched_cb.on_down					= zrtp_def_scheduler_down;
	config->cb.sched_cb.on_call_later			= zrtp_def_scheduler_call_later;
	config->cb.sched_cb.on_cancel_call_later	= zrtp_def_scheduler_cancel_call_later;
	config->cb.sched_cb.on_wait_call_later		= zrtp_def_scheduler_wait_call_later;
#endif
}

zrtp_status_t zrtp_init(zrtp_config_t* config, zrtp_global_t** zrtp)
{
    zrtp_global_t* new_zrtp;
    zrtp_status_t s = zrtp_status_ok;
	
	ZRTP_LOG(3, (_ZTU_,"INITIALIZING LIBZRTP...\n"));
	
	/* Print out configuration setting */
	zrtp_print_env_settings(config);
	
	new_zrtp = zrtp_sys_alloc(sizeof(zrtp_global_t));
	if (!new_zrtp) {
		return zrtp_status_alloc_fail;
    }	
	zrtp_memset(new_zrtp, 0, sizeof(zrtp_global_t));
		
	/*
	 * Apply configuration according to the config
	 */		
	new_zrtp->lic_mode = config->lic_mode;	
	new_zrtp->is_mitm = config->is_mitm;
	ZSTR_SET_EMPTY(new_zrtp->def_cache_path);
	zrtp_zstrcpy(ZSTR_GV(new_zrtp->def_cache_path), ZSTR_GV(config->def_cache_path));
	zrtp_memcpy(&new_zrtp->cb, &config->cb, sizeof(zrtp_callback_t));
	new_zrtp->cache_auto_store = config->cache_auto_store;
        
	ZSTR_SET_EMPTY(new_zrtp->client_id);
	zrtp_memset(new_zrtp->client_id.buffer, ' ', sizeof(zrtp_client_id_t));
	zrtp_zstrncpyc( ZSTR_GV(new_zrtp->client_id),
					(const char*)config->client_id,
					sizeof(zrtp_client_id_t));
	
    /*
	 * General Initialization
	 */
	init_mlist(&new_zrtp->sessions_head);
	
    zrtp_mutex_init(&new_zrtp->sessions_protector);   
	
    init_mlist(&new_zrtp->hash_head);
    init_mlist(&new_zrtp->cipher_head);
    init_mlist(&new_zrtp->atl_head);
    init_mlist(&new_zrtp->pktype_head);
    init_mlist(&new_zrtp->sas_head);

    /* Init RNG context */	
	s = zrtp_init_rng(new_zrtp);
    if (zrtp_status_ok != s) {
		ZRTP_LOG(1, (_ZTU_,"ERROR! zrtp_init_rng() failed:%s.\n", zrtp_log_status2str(s)));
		return zrtp_status_rng_fail;
	}
   	
	/* Initialize SRTP engine */
	s =  zrtp_srtp_init(new_zrtp);
	if (zrtp_status_ok != s) {
		ZRTP_LOG(1, (_ZTU_,"ERROR! zrtp_srtp_init() failed:<%s>\n", zrtp_log_status2str(s)));
		return zrtp_status_fail;
    }    

	if (new_zrtp->cb.cache_cb.on_init)  {
		s = new_zrtp->cb.cache_cb.on_init(new_zrtp);
		if (zrtp_status_ok != s) {
			ZRTP_LOG(1, (_ZTU_,"ERROR! cache on_init() callback failed <%s>\n", zrtp_log_status2str(s)));
			zrtp_srtp_down(new_zrtp);
			return zrtp_status_fail;
		}
	}
	
	if (new_zrtp->cb.sched_cb.on_init)  {
		s = new_zrtp->cb.sched_cb.on_init(new_zrtp);
		if (zrtp_status_ok != s) {
			ZRTP_LOG(1, (_ZTU_,"ERROR! scheduler on_init() callback failed <%s>\n", zrtp_log_status2str(s)));
			zrtp_srtp_down(new_zrtp);
			return zrtp_status_fail;
		}
	}
	
	/* Load default crypto-components */
    zrtp_prepare_pkt(new_zrtp);
    zrtp_defaults_sas(new_zrtp);
    zrtp_defaults_pkt(new_zrtp);
    zrtp_defaults_atl(new_zrtp);
    zrtp_defaults_aes_cipher(new_zrtp);
    zrtp_defaults_hash(new_zrtp);

	*zrtp = new_zrtp;
	
	ZRTP_LOG(3, (_ZTU_,"INITIALIZING LIBZRTP - DONE\n"));
    return  s;
}


/*----------------------------------------------------------------------------*/
zrtp_status_t zrtp_down(zrtp_global_t* zrtp)
{
	ZRTP_LOG(3, (_ZTU_,"DESTROYING LIBZRTP...\n"));
	
    if (!zrtp) {
		return zrtp_status_bad_param;
    }

    zrtp_comp_done(ZRTP_CC_HASH, zrtp);
    zrtp_comp_done(ZRTP_CC_SAS, zrtp);
    zrtp_comp_done(ZRTP_CC_CIPHER, zrtp);
    zrtp_comp_done(ZRTP_CC_PKT, zrtp);
    zrtp_comp_done(ZRTP_CC_ATL, zrtp);
    zrtp_done_pkt(zrtp);
    
    zrtp_mutex_destroy(zrtp->sessions_protector);	
	
	zrtp_srtp_down(zrtp);
	
	if (zrtp->cb.cache_cb.on_down) {
		zrtp->cb.cache_cb.on_down();
	}
	if (zrtp->cb.sched_cb.on_down) {
		zrtp->cb.sched_cb.on_down();
	}
	
	zrtp_down_rng(zrtp);

	zrtp_sys_free(zrtp);
	
	ZRTP_LOG(3, (_ZTU_,"DESTROYING LIBZRTP - DONE\n"));

    return zrtp_status_ok;
}

/*----------------------------------------------------------------------------*/
zrtp_status_t zrtp_session_init( zrtp_global_t* zrtp,
								zrtp_profile_t* profile,
								zrtp_zid_t zid,
								zrtp_signaling_role_t role,
								zrtp_session_t **session)
{
    uint32_t i = 0;
	zrtp_status_t s = zrtp_status_fail;
	zrtp_session_t* new_session = NULL;
        
    if (!zrtp) {
    	return zrtp_status_bad_param;
    }
	
	new_session = zrtp_sys_alloc(sizeof(zrtp_session_t));
	if (!new_session) {
		return zrtp_status_alloc_fail;		
	}
    
    zrtp_memset(new_session, 0, sizeof(zrtp_session_t));
	new_session->id = zrtp->sessions_count++;
	
	{
		zrtp_uchar32_t buff;
		ZRTP_LOG(3, (_ZTU_,"START SESSION INITIALIZATION. sID=%u.\n", new_session->id));
		ZRTP_LOG(3, (_ZTU_,"ZID=%s.\n", hex2str((const char*)zid, sizeof(zrtp_uchar12_t), (char*)buff, sizeof(buff)) ));
	}
	
	do {	
	/*
	 * Apply profile for the stream context: set flags and prepare Hello packet.
	 * If profile structure isn't provided, generate default.
	 */	 
    if (!profile) {
		ZRTP_LOG(1, (_ZTU_,"Profile in NULL - loading default one.\n"));
		zrtp_profile_defaults(&new_session->profile, zrtp);		
    } else {
		ZRTP_LOG(1, (_ZTU_,"Loading User's profile:\n"));
		if (zrtp_status_ok != zrtp_profile_check(profile, zrtp)) {
			ZRTP_LOG(1, (_ZTU_,"ERROR! Can't apply wrong profile to the session sID=%u.\n", new_session->id));
			break;
		}
		
		/* Adjust user's settings: force SHA-384 hash for ECDH-384P */
		if (zrtp_profile_find(profile, ZRTP_CC_PKT, ZRTP_PKTYPE_EC384P) > 0) {
			ZRTP_LOG(3, (_ZTU_,"User wants ECDH384 - auto-adjust profile to use SHA-384.\n"));
			profile->hash_schemes[0] = ZRTP_HASH_SHA384;
			profile->hash_schemes[1] = ZRTP_HASH_SHA256;
			profile->hash_schemes[2] = 0;
		}		
		
		zrtp_memcpy(&new_session->profile, profile, sizeof(zrtp_profile_t));
		
		{
		int i;
		ZRTP_LOG(3, (_ZTU_,"   allowclear: %s\n", profile->allowclear?"ON":"OFF"));
		ZRTP_LOG(3, (_ZTU_,"   autosecure: %s\n", profile->autosecure?"ON":"OFF"));
		ZRTP_LOG(3, (_ZTU_," disclose_bit: %s\n", profile->disclose_bit?"ON":"OFF"));
		ZRTP_LOG(3, (_ZTU_," signal. role: %s\n", zrtp_log_sign_role2str(role)));	
		ZRTP_LOG(3, (_ZTU_,"          TTL: %u\n", profile->cache_ttl));
				
		ZRTP_LOG(3, (_ZTU_,"  SAS schemes: "));
		i=0;
		while (profile->sas_schemes[i]) {
			ZRTP_LOGC(3, ("%.4s ", zrtp_comp_id2type(ZRTP_CC_SAS, profile->sas_schemes[i++])));
		}
		ZRTP_LOGC(3, ("\n")); ZRTP_LOG(1, (_ZTU_,"     Ciphers: "));
		i=0;
		while (profile->cipher_types[i]) {
			ZRTP_LOGC(3, ("%.4s ", zrtp_comp_id2type(ZRTP_CC_CIPHER, profile->cipher_types[i++])));
		}
		ZRTP_LOGC(3, ("\n")); ZRTP_LOG(1, (_ZTU_,"   PK schemes: "));
		i=0;
		while (profile->pk_schemes[i]) {
			ZRTP_LOGC(3, ("%.4s ", zrtp_comp_id2type(ZRTP_CC_PKT, profile->pk_schemes[i++])));
		}
		ZRTP_LOGC(3, ("\n")); ZRTP_LOG(1, (_ZTU_,"          ATL: "));
		i=0;
		while (profile->auth_tag_lens[i]) {
			ZRTP_LOGC(3, ("%.4s ", zrtp_comp_id2type(ZRTP_CC_ATL, profile->auth_tag_lens[i++])));
		}
		ZRTP_LOGC(3, ("\n")); ZRTP_LOG(1, (_ZTU_,"      Hashes: "));
		i=0;
		while (profile->hash_schemes[i]) {
			ZRTP_LOGC(3, ("%.4s ", zrtp_comp_id2type(ZRTP_CC_HASH, profile->hash_schemes[i++])));
		}
		ZRTP_LOGC(3, ("\n"));
		}
	}

	/* Set ZIDs */
	ZSTR_SET_EMPTY(new_session->zid);
    ZSTR_SET_EMPTY(new_session->peer_zid);
	zrtp_zstrncpyc(ZSTR_GV(new_session->zid), (const char*)zid, sizeof(zrtp_zid_t));	

	new_session->zrtp = zrtp;
	new_session->signaling_role = role;
	new_session->mitm_alert_detected = 0;

	/*
	 * Allocate memory for holding secrets and initialize with random values.
	 * Actual values will be written from the cache at the beginning of the protocol.
	 */
	new_session->secrets.rs1 = _zrtp_alloc_shared_secret(new_session);
	new_session->secrets.rs2 = _zrtp_alloc_shared_secret(new_session);	
	new_session->secrets.auxs = _zrtp_alloc_shared_secret(new_session);
	new_session->secrets.pbxs = _zrtp_alloc_shared_secret(new_session);

	if ( !new_session->secrets.rs1 || !new_session->secrets.rs2 ||
		 !new_session->secrets.auxs || !new_session->secrets.pbxs) {
		ZRTP_LOG(1, (_ZTU_,"ERROR! Can't allocate shared secrets sID=%u\n.", new_session->id));
		s = zrtp_status_alloc_fail;
		break;
	}

	/* Initialize SAS values */	
	ZSTR_SET_EMPTY(new_session->sas1);
	ZSTR_SET_EMPTY(new_session->sas2);
	ZSTR_SET_EMPTY(new_session->sasbin);
	ZSTR_SET_EMPTY(new_session->zrtpsess);
    
    /* Clear all stream structures */
    for (i=0; i<ZRTP_MAX_STREAMS_PER_SESSION ; i++) {
		new_session->streams[i].state		= ZRTP_STATE_NONE;
		new_session->streams[i].prev_state	= ZRTP_STATE_NONE;
		new_session->streams[i].mode		= ZRTP_STREAM_MODE_UNKN;
    }
        
    /* Initialize synchronization objects */
	s = zrtp_mutex_init(&new_session->streams_protector);
    if (zrtp_status_ok != s) {
		ZRTP_LOG(1, (_ZTU_,"ERROR! can't initialize Stream protector. sID=%u.\n", new_session->id));
		break;
	}	
	s = zrtp_mutex_init(&new_session->init_protector);
    if (zrtp_status_ok != s) {
		ZRTP_LOG(1, (_ZTU_,"ERROR! can't initialize Init protector. sID=%u.\n", new_session->id));
		break;
	}		
	
	s = zrtp_status_ok;
	} while (0);
	
	if (zrtp_status_ok != s) {
		zrtp_sys_free(new_session);
		return s;
	}

    /* Add new session to the global list */    
    zrtp_mutex_lock(zrtp->sessions_protector);
    mlist_add(&zrtp->sessions_head, &new_session->_mlist);
    zrtp_mutex_unlock(zrtp->sessions_protector);
    
	*session = new_session;
	
    ZRTP_LOG(3, (_ZTU_,"Session initialization - DONE. sID=%u.\n\n", new_session->id));

    return zrtp_status_ok;
}

/*----------------------------------------------------------------------------*/
void zrtp_session_down(zrtp_session_t *session)
{
	int i =0;
	
    if (!session) {
		return;
	}		

	/* Stop ZRTP engine and clear all crypto sources for every stream in the session. */
	zrtp_mutex_lock(session->streams_protector);
	for(i=0; i<ZRTP_MAX_STREAMS_PER_SESSION; i++) {
		zrtp_stream_t *the_stream = &session->streams[i]; 		
		zrtp_stream_stop(the_stream);
	}
	zrtp_mutex_unlock(session->streams_protector);

	/* Release memory allocated on initialization */
	if (session->secrets.rs1) {
		zrtp_sys_free(session->secrets.rs1);
	}
	if (session->secrets.rs2) {
		zrtp_sys_free(session->secrets.rs2);
	}
	if (session->secrets.auxs) {
		zrtp_sys_free(session->secrets.auxs);
	}
	if (session->secrets.pbxs) {
		zrtp_sys_free(session->secrets.pbxs);
	}

	/* We don't need the session key anymore - clear it */
	zrtp_wipe_zstring(ZSTR_GV(session->zrtpsess));

	/* Removing session from the global list */    
	zrtp_mutex_lock(session->zrtp->sessions_protector);
	mlist_del(&session->_mlist);
	zrtp_mutex_unlock(session->zrtp->sessions_protector);		
	
	zrtp_mutex_destroy(session->streams_protector);
	zrtp_mutex_destroy(session->init_protector);
	
	zrtp_sys_free(session);
}

/*----------------------------------------------------------------------------*/
zrtp_status_t zrtp_stream_attach(zrtp_session_t *session, zrtp_stream_t** stream)
{
    uint32_t i = 0;
	zrtp_status_t s = zrtp_status_fail;
    zrtp_stream_t* new_stream = NULL;	
    
	ZRTP_LOG(3, (_ZTU_,"ATTACH NEW STREAM to sID=%d:\n", session->id));
	
	/*
	 * Initialize first unused stream. If there are no available streams return error.
	 */
    zrtp_mutex_lock(session->streams_protector);
    for (i=0; i<ZRTP_MAX_STREAMS_PER_SESSION; i++) {
		if (ZRTP_STATE_NONE == session->streams[i].state) {
			new_stream = &session->streams[i];
			zrtp_memset(new_stream, 0, sizeof(zrtp_stream_t));
			break;
 		}
    }
	zrtp_mutex_unlock(session->streams_protector);

	if (!new_stream) {
		ZRTP_LOG(1, (_ZTU_,"\tWARNING! Can't attach one more stream. Limit is reached."
					 " Use #ZRTP_MAX_STREAMS_PER_SESSION. sID=%u\n", session->id));
		return zrtp_status_alloc_fail;
	}
	
	/*
	 * Initialize the private data stream with default initial values	 
	 */
	zrtp_mutex_init(&new_stream->stream_protector);
	_zrtp_change_state(new_stream, ZRTP_STATE_ACTIVE);
	new_stream->mode	= ZRTP_STREAM_MODE_CLEAR;
	new_stream->id		= session->zrtp->streams_count++;
	new_stream->session = session;
	new_stream->zrtp	= session->zrtp;
	new_stream->mitm_mode = ZRTP_MITM_MODE_UNKN;
	new_stream->is_hello_received = 0;
	
	ZSTR_SET_EMPTY(new_stream->cc.hmackey);
	ZSTR_SET_EMPTY(new_stream->cc.peer_hmackey);
	ZSTR_SET_EMPTY(new_stream->cc.zrtp_key);
	ZSTR_SET_EMPTY(new_stream->cc.peer_zrtp_key);

	new_stream->dh_cc.initialized_with	= ZRTP_COMP_UNKN;
	bnBegin(&new_stream->dh_cc.peer_pv);
	ZSTR_SET_EMPTY(new_stream->dh_cc.dhss);		
	
	ZRTP_LOG(3, (_ZTU_,"\tEmpty slot was found - initializing new stream with ID=%u.\n", new_stream->id));

	do {
	zrtp_string32_t hash_buff = ZSTR_INIT_EMPTY(hash_buff);
	zrtp_hash_t *hash = zrtp_comp_find(ZRTP_CC_HASH, ZRTP_HASH_SHA256, new_stream->zrtp);		
	s = zrtp_status_algo_fail;
		
	if (sizeof(uint16_t) !=  zrtp_randstr( new_stream->zrtp,
										  (uint8_t*)&new_stream->media_ctx.high_out_zrtp_seq,
										  sizeof(uint16_t))) {
		break;
	}	

	/*
	 * Compute and store message hashes to prevent DoS attacks.
	 * Generate H0 as a random nonce and compute H1, H2 and H3
	 * using the leftmost 128 bits from every hash.
	 * Then insert these directly into the message structures.
     */

	zrtp_memset(&new_stream->messages, 0, sizeof(new_stream->messages));
	ZSTR_SET_EMPTY(new_stream->messages.h0);
	ZSTR_SET_EMPTY(new_stream->messages.signaling_hash);

	/* Generate Random nonce, compute H1 and store in the DH packet */
	new_stream->messages.h0.length = (uint16_t)zrtp_randstr( new_stream->zrtp,
															 (unsigned char*)new_stream->messages.h0.buffer,
															 ZRTP_MESSAGE_HASH_SIZE);
	if (ZRTP_MESSAGE_HASH_SIZE != new_stream->messages.h0.length) {		
		break;
	}

	s = hash->hash(hash, ZSTR_GV(new_stream->messages.h0), ZSTR_GV(hash_buff));
	if (zrtp_status_ok != s) {
		break;
	}
	zrtp_memcpy(new_stream->messages.dhpart.hash, hash_buff.buffer, ZRTP_MESSAGE_HASH_SIZE);	

	/* Compute H2 for the Commit */		
	s = hash->hash_c(hash, (char*)new_stream->messages.dhpart.hash, ZRTP_MESSAGE_HASH_SIZE, ZSTR_GV(hash_buff));
	if (zrtp_status_ok != s) {
		break;
	}
	zrtp_memcpy(new_stream->messages.commit.hash, hash_buff.buffer, ZRTP_MESSAGE_HASH_SIZE);	

	/* Compute H3 for the Hello message */
	s = hash->hash_c(hash, (char*)new_stream->messages.commit.hash, ZRTP_MESSAGE_HASH_SIZE, ZSTR_GV(hash_buff));
	if (zrtp_status_ok != s) {
		break;
	}
	zrtp_memcpy(new_stream->messages.hello.hash, hash_buff.buffer, ZRTP_MESSAGE_HASH_SIZE);
	
	s = zrtp_status_ok;
	} while (0);
	
	if (zrtp_status_ok != s) {
		ZRTP_LOG(1, (_ZTU_,"\tERROR! Fail to compute messages hashes <%s>.\n", zrtp_log_status2str(s)));
		return s;
	}
	
    /*
	 * Preparing HELLO based on user's profile
	 */
	ZRTP_LOG(3, (_ZTU_,"\tPreparing ZRTP Hello according to the Session profile.\n"));
	{
	zrtp_packet_Hello_t* hello = &new_stream->messages.hello;	
	uint8_t i = 0;
	int8_t* comp_ptr = NULL;

	/* Set Protocol Version and ClientID */
	zrtp_memcpy(hello->version, ZRTP_PROTOCOL_VERSION, ZRTP_VERSION_SIZE);
	zrtp_memcpy(hello->cliend_id, session->zrtp->client_id.buffer, session->zrtp->client_id.length);
		
	/* Set flags. */
	hello->pasive	=  (ZRTP_LICENSE_MODE_PASSIVE == session->zrtp->lic_mode) ? 1 : 0;
	hello->uflag	= (ZRTP_LICENSE_MODE_UNLIMITED == session->zrtp->lic_mode) ? 1 : 0;
	hello->mitmflag = session->zrtp->is_mitm;	
	hello->sigflag	= 0;	
		
	zrtp_memcpy(hello->zid, session->zid.buffer, session->zid.length);
	
	comp_ptr = (int8_t*)hello->comp;
	i = 0;
	while ( session->profile.hash_schemes[i]) {
		zrtp_memcpy( comp_ptr,
					 zrtp_comp_id2type(ZRTP_CC_HASH, session->profile.hash_schemes[i++]),
					 ZRTP_COMP_TYPE_SIZE );
		comp_ptr += ZRTP_COMP_TYPE_SIZE;
	}
	hello->hc = i;	

	i = 0;
	while (session->profile.cipher_types[i]) {
		zrtp_memcpy( comp_ptr,
					 zrtp_comp_id2type(ZRTP_CC_CIPHER, session->profile.cipher_types[i++]),
					 ZRTP_COMP_TYPE_SIZE );
		comp_ptr += ZRTP_COMP_TYPE_SIZE;
	}
	hello->cc = i;

	i = 0;
	while (session->profile.auth_tag_lens[i] ) {
		zrtp_memcpy( comp_ptr,
					 zrtp_comp_id2type(ZRTP_CC_ATL, session->profile.auth_tag_lens[i++]),
					 ZRTP_COMP_TYPE_SIZE );
		comp_ptr += ZRTP_COMP_TYPE_SIZE;
	}
	hello->ac = i;

	i = 0;
	while (session->profile.pk_schemes[i] ) {
		zrtp_memcpy( comp_ptr,
					 zrtp_comp_id2type(ZRTP_CC_PKT, session->profile.pk_schemes[i++]),
					 ZRTP_COMP_TYPE_SIZE );
		comp_ptr += ZRTP_COMP_TYPE_SIZE;
	}
	hello->kc = i;

	i = 0;
	while (session->profile.sas_schemes[i]) {
		zrtp_memcpy( comp_ptr,
					zrtp_comp_id2type(ZRTP_CC_SAS, session->profile.sas_schemes[i++]),
					ZRTP_COMP_TYPE_SIZE );
		comp_ptr += ZRTP_COMP_TYPE_SIZE;
	}
	hello->sc = i;

	/*
	 * Hmac will appear at the end of the message, after the dynamic portion.
	 * i is the length of the dynamic part.
	 */	
	i = (hello->hc + hello->cc + hello->ac + hello->kc + hello->sc) * ZRTP_COMP_TYPE_SIZE;
	_zrtp_packet_fill_msg_hdr( new_stream,
							   ZRTP_HELLO,
							   ZRTP_HELLO_STATIC_SIZE + i + ZRTP_HMAC_SIZE,
							   &hello->hdr);
	}
	
	*stream = new_stream;
	
	ZRTP_LOG(3, (_ZTU_,"ATTACH NEW STREAM - DONE.\n"));
    return zrtp_status_ok;
}

/*---------------------------------------------------------------------------*/
zrtp_status_t zrtp_signaling_hash_get( zrtp_stream_t* stream,
									   char *hash_buff,
									   uint32_t hash_buff_length)
{	
	zrtp_string32_t hash_str = ZSTR_INIT_EMPTY(hash_str);
	zrtp_hash_t *hash = NULL;

	if (!stream || !hash_buff) {
		return zrtp_status_bad_param;
	}

	if (ZRTP_SIGN_ZRTP_HASH_LENGTH > hash_buff_length) {
		return zrtp_status_buffer_size;
	}

	if (stream->state < ZRTP_STATE_ACTIVE) {
		return zrtp_status_wrong_state;
	}

	hash = zrtp_comp_find(ZRTP_CC_HASH, ZRTP_HASH_SHA256, stream->zrtp);
	hash->hash_c( hash,
		 	     (const char*)&stream->messages.hello.hdr,
				  zrtp_ntoh16(stream->messages.hello.hdr.length) * 4,
				  ZSTR_GV(hash_str) );

	hex2str(hash_str.buffer, ZRTP_MESSAGE_HASH_SIZE, hash_buff, hash_buff_length);
	
	return zrtp_status_ok;	
}

zrtp_status_t zrtp_signaling_hash_set( zrtp_stream_t* ctx,
									   const char *hash_buff,
									   uint32_t hash_buff_length)
{
	if (!ctx || !hash_buff) {
		return zrtp_status_bad_param;
	}

	if (ZRTP_SIGN_ZRTP_HASH_LENGTH > hash_buff_length) {
		return zrtp_status_buffer_size;
	}

	if (ctx->state != ZRTP_STATE_ACTIVE) {
		return zrtp_status_wrong_state;
	}
	
	str2hex(hash_buff,
			ZRTP_SIGN_ZRTP_HASH_LENGTH,
			ctx->messages.signaling_hash.buffer,
			ctx->messages.signaling_hash.max_length);
	ctx->messages.signaling_hash.length = ZRTP_MESSAGE_HASH_SIZE;
	
	ZRTP_LOG(3, (_ZTU_,"SIGNALLING HAS was ADDED for the comparison. ID=%u\n", ctx->id));
	ZRTP_LOG(3, (_ZTU_,"Hash=%.*s.\n", ZRTP_SIGN_ZRTP_HASH_LENGTH, hash_buff));

	return zrtp_status_ok;
}


/*----------------------------------------------------------------------------*/
static const char* zrtp_pkt2str[] = {
	"Preshared",
	"Multistream",
	"DH-2048",
	"ECDH-256",
	"DH-3072",
	"ECDH-384",
	"ECDH-521",
	"DH-4096"
};

static const char* zrtp_hash2str[] = {
	"SHA-256",
	"SHA1",
	"SHA-384"
};

static const char* zrtp_cipher2str[] = {
	"AES-128",
	"AES-256"
};

static const char* zrtp_atl2str[] = {
	"HMAC-SHA1 32 bit",
	"HMAC-SHA1 80 bit"
};

static const char* zrtp_sas2str[] = {
	"Base-32",
	"Base-256"
};

zrtp_status_t zrtp_session_get(zrtp_session_t *session, zrtp_session_info_t *info)
{
	int i=0;
	if (!session || !info) {
		return zrtp_status_bad_param;
	}
	
	zrtp_memset(info, 0, sizeof(zrtp_session_info_t));
	
	ZSTR_SET_EMPTY(info->peer_clientid);
	ZSTR_SET_EMPTY(info->peer_version);
	ZSTR_SET_EMPTY(info->zid);
	ZSTR_SET_EMPTY(info->peer_zid);	
	ZSTR_SET_EMPTY(info->sas1);
	ZSTR_SET_EMPTY(info->sasbin);
	ZSTR_SET_EMPTY(info->sas2);
	ZSTR_SET_EMPTY(info->auth_name);
	ZSTR_SET_EMPTY(info->cipher_name);
	ZSTR_SET_EMPTY(info->hash_name);
	ZSTR_SET_EMPTY(info->sas_name);
	ZSTR_SET_EMPTY(info->pk_name);
	
	info->id = session->id;
	zrtp_zstrcpy(ZSTR_GV(info->zid), ZSTR_GV(session->zid));
	zrtp_zstrcpy(ZSTR_GV(info->peer_zid), ZSTR_GV(session->peer_zid));
	
	for (i=0; i<ZRTP_MAX_STREAMS_PER_SESSION; i++) {
		zrtp_stream_t* full_stream = &session->streams[i];
		if ((full_stream->state > ZRTP_STATE_ACTIVE) && !ZRTP_IS_STREAM_FAST(full_stream))
		{
			zrtp_zstrcpyc(ZSTR_GV(info->pk_name), zrtp_pkt2str[full_stream->pubkeyscheme->base.id-1]);
			
			zrtp_zstrncpyc( ZSTR_GV(info->peer_clientid),
						   (const char*)full_stream->messages.peer_hello.cliend_id, 16);
			zrtp_zstrncpyc( ZSTR_GV(info->peer_version),
						   (const char*)full_stream->messages.peer_hello.version, 4);
			
			info->secrets_ttl = full_stream->cache_ttl;
		}
	}
	
	info->sas_is_ready = (session->zrtpsess.length > 0) ? 1 : 0;
	if (info->sas_is_ready) {
		zrtp_zstrcpy(ZSTR_GV(info->sas1), ZSTR_GV(session->sas1));
		zrtp_zstrcpy(ZSTR_GV(info->sas2), ZSTR_GV(session->sas2));
		zrtp_zstrcpy(ZSTR_GV(info->sasbin), ZSTR_GV(session->sasbin));
		info->sas_is_base256 = (ZRTP_SAS_BASE256 == session->sasscheme->base.id);
		
		info->sas_is_verified = 0;
		if (session->zrtp->cb.cache_cb.on_get_verified) {
			session->zrtp->cb.cache_cb.on_get_verified( ZSTR_GV(session->zid),
													    ZSTR_GV(session->peer_zid),
													    &info->sas_is_verified);
		}

		zrtp_zstrcpyc(ZSTR_GV(info->hash_name), zrtp_hash2str[session->hash->base.id-1]);
		zrtp_zstrcpyc(ZSTR_GV(info->cipher_name), zrtp_cipher2str[session->blockcipher->base.id-1]);
		zrtp_zstrcpyc(ZSTR_GV(info->auth_name), zrtp_atl2str[session->authtaglength->base.id-1]);
		zrtp_zstrcpyc(ZSTR_GV(info->sas_name), zrtp_sas2str[session->sasscheme->base.id-1]);
		
		info->cached_flags	= session->secrets.cached_curr;
		info->matches_flags= session->secrets.matches_curr;
		info->wrongs_flags	= session->secrets.wrongs_curr;
	}
	
	return zrtp_status_ok;
}

zrtp_status_t zrtp_stream_get(zrtp_stream_t *stream, zrtp_stream_info_t *info)
{
	if (!stream || !info) {
		return zrtp_status_bad_param;
	}
	
	zrtp_memset(info, 0, sizeof(zrtp_stream_info_t));
	
	info->id			= stream->id;
	info->state			= stream->state;
	info->mode			= stream->mode;
	info->mitm_mode		= stream->mitm_mode;
	
	if (stream->state > ZRTP_STATE_ACTIVE) {
		info->last_error	= stream->last_error;
		info->peer_passive	= stream->peer_passive;
		info->res_allowclear= stream->allowclear;
		info->peer_disclose	= stream->peer_disclose_bit;
		info->peer_mitm		= stream->peer_mitm_flag;
	}
	
	return zrtp_status_ok;
}

void zrtp_session_set_userdata(zrtp_session_t *session, void* udata) {
	session->usr_data = udata;
}
void* zrtp_session_get_userdata(zrtp_session_t *session) {
	return session->usr_data;
}

void zrtp_stream_set_userdata(zrtp_stream_t *stream, void* udata) {
	stream->usr_data = udata;
}
void* zrtp_stream_get_userdata(const zrtp_stream_t *stream) {
	return stream->usr_data;
}

/*----------------------------------------------------------------------------*/
void zrtp_profile_defaults(zrtp_profile_t* profile, zrtp_global_t* zrtp)
{   
	zrtp_memset(profile, 0, sizeof(zrtp_profile_t));

	profile->autosecure			= 1;	
	profile->allowclear			= 0;
	profile->discovery_optimization = 1;
	profile->cache_ttl			= ZRTP_CACHE_DEFAULT_TTL;

	profile->sas_schemes[0]		= ZRTP_SAS_BASE256;
	profile->sas_schemes[1]		= ZRTP_SAS_BASE32;	
	profile->cipher_types[0]	= ZRTP_CIPHER_AES256;
	profile->cipher_types[1]	= ZRTP_CIPHER_AES128;
	profile->auth_tag_lens[0]	= ZRTP_ATL_HS32;
	profile->auth_tag_lens[1]   = ZRTP_ATL_HS80;
	profile->hash_schemes[0]	= ZRTP_HASH_SHA256;

	if (zrtp && (ZRTP_LICENSE_MODE_PASSIVE == zrtp->lic_mode)) {
		profile->pk_schemes[0]		= ZRTP_PKTYPE_DH2048;
		profile->pk_schemes[1]		= ZRTP_PKTYPE_EC256P;
		profile->pk_schemes[2]		= ZRTP_PKTYPE_DH3072;
	} else {
		profile->pk_schemes[0]		= ZRTP_PKTYPE_EC256P;
		profile->pk_schemes[1]		= ZRTP_PKTYPE_DH3072;
		profile->pk_schemes[2]		= ZRTP_PKTYPE_DH2048;
	}
	profile->pk_schemes[3]		= ZRTP_PKTYPE_MULT;
}

/*----------------------------------------------------------------------------*/
zrtp_status_t zrtp_profile_check(const zrtp_profile_t* profile, zrtp_global_t* zrtp)
{
    uint8_t i = 0;
	
	if (!profile || !zrtp) {
		return zrtp_status_bad_param;
	}
	
    /*
     * Fail if the required base components are not present in the profile.
     */
    if (0 > zrtp_profile_find(profile, ZRTP_CC_HASH, ZRTP_HASH_SHA256)) {
		ZRTP_LOG(1, (_ZTU_,"WARNING! can't find 'SHA256  ' in profile.\n"));
        return zrtp_status_fail;
    }
     
    if (0 > zrtp_profile_find(profile, ZRTP_CC_SAS, ZRTP_SAS_BASE32)) {
        ZRTP_LOG(1, (_ZTU_,"WARNING! can't find 'base32' in profile.\n"));
        return zrtp_status_fail;
    }
    
    if (0 > zrtp_profile_find(profile, ZRTP_CC_CIPHER, ZRTP_CIPHER_AES128)) {
        ZRTP_LOG(1, (_ZTU_,"WARNING! can't find 'AES1287  ' in profile.\n"));
        return zrtp_status_fail;
    }
     
    if (0 > zrtp_profile_find(profile, ZRTP_CC_PKT, ZRTP_PKTYPE_DH3072)) {
        ZRTP_LOG(1, (_ZTU_,"WARNING! can't find 'DH3K' in profile.\n"));
        return zrtp_status_fail;
    }

	if (0 > zrtp_profile_find(profile, ZRTP_CC_PKT, ZRTP_PKTYPE_MULT)) {
        ZRTP_LOG(1, (_ZTU_,"WARNING! can't find 'Mult' in profile.\n"));
        return zrtp_status_fail;
    }
     
    if (0 > zrtp_profile_find(profile, ZRTP_CC_ATL, ZRTP_ATL_HS32)) {
        ZRTP_LOG(1, (_ZTU_,"WARNING! can't find '32      ' in profile.\n"));
        return zrtp_status_fail;
    }

	if (0 > zrtp_profile_find(profile, ZRTP_CC_ATL, ZRTP_ATL_HS80)) {
        ZRTP_LOG(1, (_ZTU_,"WARNING! can't find '80      ' in profile.\n"));
        return zrtp_status_fail;
    }

    /*
     * Check that each component in the profile is in the global set of components.
     */
	i = 0;
	while (profile->sas_schemes[i]) {
		if (!zrtp_comp_find(ZRTP_CC_SAS, profile->sas_schemes[i++], zrtp)) {
    		return zrtp_status_fail;
		}
	}
    
	i = 0;
    while (profile->cipher_types[i]) {
		if (!zrtp_comp_find( ZRTP_CC_CIPHER, profile->cipher_types[i++], zrtp)) {
    	    return zrtp_status_fail;
		}
    }
    
	i = 0;
	while (profile->pk_schemes[i]) {
		if (!zrtp_comp_find(ZRTP_CC_PKT, profile->pk_schemes[i++], zrtp)) {
				return zrtp_status_fail;
		}
    }
    
	i = 0;
    while (profile->auth_tag_lens[i]) {
		if (!zrtp_comp_find(ZRTP_CC_ATL, profile->auth_tag_lens[i++], zrtp)) {
    	    return zrtp_status_fail;
		}
    }
    
	i = 0;
    while (profile->hash_schemes[i]) {
		if (!zrtp_comp_find(ZRTP_CC_HASH, profile->hash_schemes[i++], zrtp)) {
    	    return zrtp_status_fail;
		}
	}
	
	/* Can't use Preshared with No cahce */
	if (NULL == zrtp->cb.cache_cb.on_get) {
		i = 0;
		while (profile->pk_schemes[i]) {
			if (ZRTP_PKTYPE_PRESH == profile->pk_schemes[i++]) {
				ZRTP_LOG(1, (_ZTU_,"WARNING! can't use Preshared PK with no cache.\n"));	
				return zrtp_status_fail;
			}
		}	
	}
	
    return zrtp_status_ok;
}


/*----------------------------------------------------------------------------*/
int zrtp_profile_find(const zrtp_profile_t* profile, zrtp_crypto_comp_t type, uint8_t id)

{
	uint8_t* prof_elem = NULL;    
    unsigned int i = 0;
        
    if (!profile || !id) {
		return -1;
    }

    switch (type)
    {
	case ZRTP_CC_HASH:
		prof_elem = (uint8_t*)profile->hash_schemes;
		break;
	case ZRTP_CC_SAS:
		prof_elem = (uint8_t*)profile->sas_schemes;
		break;
	case ZRTP_CC_CIPHER:
		prof_elem = (uint8_t*)profile->cipher_types;
		break;
	case ZRTP_CC_PKT:
		prof_elem = (uint8_t*)profile->pk_schemes;
		break;
	case ZRTP_CC_ATL:
		prof_elem = (uint8_t*)profile->auth_tag_lens;
		break;
	default:
		return -1;
    }
    

	i = 0;
	while ( prof_elem[i] ) {
		if (id == prof_elem[i++]) return i;
    }
    
    return -1;
}


/*============================================================================*/
/*  ZRTP components management part											  */
/*============================================================================*/


/*----------------------------------------------------------------------------*/
#define DESTROY_COMP(mac_node, mac_tmp, mac_type, mac_head, mac_comp)\
{ \
    mac_node = mac_tmp = NULL;\
    mac_comp = NULL;\
    mlist_for_each_safe(mac_node, mac_tmp, mac_head) \
    {\
	mac_comp = (zrtp_comp_t*) mlist_get_struct(mac_type, mlist, mac_node); \
	if (mac_comp->free)\
		mac_comp->free((mac_type*)mac_comp);\
	mlist_del(mac_node);\
	zrtp_sys_free(mac_comp);\
    } \
}break

zrtp_status_t zrtp_comp_done(zrtp_crypto_comp_t type, zrtp_global_t* zrtp)
{
    mlist_t* node = NULL;
    mlist_t* tmp = NULL;
    zrtp_comp_t* comp = NULL;

    switch (type)
    {
	case ZRTP_CC_HASH:
	    DESTROY_COMP(node, tmp, zrtp_hash_t, &zrtp->hash_head, comp);
	case ZRTP_CC_SAS:
	    DESTROY_COMP(node, tmp, zrtp_sas_scheme_t, &zrtp->sas_head, comp);
	case ZRTP_CC_CIPHER:
	    DESTROY_COMP(node, tmp, zrtp_cipher_t, &zrtp->cipher_head, comp);
	case ZRTP_CC_PKT:
	    DESTROY_COMP(node, tmp, zrtp_pk_scheme_t, &zrtp->pktype_head, comp);
	case ZRTP_CC_ATL:
	    DESTROY_COMP(node, tmp, zrtp_auth_tag_length_t, &zrtp->atl_head, comp);
	default:
		break;
    }

    return zrtp_status_ok;
}

/*----------------------------------------------------------------------------*/
#define ZRTP_COMP_INIT(mac_type, mac_head, mac_elem)\
{\
    mac_type* mac_e = (mac_type*)mac_elem; \
    mlist_add_tail(mac_head, &mac_e->mlist);\
    if (mac_e->base.init)\
    	    mac_e->base.init((mac_type*)mac_e);\
} break;\

zrtp_status_t zrtp_comp_register( zrtp_crypto_comp_t type,
								  void *comp,
								  zrtp_global_t* zrtp )
{
    switch (type)
    {
	case ZRTP_CC_HASH:
		ZRTP_COMP_INIT(zrtp_hash_t, &zrtp->hash_head, comp);
	case ZRTP_CC_SAS:
		ZRTP_COMP_INIT(zrtp_sas_scheme_t, &zrtp->sas_head, comp);
	case ZRTP_CC_CIPHER:
		ZRTP_COMP_INIT(zrtp_cipher_t, &zrtp->cipher_head, comp);
	case ZRTP_CC_ATL:
		ZRTP_COMP_INIT(zrtp_auth_tag_length_t, &zrtp->atl_head, comp);
	case ZRTP_CC_PKT:
		ZRTP_COMP_INIT(zrtp_pk_scheme_t, &zrtp->pktype_head, comp);
	default:
		break;
    }

    return zrtp_status_ok;
}

/*----------------------------------------------------------------------------*/
#define ZRTP_COMP_FIND(mac_head, mac_id, mac_type, res)\
{\
    mlist_t* mac_node = NULL;\
    mlist_for_each(mac_node, mac_head)\
    {\
	zrtp_comp_t* mac_e = (zrtp_comp_t*) mlist_get_struct(mac_type, mlist, mac_node);\
	if ( mac_id == mac_e->id )\
	{\
	    res = (mac_type*)mac_e;\
	    break;\
	}\
    }\
} break;

void* zrtp_comp_find(zrtp_crypto_comp_t type, uint8_t id, zrtp_global_t* zrtp)
{
    void* res = NULL;

    switch (type)
    {
	case ZRTP_CC_HASH:
	    ZRTP_COMP_FIND(&zrtp->hash_head, id, zrtp_hash_t, res);
	case ZRTP_CC_SAS:
	    ZRTP_COMP_FIND(&zrtp->sas_head, id, zrtp_sas_scheme_t, res);
	case ZRTP_CC_CIPHER:
	    ZRTP_COMP_FIND(&zrtp->cipher_head, id, zrtp_cipher_t, res);
	case ZRTP_CC_PKT:
	    ZRTP_COMP_FIND(&zrtp->pktype_head, id, zrtp_pk_scheme_t, res);
	case ZRTP_CC_ATL:
	    ZRTP_COMP_FIND(&zrtp->atl_head, id, zrtp_auth_tag_length_t, res);
	default:
		break;
    }
    
    return res ;
}

/*----------------------------------------------------------------------------*/
char* zrtp_comp_id2type(zrtp_crypto_comp_t type, uint8_t id)
{    
	if (ZRTP_COMP_UNKN == id)
		return "Unkn";

    switch (type)
    {
	case ZRTP_CC_HASH:
		switch (id)
		{
		case ZRTP_HASH_SHA256: return ZRTP_S256;
		case ZRTP_HASH_SHA384: return ZRTP_S384;
		default: return "Unkn";
		}
		break;
	    
	case ZRTP_CC_SAS:
		switch (id)
		{
		case ZRTP_SAS_BASE32:	return ZRTP_B32;
		case ZRTP_SAS_BASE256:  return ZRTP_B256;
		default: return "Unkn";
		}
		break;

	case ZRTP_CC_CIPHER:
		switch (id)
		{		
		case ZRTP_CIPHER_AES128: return ZRTP_AES1;
		case ZRTP_CIPHER_AES256: return ZRTP_AES3;
		default: return "Unkn";
		}
		break;

	case ZRTP_CC_PKT:
		switch (id)
		{		
		case ZRTP_PKTYPE_PRESH:  return ZRTP_PRESHARED;
		case ZRTP_PKTYPE_MULT:	 return ZRTP_MULT;
		case ZRTP_PKTYPE_DH2048: return ZRTP_DH2K;
		case ZRTP_PKTYPE_DH3072: return ZRTP_DH3K;
		case ZRTP_PKTYPE_EC256P: return ZRTP_EC256P;
		case ZRTP_PKTYPE_EC384P: return ZRTP_EC384P;
		case ZRTP_PKTYPE_EC521P: return ZRTP_EC521P;
		default: return "Unkn";
		}
		break;

	case ZRTP_CC_ATL:
		switch (id)
		{
		case ZRTP_ATL_HS32: return ZRTP_HS32;
		case ZRTP_ATL_HS80: return ZRTP_HS80;
		default: return "Unkn";
		}
		break;

	default:
		return "Unkn";
    }    
}

/*----------------------------------------------------------------------------*/
uint8_t zrtp_comp_type2id(zrtp_crypto_comp_t type, char* name)
{
    switch (type)
    {
	case ZRTP_CC_HASH:
		if (!zrtp_memcmp(ZRTP_S256, name, ZRTP_COMP_TYPE_SIZE)) {
			return ZRTP_HASH_SHA256;
		}
		if (!zrtp_memcmp(ZRTP_S384, name, ZRTP_COMP_TYPE_SIZE)) {
			return ZRTP_HASH_SHA384;
		}
		break;

	case ZRTP_CC_SAS:
		if (!zrtp_memcmp(ZRTP_B32, name, ZRTP_COMP_TYPE_SIZE)) {
			return ZRTP_SAS_BASE32;
		}
		if (!zrtp_memcmp(ZRTP_B256, name, ZRTP_COMP_TYPE_SIZE)) {
			return ZRTP_SAS_BASE256;
		}
		break;
			
	case ZRTP_CC_CIPHER:
		if (!zrtp_memcmp(ZRTP_AES1, name, ZRTP_COMP_TYPE_SIZE)) {
			return ZRTP_CIPHER_AES128;
		}
		if (!zrtp_memcmp(ZRTP_AES3, name, ZRTP_COMP_TYPE_SIZE)) {
			return ZRTP_CIPHER_AES256;
		}
		break;

	case ZRTP_CC_PKT:
		if (!zrtp_memcmp(ZRTP_PRESHARED, name, ZRTP_COMP_TYPE_SIZE)) {
			return ZRTP_PKTYPE_PRESH;
		}
		if (!zrtp_memcmp(ZRTP_MULT, name, ZRTP_COMP_TYPE_SIZE)) {
			return ZRTP_PKTYPE_MULT;
		}
		if (!zrtp_memcmp(ZRTP_DH3K, name, ZRTP_COMP_TYPE_SIZE)) {
			return ZRTP_PKTYPE_DH3072;
		}
		if (!zrtp_memcmp(ZRTP_DH2K, name, ZRTP_COMP_TYPE_SIZE)) {
			return ZRTP_PKTYPE_DH2048;
		}
		if (!zrtp_memcmp(ZRTP_EC256P, name, ZRTP_COMP_TYPE_SIZE)) {
			return ZRTP_PKTYPE_EC256P;
		}
		if (!zrtp_memcmp(ZRTP_EC384P, name, ZRTP_COMP_TYPE_SIZE)) {
			return ZRTP_PKTYPE_EC384P;
		}
		if (!zrtp_memcmp(ZRTP_EC521P, name, ZRTP_COMP_TYPE_SIZE)) {
			return ZRTP_PKTYPE_EC521P;
		}
		break;

	case ZRTP_CC_ATL:
		if ( !zrtp_memcmp(ZRTP_HS32, name, ZRTP_COMP_TYPE_SIZE)) {
			return ZRTP_ATL_HS32;
		}
		if (!zrtp_memcmp(ZRTP_HS80, name, ZRTP_COMP_TYPE_SIZE)) {
			return ZRTP_ATL_HS80;
		}
		break;

	default:
		return 0;
    }

	return 0;
}

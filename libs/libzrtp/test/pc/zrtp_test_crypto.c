/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */

#include "zrtp.h"

#define	_ZTU_ "libzrtp_test"

/*---------------------------------------------------------------------------*/
static void cipher_test(zrtp_global_t *zrtp)
{
	zrtp_cipher_t *cipher = zrtp_comp_find(ZRTP_CC_CIPHER, ZRTP_CIPHER_AES128, zrtp);
	if (NULL == cipher) {
		ZRTP_LOG(1, (_ZTU_,"ERROR! can't find ZRTP_CIPHER_AES128 cipher component\n"));
	} else {
		cipher->self_test(cipher, ZRTP_CIPHER_MODE_CFB);
		cipher->self_test(cipher, ZRTP_CIPHER_MODE_CTR);
	}
	
	cipher = zrtp_comp_find(ZRTP_CC_CIPHER, ZRTP_CIPHER_AES256, zrtp);
	if (NULL == cipher) {
		ZRTP_LOG(1, (_ZTU_,"ERROR! can't find ZRTP_CIPHER_AES256 cipher component\n"));
	} else {
		cipher->self_test(cipher, ZRTP_CIPHER_MODE_CFB);
		cipher->self_test(cipher, ZRTP_CIPHER_MODE_CTR);
	}
}

/*---------------------------------------------------------------------------*/
static zrtp_status_t hash_test(zrtp_global_t *zrtp)
{
	zrtp_hash_t *hash =  zrtp_comp_find(ZRTP_CC_HASH, ZRTP_SRTP_HASH_HMAC_SHA1, zrtp);
	if (NULL == hash) {
		ZRTP_LOG(1, (_ZTU_,"ERROR! can't find ZRTP_SRTP_HASH_HMAC_SHA1 component\n"));
	} else {
		hash->hash_self_test(hash);
		hash->hmac_self_test(hash);
	}
	
	hash =  zrtp_comp_find(ZRTP_CC_HASH, ZRTP_HASH_SHA256, zrtp);
	if (NULL == hash) {
		ZRTP_LOG(1, (_ZTU_,"ERROR! can't find ZRTP_HASH_SHA256 component\n"));
	} else {
		hash->hash_self_test(hash);
		hash->hmac_self_test(hash);
	}
	
	hash =  zrtp_comp_find(ZRTP_CC_HASH, ZRTP_HASH_SHA384, zrtp);
	if (NULL == hash) {
		ZRTP_LOG(1, (_ZTU_,"ERROR! can't find ZRTP_HASH_SHA384 component\n"));
	} else {
		hash->hash_self_test(hash);
		hash->hmac_self_test(hash);
	}
	
	return zrtp_status_ok;
}

/*---------------------------------------------------------------------------*/
static zrtp_status_t dh_test(zrtp_global_t *zrtp)
{
	zrtp_pk_scheme_t *pks = zrtp_comp_find(ZRTP_CC_PKT, ZRTP_PKTYPE_DH2048, zrtp);
	if (!pks) {
		ZRTP_LOG(1, (_ZTU_,"ERROR! can't find ZRTP_PKTYPE_DH2048 component\n"));
	} else {
		pks->self_test(pks);
	}
	
	pks = zrtp_comp_find(ZRTP_CC_PKT, ZRTP_PKTYPE_DH3072, zrtp);
	if (!pks) {
		ZRTP_LOG(1, (_ZTU_,"ERROR! can't find ZRTP_PKTYPE_DH3072 component\n"));
	} else {
		pks->self_test(pks);
	}
	
	return zrtp_status_ok;
}

/*---------------------------------------------------------------------------*/
static zrtp_status_t ecdh_test(zrtp_global_t *zrtp)
{
	zrtp_pk_scheme_t *pks = zrtp_comp_find(ZRTP_CC_PKT, ZRTP_PKTYPE_EC256P, zrtp);
	if (!pks) {
		ZRTP_LOG(1, (_ZTU_,"ERROR! can't find ZRTP_PKTYPE_EC256P component\n"));
	} else {
		pks->self_test(pks);
	}
	
	pks = zrtp_comp_find(ZRTP_CC_PKT, ZRTP_PKTYPE_EC384P, zrtp);
	if (!pks) {
		ZRTP_LOG(1, (_ZTU_,"ERROR! can't find ZRTP_PKTYPE_EC384P component\n"));
	} else {
		pks->self_test(pks);
	}
	
	pks = zrtp_comp_find(ZRTP_CC_PKT, ZRTP_PKTYPE_EC521P, zrtp);
	if (!pks) {
		ZRTP_LOG(1, (_ZTU_,"ERROR! can't find ZRTP_PKTYPE_EC521P component\n"));
	} else {
		pks->self_test(pks);
	}
	
	return zrtp_status_ok;
}

/*---------------------------------------------------------------------------*/
#if (defined(ZRTP_USE_EXTERN_SRTP) && (ZRTP_USE_EXTERN_SRTP == 1))
#else

static uint8_t dk_master_key[16] = {
	0xE1, 0xF9, 0x7A, 0x0D, 0x3E, 0x01, 0x8B, 0xE0,
	0xD6, 0x4F, 0xA3, 0x2C, 0x06, 0xDE, 0x41, 0x39
};

static uint8_t dk_master_salt[14] = {
	0x0E, 0xC6, 0x75, 0xAD, 0x49, 0x8A, 0xFE, 0xEB,
	0xB6, 0x96, 0x0B, 0x3A, 0xAB, 0xE6
};


static uint8_t dk_cipher_key[16] = {
	0xC6, 0x1E, 0x7A, 0x93, 0x74, 0x4F, 0x39, 0xEE,
	0x10, 0x73, 0x4A, 0xFE, 0x3F, 0xF7, 0xA0, 0x87
};

static uint8_t dk_cipher_salt[14] = {
	0x30, 0xCB, 0xBC, 0x08, 0x86, 0x3D, 0x8C, 0x85,
	0xD4, 0x9D, 0xB3, 0x4A, 0x9A, 0xE1
};

static uint8_t dk_auth_key[94] = {
	0xCE, 0xBE, 0x32, 0x1F, 0x6F, 0xF7, 0x71, 0x6B,
	0x6F, 0xD4, 0xAB, 0x49, 0xAF, 0x25, 0x6A, 0x15,
	0x6D, 0x38, 0xBA, 0xA4, 0x8F, 0x0A, 0x0A, 0xCF,
	0x3C, 0x34, 0xE2, 0x35, 0x9E, 0x6C, 0xDB, 0xCE,
	0xE0, 0x49, 0x64, 0x6C, 0x43, 0xD9, 0x32, 0x7A,
	0xD1, 0x75, 0x57, 0x8E, 0xF7, 0x22, 0x70, 0x98,
	0x63, 0x71, 0xC1, 0x0C, 0x9A, 0x36, 0x9A, 0xC2,
	0xF9, 0x4A, 0x8C, 0x5F, 0xBC, 0xDD, 0xDC, 0x25,
	0x6D, 0x6E, 0x91, 0x9A, 0x48, 0xB6, 0x10, 0xEF,
	0x17, 0xC2, 0x04, 0x1E, 0x47, 0x40, 0x35, 0x76,
	0x6B, 0x68, 0x64, 0x2C, 0x59, 0xBB, 0xFC, 0x2F,
	0x34, 0xDB, 0x60, 0xDB, 0xDF, 0xB2
};

extern zrtp_dk_ctx *zrtp_dk_init(zrtp_cipher_t *cipher, zrtp_stringn_t *key, zrtp_stringn_t *salt);
extern zrtp_status_t zrtp_derive_key(zrtp_dk_ctx *ctx, zrtp_srtp_prf_label label, zrtp_stringn_t *result_key);
extern void zrtp_dk_deinit(zrtp_dk_ctx *ctx);

zrtp_status_t hex_cmp(uint8_t *a, uint8_t *b, uint32_t len)
{
	uint32_t i;
	zrtp_status_t res = zrtp_status_ok;
	for (i = 0; i<len; i++) {
		if (a[i] != b[i]) {
			res = zrtp_status_fail;
			break;
		}
	}
	return res;
}

zrtp_status_t dk_test(zrtp_global_t *zrtp)
{
	zrtp_status_t res;
	zrtp_string16_t master_key, master_salt, cipher_key, cipher_salt;
	zrtp_string128_t auth_key;
	zrtp_dk_ctx *ctx;

	zrtp_cipher_t *cipher = zrtp_comp_find(ZRTP_CC_CIPHER, ZRTP_CIPHER_AES128, zrtp);

	if(NULL == cipher){
		return zrtp_status_fail;
	}
	
	master_key.length = master_key.max_length = 16;
	zrtp_memcpy(master_key.buffer, dk_master_key, 16);
	
	master_salt.length = 14;
	master_salt.max_length = 16;
	zrtp_memcpy(master_salt.buffer, dk_master_salt, 14);
	

	ctx = zrtp_dk_init(cipher, (zrtp_stringn_t*)&master_key, (zrtp_stringn_t*)&master_salt);
	if(NULL == ctx){
		return zrtp_status_fail;
	}

	cipher_key.length = 16;
	cipher_key.max_length = 16;

	zrtp_derive_key(ctx, label_rtp_encryption, (zrtp_stringn_t*)&cipher_key);
	res = hex_cmp((uint8_t*)cipher_key.buffer, dk_cipher_key, cipher_key.length);
	ZRTP_LOG(3, (_ZTU_,"cipher key check...%s\n", zrtp_status_ok==res?"OK":"FAIL"));
	

	cipher_salt.length = 14;
	cipher_salt.max_length = 16;

	zrtp_derive_key(ctx, label_rtp_salt, (zrtp_stringn_t*)&cipher_salt);
	res = hex_cmp((uint8_t*)cipher_salt.buffer, dk_cipher_salt, cipher_salt.length);
	ZRTP_LOG(3, (_ZTU_,"cipher salt check...%s\n", zrtp_status_ok==res?"OK":"FAIL"));
	
	
	auth_key.length = 94;
	auth_key.max_length = 128;
	
	zrtp_derive_key(ctx, label_rtp_msg_auth, (zrtp_stringn_t*)&auth_key);
	res = hex_cmp((uint8_t*)auth_key.buffer, dk_auth_key, auth_key.length);
	ZRTP_LOG(3, (_ZTU_,"auth key check...%s\n", zrtp_status_ok==res?"OK":"FAIL"));
	
	zrtp_dk_deinit(ctx);
	
	return zrtp_status_ok;
}

/*---------------------------------------------------------------------------*/
#define TEST_MAP_WIDTH 64
#if TEST_MAP_WIDTH%8
#	define TEST_MAP_WIDTH_BYTES TEST_MAP_WIDTH/8+1
#else
#	define TEST_MAP_WIDTH_BYTES TEST_MAP_WIDTH/8
#endif

#define FIRST_TEST_MAP_INIT_WIDTH 24

extern zrtp_rp_node_t *get_rp_node_non_lock(zrtp_rp_ctx_t *ctx, uint8_t direction, uint32_t ssrc);
extern zrtp_rp_node_t *add_rp_node(zrtp_srtp_ctx_t *srtp_ctx, zrtp_rp_ctx_t *ctx, uint8_t direction, uint32_t ssrc);
extern zrtp_status_t zrtp_srtp_rp_check(zrtp_srtp_rp_t *srtp_rp, zrtp_rtp_info_t *packet);
extern zrtp_status_t zrtp_srtp_rp_add(zrtp_srtp_rp_t *srtp_rp, zrtp_rtp_info_t *packet);

void print_map(uint8_t *map, int width_bytes)
{
	int i;
	for(i=width_bytes-1; i >= 0; i--) {
		ZRTP_LOGC(3, ("%i%i%i%i%i%i%i%i",
					zrtp_bitmap_get_bit(map, 8*i+7),
					zrtp_bitmap_get_bit(map, 8*i+6),
					zrtp_bitmap_get_bit(map, 8*i+5),
					zrtp_bitmap_get_bit(map, 8*i+4),
					zrtp_bitmap_get_bit(map, 8*i+3),
					zrtp_bitmap_get_bit(map, 8*i+2),
					zrtp_bitmap_get_bit(map, 8*i+1),
					zrtp_bitmap_get_bit(map, 8*i+0)));			
	}
	ZRTP_LOG(3, (_ZTU_, "\n"));
}

void init_random_map(uint8_t *map, int width, zrtp_global_t *zrtp)
{
	int i;
	for(i=0; i<width; i++) {
		uint32_t rnd = 0;
		zrtp_randstr(zrtp, (uint8_t*)&rnd, sizeof(rnd));
		if(rnd%10 < 5) {
			zrtp_bitmap_set_bit(map, i);
		} else {
			zrtp_bitmap_clear_bit(map, i);
		}
	}
}

void inject_from_map( zrtp_srtp_global_t *srtp_global, 
					  uint32_t ssrc,
					  uint8_t *src_map, uint8_t *dst_map, int width)
{
	zrtp_rp_node_t *rp_node;
	int i;
	zrtp_rtp_info_t pkt;
	
	rp_node = get_rp_node_non_lock(srtp_global->rp_ctx, RP_INCOMING_DIRECTION, ssrc);
	if (NULL == rp_node) {
		return;	
	}
	
	for (i=0; i< width; i++) {
		if (1 == zrtp_bitmap_get_bit(src_map, i)) {
			pkt.seq = i;
			if (zrtp_status_ok == zrtp_srtp_rp_check(&rp_node->rtp_rp, &pkt)) {
				zrtp_bitmap_set_bit(dst_map, i);
				zrtp_srtp_rp_add(&rp_node->rtp_rp, &pkt);
			}
		}	
	}
}

/*---------------------------------------------------------------------------*/
int srtp_replay_protection_test(zrtp_global_t *zrtp)
{
	int res = 0;
	uint32_t ssrc = 1;
	int i = 0;
	uint8_t test_map[TEST_MAP_WIDTH_BYTES];
	uint8_t result_map[TEST_MAP_WIDTH_BYTES];
	uint8_t tmp_window[ZRTP_SRTP_WINDOW_WIDTH_BYTES];
	uint32_t tmp_seq;
	int delta, shift;
		
	zrtp_rp_node_t *rp_node;
	zrtp_srtp_global_t *srtp = zrtp->srtp_global;
	
	rp_node = add_rp_node(NULL, srtp->rp_ctx, RP_INCOMING_DIRECTION, ssrc);
	if (NULL == rp_node) {
		return -1;
	}
	
	for (i=0; i< TEST_MAP_WIDTH_BYTES; i++) {
		test_map[i] = 0;
		result_map[i] = 0;
	}
	/*
	 * 1st test
	 * ----------------------------------------------------------------------
	 */
	init_random_map(test_map, FIRST_TEST_MAP_INIT_WIDTH, zrtp);
	inject_from_map(srtp, ssrc, test_map, result_map, TEST_MAP_WIDTH);
	
	ZRTP_LOG(3, (_ZTU_,"1st test. Wnd[%i]...\n", ZRTP_SRTP_WINDOW_WIDTH));

	tmp_seq = rp_node->rtp_rp.seq;
	for (i=0; i<ZRTP_SRTP_WINDOW_WIDTH_BYTES; i++) {
		tmp_window[i] = rp_node->rtp_rp.window[i];
	}
	
	delta = tmp_seq-ZRTP_SRTP_WINDOW_WIDTH + 1;
	if (delta > 0) {
		ZRTP_LOG(3, (_ZTU_,"after  wnd: (%i;0]\n", delta));
		ZRTP_LOG(3, (_ZTU_,"inside wnd: [%i;%i]\n", tmp_seq, delta)); 
	} else {
		ZRTP_LOG(3, (_ZTU_,"after  wnd: (0;0)\n"));
		ZRTP_LOG(3, (_ZTU_,"inside wnd: [%i;0]\n", tmp_seq)); 
	}
	
	ZRTP_LOG(3, (_ZTU_,"before wnd: [%i;%i)\n", TEST_MAP_WIDTH-1, tmp_seq));
	
	ZRTP_LOG(3, (_ZTU_,"Test map: "));
	print_map(test_map, TEST_MAP_WIDTH_BYTES);
		
	ZRTP_LOG(3, (_ZTU_,"Res  map: "));
	print_map(result_map, TEST_MAP_WIDTH_BYTES);

	shift = TEST_MAP_WIDTH;
	shift -= rp_node->rtp_rp.seq + 1;

	ZRTP_LOG(3, (_ZTU_,"Window  : "));
	for(i=shift; i > 0; i--){
		ZRTP_LOGC(3, (" "));
	}
	print_map(rp_node->rtp_rp.window, ZRTP_SRTP_WINDOW_WIDTH_BYTES);
	
	/*
	 * 2nd test
	 * ----------------------------------------------------------------------
	 */
	for(i=0; i< TEST_MAP_WIDTH_BYTES; i++){
		test_map[i] = 0;
		result_map[i] = 0;
	}

	init_random_map(test_map, TEST_MAP_WIDTH, zrtp);
	inject_from_map(srtp, ssrc, test_map, result_map, TEST_MAP_WIDTH);

	ZRTP_LOG(3, (_ZTU_,"2nd test. Wnd[%i]...\n", ZRTP_SRTP_WINDOW_WIDTH));
	ZRTP_LOG(3, (_ZTU_,"Test map: "));
	print_map(test_map, TEST_MAP_WIDTH_BYTES);
		
	ZRTP_LOG(3, (_ZTU_,"Res  map: "));
	print_map(result_map, TEST_MAP_WIDTH_BYTES);

	shift = TEST_MAP_WIDTH;
	shift -= rp_node->rtp_rp.seq + 1;

	ZRTP_LOG(3, (_ZTU_,"Window  : "));
	for (i=shift; i > 0; i--) {
		//zrtp_print_log(ZRTP_LOG_DEBUG, " ");
	}
	print_map(rp_node->rtp_rp.window, ZRTP_SRTP_WINDOW_WIDTH_BYTES);

	
	/*
	  in result map:
	  - after window we should to have all zeroes
	  - into the window we should have ones only if window have zero at appropriate position
	  - before window we should have equal values of test map and result map bits
	*/	
	for (i=0; i < TEST_MAP_WIDTH; i++) {
		if (delta > 0 && i < delta) {
			/* After window */
			if (0 != zrtp_bitmap_get_bit(result_map, i)) {
				ZRTP_LOG(3, (_ZTU_,"After window. %i bit should be 0\n", i));
				res = -1;
			}
		} else if (i <= (int)tmp_seq && i >= delta) {
			/* inside window */
			
			/* check window filtering */
			if(1 == zrtp_bitmap_get_bit(result_map, i)) {			
				if (1 == zrtp_bitmap_get_bit(tmp_window, i - (tmp_seq-ZRTP_SRTP_WINDOW_WIDTH) - 1)) {				
					ZRTP_LOG(3, (_ZTU_,"Inside window. Window filtering fail. %i bit should be 0\n", i));
					res = -1;
				}
			}
			/* check test vs result maps */
			if ( zrtp_bitmap_get_bit(result_map, i) != zrtp_bitmap_get_bit(test_map, i) &&
				 !zrtp_bitmap_get_bit(tmp_window, i - (tmp_seq-ZRTP_SRTP_WINDOW_WIDTH) - 1)) {
				ZRTP_LOG(3, (_ZTU_, "Inside window. Test map isn't equal to result at bit %i\n", i));
				res = -1;
			}
				
		} else {
			/* after window */
			if (zrtp_bitmap_get_bit(result_map, i) != zrtp_bitmap_get_bit(test_map, i)) {
				ZRTP_LOG(3, (_ZTU_,"Before window. Test map isn't equal to result at bit %i\n", i));
				res = -1;
			}
		}
	}
	if (0 == res) {
		ZRTP_LOG(3, (_ZTU_,"Test passed successfully\n"));
	}

	return res;
}

#endif 


/*---------------------------------------------------------------------------*/
void zrtp_test_crypto(zrtp_global_t* zrtp)
{
	ZRTP_LOG(3, (_ZTU_,"====================CIPHERS TESTS====================\n"));	
	cipher_test(zrtp);
	ZRTP_LOG(3, (_ZTU_,"=====================HASHES TESTS====================\n"));	
	hash_test(zrtp);
	ZRTP_LOG(3, (_ZTU_,"================PUBLIC KEY SCHEMES TESTS==============\n"));
	dh_test(zrtp);
	ecdh_test(zrtp);
	ZRTP_LOG(3, (_ZTU_,"===============SRTP Key derivation TESTS==============\n"));
	dk_test(zrtp);		
	ZRTP_LOG(3, (_ZTU_,"==============SRTP Replay protection TESTS============\n"))	;
	srtp_replay_protection_test(zrtp);
}

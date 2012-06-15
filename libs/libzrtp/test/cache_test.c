/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <stdio.h>

#include "zrtp.h"
#include "cmockery/cmockery.h"

#define TEST_CACHE_PATH		"./zrtp_cache_test.dat"

static zrtp_global_t g_zrtp_cfg;

static zrtp_string16_t zid_my = ZSTR_INIT_WITH_CONST_CSTRING("000000000_00");
static zrtp_string16_t zid_a = ZSTR_INIT_WITH_CONST_CSTRING("000000000_02");
static zrtp_string16_t zid_b = ZSTR_INIT_WITH_CONST_CSTRING("000000000_03");
static zrtp_string16_t zid_c = ZSTR_INIT_WITH_CONST_CSTRING("000000000_04");
static zrtp_string16_t zid_mitm1 = ZSTR_INIT_WITH_CONST_CSTRING("000000000_m1");
static zrtp_string16_t zid_mitm2 = ZSTR_INIT_WITH_CONST_CSTRING("000000000_m2");

static zrtp_shared_secret_t rs_my4a, rs_my4b, rs_my4c, rs_my4mitm1, rs_my4mitm2;
static zrtp_shared_secret_t rs_my4a_r, rs_my4b_r, rs_my4c_r, rs_my4mitm1_r, rs_my4mitm2_r;

static zrtp_cache_id_t secerets_to_delete[24];
static unsigned secerets_to_delete_count = 0;

static void init_rs_secret_(zrtp_shared_secret_t *sec, unsigned char val_fill);

extern void zrtp_cache_create_id(const zrtp_stringn_t* first_ZID,
							 	 const zrtp_stringn_t* second_ZID,
							 	 zrtp_cache_id_t id);


void cache_setup() {
	zrtp_status_t status;
	
	/* Delete cache file from previous test if it exists. */
	remove(TEST_CACHE_PATH);
	
	secerets_to_delete_count = 0;
	
	ZSTR_SET_EMPTY(g_zrtp_cfg.def_cache_path);
	/* Configure and Initialize ZRTP cache */
	zrtp_zstrcpyc(ZSTR_GV(g_zrtp_cfg.def_cache_path), TEST_CACHE_PATH);
	
	init_rs_secret_(&rs_my4a, 'a'); init_rs_secret_(&rs_my4b, 'b'); init_rs_secret_(&rs_my4c, 'c');
	init_rs_secret_(&rs_my4mitm1, '1'); init_rs_secret_(&rs_my4mitm2, '2');

	init_rs_secret_(&rs_my4a_r, 0); init_rs_secret_(&rs_my4b_r, 0); init_rs_secret_(&rs_my4c_r, 0);
	init_rs_secret_(&rs_my4mitm1_r, 0); init_rs_secret_(&rs_my4mitm2_r, 0);
	
	/* It should NOT crash and return OK. */
	status = zrtp_def_cache_init(&g_zrtp_cfg);
	assert_int_equal(status, zrtp_status_ok);
	
	/* Add few values into it */
	printf("==> Add few test entries.\n");
	
	status = zrtp_def_cache_put(ZSTR_GV(zid_my), ZSTR_GV(zid_a), &rs_my4a);
	assert_int_equal(status, zrtp_status_ok);	
	status = zrtp_def_cache_put(ZSTR_GV(zid_my), ZSTR_GV(zid_b), &rs_my4b);
	assert_int_equal(status, zrtp_status_ok);
	status = zrtp_def_cache_put(ZSTR_GV(zid_my), ZSTR_GV(zid_c), &rs_my4c);
	assert_int_equal(status, zrtp_status_ok);
	
	status = zrtp_def_cache_put_mitm(ZSTR_GV(zid_my), ZSTR_GV(zid_mitm1), &rs_my4mitm1);
	assert_int_equal(status, zrtp_status_ok);
	status = zrtp_def_cache_put_mitm(ZSTR_GV(zid_my), ZSTR_GV(zid_mitm2), &rs_my4mitm2);
	assert_int_equal(status, zrtp_status_ok);
	
	status = zrtp_def_cache_put(ZSTR_GV(zid_my), ZSTR_GV(zid_c), &rs_my4c);
	assert_int_equal(status, zrtp_status_ok);
	
	/* Close the cache, it should be flushed to the file. */
	printf("==> Close the cache.\n");
	
	zrtp_def_cache_down();
	
	printf("==> Open just prepared cache file.\n");
	
	status = zrtp_def_cache_init(&g_zrtp_cfg);
	assert_int_equal(status, zrtp_status_ok);
	
	printf("==> Ready for the test!.\n");
}

void cache_teardown() {
	zrtp_def_cache_down();
}

/*
 * Simply init ZRTP cache with empty or non-existing filer and close it.
 * The app should not crash and trigger no errors.
*/
void cache_init_store_empty_test() {	
	zrtp_def_cache_down();
}

/*
 * Add few entries to the empty cache, flush it and then load again. Check if
 * all the entries were restored successfully.
 */
void cache_add2empty_test() {
	zrtp_status_t status;	
	int intres;
	
	/* Now, let's open the cache again and check if all the previously added values were restored successfully */
	printf("==> And open it again, it should contain all the stored values.\n");
	
	status = zrtp_def_cache_get(ZSTR_GV(zid_my), ZSTR_GV(zid_a), &rs_my4a_r, 0);
	assert_int_equal(status, zrtp_status_ok);
	assert_false(zrtp_zstrcmp(ZSTR_GV(rs_my4a_r.value), ZSTR_GV(rs_my4a.value)));
	
	status = zrtp_def_cache_get(ZSTR_GV(zid_my), ZSTR_GV(zid_b), &rs_my4b_r, 0);
	assert_int_equal(status, zrtp_status_ok);
	assert_false(zrtp_zstrcmp(ZSTR_GV(rs_my4b_r.value), ZSTR_GV(rs_my4b.value)));
		
	status = zrtp_def_cache_get_mitm(ZSTR_GV(zid_my), ZSTR_GV(zid_mitm1), &rs_my4mitm1_r);
	assert_int_equal(status, zrtp_status_ok);
	assert_false(zrtp_zstrcmp(ZSTR_GV(rs_my4mitm1_r.value), ZSTR_GV(rs_my4mitm1.value)));
	
	status = zrtp_def_cache_get_mitm(ZSTR_GV(zid_my), ZSTR_GV(zid_mitm2), &rs_my4mitm2_r);
	assert_int_equal(status, zrtp_status_ok);
	assert_false(zrtp_zstrcmp(ZSTR_GV(rs_my4mitm2_r.value), ZSTR_GV(rs_my4mitm2.value)));
}

/*
 * Test if cache properly handles Open-Close-Open with now no changes to the cache values.
 */
void cache_save_unchanged_test() {
	zrtp_status_t status;
	
	/* Now, let's open the cache again and check if all the previously added values were restored successfully */
	printf("==> Now let's Open the cache and Close it right after, make no changes.\n");
		
	zrtp_def_cache_down();
	
	/*
	 * TEST: now let's store the cache making no changes to it.
	 * After opening it should include all the secrets untouched.
	 */
	
	printf("==> And the cache again, it should contain all the stored values.\n");
	
	status = zrtp_def_cache_init(&g_zrtp_cfg);
	assert_int_equal(status, zrtp_status_ok);
	
	status = zrtp_def_cache_get(ZSTR_GV(zid_my), ZSTR_GV(zid_a), &rs_my4a_r, 0);
	assert_int_equal(status, zrtp_status_ok);
	assert_false(zrtp_zstrcmp(ZSTR_GV(rs_my4a_r.value), ZSTR_GV(rs_my4a.value)));
	
	status = zrtp_def_cache_get(ZSTR_GV(zid_my), ZSTR_GV(zid_b), &rs_my4b_r, 0);
	assert_int_equal(status, zrtp_status_ok);
	assert_false(zrtp_zstrcmp(ZSTR_GV(rs_my4b_r.value), ZSTR_GV(rs_my4b.value)));
		
	status = zrtp_def_cache_get_mitm(ZSTR_GV(zid_my), ZSTR_GV(zid_mitm1), &rs_my4mitm1_r);
	assert_int_equal(status, zrtp_status_ok);
	assert_false(zrtp_zstrcmp(ZSTR_GV(rs_my4mitm1_r.value), ZSTR_GV(rs_my4mitm1.value)));
	
	status = zrtp_def_cache_get_mitm(ZSTR_GV(zid_my), ZSTR_GV(zid_mitm2), &rs_my4mitm2_r);
	assert_int_equal(status, zrtp_status_ok);
	assert_false(zrtp_zstrcmp(ZSTR_GV(rs_my4mitm2_r.value), ZSTR_GV(rs_my4mitm2.value)));
}

/*
 * Check how the cache handles flushing of several dirty (modified) values. The cache should
 * flush to the disk modified values only and leave rest of the items untouched.
 */
void cache_modify_and_save_test() {
	zrtp_status_t status;	
	int intres;
	
	printf("==> And open it again, it should contain all the stored values.\n");
	
	/*
	 * Now, let's modify just few entries and check of the fill will be stored.
	 *
	 * We will change RS secrets rs_my4b, rs_my4c and rs_my4mitm1 while leaving
	 * rs_my4a and rs_my4mitm2 untouched.
	 */
		
	init_rs_secret_(&rs_my4b, 'x'); init_rs_secret_(&rs_my4c, 'y');
	init_rs_secret_(&rs_my4mitm1, 'z');
	
	printf("==> Now we gonna to update few cache entries and flush the cache mack to the file.\n");
	
	status = zrtp_def_cache_put(ZSTR_GV(zid_my), ZSTR_GV(zid_b), &rs_my4b);
	assert_int_equal(status, zrtp_status_ok);
	status = zrtp_def_cache_put(ZSTR_GV(zid_my), ZSTR_GV(zid_c), &rs_my4c);
	assert_int_equal(status, zrtp_status_ok);	
	status = zrtp_def_cache_put_mitm(ZSTR_GV(zid_my), ZSTR_GV(zid_mitm1), &rs_my4mitm1);
	assert_int_equal(status, zrtp_status_ok);
		
	/* Flush the cache and open it again. */
	zrtp_def_cache_down();
	
	printf("==> Open the cache and make sure all our prev. modifications saved properly.\n");
	
	status = zrtp_def_cache_init(&g_zrtp_cfg);
	assert_int_equal(status, zrtp_status_ok);
	
	/* Let's check if all our modifications are in place. */
	status = zrtp_def_cache_get(ZSTR_GV(zid_my), ZSTR_GV(zid_a), &rs_my4a_r, 0);
	assert_int_equal(status, zrtp_status_ok);
	assert_false(zrtp_zstrcmp(ZSTR_GV(rs_my4a_r.value), ZSTR_GV(rs_my4a.value)));
	
	status = zrtp_def_cache_get(ZSTR_GV(zid_my), ZSTR_GV(zid_b), &rs_my4b_r, 0);
	assert_int_equal(status, zrtp_status_ok);
	assert_false(zrtp_zstrcmp(ZSTR_GV(rs_my4b_r.value), ZSTR_GV(rs_my4b.value)));
	
	status = zrtp_def_cache_get(ZSTR_GV(zid_my), ZSTR_GV(zid_c), &rs_my4c_r, 0);
	assert_int_equal(status, zrtp_status_ok);
	assert_false(zrtp_zstrcmp(ZSTR_GV(rs_my4c_r.value), ZSTR_GV(rs_my4c.value)));
	
	status = zrtp_def_cache_get_mitm(ZSTR_GV(zid_my), ZSTR_GV(zid_mitm1), &rs_my4mitm1_r);
	assert_int_equal(status, zrtp_status_ok);
	assert_false(zrtp_zstrcmp(ZSTR_GV(rs_my4mitm1_r.value), ZSTR_GV(rs_my4mitm1.value)));
	
	status = zrtp_def_cache_get_mitm(ZSTR_GV(zid_my), ZSTR_GV(zid_mitm2), &rs_my4mitm2_r);
	assert_int_equal(status, zrtp_status_ok);
	assert_false(zrtp_zstrcmp(ZSTR_GV(rs_my4mitm2_r.value), ZSTR_GV(rs_my4mitm2.value)));
}

/* 
 * The basic idea of all cache_delete_* tests is to delete few cache entries
 * from preconfigured setup, flush caches, open the cache again and check if
 * non-deleted values are Ok.
 */

static int cache_foreach_del_func(zrtp_cache_elem_t* elem, int is_mitm, void* data, int* del) {
	unsigned c;
	
	//printf("AAAA cache_foreach_del_func(): elem index=%u\n", elem->_index);
	
	for (c=0; c<secerets_to_delete_count; c++) {
		if (!zrtp_memcmp(elem->id, secerets_to_delete[c], sizeof(zrtp_cache_id_t))) {
			printf("\t==> Delete cache element index=%u.\n", elem->_index);
			*del = 1;
			break;
		}
	}
	
	return 1;
}

void cache_delete_few_rs_test() {
	zrtp_status_t status;
	
	printf("==> Delete few RS secrets and flush the cache.\n");
	
	secerets_to_delete_count = 0;
	zrtp_cache_create_id(ZSTR_GV(zid_my), ZSTR_GV(zid_b), secerets_to_delete[secerets_to_delete_count++]);
	zrtp_cache_create_id(ZSTR_GV(zid_my), ZSTR_GV(zid_a), secerets_to_delete[secerets_to_delete_count++]);
	
	zrtp_def_cache_foreach(&g_zrtp_cfg, 0, &cache_foreach_del_func, NULL);
	
	/* Flush the cache and open it again. */
	zrtp_def_cache_down();
	
	printf("==> Open the cache and make sure all our prev. Modifications saved properly.\n");
	
	status = zrtp_def_cache_init(&g_zrtp_cfg);
	assert_int_equal(status, zrtp_status_ok);
	
	/* Let's check if all our modifications are in place. */
	
	/* my4a should be deleted. */
	status = zrtp_def_cache_get(ZSTR_GV(zid_my), ZSTR_GV(zid_a), &rs_my4a_r, 0);
	assert_int_not_equal(status, zrtp_status_ok);
	
	/* my4b should be deleted. */
	status = zrtp_def_cache_get(ZSTR_GV(zid_my), ZSTR_GV(zid_b), &rs_my4b_r, 0);
	assert_int_not_equal(status, zrtp_status_ok);
	
	/* The rest of the secrets should be in place. */
	status = zrtp_def_cache_get(ZSTR_GV(zid_my), ZSTR_GV(zid_c), &rs_my4c_r, 0);
	assert_int_equal(status, zrtp_status_ok);
	assert_false(zrtp_zstrcmp(ZSTR_GV(rs_my4c_r.value), ZSTR_GV(rs_my4c.value)));
	
	status = zrtp_def_cache_get_mitm(ZSTR_GV(zid_my), ZSTR_GV(zid_mitm1), &rs_my4mitm1_r);
	assert_int_equal(status, zrtp_status_ok);
	assert_false(zrtp_zstrcmp(ZSTR_GV(rs_my4mitm1_r.value), ZSTR_GV(rs_my4mitm1.value)));
	
	status = zrtp_def_cache_get_mitm(ZSTR_GV(zid_my), ZSTR_GV(zid_mitm2), &rs_my4mitm2_r);
	assert_int_equal(status, zrtp_status_ok);
	assert_false(zrtp_zstrcmp(ZSTR_GV(rs_my4mitm2_r.value), ZSTR_GV(rs_my4mitm2.value)));
}

void cache_delete_few_mitm_test() {
	zrtp_status_t status;
	
	printf("==> Delete few MiTM secrets and flush the cache.\n");
	
	secerets_to_delete_count = 0;
	zrtp_cache_create_id(ZSTR_GV(zid_my), ZSTR_GV(zid_mitm1), secerets_to_delete[secerets_to_delete_count++]);
	
	zrtp_def_cache_foreach(&g_zrtp_cfg, 1, &cache_foreach_del_func, NULL);
	
	/* Flush the cache and open it again. */
	zrtp_def_cache_down();
	
	printf("==> Open the cache and make sure all our prev. Modifications saved properly.\n");
	
	status = zrtp_def_cache_init(&g_zrtp_cfg);
	assert_int_equal(status, zrtp_status_ok);
	
	/* Let's check if all our modifications are in place. */
	status = zrtp_def_cache_get(ZSTR_GV(zid_my), ZSTR_GV(zid_a), &rs_my4a_r, 0);
	assert_int_equal(status, zrtp_status_ok);
	assert_false(zrtp_zstrcmp(ZSTR_GV(rs_my4a_r.value), ZSTR_GV(rs_my4a.value)));
	
	status = zrtp_def_cache_get(ZSTR_GV(zid_my), ZSTR_GV(zid_b), &rs_my4b_r, 0);
	assert_int_equal(status, zrtp_status_ok);
	assert_false(zrtp_zstrcmp(ZSTR_GV(rs_my4b_r.value), ZSTR_GV(rs_my4b.value)));
	
	status = zrtp_def_cache_get(ZSTR_GV(zid_my), ZSTR_GV(zid_c), &rs_my4c_r, 0);
	assert_int_equal(status, zrtp_status_ok);
	assert_false(zrtp_zstrcmp(ZSTR_GV(rs_my4c_r.value), ZSTR_GV(rs_my4c.value)));
	
	/* Should be deleted */
	status = zrtp_def_cache_get_mitm(ZSTR_GV(zid_my), ZSTR_GV(zid_mitm1), &rs_my4mitm1_r);
	assert_int_not_equal(status, zrtp_status_ok);
	
	status = zrtp_def_cache_get_mitm(ZSTR_GV(zid_my), ZSTR_GV(zid_mitm2), &rs_my4mitm2_r);
	assert_int_equal(status, zrtp_status_ok);
	assert_false(zrtp_zstrcmp(ZSTR_GV(rs_my4mitm2_r.value), ZSTR_GV(rs_my4mitm2.value)));
}

void cache_delete_few_rs_and_mitm_test() {
	zrtp_status_t status;
	
	printf("==> Delete few RS secrets and flush the cache.\n");
	
	secerets_to_delete_count = 0;
	zrtp_cache_create_id(ZSTR_GV(zid_my), ZSTR_GV(zid_b), secerets_to_delete[secerets_to_delete_count++]);
	zrtp_cache_create_id(ZSTR_GV(zid_my), ZSTR_GV(zid_a), secerets_to_delete[secerets_to_delete_count++]);
	
	zrtp_def_cache_foreach(&g_zrtp_cfg, 0, &cache_foreach_del_func, NULL);
	
	secerets_to_delete_count = 0;
	zrtp_cache_create_id(ZSTR_GV(zid_my), ZSTR_GV(zid_mitm1), secerets_to_delete[secerets_to_delete_count++]);
	
	zrtp_def_cache_foreach(&g_zrtp_cfg, 1, &cache_foreach_del_func, NULL);
	
	/* Flush the cache and open it again. */
	zrtp_def_cache_down();
	
	printf("==> Open the cache and make sure all our prev. Modifications saved properly.\n");
	
	status = zrtp_def_cache_init(&g_zrtp_cfg);
	assert_int_equal(status, zrtp_status_ok);
	
	/* Let's check if all our modifications are in place. */
	
	/* Should be deleted. */
	status = zrtp_def_cache_get(ZSTR_GV(zid_my), ZSTR_GV(zid_a), &rs_my4a_r, 0);
	assert_int_not_equal(status, zrtp_status_ok);
	
	/* Should be deleted. */
	status = zrtp_def_cache_get(ZSTR_GV(zid_my), ZSTR_GV(zid_b), &rs_my4b_r, 0);
	assert_int_not_equal(status, zrtp_status_ok);
	
	status = zrtp_def_cache_get(ZSTR_GV(zid_my), ZSTR_GV(zid_c), &rs_my4c_r, 0);
	assert_int_equal(status, zrtp_status_ok);
	assert_false(zrtp_zstrcmp(ZSTR_GV(rs_my4c_r.value), ZSTR_GV(rs_my4c.value)));
	
	/* Should be deleted. */
	status = zrtp_def_cache_get_mitm(ZSTR_GV(zid_my), ZSTR_GV(zid_mitm1), &rs_my4mitm1_r);
	assert_int_not_equal(status, zrtp_status_ok);
	
	status = zrtp_def_cache_get_mitm(ZSTR_GV(zid_my), ZSTR_GV(zid_mitm2), &rs_my4mitm2_r);
	assert_int_equal(status, zrtp_status_ok);
	assert_false(zrtp_zstrcmp(ZSTR_GV(rs_my4mitm2_r.value), ZSTR_GV(rs_my4mitm2.value)));
}

void cache_delete_all_rs_test() {
	zrtp_status_t status;
	
	printf("==> Delete few RS secrets and flush the cache.\n");
	
	secerets_to_delete_count = 0;
	zrtp_cache_create_id(ZSTR_GV(zid_my), ZSTR_GV(zid_b), secerets_to_delete[secerets_to_delete_count++]);
	zrtp_cache_create_id(ZSTR_GV(zid_my), ZSTR_GV(zid_a), secerets_to_delete[secerets_to_delete_count++]);
	zrtp_cache_create_id(ZSTR_GV(zid_my), ZSTR_GV(zid_c), secerets_to_delete[secerets_to_delete_count++]);
	
	zrtp_def_cache_foreach(&g_zrtp_cfg, 0, &cache_foreach_del_func, NULL);
	
	/* Flush the cache and open it again. */
	zrtp_def_cache_down();
	
	printf("==> Open the cache and make sure all our prev. Modifications saved properly.\n");
	
	status = zrtp_def_cache_init(&g_zrtp_cfg);
	assert_int_equal(status, zrtp_status_ok);
	
	/* Let's check if all our modifications are in place. */
	
	/* All RS values should be deleted. */
	status = zrtp_def_cache_get(ZSTR_GV(zid_my), ZSTR_GV(zid_a), &rs_my4a_r, 0);
	assert_int_not_equal(status, zrtp_status_ok);
	
	status = zrtp_def_cache_get(ZSTR_GV(zid_my), ZSTR_GV(zid_b), &rs_my4b_r, 0);
	assert_int_not_equal(status, zrtp_status_ok);
	
	status = zrtp_def_cache_get(ZSTR_GV(zid_my), ZSTR_GV(zid_c), &rs_my4c_r, 0);
	assert_int_not_equal(status, zrtp_status_ok);
	
	/* MiTM secrets should be in place. */
	status = zrtp_def_cache_get_mitm(ZSTR_GV(zid_my), ZSTR_GV(zid_mitm1), &rs_my4mitm1_r);
	assert_int_equal(status, zrtp_status_ok);
	assert_false(zrtp_zstrcmp(ZSTR_GV(rs_my4mitm1_r.value), ZSTR_GV(rs_my4mitm1.value)));
	
	status = zrtp_def_cache_get_mitm(ZSTR_GV(zid_my), ZSTR_GV(zid_mitm2), &rs_my4mitm2_r);
	assert_int_equal(status, zrtp_status_ok);
	assert_false(zrtp_zstrcmp(ZSTR_GV(rs_my4mitm2_r.value), ZSTR_GV(rs_my4mitm2.value)));
}

void cache_delete_all_mitm_test() {
	zrtp_status_t status;
	
	printf("==> Delete few MiTM secrets and flush the cache.\n");
	
	secerets_to_delete_count = 0;
	zrtp_cache_create_id(ZSTR_GV(zid_my), ZSTR_GV(zid_mitm1), secerets_to_delete[secerets_to_delete_count++]);
	zrtp_cache_create_id(ZSTR_GV(zid_my), ZSTR_GV(zid_mitm2), secerets_to_delete[secerets_to_delete_count++]);
	
	zrtp_def_cache_foreach(&g_zrtp_cfg, 1, &cache_foreach_del_func, NULL);
	
	/* Flush the cache and open it again. */
	zrtp_def_cache_down();
	
	printf("==> Open the cache and make sure all our prev. Modifications saved properly.\n");
	
	status = zrtp_def_cache_init(&g_zrtp_cfg);
	assert_int_equal(status, zrtp_status_ok);
	
	/* Let's check if all our modifications are in place. */
	status = zrtp_def_cache_get(ZSTR_GV(zid_my), ZSTR_GV(zid_a), &rs_my4a_r, 0);
	assert_int_equal(status, zrtp_status_ok);
	assert_false(zrtp_zstrcmp(ZSTR_GV(rs_my4a_r.value), ZSTR_GV(rs_my4a.value)));
	
	status = zrtp_def_cache_get(ZSTR_GV(zid_my), ZSTR_GV(zid_b), &rs_my4b_r, 0);
	assert_int_equal(status, zrtp_status_ok);
	assert_false(zrtp_zstrcmp(ZSTR_GV(rs_my4b_r.value), ZSTR_GV(rs_my4b.value)));
	
	status = zrtp_def_cache_get(ZSTR_GV(zid_my), ZSTR_GV(zid_c), &rs_my4c_r, 0);
	assert_int_equal(status, zrtp_status_ok);
	assert_false(zrtp_zstrcmp(ZSTR_GV(rs_my4c_r.value), ZSTR_GV(rs_my4c.value)));
	
	/* All MiTM secrets should be deleted. */
	status = zrtp_def_cache_get_mitm(ZSTR_GV(zid_my), ZSTR_GV(zid_mitm1), &rs_my4mitm1_r);
	assert_int_not_equal(status, zrtp_status_ok);
	
	assert_int_not_equal(zrtp_def_cache_get_mitm(ZSTR_GV(zid_my), ZSTR_GV(zid_mitm2), &rs_my4mitm2_r), zrtp_status_ok);
}

int main(void) {
	const UnitTest tests[] = {
		unit_test_setup_teardown(cache_init_store_empty_test, cache_setup, cache_teardown),
		unit_test_setup_teardown(cache_add2empty_test, cache_setup, cache_teardown),
		unit_test_setup_teardown(cache_save_unchanged_test, cache_setup, cache_teardown),
		unit_test_setup_teardown(cache_modify_and_save_test, cache_setup, cache_teardown),
		
		unit_test_setup_teardown(cache_delete_few_rs_test, cache_setup, cache_teardown),
		unit_test_setup_teardown(cache_delete_few_mitm_test, cache_setup, cache_teardown),
		unit_test_setup_teardown(cache_delete_few_rs_and_mitm_test, cache_setup, cache_teardown),
		unit_test_setup_teardown(cache_delete_all_mitm_test, cache_setup, cache_teardown),
  	};

	return run_tests(tests);
}


/******************************************************************************
 * Helpers
 *****************************************************************************/

static void init_rs_secret_(zrtp_shared_secret_t *sec, unsigned char val_fill) {
	
	char val_buff[ZRTP_HASH_SIZE];
	zrtp_memset(val_buff, val_fill, sizeof(val_buff));
	
	ZSTR_SET_EMPTY(sec->value);
	zrtp_zstrcpyc(ZSTR_GV(sec->value), val_buff);
	
	sec->_cachedflag = 0;
	sec->ttl = 0;
	sec->lastused_at = 0;
}

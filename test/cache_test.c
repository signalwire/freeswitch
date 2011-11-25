#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
 #include <stdio.h>

#include "zrtp.h"
#include "cmockery/cmockery.h"

#define TEST_CACHE_PATH		"./zrtp_cache_test.dat"

static zrtp_global_t g_zrtp_cfg;

void cache_setup() {
	remove(TEST_CACHE_PATH);
	
	ZSTR_SET_EMPTY(g_zrtp_cfg.def_cache_path);
	/* Configure and Initialize ZRTP cache */
	zrtp_zstrcpyc(ZSTR_GV(g_zrtp_cfg.def_cache_path), TEST_CACHE_PATH);
}

void cache_teardown() {
	zrtp_def_cache_down();
}

static void init_rs_secret_(zrtp_shared_secret_t *sec) {
	ZSTR_SET_EMPTY(sec->value);
	
	sec->_cachedflag = 0;
	sec->ttl = 0;
	sec->lastused_at = 0;
}


/*
 * Simply init ZRTP cache with empty or non-existing filer and close it.
 * The app should not crash and trigger no errors.
*/
void cache_init_store_empty_test() {	
	zrtp_status_t status;
	
	/* It should NOT crash and return OK. */
	status = zrtp_def_cache_init(&g_zrtp_cfg);
	assert_int_equal(status, zrtp_status_ok);	
	
	zrtp_def_cache_down();
}

/*
 * Add few entries to the empty cache, flush it and then load again. Check if
 * all the entries were restored successfully.
 */
void cache_add2empty_test() {	
	zrtp_status_t status;
	
	int intres;
	
	zrtp_string16_t zid_my = ZSTR_INIT_WITH_CONST_CSTRING("000000000_01");
	zrtp_string16_t zid_a = ZSTR_INIT_WITH_CONST_CSTRING("000000000_02");
	zrtp_string16_t zid_b = ZSTR_INIT_WITH_CONST_CSTRING("000000000_03");
	zrtp_string16_t zid_c = ZSTR_INIT_WITH_CONST_CSTRING("000000000_04");
	zrtp_string16_t zid_mitm1 = ZSTR_INIT_WITH_CONST_CSTRING("000000000_04");
	
	zrtp_shared_secret_t rs_my4a, rs_my4b, rs_my4c, rs_my4mitm1;
	zrtp_shared_secret_t rs_my4a_r, rs_my4b_r, rs_my4c_r, rs_my4mitm1_r;
	
	init_rs_secret_(&rs_my4a); init_rs_secret_(&rs_my4b);
	init_rs_secret_(&rs_my4c); init_rs_secret_(&rs_my4mitm1);

	init_rs_secret_(&rs_my4a_r); init_rs_secret_(&rs_my4b_r);
	init_rs_secret_(&rs_my4c_r); init_rs_secret_(&rs_my4mitm1_r);
		
	printf("Open empty cache file for.\n");
	status = zrtp_def_cache_init(&g_zrtp_cfg);
	assert_int_equal(status, zrtp_status_ok);
	
	/* Test if cache-init does bot corrupt config. */
	assert_false(strncmp(g_zrtp_cfg.def_cache_path.buffer, TEST_CACHE_PATH, strlen(TEST_CACHE_PATH)));
			
	/* Add few values into it */
	printf("Add few test entries.\n");
	
	status = zrtp_def_cache_put(ZSTR_GV(zid_my), ZSTR_GV(zid_a), &rs_my4a);
	assert_int_equal(status, zrtp_status_ok);
	
	status = zrtp_def_cache_put(ZSTR_GV(zid_my), ZSTR_GV(zid_b), &rs_my4b);
	assert_int_equal(status, zrtp_status_ok);
	
	status = zrtp_def_cache_put_mitm(ZSTR_GV(zid_my), ZSTR_GV(zid_mitm1), &rs_my4mitm1);
	assert_int_equal(status, zrtp_status_ok);
	
	status = zrtp_def_cache_put(ZSTR_GV(zid_my), ZSTR_GV(zid_c), &rs_my4c);
	assert_int_equal(status, zrtp_status_ok);
	
	/* Close the cache, it should be flushed to the file. */
	printf("Close the cache.\n");
	
	zrtp_def_cache_down();
		
	/* Test if cache-close does bot corrupt config. */
	assert_false(strncmp(g_zrtp_cfg.def_cache_path.buffer, TEST_CACHE_PATH, strlen(TEST_CACHE_PATH)));
	
	/* Now, let's open the cache again and check if all the previously added values were restored successfully */
	printf("And open it again, it should contain all the stored values.\n");
	
	status = zrtp_def_cache_init(&g_zrtp_cfg);
	assert_int_equal(status, zrtp_status_ok);
	
	status = zrtp_def_cache_get(ZSTR_GV(zid_my), ZSTR_GV(zid_a), &rs_my4a_r, 0);
	assert_int_equal(status, zrtp_status_ok);
		
	assert_false(zrtp_zstrcmp(ZSTR_GV(rs_my4a_r.value), ZSTR_GV(rs_my4a.value)));
	
	/* Test if cache-close does bot corrupt config. */
	assert_false(strncmp(g_zrtp_cfg.def_cache_path.buffer, TEST_CACHE_PATH, strlen(TEST_CACHE_PATH)));
}



int main(void) {
	const UnitTest tests[] = {
		//unit_test_setup_teardown(cache_init_store_empty_test, cache_setup, cache_teardown),
		unit_test_setup_teardown(cache_add2empty_test, cache_setup, cache_teardown),
  	};

	return run_tests(tests);
}

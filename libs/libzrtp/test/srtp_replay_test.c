/*
 * libZRTP SDK library, implements the ZRTP secure VoIP protocol.
 * Copyright (c) 2006-2009 Philip R. Zimmermann.  All rights reserved.
 * Contact: http://philzimmermann.com
 * For licensing and other legal details, see the file zrtp_legal.c.
 * 
 * Viktor Krykun <v.krikun at zfoneproject.com> 
 */

#include <setjmp.h>
#include <stdio.h>

#include "zrtp.h"
#include "cmockery/cmockery.h"

#define _ZTU_ "srtp replay test"

zrtp_global_t *zrtp;

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


void setup() {
	zrtp_status_t s;
	zrtp_config_t zrtp_config;
	
	zrtp_config_defaults(&zrtp_config);
	
	s = zrtp_init(&zrtp_config, &zrtp);
	assert_int_equal(s, zrtp_status_ok);
}

void teardown() {
	zrtp_down(zrtp);
}

static void print_map(uint8_t *map, int width_bytes)
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

static void init_random_map(uint8_t *map, int width, zrtp_global_t *zrtp) {
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
					  uint8_t *src_map, uint8_t *dst_map, int width) {
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

// TODO: split test into several, more atomic tests
static void srtp_replay_test() {
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
	assert_non_null(rp_node);
		
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
	
	assert_int_equal(res, 0);
}

int main(void) {
	const UnitTest tests[] = {
		unit_test_setup_teardown(srtp_replay_test, setup, teardown),
  	};

	return run_tests(tests);
}

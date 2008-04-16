/*
 *  testm3ua.c
 *  openzap
 *
 *  Created by Shane Burrell on 4/8/08.
 *  Copyright 2008 __MyCompanyName__. All rights reserved.
 *
 */

#include "testm3ua.h"
#include "openzap.h"
#include "zap_m3ua.h"

static ZIO_SIGNAL_CB_FUNCTION(on_signal)
{
	return ZAP_FAIL;
}

int main(int argc, char *argv[])
{
	zap_span_t *span;
	//m3ua_data_t *data;

	zap_global_set_default_logger(ZAP_LOG_LEVEL_DEBUG);

	if (argc < 5) {
		printf("more args needed\n");
		exit(-1);
	}

	if (zap_global_init() != ZAP_SUCCESS) {
		fprintf(stderr, "Error loading OpenZAP\n");
		exit(-1);
	}

	printf("OpenZAP loaded\n");

	if (zap_span_find(atoi(argv[1]), &span) != ZAP_SUCCESS) {
		fprintf(stderr, "Error finding OpenZAP span\n");
		goto done;
	}
	

	if (zap_m3ua_configure_span(span) == ZAP_SUCCESS) {
		//data = span->signal_data;
		zap_m3ua_start(span);
	} else {
		fprintf(stderr, "Error starting M3UA\n");
		goto done;
	}

	//while(zap_test_flag(data, ZAP_M3UA_RUNNING)) {
	//	zap_sleep(1 * 1000);
	//}

 done:

	zap_global_destroy();

}

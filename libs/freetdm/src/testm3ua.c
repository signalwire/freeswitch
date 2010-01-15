/*
 *  testm3ua.c
 *  freetdm
 *
 *  Created by Shane Burrell on 4/8/08.
 *  Copyright 2008 __MyCompanyName__. All rights reserved.
 *
 */

#include "testm3ua.h"
#include "freetdm.h"
#include "ftdm_m3ua.h"

static FIO_SIGNAL_CB_FUNCTION(on_signal)
{
	return FTDM_FAIL;
}

int main(int argc, char *argv[])
{
	ftdm_span_t *span;
	//m3ua_data_t *data;

	ftdm_global_set_default_logger(FTDM_LOG_LEVEL_DEBUG);

	if (argc < 5) {
		printf("more args needed\n");
		exit(-1);
	}

	if (ftdm_global_init() != FTDM_SUCCESS) {
		fprintf(stderr, "Error loading FreeTDM\n");
		exit(-1);
	}

	printf("FreeTDM loaded\n");

	if (ftdm_span_find(atoi(argv[1]), &span) != FTDM_SUCCESS) {
		fprintf(stderr, "Error finding FreeTDM span\n");
		goto done;
	}
	

	if (ftdm_m3ua_configure_span(span) == FTDM_SUCCESS) {
		//data = span->signal_data;
		ftdm_m3ua_start(span);
	} else {
		fprintf(stderr, "Error starting M3UA\n");
		goto done;
	}

	//while(ftdm_test_flag(data, FTDM_M3UA_RUNNING)) {
	//	ftdm_sleep(1 * 1000);
	//}

 done:

	ftdm_global_destroy();

}

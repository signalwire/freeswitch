#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "../../src/include/private/ftdm_core.h"

static int running = 1;

typedef struct custom_data {
	ftdm_timer_t *heartbeat_timer;
	int beat;
	int counter;
	ftdm_sched_callback_t callback;
	ftdm_sched_t *sched;
} custom_data_t;

void trap(int signal)
{
	running = 0;
}

void handle_heartbeat(void *usrdata)
{
	ftdm_status_t status;
	custom_data_t *data = usrdata;

	printf("beep (elapsed %dms count= %d)\n", data->beat, data->counter);
	if (data->beat > 1000) {
		data->beat -= 1000;
	} else if (data->beat <= 1000 && data->beat > 200) {
		data->beat -= 100;
	} else if (data->beat <= 200 && data->beat > 100) {
		if (!data->counter--) {
			data->counter = 5;
			data->beat -= 100;
		}
	} else if (data->beat <= 100 && data->beat > 10) {
		if (!data->counter--) {
			data->counter = 10;
			data->beat -= 10;
			if (data->beat == 10) {
				data->counter = 200;
			}
		}
	} else {
		if (!data->counter--) {
			data->counter = 5;
			data->beat--;
		}
	}

	if (!data->beat) {
		printf("beeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeep you're dead!\n");
		return;
	}

	data->heartbeat_timer = NULL;
	status = ftdm_sched_timer(data->sched, "heartbeat", data->beat, data->callback, data, &data->heartbeat_timer);
	if (status != FTDM_SUCCESS) {
		fprintf(stderr, "Error creating heartbeat timer\n");
		running = 0;
		return;
	}
}

int main(int argc, char *argv[])
{
	ftdm_status_t status;
	custom_data_t data;

	ftdm_sched_t *sched;
	signal(SIGINT, trap);
	
	ftdm_global_set_default_logger(FTDM_LOG_LEVEL_DEBUG);

	if (ftdm_global_init() != FTDM_SUCCESS) {
		fprintf(stderr, "Error loading FreeTDM\n");
		exit(-1);
	}

	status = ftdm_sched_create(&sched, "testsched");
	if (status != FTDM_SUCCESS) {
		fprintf(stderr, "Error creating sched\n");
		exit(-1);
	}

	data.sched = sched;
	data.counter = 10;
	data.beat = 5000;
	data.callback = handle_heartbeat;
	status = ftdm_sched_timer(sched, "heartbeat", data.beat, data.callback, &data, &data.heartbeat_timer);
	if (status != FTDM_SUCCESS) {
		fprintf(stderr, "Error creating heartbeat timer\n");
		exit(-1);
	}

	ftdm_sched_free_run(sched);

	while (running) {
		ftdm_sleep(10);
	}

	ftdm_global_destroy();

	printf("Done, press any key to die!\n");

	getchar();
	return 0;
}


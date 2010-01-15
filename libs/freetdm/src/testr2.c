#include "freetdm.h"
#include <signal.h>

static int R = 0;
static ftdm_mutex_t *mutex = NULL;

static FIO_SIGNAL_CB_FUNCTION(on_r2_signal)
{
    ftdm_log(FTDM_LOG_DEBUG, "Got R2 channel sig [%s] in channel\n", ftdm_signal_event2str(sigmsg->event_id), sigmsg->channel->physical_chan_id);
    return FTDM_SUCCESS;
}

static void handle_SIGINT(int sig)
{
	ftdm_mutex_lock(mutex);
	R = 0;
	ftdm_mutex_unlock(mutex);
	return;
}

int main(int argc, char *argv[])
{
	ftdm_span_t *span;
	ftdm_mutex_create(&mutex);

	ftdm_global_set_default_logger(FTDM_LOG_LEVEL_DEBUG);

	if (argc < 2) {
		printf("umm no\n");
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
	


	if (ftdm_configure_span("r2", span, on_r2_signal,
						   "variant", "mx",
						   "max_ani", 10,
						   "max_dnis", 4,
						   "logging", "all",
						   TAG_END) == FTDM_SUCCESS) {
						   

		ftdm_span_start(span);
	} else {
		fprintf(stderr, "Error starting R2 span\n");
		goto done;
	}

	signal(SIGINT, handle_SIGINT);
	ftdm_mutex_lock(mutex);
	R = 1;
	ftdm_mutex_unlock(mutex);
	while(R) {
		ftdm_sleep(1 * 1000);
	}

 done:

	ftdm_global_destroy();

	return 1;

}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */

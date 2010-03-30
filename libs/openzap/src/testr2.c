#include "openzap.h"
#include <signal.h>

static int R = 0;
static zap_mutex_t *mutex = NULL;

static ZIO_SIGNAL_CB_FUNCTION(on_r2_signal)
{
    zap_log(ZAP_LOG_DEBUG, "Got R2 channel sig [%s] in channel\n", zap_signal_event2str(sigmsg->event_id), sigmsg->channel->physical_chan_id);
    return ZAP_SUCCESS;
}

static void handle_SIGINT(int sig)
{
	zap_mutex_lock(mutex);
	R = 0;
	zap_mutex_unlock(mutex);
	return;
}

int main(int argc, char *argv[])
{
	zap_span_t *span;
	zap_mutex_create(&mutex);

	zap_global_set_default_logger(ZAP_LOG_LEVEL_DEBUG);

	if (argc < 2) {
		printf("umm no\n");
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
	


	if (zap_configure_span("r2", span, on_r2_signal,
						   "variant", "mx",
						   "max_ani", 10,
						   "max_dnis", 4,
						   "logging", "all",
						   TAG_END) == ZAP_SUCCESS) {
						   

		zap_span_start(span);
	} else {
		fprintf(stderr, "Error starting R2 span\n");
		goto done;
	}

	signal(SIGINT, handle_SIGINT);
	zap_mutex_lock(mutex);
	R = 1;
	zap_mutex_unlock(mutex);
	while(R) {
		zap_sleep(1 * 1000);
	}

 done:

	zap_global_destroy();

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

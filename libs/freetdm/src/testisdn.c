#include "freetdm.h"
#include <signal.h>


static FIO_SIGNAL_CB_FUNCTION(on_signal)
{
	return FTDM_FAIL;
}

static int R = 0;
static void handle_SIGINT(int sig)
{
	if (sig);
	R = 0;
	return;
}

int main(int argc, char *argv[])
{
	ftdm_span_t *span;
	
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
	
	if (ftdm_configure_span("isdn", span, on_signal, 
						   "mode", "te", 
						   "dialect", "national",
						   TAG_END) == FTDM_SUCCESS) {
		ftdm_span_start(span);
	} else {
		fprintf(stderr, "Error starting ISDN D-Channel\n");
		goto done;
	}

	signal(SIGINT, handle_SIGINT);
	R = 1;
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */

#include "freetdm.h"

static ZIO_SIGNAL_CB_FUNCTION(on_signal)
{
	return FTDM_FAIL;
}

static int R = 0;
#if 0
static void handle_SIGINT(int sig)
{
	if (sig);
	R = 0;
	return;
}
#endif
int main(int argc, char *argv[])
{
	ftdm_span_t *span;
	int local_port, remote_port;

	local_port = remote_port = 53000;

	ftdm_global_set_default_logger(FTDM_LOG_LEVEL_DEBUG);

	if (argc < 2) {
		printf("umm no\n");
		exit(-1);
	}

	if (ftdm_global_init() != FTDM_SUCCESS) {
		fprintf(stderr, "Error loading OpenFTDM\n");
		exit(-1);
	}

	if (ftdm_global_configuration() != FTDM_SUCCESS) {
		fprintf(stderr, "Error configuring OpenFTDM\n");
		exit(-1);
	}

	printf("OpenFTDM loaded\n");

	if (ftdm_span_find(atoi(argv[1]), &span) != FTDM_SUCCESS) {
		fprintf(stderr, "Error finding OpenFTDM span\n");
		goto done;
	}

#if 1
	if (1) {
		if (ftdm_configure_span("sangoma_boost", span, on_signal,
							"sigmod", "sangoma_brid",
							"local_port", &local_port,
							"remote_ip", "127.0.0.66",
							"remote_port", &remote_port,
							TAG_END) == FTDM_SUCCESS) {
			ftdm_span_start(span);

		} else {
			fprintf(stderr, "Error starting sangoma_boost\n");
			goto done;
		}
	}
#endif

	while(ftdm_running() && R) {
		ftdm_sleep(1 * 1000);
	}

 done:

	ftdm_global_destroy();

	return 0;
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

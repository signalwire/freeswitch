#include "openzap.h"

static ZIO_SIGNAL_CB_FUNCTION(on_signal)
{
	return ZAP_FAIL;
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
	zap_span_t *span;
	int local_port, remote_port;

	local_port = remote_port = 53000;

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
	
	if (zap_configure_span("ss7_boost", span, on_signal, 
						   "local_ip", "127.0.0.65",
						   "local_port", &local_port,
						   "remote_ip", "127.0.0.66",
						   "remote_port", &remote_port,
						   TAG_END) == ZAP_SUCCESS) {
		zap_span_start(span);
	} else {
		fprintf(stderr, "Error starting SS7_BOOST\n");
		goto done;
	}

	while(zap_running() && R) {
		zap_sleep(1 * 1000);
	}

 done:

	zap_global_destroy();

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
